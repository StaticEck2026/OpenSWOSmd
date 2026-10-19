/*
 * adf.h -- read files out of an Amiga floppy image (.adf), OFS or FFS.
 *
 * Only what the data setup needs: open an image, look a file up by its path
 * ("grafs/CHARSET.RAW", case-insensitive like AmigaDOS) and read it whole, or
 * walk every file on the disk. Written from the AmigaDOS block layout; Michael
 * Steil's extract-adf (pagetable.com) was the reference for it.
 */
#ifndef SWOSDATA_ADF_H
#define SWOSDATA_ADF_H

#include <stdint.h>

typedef struct Adf Adf;

/* Load an image (DD 901120 or HD 1802240 bytes). NULL and a message in `err`
 * (at least 256 bytes) when it is not an AmigaDOS disk. */
Adf     *Adf_Open(const char *path, char *err);
void     Adf_Close(Adf *adf);

/* The volume name from the root block, e.g. "SWOS". */
const char *Adf_VolumeName(const Adf *adf);

/* Read a file by its path from the root, '/'-separated. Returns a malloc'd
 * buffer (free it) and its size, or NULL when the file is not there or its
 * blocks do not hold together (`err` says which). */
uint8_t *Adf_ReadFile(Adf *adf, const char *path, uint32_t *size, char *err);

/* Every file on the disk, depth first: cb(path, size, user). A non-zero return
 * from cb stops the walk and is returned. */
typedef int32_t (*AdfWalkFn)(const char *path, uint32_t size, void *user);
int32_t  Adf_Walk(Adf *adf, AdfWalkFn cb, void *user);

#endif
