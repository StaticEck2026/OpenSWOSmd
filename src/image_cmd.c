/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "image_cmd.h"
#include "game_types.h"
#include "display.h"

/* ---- External globals (all live in SWOS2.s) ----
 * imageStreamCursor, frontbufferBasePtr, spriteGraphicsPtr are pointers stored
 * as uint32_t in asm. We declare them matching other C files and cast in use.
 */
extern uint32_t imageStreamCursor;           /* actually uint8_t*, match frontend.c */
extern uint8_t frontbufferMaskBuffer[];
extern volatile uint32_t frontbufferBasePtr;         /* actually uint8_t*, match display.c */
extern uint16_t panicReasonCode;
extern uint8_t *spriteGraphicsPtr;          /* actually uint32_t*[], match game_utils.c */
extern int16_t currentSpriteIndex;
extern volatile uint16_t playGame;
extern volatile uint16_t playHighlightFlag;
extern uint16_t useIndividualPlayerSkinColor;
extern uint32_t imageStreamBytesLeft;
extern uint8_t menuInsBank[];

/* ---- External functions ---- */
extern void _PanicHaltLoop(void) __attribute__((noreturn));
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);


/* ================================================================
 * blitDispatch  (SWOS2.s line 6481)
 *
 * Builds a SpriteGraphics entry in the image stream, copies/generates
 * pixel data and mask data.
 *
 * Parameters (matching asm register assignments):
 *   spriteIndex  (d0)  - index into sprite table
 *   xOffset      (d1)  - source x offset (pixels, shifted to byte)
 *   yOffset      (d2)  - source y offset (pixels, multiplied by stride)
 *   width        (d3)  - source pixel width
 *   height       (d4)  - source height in lines
 *   maskSel      (d5)  - mask selector: 0=no mask, >0=sprite index, <0=inline compose
 *   dstX         (d6)  - destination X offset
 *   dstY         (d7)  - destination Y offset
 *   src          (a1)  - source buffer pointer (NULL = checkerboard fill)
 *   planesFlag   (a6)  - 0=single plane mask, 1=multi-plane mask
 * ================================================================ */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
static void blitDispatch(uint16_t spriteIndex, uint16_t xOffset, uint16_t yOffset,
                         uint16_t width, uint16_t height, uint16_t maskSel,
                         int16_t dstX, int16_t dstY,
                         uint8_t *src, uint32_t planesFlag)
{
    SpriteGraphics *headerBase;  /* pointer to SpriteGraphics header */
    uint8_t *pixelPtr;           /* working write cursor for pixel/mask data */
    uint16_t wquads;             /* width in 16-pixel tiles */
    int32_t rowSkip;             /* bytes to skip between source rows */
    uint16_t i, j;
    uint16_t *wp;

    /* Panic if mask selector is zero */
    if (maskSel == 0) {
        panicReasonCode = 0x00C6;
        _PanicHaltLoop();
    }

    /* ---- Sprite index filtering (game mode only) ---- */
    if (playGame != 0 && useIndividualPlayerSkinColor == 0) {
        if (spriteIndex >= 0x148 && spriteIndex <= 0x1AC) return;
        if (spriteIndex >= 0x1AD && spriteIndex <= 0x211) return;
        if (spriteIndex >= 0x277 && spriteIndex <= 0x2DB) return;
        if (spriteIndex >= 0x2DC && spriteIndex <= 0x340) return;
        if (spriteIndex >= 0x37B && spriteIndex <= 0x3B4) return;
        if (spriteIndex >= 0x3EF && spriteIndex <= 0x428) return;
    }

    /* ---- Write sprite table entry pointer ---- */
    headerBase = (SpriteGraphics *)imageStreamCursor;
    ((uint32_t *)spriteGraphicsPtr)[spriteIndex] = (uint32_t)headerBase;

    /* ---- Write SpriteGraphics header ---- */
    headerBase->ordinal       = (int16_t)spriteIndex;
    currentSpriteIndex        = (int16_t)spriteIndex;
    headerBase->centerX       = dstX;
    headerBase->centerY       = dstY;
    headerBase->pixWidth      = (int16_t)width;
    headerBase->nlines        = (int16_t)height;
    headerBase->type          = 0;
    if (planesFlag != 0)
        headerBase->type      = 0xFF;  /* sne sets 0xFF when condition true */

    /* Adjust source pointer for x/y offset (if src != NULL) */
    if (src != (uint8_t *)0) {
        uint32_t byteOff = (uint32_t)((xOffset >> 3) & 0xFFFE);
        uint32_t lineOff = (uint32_t)yOffset * IMAGE_ROW_STRIDE_BYTES;
        src += byteOff + lineOff;
    }

    /* ---- Calculate tile dimensions ---- */
    wquads = (uint16_t)((width + 15) >> 4);
    headerBase->wquads = (int16_t)wquads;

    /* nlinesDiv4 << 8 | nlines/height encoding:
       asm does: lsl.w #6,d0; add.w d3,d0; add.w d3,d0
       → d0 = height * 64 + wquads * 2 */
    {
        uint16_t val = (uint16_t)(height << 6) + wquads + wquads;
        headerBase->bltsize = val;
    }

    /* sprData pointer = start of payload (just past the header) */
    headerBase->sprData = (uint32_t)(headerBase + 1);
    pixelPtr = (uint8_t *)(headerBase + 1);

    /* ---- Copy or fill pixel data ---- */
    if (src == (uint8_t *)0) {
        /* Fill with checkerboard pattern 0x5555 */
        rowSkip = (int32_t)(BITPLANE_ROW_BYTES - (int32_t)wquads * 2);
        wp = (uint16_t *)pixelPtr;
        {
            uint16_t totalRows = (uint16_t)(height * NUM_PLANES);
            for (i = 0; i < totalRows; i++) {
                for (j = 0; j < wquads; j++) {
                    *wp++ = 0x5555;
                }
            }
        }
        pixelPtr = (uint8_t *)wp;
    } else {
        /* Copy from source buffer, row by row across all bitplanes */
        rowSkip = (int32_t)(BITPLANE_ROW_BYTES - (int32_t)wquads * 2);
        {
            uint16_t *srcW = (uint16_t *)src;
            uint16_t totalRows = (uint16_t)(height * NUM_PLANES);
            wp = (uint16_t *)pixelPtr;
            for (i = 0; i < totalRows; i++) {
                for (j = 0; j < wquads; j++) {
                    *wp++ = *srcW++;
                }
                srcW = (uint16_t *)((uint8_t *)srcW + rowSkip);
            }
            pixelPtr = (uint8_t *)wp;
        }
    }

    /* ---- Mask data generation ---- */
    /* Clear mask pointer initially */
    headerBase->maskPtr = 0;

    if (maskSel == 0) {
        /* No mask — jump to cursor update */
        goto update_cursor;
    }

    if ((int16_t)maskSel > 0) {
        /* Positive maskSel: copy mask pointer from another sprite */
        SpriteGraphics *otherSprite = (SpriteGraphics *)(((uint32_t *)spriteGraphicsPtr)[maskSel]);
        headerBase->maskPtr = otherSprite->maskPtr;
        goto update_cursor;
    }

    /* Negative maskSel (bit 15 set): inline mask composition */
    headerBase->maskPtr = (uint32_t)pixelPtr;

    if (src == (uint8_t *)0 || maskSel == (uint16_t)0xFFFE) {
        /* Generate right-edge mask */
        uint16_t edgeBits = (uint16_t)(wquads * 16 - (uint16_t)headerBase->pixWidth);
        uint16_t edgeMask = (uint16_t)(0xFFFF << edgeBits);
        uint16_t totalRows = (uint16_t)headerBase->nlines;
        if (planesFlag != 0)
            totalRows *= NUM_PLANES;

        wp = (uint16_t *)pixelPtr;
        for (i = 0; i < totalRows; i++) {
            for (j = 0; j + 1 < wquads; j++) {
                *wp++ = 0xFFFF;
            }
            *wp++ = edgeMask;
        }
        pixelPtr = (uint8_t *)wp;
    } else {
        /* Compose mask by OR'ing bitplanes together */
        uint8_t *pixelData = (uint8_t *)headerBase->sprData;
        uint16_t nlines = (uint16_t)headerBase->nlines;
        uint16_t planeWordStride = wquads * 2;  /* bytes per plane row */
        uint16_t rowIdx;

        wp = (uint16_t *)pixelPtr;
        for (rowIdx = 0; rowIdx < nlines; rowIdx++) {
            uint16_t planeRepeat;
            uint16_t *rowBase;

            if (planesFlag == 0)
                planeRepeat = 1;
            else
                planeRepeat = NUM_PLANES;

            rowBase = (uint16_t *)pixelData;

            for (i = 0; i < planeRepeat; i++) {
                uint16_t *plane0 = rowBase;
                for (j = 0; j < wquads; j++) {
                    uint16_t accum = *plane0;
                    uint16_t offset = planeWordStride;
                    uint16_t k;
                    for (k = 0; k < NUM_PLANES_MINUS1; k++) {
                        accum |= *(uint16_t *)((uint8_t *)plane0 + offset);
                        offset += planeWordStride;
                    }
                    *wp++ = accum;
                    plane0++;
                }
            }

            /* Advance pixelData by wquads * NUM_PLANES * 2 bytes per scanline */
            pixelData += (uint32_t)wquads * (1 << NUM_PLANES_SHIFT_PLUS1);
        }
        pixelPtr = (uint8_t *)wp;
    }

update_cursor:
    /* ---- Update stream cursor and check bounds ---- */
    imageStreamCursor = (uint32_t)pixelPtr;

    if (playGame == 0 && playHighlightFlag == 0) {
        /* Menu mode: check against menuInsBank boundary */
        imageStreamBytesLeft = (uint32_t)((uint8_t *)menuInsBank - pixelPtr);
        if (pixelPtr >= (uint8_t *)menuInsBank) {
            panicReasonCode = 0x0008;
            _PanicHaltLoop();
        }
    } else {
        /* Game mode: check against imageStreamCursor itself (static buffer end) */
        imageStreamBytesLeft = (uint32_t)((uint8_t *)&imageStreamCursor - pixelPtr);
        if (pixelPtr >= (uint8_t *)&imageStreamCursor) {
            panicReasonCode = 0x0008;
            _PanicHaltLoop();
        }
    }
}

/* ================================================================
 * _ProcessImageCommandStream  (SWOS2.s line 6253)
 *
 * Image command stream interpreter. Reads opcodes from cmdBuffer,
 * dispatches drawing operations via blitDispatch, and finishes by
 * copying the frontbuffer to the mask buffer.
 *
 * Opcode format: upper 4 bits = type, lower 12 bits = sprite index.
 * 0xFFFF = end of stream.
 *
 * Parameters:
 *   cmdBuffer  (a0) - pointer to the command stream
 *   workBuffer (a3) - destination buffer for sprite data
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _ProcessImageCommandStream(const uint16_t *cmdBuffer, uint8_t *workBuffer)
{
    const uint8_t *a0 = (const uint8_t *)cmdBuffer;
    uint16_t opcode, opcodeType;
    uint16_t spriteIndex, xOff, yOff, w, h, maskSel;
    int16_t dstX, dstY;
    uint32_t planesFlag;

    imageStreamCursor = (uint32_t)workBuffer;

    for (;;) {
        opcode = *(const uint16_t *)a0;
        a0 += 2;

        if (opcode == 0xFFFF)
            break;

        opcodeType = opcode & 0xF000;

        /* ---- Opcode 0x7000: Indirect file load ---- */
        if (opcodeType == 0x7000) {
            const uint8_t *savedA0 = a0 + 4;  /* skip the pointer long */
            const uint8_t *filename = *(const uint8_t *const *)a0;

            /* LoadFile with retry until success */
            while (_LoadFileEx((uint8_t *)filename, frontbufferMaskBuffer, (uint32_t *)0) != 0)
                ;

            a0 = savedA0;
            continue;
        }

        /* ---- Opcode 0x8000: Double-indirect file load ---- */
        if (opcodeType == 0x8000) {
            const uint8_t *savedA0 = a0 + 4;
            const uint8_t *ptr = *(const uint8_t *const *)a0;
            const uint8_t *filename = *(const uint8_t *const *)ptr;

            while (_LoadFileEx((uint8_t *)filename, frontbufferMaskBuffer, (uint32_t *)0) != 0)
                ;

            a0 = savedA0;
            continue;
        }

        /* ---- Opcode 0xC000: Byte x/y, then shared dispatch A ---- */
        if (opcodeType == 0xC000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = (uint16_t)(a0[0] & 0xFF);
            yOff = (uint16_t)(a0[1] & 0xFF);
            a0 += 2;
            goto dispatch_pack_A;
        }

        /* ---- Opcode 0x1000: Word x/y, then shared dispatch A ---- */
        if (opcodeType == 0x1000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = *(const uint16_t *)a0; a0 += 2;
            yOff = *(const uint16_t *)a0; a0 += 2;
            goto dispatch_pack_A;
        }

        /* ---- Opcode 0x5000: Like 0x1000 but maskSel=0xFFFE ---- */
        if (opcodeType == 0x5000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = *(const uint16_t *)a0; a0 += 2;
            yOff = *(const uint16_t *)a0; a0 += 2;
            w = (uint16_t)(a0[0] & 0xFF); a0++;
            h = (uint16_t)(a0[0] & 0xFF); a0++;
            maskSel = 0xFFFE;
            planesFlag = 0;
            dstX = 0;
            dstY = 0;
            blitDispatch(spriteIndex, xOff, yOff, w, h, maskSel, dstX, dstY,
                         frontbufferMaskBuffer, planesFlag);
            continue;
        }

        /* ---- Opcode 0xD000: Byte x/y, then shared dispatch B ---- */
        if (opcodeType == 0xD000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = (uint16_t)(a0[0] & 0xFF);
            yOff = (uint16_t)(a0[1] & 0xFF);
            a0 += 2;
            goto dispatch_pack_B;
        }

        /* ---- Opcode 0x2000: Word x/y, then shared dispatch B ---- */
        if (opcodeType == 0x2000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = *(const uint16_t *)a0; a0 += 2;
            yOff = *(const uint16_t *)a0; a0 += 2;
            goto dispatch_pack_B;
        }

        /* ---- Opcode 0xE000: Byte x/y, then shared dispatch C ---- */
        if (opcodeType == 0xE000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = (uint16_t)(a0[0] & 0xFF);
            yOff = (uint16_t)(a0[1] & 0xFF);
            a0 += 2;
            goto dispatch_pack_C;
        }

        /* ---- Opcode 0x3000: Word x/y, then shared dispatch C ---- */
        if (opcodeType == 0x3000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = *(const uint16_t *)a0; a0 += 2;
            yOff = *(const uint16_t *)a0; a0 += 2;
            goto dispatch_pack_C;
        }

        /* ---- Opcode 0xA000: Word x/y, planesFlag=1, maskSel=0xFFFF ---- */
        if (opcodeType == 0xA000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = *(const uint16_t *)a0; a0 += 2;
            yOff = *(const uint16_t *)a0; a0 += 2;
            w = (uint16_t)(a0[0] & 0xFF); a0++;
            h = (uint16_t)(a0[0] & 0xFF); a0++;
            dstX = (int16_t)(int8_t)a0[0]; a0++;
            dstY = (int16_t)(int8_t)a0[0]; a0++;
            maskSel = 0xFFFF;
            planesFlag = 1;
            blitDispatch(spriteIndex, xOff, yOff, w, h, maskSel, dstX, dstY,
                         frontbufferMaskBuffer, planesFlag);
            continue;
        }

        /* ---- Opcode 0xF000: Byte x/y, then shared dispatch D ---- */
        if (opcodeType == 0xF000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = (uint16_t)(a0[0] & 0xFF);
            yOff = (uint16_t)(a0[1] & 0xFF);
            a0 += 2;
            goto dispatch_pack_D;
        }

        /* ---- Opcode 0xB000: Word x/y, then shared dispatch D ---- */
        if (opcodeType == 0xB000) {
            spriteIndex = opcode & 0x0FFF;
            xOff = *(const uint16_t *)a0; a0 += 2;
            yOff = *(const uint16_t *)a0; a0 += 2;
            goto dispatch_pack_D;
        }

        /* ---- Opcode 0x9000: Draw from saved pointer (src=NULL) ---- */
        if (opcodeType == 0x9000) {
            spriteIndex = opcode & 0x0FFF;
            w = *(const uint16_t *)a0; a0 += 2;
            h = *(const uint16_t *)a0; a0 += 2;
            maskSel = 0xFFFF;
            planesFlag = 0;
            dstX = 0;
            dstY = 0;
            blitDispatch(spriteIndex, 0, 0, w, h, maskSel, dstX, dstY,
                         (uint8_t *)0, planesFlag);
            continue;
        }

        /* ---- Unknown opcode ---- */
        panicReasonCode = 0x0378;
        _PanicHaltLoop();

        /* ---- Shared dispatch entry points (using goto) ---- */
    dispatch_pack_A:
        /* Byte w/h, maskSel=0xFFFF, planesFlag=0, dstX=dstY=0 */
        w = (uint16_t)(a0[0] & 0xFF); a0++;
        h = (uint16_t)(a0[0] & 0xFF); a0++;
        maskSel = 0xFFFF;
        planesFlag = 0;
        dstX = 0;
        dstY = 0;
        blitDispatch(spriteIndex, xOff, yOff, w, h, maskSel, dstX, dstY,
                     frontbufferMaskBuffer, planesFlag);
        continue;

    dispatch_pack_B:
        /* Byte w/h + byte dstX/dstY (sign-extended), maskSel=0xFFFF, planesFlag=0 */
        w = (uint16_t)(a0[0] & 0xFF); a0++;
        h = (uint16_t)(a0[0] & 0xFF); a0++;
        dstX = (int16_t)(int8_t)a0[0]; a0++;
        dstY = (int16_t)(int8_t)a0[0]; a0++;
        maskSel = 0xFFFF;
        planesFlag = 0;
        blitDispatch(spriteIndex, xOff, yOff, w, h, maskSel, dstX, dstY,
                     frontbufferMaskBuffer, planesFlag);
        continue;

    dispatch_pack_C:
        /* Byte w/h + byte dstX/dstY (sign-extended) + word maskSel, planesFlag=0 */
        w = (uint16_t)(a0[0] & 0xFF); a0++;
        h = (uint16_t)(a0[0] & 0xFF); a0++;
        dstX = (int16_t)(int8_t)a0[0]; a0++;
        dstY = (int16_t)(int8_t)a0[0]; a0++;
        maskSel = *(const uint16_t *)a0; a0 += 2;
        planesFlag = 0;
        blitDispatch(spriteIndex, xOff, yOff, w, h, maskSel, dstX, dstY,
                     frontbufferMaskBuffer, planesFlag);
        continue;

    dispatch_pack_D:
        /* Byte w/h + byte dstX/dstY (sign-extended) + word maskSel, planesFlag=1 */
        w = (uint16_t)(a0[0] & 0xFF); a0++;
        h = (uint16_t)(a0[0] & 0xFF); a0++;
        dstX = (int16_t)(int8_t)a0[0]; a0++;
        dstY = (int16_t)(int8_t)a0[0]; a0++;
        maskSel = *(const uint16_t *)a0; a0 += 2;
        planesFlag = 1;
        blitDispatch(spriteIndex, xOff, yOff, w, h, maskSel, dstX, dstY,
                     frontbufferMaskBuffer, planesFlag);
        continue;
    }

    /* ---- End of stream: copy frontbuffer to mask buffer ---- */
    {
        const uint16_t *srcPtr = (const uint16_t *)frontbufferBasePtr;
        uint16_t *dstPtr = (uint16_t *)frontbufferMaskBuffer;
        uint16_t count;
        for (count = 0; count <= FB_COPY_WORDS_MINUS1; count++) {
            *dstPtr++ = *srcPtr++;
        }
    }
}
