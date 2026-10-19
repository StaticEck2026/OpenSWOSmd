/*
 * jag_tier.h -- the game's Jaguar sprite tier (bin/grafs_jag) from the Atari
 * Jaguar cartridge: swosgfx's Jaguar banks (jag2amiga.h, -jag2amiga) as the
 * files the game reads. The port of scripts/make_jag_graphics.py.
 *
 * The files (all unpacked):
 *   CJCTEAM1/2/3.RAW, CJCTEAMG.RAW, CJCBITS.RAW, CJCBENCH.RAW
 *       320x256 at 8 planes, row-interleaved (amiga_raw.h): 81920 bytes.
 *   JAG.PAL   "JGP1", 256 RGB triplets, then 256 flags: 1 where any of the six
 *             banks uses that index (index 0 included).
 *   JAGKITS.BIN  "JGK1", the 768 palette bytes, a count, then per graphic its
 *             width, height and w*h chunky pixels: the Jaguar's kit-screen art
 *             (shirts, icons, shorts, socks), cut from the UNPATCHED CJCGRAFS
 *             sheet by fixed boxes (src/kit_art.c reads them in that order).
 */
#ifndef SWOSDATA_JAG_TIER_H
#define SWOSDATA_JAG_TIER_H

#include <stdint.h>

#include "jag2amiga.h"
#include "sg_util.h"

/* One output file: its name in grafs_jag/ and its bytes (data malloc'd). */
typedef struct {
    char  name[16];
    SgBuf data;
} SdJagFile;

#define SD_JAG_FILE_COUNT 8     /* the six .RAW banks, JAG.PAL, JAGKITS.BIN */

/* The whole tier. `romPath`: the cartridge image; `dosDir`: the PC game folder;
 * `setupDir`: holds CJCTEAMn_CELEB.bmp (NULL or missing files: the PC
 * celebration instead). Fills out[0..SD_JAG_FILE_COUNT-1]; free with
 * Sd_JagFilesFree. 0 on success; on failure nothing is left allocated and
 * `err` (256 bytes) says why. `celebrations`, if not NULL, gets a bit per kit
 * bank (1 << SG_JAG_CJCTEAMn) whose hand-drawn celebration was used. */
int32_t Sd_JagBuild(const char *romPath, const char *dosDir, const char *setupDir,
                    SdJagFile out[SD_JAG_FILE_COUNT], uint32_t *celebrations, char *err);
void    Sd_JagFilesFree(SdJagFile out[SD_JAG_FILE_COUNT]);

#endif
