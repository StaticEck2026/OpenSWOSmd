/*
 * scroll_blit.c — Scroll blitter system
 *
 * Handles updating the frontbuffer when the camera scrolls:
 *   - ScrollXUpdateColumnsAndBlit: blits pitch tile columns on X scroll
 *   - ScrollYUpdateRowsAndBlit: CPU row copy + blitter mask copy on Y scroll
 *
 * Ported from SWOS2.s lines 5504-5884.
 */

#include "scroll_blit.h"
#include "display.h"

/* ---- External globals ---- */
extern int16_t  cameraX;
extern int16_t  cameraY;
extern int16_t  oldCameraX;
extern int16_t  oldCameraY;
extern uint16_t cameraXMod320;
extern uint16_t cameraYMod320;
extern uint16_t cameraCoordinatesValid;
extern volatile uint32_t frontbufferBasePtr;
extern uint8_t  frontbufferMaskBuffer[];
extern uint8_t  pitchDatBuffer[];

/* ---- Scroll-blit-specific constants ---- */
#define SCROLL_RIGHT_EDGE_X         335

/* ---- Inline data for ScrollYUpdateRowsAndBlit ---- */
static uint16_t blitWordCount;
static uint32_t maskBlitDestPtr;
static uint16_t maskBlitPendingBytesA;
static uint32_t maskBlitSecondStreamPtr;
static uint16_t maskBlitPendingBytesB;

/* ---- Wait for blitter idle ---- */
static void waitBlit(void)
{
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT))
        ;
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;
}

/* ================================================================
 * ScrollXUpdateColumnsAndBlit  (SWOS2.s lines 5504-5678)
 *
 * Compares cameraX vs oldCameraX to detect horizontal scroll.
 * If a tile boundary is crossed, blits a column of pitch tiles
 * into the frontbuffer, then copies that column to the mask buffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ScrollXUpdateColumnsAndBlit(void)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    int16_t d0, d1;
    uint16_t d2, d3, d5;
    uint8_t *a0;
    uint8_t *a1;

    /* Check if camera moved horizontally */
    d0 = cameraX;
    if (d0 == oldCameraX)
        return;

    if (d0 >= oldCameraX) {
        /* Moved right */
        d5 = SCROLL_RIGHT_EDGE_X;
        d0 = oldCameraX + SCROLL_RIGHT_EDGE_X;
        d0 >>= TILE_SHIFT_X;  /* asr.w */
        d1 = cameraX + SCROLL_RIGHT_EDGE_X;
        d1 >>= TILE_SHIFT_X;  /* asr.w */
    } else {
        /* Moved left */
        d5 = 0;
        d0 = oldCameraX >> TILE_SHIFT_X;  /* asr.w (signed) */
        d1 = cameraX >> TILE_SHIFT_X;     /* asr.w (signed) */
    }

    /* Did we cross a tile boundary? */
    if (d1 == d0) {
        /* No tile boundary crossed — just update oldCameraX */
        oldCameraX = cameraX;
        cameraCoordinatesValid = 0;
        return;
    }

    /* Tile boundary crossed — do the column blit */
    cameraCoordinatesValid = 0;
    oldCameraX = cameraX;

    /* Compute pitch data pointer for the tile column */
    {
        uint16_t tileRow = (uint16_t)cameraY >> TILE_SHIFT_X;  /* lsr.w */
        uint32_t pitchOff = (uint32_t)tileRow * PITCH_PTR_ROW_BYTES;
        uint16_t tileCol = ((uint16_t)(cameraX + d5)) >> TILE_SHIFT_X;  /* lsr.w */
        tileCol <<= 2;
        pitchOff += tileCol;
        a1 = pitchDatBuffer + pitchOff;
    }

    /* Compute frontbuffer destination pointer */
    {
        uint32_t xByteOff = ((uint16_t)(cameraXMod320 + d5)) >> 3;
        xByteOff &= WORD_ALIGN_EVEN_MASK;
        uint32_t yByteOff = (uint32_t)cameraYMod320 * ROW_STRIDE_BYTES;
        a0 = (uint8_t *)frontbufferBasePtr + xByteOff + yByteOff;
    }

    /* Set up blitter for column copy (A → D, no shift) */
    waitBlit();
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)a0;
    *(volatile uint16_t *)(custom + BLTAMOD_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTDMOD_OFF) = BLTDMOD_PLANE_STEP;
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTALWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTCON0_OFF) = BLTCON0_USEA_USED_LF_F0;

    /* ---- Phase 1: top of frontbuffer (from cameraYMod320 to SCREEN_HEIGHT) ---- */
    d2 = cameraYMod320;
    d1 = d2 & TILE_MASK;

    if (d1 != 0) {
        /* Partial first tile — blit remainder of top tile */
        d3 = TILE_HEIGHT - d1;
        d2 += d3;

        /* d1 = row offset into tile source (lsl.w #3, ext.l) */
        uint16_t srcOff = d1 << NUM_PLANES_SHIFT_PLUS1;

        /* bltsize: d3 rows * NUM_PLANES, 1 word wide
         * BLTSIZE = (height << 6) | width, height = d3 * NUM_PLANES */
        uint16_t bltsize = ((d3 << NUM_PLANES_SHIFT) << 6) | 1;

        a0 = *(uint8_t **)a1 + srcOff;
        a1 += PITCH_PTR_ROW_BYTES;

        waitBlit();
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)a0;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
    }

    /* Blit full tiles until we reach SCREEN_HEIGHT */
    while (d2 < SCREEN_HEIGHT) {
        a0 = *(uint8_t **)a1;
        a1 += PITCH_PTR_ROW_BYTES;

        waitBlit();
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)a0;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = BLTSIZE_TILE_COL;
        d2 += TILE_HEIGHT;
    }

    /* ---- Phase 2: wrap to top of frontbuffer ---- */
    /* Reset destination to top of column (no Y offset) */
    {
        uint32_t xByteOff = ((uint16_t)(cameraXMod320 + d5)) >> 3;
        xByteOff &= WORD_ALIGN_EVEN_MASK;
        a0 = (uint8_t *)frontbufferBasePtr + xByteOff;
    }

    waitBlit();
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = (uint32_t)a0;

    /* Blit full tiles from wrap point down to cameraYMod320 */
    d2 = cameraYMod320;
    if (d2 >= TILE_HEIGHT) {
        do {
            a0 = *(uint8_t **)a1;
            a1 += PITCH_PTR_ROW_BYTES;

            waitBlit();
            *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)a0;
            *(volatile uint16_t *)(custom + BLTSIZE_OFF) = BLTSIZE_TILE_COL;
            d2 -= TILE_HEIGHT;
        } while (d2 >= TILE_HEIGHT);
    }

    /* Partial bottom remainder */
    if (d2 != 0) {
        a0 = *(uint8_t **)a1;
        uint16_t bltsize = ((d2 << NUM_PLANES_SHIFT) << 6) | 1;

        waitBlit();
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)a0;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
    }

    /* ---- Phase 3: copy column from frontbuffer to mask buffer ---- */
    {
        uint32_t fbColOff = ((uint16_t)(cameraXMod320 + d5)) >> 3;
        fbColOff &= WORD_ALIGN_EVEN_MASK;
        uint32_t srcAddr = frontbufferBasePtr + fbColOff;
        uint32_t dstAddr = srcAddr - frontbufferBasePtr + (uint32_t)frontbufferMaskBuffer;

        waitBlit();
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = dstAddr;
        *(volatile uint16_t *)(custom + BLTAMOD_OFF) = BLTDMOD_PLANE_STEP;
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = BLTSIZE_TILE_COL;
    }

    /* ---- Final: 1-word dummy blit ---- */
    waitBlit();
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = 1;
}

/* ================================================================
 * ScrollYUpdateRowsAndBlit  (SWOS2.s lines 5682-5872)
 *
 * Compares cameraY vs oldCameraY to detect vertical scroll.
 * CPU-based row copy from pitch tile data into frontbuffer.
 * Blitter-based mask copy from frontbuffer to mask buffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ScrollYUpdateRowsAndBlit(void)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    int16_t deltaY;
    uint16_t d4, d5, d6;
    int32_t d3;       /* src scanline offset within tile (ext.l) */
    uint32_t d7;      /* dest stride per row */
    int16_t rowCount;
    uint8_t *a1;      /* pitch pointer table cursor */
    uint8_t *a3;      /* dest plane 0 */
#ifndef AGA
    uint8_t *a4;      /* dest plane 1 */
    uint8_t *a5;      /* dest plane 2 */
    uint8_t *a6_ptr;  /* dest plane 3 */
#endif

    /* Check if camera moved vertically */
    deltaY = cameraY - oldCameraY;
    if (deltaY == 0)
        return;

    if (deltaY > 0) {
        /* Scrolled down: d5 = SCREEN_HEIGHT - deltaY (first dest row) */
        d5 = SCREEN_HEIGHT - deltaY;
    } else {
        /* Scrolled up: d5 = 0 (first dest row), deltaY = abs */
        d5 = 0;
        deltaY = -deltaY;
    }

    oldCameraY = cameraY;
    cameraCoordinatesValid = 0;

    /* ---- Compute pitch data pointer ---- */
    a1 = pitchDatBuffer;
    {
        uint16_t tileRow = (uint16_t)(cameraY + d5) >> TILE_SHIFT_X;
        uint32_t pitchRowOff = (uint32_t)tileRow * PITCH_PTR_ROW_BYTES;
        a1 += pitchRowOff;
        uint16_t tileCol = (uint16_t)cameraX >> TILE_SHIFT_X;
        tileCol <<= 2;
        int32_t colOff = (int32_t)(int16_t)tileCol;  /* ext.l d0 */
        a1 += colOff;
    }

    /* ---- Compute frontbuffer dest base ---- */
    a3 = (uint8_t *)frontbufferBasePtr;
    {
        uint32_t xByteOff = (uint16_t)cameraXMod320 >> 3;
        xByteOff &= WORD_ALIGN_EVEN_MASK;
        a3 += xByteOff;
        uint16_t yWrapped = cameraYMod320 + d5;
        if (yWrapped >= SCREEN_HEIGHT)
            yWrapped -= SCREEN_HEIGHT;
        uint32_t yByteOff = (uint32_t)yWrapped * ROW_STRIDE_BYTES;
        a3 += yByteOff;
    }

    /* ---- Determine word count and dest stride per row ---- */
    if (((uint16_t)cameraX & 0xF) != 0) {
        d6 = 0x15;  /* 21 words (unaligned) */
        d7 = PITCH_DEST_MOD_UNALIGNED;
    } else {
        d6 = 0x14;  /* 20 words (aligned) */
        d7 = PITCH_DEST_MOD_ALIGNED;
    }

    /* ---- Init plane pointers ---- */
#ifndef AGA
    a4 = a3 + PLANE_STRIDE;
    a5 = a3 + PLANE_STRIDE_TIMES2;
    a6_ptr = a3 + PITCH_DEST_MOD_UNALIGNED;  /* plane 3 = a3 + 132 */
#endif

    /* ---- Compute wrapped dest Y position ---- */
    d4 = cameraYMod320 + d5;
    if (d4 >= SCREEN_HEIGHT)
        d4 -= SCREEN_HEIGHT;

    /* ---- Init mask blit accumulators ---- */
    maskBlitDestPtr = (uint32_t)a3;
    blitWordCount = d6 + 1;
    maskBlitPendingBytesA = 0;
    maskBlitPendingBytesB = 0;
    maskBlitSecondStreamPtr = 0;

    /* ---- Compute source scanline offset within tile ---- */
    d3 = (int32_t)(uint16_t)(((uint16_t)(cameraY + d5)) & TILE_MASK);
    d3 <<= NUM_PLANES_SHIFT_PLUS1;  /* * 8 bytes per scanline */

    /* ---- CPU row copy loop ---- */
    rowCount = deltaY - 1;  /* dbf counts down to -1 */
    do {
        int16_t wordIdx = d6;
        uint8_t *a1_saved = a1;

        /* Inner loop: copy one row of words across all tiles */
        do {
            uint8_t *tilePtr = *(uint8_t **)a1 + d3;
            a1 += 4;  /* next tile pointer */

#ifdef AGA
            /* Copy one word per plane (8 planes, offset from a3) */
            {
                int16_t p;
                for (p = 0; p < NUM_PLANES; p++)
                {
                    *(uint16_t *)(a3 + p * PLANE_STRIDE) = *(uint16_t *)tilePtr;
                    tilePtr += 2;
                }
            }
            a3 += 2;
#else
            /* Copy one word per plane (4 planes interleaved) */
            *(uint16_t *)a3 = *(uint16_t *)tilePtr; tilePtr += 2;
            a3 += 2;
            *(uint16_t *)a4 = *(uint16_t *)tilePtr; tilePtr += 2;
            a4 += 2;
            *(uint16_t *)a5 = *(uint16_t *)tilePtr; tilePtr += 2;
            a5 += 2;
            *(uint16_t *)a6_ptr = *(uint16_t *)tilePtr;
            a6_ptr += 2;
#endif

            wordIdx--;
        } while (wordIdx >= 0);

        a1 = a1_saved;

        maskBlitPendingBytesA += NUM_PLANES;
        maskBlitPendingBytesB += NUM_PLANES;

        /* Advance source scanline within tile */
        d3 += SRC_SCANLINE_BYTES;
        if (d3 == TILE_SIZE) {
            /* Crossed tile boundary — advance pitch pointer row */
            a1 += PITCH_PTR_ROW_BYTES;
            d3 = 0;
        }

        /* Advance dest pointers by row stride */
        a3 += d7;
#ifndef AGA
        a4 += d7;
        a5 += d7;
        a6_ptr += d7;
#endif

        /* Handle frontbuffer Y-wrap */
        d4 += 1;
        if (d4 == SCREEN_HEIGHT) {
            a3 -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
#ifndef AGA
            a4 -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
            a5 -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
            a6_ptr -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
#endif
            d4 = 0;
            maskBlitSecondStreamPtr = (uint32_t)a3;
            maskBlitPendingBytesB = 0;
        }

        rowCount--;
    } while (rowCount >= 0);

    /* ---- Blitter mask copy: frontbuffer → mask buffer ---- */
    waitBlit();
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTALWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTCON0_OFF) = BLTCON0_USEA_USED_LF_F0;

    {
        uint16_t modVal = (PLANE_STRIDE_DIV2 - blitWordCount) * 2;
        *(volatile uint16_t *)(custom + BLTAMOD_OFF) = modVal;
        *(volatile uint16_t *)(custom + BLTDMOD_OFF) = modVal;
    }

    /* If there's a second stream, subtract its bytes from stream A */
    if (maskBlitSecondStreamPtr != 0) {
        maskBlitPendingBytesA -= maskBlitPendingBytesB;
    }

    /* Set up source and dest for stream A */
    {
        uint32_t srcAddr = maskBlitDestPtr;
        uint32_t dstAddr = srcAddr - frontbufferBasePtr + (uint32_t)frontbufferMaskBuffer;

        waitBlit();
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = dstAddr;
    }

    /* ---- Stream A: blit in batches of max 0x400 rows ---- */
    for (;;) {
        uint16_t batch = maskBlitPendingBytesA;
        if (batch == 0)
            break;
        if (batch > 0x400)
            batch = 0x400;
        maskBlitPendingBytesA -= batch;

        uint16_t bltsize = (batch << 6) | blitWordCount;

        waitBlit();
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
    }

    /* ---- Stream B (if Y-wrap occurred) ---- */
    if (maskBlitSecondStreamPtr == 0)
        return;
    if (maskBlitPendingBytesB == 0)
        return;

    {
        uint32_t srcAddr = maskBlitSecondStreamPtr;
        uint32_t dstAddr = srcAddr - frontbufferBasePtr + (uint32_t)frontbufferMaskBuffer;

        waitBlit();
        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcAddr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = dstAddr;
    }

    /* ---- Stream B batches ---- */
    for (;;) {
        uint16_t batch = maskBlitPendingBytesB;
        if (batch == 0)
            return;
        if (batch > 0x400)
            batch = 0x400;
        maskBlitPendingBytesB -= batch;

        uint16_t bltsize = (batch << 6) | blitWordCount;

        waitBlit();
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = bltsize;
    }
}
