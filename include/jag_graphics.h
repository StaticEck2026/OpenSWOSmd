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

/* How the sprites are shaded (docs/PALETTE_V2.md §5).
 *   outfit  0 = every kit shade is the flat team colour, as the 16-colour game
 *   shadow  0 = the drop shadow is SWOS's own colour 8, one near-black
 *   hair    0 = hair is what the game's own table makes it: a black head is the
 *           flat block the 16-colour game draws
 *   step    sixteenths between kit shades: 16, 16-step, 16-2*step, 16-3*step.
 *           Hair follows it too (a black head lifts by 3 x step).
 *           The Jaguar's own ratios are about 3.3, which takes a near-white sock
 *           down to 37.5% and reads as dark grey; 2 keeps it white. */
void JagGfx_SetShading(int32_t outfit, int32_t shadow, int32_t hair, int32_t step);

/* One shade of the kit ramp, in sixteenths of the colour (shade 0 = 16, the colour
 * itself). What shade_step set; the menus' Jaguar kit drawings use it too. */
int32_t JagGfx_KitShade(int32_t shade);

/* True for a bank the Jaguar tier provides (by the name an image script asks for,
 * with or without its folder). The loader uses it to know whose pixels it has. */
int32_t JagGfx_IsBank(const char *name);

/* Sheet colour -> palette entry, for a bank's pixels as they are decoded. Identity
 * below 16 and on the kit block (a team's own colours replace those later,
 * JagGfx_TeamTable); the art's other colours are installed in free entries as they
 * are first met. NULL if the tier is off or has no palette. */
const uint8_t *JagGfx_SheetRemap(void);

/* Move a chunky buffer's pixels off the entries this tier has taken for itself.
 * Those entries are exact duplicates of another entry, so every pixel moved lands
 * on the identical colour and nothing changes visibly -- which is what makes the
 * entry free. Call it on pitch tiles after they are decoded. Returns how many
 * pixels moved; 0 when the tier is off. docs/PALETTE_V2.md. */
int32_t JagGfx_RemapDonated(uint8_t *pixels, uint32_t count);

/* A match is being set up: the kit ramps of the last one are forgotten. */
void JagGfx_NewMatch(void);

/* The pitch's palette has just been applied (match_setup.c), so the shadow can be
 * worked out from the grass this match is played on. */
void JagGfx_PitchApplied(void);

/* The 256-entry table that recolours a Jaguar sprite, from the 16-entry one the
 * game builds for a team (shirt, stripes, shorts, socks, skin and hair). Each kit
 * shade becomes that colour's own shade; everything else keeps what the 16-entry
 * table says, or the colour the art was drawn in. */
void JagGfx_TeamTable(const uint16_t *amiga16, uint16_t *out256);

#endif /* JAG_GRAPHICS_H */
