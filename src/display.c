#include "display.h"

/* ---- External globals (all live in SWOS2.s) ---- */

/* Frontbuffer pointers */
extern uint8_t frontbufferBase[];
extern uint8_t frontbufferBaseAlt[];
extern uint8_t frontbufferMaskBuffer[];
extern volatile uint32_t frontbufferBasePtr;
extern volatile uint32_t frontbufferBaseAltPtr;

/* Bitplane bases (BSSInChip, 0x384 bytes each) */
extern uint8_t bitPlane0Base[];
extern uint8_t bitPlane1Base[];
extern uint8_t bitPlane2Base[];
extern uint8_t bitPlane3Base[];
extern uint8_t bitPlane0BaseAlt[];
extern uint8_t bitPlane1BaseAlt[];
extern uint8_t bitPlane2BaseAlt[];
extern uint8_t bitPlane3BaseAlt[];

/* Bitplane base pointers */
extern volatile uint32_t bitPlane0BasePtr;
extern volatile uint32_t bitPlane1BasePtr;
extern volatile uint32_t bitPlane2BasePtr;
extern volatile uint32_t bitPlane3BasePtr;

/* Copper list */
extern uint8_t copperListTemplate[];
extern uint8_t copperListBuffer[];
extern uint32_t copperZeroLong;

/* Sprite blit queue sentinels (4 bytes each: 2-byte sentinel + padding/queue) */
extern uint32_t kSpriteBlitQueueSentinel_A;
extern uint32_t kSpriteBlitQueueSentinel_B;
extern volatile uint32_t spriteBlitQueue;

/* Camera state */
extern int16_t cameraX;          /* dc.l but word-accessed (big-endian high word) */
extern int16_t cameraY;          /* dc.l but word-accessed (big-endian high word) */
extern uint16_t cameraXMod320;
extern uint16_t cameraYMod320;
extern int16_t oldCameraX;
extern int16_t oldCameraY;

/* Double-buffer saved camera state (slot 0) */
extern uint16_t dstXByteOffsetSaved0;
extern uint16_t dstYStartLinesSaved0;
extern int16_t oldCameraXSaved0;
extern int16_t oldCameraYSaved0;
extern int16_t cameraXSaved0;
extern int16_t cameraYSaved0;

/* Double-buffer saved camera state (slot 1) */
extern uint16_t dstXByteOffsetSaved1;
extern uint16_t dstYStartLinesSaved1;
extern int16_t oldCameraXSaved1;
extern int16_t oldCameraYSaved1;
extern int16_t cameraXSaved1;
extern int16_t cameraYSaved1;

/* Display flags */
extern uint16_t usePlaneOffsetFlag;
extern uint16_t useAltBaseFlag;
extern uint16_t paletteIndex;

/* Sprite graphics table pointer */
extern uint8_t *spriteGraphicsPtr;

/* ================================================================
 * Helper: write a 32-bit pointer into a copper list entry.
 * Each copper move pair is 8 bytes: [reg_hi][val_hi][reg_lo][val_lo]
 * The high 16 bits of the pointer go at offset +2, low 16 bits at +6.
 *
 * Uses inline asm to guarantee word-level (move.w) writes.
 * GCC's volatile uint16_t store decomposes into byte-level move.b
 * pairs, which is incorrect for copper list entries that may be
 * read by DMA mid-update.
 * ================================================================ */
static void copper_write_ptr(uint8_t *entry, uint32_t ptr)
{
    uint16_t hi = (uint16_t)(ptr >> 16);
    uint16_t lo = (uint16_t)(ptr);
    __asm__ volatile ("move.w %0,2(%1)" : : "d"(hi), "a"(entry) : "memory");
    __asm__ volatile ("move.w %0,6(%1)" : : "d"(lo), "a"(entry) : "memory");
}

/* Helper: write a 16-bit word to a copper list buffer address.
 * Forces a move.w instruction — GCC's volatile uint16_t store
 * decomposes into byte-level move.b pairs. */
static void copper_write_word(uint8_t *addr, uint16_t val)
{
    __asm__ volatile ("move.w %0,(%1)" : : "d"(val), "a"(addr) : "memory");
}

/* ================================================================
 * InitDisplayAndCopper  (SWOS2.s line 5416)
 *
 * Sets up display: frontbuffer pointers, copper list, bitplanes,
 * DMA, blitter idle wait.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InitDisplayAndCopper(void)
{
    int16_t i;

    /* Set frontbuffer base pointers */
    frontbufferBasePtr = (uint32_t)frontbufferBase;
    frontbufferBaseAltPtr = (uint32_t)frontbufferBaseAlt;

    /* Initialize sprite blit queue sentinels */
    kSpriteBlitQueueSentinel_A = 0xFFFFFFFFu;
    kSpriteBlitQueueSentinel_B = 0xFFFFFFFFu;

    /* Copy copper list template → buffer (320 bytes) */
    {
        const uint8_t *src = copperListTemplate;
        uint8_t *dst = copperListBuffer;
        for (i = COPPERLIST_SIZE_BYTES - 1; i >= 0; i--)
        {
            *dst++ = *src++;
        }
    }

    /* Clear copperZeroLong */
    copperZeroLong = 0;

    /* Disable interrupts during vblank sync */
    *(volatile uint16_t *)INTENA_ADDR = 0x4000u;

    /* Wait for beam position <= line 10 (top of frame) */
    {
        uint32_t vpos;
        do
        {
            vpos = *(volatile uint32_t *)VPOSR_ADDR;
            vpos = (vpos & 0x1FFFFu) >> 8;
        } while (vpos > 0x0Au);
    }

    /* Re-enable interrupts */
    *(volatile uint16_t *)INTENA_ADDR = 0xC000u;

    /* Write copperZeroLong pointer into first 8 copper list entries */
    {
        uint8_t *entry = copperListBuffer + 4;
        uint32_t ptr = (uint32_t)&copperZeroLong;
        for (i = 7; i >= 0; i--)
        {
            copper_write_ptr(entry, ptr);
            entry += 8;
        }
    }

    /* Clear palette table A (16 entries at copperListBuffer+0x72, stride 4) */
    {
        uint8_t *p = copperListBuffer + 0x72;
        for (i = 15; i >= 0; i--)
        {
            copper_write_word(p, 0);
            p += 4; /* skip 4 bytes */
        }
    }

    /* Clear palette table B (16 entries at copperListBuffer+0xB2, stride 4) */
    {
        uint8_t *p = copperListBuffer + 0xB2;
        for (i = 15; i >= 0; i--)
        {
            copper_write_word(p, 0);
            p += 4; /* skip 4 bytes */
        }
    }

    /* Set bitplane base pointers */
    bitPlane0BasePtr = (uint32_t)bitPlane0Base;
    bitPlane1BasePtr = (uint32_t)bitPlane1Base;
    bitPlane2BasePtr = (uint32_t)bitPlane2Base;
    bitPlane3BasePtr = (uint32_t)bitPlane3Base;

    /* Clear first long of each bitplane */
    *(volatile uint32_t *)bitPlane0Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane1Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane2Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane3Base = copperZeroLong;

    /* Update plane pointers and blit parameters in copper list */
    _UpdatePlanePtrsAndBlitParams();

    /* Install copper list */
    *(volatile uint32_t *)COP1LC_ADDR = (uint32_t)copperListBuffer;

    /* Strobe copper to start — original asm does: move.w (COPJMP1).l,d0
     * This is a pure read strobe; any access triggers copper PC reload. */
    {
        volatile uint16_t dummy = *(volatile uint16_t *)COPJMP1_ADDR;
        (void)dummy;
    }

    /* Enable blitter DMA, wait for idle, disable */
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT))
    {
        /* wait for blitter idle */
    }
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;

    /* Enable copper + bitplane DMA */
    *(volatile uint16_t *)DMACON_ADDR = 0x8280u;
    /* Enable sprite DMA */
    *(volatile uint16_t *)DMACON_ADDR = 0x8220u;

    /* Re-strobe copper after DMA is fully enabled.
     * Some emulators (FS-UAE) may ignore the pre-DMA strobe,
     * so re-trigger to ensure the copper starts from COP1LC. */
    {
        volatile uint16_t dummy = *(volatile uint16_t *)COPJMP1_ADDR;
        (void)dummy;
    }
}

/* ================================================================
 * UpdatePlanePtrsAndBlitParams  (SWOS2.s line 5488)
 *
 * Writes bitplane pointers into the copper list, handles scroll
 * alignment, builds plane pointer tables for double-buffered display.
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdatePlanePtrsAndBlitParams(void)
{
    uint32_t d0;
    uint16_t d1;
    int16_t i;

    /* Write the 4 bitplane base pointers into copper list header (offsets 4, 0xC, 0x14, 0x1C) */
    copper_write_ptr(copperListBuffer + 0x04, bitPlane0BasePtr);
    copper_write_ptr(copperListBuffer + 0x0C, bitPlane1BasePtr);
    copper_write_ptr(copperListBuffer + 0x14, bitPlane2BasePtr);
    copper_write_ptr(copperListBuffer + 0x1C, bitPlane3BasePtr);

    /* Handle horizontal scroll alignment */
    d1 = cameraXMod320 & 0x0Fu;
    if (d1 == 0)
    {
        /* Aligned case */
        copper_write_word(copperListBuffer + 0x5A, PITCH_DEST_MOD_ALIGNED);
        copper_write_word(copperListBuffer + 0x5E, PITCH_DEST_MOD_ALIGNED);
        copper_write_word(copperListBuffer + 0x62, 0x0030u);
        copper_write_word(copperListBuffer + 0x4A, 0);
    }
    else
    {
        /* Unaligned case */
        uint16_t shift = 0x10u - d1;
        copper_write_word(copperListBuffer + 0x5A, PITCH_DEST_MOD_UNALIGNED);
        copper_write_word(copperListBuffer + 0x5E, PITCH_DEST_MOD_UNALIGNED);
        copper_write_word(copperListBuffer + 0x62, 0x0028u);
        copper_write_word(copperListBuffer + 0x4A, (uint16_t)(shift | (shift << 4)));
    }

    /* Build plane pointer tables at copper offsets 0xF4 and 0x11C */
    {
        uint8_t *tableA = copperListBuffer + 0xF4;
        uint8_t *tableB = copperListBuffer + 0x11C;
        uint32_t baseOffset;
        uint32_t altOffset;

        /* Compute Y offset */
        baseOffset = (uint32_t)cameraYMod320 * ROW_STRIDE_BYTES;

        /* Select base address */
        if (usePlaneOffsetFlag != 0)
        {
            baseOffset += (uint32_t)frontbufferMaskBuffer;
        }
        else if (useAltBaseFlag != 0)
        {
            baseOffset += frontbufferBaseAltPtr;
        }
        else
        {
            baseOffset += frontbufferBasePtr;
        }

        /* Add X byte offset */
        d1 = cameraXMod320;
        d1 >>= 3;
        d1 &= 0xFFFEu; /* word-align */
        baseOffset += d1;

        /* Compute alt offset for second table */
        if (usePlaneOffsetFlag != 0)
        {
            altOffset = (uint32_t)d1 + (uint32_t)frontbufferMaskBuffer;
        }
        else
        {
            altOffset = (uint32_t)d1 + frontbufferBasePtr;
        }

        /* Write 4 plane pointer pairs */
        for (i = NUM_PLANES_MINUS1; i >= 0; i--)
        {
            copper_write_ptr(tableA, baseOffset);
            copper_write_ptr(tableB, altOffset);
            baseOffset += PLANE_STRIDE;
            altOffset += PLANE_STRIDE;
            tableA += 8;
            tableB += 8;
        }
    }

    /* Compute and write blit line count */
    {
        uint16_t lineCount = SCREEN_HEIGHT - cameraYMod320 + 0x24u;
        if (lineCount > 0xFFu)
        {
            copper_write_word(copperListBuffer + 0x114, 0xFFE1u);
        }
        else
        {
            copper_write_word(copperListBuffer + 0x114, 1);
        }
        lineCount &= 0xFFu;
        copper_write_word(copperListBuffer + 0x118, (uint16_t)((lineCount << 8) | 1u));
    }
}

/* ================================================================
 * SwapBuffersAndCameraViewState  (SWOS2.s line 5613)
 *
 * Swaps between primary and alternate frontbuffers + bitplane sets.
 * Saves current camera state and restores the other frame's state.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SwapBuffersAndCameraViewState(void)
{
    if (frontbufferBasePtr == (uint32_t)frontbufferBase)
    {
        /* Currently using primary → swap to alt */
        bitPlane0BasePtr = (uint32_t)bitPlane0BaseAlt;
        bitPlane1BasePtr = (uint32_t)bitPlane1BaseAlt;
        bitPlane2BasePtr = (uint32_t)bitPlane2BaseAlt;
        bitPlane3BasePtr = (uint32_t)bitPlane3BaseAlt;

        *(volatile uint32_t *)bitPlane0BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane1BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane2BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane3BaseAlt = copperZeroLong;

        frontbufferBasePtr = (uint32_t)frontbufferBaseAlt;
        frontbufferBaseAltPtr = (uint32_t)frontbufferBase;
        spriteBlitQueue = (uint32_t)&kSpriteBlitQueueSentinel_B;

        /* Save current camera state to slot 0 */
        dstXByteOffsetSaved0 = cameraXMod320;
        dstYStartLinesSaved0 = cameraYMod320;
        oldCameraXSaved0 = oldCameraX;
        oldCameraYSaved0 = oldCameraY;
        cameraXSaved0 = cameraX;
        cameraYSaved0 = cameraY;

        /* Restore camera state from slot 1 */
        cameraXMod320 = dstXByteOffsetSaved1;
        cameraYMod320 = dstYStartLinesSaved1;
        oldCameraX = oldCameraXSaved1;
        oldCameraY = oldCameraYSaved1;
        cameraX = cameraXSaved1;
        cameraY = cameraYSaved1;
    }
    else
    {
        /* Currently using alt → swap to primary */
        bitPlane0BasePtr = (uint32_t)bitPlane0Base;
        bitPlane1BasePtr = (uint32_t)bitPlane1Base;
        bitPlane2BasePtr = (uint32_t)bitPlane2Base;
        bitPlane3BasePtr = (uint32_t)bitPlane3Base;

        *(volatile uint32_t *)bitPlane0Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane1Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane2Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane3Base = copperZeroLong;

        frontbufferBasePtr = (uint32_t)frontbufferBase;
        frontbufferBaseAltPtr = (uint32_t)frontbufferBaseAlt;
        spriteBlitQueue = (uint32_t)&kSpriteBlitQueueSentinel_A;

        /* Save current camera state to slot 1 */
        dstXByteOffsetSaved1 = cameraXMod320;
        dstYStartLinesSaved1 = cameraYMod320;
        oldCameraXSaved1 = oldCameraX;
        oldCameraYSaved1 = oldCameraY;
        cameraXSaved1 = cameraX;
        cameraYSaved1 = cameraY;

        /* Restore camera state from slot 0 */
        cameraXMod320 = dstXByteOffsetSaved0;
        cameraYMod320 = dstYStartLinesSaved0;
        oldCameraX = oldCameraXSaved0;
        oldCameraY = oldCameraYSaved0;
        cameraX = cameraXSaved0;
        cameraY = cameraYSaved0;
    }
}

/* ================================================================
 * SetCameraCoordinates  (SWOS2.s line 6158)
 *
 * Iteratively moves cameraX/cameraXMod320 toward targetX and
 * cameraY/cameraYMod320 toward targetY, one pixel per iteration,
 * with wrapping at 352/272.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetCameraCoordinates(int16_t targetX, int16_t targetY)
{
    /* Move X toward target */
    while (cameraX != targetX)
    {
        if (cameraX < targetX)
        {
            cameraX += 1;
            cameraXMod320 += 1;
            if (cameraXMod320 == 352)
                cameraXMod320 = 0;
        }
        else
        {
            cameraX -= 1;
            cameraXMod320 -= 1;
            if ((int16_t)cameraXMod320 < 0)
                cameraXMod320 = 351;
        }
    }

    /* Move Y toward target */
    while (cameraY != targetY)
    {
        if (cameraY < targetY)
        {
            cameraY += 1;
            cameraYMod320 += 1;
            if (cameraYMod320 == 272)
                cameraYMod320 = 0;
        }
        else
        {
            cameraY -= 1;
            cameraYMod320 -= 1;
            if ((int16_t)cameraYMod320 < 0)
                cameraYMod320 = 271;
        }
    }
}

/* ================================================================
 * DoubleBufferSwapSequence  (SWOS2.s line 43156)
 *
 * Resets palette, zeroes camera position, swaps buffers twice
 * (updating copper plane pointers each time).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DoubleBufferSwapSequence(void)
{
    paletteIndex = 0;
    _SetCameraCoordinates(0, 0);
    _SwapBuffersAndCameraViewState();
    _UpdatePlanePtrsAndBlitParams();
    _SwapBuffersAndCameraViewState();
    _UpdatePlanePtrsAndBlitParams();
}

/* ================================================================
 * BlitterCopyPanelToScreen2  (SWOS2.s line 43526)
 *
 * Copies frontbufferMaskBuffer to frontbuffer via blitter A→D copy.
 * ================================================================ */
__attribute__((used, externally_visible))
void _BlitterCopyPanelToScreen2(void)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;

    /* Enable blitter DMA and wait for idle */
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT))
    {
        /* wait */
    }
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;

    /* Configure blitter for A→D copy */
    *(volatile uint16_t *)(custom + BLTALWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTCON0_OFF) = (uint16_t)BLTCON0_USEA_USED_LF_F0;
    *(volatile uint16_t *)(custom + BLTAMOD_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTDMOD_OFF) = 0;

    /* Set source and destination pointers */
    *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)frontbufferMaskBuffer;
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = frontbufferBasePtr;

    /* Kick the blit (writing BLTSIZE starts it) */
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = (uint16_t)BLTSIZE_OVERLAY_PANEL;

    /* Wait for blit to complete, then disable blitter DMA */
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT))
    {
        /* wait */
    }
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;

    /* Reset blit size to small default */
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = 0x0016u;
}

/* ================================================================
 * ProcessSpriteBlitQueue  (SWOS2.s line 5522)
 *
 * Drains the sprite blit queue (a linked list of A→D blitter
 * copy entries). Each entry is 12 bytes:
 *   +0: BLTAPT (source ptr, -1 = sentinel/end)
 *   +4: BLTDPT (dest ptr)
 *   +8: BLTAMOD/BLTDMOD
 *  +10: BLTSIZE (kicks blit)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ProcessSpriteBlitQueue(void)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    volatile uint32_t *queue;

    /* Enable blitter and wait idle */
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT)) {}
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;

    /* Set blitter config: A→D copy, full masks, no shift */
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTALWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTCON0_OFF) = (uint16_t)BLTCON0_USEA_USED_LF_F0;

    queue = (volatile uint32_t *)spriteBlitQueue;

    for (;;)
    {
        uint32_t srcPtr;
        uint16_t *entry16;

        /* Wait for blitter idle */
        *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
        (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
        while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT)) {}
        *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;

        srcPtr = queue[0];
        if ((int32_t)srcPtr < 0)
            break; /* sentinel: end of queue */

        *(volatile uint32_t *)(custom + BLTAPT_OFF) = srcPtr;
        *(volatile uint32_t *)(custom + BLTDPT_OFF) = queue[1];
        entry16 = (uint16_t *)queue;
        *(volatile uint16_t *)(custom + BLTAMOD_OFF) = entry16[4]; /* +8 */
        *(volatile uint16_t *)(custom + BLTDMOD_OFF) = entry16[4]; /* same mod for A and D */
        *(volatile uint16_t *)(custom + BLTSIZE_OFF) = entry16[5]; /* +10, kicks blit */

        queue = (volatile uint32_t *)((uint8_t *)queue + 12);
    }

    /* Reset queue: write sentinel at start */
    {
        uint32_t *queueBase = (uint32_t *)spriteBlitQueue;
        *queueBase = 0xFFFFFFFFu;
    }
}

/* ================================================================
 * BlitterCopyPanelToScreen  (SWOS2.s line 6591)
 *
 * Copies frontbufferMaskBuffer to frontbuffer (panel/overlay blit).
 * Same as BlitterCopyPanelToScreen2 but uses CUSTOM base register
 * style (identical logic).
 * ================================================================ */
__attribute__((used, externally_visible))
void _BlitterCopyPanelToScreen(void)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;

    /* Wait for blitter idle */
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT)) {}
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;

    /* Configure: A→D copy, full masks */
    *(volatile uint16_t *)(custom + BLTALWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTCON0_OFF) = (uint16_t)BLTCON0_USEA_USED_LF_F0;
    *(volatile uint16_t *)(custom + BLTAMOD_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTDMOD_OFF) = 0;

    /* Source = frontbufferMaskBuffer, Dest = frontbuffer */
    *(volatile uint32_t *)(custom + BLTAPT_OFF) = (uint32_t)frontbufferMaskBuffer;
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = frontbufferBasePtr;
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = (uint16_t)BLTSIZE_OVERLAY_PANEL;

    /* Wait, then kick small trailing blit */
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT)) {}
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = 0x0016u;
}

/* ================================================================
 * HWSprite_LoadFromSpriteGraphics  (SWOS2.s line 6619)
 *
 * Clears a hardware sprite using the blitter. Looks up sprite data
 * from spriteGraphicsPtr table, then does a D-only fill (ADAT=0).
 *
 * Sprite entry layout: +0 = pointer to sprite data, +12 = height,
 * +14 = stride (TILE_STRIDE field, used as width in words).
 * ================================================================ */
__attribute__((used, externally_visible))
void _HWSprite_LoadFromSpriteGraphics(uint16_t spriteIndex)
{
    volatile uint8_t *custom = (volatile uint8_t *)CUSTOM_BASE;
    uint32_t *gfxTable = (uint32_t *)spriteGraphicsPtr;
    uint8_t *sprEntry;
    uint32_t destPtr;
    uint16_t height;
    uint16_t stride;
    uint16_t blitSize;

    sprEntry = (uint8_t *)gfxTable[spriteIndex];

    /* Wait for blitter idle */
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_ON;
    (void)(*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT));
    while (*(volatile uint8_t *)DMACONR_ADDR & (1u << DMACONR_BLTBUSY_BIT)) {}
    *(volatile uint16_t *)DMACON_ADDR = (uint16_t)DMACON_BLT_OFF;

    /* Configure: D-only fill (LF=F0 with no source channels) */
    *(volatile uint16_t *)(custom + BLTALWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTAFWM_OFF) = BLT_FWM_ALL;
    *(volatile uint16_t *)(custom + BLTCON1_OFF) = 0;
    *(volatile uint16_t *)(custom + BLTCON0_OFF) = (uint16_t)BLTCON0_USED_LF_F0;
    *(volatile uint16_t *)(custom + BLTADAT_OFF) = 0; /* fill with zeros */
    *(volatile uint16_t *)(custom + BLTDMOD_OFF) = 0;

    /* Dest = sprite data pointer (first long of sprite entry) */
    destPtr = *(uint32_t *)sprEntry;
    *(volatile uint32_t *)(custom + BLTDPT_OFF) = destPtr;

    /* BLTSIZE = (height << 8) | stride */
    height = *(uint16_t *)(sprEntry + 12);
    stride = *(uint16_t *)(sprEntry + 14); /* TILE_STRIDE offset */
    blitSize = (uint16_t)((height << 8) | stride);
    *(volatile uint16_t *)(custom + BLTSIZE_OFF) = blitSize;
}

/* ================================================================
 * FlashColor0Delay  (SWOS2.s line 5490)
 *
 * Debug/diagnostic: sets COLOR00 to a given color, loops for
 * loopCount iterations, then restores COLOR00 from copper list.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FlashColor0Delay(uint32_t loopCount, uint16_t color)
{
    volatile uint16_t *color00 = (volatile uint16_t *)COLOR00_ADDR;
    volatile uint16_t *copperColor0 = (volatile uint16_t *)(copperListBuffer + 0x72);

    *color00 = color;
    while (--loopCount != 0)
    {
        *color00 = color; /* keep writing to prevent optimizer from removing */
    }
    *color00 = *copperColor0;
}
