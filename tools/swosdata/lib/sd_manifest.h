/*
 * sd_manifest.h -- which files the setup takes from the player's game.
 */
#ifndef SWOSDATA_MANIFEST_H
#define SWOSDATA_MANIFEST_H

#include <stddef.h>

typedef struct {
    const char *dest;   /* under the assets folder, e.g. "sound_dos/fx/cheer.raw" */
    const char *src;    /* in the PC game folder, as a GOG install spells it */
} SdCopy;

/* NULL-terminated paths, the same on the disks and in the assets folder. */
extern const char *const g_sdAmigaCopy[];
/* Ends with { NULL, NULL }. */
extern const SdCopy g_sdDosCopy[];
/* SWOS 2020's pitches, src in its game folder (swos/). Ends with { NULL, NULL }. */
extern const SdCopy g_sd2020Copy[];

#endif
