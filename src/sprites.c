#include "sprites.h"
#include "display.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint8_t *spriteGraphicsPtr;
extern volatile uint16_t panicReasonCode;

extern uint32_t allSpritesArray[];     /* Null-terminated (0xFFFFFFFF) pointer array */
extern uint32_t sortedSprites[];       /* 78 entries max */
extern uint16_t numSpritesToRender;

extern volatile uint32_t spriteBlitQueue;         /* 4-byte var holding pointer to blit queue */
extern volatile uint32_t frontbufferBasePtr;      /* 4-byte var holding pointer to frontbuffer */
extern uint8_t frontbufferMaskBuffer[];  /* BSS label: mask buffer base address */
extern int16_t cameraX;
extern int16_t cameraY;
extern uint16_t cameraXMod320;
extern uint16_t cameraYMod320;
extern uint16_t overlayActiveFlag;

/* Render stacks: xxxPtr are 4-byte pointer vars, xxxBase are BSS labels */
extern uint32_t renderStackPtr;
extern uint8_t renderStackBase[];
extern uint32_t plane01RenderStackPtr;
extern uint8_t plane01RenderStackBase[];
extern uint32_t plane23RenderStackPtr;
extern uint8_t plane23RenderStackBase[];

/* Copper list plane pointers (4-byte vars) */
extern volatile uint32_t bitPlane0BasePtr;
extern volatile uint32_t bitPlane1BasePtr;
extern volatile uint32_t bitPlane2BasePtr;
extern volatile uint32_t bitPlane3BasePtr;
extern uint32_t copperZeroLong;

/* Player sprite identification for DrawQueuedSprites */
extern Sprite goalie1Sprite;

extern void _PanicHaltLoop(void) __attribute__((noreturn));
extern void _SaveCoordinatesForHighlights(int16_t spriteIndexOrMarker, uint16_t x, uint16_t y);


/* ---- Helper: wait for blitter idle ---- */
static void waitBlit(void)
{
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    /* First btst is a timing pad — the blitter busy bit may not be set yet
       immediately after BLTSIZE write. This matches the asm double-check pattern. */
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT))
        ;
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;
}

/* ================================================================
 * CopySprite
 *
 * Copies one sprite into another at a given offset using the Amiga
 * blitter.  Handles both linked-plane (with mask) and separate-plane
 * (no mask) sprite formats.
 *
 * asm: d0=srcSprite, d1=xOffset, d2=yOffset, d3=dstSprite
 * C bridge: (srcSprite, dstSprite, xOffset, yOffset)
 * ================================================================ */
__attribute__((used, externally_visible))
void _CopySprite(uint16_t srcSprite, uint16_t dstSprite,
                 uint16_t xOffset, uint16_t yOffset)
{
    uint8_t *custom = (uint8_t *)CUSTOM_BASE;
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;

    /* Load destination sprite header */
    SpriteGraphics *dstHdr = sprGfxTable[dstSprite];
    /* Load source sprite header */
    SpriteGraphics *srcHdr = sprGfxTable[srcSprite];

    /* Destination tile stride in bytes (per plane row) */
    uint16_t dstStrideBytes = (uint16_t)dstHdr->wquads << 1;
    /* Destination sprite data pointer */
    uint8_t *dstData = (uint8_t *)dstHdr->sprData;

    /* Wait for blitter idle */
    waitBlit();

    /* Compute horizontal shift and edge masks */
    uint16_t bitShift = xOffset & 0xF;
    uint16_t srcPixMod = (uint16_t)srcHdr->pixWidth & 0xF;
    if (srcPixMod == 0)
        srcPixMod = 16;
    uint16_t totalBits = bitShift + srcPixMod;

    uint16_t blitWords = (uint16_t)srcHdr->wquads + 1;
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

    /* Check if source has mask data (offset 4) */
    uint32_t srcMaskPtr = srcHdr->maskPtr;
    uint16_t bltcon0;
    if (srcMaskPtr != 0) {
        /* Linked mode: cookie cut with mask */
        bltcon0 = shiftVal | BLTCON0_BASE_LINKED;
    } else {
        /* Single mode: straight copy, no mask (A = all 1s via BLTADAT) */
        bltcon0 = shiftVal | BLTCON0_BASE_SINGLE;
        *(volatile uint16_t *)(custom + BLTADAT_OFF) = 0xFFFF;
    }
    *(volatile uint16_t *)(custom + BLTCON0_OFF) = bltcon0;

    /* Compute destination byte offset for xOffset */
    uint32_t wordByteOff = (uint32_t)((xOffset >> 3) & 0xFFFE);

    /* Compute y offset in interleaved plane data */
    uint16_t dstInterleavedStride = dstStrideBytes << NUM_PLANES_SHIFT;
    uint32_t yByteOff = (uint32_t)yOffset * dstInterleavedStride;
    uint32_t dstAddr = (uint32_t)dstData + yByteOff + wordByteOff;

    *(volatile uint32_t *)(custom + BLTCPT_OFF) = dstAddr;
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = dstAddr;

    /* Source sprite type: offset 8 */
    uint16_t srcType = srcHdr->type;
    uint16_t blitWidthBytes = blitWords << 1;

    if (srcType != 0) {
        /* ---- Linked / interleaved path ---- */
        uint16_t nlines = (uint16_t)srcHdr->nlines;
        uint16_t bltsize = ((nlines << NUM_PLANES_SHIFT) << 6) | blitWords;

        /* Source A/B modulo */
        uint16_t srcStride = (uint16_t)srcHdr->wquads << 1;
        *(volatile uint16_t *)(custom + BLTAMOD_OFF) = srcStride - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTBMOD_OFF) = srcStride - blitWidthBytes;

        /* Destination C/D modulo */
        *(volatile uint16_t *)(custom + BLTCMOD_OFF) = dstStrideBytes - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTDMOD_OFF) = dstStrideBytes - blitWidthBytes;

        /* Source pointers */
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = srcHdr->sprData;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcMaskPtr;

        /* Kick blit */
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
    } else {
        /* ---- Four separate planes path ---- */
        uint16_t nlines = (uint16_t)srcHdr->nlines;
        uint16_t bltsize = (nlines << 6) | blitWords;

        /* B modulo: src interleaved stride (4 planes) - blit width */
        uint16_t srcWquads = (uint16_t)srcHdr->wquads;
        uint16_t srcBmod = (srcWquads << NUM_PLANES_SHIFT_PLUS1) - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTBMOD_OFF) = srcBmod;

        /* A modulo: per-plane stride - blit width */
        uint16_t srcAmod = (srcWquads << 1) - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTAMOD_OFF) = srcAmod;

        /* C/D modulo: dst interleaved stride - blit width */
        uint16_t dstCDmod = (dstStrideBytes << NUM_PLANES_SHIFT) - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTCMOD_OFF) = dstCDmod;
        *(volatile uint16_t *)(custom + BLTDMOD_OFF) = dstCDmod;

        /* Source data and mask pointers */
        uint32_t srcDataPtr = srcHdr->sprData;
        uint32_t srcMask = srcHdr->maskPtr;  /* may be 0 for single-plane */

        /* Per-plane stride for advancing between passes */
        uint32_t dstPlaneStep = (uint32_t)dstStrideBytes;
        uint32_t srcPlaneStep = (uint32_t)(srcWquads << 1);

        /* Pass 1 */
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = srcDataPtr;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcMask;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;

        /* Advance to plane 1 */
        dstAddr += dstPlaneStep;
        srcDataPtr += srcPlaneStep;

        /* Pass 2 */
        waitBlit();
        *(volatile uint32_t *)(custom + BLTCPT_OFF) = dstAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = dstAddr;
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = srcDataPtr;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcMask;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;

        /* Advance to plane 2 */
        dstAddr += dstPlaneStep;
        srcDataPtr += srcPlaneStep;

        /* Pass 3 */
        waitBlit();
        *(volatile uint32_t *)(custom + BLTCPT_OFF) = dstAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = dstAddr;
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = srcDataPtr;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcMask;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;

        /* Advance to plane 3 */
        dstAddr += dstPlaneStep;
        srcDataPtr += srcPlaneStep;

        /* Pass 4 */
        waitBlit();
        *(volatile uint32_t *)(custom + BLTCPT_OFF) = dstAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = dstAddr;
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = srcDataPtr;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcMask;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
    }
}

/* ================================================================
 * CopyWholeSprite
 *
 * Copies entire source sprite into destination sprite at offset (0,0).
 * ================================================================ */
__attribute__((used, externally_visible))
void _CopyWholeSprite(uint16_t srcSprite, uint16_t dstSprite)
{
    _CopySprite(srcSprite, dstSprite, 0, 0);
}

/* ================================================================
 * ConvertSpriteColors
 *
 * Re-palettizes a 4-plane sprite using a color lookup table.
 * Each pixel's 4-bit color index is looked up in colorTable
 * and the result is written back.
 *
 * Two modes:
 *   - Fast (*colorTable != 0): remaps ALL pixels including color 0
 *   - Slow (*colorTable == 0): remaps only non-zero pixels (preserves transparency)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ConvertSpriteColors(uint16_t spriteIndex, uint16_t *colorTable)
{
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;
    SpriteGraphics *spr = sprGfxTable[spriteIndex];

    uint16_t wquads = (uint16_t)spr->wquads;
    uint16_t nlines = (uint16_t)spr->nlines;

    /* Per-plane row size in bytes */
    uint16_t planeRowBytes = wquads << 1;

    /* Inter-plane skip: after processing one row's worth of words from each plane,
       each pointer has advanced by planeRowBytes. To reach the next row we need to
       skip the remaining (NUM_PLANES-1) planes */
    int32_t interplaneSkip;

    if (*colorTable != 0) {
        /* ---- Fast path: remap all pixels ---- */
        interplaneSkip = (int32_t)((uint16_t)(wquads * (NUM_PLANES - 1) * 2));
    } else {
        /* ---- Slow path: bounds check ---- */
        if (wquads > 0x14) {
            panicReasonCode = 0x982;
            _PanicHaltLoop();
        }
        interplaneSkip = (int32_t)((uint16_t)(wquads * (NUM_PLANES - 1) * 2));
        if (interplaneSkip < 0) {
            panicReasonCode = 0x987;
            _PanicHaltLoop();
        }
    }

    /* Set up plane pointers */
    uint16_t *dataBase = (uint16_t *)spr->sprData;
    uint16_t *planes[NUM_PLANES];
    int16_t pl;
    for (pl = 0; pl < NUM_PLANES; pl++)
        planes[pl] = (uint16_t *)((uint8_t *)dataBase + pl * planeRowBytes);

    uint16_t skipTransparent = (*colorTable == 0);

    uint16_t line;
    for (line = 0; line < nlines; line++) {
        uint16_t quad;
        for (quad = 0; quad < wquads; quad++) {
            /* Read one word from each plane */
            uint16_t w[NUM_PLANES];
            for (pl = 0; pl < NUM_PLANES; pl++)
                w[pl] = *planes[pl];

            /* Process 16 pixels (MSB to LSB) */
            uint16_t pixel;
            for (pixel = 0; pixel < 16; pixel++) {
                /* Extract N-bit color from MSBs of each plane word */
                uint16_t colorIdx = 0;
                for (pl = NUM_PLANES - 1; pl >= 0; pl--) {
                    colorIdx = (colorIdx << 1) | ((w[pl] >> 15) & 1);
                    w[pl] <<= 1;
                }

                /* Skip transparent pixels in slow path */
                if (skipTransparent && colorIdx == 0)
                    continue;

                /* Look up new color */
                uint16_t newColor = colorTable[colorIdx];

                /* Distribute new color bits back to plane words */
                for (pl = 0; pl < NUM_PLANES; pl++) {
                    if (newColor & (1 << pl))
                        w[pl] |= 1;
                }
            }

            /* Write back modified words */
            for (pl = 0; pl < NUM_PLANES; pl++)
                *planes[pl]++ = w[pl];
        }

        /* Skip to next line (advance past remaining planes) */
        for (pl = 0; pl < NUM_PLANES; pl++)
            planes[pl] = (uint16_t *)((uint8_t *)planes[pl] + interplaneSkip);
    }
}

/* ================================================================
 * SortSprites  (SWOS2.s line 7434)
 *
 * Improved bubble sort on sortedSprites[] by SPRITE_Y.
 * Stops early if no swaps occurred in a pass.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SortSprites(void)
{
    int16_t n = (int16_t)numSpritesToRender - 2;
    if (n < 0)
        return;

    uint32_t *base = sortedSprites;

    /* Assembly does NOT shrink the inner loop — always a full pass of n+1
     * comparisons, repeating until no swaps occur (pure unoptimized bubble sort). */
    for (;;) {
        int16_t swapped = 0;
        uint32_t *p = base;
        int16_t i;
        for (i = n; i >= 0; i--) {
            Sprite *s1 = (Sprite *)p[0];
            Sprite *s2 = (Sprite *)p[1];
            int16_t y1 = s1->y.s.i;
            int16_t y2 = s2->y.s.i;
            if (y1 > y2) {
                /* swap */
                p[0] = (uint32_t)s2;
                p[1] = (uint32_t)s1;
                swapped = 1;
            }
            p++;
        }
        if (!swapped)
            return;
    }
}

/* ================================================================
 * InitDisplaySprites  (SWOS2.s line 7412)
 *
 * Copies visible sprites from allSpritesArray into sortedSprites,
 * counts them into numSpritesToRender, then sorts by Y.
 * allSpritesArray is null-terminated with 0xFFFFFFFF.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitDisplaySprites(void)
{
    uint16_t count = 0;
    uint32_t *dst = sortedSprites;
    uint32_t *src = allSpritesArray;

    for (;;) {
        uint32_t ptr = *src++;
        if (ptr == 0xFFFFFFFF)
            break;
        Sprite *spr = (Sprite *)ptr;
        if (spr->visible == 0)
            continue;
        *dst++ = ptr;
        count++;
    }
    numSpritesToRender = count;
    _SortSprites();
}

/* ================================================================
 * DrawSpriteClipped  (SWOS2.s line 6563)
 *
 * Main clipped sprite blitter. Clips sprite to screen bounds,
 * applies camera offset wrapping, programs the Amiga blitter
 * for cookie-cut (masked) or straight copy across 4 bitplanes.
 * Handles Y-wrap around the circular framebuffer.
 *
 * Sprite header (a0):
 *   +0  = data pointer (plane interleaved)
 *   +4  = mask pointer (0 = no mask / single-plane mode)
 *   +8  = type (SPRITE_ANIMATIONTABLE+2; nonzero = branch_linked_planes)
 *   +10 = pixel width
 *   +12 = nlines (height)
 *   +14 = TILE_STRIDE (wquads)
 *   +16 = centerX
 *   +18 = centerY
 *
 * Queue entry format (12 bytes each):
 *   +0  = mask buffer address
 *   +4  = frontbuffer address
 *   +8  = modulo/stride
 *   +10 = bltsize (4-plane height)
 * Terminated by 0xFFFFFFFF.
 * ================================================================ */

/* ---- Sprite blitter constants ---- */
#define SPRITE_MAX_Y_DRAW       257
/* SPRITE_BLTSIZE_W1, SPRITE_BLTSIZE_W2 now defined in display.h */

/* ================================================================
 * QueueMaskedSpriteBlitClipped  (SWOS2.s line 13202)
 *
 * Blits a masked player sprite (cookie-cut) into the frontbuffer.
 * Validates sprite header (nlines==15, pixWidth<=16, type!=0),
 * clamps to screen, applies camera offset with Y-wrap, then
 * programs blitter and queues a blit entry for later replay.
 *
 * Two code paths based on horizontal overlap:
 *   1-word: sprite + x_offset fits in 16 pixels
 *   2-word: sprite spans into next word
 *
 * If validation fails, falls through to DrawSpriteClipped.
 *
 * in: a0=spriteGraphics, d1=x, d2=y
 * ================================================================ */
__attribute__((used, externally_visible))
void _QueueMaskedSpriteBlitClipped(SpriteGraphics *a0, int16_t d1, int16_t d2)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;

    /* Validate sprite: nlines must be 15, pixWidth <= 16, type != 0 */
    if ((uint16_t)a0->nlines != 15)
        goto fallback;
    if ((uint16_t)a0->pixWidth > 16)
        goto fallback;
    if (a0->type == 0)
        goto fallback;

    /* Y must be non-negative and within drawable range */
    if (d2 < 0)
        goto fallback;
    if (d2 > SPRITE_MAX_Y_DRAW)
        goto fallback;

    /* Apply camera offsets */
    d1 += (int16_t)cameraXMod320;
    d2 += (int16_t)cameraYMod320;

    /* Wrap Y if beyond screen height */
    if (d2 >= SCREEN_HEIGHT)
        d2 -= SCREEN_HEIGHT;

    /* Re-validate Y after wrap */
    if (d2 > SPRITE_MAX_Y_DRAW) {
        /* Undo camera offsets and fall through to DrawSpriteClipped */
        d1 -= (int16_t)cameraXMod320;
        d2 -= (int16_t)cameraYMod320;
        goto fallback;
    }

    {
        /* Check horizontal overlap: (x & 15) + pixWidth */
        uint16_t xFrac = (uint16_t)d1 & TILE_MASK;
        uint16_t pixW = (uint16_t)a0->pixWidth;
        uint16_t overlap = xFrac + pixW;

        /* Compute blitter shift value: (x & 15) << 12 */
        uint16_t shiftVal = xFrac << 12;

        /* Compute destination address */
        uint32_t *a3 = (uint32_t *)spriteBlitQueue;
        uint8_t *a1 = (uint8_t *)frontbufferBasePtr;

        int16_t xWordOff = (d1 >> TILE_SHIFT_X) << 1; /* asr.w + asl.w (signed) */
        int32_t xWordOffL = (int32_t)xWordOff; /* sign-extend */
        uint32_t yOff = (uint16_t)d2 * (uint32_t)ROW_STRIDE_BYTES;
        uint32_t destAddr = (uint32_t)a1 + yOff + (uint32_t)xWordOffL;

        /* Wait for blitter and program registers */
        waitBlit();

        /* BLTCON1 = shift, BLTCON0 = shift + cookie-cut minterm */
        *(volatile uint16_t *)(custom + BLTCON1_OFF) = shiftVal;
        *(volatile uint16_t *)(custom + BLTCON0_OFF) = shiftVal + BLTCON0_BASE_LINKED;

        /* Destination */
        *(volatile uint32_t *)(custom + BLTCPT_OFF) = destAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = destAddr;

        /* Queue entry: [maskAddr(4), destAddr(4), mod(2), bltsize(2)] */
        *(uint32_t *)((uint8_t *)a3 + 4) = destAddr;
        uint32_t maskBufAddr = destAddr - (uint32_t)a1 + (uint32_t)frontbufferMaskBuffer;
        *(uint32_t *)((uint8_t *)a3 + 0) = maskBufAddr;

        /* Source pointers: data=BLTBPT, mask=BLTAPT */
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = a0->sprData;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = a0->maskPtr;

        /* First word mask always all-ones */
        *(volatile uint16_t *)(custom + BLTAFWM_OFF) = 0xFFFF;

        if (overlap <= 16) {
            /* ---- Single-word path ---- */
            *(volatile uint16_t *)(custom + BLTAMOD_OFF) = 0;
            *(volatile uint16_t *)(custom + BLTBMOD_OFF) = 0;
            *(volatile uint16_t *)(custom + BLTCMOD_OFF) = PLANE_STRIDE_MINUS2;
            *(volatile uint16_t *)(custom + BLTDMOD_OFF) = PLANE_STRIDE_MINUS2;
            *(uint16_t *)((uint8_t *)a3 + 8) = PLANE_STRIDE_MINUS2;
            *(volatile uint16_t *)(custom + BLTALWM_OFF) = 0xFFFF;
            *(volatile uint16_t *)(custom + BLTSIZE_OFF) = SPRITE_BLTSIZE_W1;
            *(uint16_t *)((uint8_t *)a3 + 10) = SPRITE_BLTSIZE_W1;
        } else {
            /* ---- Two-word path ---- */
            *(volatile uint16_t *)(custom + BLTAMOD_OFF) = 0xFFFE; /* -2 */
            *(volatile uint16_t *)(custom + BLTBMOD_OFF) = 0xFFFE;
            *(volatile uint16_t *)(custom + BLTCMOD_OFF) = PLANE_STRIDE_MINUS4;
            *(volatile uint16_t *)(custom + BLTDMOD_OFF) = PLANE_STRIDE_MINUS4;
            *(uint16_t *)((uint8_t *)a3 + 8) = PLANE_STRIDE_MINUS4;
            *(volatile uint16_t *)(custom + BLTALWM_OFF) = 0;
            *(volatile uint16_t *)(custom + BLTSIZE_OFF) = SPRITE_BLTSIZE_W2;
            *(uint16_t *)((uint8_t *)a3 + 10) = SPRITE_BLTSIZE_W2;
        }

        /* Advance queue pointer, write sentinel */
        a3 = (uint32_t *)((uint8_t *)a3 + 12);
        *(uint32_t *)a3 = BLT_BLITQ_END;
        spriteBlitQueue = (uint32_t)a3;
    }
    return;

fallback:
    _DrawSpriteClipped(a0, d1, d2);
}

/* Local statics matching SWOS2.s local data (queuedLineCount, savedSrcPlaneB, savedSrcPlaneA) */
static uint16_t queuedLineCount;
static uint32_t savedSrcPlaneA;
static uint32_t savedSrcPlaneB;

__attribute__((used, externally_visible))
void _DrawSpriteClipped(SpriteGraphics *a0, int16_t d1, int16_t d2)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    uint16_t sprType = a0->type;

    if (sprType != 0) {
        /* ---- branch_linked_planes path ---- */
        uint32_t *a3 = (uint32_t *)spriteBlitQueue;
        uint8_t *a1 = (uint8_t *)frontbufferBasePtr;
        int16_t d3 = a0->nlines;
        int16_t d5 = 0;

        /* Clip top */
        if (d2 < 0) {
            d5 = -d2;
            d3 += d2;
            d2 = 0;
        }
        /* Clip bottom */
        {
            int16_t d0 = d2 + d3 - SCREEN_HEIGHT;
            if (d0 > 0)
                d3 -= d0;
        }

        /* Apply camera offsets with Y wrap */
        d1 += cameraXMod320;
        d2 += cameraYMod320;
        if (d2 >= SCREEN_HEIGHT)
            d2 -= SCREEN_HEIGHT;

        /* Compute edge masks and word count */
        uint16_t d6 = 0;
        uint16_t bitPos = (uint16_t)d1 & TILE_MASK;
        uint16_t sprPixMod = (uint16_t)a0->pixWidth & TILE_MASK;
        if (sprPixMod == 0)
            sprPixMod = 16;
        uint16_t totalBits = bitPos + sprPixMod;
        uint16_t d7 = (uint16_t)a0->wquads + 1;  /* WQUADS for linked planes */
        if (totalBits <= 16) {
            d6 = 0xFFFF;
            d7--;
        }

        /* Wait for blitter idle and program registers */
        waitBlit();
        *(volatile uint16_t *)(custom + BLTALWM_OFF) = d6;
        *(volatile uint16_t *)(custom + BLTAFWM_OFF) = 0xFFFF;

        /* Shift amount */
        uint16_t shiftVal = ((uint16_t)d1 & TILE_MASK) << 12;
        *(volatile uint16_t *)(custom + BLTCON1_OFF) = shiftVal;

        /* BLTCON0 mode */
        uint32_t maskPtr = a0->maskPtr;
        uint16_t bltcon0;
        if (maskPtr != 0) {
            bltcon0 = shiftVal | BLTCON0_BASE_LINKED;
        } else {
            bltcon0 = shiftVal | BLTCON0_BASE_SINGLE;
            *(volatile uint16_t *)(custom + BLTADAT_OFF) = 0xFFFF;
        }
        *(volatile uint16_t *)(custom + BLTCON0_OFF) = bltcon0;

        /* Check for Y-wrap split */
        uint16_t d4 = 0;
        {
            int16_t overshoot = d2 + d3 - SCREEN_HEIGHT;
            if (overshoot > 0) {
                d4 = (uint16_t)overshoot;
                d3 -= overshoot;
            }
        }

        /* Compute destination pointer */
        int16_t xWordOff = (d1 >> TILE_SHIFT_X) << 1; /* asr.w + asl.w (signed) */
        int32_t xWordOffL = (int32_t)xWordOff;  /* sign-extended */
        uint32_t d6save = (uint32_t)xWordOffL;
        uint32_t yOff = (uint16_t)d2 * (uint32_t)ROW_STRIDE_BYTES;
        uint32_t destAddr = (uint32_t)a1 + yOff + xWordOffL;

        *(volatile uint32_t *)(custom + BLTCPT_OFF) = destAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = destAddr;

        /* Queue entry: mask+fb addresses */
        *(uint32_t *)((uint8_t *)a3 + 4) = destAddr;
        uint32_t maskBufAddr = destAddr - (uint32_t)a1 + (uint32_t)frontbufferMaskBuffer;
        *(uint32_t *)((uint8_t *)a3 + 0) = maskBufAddr;

        /* BLTSIZE = (nlines * NUM_PLANES) << 6 | wordCount */
        uint16_t bltsize = ((uint16_t)d3 << NUM_PLANES_SHIFT << 6) | d7;
        uint16_t blitWidthBytes = d7 << 1;

        /* Modulos */
        uint16_t tileStride = (uint16_t)a0->wquads;
        uint16_t srcMod = (tileStride << 1) - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTAMOD_OFF) = srcMod;
        *(volatile uint16_t *)(custom + BLTBMOD_OFF) = srcMod;
        uint16_t destMod = PLANE_STRIDE - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTCMOD_OFF) = destMod;
        *(volatile uint16_t *)(custom + BLTDMOD_OFF) = destMod;
        /* Queue modulo */
        *(uint16_t *)((uint8_t *)a3 + 8) = destMod;

        /* Source top skip */
        uint32_t srcSkip = 0;
        if (d5 != 0) {
            srcSkip = (uint32_t)((uint16_t)d5 << NUM_PLANES_SHIFT) * (uint32_t)(tileStride << 1);
        }
        uint32_t srcData = a0->sprData + srcSkip;
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = srcData;
        uint32_t srcMask2 = a0->maskPtr + srcSkip;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcMask2;

        /* Kick blit */
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
        *(uint16_t *)((uint8_t *)a3 + 10) = bltsize;
        a3 = (uint32_t *)((uint8_t *)a3 + 12);

        /* Handle Y-wrap tail portion */
        if (d4 != 0) {
            uint32_t destAddr2 = (uint32_t)a1 + d6save;
            blitWidthBytes = d7; /* restore d7 (still in words) */
            /* Recompute: d7 was already halved? No, d7 is word count.
               In asm: lsr.w #1,d7 then uses d7 as half-word count for bltsize.
               Actually re-reading: d7 doesn't get halved in linked path.
               The asm does: d4 bltsize = (d4 << NUM_PLANES_SHIFT << 6) | d7 */
            uint16_t tailBltsize = ((uint16_t)d4 << NUM_PLANES_SHIFT << 6) | d7;

            *(uint32_t *)((uint8_t *)a3 + 4) = destAddr2;
            waitBlit();
            *(volatile uint32_t *)(custom + BLTCPT_OFF) = destAddr2;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = destAddr2;
            *(volatile uint16_t *)(custom + BLTSIZE_OFF) = tailBltsize;

            uint32_t tailMask = destAddr2 - (uint32_t)a1 + (uint32_t)frontbufferMaskBuffer;
            *(uint32_t *)((uint8_t *)a3 + 0) = tailMask;
            /* Copy modulo from previous entry */
            *(uint16_t *)((uint8_t *)a3 + 8) = *(uint16_t *)((uint8_t *)a3 - 4);
            *(uint16_t *)((uint8_t *)a3 + 10) = tailBltsize;
            a3 = (uint32_t *)((uint8_t *)a3 + 12);
        }

        /* Seal queue */
        *(uint32_t *)a3 = BLT_BLITQ_END;
        spriteBlitQueue = (uint32_t)a3;
        return;
    }

    /* ---- Normal (non-linked) 4-plane path ---- */
    {
        uint32_t *a3 = (uint32_t *)spriteBlitQueue;
        uint8_t *a1 = (uint8_t *)frontbufferBasePtr;

        /* Redundant type check removed - we know sprType == 0 here */
        int16_t d3 = a0->nlines;
        int16_t d5 = 0;

        /* Clip top */
        if (d2 < 0) {
            d5 = -d2;
            d3 += d2;
            d2 = 0;
        }
        /* Clip bottom */
        {
            int16_t d0 = d2 + d3 - SCREEN_HEIGHT;
            if (d0 > 0)
                d3 -= d0;
        }

        /* Apply camera offsets with Y wrap */
        d1 += cameraXMod320;
        d2 += cameraYMod320;
        if (d2 >= SCREEN_HEIGHT)
            d2 -= SCREEN_HEIGHT;

        /* Compute edge masks and word count */
        uint16_t d6 = 0;
        uint16_t bitPos = (uint16_t)d1 & TILE_MASK;
        uint16_t sprPixMod = (uint16_t)a0->pixWidth & TILE_MASK;
        if (sprPixMod == 0)
            sprPixMod = 16;
        uint16_t totalBits = bitPos + sprPixMod;
        uint16_t d7 = (uint16_t)a0->wquads + 1;  /* TILE_STRIDE for normal path */
        if (totalBits <= 16) {
            d6 = 0xFFFF;
            d7--;
        }

        /* Wait for blitter idle */
        waitBlit();
        *(volatile uint16_t *)(custom + BLTALWM_OFF) = d6;
        *(volatile uint16_t *)(custom + BLTAFWM_OFF) = 0xFFFF;

        /* Shift amount */
        uint16_t shiftVal = ((uint16_t)d1 & TILE_MASK) << 12;
        *(volatile uint16_t *)(custom + BLTCON1_OFF) = shiftVal;

        /* BLTCON0 mode */
        uint32_t maskPtr = a0->maskPtr;
        uint16_t bltcon0;
        if (maskPtr != 0) {
            bltcon0 = shiftVal | BLTCON0_BASE_LINKED;
        } else {
            bltcon0 = shiftVal | BLTCON0_BASE_SINGLE;
            *(volatile uint16_t *)(custom + BLTADAT_OFF) = 0xFFFF;
        }
        *(volatile uint16_t *)(custom + BLTCON0_OFF) = bltcon0;

        /* Check for Y-wrap split */
        uint16_t d4 = 0;
        {
            int16_t overshoot = d2 + d3 - SCREEN_HEIGHT;
            if (overshoot > 0) {
                d4 = (uint16_t)overshoot;
                d3 -= overshoot;
            }
        }

        /* Save queued line count */
        queuedLineCount = (uint16_t)d3;

        /* Compute destination pointer */
        int16_t xByteOff = (d1 >> TILE_SHIFT_X) << 1; /* asr.w + asl.w (signed) */
        int32_t xByteOffL = (int32_t)xByteOff;
        uint32_t d6save = (uint32_t)xByteOffL;
        uint32_t yOff = (uint16_t)d2 * (uint32_t)ROW_STRIDE_BYTES;
        uint32_t destAddr = (uint32_t)a1 + yOff + xByteOffL;

        *(volatile uint32_t *)(custom + BLTCPT_OFF) = destAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = destAddr;
        uint8_t *a2 = (uint8_t *)destAddr;  /* tracks dest for multi-plane passes */

        /* Queue entry */
        *(uint32_t *)((uint8_t *)a3 + 4) = destAddr;
        uint32_t maskBufAddr = destAddr - (uint32_t)a1 + (uint32_t)frontbufferMaskBuffer;
        *(uint32_t *)((uint8_t *)a3 + 0) = maskBufAddr;

        /* BLTSIZE for 4-plane blit: (nlines * 4) << 6 | wordCount */
        uint16_t bltsize4 = ((uint16_t)d3 << NUM_PLANES_SHIFT << 6) | d7;
        *(uint16_t *)((uint8_t *)a3 + 10) = bltsize4;

        /* Per-plane bltsize: nlines << 6 | wordCount */
        uint16_t bltsize1 = ((uint16_t)d3 << 6) | d7;
        uint16_t blitWidthBytes = d7 << 1;

        /* Modulos */
        uint16_t tileStride = (uint16_t)a0->wquads;
        uint16_t srcModA = tileStride << 1;
        uint16_t d1mod = srcModA;  /* save for source B modulo */
        uint16_t srcModAB = srcModA - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTAMOD_OFF) = srcModAB;

        /* Source B modulo: stride * 4 planes - blit width */
        uint16_t srcModB = (d1mod << NUM_PLANES_SHIFT) - blitWidthBytes;
        *(volatile uint16_t *)(custom + BLTBMOD_OFF) = srcModB;

        /* Dest C/D modulo: PLANE_STRIDE - blit width (per row, per plane) */
        uint16_t destMod = PLANE_STRIDE - blitWidthBytes;
        *(uint16_t *)((uint8_t *)a3 + 8) = destMod;
        /* Actual C/D mod includes inter-plane gap: ROW_STRIDE - PLANE_STRIDE + destMod */
        uint16_t destCDMod = destMod + (ROW_STRIDE_BYTES - PLANE_STRIDE);
        *(volatile uint16_t *)(custom + BLTCMOD_OFF) = destCDMod;
        *(volatile uint16_t *)(custom + BLTDMOD_OFF) = destCDMod;

        /* Source top skip */
        uint32_t srcSkip5 = 0;
        if (d5 != 0) {
            srcSkip5 = (uint32_t)((uint16_t)d5 << NUM_PLANES_SHIFT) * (uint32_t)(tileStride << 1);
        }

        /* Source plane data */
        uint32_t srcDataAddr = a0->sprData + srcSkip5;
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = srcDataAddr;
        uint8_t *a5ptr = (uint8_t *)srcDataAddr;

        /* Source mask */
        uint32_t srcMaskAddr = a0->maskPtr + (srcSkip5 >> NUM_PLANES_SHIFT);
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcMaskAddr;
        uint8_t *a4ptr = (uint8_t *)srcMaskAddr;

        a3 = (uint32_t *)((uint8_t *)a3 + 12);

        /* Kick first plane blit */
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize1;

        /* Save source pointers for tail portion */
        savedSrcPlaneB = (uint32_t)a4ptr;
        savedSrcPlaneA = (uint32_t)a5ptr;

        /* Blit remaining planes (1..NUM_PLANES-1) */
        uint32_t srcPlaneStep = (uint32_t)(tileStride << 1);
        a2 += PLANE_STRIDE;
        a5ptr += srcPlaneStep;

        {
            int16_t planeIdx;
            for (planeIdx = NUM_PLANES_MINUS1; planeIdx > 0; planeIdx--)
            {
                waitBlit();
                *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)a2;
                *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)a2;
                *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)a4ptr;
                *(volatile uint32_t *)(custom + BLTBPT_OFF) = (uint32_t)a5ptr;
                *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize1;

                a2 += PLANE_STRIDE;
                a5ptr += srcPlaneStep;
            }
        }

        /* Handle Y-wrap tail portion */
        if (d4 != 0) {
            /* Compute source skip for remaining lines */
            uint32_t tailSrcSkipB = (uint32_t)queuedLineCount * (uint32_t)(tileStride << 1);
            uint32_t tailSrcB = savedSrcPlaneB + tailSrcSkipB;
            uint32_t tailSrcSkipA = tailSrcSkipB << NUM_PLANES_SHIFT;
            uint32_t tailSrcA = savedSrcPlaneA + tailSrcSkipA;

            waitBlit();

            /* Dest wraps to top of buffer at column offset */
            uint32_t tailDest = (uint32_t)a1 + d6save;
            *(volatile uint32_t *)(custom + BLTCPT_OFF) = tailDest;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = tailDest;
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = tailSrcB;
            *(volatile uint32_t *)(custom + BLTBPT_OFF) = tailSrcA;

            /* Queue entry for tail */
            *(uint32_t *)((uint8_t *)a3 + 4) = tailDest;
            uint32_t tailMask = tailDest - (uint32_t)a1 + (uint32_t)frontbufferMaskBuffer;
            *(uint32_t *)((uint8_t *)a3 + 0) = tailMask;

            /* Per-plane blit size */
            uint16_t tailBltsize1 = ((uint16_t)d4 << 6) | (blitWidthBytes >> 1);
            *(volatile uint16_t *)(custom + BLTSIZE_OFF) = tailBltsize1;

            /* N-plane blit size for queue */
            uint16_t tailBltsizeN = ((uint16_t)d4 << NUM_PLANES_SHIFT << 6) | (blitWidthBytes >> 1);
            *(uint16_t *)((uint8_t *)a3 + 10) = tailBltsizeN;

            /* Blit remaining tail planes (1..NUM_PLANES-1) */
            tailDest += PLANE_STRIDE;
            tailSrcA += srcPlaneStep;

            {
                int16_t planeIdx;
                for (planeIdx = NUM_PLANES_MINUS1; planeIdx > 0; planeIdx--)
                {
                    waitBlit();
                    *(volatile uint32_t *)(custom + BLTCPT_OFF) = tailDest;
                    *(volatile uint32_t *)(custom + BLTDPT_OFF) = tailDest;
                    *(volatile uint32_t *)(custom + BLTAPT_OFF) = tailSrcB;
                    *(volatile uint32_t *)(custom + BLTBPT_OFF) = tailSrcA;
                    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = tailBltsize1;

                    tailDest += PLANE_STRIDE;
                    tailSrcA += srcPlaneStep;
                }
            }

            /* Copy modulo from previous entry */
            *(uint16_t *)((uint8_t *)a3 + 8) = *(uint16_t *)((uint8_t *)a3 - 4);
            a3 = (uint32_t *)((uint8_t *)a3 + 12);
        }

        /* Seal queue */
        *(uint32_t *)a3 = BLT_BLITQ_END;
        spriteBlitQueue = (uint32_t)a3;
    }
}

/* ================================================================
 * DrawSprite_entry2  (SWOS2.s line 6313)
 *
 * Draws sprite at (d1, d2) using DrawSpriteClipped, then appends
 * a queue terminator. Saves/restores spriteBlitQueue around the
 * clipped draw.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawSprite_entry2(SpriteGraphics *a0, int16_t d1, int16_t d2)
{
    uint32_t savedQueue = spriteBlitQueue;
    _DrawSpriteClipped(a0, d1, d2);
    spriteBlitQueue = savedQueue;
    *(uint32_t *)savedQueue = BLT_BLITQ_END;
}

/* ================================================================
 * DrawSprite16Pixels  (SWOS2.s line 6324)
 *
 * Looks up sprite by index and draws it at (d1, d2).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawSprite16Pixels(uint16_t spriteIndex, int16_t x, int16_t y)
{
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;
    SpriteGraphics *spr = sprGfxTable[spriteIndex];
    _DrawSprite_entry2(spr, x, y);
}

/* ================================================================
 * DrawSpriteCentered  (SWOS2.s line 6338)
 *
 * Like DrawSprite16Pixels but adjusts x,y by the sprite's center
 * offsets before drawing.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawSpriteCentered(uint16_t spriteIndex, int16_t x, int16_t y)
{
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;
    SpriteGraphics *spr = sprGfxTable[spriteIndex];
    x -= spr->centerX;
    y -= spr->centerY;
    _DrawSprite_entry2(spr, x, y);
}

/* ================================================================
 * DrawSpriteInCameraView  (SWOS2.s line 6520)
 *
 * Draws a sprite in camera-relative coordinates with bounds
 * checking. Skips draw if sprite is fully outside view.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawSpriteInCameraView(uint16_t spriteIndex, int16_t x, int16_t y)
{
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;
    SpriteGraphics *spr = sprGfxTable[spriteIndex];

    x -= cameraX;
    y -= cameraY;

    /* Cull if completely off-screen */
    if (x >= SCREEN_WIDTH)
        return;
    if (y >= SCREEN_HEIGHT)
        return;
    {
        int16_t negWidth = -spr->pixWidth;
        if (x <= negWidth)
            return;
    }
    {
        int16_t negHeight = -spr->nlines;
        if (y <= negHeight)
            return;
    }

    _DrawSpriteClipped(spr, x, y);
}

/* ================================================================
 * DrawSpriteHotspotInCameraView  (SWOS2.s line 6504)
 *
 * Like DrawSpriteInCameraView but adjusts position by the sprite's
 * center (hotspot) offsets first.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawSpriteHotspotInCameraView(uint16_t spriteIndex, int16_t x, int16_t y)
{
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;
    SpriteGraphics *spr = sprGfxTable[spriteIndex];

    x -= spr->centerX;
    y -= spr->centerY;

    /* Fall through to DrawSpriteInCameraView logic (with spr already resolved) */
    x -= cameraX;
    y -= cameraY;

    if (x >= SCREEN_WIDTH)
        return;
    if (y >= SCREEN_HEIGHT)
        return;
    {
        int16_t negWidth = -spr->pixWidth;
        if (x <= negWidth)
            return;
    }
    {
        int16_t negHeight = -spr->nlines;
        if (y <= negHeight)
            return;
    }

    _DrawSpriteClipped(spr, x, y);
}

/* ================================================================
 * DarkenRectangle  (SWOS2.s line 6555)
 *
 * Darkens a rectangle by drawing the sprite's high-bit overlay.
 * Resolves sprite by index and falls through to DrawSpriteClipped.
 * In assembly this falls through to DrawSpriteClipped via bra *+4;
 * the sprite type at offset 8 is non-zero for darken sprites, which
 * triggers the branch_linked_planes path.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DarkenRectangle(uint16_t spriteIndex, int16_t x, int16_t y)
{
    SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;
    SpriteGraphics *spr = sprGfxTable[spriteIndex];
    _DrawSpriteClipped(spr, x, y);
}

/* ================================================================
 * DrawQueuedSprites  (SWOS2.s line 6971)
 *
 * Pops sprites from the main render stack and dispatches them
 * to the appropriate draw function based on sprite type and
 * player range.
 *
 * Render stack entries (12 bytes, pushed upward):
 *   +0 = sprite struct pointer (a1/a4)
 *   +4 = sprite graphics pointer (a0)
 *   +8 = x (int16_t)
 *   +10 = y (int16_t)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawQueuedSprites(void)
{
    uint8_t *a2 = (uint8_t *)renderStackPtr;

    while ((uint32_t)a2 != (uint32_t)renderStackBase) {
        a2 -= 2;
        int16_t d2 = *(int16_t *)a2;
        a2 -= 2;
        int16_t d1 = *(int16_t *)a2;
        a2 -= 4;
        SpriteGraphics *a0 = *(SpriteGraphics **)a2;
        a2 -= 4;
        Sprite *a4 = *(Sprite **)a2;

        /* Check if highlight recording needed */
        if (a4->teamNumber != 0) {
            int16_t d0 = a0->ordinal;
            _SaveCoordinatesForHighlights(d0, (uint16_t)d1, (uint16_t)d2);
        }

        /* Check if a4 is a player sprite (teamNumber 1 or 2).
         * Original asm used address range check (goalie1Sprite..bottomCornerLeftPositions),
         * but sprites moved from asm to C data sections so addresses are no longer contiguous. */
        if (a4->teamNumber == 1 || a4->teamNumber == 2) {
            /* Player sprite: use masked blit.
             * QueueMaskedSpriteBlitClipped expects registers:
             * a0=spriteGfx, a4=spriteStruct, d1=x, d2=y
             * Since this is still in asm, we call it via the asm entry point.
             * The asm stub will use the register state left by the caller. */
            _QueueMaskedSpriteBlitClipped(a0, d1, d2);
            continue;
        }

        /* Check sprite save flag: if SPRITE_SAVESPRITE ($48) in the sprite struct */
        if (a4->saveSprite != 0) {
            /* Use DrawSprite_entry2 path (saves/restores queue) */
            _DrawSprite_entry2(a0, d1, d2);
        } else {
            /* Normal clipped draw */
            _DrawSpriteClipped(a0, d1, d2);
        }
    }
}

/* ================================================================
 * RenderSpriteBandPlanePair  (SWOS2.s line 7069)
 *
 * Renders a single sprite into a hardware sprite band copper list
 * pair (a4/a5). Programs VPOS/HPOS/CTL words and blits sprite
 * data into the copper list buffers.
 *
 * in: a0 = sprite graphics, d1 = x, d2 = y
 *     a4/a5 = copper list plane pair pointers (advanced on exit)
 *     a6 = CUSTOM base
 * ================================================================ */
static void renderSpriteBandPlanePair(SpriteGraphics *sprGfx, int16_t x, int16_t y,
                                       uint16_t **pA4, uint16_t **pA5)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    uint16_t *a4 = *pA4;
    uint16_t *a5 = *pA5;

    int16_t d3 = y + sprGfx->nlines;

    /* Add display offsets */
    x += 0x70;    /* horizontal display start */
    y += 0x24;    /* vertical display start */
    d3 += 0x24;

    /* VPOS high byte | HPOS/2 */
    uint16_t d4 = ((uint16_t)y & 0xFF) << 8;
    uint16_t hpos = ((uint16_t)x >> 1) & 0xFF;
    uint16_t posWord = d4 + hpos;
    *a4++ = posWord;
    *a5++ = posWord;

    /* CTL word: VSTOP low byte << 8, with carry bits */
    uint16_t ctlD4 = ((uint16_t)d3 & 0xFF) << 8;
    if ((uint16_t)y & 0x100)
        ctlD4 |= 4;   /* VPOS bit 8 -> CTL bit 2 */
    if ((uint16_t)d3 & 0x100)
        ctlD4 |= 2;   /* VSTOP bit 8 -> CTL bit 1 */
    if ((uint16_t)x & 1)
        ctlD4 |= 1;   /* HPOS low bit -> CTL bit 0 */

    *a4++ = ctlD4;
    uint16_t ctlA5 = ctlD4 | 0x80;  /* bset #7 on low byte of a5 entry: ATTACH bit */
    *a5++ = ctlA5;

    /* Blit sprite data into copper list buffers */
    /* First plane pair */
    waitBlit();
    *(volatile uint32_t *)(custom + BLTAPT_OFF) = sprGfx->sprData;
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)a4;
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = sprGfx->bltsize;

    /* Second plane pair: source at data+4 */
    uint32_t srcPlane2 = sprGfx->sprData + 4;
    waitBlit();
    *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcPlane2;
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)a5;
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = sprGfx->bltsize;

    /* Advance past sprite data: nlines * 4 bytes (2 words per line) */
    uint16_t nlines = (uint16_t)sprGfx->nlines;
    uint16_t dataBytes = nlines << 2;
    a4 += (dataBytes >> 1);
    a5 += (dataBytes >> 1);

    *pA4 = a4;
    *pA5 = a5;
}

/* ================================================================
 * RenderSpriteBandsToCopperLists  (SWOS2.s line 7007)
 *
 * Processes plane01 and plane23 render stacks, rendering each
 * sprite band entry into the copper list sprite buffers.
 * ================================================================ */
__attribute__((used, externally_visible))
void _RenderSpriteBandsToCopperLists(void)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;

    /* Set up blitter for straight copy (A->D) */
    waitBlit();
    *(volatile uint16_t *)(custom + BLTALWM_OFF) = 0xFFFF;
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = 0xFFFF;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTCON0_OFF) = BLTCON0_USEA_USED_LF_F0;
    *(volatile uint16_t *)(custom + BLTAMOD_OFF) = (NUM_PLANES - 2) * 2;
    *(volatile uint16_t *)(custom + BLTDMOD_OFF) = 0;

    /* ---- Process plane 0/1 render stack ---- */
    uint16_t *a4 = (uint16_t *)bitPlane0BasePtr;
    uint16_t *a5 = (uint16_t *)bitPlane1BasePtr;
    uint8_t *a3 = (uint8_t *)plane01RenderStackPtr;

    while ((uint32_t)a3 != (uint32_t)plane01RenderStackBase) {
        a3 -= 2;
        int16_t d2 = *(int16_t *)a3;
        a3 -= 2;
        int16_t d1 = *(int16_t *)a3;
        a3 -= 4;
        SpriteGraphics *a0 = *(SpriteGraphics **)a3;
        a3 -= 4;
        uint8_t *a1 = *(uint8_t **)a3;

        if (*(uint16_t *)a1 != 0) {
            int16_t d0 = a0->ordinal;
            _SaveCoordinatesForHighlights(d0, (uint16_t)d1, (uint16_t)d2);
        }

        renderSpriteBandPlanePair(a0, d1, d2, &a4, &a5);
    }

    /* Terminate plane 0/1 copper lists */
    *(uint32_t *)a4 = copperZeroLong;
    *(uint32_t *)a5 = copperZeroLong;

    /* ---- Process plane 2/3 render stack ---- */
    a4 = (uint16_t *)bitPlane2BasePtr;
    a5 = (uint16_t *)bitPlane3BasePtr;
    a3 = (uint8_t *)plane23RenderStackPtr;

    while ((uint32_t)a3 != (uint32_t)plane23RenderStackBase) {
        a3 -= 2;
        int16_t d2 = *(int16_t *)a3;
        a3 -= 2;
        int16_t d1 = *(int16_t *)a3;
        a3 -= 4;
        SpriteGraphics *a0 = *(SpriteGraphics **)a3;
        a3 -= 4;
        uint8_t *a1 = *(uint8_t **)a3;

        if (*(uint16_t *)a1 != 0) {
            int16_t d0 = a0->ordinal;
            _SaveCoordinatesForHighlights(d0, (uint16_t)d1, (uint16_t)d2);
        }

        renderSpriteBandPlanePair(a0, d1, d2, &a4, &a5);
    }

    /* Terminate plane 2/3 copper lists */
    *(uint32_t *)a4 = copperZeroLong;
    *(uint32_t *)a5 = copperZeroLong;
}

/* ================================================================
 * BuildSpriteBandAndGeneralRenderStacks  (SWOS2.s line 7130)
 *
 * Iterates sorted sprites, culls off-screen ones, and distributes
 * visible sprites into one of three render stacks:
 *   - plane01 band (hardware sprite band, top-most Y)
 *   - plane23 band (hardware sprite band, second-top Y)
 *   - general render stack (software blitter sprites)
 *
 * Uses occlusion buckets (16-pixel wide columns) to determine
 * whether a narrow sprite (<=16px) can use a hardware band or
 * must fall back to the general stack.
 * ================================================================ */

/* Local data matching SWOS2.s (plane01BandTopY, plane23BandTopY at lines 7294-7297) */
static int16_t plane01BandTopY_local;
static int16_t plane23BandTopY_local;

/* Occlusion buckets: In asm, occlMinYBuckets (5 words) and occlMinYBucketsActive
   (26 words) are contiguous. Negative bucket indices (from sprites partially off
   the left screen edge) read/write into the guard region (occlMinYBuckets).
   We MUST keep them contiguous to avoid undefined behavior from index -1 writes. */
static int16_t occlBuckets_combined[5 + 26]; /* 31 words, matching asm layout */
#define occlMinYBuckets_local   (occlBuckets_combined)
#define occlMinYBucketsActive_local (occlBuckets_combined + 5)

__attribute__((used, externally_visible))
void _BuildSpriteBandAndGeneralRenderStacks(void)
{
    int16_t d7 = (int16_t)numSpritesToRender;
    if (d7 == 0)
        return;

    uint8_t *a4 = (uint8_t *)plane01RenderStackBase;
    uint8_t *a5 = (uint8_t *)plane23RenderStackBase;
    uint8_t *a6loc = (uint8_t *)renderStackBase;

    /* Initialize all 31 occlusion bucket words to 0x7FFF (matching asm: 31 move.w's) */
    {
        int16_t i;
        for (i = 0; i < 31; i++)
            occlBuckets_combined[i] = 0x7FFF;
    }

    plane01BandTopY_local = 0x7D00;
    plane23BandTopY_local = 0x7D00;

    int16_t *a3 = occlMinYBucketsActive_local;

    /* Walk sorted sprites from back to front (highest index first) */
    uint32_t *a2 = sortedSprites + d7;
    d7--;

    while ((uint32_t)a2 != (uint32_t)sortedSprites) {
        a2--;
        Sprite *a1 = (Sprite *)(*a2);

        int16_t sprIdx = a1->imageIndex;
        if (sprIdx < 0) {
            /* Asm: bmi.w dec_and_loop — skip without touching onScreen */
            d7--;
            if (d7 < 0) break;
            continue;
        }

        /* Look up sprite graphics */
        SpriteGraphics **sprGfxTable = (SpriteGraphics **)(uint32_t)spriteGraphicsPtr;
        SpriteGraphics *a0 = sprGfxTable[(uint16_t)sprIdx];

        /* Camera-relative position with hotspot adjustment */
        int16_t d1 = a1->x.s.i - a0->centerX - cameraX;
        int16_t d2 = a1->y.s.i - a0->centerY - cameraY;
        d2 -= a1->z.s.i;

        /* Cull if off-screen */
        if (d1 >= SCREEN_WIDTH || d2 >= SCREEN_HEIGHT) {
            a1->onScreen = 0;
            d7--;
            if (d7 < 0) break;
            continue;
        }
        {
            int16_t negW = -a0->pixWidth;
            if (d1 <= negW) {
                a1->onScreen = 0;
                d7--;
                if (d7 < 0) break;
                continue;
            }
        }
        {
            int16_t negH = -a0->nlines;
            if (d2 <= negH) {
                a1->onScreen = 0;
                d7--;
                if (d7 < 0) break;
                continue;
            }
        }

        /* Check if overlay is active or sprite is wider than 16px */
        if (overlayActiveFlag != 0 || a0->pixWidth > 16) {
            /* Wide sprite: update occlusion buckets and go to general stack */
            int16_t leftBucket = (d1 >> 4) << 1;
            int16_t rightBucket = ((d1 + a0->pixWidth - 1) >> 4) << 1;
            int16_t d3 = leftBucket;
            while (d3 <= rightBucket) {
                if (d2 < a3[d3 >> 1])
                    a3[d3 >> 1] = d2;
                d3 += 2;
            }

            a1->field_56 = 0;

            /* Push to general render stack */
            *(uint32_t *)a6loc = (uint32_t)a1; a6loc += 4;
            *(uint32_t *)a6loc = (uint32_t)a0; a6loc += 4;
            *(int16_t *)a6loc = d1; a6loc += 2;
            *(int16_t *)a6loc = d2; a6loc += 2;
        } else {
            /* Narrow sprite (<=16px): try to assign to a hardware band */
            int16_t leftBucket = (d1 >> 4) << 1;
            int16_t rightBucket = ((d1 + a0->pixWidth - 1) >> 4) << 1;

            /* Check if sprite top is above screen (can't use bands) */
            if (d2 < 0) {
                goto enqueue_general;
            }

            /* Check bottom edge against occlusion buckets */
            int16_t d3 = d2 + a0->nlines;
            if (d3 > a3[leftBucket >> 1] || d3 > a3[rightBucket >> 1]) {
                goto enqueue_general;
            }

            /* Try plane23 band first (top-most) */
            if (d3 < plane23BandTopY_local) {
                a1->field_56 = 2;
                plane23BandTopY_local = d2;
                *(uint32_t *)a5 = (uint32_t)a1; a5 += 4;
                *(uint32_t *)a5 = (uint32_t)a0; a5 += 4;
                *(int16_t *)a5 = d1; a5 += 2;
                *(int16_t *)a5 = d2; a5 += 2;
                goto post_enqueue;
            }

            /* Try plane01 band */
            if (d3 < plane01BandTopY_local) {
                a1->field_56 = 1;
                plane01BandTopY_local = d2;
                *(uint32_t *)a4 = (uint32_t)a1; a4 += 4;
                *(uint32_t *)a4 = (uint32_t)a0; a4 += 4;
                *(int16_t *)a4 = d1; a4 += 2;
                *(int16_t *)a4 = d2; a4 += 2;
                goto post_enqueue;
            }

            /* Falls through to general stack */
enqueue_general:
            a1->field_56 = 0;
            *(uint32_t *)a6loc = (uint32_t)a1; a6loc += 4;
            *(uint32_t *)a6loc = (uint32_t)a0; a6loc += 4;
            *(int16_t *)a6loc = d1; a6loc += 2;
            *(int16_t *)a6loc = d2; a6loc += 2;

post_enqueue:
            /* Update occlusion bucket minimums */
            if (d2 < a3[leftBucket >> 1])
                a3[leftBucket >> 1] = d2;
            if (d2 < a3[rightBucket >> 1])
                a3[rightBucket >> 1] = d2;
        }

        /* Mark sprite as on-screen */
        a1->onScreen = 1;
        d7--;
        if (d7 < 0) break;
        continue;
    }

    /* Commit stack pointers */
    plane01RenderStackPtr = (uint32_t)a4;
    plane23RenderStackPtr = (uint32_t)a5;
    renderStackPtr = (uint32_t)a6loc;
}

/* ================================================================
 * DrawSprites  (SWOS2.s line 7322)
 *
 * Top-level sprite rendering: sort -> build render stacks ->
 * draw queued (blitter) sprites -> render bands to copper lists.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawSprites(void)
{
    _SortSprites();
    _BuildSpriteBandAndGeneralRenderStacks();
    _DrawQueuedSprites();
    _RenderSpriteBandsToCopperLists();
}
