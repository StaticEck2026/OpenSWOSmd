/* md_display.c -- the Amiga display entry points the match engine calls, for the VDP.
 *
 * On the Amiga these drive double-buffered bitplanes, the blitter and the copper. The
 * Mega Drive draws nothing here: the pitch is a VDP plane, every object a hardware
 * sprite, and md_render.c builds both from the state these functions leave. What IS kept
 * exactly is everything the match reads back:
 *
 *   - the camera, including the per-buffer camera state SwapBuffersAndCameraViewState
 *     saves and restores (the scroll's "did the camera move" flag is measured against
 *     the buffer's own last camera, so a single-buffered port would drift from the
 *     original's break timers);
 *   - DrawSprites' `onScreen` cull, which the AI reads to pick the controlled player;
 *   - the sprite list, sorted as the original sorts it.
 *
 * Function bodies that are copies of src/display.c and src/sprites.c say so; keep them
 * in step if those change.
 */
#include "md_hw.h"
#include "md_game.h"
#include "md_render.h"
#include "game_types.h"
#include "display.h"
#include "sprites.h"

/* ---- buffer/camera state (src/display.c) ---- */
extern uint16_t _cameraXMod320, _cameraYMod320;
extern int16_t _oldCameraX, _oldCameraY;
extern uint16_t _dstXByteOffsetSaved0, _dstYStartLinesSaved0;
extern int16_t _oldCameraXSaved0, _oldCameraYSaved0, _cameraXSaved0, _cameraYSaved0;
extern uint16_t _dstXByteOffsetSaved1, _dstYStartLinesSaved1;
extern int16_t _oldCameraXSaved1, _oldCameraYSaved1, _cameraXSaved1, _cameraYSaved1;
extern uint16_t _paletteIndex;

extern FixedPoint _cameraX, _cameraY;

static uint8_t s_bufferAlt;   /* which of the two Amiga buffers is the drawing one */

/* SwapBuffersAndCameraViewState (display.c, SWOS2.s line 6224): the camera half. */
void SwapBuffersAndCameraViewState(void)
{
    if (!s_bufferAlt) {
        s_bufferAlt = 1;
        _dstXByteOffsetSaved0 = _cameraXMod320;
        _dstYStartLinesSaved0 = _cameraYMod320;
        _oldCameraXSaved0 = _oldCameraX;
        _oldCameraYSaved0 = _oldCameraY;
        _cameraXSaved0 = _cameraX.s.i;
        _cameraYSaved0 = _cameraY.s.i;
        _cameraXMod320 = _dstXByteOffsetSaved1;
        _cameraYMod320 = _dstYStartLinesSaved1;
        _oldCameraX = _oldCameraXSaved1;
        _oldCameraY = _oldCameraYSaved1;
        _cameraX.s.i = _cameraXSaved1;
        _cameraY.s.i = _cameraYSaved1;
    } else {
        s_bufferAlt = 0;
        _dstXByteOffsetSaved1 = _cameraXMod320;
        _dstYStartLinesSaved1 = _cameraYMod320;
        _oldCameraXSaved1 = _oldCameraX;
        _oldCameraYSaved1 = _oldCameraY;
        _cameraXSaved1 = _cameraX.s.i;
        _cameraYSaved1 = _cameraY.s.i;
        _cameraXMod320 = _dstXByteOffsetSaved0;
        _cameraYMod320 = _dstYStartLinesSaved0;
        _oldCameraX = _oldCameraXSaved0;
        _oldCameraY = _oldCameraYSaved0;
        _cameraX.s.i = _cameraXSaved0;
        _cameraY.s.i = _cameraYSaved0;
    }
}

/* SetCameraCoordinates (display.c, SWOS2.s line 6158): a pixel at a time, as the original,
 * because the Mod320 counters wrap on the way. */
void SetCameraCoordinates(int16_t targetX, int16_t targetY)
{
    int16_t panRange = (int16_t)(0x2A0 - SCREEN_WIDTH);
    if (targetX < 0) targetX = 0;
    else if (targetX > panRange) targetX = panRange;

    while (_cameraX.s.i != targetX) {
        if (_cameraX.s.i < targetX) {
            _cameraX.s.i += 1;
            _cameraXMod320 += 1;
            if (_cameraXMod320 == SCROLL_X_WRAP) _cameraXMod320 = 0;
        } else {
            _cameraX.s.i -= 1;
            _cameraXMod320 -= 1;
            if ((int16_t)_cameraXMod320 < 0) _cameraXMod320 = SCROLL_X_WRAP - 1;
        }
    }
    while (_cameraY.s.i != targetY) {
        if (_cameraY.s.i < targetY) {
            _cameraY.s.i += 1;
            _cameraYMod320 += 1;
            if (_cameraYMod320 == 272) _cameraYMod320 = 0;
        } else {
            _cameraY.s.i -= 1;
            _cameraYMod320 -= 1;
            if ((int16_t)_cameraYMod320 < 0) _cameraYMod320 = 271;
        }
    }
}

/* DoubleBufferSwapSequence (display.c): palette 0, camera to 0,0, both buffers. */
void DoubleBufferSwapSequence(void)
{
    _paletteIndex = 0;
    SetCameraCoordinates(0, 0);
    SwapBuffersAndCameraViewState();
    SwapBuffersAndCameraViewState();
}

/* ---- the Amiga's drawing machinery: nothing to do on the VDP ---- */
volatile uint16_t g_blitQActive;
uint16_t g_scrollTwinQueue;
void UpdatePlanePtrsAndBlitParams(void) {}
void MirrorFrontbuffer(void) {}
void ClearFrontbuffers(void) {}
void ProcessSpriteBlitQueue(void) {}
void ProcessSpriteBlitQueueAsync(void) {}
void BlitQueueDrain(void) {}
void RestoreWindowFromTwins(void) {}
void BlitterCopyPanelToScreen(void) {}
void ScrollXUpdateColumnsAndBlit(void) {}
void ScrollYUpdateRowsAndBlit(void) {}
void HWSprite_LoadFromSpriteGraphics(uint16_t spriteIndex) { (void)spriteIndex; }
void BlitSpritePlanar_Long_StridedDest(uint16_t spriteIndex) { (void)spriteIndex; }
void BlitSpritePlanar_Word_StridedDest(uint16_t spriteIndex) { (void)spriteIndex; }

/* Sprite pixels are never rewritten on the cartridge: the kits are palette lines
 * (md_render.c builds them from the colour tables these calls are given), and the art
 * lives in ROM. */
void ConvertSpriteColors(uint16_t spriteIndex, uint16_t *colorTable) { (void)spriteIndex; (void)colorTable; }
void CopySprite(uint16_t srcSprite, uint16_t dstSprite, uint16_t a, uint16_t b)
{
    (void)srcSprite; (void)dstSprite; (void)a; (void)b;
}

/* Text2Sprite renders a name into a sprite's bitmap and returns its width; the HUD on the
 * cartridge is drawn from the names themselves (md_render.c), so only the width -- which
 * the game positions things by -- is computed, from the font's character widths. */
uint16_t Text2Sprite(uint16_t xOff, uint16_t yOff, uint16_t dstSpriteIndex, uint8_t *text,
                     uint8_t *charTable, uint16_t planeMask, uint16_t *outTotalWidth)
{
    uint16_t w = 0;
    (void)xOff; (void)yOff; (void)dstSpriteIndex; (void)charTable; (void)planeMask;
    if (text) for (; *text; text++) w += (*text == ' ') ? 4 : 6;
    if (outTotalWidth) *outTotalWidth = w;
    return w;
}

/* The overlays drawn into the frame (statistics, the bench, the result panel) go
 * through these; md_render.c draws its own panels from the same state. */
void DarkenRectangle(uint16_t spriteIndex, int16_t x, int16_t y) { (void)spriteIndex; (void)x; (void)y; }
void DrawSprite16Pixels(uint16_t spriteIndex, int16_t x, int16_t y) { (void)spriteIndex; (void)x; (void)y; }
void DrawSpriteInCameraView(uint16_t spriteIndex, int16_t x, int16_t y) { (void)spriteIndex; (void)x; (void)y; }
void DrawSpriteHotspotInCameraView(uint16_t spriteIndex, int16_t x, int16_t y) { (void)spriteIndex; (void)x; (void)y; }
void FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
uint16_t DrawMenuText(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable)
{
    (void)x; (void)y; (void)color; (void)text; (void)charsTable;
    return 0;
}

/* ---- sprites (src/sprites.c) ---- */
extern Sprite *_sortedSprites[];
extern Sprite **_allSpritesArray;
extern uint16_t _numSpritesToRender;
extern uint8_t *_spriteGraphicsPtr;

const Sprite *g_spriteAnimVisible[SPRITE_ANIMVIS_MAX];
int16_t g_spriteAnimVisibleCount;

/* SortSprites (sprites.c, SWOS2.s line 7434): the original's bubble sort, unchanged --
 * equal-y sprites keep the order it leaves them in, and that order is the draw order. */
void SortSprites(void)
{
    int16_t n = (int16_t)_numSpritesToRender - 2;
    if (n < 0) return;
    for (;;) {
        int16_t swapped = 0;
        Sprite **p = _sortedSprites;
        for (int16_t i = n; i >= 0; i--) {
            Sprite *s1 = p[0], *s2 = p[1];
            if (s1->y.s.i > s2->y.s.i) { p[0] = s2; p[1] = s1; swapped = 1; }
            p++;
        }
        if (!swapped) return;
    }
}

/* InitDisplaySprites (sprites.c, SWOS2.s line 7412). */
void InitDisplaySprites(void)
{
    uint16_t count = 0;
    Sprite **dst = _sortedSprites, **src = _allSpritesArray;
    for (;;) {
        Sprite *spr = *src++;
        if ((uintptr_t)spr == (uintptr_t)-1) break;
        if (spr->visible == 0) continue;
        *dst++ = spr;
        count++;
    }
    _numSpritesToRender = count;
    SortSprites();
}

int Sprite_AnimVisible(const Sprite *s)
{
    for (int16_t i = 0; i < g_spriteAnimVisibleCount; i++)
        if (g_spriteAnimVisible[i] == s) return 1;
    return 0;
}

#define HUD_LAYER_Z_MIN 8000

MdSprCmd md_sprList[MD_SPRLIST_MAX];
uint16_t md_sprCount;

/* DrawSprites (sprites.c, SWOS2.s line 7322): sort, then BuildSpriteBandAndGeneralRenderStacks'
 * cull -- `onScreen` exactly as the original decides it, against the 336x272 simulation
 * window -- and, instead of the band/bob routing, the list md_render.c turns into VDP
 * sprites. The list runs front to back: sprites further down the pitch first, as the VDP
 * shows earlier sprites over later ones. HUD sprites (z >= 8000) are listed too; the
 * renderer draws the HUD itself and skips them. */
void DrawSprites(void)
{
    SortSprites();
    g_spriteAnimVisibleCount = 0;
    md_sprCount = 0;

    int16_t n = (int16_t)_numSpritesToRender;
    SpriteGraphics **table = (SpriteGraphics **)_spriteGraphicsPtr;
    Sprite **a2 = _sortedSprites + n;
    while (a2 != _sortedSprites) {
        Sprite *a1 = *--a2;
        int16_t idx = a1->imageIndex;
        if (idx < 0) continue;
        SpriteGraphics *a0 = table[(uint16_t)idx];
        int16_t d1 = a1->x.s.i - a0->centerX - _cameraX.s.i;
        int16_t d2 = a1->y.s.i - a0->centerY - _cameraY.s.i - a1->z.s.i;
        int16_t negW = -a0->pixWidth, negH = -a0->nlines;
        int onScreen = !(d1 >= SCREEN_WIDTH || d2 >= _sim_view_height || d1 <= negW || d2 <= negH);
        a1->onScreen = (int16_t)onScreen;
        if (!onScreen) continue;
        a1->field_56 = 0;
        if (md_sprCount < MD_SPRLIST_MAX) {
            MdSprCmd *c = &md_sprList[md_sprCount++];
            c->spr = a1;
            c->image = (uint16_t)idx;
            c->x = d1;
            c->y = d2;
        }
    }
}

/* LoadOverlayToFrontbuffer (src/file_io.c): a full-screen picture straight into the frame.
 * The cartridge's screens are drawn by md_frontend.c. */
void LoadOverlayToFrontbuffer(const uint8_t *filename) { (void)filename; }
