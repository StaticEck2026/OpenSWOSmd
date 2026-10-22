/*
 * lha.c -- LhA archives, written and read (lha.h).
 *
 * THE -lh5- FORMAT (ar002, Haruhiko Okumura, public domain, which LHarc took over):
 * LZ77 over an 8 KB window -- a literal, or a match of 3 to 256 bytes at a distance of
 * 1 to 8192 -- in blocks, each a 16-bit symbol count and three Huffman codes:
 *   C, 510 symbols: 0-255 a literal, 256 + n a match of n + 3 bytes;
 *   P, 14 symbols: a match distance minus one, as its bit length (then the bits below
 *      the top one, raw);
 *   T, 19 symbols: the code the C code's lengths are written in (0 one zero, 1 a run of
 *      3-18 zeros, 2 a run of 20+, n + 2 a length of n).
 * A code with one symbol is written as its symbol alone and costs no bits to use. Codes
 * are canonical, at most 16 bits, sent most significant bit first.
 */
#include "lha.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- shared ---------------------------------------------------------------- */

#define DICBIT     13
#define DICSIZ     (1u << DICBIT)
#define MAXMATCH   256
#define THRESHOLD  3
#define NC         (255 + MAXMATCH + 2 - THRESHOLD)    /* 510 */
#define CBIT       9
#define NT         19
#define TBIT       5
#define NP         (DICBIT + 1)                         /* 14 */
#define PBIT       4
#define MAXBITS    16
#define BLOCK_MAX  16384

static uint16_t s_crcTable[256];

static void crc_init(void)
{
    if (s_crcTable[1])
        return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t r = i;
        for (int32_t j = 0; j < 8; j++)
            r = (r & 1) ? (r >> 1) ^ 0xA001u : r >> 1;
        s_crcTable[i] = (uint16_t)r;
    }
}

/* CRC-16/ARC, LhA's file checksum. */
static uint16_t crc16(const uint8_t *p, uint32_t n)
{
    uint16_t c = 0;
    crc_init();
    for (uint32_t i = 0; i < n; i++)
        c = (uint16_t)(s_crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8));
    return c;
}

static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v & 0xFFFF); wr16(p + 2, v >> 16); }
static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (rd16(p + 2) << 16); }

/* Canonical codes from lengths: by length, then by symbol. */
static void make_codes(int32_t n, const uint8_t *len, uint16_t *code)
{
    uint16_t count[MAXBITS + 2] = { 0 }, start[MAXBITS + 2];
    for (int32_t i = 0; i < n; i++)
        count[len[i]]++;
    count[0] = 0;
    start[1] = 0;
    for (int32_t i = 1; i <= MAXBITS; i++)
        start[i + 1] = (uint16_t)((start[i] + count[i]) << 1);
    for (int32_t i = 0; i < n; i++)
        code[i] = len[i] ? start[len[i]]++ : 0;
}

/* ---- writing: bits ---------------------------------------------------------- */

typedef struct {
    uint8_t *buf;
    uint32_t len, cap;
    uint32_t bits;      /* pending bits, MSB first */
    int32_t  nbits;
} BitOut;

static void out_byte(BitOut *o, uint8_t b)
{
    if (o->len == o->cap) {
        o->cap = o->cap ? o->cap * 2 : 65536;
        o->buf = realloc(o->buf, o->cap);
    }
    o->buf[o->len++] = b;
}

static void putbits(BitOut *o, int32_t n, uint32_t v)
{
    for (int32_t i = n - 1; i >= 0; i--) {
        o->bits = (o->bits << 1) | ((v >> i) & 1);
        if (++o->nbits == 8) {
            out_byte(o, (uint8_t)o->bits);
            o->bits = 0;
            o->nbits = 0;
        }
    }
}

static void flushbits(BitOut *o)
{
    if (o->nbits)
        putbits(o, 8 - o->nbits, 0);
}

/* ---- writing: Huffman ------------------------------------------------------- */

/* Code lengths for `freq`, none longer than MAXBITS. Returns the number of symbols
 * used; with one, *only is that symbol and every length is 0 (ar002's single-symbol
 * code). */
static int32_t make_lengths(int32_t n, const uint32_t *freqIn, uint8_t *len, int32_t *only)
{
    uint32_t freq[NC];
    int32_t used = 0;
    memset(len, 0, (size_t)n);
    for (int32_t i = 0; i < n; i++) {
        freq[i] = freqIn[i];
        if (freq[i]) {
            used++;
            *only = i;
        }
    }
    if (used <= 1)
        return used;

    for (;;) {
        /* Huffman by repeated merging of the two least frequent nodes. */
        int32_t parent[2 * NC], heap[2 * NC], hn = 0, nodes = n;
        uint64_t w[2 * NC];
        for (int32_t i = 0; i < n; i++) {
            w[i] = freq[i];
            parent[i] = -1;
            if (freq[i])
                heap[hn++] = i;
        }
        while (hn > 1) {
            int32_t a = 0, b = 1;
            if (w[heap[b]] < w[heap[a]]) { a = 1; b = 0; }
            for (int32_t i = 2; i < hn; i++) {
                if (w[heap[i]] < w[heap[a]]) { b = a; a = i; }
                else if (w[heap[i]] < w[heap[b]]) b = i;
            }
            int32_t x = heap[a], y = heap[b];
            w[nodes] = w[x] + w[y];
            parent[nodes] = -1;
            parent[x] = parent[y] = nodes;
            if (a > b) { int32_t t = a; a = b; b = t; }
            heap[a] = nodes++;
            heap[b] = heap[--hn];
        }
        int32_t deepest = 0;
        for (int32_t i = 0; i < n; i++) {
            if (!freq[i])
                continue;
            int32_t d = 0;
            for (int32_t p = parent[i]; p >= 0; p = parent[p])
                d++;
            len[i] = (uint8_t)(d > 255 ? 255 : d);
            if (d > deepest)
                deepest = d;
        }
        if (deepest <= MAXBITS)
            return used;
        for (int32_t i = 0; i < n; i++)     /* too deep: flatten the counts, again */
            if (freq[i])
                freq[i] = (freq[i] + 1) / 2;
    }
}

/* ---- writing: LZ77 + blocks ----------------------------------------------------- */

typedef struct {
    BitOut   out;
    uint16_t sym[BLOCK_MAX];
    uint16_t pos[BLOCK_MAX];
    int32_t  n;
    uint32_t cfreq[NC], pfreq[NP];
} Lh5;

static int32_t bitlen(uint32_t v)
{
    int32_t c = 0;
    while (v) { v >>= 1; c++; }
    return c;
}

static void write_pt_len(BitOut *o, const uint8_t *len, int32_t n, int32_t nbit, int32_t special)
{
    while (n > 0 && len[n - 1] == 0)
        n--;
    putbits(o, nbit, (uint32_t)n);
    for (int32_t i = 0; i < n;) {
        int32_t k = len[i++];
        if (k <= 6)
            putbits(o, 3, (uint32_t)k);
        else
            putbits(o, k - 3, (1u << (k - 3)) - 2);
        if (i == special) {
            while (i < 6 && len[i] == 0)
                i++;
            putbits(o, 2, (uint32_t)((i - 3) & 3));
        }
    }
}

static void send_block(Lh5 *z)
{
    BitOut *o = &z->out;
    uint8_t clen[NC], tlen[NT], plen[NP];
    uint16_t ccode[NC], tcode[NT], pcode[NP];
    int32_t conly = 0, tonly = 0, ponly = 0;

    int32_t cused = make_lengths(NC, z->cfreq, clen, &conly);
    putbits(o, 16, (uint32_t)z->n);
    if (cused > 1) {
        make_codes(NC, clen, ccode);
        /* The T code: how the C lengths are written. */
        uint32_t tfreq[NT] = { 0 };
        int32_t cn = NC;
        while (cn > 0 && clen[cn - 1] == 0)
            cn--;
        for (int32_t i = 0; i < cn;) {
            int32_t k = clen[i++];
            if (k == 0) {
                int32_t count = 1;
                while (i < cn && clen[i] == 0) { i++; count++; }
                if (count <= 2) tfreq[0] += (uint32_t)count;
                else if (count <= 18) tfreq[1]++;
                else if (count == 19) { tfreq[0]++; tfreq[1]++; }
                else tfreq[2]++;
            } else {
                tfreq[k + 2]++;
            }
        }
        int32_t tused = make_lengths(NT, tfreq, tlen, &tonly);
        if (tused > 1) {
            make_codes(NT, tlen, tcode);
            write_pt_len(o, tlen, NT, TBIT, 3);
        } else {
            memset(tcode, 0, sizeof tcode);
            putbits(o, TBIT, 0);
            putbits(o, TBIT, (uint32_t)tonly);
        }
        putbits(o, CBIT, (uint32_t)cn);
        for (int32_t i = 0; i < cn;) {
            int32_t k = clen[i++];
            if (k == 0) {
                int32_t count = 1;
                while (i < cn && clen[i] == 0) { i++; count++; }
                if (count <= 2) {
                    for (int32_t j = 0; j < count; j++)
                        putbits(o, tlen[0], tcode[0]);
                } else if (count <= 18) {
                    putbits(o, tlen[1], tcode[1]);
                    putbits(o, 4, (uint32_t)(count - 3));
                } else if (count == 19) {
                    putbits(o, tlen[0], tcode[0]);
                    putbits(o, tlen[1], tcode[1]);
                    putbits(o, 4, 15);
                } else {
                    putbits(o, tlen[2], tcode[2]);
                    putbits(o, CBIT, (uint32_t)(count - 20));
                }
            } else {
                putbits(o, tlen[k + 2], tcode[k + 2]);
            }
        }
    } else {
        memset(ccode, 0, sizeof ccode);
        putbits(o, TBIT, 0);
        putbits(o, TBIT, 0);
        putbits(o, CBIT, 0);
        putbits(o, CBIT, (uint32_t)conly);
    }

    int32_t pused = make_lengths(NP, z->pfreq, plen, &ponly);
    if (pused > 1) {
        make_codes(NP, plen, pcode);
        write_pt_len(o, plen, NP, PBIT, -1);
    } else {
        memset(pcode, 0, sizeof pcode);
        putbits(o, PBIT, 0);
        putbits(o, PBIT, (uint32_t)ponly);
    }

    for (int32_t i = 0; i < z->n; i++) {
        uint16_t c = z->sym[i];
        putbits(o, clen[c], ccode[c]);
        if (c >= 256) {
            uint32_t p = z->pos[i];
            int32_t s = bitlen(p);
            putbits(o, plen[s], pcode[s]);
            if (s > 1)
                putbits(o, s - 1, p & ((1u << (s - 1)) - 1));
        }
    }
    z->n = 0;
    memset(z->cfreq, 0, sizeof z->cfreq);
    memset(z->pfreq, 0, sizeof z->pfreq);
}

static void emit(Lh5 *z, uint16_t c, uint16_t p)
{
    z->sym[z->n] = c;
    z->pos[z->n] = p;
    z->cfreq[c]++;
    if (c >= 256)
        z->pfreq[bitlen(p)]++;
    if (++z->n == BLOCK_MAX)
        send_block(z);
}

#define HASH_BITS 15
#define HASH_SIZE (1u << HASH_BITS)
#define MAX_CHAIN 1024

static uint32_t hash3(const uint8_t *p)
{
    return ((uint32_t)p[0] << 10 ^ (uint32_t)p[1] << 5 ^ p[2]) & (HASH_SIZE - 1);
}

typedef struct {
    const uint8_t *data;
    uint32_t size;
    int32_t *head, *prev;
    uint32_t inserted;      /* positions hashed so far */
} Matcher;

static void insert_upto(Matcher *m, uint32_t end)
{
    for (; m->inserted < end; m->inserted++) {
        uint32_t i = m->inserted;
        if (i + THRESHOLD <= m->size) {
            uint32_t h = hash3(m->data + i);
            m->prev[i] = m->head[h];
            m->head[h] = (int32_t)i;
        }
    }
}

/* The longest match for position i among those before it. */
static uint32_t longest(const Matcher *m, uint32_t i, uint32_t *dist)
{
    uint32_t best = 0;
    if (i + THRESHOLD > m->size)
        return 0;
    uint32_t maxLen = m->size - i < MAXMATCH ? m->size - i : MAXMATCH;
    int32_t chain = 0;
    for (int32_t c = m->head[hash3(m->data + i)]; c >= 0 && chain < MAX_CHAIN; c = m->prev[c], chain++) {
        uint32_t d = i - (uint32_t)c;
        if (d > DICSIZ)
            break;
        if (m->data[c + best] != m->data[i + best])
            continue;
        uint32_t l = 0;
        while (l < maxLen && m->data[c + l] == m->data[i + l])
            l++;
        if (l > best) {
            best = l;
            *dist = d;
            if (l == maxLen)
                break;
        }
    }
    return best;
}

/* -lh5- of data[0..size). A malloc'd buffer and its size. LAZY matching: a match is
 * taken only if the one starting a byte later is no longer -- then the byte goes out
 * as a literal and the later match is taken instead. */
static uint8_t *lh5_pack(const uint8_t *data, uint32_t size, uint32_t *outSize)
{
    Lh5 *z = calloc(1, sizeof *z);
    Matcher m = { data, size, malloc(sizeof(int32_t) * HASH_SIZE),
                  malloc(sizeof(int32_t) * (size ? size : 1)), 0 };
    for (uint32_t i = 0; i < HASH_SIZE; i++)
        m.head[i] = -1;

    uint32_t i = 0;
    while (i < size) {
        uint32_t dist = 0, len;
        insert_upto(&m, i);
        len = longest(&m, i, &dist);
        if (len >= THRESHOLD && len < MAXMATCH && i + 1 < size) {
            uint32_t d2 = 0, l2;
            insert_upto(&m, i + 1);
            l2 = longest(&m, i + 1, &d2);
            if (l2 > len) {
                emit(z, data[i], 0);
                i++;
                continue;
            }
        }
        if (len >= THRESHOLD) {
            emit(z, (uint16_t)(len - THRESHOLD + 256), (uint16_t)(dist - 1));
            i += len;
        } else {
            emit(z, data[i], 0);
            i++;
        }
    }
    if (z->n)
        send_block(z);
    flushbits(&z->out);
    free(m.head);
    free(m.prev);
    uint8_t *buf = z->out.buf;
    *outSize = z->out.len;
    free(z);
    return buf;
}

/* ---- writing: the archive ------------------------------------------------------ */

struct LhaWriter {
    FILE *f;
};

static uint32_t dos_time(int64_t t)
{
    time_t tt = t ? (time_t)t : time(NULL);
    struct tm *tm = localtime(&tt);
    if (!tm || tm->tm_year < 80)
        return (1u << 21) | (1u << 16);                 /* 1980-01-01 */
    return ((uint32_t)(tm->tm_year - 80) << 25) | ((uint32_t)(tm->tm_mon + 1) << 21) |
           ((uint32_t)tm->tm_mday << 16) | ((uint32_t)tm->tm_hour << 11) |
           ((uint32_t)tm->tm_min << 5) | ((uint32_t)tm->tm_sec / 2);
}

LhaWriter *Lha_Create(const char *path, char *err)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(err, 256, "cannot create %s", path);
        return NULL;
    }
    LhaWriter *w = calloc(1, sizeof *w);
    w->f = f;
    return w;
}

/* One entry: `isDir` with no data. Level 1: the name in the base header, the folders
 * before it in a 0x02 extension. */
static int32_t add_entry(LhaWriter *w, const char *path, const uint8_t *data, uint32_t size,
                         int64_t mtime, int32_t isDir, char *err)
{
    char dir[512];
    const char *name;
    size_t plen = strlen(path);

    /* Split "a/b/c" into "a/b/" and "c"; a folder "a/b/" is all directory. */
    if (isDir) {
        snprintf(dir, sizeof dir, "%s", path);
        name = "";
    } else {
        const char *slash = strrchr(path, '/');
        size_t dl = slash ? (size_t)(slash - path + 1) : 0;
        if (dl >= sizeof dir || plen - dl > 255) {
            snprintf(err, 256, "%s: name too long", path);
            return -1;
        }
        memcpy(dir, path, dl);
        dir[dl] = '\0';
        name = path + dl;
    }
    size_t nlen = strlen(name), dlen = strlen(dir);

    uint8_t *packed = NULL;
    uint32_t psize = 0;
    const char *method = "-lh0-";
    if (!isDir && size > 0) {
        packed = lh5_pack(data, size, &psize);
        if (psize < size)
            method = "-lh5-";
        else {
            free(packed);
            packed = NULL;
        }
    }
    const uint8_t *body = packed ? packed : data;
    uint32_t bsize = packed ? psize : (isDir ? 0 : size);

    uint8_t h[512];
    uint32_t extSize = dlen ? (uint32_t)(dlen + 3) : 0;
    uint32_t base = (uint32_t)(27 + nlen);
    memset(h, 0, sizeof h);
    h[0] = (uint8_t)(base - 2);
    memcpy(h + 2, method, 5);
    wr32(h + 7, bsize + extSize);
    wr32(h + 11, isDir ? 0 : size);
    wr32(h + 15, dos_time(mtime));
    h[19] = isDir ? 0x10 : 0x00;
    h[20] = 1;
    h[21] = (uint8_t)nlen;
    memcpy(h + 22, name, nlen);
    wr16(h + 22 + nlen, isDir ? 0 : crc16(data, size));
    h[24 + nlen] = 'A';
    wr16(h + 25 + nlen, extSize);
    uint32_t sum = 0;
    for (uint32_t i = 2; i < base; i++)
        sum += h[i];
    h[1] = (uint8_t)sum;

    uint8_t ext[520];
    uint32_t elen = 0;
    if (dlen) {
        ext[elen++] = 0x02;
        for (size_t i = 0; i < dlen; i++)
            ext[elen++] = dir[i] == '/' ? 0xFF : (uint8_t)dir[i];
        ext[elen++] = 0;
        ext[elen++] = 0;
    }
    int32_t ok = fwrite(h, 1, base, w->f) == base && fwrite(ext, 1, elen, w->f) == elen &&
                 (bsize == 0 || fwrite(body, 1, bsize, w->f) == bsize);
    free(packed);
    if (!ok) {
        snprintf(err, 256, "%s: cannot write the archive", path);
        return -1;
    }
    return 0;
}

int32_t Lha_AddFile(LhaWriter *w, const char *path, const uint8_t *data, uint32_t size,
                    int64_t mtime, char *err)
{
    return add_entry(w, path, data, size, mtime, 0, err);
}

int32_t Lha_AddDir(LhaWriter *w, const char *path, int64_t mtime, char *err)
{
    char p[512];
    size_t n = strlen(path);
    snprintf(p, sizeof p, "%s%s", path, n && path[n - 1] == '/' ? "" : "/");
    return add_entry(w, p, NULL, 0, mtime, 1, err);
}

int32_t Lha_Close(LhaWriter *w, char *err)
{
    int32_t rc = 0;
    if (fputc(0, w->f) == EOF)
        rc = -1;
    if (fclose(w->f) != 0)
        rc = -1;
    if (rc)
        snprintf(err, 256, "cannot finish the archive");
    free(w);
    return rc;
}

/* ---- reading -------------------------------------------------------------------- */

typedef struct {
    char     path[512];
    char     method[6];
    uint32_t offset;        /* packed data in the archive */
    uint32_t packed;
    uint32_t size;
    uint16_t crc;
    int32_t  isDir;
} Entry;

struct LhaArchive {
    uint8_t *data;
    uint32_t len;
    Entry   *e;
    int32_t  n;
};

static void path_from(char *out, size_t cap, const uint8_t *dir, size_t dlen, const uint8_t *name,
                      size_t nlen, int32_t isDir)
{
    size_t o = 0;
    for (size_t i = 0; i < dlen && o + 1 < cap; i++) {
        uint8_t c = dir[i];
        out[o++] = (c == 0xFF || c == '\\') ? '/' : (char)c;
    }
    if (o && out[o - 1] != '/' && nlen && o + 1 < cap)
        out[o++] = '/';
    for (size_t i = 0; i < nlen && o + 1 < cap; i++) {
        uint8_t c = name[i];
        out[o++] = (c == 0xFF || c == '\\') ? '/' : (char)c;
    }
    if (isDir && (o == 0 || out[o - 1] != '/') && o + 1 < cap)
        out[o++] = '/';
    out[o] = '\0';
}

static int32_t parse(LhaArchive *a, char *err)
{
    uint32_t p = 0, cap = 0;
    while (p < a->len && a->data[p] != 0) {
        const uint8_t *h = a->data + p;
        uint32_t left = a->len - p;
        if (left < 24 || h[2] != '-' || h[3] != 'l' || h[6] != '-') {
            snprintf(err, 256, "not an LhA archive (or damaged at byte %u)", (unsigned)p);
            return -1;
        }
        Entry e;
        memset(&e, 0, sizeof e);
        memcpy(e.method, h + 2, 5);
        int32_t level = h[20];
        uint8_t dirb[512], nameb[256];
        size_t dlen = 0, nlen = 0;
        uint32_t next;

        if (level == 0 || level == 1) {
            uint32_t base = (uint32_t)h[0] + 2;
            nlen = h[21];
            if (base > left || 22 + nlen + 2 > base) goto bad;
            memcpy(nameb, h + 22, nlen);
            e.crc  = (uint16_t)rd16(h + 22 + nlen);
            e.size = rd32(h + 11);
            uint32_t skip = rd32(h + 7);
            uint32_t extBytes = 0;
            if (level == 1) {
                uint32_t q = p + base;
                uint32_t sz = rd16(h + base - 2);
                while (sz) {
                    if (q + sz > a->len || sz < 3) goto bad;
                    const uint8_t *x = a->data + q;
                    if (x[0] == 0x02 && sz - 3 < sizeof dirb) { dlen = sz - 3; memcpy(dirb, x + 1, dlen); }
                    else if (x[0] == 0x01 && sz - 3 < sizeof nameb) { nlen = sz - 3; memcpy(nameb, x + 1, nlen); }
                    extBytes += sz;
                    q += sz;
                    sz = rd16(x + sz - 2);
                }
                if (skip < extBytes) goto bad;
                e.packed = skip - extBytes;
            } else {
                e.packed = skip;
                /* Level 0 keeps the path in the name, with \ or / between folders. */
            }
            e.offset = p + base + extBytes;
            next = e.offset + e.packed;
        } else if (level == 2) {
            uint32_t total = rd16(h);
            if (total > left || total < 26) goto bad;
            e.packed = rd32(h + 7);
            e.size   = rd32(h + 11);
            e.crc    = (uint16_t)rd16(h + 21);
            uint32_t q = p + 26, sz = rd16(h + 24);
            while (sz) {
                if (q + sz > p + total || sz < 3) goto bad;
                const uint8_t *x = a->data + q;
                if (x[0] == 0x02 && sz - 3 < sizeof dirb) { dlen = sz - 3; memcpy(dirb, x + 1, dlen); }
                else if (x[0] == 0x01 && sz - 3 < sizeof nameb) { nlen = sz - 3; memcpy(nameb, x + 1, nlen); }
                q += sz;
                sz = rd16(x + sz - 2);
            }
            e.offset = p + total;
            next = e.offset + e.packed;
        } else {
            snprintf(err, 256, "LhA header level %d is not supported", level);
            return -1;
        }
        if (next > a->len) goto bad;
        e.isDir = memcmp(e.method, "-lhd-", 5) == 0 || (level < 2 && (h[19] & 0x10) && e.size == 0);
        path_from(e.path, sizeof e.path, dirb, dlen, nameb, nlen, e.isDir);
        if (a->n == (int32_t)cap) {
            cap = cap ? cap * 2 : 64;
            a->e = realloc(a->e, sizeof(Entry) * cap);
        }
        a->e[a->n++] = e;
        p = next;
    }
    return 0;
bad:
    snprintf(err, 256, "the LhA archive is damaged at byte %u", (unsigned)p);
    return -1;
}

LhaArchive *Lha_Open(const char *path, char *err)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, 256, "cannot open %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    LhaArchive *a = calloc(1, sizeof *a);
    a->data = malloc(n > 0 ? (size_t)n : 1);
    a->len = (uint32_t)(n > 0 ? n : 0);
    if (n <= 0 || fread(a->data, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        Lha_Free(a);
        snprintf(err, 256, "cannot read %s", path);
        return NULL;
    }
    fclose(f);
    if (parse(a, err) != 0 || a->n == 0) {
        if (a->n == 0)
            snprintf(err, 256, "%s is not an LhA archive", path);
        Lha_Free(a);
        return NULL;
    }
    return a;
}

void Lha_Free(LhaArchive *a)
{
    if (!a)
        return;
    free(a->data);
    free(a->e);
    free(a);
}

int32_t     Lha_Count(const LhaArchive *a)          { return a->n; }
const char *Lha_Path(const LhaArchive *a, int32_t i) { return a->e[i].path; }
uint32_t    Lha_Size(const LhaArchive *a, int32_t i) { return a->e[i].size; }

int32_t Lha_Find(const LhaArchive *a, const char *path)
{
    for (int32_t i = 0; i < a->n; i++) {
        const char *x = a->e[i].path, *y = path;
        while (*x && *y && tolower((unsigned char)*x) == tolower((unsigned char)*y))
            x++, y++;
        if (!*x && !*y)
            return i;
    }
    return -1;
}

/* ---- reading: -lh5- -------------------------------------------------------------- */

typedef struct {
    const uint8_t *p;
    uint32_t len, pos;      /* in bits */
} BitIn;

static uint32_t getbits(BitIn *b, int32_t n)
{
    uint32_t v = 0;
    for (int32_t i = 0; i < n; i++) {
        uint32_t byte = b->pos >> 3;
        uint32_t bit = byte < b->len ? (b->p[byte] >> (7 - (b->pos & 7))) & 1 : 0;
        v = (v << 1) | bit;
        b->pos++;
    }
    return v;
}

static uint32_t peekbits(const BitIn *b, int32_t n)
{
    BitIn t = *b;
    return getbits(&t, n);
}

/* A canonical code to decode with: the symbols by length then value. `only` >= 0: one
 * symbol, no bits. */
typedef struct {
    uint16_t count[MAXBITS + 1];
    uint16_t sym[NC];
    int32_t  only;
} Code;

static void code_build(Code *c, const uint8_t *len, int32_t n)
{
    int32_t k = 0;
    memset(c->count, 0, sizeof c->count);
    c->only = -1;
    for (int32_t i = 0; i < n; i++)
        c->count[len[i]]++;
    c->count[0] = 0;
    for (int32_t l = 1; l <= MAXBITS; l++)
        for (int32_t i = 0; i < n; i++)
            if (len[i] == l)
                c->sym[k++] = (uint16_t)i;
}

static int32_t decode(BitIn *b, const Code *c)
{
    if (c->only >= 0)
        return c->only;
    int32_t code = 0, first = 0, index = 0;
    for (int32_t l = 1; l <= MAXBITS; l++) {
        code |= (int32_t)getbits(b, 1);
        int32_t count = c->count[l];
        if (code - first < count)
            return c->sym[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static int32_t read_pt_len(BitIn *b, Code *c, int32_t nn, int32_t nbit, int32_t special)
{
    uint8_t len[NT > NP ? NT : NP];
    int32_t n = (int32_t)getbits(b, nbit);
    memset(len, 0, sizeof len);
    if (n == 0) {
        code_build(c, len, nn);
        c->only = (int32_t)getbits(b, nbit);
        return c->only < nn ? 0 : -1;
    }
    if (n > nn)
        return -1;
    for (int32_t i = 0; i < n;) {
        int32_t k = (int32_t)peekbits(b, 3);
        if (k == 7) {
            getbits(b, 3);
            while (getbits(b, 1))
                if (++k > MAXBITS)
                    return -1;
        } else {
            getbits(b, 3);
        }
        len[i++] = (uint8_t)k;
        if (i == special) {
            int32_t z = (int32_t)getbits(b, 2);
            while (z-- > 0 && i < nn)
                len[i++] = 0;
        }
    }
    code_build(c, len, nn);
    return 0;
}

static int32_t read_c_len(BitIn *b, Code *c, const Code *t)
{
    uint8_t len[NC];
    int32_t n = (int32_t)getbits(b, CBIT);
    memset(len, 0, sizeof len);
    if (n == 0) {
        code_build(c, len, NC);
        c->only = (int32_t)getbits(b, CBIT);
        return c->only < NC ? 0 : -1;
    }
    if (n > NC)
        return -1;
    for (int32_t i = 0; i < n;) {
        int32_t k = decode(b, t);
        if (k < 0)
            return -1;
        if (k <= 2) {
            int32_t z = k == 0 ? 1 : k == 1 ? (int32_t)getbits(b, 4) + 3 : (int32_t)getbits(b, CBIT) + 20;
            while (z-- > 0 && i < NC)
                len[i++] = 0;
        } else {
            len[i++] = (uint8_t)(k - 2);
        }
    }
    code_build(c, len, NC);
    return 0;
}

static int32_t lh5_unpack(const uint8_t *in, uint32_t inLen, uint8_t *out, uint32_t outLen)
{
    BitIn b = { in, inLen, 0 };
    Code *t = malloc(sizeof *t), *c = malloc(sizeof *c), *p = malloc(sizeof *p);
    uint32_t o = 0;
    int32_t rc = 0;
    while (o < outLen && rc == 0) {
        uint32_t blk = getbits(&b, 16);
        if (read_pt_len(&b, t, NT, TBIT, 3) || read_c_len(&b, c, t) || read_pt_len(&b, p, NP, PBIT, -1)) {
            rc = -1;
            break;
        }
        for (uint32_t k = 0; k < blk && o < outLen; k++) {
            int32_t s = decode(&b, c);
            if (s < 0) { rc = -1; break; }
            if (s < 256) {
                out[o++] = (uint8_t)s;
                continue;
            }
            uint32_t mlen = (uint32_t)s - 256 + THRESHOLD;
            int32_t slot = decode(&b, p);
            if (slot < 0) { rc = -1; break; }
            uint32_t dist = slot <= 1 ? (uint32_t)slot : (1u << (slot - 1)) + getbits(&b, slot - 1);
            dist += 1;
            if (dist > o) { rc = -1; break; }
            for (uint32_t j = 0; j < mlen && o < outLen; j++, o++)
                out[o] = out[o - dist];
        }
        if (b.pos > inLen * 8u + 64)
            rc = -1;
    }
    free(t);
    free(c);
    free(p);
    return rc;
}

uint8_t *Lha_Read(const LhaArchive *a, int32_t i, uint32_t *size, char *err)
{
    const Entry *e = &a->e[i];
    uint8_t *out = malloc(e->size ? e->size : 1);
    if (e->isDir) {
        *size = 0;
        return out;
    }
    if (memcmp(e->method, "-lh0-", 5) == 0) {
        if (e->packed != e->size)
            goto bad;
        memcpy(out, a->data + e->offset, e->size);
    } else if (memcmp(e->method, "-lh5-", 5) == 0) {
        if (lh5_unpack(a->data + e->offset, e->packed, out, e->size) != 0)
            goto bad;
    } else {
        free(out);
        snprintf(err, 256, "%.200s: packed %.5s, which is not supported", e->path, e->method);
        return NULL;
    }
    if (crc16(out, e->size) != e->crc)
        goto bad;
    *size = e->size;
    return out;
bad:
    free(out);
    snprintf(err, 256, "%.200s: damaged in the archive", e->path);
    return NULL;
}
