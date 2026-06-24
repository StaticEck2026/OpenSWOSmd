/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "text_render.h"
#include "display.h"
#ifdef SWOS_SDL_BUILD
#include <stdint.h>
#include <stddef.h>
#endif

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint8_t *_spriteGraphicsPtr;
extern volatile uintptr_t _frontbufferBasePtr;
extern uint16_t _cameraXMod320;
extern uint16_t _cameraYMod320;
extern volatile uint16_t _panicReasonCode;

extern void PanicHaltLoop(void) __attribute__((noreturn));


/* ---- Helper: wait for blitter idle ---- */
static inline __attribute__((always_inline)) void waitBlit(void)
{
    _custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(_custom->dmaconr & (1u << 14));
    while (_custom->dmaconr & (1u << 14))
        ;
    _custom->dmacon = (uint16_t)DMACON_BLT_OFF;
}

/* ================================================================
 * blitStringToFrontBuffer  (static — only called from DrawMenuText)
 *
 * Renders a single glyph character from a sprite font to the
 * frontbuffer using the Amiga blitter.  4-pass operation (one per
 * bitplane).  Handles vertical screen wrap.
 *
 * d0=sprIdx  d1=x  d2=y  d3=planeMask
 * ================================================================ */
#ifdef SWOS_SDL_BUILD
/* SDL build: render a glyph from chunky 8bpp sprite data to the chunky
 * 8bpp frontbuffer. Sprite data layout (built by `blitDispatch`):
 *   - 1 byte per pixel = palette index (0..NUM_COLOURS-1)
 *   - row stride = wquads * 16 bytes
 *   - palette index 0 = transparent
 *
 * Body/shadow remap: glyph fonts encode the body in palette index bit 1
 * (= colour 2) and the shadow in bit (NUM_PLANES-1) (= colour 8 for OCS).
 * The Amiga renderer used the `color` parameter to override the body
 * colour while preserving the shadow. We do the same:
 *   bodyBit   = (src >> 1) & 1
 *   shadowBit = (src >> (NUM_PLANES-1)) & 1
 *   For each output palette bit p:
 *     if p == NUM_PLANES-1: bit = (color>>p)&1 ? 1 : shadowBit
 *     else                : bit = (color>>p)&1 ? bodyBit : 0
 *
 * Pixels with both bodyBit and shadowBit clear are transparent. */
static void blitGlyphBitplanes(uint16_t sprIdx, int16_t dstX, int16_t dstY,
                               uint16_t color)
{
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)_spriteGraphicsPtr;
    SpriteGraphics *spr = sprGfxTable[sprIdx];
    if (!spr) return;

    const uint8_t *data = (const uint8_t *)(uintptr_t)spr->sprData;
    uint8_t *dstBase = (uint8_t *)(uintptr_t)_frontbufferBasePtr + _g_menu_draw_x_offset;
    if (!data || !dstBase) return;

    int16_t glyphW = spr->pixWidth;
    int16_t glyphH = spr->nlines;
    int16_t pitch  = spr->wquads * 16;       /* chunky row stride */

    for (int16_t py = 0; py < glyphH; py++) {
        int16_t dy = dstY + py;
        if (dy < 0 || dy >= SCREEN_HEIGHT) continue;

        const uint8_t *srcRow = data + (size_t)py * pitch;
        uint8_t       *dstRow = dstBase + (size_t)dy * FRONTBUFFER_ROW_STRIDE_BYTES;

        for (int16_t px = 0; px < glyphW; px++) {
            int16_t dx = dstX + px;
            if (dx < 0 || dx >= _g_screen_width) continue;

            uint8_t src = srcRow[px];
            uint8_t bodyBit   = (uint8_t)((src >> 1) & 1u);
            uint8_t shadowBit = (uint8_t)((src >> (NUM_PLANES - 1)) & 1u);
            if (bodyBit == 0 && shadowBit == 0) continue;

            uint8_t out = 0;
            for (uint16_t p = 0; p < NUM_PLANES - 1; p++) {
                if ((color >> p) & 1u) out |= (uint8_t)(bodyBit << p);
            }
            if ((color >> (NUM_PLANES - 1)) & 1u)
                out |= (uint8_t)(1u << (NUM_PLANES - 1));
            else
                out |= (uint8_t)(shadowBit << (NUM_PLANES - 1));

            if (out) dstRow[dx] = out;
        }
    }
}

#endif

#ifdef SWOS_SDL_BUILD
static void blitStringToFrontBuffer(uint16_t sprIdx, int16_t x, int16_t y, uint16_t planeMask)
{
    /* planeMask is the color value here (DrawMenuText passes `color`).
     *
     * Do NOT apply cameraXMod320/cameraYMod320 on SDL. Those are Amiga
     * hardware-scroll wrap offsets used when the chip display pans its
     * bitplane base pointer — correct on Amiga but wrong for our chunky
     * frontbuffer that we redraw top-down each frame (same reasoning as
     * blitChunkySprite in sprites.c). When the in-game camera leaves
     * those offsets non-zero and we land in a menu, the text Y would
     * wrap through SCREEN_HEIGHT and multiple glyphs land on overlapping
     * rows — producing "SSEEIECTS MORE TEAMS" in place of
     * "SELECT 2 MORE TEAMS" on the post-match team-select screen. */
    blitGlyphBitplanes(sprIdx, x, y, planeMask);
}
#else
static void blitStringToFrontBuffer(uint16_t sprIdx, int16_t x, int16_t y, uint16_t planeMask)
{
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)_spriteGraphicsPtr;

    /* Look up glyph sprite header */
    SpriteGraphics *spr = sprGfxTable[sprIdx];

    /* Glyph must have mask (type == 0) */
    if (spr->type != 0) {
        _panicReasonCode = 0x1556;
        PanicHaltLoop();
    }

    uint8_t *dstBase = (uint8_t *)_frontbufferBasePtr + _g_menu_draw_x_offset;
    uint16_t glyphHeight = (uint16_t)spr->nlines;

    /* Apply camera offset */
    x += (int16_t)_cameraXMod320;
    y += (int16_t)_cameraYMod320;
    if (y >= SCREEN_HEIGHT)
        y -= SCREEN_HEIGHT;

    /* Wait for any previous blit */
    waitBlit();

    /* Compute horizontal shift and edge masks */
    uint16_t bitShift = (uint16_t)x & 0xF;
    uint16_t glyphPixMod = (uint16_t)spr->pixWidth & 0xF;
    if (glyphPixMod == 0)
        glyphPixMod = 16;
    uint16_t totalBits = bitShift + glyphPixMod;

    uint16_t blitWords = (uint16_t)spr->wquads + 1;
    uint16_t lastWordMask = 0;

    if (totalBits <= 16) {
        lastWordMask = 0xFFFF;
        blitWords--;
    }

    _custom->bltalwm = lastWordMask;
    _custom->bltafwm = 0xFFFF;

    /* BLTCON1: shift amount in bits 12-15 */
    uint16_t shiftVal = bitShift << 12;
    _custom->bltcon1 = shiftVal;

    /* Glyph must have mask pointer */
    if (spr->maskPtr == 0) {
        _panicReasonCode = 0x1556;
        PanicHaltLoop();
    }

    /* Compute vertical wrap */
    uint16_t wrapLines = 0;
    uint16_t firstHeight = glyphHeight;
    if ((uint16_t)y + glyphHeight > SCREEN_HEIGHT) {
        wrapLines = (uint16_t)y + glyphHeight - SCREEN_HEIGHT;
        firstHeight = glyphHeight - wrapLines;
    }

    uint16_t savedFirstHeight = firstHeight;

    /* Compute destination pointer */
    int16_t wordOffset = ((int16_t)x >> 4) << 1;
    int32_t dstXByteOffset = (int32_t)(int16_t)wordOffset;
    uint8_t *destPtr = dstBase + (uint16_t)y * (uint32_t)ROW_STRIDE_BYTES + dstXByteOffset;

    /* BLTSIZE = height << 6 | blitWords */
    uint16_t bltsize = (firstHeight << 6) | blitWords;

    /* Compute modulos */
    uint16_t blitWidthBytes = blitWords << 1;
    uint16_t srcWquads = (uint16_t)spr->wquads;
    uint16_t srcStrideBytes = srcWquads << 1;
    uint16_t srcInterleavedBytes = srcStrideBytes << NUM_PLANES_SHIFT;

    _custom->bltamod = srcStrideBytes - blitWidthBytes;
    _custom->bltbmod = srcInterleavedBytes - blitWidthBytes;

    uint16_t dstMod = PLANE_STRIDE - blitWidthBytes + PITCH_DEST_MOD_UNALIGNED;
    _custom->bltcmod = dstMod;
    _custom->bltdmod = dstMod;

    /* Source data pointers */
    uint32_t maskPtr   = (uint32_t)spr->maskPtr;
    uint32_t dataBase  = (uint32_t)spr->sprData;
    uint32_t plane1Ptr = dataBase + srcStrideBytes;          /* skip plane 0 */
    uint32_t planeLastPtr = plane1Ptr + (uint32_t)srcStrideBytes * (NUM_PLANES - 2);  /* last plane */

restart_blit_span:
    ;
    uint16_t pMask = planeMask;
    uint8_t *dst = destPtr;

    /* Passes 0 through NUM_PLANES-2 */
    for (int16_t pass = NUM_PLANES - 2; pass >= 0; pass--) {
        uint16_t bit = pMask & 1;
        pMask >>= 1;

        if (bit) {
            /* Color bit set → cookie cut glyph through mask */
            _custom->bltcon0 = shiftVal | BLTCON0_BASE_LINKED;
            _custom->bltapt = maskPtr;
            _custom->bltbpt = plane1Ptr;
            _custom->bltcpt = (uint32_t)dst;
            _custom->bltdpt = (uint32_t)dst;
        } else {
            /* Color bit clear → clear through mask */
            _custom->bltcon0 = shiftVal | BLTCON0_CLEAR_MASK;
            _custom->bltapt = maskPtr;
            _custom->bltcpt = (uint32_t)dst;
            _custom->bltdpt = (uint32_t)dst;
        }

        /* Kick blit and wait */
        _custom->bltsize = bltsize;
        waitBlit();

        dst += PLANE_STRIDE;
    }

    /* Final pass: last plane — asymmetric logic */
    {
        uint16_t bitLast = pMask & 1;

        if (bitLast) {
            /* Color bit set → OR mask into dest (set all masked pixels) */
            _custom->bltcon0 = shiftVal | BLTCON0_OR_MASK;
            _custom->bltapt = maskPtr;
            _custom->bltcpt = (uint32_t)dst;
            _custom->bltdpt = (uint32_t)dst;
        } else {
            /* Color bit clear → cookie cut last plane glyph data */
            _custom->bltcon0 = shiftVal | BLTCON0_BASE_LINKED;
            _custom->bltapt = maskPtr;
            _custom->bltbpt = planeLastPtr;
            _custom->bltcpt = (uint32_t)dst;
            _custom->bltdpt = (uint32_t)dst;
        }

        /* Kick final blit */
        _custom->bltsize = bltsize;
    }

    /* Handle vertical wrap */
    if (wrapLines != 0) {
        /* Advance source pointers past the first span */
        uint32_t maskAdvance = (uint32_t)savedFirstHeight * srcWquads * 2;
        maskPtr += maskAdvance;
        uint32_t dataAdvance = maskAdvance << NUM_PLANES_SHIFT;
        planeLastPtr += dataAdvance;
        plane1Ptr += dataAdvance;

        /* Reset dest to top of screen at same x offset */
        destPtr = dstBase + dstXByteOffset;

        /* New BLTSIZE for wrapped portion */
        bltsize = (wrapLines << 6) | (blitWidthBytes >> 1);
        wrapLines = 0;

        /* Wait for the plane 3 blit to finish before restarting */
        waitBlit();
        goto restart_blit_span;
    }
}
#endif /* !SWOS_SDL_BUILD */

/* ================================================================
 * GetTextWidth
 *
 * Measures the pixel width of a text string using a CharTable.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t GetTextWidth(uint8_t *text, uint8_t *charsTable)
{
    CharsTableHeader *ct = (CharsTableHeader *)charsTable;
    uint16_t sprIdxOff = (uint16_t)ct->spriteIndexOffset;
    uint16_t charSpacing = (uint16_t)ct->charSpacing;
    uint16_t *convTable = (uint16_t *)(ct + 1);
    uint16_t spaceWidth = (uint16_t)ct->spaceWidth;
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)_spriteGraphicsPtr;

    uint16_t width = 0;
    uint8_t ch;

    /* TEMP diagnostic — narrow to the player-name path: smallCharsTable
     * (sprIdxOff=0), text starting with a digit, and only print the
     * first 8 unique strings. The match-init makePlayerNameSprite
     * calls produce bogus widths even though identical lookups via
     * the frontend menus return sane values, which suggests sprite
     * slots that hold glyphs get reloaded between phases. */
    while ((ch = *text++) != 0) {
        if (ch == ' ') {
            width += spaceWidth;
        } else {
            uint16_t idx = (uint16_t)convTable[(uint8_t)(ch - ' ')];
            idx += sprIdxOff;
            SpriteGraphics *sprHeader = sprGfxTable[idx];
            uint16_t pixWidth = (uint16_t)sprHeader->pixWidth;
            width += pixWidth + charSpacing;
        }
    }

    return width;
}

/* ================================================================
 * DrawMenuText
 *
 * Renders a text string to the frontbuffer character-by-character.
 * Returns the final x position (next character would start here).
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t DrawMenuText(uint16_t x, uint16_t y, uint16_t color,
                       uint8_t *text, uint8_t *charsTable)
{
    CharsTableHeader *ct = (CharsTableHeader *)charsTable;
    uint16_t sprIdxOff = (uint16_t)ct->spriteIndexOffset;
    uint16_t charSpacing = (uint16_t)ct->charSpacing;
    uint16_t *convTable = (uint16_t *)(ct + 1);
    uint16_t spaceWidth = (uint16_t)ct->spaceWidth;
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)_spriteGraphicsPtr;

    int16_t curX = (int16_t)x;
    int16_t nextX = curX;
    uint8_t ch;

    while ((ch = *text++) != 0) {
        if (ch == ' ') {
            nextX = curX + (int16_t)spaceWidth;
            if (curX < 0) {
                curX = nextX;
                continue;
            }
            if ((uint16_t)nextX >= SCREEN_WIDTH)
                break;
            curX = nextX;
            continue;
        }

        /* Convert character to sprite index */
        uint16_t idx = (uint16_t)convTable[(uint8_t)(ch - ' ')];
        idx += sprIdxOff;
        uint16_t sprIdx = idx;

        SpriteGraphics *sprHeader = sprGfxTable[idx];
        uint16_t pixWidth = (uint16_t)sprHeader->pixWidth;
        nextX = curX + (int16_t)pixWidth + (int16_t)charSpacing;

        if (curX < 0) {
            curX = nextX;
            continue;
        }
        if ((uint16_t)nextX >= SCREEN_WIDTH)
            break;

        blitStringToFrontBuffer(sprIdx, curX, (int16_t)y, color);
        curX = nextX;
    }

    return (uint16_t)nextX;
}

/* ================================================================
 * DrawMenuTextRightAligned
 *
 * Measures text width, then draws right-aligned at x.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t DrawMenuTextRightAligned(uint16_t x, uint16_t y, uint16_t color,
                                   uint8_t *text, uint8_t *charsTable)
{
    uint16_t textWidth = GetTextWidth(text, charsTable);
    uint16_t startX = x - textWidth + 1;
    return DrawMenuText(startX, y, color, text, charsTable);
}

/* ================================================================
 * DrawMenuTextCentered
 *
 * Measures text width, then draws centered at x.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t DrawMenuTextCentered(uint16_t x, uint16_t y, uint16_t color,
                               uint8_t *text, uint8_t *charsTable)
{
    uint16_t textWidth = GetTextWidth(text, charsTable);
    uint16_t halfWidth = (textWidth + 1) >> 1;
    uint16_t startX = x - halfWidth;
    return DrawMenuText(startX, y, color, text, charsTable);
}
