/* palette_data.h — Palette data */
#ifndef PALETTE_DATA_H
#define PALETTE_DATA_H

#include "game_types.h"

/* Full 16-color palettes (all zeros / near-zeros — runtime-filled) */
extern uint16_t _blackPalette[16];
extern uint16_t _panicPalette[16];

/* OCS pitch condition palettes: 3 entries each.
 * (AGA mode uses 9 entries each — see SWOS2.s IFNE AGA blocks.)
 * These are partial palettes applied to gamePalette via pitchPalettePtrs[]. */
extern uint16_t _normalPalette[3];
extern uint16_t _dryPalette[3];
extern uint16_t _wetPalette[3];
extern uint16_t _softPalette[3];
extern uint16_t _hardPalette[3];
extern uint16_t _frozenPalette[3];
extern uint16_t _muddyPalette[3];

#endif /* PALETTE_DATA_H */
