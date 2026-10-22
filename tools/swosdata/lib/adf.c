/*
 * adf.c -- read files out of an Amiga disk image: a floppy (.adf), OFS or FFS, or a
 * hard disk (.hdf).
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
 * The root is the middle block of the volume (880 on a DD floppy). Byte 3 of the
 * boot block's "DOS" says the flavour: bit 0 set = FFS. Its other bits
 * (international names, directory caches) do not change any of the above.
 *
 * A HARD DISK IMAGE is one of two things. Most SWOS ones (ASWOS's, sensiblesoccer.de's)
 * are a single bare partition: the volume starts at byte 0 with the same "DOS" boot
 * block, only bigger, so a floppy is just the smallest case. WinUAE can also make a
 * whole disk with a Rigid Disk Block ("RDSK" in one of its first 16 blocks), whose
 * PART blocks say where each partition starts and how big it is; each is read as a
 * volume of its own (Adf_OpenPartition). An image is read block by block from the
 * file, never loaded whole: a hard disk can be hundreds of megabytes.
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
#define MAX_PARTS    16
#define RDB_SCAN     16          /* the RDB is in one of the first 16 blocks */

struct Adf {
    FILE     *f;
    uint64_t  base;              /* the volume's first byte in the image */
    uint32_t  blocks;            /* the volume's size */
    uint32_t  root;
    int32_t   ffs;
    char      volume[32];
};

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t field(const uint8_t *b, uint32_t off) { return rd32(b + off); }

static int32_t file_seek(FILE *f, uint64_t at)
{
#ifdef _WIN32
    return _fseeki64(f, (__int64)at, SEEK_SET);
#else
    return fseeko(f, (off_t)at, SEEK_SET);
#endif
}

static uint64_t file_size(FILE *f)
{
#ifdef _WIN32
    _fseeki64(f, 0, SEEK_END);
    return (uint64_t)_ftelli64(f);
#else
    fseeko(f, 0, SEEK_END);
    return (uint64_t)ftello(f);
#endif
}

/* Block `n` of the volume into `buf`. 0 on success. */
static int32_t get_block(const Adf *adf, uint32_t n, uint8_t *buf)
{
    if (n >= adf->blocks || file_seek(adf->f, adf->base + (uint64_t)n * BSIZE) != 0)
        return -1;
    return fread(buf, 1, BSIZE, adf->f) == BSIZE ? 0 : -1;
}

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

static int32_t is_root(const uint8_t *b)
{
    return field(b, 0) == T_HEADER && (int32_t)field(b, 508) == ST_ROOT && checksum_ok(b);
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

/* ---- finding the volumes in an image ---------------------------------------- */

typedef struct {
    uint64_t base;
    uint32_t blocks;
    uint32_t reserved;
} Part;

/* The volumes in the image at `f` of `size` bytes: one for a floppy or a bare
 * partition, one per partition for a disk with an RDB. Returns how many. */
static int32_t find_parts(FILE *f, uint64_t size, Part *out, int32_t max)
{
    uint8_t b[BSIZE];
    int32_t n = 0;

    /* A Rigid Disk Block: "RDSK", in one of the first blocks. */
    for (uint32_t i = 0; i < RDB_SCAN && (uint64_t)(i + 1) * BSIZE <= size; i++) {
        if (file_seek(f, (uint64_t)i * BSIZE) != 0 || fread(b, 1, BSIZE, f) != BSIZE)
            break;
        if (memcmp(b, "RDSK", 4) != 0)
            continue;
        uint32_t bsize = field(b, 16);              /* rdb_BlockBytes */
        uint32_t part  = field(b, 28);              /* rdb_PartitionList */
        if (bsize != BSIZE)
            return 0;                               /* only 512-byte blocks */
        for (int32_t guard = 0; part != 0xFFFFFFFFu && guard < 64 && n < max; guard++) {
            if ((uint64_t)(part + 1) * BSIZE > size ||
                file_seek(f, (uint64_t)part * BSIZE) != 0 || fread(b, 1, BSIZE, f) != BSIZE ||
                memcmp(b, "PART", 4) != 0)
                break;
            /* pb_Environment (DosEnvec) at 128: [3] surfaces, [5] blocks per track,
             * [6] reserved, [9] low cylinder, [10] high cylinder; [1] block size in
             * longwords. */
            const uint8_t *env = b + 128;
            uint32_t longs = field(env, 1 * 4), surfaces = field(env, 3 * 4);
            uint32_t bpt = field(env, 5 * 4), reserved = field(env, 6 * 4);
            uint32_t low = field(env, 9 * 4), high = field(env, 10 * 4);
            uint64_t perCyl = (uint64_t)surfaces * bpt;
            if (longs == BSIZE / 4 && perCyl && high >= low) {
                out[n].base     = (uint64_t)low * perCyl * BSIZE;
                out[n].blocks   = (uint32_t)((uint64_t)(high - low + 1) * perCyl);
                out[n].reserved = reserved ? reserved : 2;
                if (out[n].base + (uint64_t)out[n].blocks * BSIZE <= size)
                    n++;
            }
            part = field(b, 16);                    /* pb_Next */
        }
        return n;
    }

    /* A floppy or a bare partition: the volume is the whole image. */
    if (size >= 4 * BSIZE && size / BSIZE <= 0xFFFFFFFFu && n < max) {
        out[n].base = 0;
        out[n].blocks = (uint32_t)(size / BSIZE);
        out[n].reserved = 2;
        n++;
    }
    return n;
}

/* Open volume `p` of the image: its "DOS" boot block and its root. */
static Adf *open_part(FILE *f, const Part *p, const char *path, char *err)
{
    uint8_t b[BSIZE];
    Adf *adf = calloc(1, sizeof *adf);
    adf->f      = f;
    adf->base   = p->base;
    adf->blocks = p->blocks;

    if (get_block(adf, 0, b) != 0 || memcmp(b, "DOS", 3) != 0) {
        free(adf);
        snprintf(err, 256, "%s is not an AmigaDOS disk (no DOS boot block)", path);
        return NULL;
    }
    adf->ffs = b[3] & 1;

    /* Where AmigaDOS puts the root: the middle of the volume, (blocks - 1 + reserved)
     * / 2 -- 880 on a floppy. A floppy's boot block also names it at byte 8; take that
     * when the middle is not one (a volume formatted with other geometry). */
    {
        uint32_t tries[2];
        tries[0] = (p->blocks - 1 + p->reserved) / 2;
        tries[1] = field(b, 8);
        adf->root = 0;
        for (int32_t i = 0; i < 2 && !adf->root; i++) {
            uint8_t r[BSIZE];
            if (tries[i] && get_block(adf, tries[i], r) == 0 && is_root(r)) {
                adf->root = tries[i];
                block_name(r, adf->volume);
            }
        }
    }
    /* Neither: an image made for one geometry and saved at another size -- SWOS-HD.HDF
     * (2001) is 3,000,000 bytes, a partial last block, with its root at 2928 where the
     * middle of what is there is 2930. Look outward from the middle for the root. */
    if (!adf->root) {
        uint32_t mid = (p->blocks - 1 + p->reserved) / 2;
        for (uint32_t d = 1; d <= 4096 && !adf->root; d++) {
            for (int32_t side = 0; side < 2 && !adf->root; side++) {
                uint32_t n = side ? mid + d : mid - d;
                uint8_t r[BSIZE];
                if ((side || mid >= d) && n < p->blocks && get_block(adf, n, r) == 0 && is_root(r)) {
                    adf->root = n;
                    block_name(r, adf->volume);
                }
            }
        }
    }
    if (!adf->root) {
        free(adf);
        snprintf(err, 256, "%s: the root block is damaged", path);
        return NULL;
    }
    return adf;
}

static FILE *open_image(const char *path, uint64_t *size, char *err)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, 256, "cannot open %s", path);
        return NULL;
    }
    *size = file_size(f);
    if (*size < 4 * BSIZE) {
        fclose(f);
        snprintf(err, 256, "%s is too small to be an Amiga disk image", path);
        return NULL;
    }
    return f;
}

int32_t Adf_CountVolumes(const char *path)
{
    char err[256];
    uint64_t size;
    Part parts[MAX_PARTS];
    FILE *f = open_image(path, &size, err);
    if (!f)
        return 0;
    int32_t n = find_parts(f, size, parts, MAX_PARTS);
    fclose(f);
    return n;
}

Adf *Adf_OpenVolume(const char *path, int32_t index, char *err)
{
    uint64_t size;
    Part parts[MAX_PARTS];
    FILE *f = open_image(path, &size, err);
    if (!f)
        return NULL;
    int32_t n = find_parts(f, size, parts, MAX_PARTS);
    if (index < 0 || index >= n) {
        fclose(f);
        snprintf(err, 256, n ? "%s has no volume %d" : "%s is not an Amiga disk image", path, (int)index);
        return NULL;
    }
    Adf *adf = open_part(f, &parts[index], path, err);
    if (!adf)
        fclose(f);
    return adf;
}

Adf *Adf_Open(const char *path, char *err)
{
    return Adf_OpenVolume(path, 0, err);
}

void Adf_Close(Adf *adf)
{
    if (!adf)
        return;
    if (adf->f)
        fclose(adf->f);
    free(adf);
}

const char *Adf_VolumeName(const Adf *adf) { return adf->volume; }

int32_t Adf_PeekVolumeName(const char *path, char out[32])
{
    char err[256];
    Adf *adf = Adf_Open(path, err);
    if (!adf)
        return -1;
    snprintf(out, 32, "%s", adf->volume);
    Adf_Close(adf);
    return 0;
}

/* Find `name` among the entries of directory block `dir`; 0 when it is not
 * there. Walks every hash chain rather than hashing, so the international and
 * plain name hashes both work. */
static uint32_t dir_lookup(const Adf *adf, uint32_t dir, const char *name, size_t len)
{
    uint8_t d[BSIZE], e[BSIZE];
    if (get_block(adf, dir, d) != 0)
        return 0;
    for (uint32_t i = 0; i < TABLE_SIZE; i++) {
        uint32_t n = field(d, 24 + i * 4);
        for (uint32_t guard = 0; n && guard < adf->blocks; guard++) {
            if (get_block(adf, n, e) != 0 || field(e, 0) != T_HEADER)
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

static uint32_t resolve(const Adf *adf, uint32_t from, const char *path)
{
    uint32_t cur = from;
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
    uint8_t h[BSIZE], t[BSIZE], d[BSIZE];
    uint32_t hdr = resolve(adf, adf->root, path);
    if (!hdr || get_block(adf, hdr, h) != 0 || (int32_t)field(h, 508) != ST_FILE) {
        snprintf(err, 256, "%s: not on the disk", path);
        return NULL;
    }
    uint32_t total = field(h, 324);
    uint8_t *out = malloc(total ? total : 1);
    uint32_t got = 0;
    uint32_t per = adf->ffs ? BSIZE : BSIZE - 24;
    int32_t haveT = 1;

    /* The data pointers: the header's table, then each extension block's,
     * every table filled from its end (entry 71 is the first block). */
    memcpy(t, h, BSIZE);
    for (uint32_t guard = 0; haveT && got < total && guard < adf->blocks; guard++) {
        uint32_t count = field(t, 8);
        if (count > TABLE_SIZE)
            count = TABLE_SIZE;
        for (uint32_t i = 0; i < count && got < total; i++) {
            uint32_t n = field(t, 24 + (TABLE_SIZE - 1 - i) * 4);
            if (get_block(adf, n, d) != 0)
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
        haveT = 0;
        if (ext) {
            if (get_block(adf, ext, t) != 0 || field(t, 0) != T_LIST)
                goto broken;
            haveT = 1;
        }
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

int64_t Adf_FileSize(Adf *adf, const char *path)
{
    uint8_t h[BSIZE];
    uint32_t hdr = resolve(adf, adf->root, path);
    if (!hdr || get_block(adf, hdr, h) != 0 || (int32_t)field(h, 508) != ST_FILE)
        return -1;
    return (int64_t)field(h, 324);
}

static int32_t walk_dir(Adf *adf, uint32_t dir, char *path, size_t plen, int32_t depth,
                        AdfWalkFn cb, void *user)
{
    uint8_t d[BSIZE], e[BSIZE];
    if (depth > MAX_DEPTH || get_block(adf, dir, d) != 0)
        return 0;
    for (uint32_t i = 0; i < TABLE_SIZE; i++) {
        uint32_t n = field(d, 24 + i * 4);
        for (uint32_t guard = 0; n && guard < adf->blocks; guard++) {
            if (get_block(adf, n, e) != 0 || field(e, 0) != T_HEADER)
                break;
            char nm[32];
            block_name(e, nm);
            size_t len = strlen(nm);
            uint32_t next = field(e, 496);
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
            n = next;
        }
    }
    return 0;
}

int32_t Adf_Walk(Adf *adf, AdfWalkFn cb, void *user)
{
    char path[512] = "";
    return walk_dir(adf, adf->root, path, 0, 0, cb, user);
}
