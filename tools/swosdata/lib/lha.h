/*
 * lha.h -- LhA archives (.lha), the Amiga's own: written for the WHDLoad package the
 * setup makes, read to take the game out of a WHDLoad install someone downloaded.
 *
 * WRITING makes what an Amiga LhA makes and every LhA reads: level-1 headers, OS 'A',
 * each entry's folder in a directory extension (0x02) with 0xFF between names, files
 * compressed -lh5- (an 8 KB window, Huffman-coded blocks: the format Haruhiko Okumura's
 * public-domain ar002 defines) or stored -lh0- when that comes out no bigger.
 *
 * READING takes level 0, 1 and 2 headers and -lh0-, -lh5- and -lhd- (a folder) entries,
 * which is what Amiga WHDLoad installs are packed with.
 */
#ifndef SWOSDATA_LHA_H
#define SWOSDATA_LHA_H

#include <stdint.h>

/* ---- writing ---- */
typedef struct LhaWriter LhaWriter;

/* Create `path`. NULL and a message in `err` (at least 256 bytes) on failure. */
LhaWriter *Lha_Create(const char *path, char *err);
/* A file, `path` from the archive's top with '/' between names ("Game/data/SWOS2").
 * `mtime` is a Unix time (0 = now). 0 on success. */
int32_t    Lha_AddFile(LhaWriter *w, const char *path, const uint8_t *data, uint32_t size,
                       int64_t mtime, char *err);
/* An empty folder ("Game/data/save/"). */
int32_t    Lha_AddDir(LhaWriter *w, const char *path, int64_t mtime, char *err);
/* Write the end marker and close. 0 on success; the writer is freed either way. */
int32_t    Lha_Close(LhaWriter *w, char *err);

/* ---- reading ---- */
typedef struct LhaArchive LhaArchive;

/* Read the whole archive at `path` into memory and index it. NULL and `err` when it is
 * not an LhA archive. */
LhaArchive *Lha_Open(const char *path, char *err);
void        Lha_Free(LhaArchive *a);
/* How many entries, and entry `i`'s full path ('/' between names; a folder ends in
 * '/') and unpacked size. */
int32_t     Lha_Count(const LhaArchive *a);
const char *Lha_Path(const LhaArchive *a, int32_t i);
uint32_t    Lha_Size(const LhaArchive *a, int32_t i);
/* Unpack entry `i`: a malloc'd buffer (free it), its CRC checked. NULL and `err` when it
 * is damaged or packed some other way. */
uint8_t    *Lha_Read(const LhaArchive *a, int32_t i, uint32_t *size, char *err);
/* The entry whose path is `path`, any case; -1 when there is none. */
int32_t     Lha_Find(const LhaArchive *a, const char *path);

#endif
