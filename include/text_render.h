/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef TEXT_RENDER_H
#define TEXT_RENDER_H

#include "game_types.h"

/* Glyph drop-shadow plane. The SWOS_Font charset is a 4bpp asset: the glyph BODY
 * is colour 2 (bit 1) and the drop SHADOW is colour 8 (bit 3), regardless of the
 * display bit depth. Use this FIXED plane for the shadow, NOT NUM_PLANES-1 --
 * on the 8-plane (AGA/SDL) build NUM_PLANES-1 is bit 7, which the 4bpp glyph never
 * sets, so the shadow silently vanished. (NUM_PLANES-1 == 3 only on the 4-plane
 * OCS build, which is why it used to work.) */
#define GLYPH_SHADOW_PLANE   3

uint16_t GetTextWidth(uint8_t *text, uint8_t *charsTable);
uint16_t DrawMenuText(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);
uint16_t DrawMenuTextRightAligned(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);
uint16_t DrawMenuTextCentered(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);

#endif
