/* jag_graphics.h — the Atari Jaguar sprite tier. src/jag_graphics.c says how it works. */
#ifndef JAG_GRAPHICS_H
#define JAG_GRAPHICS_H

/* game_types.h, not <stdint.h>: this header is included from files that build for
 * the m68k target too, and that toolchain is -nostdlib. */
#include "game_types.h"

/* The kit in the Jaguar art: four slots of four shades, at the top of its palette.
 * Shade 0 is the colour itself, 3 the deepest shadow. */
#define JAG_KIT_FIRST       240
#define JAG_KIT_SLOTS       4
#define JAG_KIT_SHADES      4

/* --dos-graphics's opposite number: bin/grafs_jag instead of the Amiga banks. */
int32_t JagGfx_Enabled(void);
void    JagGfx_SetEnabled(int32_t on);

/* True for a bank the Jaguar tier provides (by the name an image script asks for,
 * with or without its folder). The loader uses it to know whose pixels it has. */
int32_t JagGfx_IsBank(const char *name);

/* Sheet colour -> palette entry, for a bank's pixels as they are decoded. Identity
 * below 16 and on the kit block (a team's own colours replace those later,
 * JagGfx_TeamTable); the art's other colours are installed in free entries as they
 * are first met. NULL if the tier is off or has no palette. */
const uint8_t *JagGfx_SheetRemap(void);

/* A match is being set up: the kit ramps of the last one are forgotten. */
void JagGfx_NewMatch(void);

/* The 256-entry table that recolours a Jaguar sprite, from the 16-entry one the
 * game builds for a team (shirt, stripes, shorts, socks, skin and hair). Each kit
 * shade becomes that colour's own shade; everything else keeps what the 16-entry
 * table says, or the colour the art was drawn in. */
void JagGfx_TeamTable(const uint16_t *amiga16, uint16_t *out256);

#endif /* JAG_GRAPHICS_H */
