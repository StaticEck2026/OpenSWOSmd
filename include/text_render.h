#ifndef TEXT_RENDER_H
#define TEXT_RENDER_H

#include "game_types.h"

uint16_t _GetTextWidth(uint8_t *text, uint8_t *charsTable);
uint16_t _DrawMenuText(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);
uint16_t _DrawMenuTextRightAligned(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);
uint16_t _DrawMenuTextCentered(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable);

#endif
