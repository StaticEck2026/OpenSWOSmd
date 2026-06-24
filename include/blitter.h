/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef BLITTER_H
#define BLITTER_H

#include "game_types.h"

void FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void RectRowMaskedApplyScreenOffset(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *mask, uint8_t *buf);

void BlitSpritePlanar_Word(uint16_t spriteIndex);
void BlitSpritePlanar_Word_StridedDest(uint16_t spriteIndex);
void BlitSpritePlanar_Long_StridedDest(uint16_t spriteIndex);

void BlitGlyphToSpriteBitmap(uint16_t glyphIndex, uint16_t xOff, uint16_t yOff, uint16_t planeMask, uint16_t dstSpriteIndex);

uint16_t Text2Sprite(uint16_t xOff, uint16_t yOff, uint16_t dstSpriteIndex,
                      uint8_t *text, uint8_t *charsTable, uint16_t planeMask,
                      uint16_t *outTotalWidth);

#endif
