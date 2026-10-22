/*
 * adf.h -- read files out of an Amiga disk image: a floppy (.adf) or a hard disk
 * (.hdf), OFS or FFS.
 *
 * Only what the data setup needs: open an image, look a file up by its path
 * ("grafs/CHARSET.RAW", case-insensitive like AmigaDOS) and read it whole, or
 * walk every file on the disk. Written from the AmigaDOS block layout; Michael
 * Steil's extract-adf (pagetable.com) was the reference for it.
 *
 * A hard disk image is a single bare partition (ASWOS's, sensiblesoccer.de's) or a
 * whole disk with a Rigid Disk Block and several partitions (WinUAE); each partition
 * is a VOLUME, numbered from 0. A floppy and a bare partition have one.
 */
#ifndef SWOSDATA_ADF_H
#define SWOSDATA_ADF_H

#include <stdint.h>

typedef struct Adf Adf;

/* Open an image's first volume. The file stays open (read block by block) until
 * Adf_Close. NULL and a message in `err` (at least 256 bytes) when it is not an
 * AmigaDOS disk. */
Adf     *Adf_Open(const char *path, char *err);
/* The same for volume `index` of a partitioned hard disk image. */
Adf     *Adf_OpenVolume(const char *path, int32_t index, char *err);
/* How many volumes the image holds: 1 for a floppy or a bare partition, 0 when it is
 * not an Amiga disk image at all. Reads the partition table only. */
int32_t  Adf_CountVolumes(const char *path);
void     Adf_Close(Adf *adf);

/* The volume name from the root block, e.g. "SWOS". */
const char *Adf_VolumeName(const Adf *adf);

/* The volume name without opening the disk: reads the size, the boot block's
 * "DOS" and the root block only -- cheap enough to ask of every .adf in a big
 * collection. 0 with the name in out (31 chars + NUL), -1 when the file is not
 * an AmigaDOS disk image. */
int32_t     Adf_PeekVolumeName(const char *path, char out[32]);

/* Read a file by its path from the root, '/'-separated. Returns a malloc'd
 * buffer (free it) and its size, or NULL when the file is not there or its
 * blocks do not hold together (`err` says which). */
uint8_t *Adf_ReadFile(Adf *adf, const char *path, uint32_t *size, char *err);

/* A file's size, or -1 when `path` is not a file on the disk. Reads its header only. */
int64_t  Adf_FileSize(Adf *adf, const char *path);

/* Every file on the disk, depth first: cb(path, size, user). A non-zero return
 * from cb stops the walk and is returned. */
typedef int32_t (*AdfWalkFn)(const char *path, uint32_t size, void *user);
int32_t  Adf_Walk(Adf *adf, AdfWalkFn cb, void *user);

#endif
