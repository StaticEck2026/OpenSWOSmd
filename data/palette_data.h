/* palette_data.h — Palette data extracted from SWOS2.s / Data.c
 * NOT part of the build yet — lives in data/ for preparation.
 */
#ifndef PALETTE_DATA_H
#define PALETTE_DATA_H

#include <stdint.h>

/* Full 16-color palettes (all zeros / near-zeros — runtime-filled) */
extern uint16_t blackPalette[16];
extern uint16_t panicPalette[16];

/* OCS pitch condition palettes: 3 entries each.
 * (AGA mode uses 9 entries each — see SWOS2.s IFNE AGA blocks.)
 * These are partial palettes applied to gamePalette via pitchPalettePtrs[]. */
extern uint16_t normalPalette[3];
extern uint16_t dryPalette[3];
extern uint16_t wetPalette[3];
extern uint16_t softPalette[3];
extern uint16_t hardPalette[3];
extern uint16_t frozenPalette[3];
extern uint16_t muddyPalette[3];

#endif /* PALETTE_DATA_H */
