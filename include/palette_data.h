/* palette_data.h — Palette data */
#ifndef PALETTE_DATA_H
#define PALETTE_DATA_H

#include "game_types.h"

/* paletteTable fade palettes. Unsized: 256 entries under AGA (SetFadeLevel
 * walks 8 banks x 32), 16 under OCS — see palette_data.c. */
extern uint16_t _blackPalette[];
extern uint16_t _panicPalette[];

/* OCS pitch condition palettes: 3 entries each.
 * (AGA mode uses 9 entries each — see SWOS2.s IFNE AGA blocks.)
 * These are partial palettes applied to gamePalette via pitchPalettePtrs[]. */
/* Unsized: 9 entries under AGA (grass + line/detail), 3 under OCS (grass). */
extern uint16_t _normalPalette[];
extern uint16_t _dryPalette[];
extern uint16_t _wetPalette[];
extern uint16_t _softPalette[];
extern uint16_t _hardPalette[];
extern uint16_t _frozenPalette[];
extern uint16_t _muddyPalette[];

#endif /* PALETTE_DATA_H */
