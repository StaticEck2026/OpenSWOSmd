#include "text_render.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint8_t *spriteGraphicsPtr;
extern uint32_t frontbufferBasePtr;
extern uint16_t cameraXMod320;
extern uint16_t cameraYMod320;
extern volatile uint16_t panicReasonCode;

extern void _PanicHaltLoop(void) __attribute__((noreturn));

/* ---- Display constants ---- */
#define SCREEN_WIDTH     336
#define SCREEN_HEIGHT    272
#define PLANE_STRIDE     44
#define ROW_STRIDE_BYTES 176
#define NUM_PLANES_SHIFT 2
#define PITCH_DEST_MOD_UNALIGNED 132

/* ---- Amiga hardware registers ---- */
#define CUSTOM_BASE      0x00DFF000u
#define DMACONR_ADDR     0x00DFF002u
#define DMACON_ADDR      0x00DFF096u

#define BLTCON0_OFF      64
#define BLTCON1_OFF      66
#define BLTAFWM_OFF      68
#define BLTALWM_OFF      70
#define BLTCPT_OFF       72
#define BLTBPT_OFF       76
#define BLTAPT_OFF       80
#define BLTDPT_OFF       84
#define BLTSIZE_OFF      88
#define BLTCMOD_OFF      96
#define BLTBMOD_OFF      98
#define BLTAMOD_OFF      100
#define BLTDMOD_OFF      102

#define DMACON_BLT_ON    33792u
#define DMACON_BLT_OFF   1024u
#define DMACONR_BLTBUSY_BIT 6

#define BLTCON0_BASE_LINKED  0x0FCA
#define BLTCON0_CLEAR_MASK   0x0B0A
#define BLTCON0_OR_MASK      0x0BFA

/* ---- Helper: wait for blitter idle ---- */
static void waitBlit(void)
{
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT))
        ;
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;
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
static void blitStringToFrontBuffer(uint16_t sprIdx, int16_t x, int16_t y, uint16_t planeMask)
{
    uint8_t *custom = (uint8_t *)CUSTOM_BASE;
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;

    /* Look up glyph sprite header */
    SpriteGraphics *spr = sprGfxTable[sprIdx];

    /* Glyph must have mask (type == 0) */
    if (spr->type != 0) {
        panicReasonCode = 0x1556;
        _PanicHaltLoop();
    }

    uint8_t *dstBase = (uint8_t *)(uint32_t)frontbufferBasePtr;
    uint16_t glyphHeight = (uint16_t)spr->nlines;

    /* Apply camera offset */
    x += (int16_t)cameraXMod320;
    y += (int16_t)cameraYMod320;
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

    *(volatile uint16_t *)(custom + BLTALWM_OFF) = lastWordMask;
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = 0xFFFF;

    /* BLTCON1: shift amount in bits 12-15 */
    uint16_t shiftVal = bitShift << 12;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = shiftVal;

    /* Glyph must have mask pointer */
    if (spr->maskPtr == 0) {
        panicReasonCode = 0x1556;
        _PanicHaltLoop();
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

    *(volatile uint16_t *)(custom + BLTAMOD_OFF) = srcStrideBytes - blitWidthBytes;
    *(volatile uint16_t *)(custom + BLTBMOD_OFF) = srcInterleavedBytes - blitWidthBytes;

    uint16_t dstMod = PLANE_STRIDE - blitWidthBytes + PITCH_DEST_MOD_UNALIGNED;
    *(volatile uint16_t *)(custom + BLTCMOD_OFF) = dstMod;
    *(volatile uint16_t *)(custom + BLTDMOD_OFF) = dstMod;

    /* Source data pointers */
    uint32_t maskPtr   = spr->maskPtr;
    uint32_t dataBase  = spr->sprData;
    uint32_t plane1Ptr = dataBase + srcStrideBytes;          /* skip plane 0 */
    uint32_t plane3Ptr = plane1Ptr + (uint32_t)srcStrideBytes * 2;  /* skip planes 1,2 */

restart_blit_span:
    ;
    uint16_t pMask = planeMask;
    uint8_t *dst = destPtr;

    /* Passes 0-2: planes 0, 1, 2 */
    for (int16_t pass = 2; pass >= 0; pass--) {
        uint16_t bit = pMask & 1;
        pMask >>= 1;

        if (bit) {
            /* Color bit set → cookie cut glyph through mask */
            *(volatile uint16_t *)(custom + BLTCON0_OFF) = shiftVal | BLTCON0_BASE_LINKED;
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = maskPtr;
            *(volatile uint32_t *)(custom + BLTBPT_OFF) = plane1Ptr;
            *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)dst;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)dst;
        } else {
            /* Color bit clear → clear through mask */
            *(volatile uint16_t *)(custom + BLTCON0_OFF) = shiftVal | BLTCON0_CLEAR_MASK;
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = maskPtr;
            *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)dst;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)dst;
        }

        /* Kick blit and wait */
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
        waitBlit();

        dst += PLANE_STRIDE;
    }

    /* Pass 3: plane 3 — asymmetric logic */
    {
        uint16_t bit3 = pMask & 1;

        if (bit3) {
            /* Color bit 3 set → OR mask into dest (set all masked pixels) */
            *(volatile uint16_t *)(custom + BLTCON0_OFF) = shiftVal | BLTCON0_OR_MASK;
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = maskPtr;
            *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)dst;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)dst;
        } else {
            /* Color bit 3 clear → cookie cut plane 3 glyph data */
            *(volatile uint16_t *)(custom + BLTCON0_OFF) = shiftVal | BLTCON0_BASE_LINKED;
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = maskPtr;
            *(volatile uint32_t *)(custom + BLTBPT_OFF) = plane3Ptr;
            *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)dst;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)dst;
        }

        /* Kick final blit */
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
    }

    /* Handle vertical wrap */
    if (wrapLines != 0) {
        /* Advance source pointers past the first span */
        uint32_t maskAdvance = (uint32_t)savedFirstHeight * srcWquads * 2;
        maskPtr += maskAdvance;
        uint32_t dataAdvance = maskAdvance << NUM_PLANES_SHIFT;
        plane3Ptr += dataAdvance;
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

/* ================================================================
 * GetTextWidth
 *
 * Measures the pixel width of a text string using a CharTable.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetTextWidth(uint8_t *text, uint8_t *charsTable)
{
    CharsTableHeader *ct = (CharsTableHeader *)charsTable;
    uint16_t sprIdxOff = (uint16_t)ct->spriteIndexOffset;
    uint16_t charSpacing = (uint16_t)ct->charSpacing;
    uint8_t *convTable = (uint8_t *)(ct + 1);
    uint16_t spaceWidth = (uint16_t)ct->spaceWidth;
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;

    uint16_t width = 0;
    uint8_t ch;

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
uint16_t _DrawMenuText(uint16_t x, uint16_t y, uint16_t color,
                       uint8_t *text, uint8_t *charsTable)
{
    CharsTableHeader *ct = (CharsTableHeader *)charsTable;
    uint16_t sprIdxOff = (uint16_t)ct->spriteIndexOffset;
    uint16_t charSpacing = (uint16_t)ct->charSpacing;
    uint8_t *convTable = (uint8_t *)(ct + 1);
    uint16_t spaceWidth = (uint16_t)ct->spaceWidth;
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;

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
uint16_t _DrawMenuTextRightAligned(uint16_t x, uint16_t y, uint16_t color,
                                   uint8_t *text, uint8_t *charsTable)
{
    uint16_t textWidth = _GetTextWidth(text, charsTable);
    uint16_t startX = x - textWidth + 1;
    return _DrawMenuText(startX, y, color, text, charsTable);
}

/* ================================================================
 * DrawMenuTextCentered
 *
 * Measures text width, then draws centered at x.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _DrawMenuTextCentered(uint16_t x, uint16_t y, uint16_t color,
                               uint8_t *text, uint8_t *charsTable)
{
    uint16_t textWidth = _GetTextWidth(text, charsTable);
    uint16_t halfWidth = (textWidth + 1) >> 1;
    uint16_t startX = x - halfWidth;
    return _DrawMenuText(startX, y, color, text, charsTable);
}
