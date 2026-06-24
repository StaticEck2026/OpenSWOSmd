/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef SPRITES_H
#define SPRITES_H

#include "game_types.h"

void CopySprite(uint16_t srcSprite, uint16_t dstSprite, uint16_t xOffset, uint16_t yOffset);
void CopyWholeSprite(uint16_t srcSprite, uint16_t dstSprite);
void ConvertSpriteColors(uint16_t spriteIndex, uint16_t *colorTable);

void SortSprites(void);
void InitDisplaySprites(void);

void QueueMaskedSpriteBlitClipped(SpriteGraphics *sprGfx, int16_t x, int16_t y);
void DrawSpriteClipped(SpriteGraphics *sprite, int16_t x, int16_t y);
void DrawSprite_entry2(SpriteGraphics *sprite, int16_t x, int16_t y);
void DrawSprite16Pixels(uint16_t spriteIndex, int16_t x, int16_t y);
void DrawSpriteCentered(uint16_t spriteIndex, int16_t x, int16_t y);
void DrawSpriteInCameraView(uint16_t spriteIndex, int16_t x, int16_t y);
void DrawSpriteHotspotInCameraView(uint16_t spriteIndex, int16_t x, int16_t y);
void DarkenRectangle(uint16_t spriteIndex, int16_t x, int16_t y);
void DrawQueuedSprites(void);
void RenderSpriteBandsToCopperLists(void);
void BuildSpriteBandAndGeneralRenderStacks(void);
void DrawSprites(void);

#endif
