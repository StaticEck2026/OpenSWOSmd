/* palette_data.c — Palette data extracted from SWOS2.s / Data.c
 * NOT part of the build yet — lives in data/ for preparation.
 */
#include "palette_data.h"

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

/* OCS pitch condition palettes: 3 entries each (SWOS2.s line ~3681, ELSE branch of AGA) */

uint16_t _normalPalette[3] = { 0x0790, 0x0690, 0x0990 };
uint16_t _dryPalette[3]    = { 0x0990, 0x0890, 0x0790 };
uint16_t _wetPalette[3]    = { 0x0390, 0x0790, 0x0390 };
uint16_t _softPalette[3]   = { 0x0370, 0x0770, 0x0370 };
uint16_t _hardPalette[3]   = { 0x0970, 0x0870, 0x0770 };
uint16_t _frozenPalette[3] = { 0x0996, 0x0896, 0x0796 };
uint16_t _muddyPalette[3]  = { 0x0750, 0x0650, 0x0550 };

#endif /* PALETTE_DATA_C */
