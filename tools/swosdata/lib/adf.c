/*
 * adf.c -- read files out of an Amiga floppy image (.adf), OFS or FFS.
 *
 * The AmigaDOS layout, as far as reading needs it. Every block is 512 bytes and
 * every field a big-endian longword:
 *
 *   header block (root, directory, file)        OFS data block
 *     0  type         2 = T_HEADER               0  type      8 = T_DATA
 *     4  own block number                        4  header block of its file
 *     8  high_seq: data pointers in this block   8  sequence number (1-based)
 *    24  table[72]: a directory's hash table,   12  bytes of data (<= 488)
 *        a file's data blocks, LAST first       16  next data block
 *   324  byte size of a file                    24  the data
 *   432  name length, name (BCPL)
 *   496  next entry on the same hash chain     FFS data blocks are 512 bytes of
 *   504  extension: the next T_LIST block       data and nothing else.
 *        of data pointers
 *   508  secondary type: 1 root, 2 dir, -3 file
 *
 * The root is the middle block of the disk (880 on a DD floppy). Byte 3 of the
 * boot block's "DOS" says the flavour: bit 0 set = FFS. Its other bits
 * (international names, directory caches) do not change any of the above.
 *
 * Michael Steil's extract-adf (pagetable.com) was the reference; this reads a
 * healthy disk by its directory tree rather than scanning sectors for
 * survivors, which is all a game disk needs.
 */
#include "adf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BSIZE        512
#define TABLE_SIZE   72          /* (BSIZE / 4) - 56 */
#define T_HEADER     2
#define T_DATA       8
#define T_LIST       16
#define ST_ROOT      1
#define ST_USERDIR   2
#define ST_FILE      (-3)
#define MAX_DEPTH    16

struct Adf {
    uint8_t  *image;
    uint32_t  blocks;
    uint32_t  root;
    int32_t   ffs;
    char      volume[32];
};

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static const uint8_t *block(const Adf *adf, uint32_t n)
{
    return n < adf->blocks ? adf->image + (size_t)n * BSIZE : NULL;
}

static uint32_t field(const uint8_t *b, uint32_t off) { return rd32(b + off); }

/* A header block's name, BCPL: length byte at 432, then the characters. */
static void block_name(const uint8_t *b, char *out)
{
    uint32_t len = b[432];
    if (len > 30)
        len = 30;
    memcpy(out, b + 433, len);
    out[len] = '\0';
}

/* The sum of all 128 longwords of a valid header block is zero. */
static int32_t checksum_ok(const uint8_t *b)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < BSIZE; i += 4)
        sum += rd32(b + i);
    return sum == 0;
}

static int32_t names_equal(const char *a, const char *b, size_t blen)
{
    size_t alen = strlen(a);
    if (alen != blen)
        return 0;
    for (size_t i = 0; i < alen; i++)
        if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i]))
            return 0;
    return 1;
}

Adf *Adf_Open(const char *path, char *err)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, 256, "cannot open %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size != 901120 && size != 1802240) {
        fclose(f);
        snprintf(err, 256, "%s is %ld bytes, not an ADF (901120 or 1802240)", path, size);
        return NULL;
    }
    Adf *adf = calloc(1, sizeof *adf);
    adf->image = malloc((size_t)size);
    if (fread(adf->image, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        Adf_Close(adf);
        snprintf(err, 256, "cannot read %s", path);
        return NULL;
    }
    fclose(f);
    adf->blocks = (uint32_t)(size / BSIZE);
    adf->root = adf->blocks / 2;

    if (memcmp(adf->image, "DOS", 3) != 0) {
        Adf_Close(adf);
        snprintf(err, 256, "%s is not an AmigaDOS disk (no DOS boot block)", path);
        return NULL;
    }
    adf->ffs = adf->image[3] & 1;

    const uint8_t *root = block(adf, adf->root);
    if (field(root, 0) != T_HEADER || (int32_t)field(root, 508) != ST_ROOT || !checksum_ok(root)) {
        Adf_Close(adf);
        snprintf(err, 256, "%s: the root block is damaged", path);
        return NULL;
    }
    block_name(root, adf->volume);
    return adf;
}

void Adf_Close(Adf *adf)
{
    if (!adf)
        return;
    free(adf->image);
    free(adf);
}

const char *Adf_VolumeName(const Adf *adf) { return adf->volume; }

/* Find `name` among the entries of directory block `dir`; 0 when it is not
 * there. Walks every hash chain rather than hashing, so the international and
 * plain name hashes both work. */
static uint32_t dir_lookup(const Adf *adf, uint32_t dir, const char *name, size_t len)
{
    const uint8_t *d = block(adf, dir);
    for (uint32_t i = 0; i < TABLE_SIZE; i++) {
        uint32_t n = field(d, 24 + i * 4);
        for (uint32_t guard = 0; n && guard < adf->blocks; guard++) {
            const uint8_t *e = block(adf, n);
            if (!e || field(e, 0) != T_HEADER)
                break;
            char nm[32];
            block_name(e, nm);
            if (names_equal(nm, name, len))
                return n;
            n = field(e, 496);
        }
    }
    return 0;
}

static uint32_t resolve(const Adf *adf, const char *path)
{
    uint32_t cur = adf->root;
    const char *p = path;
    while (*p) {
        while (*p == '/')
            p++;
        const char *end = strchr(p, '/');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (!len)
            break;
        cur = dir_lookup(adf, cur, p, len);
        if (!cur)
            return 0;
        p += len;
    }
    return cur;
}

uint8_t *Adf_ReadFile(Adf *adf, const char *path, uint32_t *size, char *err)
{
    uint32_t hdr = resolve(adf, path);
    const uint8_t *h = hdr ? block(adf, hdr) : NULL;
    if (!h || (int32_t)field(h, 508) != ST_FILE) {
        snprintf(err, 256, "%s: not on the disk", path);
        return NULL;
    }
    uint32_t total = field(h, 324);
    uint8_t *out = malloc(total ? total : 1);
    uint32_t got = 0;
    uint32_t per = adf->ffs ? BSIZE : BSIZE - 24;

    /* The data pointers: the header's table, then each extension block's,
     * every table filled from its end (entry 71 is the first block). */
    const uint8_t *t = h;
    for (uint32_t guard = 0; t && got < total && guard < adf->blocks; guard++) {
        uint32_t count = field(t, 8);
        if (count > TABLE_SIZE)
            count = TABLE_SIZE;
        for (uint32_t i = 0; i < count && got < total; i++) {
            uint32_t n = field(t, 24 + (TABLE_SIZE - 1 - i) * 4);
            const uint8_t *d = block(adf, n);
            if (!d)
                goto broken;
            const uint8_t *src = d;
            uint32_t len = per;
            if (!adf->ffs) {
                if (field(d, 0) != T_DATA || field(d, 4) != hdr)
                    goto broken;
                len = field(d, 12);
                if (len > per)
                    goto broken;
                src = d + 24;
            }
            if (len > total - got)
                len = total - got;
            memcpy(out + got, src, len);
            got += len;
        }
        uint32_t ext = field(t, 504);
        t = ext ? block(adf, ext) : NULL;
        if (t && field(t, 0) != T_LIST)
            goto broken;
    }
    if (got != total)
        goto broken;
    *size = total;
    return out;

broken:
    free(out);
    snprintf(err, 256, "%s: its blocks on the disk are damaged", path);
    return NULL;
}

static int32_t walk_dir(Adf *adf, uint32_t dir, char *path, size_t plen, int32_t depth,
                        AdfWalkFn cb, void *user)
{
    if (depth > MAX_DEPTH)
        return 0;
    const uint8_t *d = block(adf, dir);
    for (uint32_t i = 0; i < TABLE_SIZE; i++) {
        uint32_t n = field(d, 24 + i * 4);
        for (uint32_t guard = 0; n && guard < adf->blocks; guard++) {
            const uint8_t *e = block(adf, n);
            if (!e || field(e, 0) != T_HEADER)
                break;
            char nm[32];
            block_name(e, nm);
            size_t len = strlen(nm);
            if (plen + len + 2 < 512) {
                memcpy(path + plen, nm, len + 1);
                int32_t st = (int32_t)field(e, 508);
                int32_t rc = 0;
                if (st == ST_FILE)
                    rc = cb(path, field(e, 324), user);
                else if (st == ST_USERDIR) {
                    path[plen + len] = '/';
                    path[plen + len + 1] = '\0';
                    rc = walk_dir(adf, n, path, plen + len + 1, depth + 1, cb, user);
                }
                path[plen] = '\0';
                if (rc)
                    return rc;
            }
            n = field(e, 496);
        }
    }
    return 0;
}

int32_t Adf_Walk(Adf *adf, AdfWalkFn cb, void *user)
{
    char path[512] = "";
    return walk_dir(adf, adf->root, path, 0, 0, cb, user);
}
