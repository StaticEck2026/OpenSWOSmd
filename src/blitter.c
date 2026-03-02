#include "blitter.h"
#include "text_render.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint8_t *spriteGraphicsPtr;
extern uint32_t frontbufferBasePtr;
extern uint16_t cameraXMod320;
extern uint16_t cameraYMod320;
extern volatile uint16_t panicReasonCode;

extern void _PanicHaltLoop(void) __attribute__((noreturn));

/* CharsTableHeader is defined in game_types.h */

/* ---- Display constants ---- */
#define SCREEN_HEIGHT    272
#define PLANE_STRIDE     44
#define ROW_STRIDE_BYTES 176
#define TILE_MASK        15
#define NUM_PLANES_SHIFT 2
#define FB_BLT_MOD_AFTER_1_WORD 174


/* ---- Amiga hardware registers ---- */
#define CUSTOM_BASE         0x00DFF000u
#define DMACONR_ADDR        0x00DFF002u
#define DMACON_ADDR         0x00DFF096u

#define BLTCON0_OFF    64
#define BLTCON1_OFF    66
#define BLTAFWM_OFF    68
#define BLTALWM_OFF    70
#define BLTCPT_OFF     72
#define BLTBPT_OFF     76
#define BLTAPT_OFF     80
#define BLTDPT_OFF     84
#define BLTSIZE_OFF    88
#define BLTCMOD_OFF    96
#define BLTBMOD_OFF    98
#define BLTAMOD_OFF    100
#define BLTDMOD_OFF    102
#define BLTBDAT_OFF    114

#define DMACON_BLT_ON    33792u
#define DMACON_BLT_OFF   1024u
#define DMACONR_BLTBUSY_BIT 6

/* ---- Blitter function code constants ---- */
#define BLTCON0_USEA_LF_FC          0x08FC
#define BLTCON0_USED_LF_00          0x0100
#define BLTCON0_USEA_USED_LF_F0     0x09F0
#define BLTCON0_USEA_USED_LF_FC     0x09FC
#define BLTCON0_USED_LF_FF          0x01FF
#define BLTCON0_USEA_USEB_USED_LF_FC 0x0DFC
#define BLTCON0_BASE_LINKED          0x0FCA
#define BLTCON0_CLEAR_MASK           0x0B0A
#define BLTCON0_OR_MASK              0x0BFA

static void waitBlit(void)
{
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    /* First read: dummy read to prime Agnus pipeline (matches asm pattern) */
    (void)(*(volatile uint8_t *)DMACONR_ADDR);
    /* Second read: actual busy-wait loop */
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT))
        ;
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;
}

/* ================================================================
 * Mask lookup tables (originally PC-relative data in SWOS2.s)
 * ================================================================ */

/* leftMaskTable: indexed by (xBit * 2), xBit = 1..15 → entry 0..14 */
static const uint16_t leftMaskTable[15] = {
    0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xFF80, 0xFFC0, 0xFFE0, 0xFFF0, 0xFFF8, 0xFFFC, 0xFFFE
};

/* partialSpanMaskTable: indexed by (overrun * 2), overrun = 1..15 → entry 0..14
   Entry 0 in asm was $4E75 (rts opcode) — never used as a mask value. */
static const uint16_t partialSpanMaskTable[15] = {
    0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F,
    0x00FF, 0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF
};

/* tailMaskTable: indexed by (pixels & TILE_MASK) * 2, values 0..15 */
static const uint16_t tailMaskTable[16] = {
    0xFFFF, 0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00,
    0xFF00, 0xFF80, 0xFFC0, 0xFFE0, 0xFFF0, 0xFFF8, 0xFFFC, 0xFFFE
};

/* firstWordMaskTable / lastWordMaskTable for blitter_rectrow_masked */
static const uint16_t firstWordMaskTable[16] = {
    0xFFFF, 0x7FFF, 0x3FFF, 0x1FFF, 0x0FFF, 0x07FF, 0x03FF, 0x01FF,
    0x00FF, 0x007F, 0x003F, 0x001F, 0x000F, 0x0007, 0x0003, 0x0001
};

static const uint16_t lastWordMaskTable[16] = {
    0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xFF80, 0xFFC0, 0xFFE0, 0xFFF0, 0xFFF8, 0xFFFC, 0xFFFE, 0xFFFF
};


/* ================================================================
 * BlitterSpanCoreMasked  (static)
 *
 * Programs the Amiga blitter to fill/clear a horizontal span on
 * a single bitplane. Handles left edge, middle words, and right edge.
 *
 * Params carried in/out:
 *   d0 = x bit position (xBit, 0-15)
 *   d1 = x bit offset within word (0-15)
 *   d2 = remaining pixel width
 *   d3 = height (rows)
 *   d4 = color mask (shifted right per plane)
 *   d5/d6 = bltcon0 codes (fill/clear)
 *   a0 = plane pointer
 *   a6 = CUSTOM base
 * ================================================================ */
static void blitterSpanCoreMasked(uint8_t *plane, uint16_t xBit, uint16_t width,
                                  uint16_t height, uint16_t colorBit, uint16_t custom_unused)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    uint16_t d5, d6;
    uint16_t d7, d0;

    /* Determine fill vs clear mode based on color bit */
    if (colorBit & 1) {
        d5 = BLTCON0_USEA_USED_LF_FC;
        d6 = BLTCON0_USED_LF_FF;
    } else {
        d5 = BLTCON0_USEA_USED_LF_F0;
        d6 = BLTCON0_USED_LF_00;
    }
    (void)custom_unused;

    /* ---- Left edge ---- */
    if (xBit != 0) {
        d7 = leftMaskTable[xBit - 1];
        d0 = 16 - xBit;

        if ((int16_t)(width - d0) < 0) {
            /* Span fits entirely within left edge word */
            uint16_t overrun = d0 - width;
            d7 |= partialSpanMaskTable[overrun - 1];
            width = 0;
        } else {
            width -= d0;
        }

        /* Left edge: 1-word masked blit
           NOTE: Known artifact — SET case (BLTBDAT with USEB=0) produces
           colored blocks at the left edge. Root cause unknown — generated
           code matches original asm exactly. To be investigated later. */
        waitBlit();
        *(volatile uint16_t *)(custom + BLTAFWM_OFF) = d7;
        *(volatile uint16_t *)(custom + BLTALWM_OFF) = d7;
        d7 = ~d7;
        *(volatile uint16_t *)(custom + BLTBDAT_OFF) = d7;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)plane;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)plane;
        *(volatile uint16_t *)(custom + BLTAMOD_OFF) = FB_BLT_MOD_AFTER_1_WORD;
        *(volatile uint16_t *)(custom + BLTDMOD_OFF) = FB_BLT_MOD_AFTER_1_WORD;
        *(volatile uint16_t *)(custom + BLTCON0_OFF) = d5;
        *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
        {
            uint16_t blitsize = (height << 6) | 1;
            *(volatile uint16_t *)(custom + BLTSIZE_OFF) = blitsize;
        }

        plane += 2;

        if (width == 0)
            return;
    }

    /* ---- Middle words ---- */
    d0 = width >> 4;
    if (d0 != 0) {
        uint16_t mod = ROW_STRIDE_BYTES - d0 * 2;

        waitBlit();
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)plane;
        *(volatile uint16_t *)(custom + BLTDMOD_OFF) = mod;
        *(volatile uint16_t *)(custom + BLTCON0_OFF) = d6;
        *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
        {
            uint16_t blitsize = (height << 6) | d0;
            *(volatile uint16_t *)(custom + BLTSIZE_OFF) = blitsize;
        }

        plane += d0 * 2;
    }

    /* ---- Right edge ---- */
    {
        uint16_t tail = width & TILE_MASK;
        if (tail != 0) {
            d7 = ~tailMaskTable[tail];

            waitBlit();
            *(volatile uint16_t *)(custom + BLTAFWM_OFF) = d7;
            *(volatile uint16_t *)(custom + BLTALWM_OFF) = d7;
            d7 = ~d7;
            *(volatile uint16_t *)(custom + BLTBDAT_OFF) = d7;
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)plane;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)plane;
            *(volatile uint16_t *)(custom + BLTAMOD_OFF) = FB_BLT_MOD_AFTER_1_WORD;
            *(volatile uint16_t *)(custom + BLTDMOD_OFF) = FB_BLT_MOD_AFTER_1_WORD;
            *(volatile uint16_t *)(custom + BLTCON0_OFF) = d5;
            *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
            {
                uint16_t blitsize = (height << 6) | 1;
                *(volatile uint16_t *)(custom + BLTSIZE_OFF) = blitsize;
            }
        }
    }
}


/* ================================================================
 * blitterSpan4Planes  (static)
 *
 * Fills a horizontal span across all 4 bitplanes.
 * Calls blitterSpanCoreMasked once per plane.
 *
 * In:
 *   d0 = x pixel
 *   d1 = y pixel row
 *   d2 = width in pixels
 *   d3 = height in rows
 *   d4 = 4-bit color
 *   a0 = framebuffer base
 * ================================================================ */
static void blitterSpan4Planes(uint16_t x, uint16_t y, uint16_t width,
                               uint16_t height, uint16_t color, uint8_t *buf)
{
    uint16_t xBit;
    uint8_t *plane;

    /* Compute plane base pointer */
    plane = buf + (uint32_t)y * ROW_STRIDE_BYTES;

    /* Word-align the x coordinate */
    plane += (x >> 3) & 0xFFFE;   /* WORD_ALIGN_EVEN_MASK = ~1 */

    xBit = x & TILE_MASK;

    /* Plane 0 */
    blitterSpanCoreMasked(plane, xBit, width, height, color, 0);
    plane += PLANE_STRIDE;

    /* Plane 1 */
    blitterSpanCoreMasked(plane, xBit, width, height, color >> 1, 0);
    plane += PLANE_STRIDE;

    /* Plane 2 */
    blitterSpanCoreMasked(plane, xBit, width, height, color >> 2, 0);
    plane += PLANE_STRIDE;

    /* Plane 3 */
    blitterSpanCoreMasked(plane, xBit, width, height, color >> 3, 0);
}


/* ================================================================
 * _FillRectangle
 *
 * Fills a rectangle on the frontbuffer with a solid color.
 * Handles vertical wrapping at SCREEN_HEIGHT (272).
 * Applies camera X/Y offsets.
 *
 * In (from asm):
 *   d0 = x, d1 = y, d2 = width, d3 = height, d4 = color
 *   a0 = frontbufferBasePtr (loaded internally)
 * ================================================================ */
__attribute__((used, externally_visible))
void _FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t *buf = (uint8_t *)frontbufferBasePtr;
    uint16_t wrapRows;
    int16_t overflow;

    x += cameraXMod320;
    y += cameraYMod320;

    if (y >= SCREEN_HEIGHT)
        y -= SCREEN_HEIGHT;

    /* Check for vertical wrap */
    wrapRows = 0;
    overflow = (int16_t)(y + h - SCREEN_HEIGHT);
    if (overflow > 0) {
        wrapRows = (uint16_t)overflow;
        h -= wrapRows;
    }

    /* Blit first span */
    blitterSpan4Planes(x, y, w, h, color, buf);

    /* Blit wrapped portion (starts at row 0) */
    if (wrapRows != 0) {
        blitterSpan4Planes(x, 0, w, wrapRows, color, buf);
    }
}


/* ================================================================
 * blitterRectRowMasked  (static)
 *
 * Blits a masked rectangular region from source to destination buffer.
 * First fills the span in the dest buffer, then applies a word-aligned
 * masked blit from src (mask) to dest.
 *
 * In:
 *   d0 = x pixel, d1 = y pixel, d2 = width, d3 = height
 *   a0 = mask buffer, a1 = dest buffer
 * ================================================================ */
static void blitterRectRowMasked(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 uint8_t *mask, uint8_t *buf)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    uint16_t wordStart, wordEnd, wordSpan;
    uint16_t fwm, lwm;
    uint16_t mod;
    uint8_t *srcRow, *dstRow;

    /* First pass: fill the span in dest with color 0 (clear) */
    blitterSpan4Planes(x, y, w, h, 0, buf);

    /* Scale y by 4 planes worth of stride */
    {
        uint32_t offset = (uint32_t)(y << 2) * PLANE_STRIDE;
        mask += offset;
        buf += offset;
    }

    /* Scale h by 4 planes */
    h <<= 2;

    /* Compute word boundaries */
    wordStart = x >> 4;
    wordEnd = (x + w - 1) >> 4;
    wordSpan = wordEnd - wordStart;

    /* Advance pointers to start word */
    mask += wordStart * 2;
    buf += wordStart * 2;

    /* Compute first/last word masks */
    fwm = firstWordMaskTable[x & TILE_MASK];
    lwm = lastWordMaskTable[(x + w - 1) & TILE_MASK];

    /* Wait for blitter idle */
    waitBlit();

    wordSpan += 1;

    *(volatile uint16_t *)(custom + BLTCON0_OFF) = BLTCON0_USEA_USEB_USED_LF_FC;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = fwm;
    *(volatile uint16_t *)(custom + BLTALWM_OFF) = lwm;
    *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)mask;
    *(volatile uint32_t *)(custom + BLTBPT_OFF) = (uint32_t)buf;
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)buf;

    mod = PLANE_STRIDE - wordSpan * 2;
    *(volatile uint16_t *)(custom + BLTAMOD_OFF) = mod;
    *(volatile uint16_t *)(custom + BLTBMOD_OFF) = mod;
    *(volatile uint16_t *)(custom + BLTDMOD_OFF) = mod;

    {
        uint16_t blitsize = (h << 6) | wordSpan;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = blitsize;
    }
}


/* ================================================================
 * _RectRowMaskedApplyScreenOffset
 *
 * Thin wrapper: adds camera offsets then calls blitterRectRowMasked.
 * ================================================================ */
__attribute__((used, externally_visible))
void _RectRowMaskedApplyScreenOffset(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                     uint8_t *mask, uint8_t *buf)
{
    x += cameraXMod320;
    y += cameraYMod320;
    blitterRectRowMasked(x, y, w, h, mask, buf);
}


/* ================================================================
 * _BlitSpritePlanar_Word
 *
 * Builds a 1-plane OR-combined mask from 4 interleaved bitplanes.
 * Two paths: normal (word-at-a-time) and fast (longword for 2-wide sprites).
 * If sprite type != 0 (linked), writes mask to all 4 output planes.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _BlitSpritePlanar_Word(uint16_t spriteIndex)
{
    uint8_t *base = spriteGraphicsPtr;
    SpriteGraphics *hdr;
    uint16_t type;
    uint8_t *srcData;
    uint8_t *dstMask;
    uint16_t nlines, wquads;
    uint16_t stride2, stride3, stride4;
    int32_t advance;
    int16_t row, col;

    hdr = *(SpriteGraphics **)(base + (uint32_t)(spriteIndex << 2));
    type = hdr->type;
    srcData = (uint8_t *)hdr->sprData;
    dstMask = (uint8_t *)hdr->maskPtr;
    nlines = (uint16_t)hdr->nlines;
    wquads = (uint16_t)hdr->wquads;

    stride2 = wquads * 2;
    stride3 = stride2 + stride2;  /* stride2 * 2 = wquads * 4 */
    stride4 = stride3 + stride2;  /* wquads * 6 */

    if (type == 0) {
        /* ---- 1-plane output ---- */
        if (wquads == 2) {
            /* Fast path: longword access for 2-word-wide sprites.
               Asm uses lsl #1 on wquads=2 to get stride2=4, then
               stride3=8, stride4=12, advance=16.
               Note: dstMask is NOT advanced per row (matches asm behavior). */
            int16_t r;
            advance = (int32_t)(stride2 * 4);

            for (r = nlines - 1; r >= 0; r--) {
                uint32_t val = *(uint32_t *)srcData;
                val |= *(uint32_t *)(srcData + stride2);
                val |= *(uint32_t *)(srcData + stride3);
                val |= *(uint32_t *)(srcData + stride4);
                *(uint32_t *)dstMask = val;
                srcData += advance;
            }
        } else {
            /* General path: word-at-a-time */
            uint16_t wordsPerRow = wquads;
            advance = (int32_t)(stride2 * 3);  /* skip 3 remaining planes after inner loop */

            for (row = nlines - 1; row >= 0; row--) {
                for (col = wordsPerRow - 1; col >= 0; col--) {
                    uint16_t val = *(uint16_t *)srcData;
                    val |= *(uint16_t *)(srcData + stride2);
                    val |= *(uint16_t *)(srcData + stride3);
                    val |= *(uint16_t *)(srcData + stride4);
                    *(uint16_t *)dstMask = val;
                    srcData += 2;
                    dstMask += 2;
                }
                srcData += advance;
            }
        }
    } else {
        /* ---- 4-plane output (type != 0) ---- */
        if (wquads == 2) {
            /* Fast path: longword access, output replicated to 4 planes */
            int16_t r;
            advance = (int32_t)(stride2 * 4);

            for (r = nlines - 1; r >= 0; r--) {
                uint32_t val = *(uint32_t *)srcData;
                val |= *(uint32_t *)(srcData + stride2);
                val |= *(uint32_t *)(srcData + stride3);
                val |= *(uint32_t *)(srcData + stride4);
                *(uint32_t *)dstMask = val;
                *(uint32_t *)(dstMask + stride2) = val;
                *(uint32_t *)(dstMask + stride3) = val;
                *(uint32_t *)(dstMask + stride4) = val;
                srcData += advance;
                dstMask += advance;
            }
        } else {
            /* General path: word-at-a-time, 4 output planes */
            uint16_t wordsPerRow = wquads;
            advance = (int32_t)(stride2 * 3);

            for (row = nlines - 1; row >= 0; row--) {
                for (col = wordsPerRow - 1; col >= 0; col--) {
                    uint16_t val = *(uint16_t *)srcData;
                    val |= *(uint16_t *)(srcData + stride2);
                    val |= *(uint16_t *)(srcData + stride3);
                    val |= *(uint16_t *)(srcData + stride4);
                    *(uint16_t *)dstMask = val;
                    *(uint16_t *)(dstMask + stride2) = val;
                    *(uint16_t *)(dstMask + stride3) = val;
                    *(uint16_t *)(dstMask + stride4) = val;
                    srcData += 2;
                    dstMask += 2;
                }
                srcData += advance;
                dstMask += advance;
            }
        }
    }
}


/* ================================================================
 * _BlitSpritePlanar_Word_StridedDest
 *
 * Like BlitSpritePlanar_Word but destination advances by stride2
 * per row instead of full 4-plane stride. Uses word access only.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _BlitSpritePlanar_Word_StridedDest(uint16_t spriteIndex)
{
    uint8_t *base = spriteGraphicsPtr;
    SpriteGraphics *hdr;
    uint16_t type;
    uint8_t *srcData, *dstMask;
    uint16_t nlines;
    uint16_t stride2, stride3, stride4;
    int32_t srcAdvance, dstAdvance;
    int16_t row;

    hdr = *(SpriteGraphics **)(base + (uint32_t)(spriteIndex << 2));
    type = hdr->type;
    srcData = (uint8_t *)hdr->sprData;
    dstMask = (uint8_t *)hdr->maskPtr;
    nlines = (uint16_t)hdr->nlines;

    stride2 = (uint16_t)hdr->wquads * 2;
    stride3 = stride2 + stride2;
    stride4 = stride3 + stride2;
    srcAdvance = (int32_t)(stride2 * 4);
    dstAdvance = (int32_t)stride2;

    if (type == 0) {
        /* 1-plane output */
        for (row = nlines - 1; row >= 0; row--) {
            uint16_t val = *(uint16_t *)srcData;
            val |= *(uint16_t *)(srcData + stride2);
            val |= *(uint16_t *)(srcData + stride3);
            val |= *(uint16_t *)(srcData + stride4);
            *(uint16_t *)dstMask = val;
            srcData += srcAdvance;
            dstMask += dstAdvance;
        }
    } else {
        /* 4-plane output */
        for (row = nlines - 1; row >= 0; row--) {
            uint16_t val = *(uint16_t *)srcData;
            val |= *(uint16_t *)(srcData + stride2);
            val |= *(uint16_t *)(srcData + stride3);
            val |= *(uint16_t *)(srcData + stride4);
            *(uint16_t *)dstMask = val;
            *(uint16_t *)(dstMask + stride2) = val;
            *(uint16_t *)(dstMask + stride3) = val;
            *(uint16_t *)(dstMask + stride4) = val;
            srcData += srcAdvance;
            dstMask += srcAdvance;
        }
    }
}


/* ================================================================
 * _BlitSpritePlanar_Long_StridedDest
 *
 * Like BlitSpritePlanar_Word_StridedDest but uses 32-bit (longword)
 * access for wider sprites.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _BlitSpritePlanar_Long_StridedDest(uint16_t spriteIndex)
{
    uint8_t *base = spriteGraphicsPtr;
    SpriteGraphics *hdr;
    uint16_t type;
    uint8_t *srcData, *dstMask;
    uint16_t nlines;
    uint16_t stride2, stride3, stride4;
    int32_t srcAdvance, dstAdvance;
    int16_t row;

    hdr = *(SpriteGraphics **)(base + (uint32_t)(spriteIndex << 2));
    type = hdr->type;
    srcData = (uint8_t *)hdr->sprData;
    dstMask = (uint8_t *)hdr->maskPtr;
    nlines = (uint16_t)hdr->nlines;

    stride2 = (uint16_t)hdr->wquads * 2;
    stride3 = stride2 + stride2;
    stride4 = stride3 + stride2;
    srcAdvance = (int32_t)(stride2 * 4);
    dstAdvance = (int32_t)stride2;

    if (type == 0) {
        /* 1-plane output */
        for (row = nlines - 1; row >= 0; row--) {
            uint32_t val = *(uint32_t *)srcData;
            val |= *(uint32_t *)(srcData + stride2);
            val |= *(uint32_t *)(srcData + stride3);
            val |= *(uint32_t *)(srcData + stride4);
            *(uint32_t *)dstMask = val;
            srcData += srcAdvance;
            dstMask += dstAdvance;
        }
    } else {
        /* 4-plane output */
        for (row = nlines - 1; row >= 0; row--) {
            uint32_t val = *(uint32_t *)srcData;
            val |= *(uint32_t *)(srcData + stride2);
            val |= *(uint32_t *)(srcData + stride3);
            val |= *(uint32_t *)(srcData + stride4);
            *(uint32_t *)dstMask = val;
            *(uint32_t *)(dstMask + stride2) = val;
            *(uint32_t *)(dstMask + stride3) = val;
            *(uint32_t *)(dstMask + stride4) = val;
            srcData += srcAdvance;
            dstMask += srcAdvance;
        }
    }
}


/* ================================================================
 * _BlitGlyphToSpriteBitmap
 *
 * Uses the Amiga blitter to render a glyph (character sprite) into
 * a destination sprite bitmap. Programs the blitter for each of
 * 4 bitplanes with appropriate minterm/function codes.
 *
 * In:
 *   glyphIndex     - source glyph sprite index
 *   xOff           - x pixel offset in destination sprite
 *   yOff           - y offset (row) in destination sprite
 *   planeMask      - 4-bit mask: which planes get the glyph color
 *   dstSpriteIndex - destination sprite index
 * ================================================================ */
__attribute__((used, externally_visible))
void _BlitGlyphToSpriteBitmap(uint16_t glyphIndex, uint16_t xOff, uint16_t yOff,
                              uint16_t planeMask, uint16_t dstSpriteIndex)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    uint8_t *base = spriteGraphicsPtr;
    SpriteGraphics *srcHdr, *dstHdr;
    uint8_t *dstBase;
    uint16_t dstRowBytes;
    int32_t dstRowAdv;
    uint16_t glyphHeight;
    uint16_t xBit, glyphPixWidth;
    uint16_t blitWords, lastWordMask;
    uint16_t bltconShift;
    uint16_t blitsize;
    uint8_t *maskPtr, *glyphData, *glyphPlane3;
    int32_t xWordOff;
    uint8_t *dstPtr;
    uint16_t srcAMod, srcBMod, dstMod;
    uint16_t srcStride2;
    int16_t i;

    /* Look up source glyph header */
    srcHdr = *(SpriteGraphics **)(base + (uint32_t)(glyphIndex << 2));

    /* Validate: type must be 0 (separate planes) */
    if (srcHdr->type != 0) {
        panicReasonCode = 0x1566;
        _PanicHaltLoop();
    }

    /* Look up destination sprite header */
    dstHdr = *(SpriteGraphics **)(base + (uint32_t)(dstSpriteIndex << 2));

    /* Destination row stride in bytes (per plane) */
    dstRowBytes = (uint16_t)dstHdr->wquads * 2;
    dstRowAdv = (int32_t)dstRowBytes;

    /* Destination data pointer */
    dstBase = (uint8_t *)dstHdr->sprData;

    /* Glyph height */
    glyphHeight = (uint16_t)srcHdr->nlines;

    /* Enable then wait for blitter idle */
    waitBlit();

    /* Compute x shift and word span */
    xBit = xOff & TILE_MASK;

    /* Glyph pixel width (masked to 4 bits, 0 means 16) */
    glyphPixWidth = (uint16_t)srcHdr->pixWidth & TILE_MASK;
    if (glyphPixWidth == 0)
        glyphPixWidth = 16;

    /* blitWords = number of destination words to touch */
    blitWords = (uint16_t)srcHdr->wquads;
    blitWords += 1;

    /* Check if glyph + shift fits in fewer words */
    lastWordMask = 0;
    if (xBit + glyphPixWidth <= 16) {
        lastWordMask = 0xFFFF;
        blitWords -= 1;
    }

    *(volatile uint16_t *)(custom + BLTALWM_OFF) = lastWordMask;
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = 0xFFFF;

    /* Shift amount → bltcon1 (descending=0, shift in bits 12-15 of con0) */
    bltconShift = xBit << 12;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = bltconShift;

    /* Validate mask pointer exists */
    maskPtr = (uint8_t *)srcHdr->maskPtr;
    if (maskPtr == 0) {
        panicReasonCode = 0x1567;
        _PanicHaltLoop();
    }

    /* Compute destination pointer: dstBase + xWordOff + yOff * rowStride * 4planes */
    xWordOff = (int32_t)((int16_t)(xOff >> 4) * 2);
    {
        uint32_t yRowOff = (uint32_t)yOff * dstRowBytes;
        yRowOff <<= NUM_PLANES_SHIFT;
        dstPtr = dstBase + yRowOff + xWordOff;
    }

    /* Compute blitsize: height in 6-bit field + words */
    blitsize = (glyphHeight << 6) + blitWords;

    /* Source word count */
    blitWords *= 2;  /* now in bytes */

    /* Source modulo: srcStride = wquads * 2, per row */
    srcStride2 = (uint16_t)srcHdr->wquads * 2;

    /* Source A (mask) modulo: stride2 - blitWords */
    srcAMod = srcStride2 - blitWords;

    /* Source B (glyph) modulo: stride2 * 4planes - blitWords */
    srcBMod = srcStride2 * 4 - blitWords;

    *(volatile uint16_t *)(custom + BLTAMOD_OFF) = srcAMod;
    *(volatile uint16_t *)(custom + BLTBMOD_OFF) = srcBMod;

    /* Dest modulo: dstRowBytes - blitWords + 3 * dstRowBytes = 4 * dstRowBytes - blitWords */
    dstMod = dstRowBytes * 4 - blitWords;
    *(volatile uint16_t *)(custom + BLTCMOD_OFF) = dstMod;
    *(volatile uint16_t *)(custom + BLTDMOD_OFF) = dstMod;

    /* Glyph data: a5 = data + srcStride2 (plane 1), a3 = data + 3*srcStride2 (plane 3) */
    glyphData = (uint8_t *)srcHdr->sprData;      /* sprData (plane 0 start) */
    glyphData += (int32_t)srcStride2;            /* advance to plane 1 */
    glyphPlane3 = glyphData + (int32_t)srcStride2 * 2;  /* plane 1 + 2*stride = plane 3 */

    /* ---- Blit passes 0-2 (3 iterations) ---- */
    for (i = 2; i >= 0; i--) {
        planeMask >>= 1;

        if (planeMask & 1) {
            /* Cookie-cut: D = (A & B) | (~A & C), BLTCON0 = $FCA + shift */
            uint16_t con0 = bltconShift + BLTCON0_BASE_LINKED;
            *(volatile uint16_t *)(custom + BLTCON0_OFF) = con0;
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)maskPtr;
            *(volatile uint32_t *)(custom + BLTBPT_OFF) = (uint32_t)glyphData;
            *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)dstPtr;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)dstPtr;
        } else {
            /* Clear-through-mask: D = ~A & C, BLTCON0 = $B0A + shift */
            uint16_t con0 = bltconShift + BLTCON0_CLEAR_MASK;
            *(volatile uint16_t *)(custom + BLTCON0_OFF) = con0;
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)maskPtr;
            *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)dstPtr;
            *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)dstPtr;
        }

        /* Kick blit */
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = blitsize;

        /* Wait for blit to complete */
        waitBlit();

        /* Advance destination to next plane */
        dstPtr += dstRowAdv;
    }

    /* ---- Final pass (plane 3): asymmetric ---- */
    planeMask >>= 1;

    if (planeMask & 1) {
        /* OR-mask: D = (A & B) | C, BLTCON0 = $BFA + shift */
        uint16_t con0 = bltconShift + BLTCON0_OR_MASK;
        *(volatile uint16_t *)(custom + BLTCON0_OFF) = con0;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)maskPtr;
        *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)dstPtr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)dstPtr;
    } else {
        /* Cookie-cut with plane 3 data: D = (A & B) | (~A & C), use glyphPlane3 */
        uint16_t con0 = bltconShift + BLTCON0_BASE_LINKED;
        *(volatile uint16_t *)(custom + BLTCON0_OFF) = con0;
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)maskPtr;
        *(volatile uint32_t *)(custom + BLTBPT_OFF) = (uint32_t)glyphPlane3;
        *(volatile uint32_t *)(custom + BLTCPT_OFF) = (uint32_t)dstPtr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)dstPtr;
    }

    /* Kick final blit (no wait — caller or next operation will sync) */
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = blitsize;
}


/* ================================================================
 * _Text2Sprite
 *
 * Renders text into a sprite bitmap. Iterates over each character,
 * looks up the glyph sprite, and blits it into the destination sprite.
 *
 * In:
 *   xOff           - initial x pixel offset in destination sprite
 *   yOff           - y offset (for EGA compatibility, usually 0)
 *   dstSpriteIndex - destination sprite index
 *   text           - null-terminated ASCII string
 *   charsTable     - character table (spacing, conversion, sprite indices)
 *
 * Out:
 *   Returns 0 if OK (text fits), 1 if text wider than sprite.
 *   *outTotalWidth = final x position (total pixel width).
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _Text2Sprite(uint16_t xOff, uint16_t yOff, uint16_t dstSpriteIndex,
                      uint8_t *text, uint8_t *charsTable, uint16_t planeMask,
                      uint16_t *outTotalWidth)
{
    uint8_t *base = spriteGraphicsPtr;
    SpriteGraphics *dstHdr;
    uint16_t textWidth;
    uint16_t spriteIndexOffset;
    uint16_t charSpacing;
    uint16_t spaceWidth;
    uint8_t *convTable;
    uint8_t ch;

    /* Get total text width (GetTextWidth already ported to C) */
    textWidth = _GetTextWidth(text, charsTable);
    textWidth += xOff;

    /* Check if text fits in destination sprite */
    dstHdr = *(SpriteGraphics **)(base + (uint32_t)(dstSpriteIndex << 2));
    if (textWidth > (uint16_t)dstHdr->pixWidth)
        return 1;  /* error: text wider than sprite */

    /* Extract char table fields */
    CharsTableHeader *ct = (CharsTableHeader *)charsTable;
    spriteIndexOffset = (uint16_t)ct->spriteIndexOffset;
    charSpacing = (uint16_t)ct->charSpacing;
    spaceWidth = (uint16_t)ct->spaceWidth;
    convTable = (uint8_t *)(ct + 1);

    /* Process each character */
    while ((ch = *text++) != 0) {
        if (ch == ' ') {
            xOff += spaceWidth;
        } else {
            uint16_t glyphIdx;
            SpriteGraphics *glyphHdr;
            uint16_t glyphWidth;
            uint16_t nextX;

            /* Map ASCII to glyph index */
            glyphIdx = (uint16_t)convTable[ch - ' '] + spriteIndexOffset;

            /* Get glyph pixel width */
            glyphHdr = *(SpriteGraphics **)(base + (uint32_t)(glyphIdx << 2));
            glyphWidth = (uint16_t)glyphHdr->pixWidth;
            nextX = xOff + glyphWidth + charSpacing;

            /* Blit glyph into destination sprite */
            _BlitGlyphToSpriteBitmap(glyphIdx, xOff, yOff, planeMask, dstSpriteIndex);

            xOff = nextX;
        }
    }

    /* Build OR-combined mask for the final sprite */
    _BlitSpritePlanar_Word(dstSpriteIndex);

    /* Return total width to caller via output parameter */
    if (outTotalWidth)
        *outTotalWidth = xOff;

    return 0;
}
