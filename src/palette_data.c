/* palette_data.c — Palette data extracted from SWOS2.s / Data.c
 * NOT part of the build yet — lives in data/ for preparation.
 */
#include "palette_data.h"
/* display.h is the single source of `#define AGA`. Without it the #ifdef AGA
 * guards below silently take the OCS #else branch, compiling the pitch palettes
 * as [3] instead of [9] — WriteCopperPitchPalette then reads 6 entries off the
 * end of each, corrupting the pitch detail colours. */
#include "display.h"

#if PALETTE_DATA_C

/* blackPalette: 16 zero entries (SWOS2.s line ~2955) */
uint16_t _blackPalette[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/* panicPalette: 15 zeros + 0x000F (SWOS2.s line ~2967) */
uint16_t _panicPalette[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x000F
};

/* Pitch condition palettes. WriteCopperPitchPalette overlays these onto
 * gamePalette at _paletteIndices before pushing to the copper.
 *
 * AGA: 9 entries each — grass (indices 0/7/9) + line/detail (78/79/80/81/106/107).
 * Converted from SwosGfx DosPalette.Pitches[condition][0..8] EXACTLY as
 * GetPitchPalette() does: per 8-bit channel apply the DOS "grid" doubling
 * `(v & ~1) << 1` (clamp 255), THEN Amiga-12 (upper nibble). The doubling is
 * essential — without it the pitch renders half-bright (the first 3 grass
 * entries reproduce the OCS _normalPalette etc. only WITH the transform).
 * Order matches _pitchPalettePtrs (frozen/muddy/wet/soft/normal/dry/hard).
 * The OCS #else keeps the 3-entry grass-only palettes (SWOS2.s ~3681).
 *
 * (This AGA data was in the C port originally and got clobbered when the
 * palette data was regenerated from the OCS source — same as menu/gamePalette.) */
#ifdef AGA
uint16_t _normalPalette[9] = { 0x0790, 0x0690, 0x0990, 0x0BC9, 0x08A2, 0x0350, 0x0460, 0x0780, 0x0CB8 };
uint16_t _dryPalette[9]    = { 0x0990, 0x0890, 0x0790, 0x0BB9, 0x08B3, 0x0350, 0x0460, 0x0870, 0x0BA8 };
uint16_t _wetPalette[9]    = { 0x0390, 0x0790, 0x0390, 0x0BCA, 0x08A3, 0x0350, 0x0460, 0x0670, 0x0BB9 };
uint16_t _softPalette[9]   = { 0x0370, 0x0770, 0x0370, 0x0BC9, 0x0592, 0x0240, 0x0350, 0x0550, 0x0AA8 };
uint16_t _hardPalette[9]   = { 0x0970, 0x0870, 0x0770, 0x0CB9, 0x0B80, 0x0430, 0x0640, 0x0560, 0x0B97 };
uint16_t _frozenPalette[9] = { 0x0996, 0x0896, 0x0796, 0x0BBA, 0x0998, 0x0333, 0x0554, 0x0875, 0x0A98 };
uint16_t _muddyPalette[9]  = { 0x0750, 0x0650, 0x0550, 0x0CBA, 0x0B70, 0x0420, 0x0530, 0x0530, 0x0B97 };
#else
uint16_t _normalPalette[3] = { 0x0790, 0x0690, 0x0990 };
uint16_t _dryPalette[3]    = { 0x0990, 0x0890, 0x0790 };
uint16_t _wetPalette[3]    = { 0x0390, 0x0790, 0x0390 };
uint16_t _softPalette[3]   = { 0x0370, 0x0770, 0x0370 };
uint16_t _hardPalette[3]   = { 0x0970, 0x0870, 0x0770 };
uint16_t _frozenPalette[3] = { 0x0996, 0x0896, 0x0796 };
uint16_t _muddyPalette[3]  = { 0x0750, 0x0650, 0x0550 };
#endif

#endif /* PALETTE_DATA_C */
