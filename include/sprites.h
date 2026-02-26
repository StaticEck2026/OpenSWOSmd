#ifndef SPRITES_H
#define SPRITES_H

#include "game_types.h"

void _CopySprite(uint16_t srcSprite, uint16_t dstSprite, uint16_t xOffset, uint16_t yOffset);
void _CopyWholeSprite(uint16_t srcSprite, uint16_t dstSprite);
void _ConvertSpriteColors(uint16_t spriteIndex, uint16_t *colorTable);

void _SortSprites(void);
void _InitDisplaySprites(void);

void _QueueMaskedSpriteBlitClipped(SpriteGraphics *sprGfx, int16_t x, int16_t y);
void _DrawSpriteClipped(SpriteGraphics *sprite, int16_t x, int16_t y);
void _DrawSprite_entry2(SpriteGraphics *sprite, int16_t x, int16_t y);
void _DrawSprite16Pixels(uint16_t spriteIndex, int16_t x, int16_t y);
void _DrawSpriteCentered(uint16_t spriteIndex, int16_t x, int16_t y);
void _DrawSpriteInCameraView(uint16_t spriteIndex, int16_t x, int16_t y);
void _DrawSpriteHotspotInCameraView(uint16_t spriteIndex, int16_t x, int16_t y);
void _DarkenRectangle(uint16_t spriteIndex, int16_t x, int16_t y);
void _DrawQueuedSprites(void);
void _RenderSpriteBandsToCopperLists(void);
void _BuildSpriteBandAndGeneralRenderStacks(void);
void _DrawSprites(void);

#endif
