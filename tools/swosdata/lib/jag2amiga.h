/*
 * jag2amiga.h -- OpenSWOS's Jaguar sprite tier (bin/grafs_jag) from the Atari
 * Jaguar cartridge, Sensible Soccer International Edition (1995).
 *
 * A port of two steps that used to run one after the other:
 *   1. SwosGfx -dos -jag2amiga (JagToAmiga.cs): unpack the cartridge's gameplay
 *      sheets and lay them into the Amiga bank layout, the PC game's sprites
 *      filling whatever the Jaguar has not got;
 *   2. scripts/make_jag_graphics.py: those 8bpp banks to the game's files.
 *
 * The cartridge. The game's art is in LZ-packed blobs, each unpacking to one
 * 320x256 8bpp sheet, at cartridge addresses (mapped at $800000):
 *     CJCGRAFS $8947C0  CJCBITS  $8993D8  CJCBENCH $89D0B0  CJCTEAMG $8A02E8
 *     CJCTEAM1 $8AC828  CJCTEAM2 $8AFB58  CJCTEAM3 $8B2FE0
 * The packing ("lz-bits", jagfx compression.py) is LZSS read MSB first from a
 * bit stream, with an 8 KB ring whose write position starts at 1:
 *     1 <8 bits>              a literal byte
 *     0 <13 bits> <4 bits>    copy len+3 bytes from ring[offset] -- an ABSOLUTE
 *                             ring position, not a distance back
 *     0 <13 zero bits>        end of stream
 * The in-game palette is 256 16-bit CRY words at file offset $6FA42: chroma in
 * the high byte picks an R/G/B from Atari's tables, intensity in the low byte
 * scales it (c * i / 255).
 *
 * The banks. The Jaguar port kept the Amiga's bank coordinates, so the Amiga's
 * own image-command scripts (dos2amiga.h) say where every sprite is. For each
 * placement the window lifted from the sheet is the PC sprite's size (clipped
 * to the script's window); a placement the sheet leaves empty, or which the
 * Jaguar used for something else (the bank table in jag2amiga.c), gets the PC
 * sprite instead, its colours matched to the nearest Jaguar ones. Before that
 * walk the sheets are patched:
 *   - CJCTEAMn: the goal celebration, which the Jaguar game has not got, from
 *     the project's own drawings <setupDir>/CJCTEAMn_CELEB.bmp (48x32, the bank
 *     block at (144,64), Jaguar palette). A missing file leaves the cells to
 *     the PC sprites, as SwosGfx does.
 *   - CJCBENCH: the white-shirted manager (sprites 1189-1194; the Jaguar drew
 *     only the brown one), made here from the PC sprites with the shadow colour
 *     (8) on the Jaguar's shadow index 30, as two 48x24 crops at (16,24) and
 *     (256,24); and the SELECT SUBSTITUTE arrow (1170, 9x8 at (272,120)), the
 *     Jaguar's own arrow mirrored to point right with its drop shadow cast again
 *     one pixel down-right in the Jaguar's shades. SwosGfx read these three as
 *     CJCBENCH_PATCH_<x>_<y>.bmp; they are derived art, so they are derived here.
 *   A crop REPLACES its rectangle, zeros included, and the cells it covers are
 *   taken from the sheet whatever the bank table says.
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
#ifndef SWOSDATA_JAG2AMIGA_H
#define SWOSDATA_JAG2AMIGA_H

#include <stdint.h>

#include "sd_image.h"
#include "sd_util.h"

#define SD_JAG_ROM_SIZE    2097152u     /* the 2 MB cartridge image */

/* The six banks the game loads, in the order of the files. */
typedef enum {
    SD_JAG_CJCTEAM1, SD_JAG_CJCTEAM2, SD_JAG_CJCTEAM3,
    SD_JAG_CJCTEAMG, SD_JAG_CJCBITS, SD_JAG_CJCBENCH,
    SD_JAG_BANK_COUNT
} SdJagBank;

extern const char *const Sd_JagBankNames[SD_JAG_BANK_COUNT];   /* "CJCTEAM1", ... */

/* One output file: its name in grafs_jag/ and its bytes (data malloc'd). */
typedef struct {
    char  name[16];
    SdBuf data;
} SdJagFile;

#define SD_JAG_FILE_COUNT 8     /* the six .RAW banks, JAG.PAL, JAGKITS.BIN */

/* The whole tier. `romPath`: the cartridge image; `dosDir`: the PC game folder;
 * `setupDir`: holds CJCTEAMn_CELEB.bmp (NULL or missing files: the PC
 * celebration instead). Fills out[0..SD_JAG_FILE_COUNT-1]; free with
 * Sd_JagFilesFree. 0 on success; on failure nothing is left allocated and
 * `err` (256 bytes) says why. `celebrations`, if not NULL, gets a bit per kit
 * bank (1 << SD_JAG_CJCTEAMn) whose hand-drawn celebration was used. */
int32_t Sd_JagBuild(const char *romPath, const char *dosDir, const char *setupDir,
                    SdJagFile out[SD_JAG_FILE_COUNT], uint32_t *celebrations, char *err);
void    Sd_JagFilesFree(SdJagFile out[SD_JAG_FILE_COUNT]);

/* The step before the files, for tools and tests: the six banks as 8bpp
 * pictures in the Jaguar palette (SwosGfx's Amiga_Images_256_Jag/<BANK>.bmp),
 * plus the raw CJCGRAFS sheet (its CJCGRAFS_JAGSHEET.bmp). Free each with
 * SdImage_Free. `rom` is the cartridge image in memory. */
int32_t Sd_JagBanks(const SdBuf *rom, const char *dosDir, const char *setupDir,
                    SdImage banks[SD_JAG_BANK_COUNT], SdImage *grafsSheet,
                    uint32_t *celebrations, char *err);

/* The pieces. Unpack the lz-bits stream at cartridge address (or file offset)
 * `address` into exactly `size` bytes (malloc'd into out). 0 on success, -1 when
 * the address is outside the image or the stream ends short. */
int32_t Sd_JagUnpack(const SdBuf *rom, uint32_t address, uint32_t size, SdBuf *out, char *err);
/* The in-game palette, 0x00RRGGBB. 0 on success. */
int32_t Sd_JagGamePalette(const SdBuf *rom, uint32_t pal[256], char *err);

#endif
