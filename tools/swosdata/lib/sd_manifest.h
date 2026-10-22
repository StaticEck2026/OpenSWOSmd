/*
 * sd_manifest.h -- which files the setup takes from the player's game.
 */
#ifndef SWOSDATA_MANIFEST_H
#define SWOSDATA_MANIFEST_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *dest;   /* under the assets folder, e.g. "sound_dos/fx/cheer.raw" */
    const char *src;    /* in the PC game folder, as a GOG install spells it */
} SdCopy;

/* NULL-terminated paths, the same on the disks and in the assets folder. */
extern const char *const g_sdAmigaCopy[];

/* The Amiga game's team data, told apart by a fingerprint of the files (see
 * SdSources_AmigaSeason): the 96/97 release, and the community's later seasons as they
 * are found in the wild. */
typedef struct {
    uint64_t    fingerprint;
    const char *season;     /* "96/97", "24/25" ... */
    const char *where;      /* which copy it was taken from */
} SdAmigaSeason;
extern const SdAmigaSeason g_sdAmigaSeasons[];
/* Ends with { NULL, NULL }. */
extern const SdCopy g_sdDosCopy[];
/* SWOS 2020's pitches, src in its game folder (swos/). Ends with { NULL, NULL }. */
extern const SdCopy g_sd2020Copy[];

#endif
