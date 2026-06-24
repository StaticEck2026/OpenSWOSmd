/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef TEXT_RENDER_H
#define TEXT_RENDER_H

#include "game_types.h"

uint16_t GetTextWidth(uint8_t *text, uint8_t *charsTable);
uint16_t DrawMenuText(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);
uint16_t DrawMenuTextRightAligned(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);
uint16_t DrawMenuTextCentered(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);

#endif
