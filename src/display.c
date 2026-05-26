/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "display.h"
#include "game_types.h"
#include "swos_types.h"
#include <string.h>

/* ---- Blitter register write macros ----
 * GCC m68k 15.1.0 decomposes `volatile uint16_t = N` struct-member
 * stores into byte-pair `move.b` instructions in some contexts. For
 * hardware-trigger registers (writing `bltsize` starts the blitter),
 * the decomposed write triggers the blit mid-update with a half-formed
 * size — fill/copy operations either do nothing or write garbage.
 * Same macros as in blitter.c / sprites.c / scroll_blit.c.  */
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
#define BLT_W(reg, val)                                                     \
    do {                                                                    \
        uint16_t _v = (uint16_t)(val);                                      \
        volatile uint16_t *_p = &(reg);                                     \
        __asm__ volatile ("move.w %0,(%1)" : : "d"(_v), "a"(_p) : "memory");\
    } while (0)
#define BLT_L(reg, val)                                                     \
    do {                                                                    \
        uint32_t _v = (uint32_t)(val);                                      \
        volatile uint32_t *_p = &(reg);                                     \
        __asm__ volatile ("move.l %0,(%1)" : : "d"(_v), "a"(_p) : "memory");\
    } while (0)
#else
#define BLT_W(reg, val) ((void)((reg) = (uint16_t)(val)))
#define BLT_L(reg, val) ((void)((reg) = (uint32_t)(val)))
#endif

/* Custom chip base pointer — set once, used by all HW register access */
volatile struct Custom *custom = (volatile struct Custom *)0xDFF000;

/* ---- External globals (all live in SWOS2.s) ---- */

/* Frontbuffer pointers */
extern uint8_t frontbufferBase[];
extern uint8_t frontbufferBaseAlt[];
extern uint8_t frontbufferMaskBuffer[];
extern volatile uintptr_t frontbufferBasePtr;
extern volatile uintptr_t frontbufferBaseAltPtr;

/* Bitplane bases (BSSInChip, 0x384 bytes each) */
extern uint8_t bitPlane0Base[];
extern uint8_t bitPlane1Base[];
extern uint8_t bitPlane2Base[];
extern uint8_t bitPlane3Base[];
#ifdef AGA
extern uint8_t bitPlane4Base[];
extern uint8_t bitPlane5Base[];
extern uint8_t bitPlane6Base[];
extern uint8_t bitPlane7Base[];
#endif
extern uint8_t bitPlane0BaseAlt[];
extern uint8_t bitPlane1BaseAlt[];
extern uint8_t bitPlane2BaseAlt[];
extern uint8_t bitPlane3BaseAlt[];
#ifdef AGA
extern uint8_t bitPlane4BaseAlt[];
extern uint8_t bitPlane5BaseAlt[];
extern uint8_t bitPlane6BaseAlt[];
extern uint8_t bitPlane7BaseAlt[];
#endif

/* Bitplane base pointers */
extern volatile uint32_t bitPlane0BasePtr;
extern volatile uint32_t bitPlane1BasePtr;
extern volatile uint32_t bitPlane2BasePtr;
extern volatile uint32_t bitPlane3BasePtr;
#ifdef AGA
extern volatile uint32_t bitPlane4BasePtr;
extern volatile uint32_t bitPlane5BasePtr;
extern volatile uint32_t bitPlane6BasePtr;
extern volatile uint32_t bitPlane7BasePtr;
#endif

/* Copper list */
extern uint8_t copperListBuffer[];
extern uint32_t copperZeroLong;

/* Sprite blit queue sentinels (4 bytes each: 2-byte sentinel + padding/queue) */
extern uint32_t kSpriteBlitQueueSentinel_A;
extern uint32_t kSpriteBlitQueueSentinel_B;
extern volatile uintptr_t spriteBlitQueue;

/* Camera state */
extern FixedPoint cameraX;          /* dc.l but word-accessed (big-endian high word) */
extern FixedPoint cameraY;          /* dc.l but word-accessed (big-endian high word) */
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
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
    /* Amiga: force real move.w instructions — GCC's volatile uint16_t store
     * can decompose into byte-level move.b pairs which corrupt the live
     * copper list. */
    __asm__ volatile ("move.w %0,2(%1)" : : "d"(hi), "a"(entry) : "memory");
    __asm__ volatile ("move.w %0,6(%1)" : : "d"(lo), "a"(entry) : "memory");
#else
    /* SDL host: copper list is just a buffer in main RAM, plain stores fine. */
    *(volatile uint16_t *)(entry + 2) = hi;
    *(volatile uint16_t *)(entry + 6) = lo;
#endif
}

/* Helper: write a 16-bit word to a copper list buffer address.
 * Forces a move.w instruction on Amiga; plain store on SDL host. */
static void copper_write_word(uint8_t *addr, uint16_t val)
{
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
    __asm__ volatile ("move.w %0,(%1)" : : "d"(val), "a"(addr) : "memory");
#else
    *(volatile uint16_t *)addr = val;
#endif
}

/* ================================================================
 * Copper list template — const data, copied to copperListBuffer at init.
 * OCS: 160 words (320 bytes), AGA: 658 words (1316 bytes).
 * ================================================================ */
static const uint16_t copperListTemplate[COPPERLIST_SIZE_BYTES / 2] = {
#ifdef AGA
    /* ===== AGA copper list template (8 planes, 256 colors) ===== */
    0x1401, 0xFFFE,                     /* Wait vpos >= 0x14 */
    /* 8 sprite pointer pairs */
    0x0120, 0x0000, 0x0122, 0x0000,     /* SPR0 */
    0x0124, 0x0000, 0x0126, 0x0000,     /* SPR1 */
    0x0128, 0x0000, 0x012A, 0x0000,     /* SPR2 */
    0x012C, 0x0000, 0x012E, 0x0000,     /* SPR3 */
    0x0130, 0x0000, 0x0132, 0x0000,     /* SPR4 */
    0x0134, 0x0000, 0x0136, 0x0000,     /* SPR5 */
    0x0138, 0x0000, 0x013A, 0x0000,     /* SPR6 */
    0x013C, 0x0000, 0x013E, 0x0000,     /* SPR7 */
    0x0100, 0x0210,                     /* BPLCON0: 8 bitplanes (BPU3=1) + COLOR */
    0x0102, 0x0000,                     /* BPLCON1 */
    0x0104, 0x0024,                     /* BPLCON2 */
    0x01FE, 0x0000,                     /* NO-OP (FMODE placeholder) */
    0x01FE, 0x0000,                     /* NO-OP */
    0x0108, 0x0000,                     /* BPL1MOD */
    0x010A, 0x0000,                     /* BPL2MOD */
    0x0092, 0x0028,                     /* DDFSTRT */
    0x0094, 0x00D0,                     /* DDFSTOP */
    0x008E, 0x2471,                     /* DIWSTRT */
    0x0090, 0x34C1,                     /* DIWSTOP */
    /* Bank 0 (colors 0..31) */
    0x0106, 0x0C00,                     /* BPLCON3: bank 0 */
    0x0180, 0, 0x0182, 0, 0x0184, 0, 0x0186, 0,
    0x0188, 0, 0x018A, 0, 0x018C, 0, 0x018E, 0,
    0x0190, 0, 0x0192, 0, 0x0194, 0, 0x0196, 0,
    0x0198, 0, 0x019A, 0, 0x019C, 0, 0x019E, 0,
    0x01A0, 0, 0x01A2, 0, 0x01A4, 0, 0x01A6, 0,
    0x01A8, 0, 0x01AA, 0, 0x01AC, 0, 0x01AE, 0,
    0x01B0, 0, 0x01B2, 0, 0x01B4, 0, 0x01B6, 0,
    0x01B8, 0, 0x01BA, 0, 0x01BC, 0, 0x01BE, 0,
    /* Bank 1 (colors 32..63) */
    0x0106, 0x2C00,
    0x0180, 0, 0x0182, 0, 0x0184, 0, 0x0186, 0,
    0x0188, 0, 0x018A, 0, 0x018C, 0, 0x018E, 0,
    0x0190, 0, 0x0192, 0, 0x0194, 0, 0x0196, 0,
    0x0198, 0, 0x019A, 0, 0x019C, 0, 0x019E, 0,
    0x01A0, 0, 0x01A2, 0, 0x01A4, 0, 0x01A6, 0,
    0x01A8, 0, 0x01AA, 0, 0x01AC, 0, 0x01AE, 0,
    0x01B0, 0, 0x01B2, 0, 0x01B4, 0, 0x01B6, 0,
    0x01B8, 0, 0x01BA, 0, 0x01BC, 0, 0x01BE, 0,
    /* Bank 2 (colors 64..95) */
    0x0106, 0x4C00,
    0x0180, 0, 0x0182, 0, 0x0184, 0, 0x0186, 0,
    0x0188, 0, 0x018A, 0, 0x018C, 0, 0x018E, 0,
    0x0190, 0, 0x0192, 0, 0x0194, 0, 0x0196, 0,
    0x0198, 0, 0x019A, 0, 0x019C, 0, 0x019E, 0,
    0x01A0, 0, 0x01A2, 0, 0x01A4, 0, 0x01A6, 0,
    0x01A8, 0, 0x01AA, 0, 0x01AC, 0, 0x01AE, 0,
    0x01B0, 0, 0x01B2, 0, 0x01B4, 0, 0x01B6, 0,
    0x01B8, 0, 0x01BA, 0, 0x01BC, 0, 0x01BE, 0,
    /* Bank 3 (colors 96..127) */
    0x0106, 0x6C00,
    0x0180, 0, 0x0182, 0, 0x0184, 0, 0x0186, 0,
    0x0188, 0, 0x018A, 0, 0x018C, 0, 0x018E, 0,
    0x0190, 0, 0x0192, 0, 0x0194, 0, 0x0196, 0,
    0x0198, 0, 0x019A, 0, 0x019C, 0, 0x019E, 0,
    0x01A0, 0, 0x01A2, 0, 0x01A4, 0, 0x01A6, 0,
    0x01A8, 0, 0x01AA, 0, 0x01AC, 0, 0x01AE, 0,
    0x01B0, 0, 0x01B2, 0, 0x01B4, 0, 0x01B6, 0,
    0x01B8, 0, 0x01BA, 0, 0x01BC, 0, 0x01BE, 0,
    /* Bank 4 (colors 128..159) */
    0x0106, 0x8C00,
    0x0180, 0, 0x0182, 0, 0x0184, 0, 0x0186, 0,
    0x0188, 0, 0x018A, 0, 0x018C, 0, 0x018E, 0,
    0x0190, 0, 0x0192, 0, 0x0194, 0, 0x0196, 0,
    0x0198, 0, 0x019A, 0, 0x019C, 0, 0x019E, 0,
    0x01A0, 0, 0x01A2, 0, 0x01A4, 0, 0x01A6, 0,
    0x01A8, 0, 0x01AA, 0, 0x01AC, 0, 0x01AE, 0,
    0x01B0, 0, 0x01B2, 0, 0x01B4, 0, 0x01B6, 0,
    0x01B8, 0, 0x01BA, 0, 0x01BC, 0, 0x01BE, 0,
    /* Bank 5 (colors 160..191) */
    0x0106, 0xAC00,
    0x0180, 0, 0x0182, 0, 0x0184, 0, 0x0186, 0,
    0x0188, 0, 0x018A, 0, 0x018C, 0, 0x018E, 0,
    0x0190, 0, 0x0192, 0, 0x0194, 0, 0x0196, 0,
    0x0198, 0, 0x019A, 0, 0x019C, 0, 0x019E, 0,
    0x01A0, 0, 0x01A2, 0, 0x01A4, 0, 0x01A6, 0,
    0x01A8, 0, 0x01AA, 0, 0x01AC, 0, 0x01AE, 0,
    0x01B0, 0, 0x01B2, 0, 0x01B4, 0, 0x01B6, 0,
    0x01B8, 0, 0x01BA, 0, 0x01BC, 0, 0x01BE, 0,
    /* Bank 6 (colors 192..223) */
    0x0106, 0xCC00,
    0x0180, 0, 0x0182, 0, 0x0184, 0, 0x0186, 0,
    0x0188, 0, 0x018A, 0, 0x018C, 0, 0x018E, 0,
    0x0190, 0, 0x0192, 0, 0x0194, 0, 0x0196, 0,
    0x0198, 0, 0x019A, 0, 0x019C, 0, 0x019E, 0,
    0x01A0, 0, 0x01A2, 0, 0x01A4, 0, 0x01A6, 0,
    0x01A8, 0, 0x01AA, 0, 0x01AC, 0, 0x01AE, 0,
    0x01B0, 0, 0x01B2, 0, 0x01B4, 0, 0x01B6, 0,
    0x01B8, 0, 0x01BA, 0, 0x01BC, 0, 0x01BE, 0,
    /* Bank 7 (colors 224..255) */
    0x0106, 0xEC00,
    0x0180, 0, 0x0182, 0, 0x0184, 0, 0x0186, 0,
    0x0188, 0, 0x018A, 0, 0x018C, 0, 0x018E, 0,
    0x0190, 0, 0x0192, 0, 0x0194, 0, 0x0196, 0,
    0x0198, 0, 0x019A, 0, 0x019C, 0, 0x019E, 0,
    0x01A0, 0, 0x01A2, 0, 0x01A4, 0, 0x01A6, 0,
    0x01A8, 0, 0x01AA, 0, 0x01AC, 0, 0x01AE, 0,
    0x01B0, 0, 0x01B2, 0, 0x01B4, 0, 0x01B6, 0,
    0x01B8, 0, 0x01BA, 0, 0x01BC, 0, 0x01BE, 0,
    /* Reset to bank 0 */
    0x0106, 0x0C00,
    0x2301, 0xFFFE,                     /* Wait vpos >= 0x23 */
    /* BPL pointers block A (8 planes) */
    0x00E0, 0, 0x00E2, 0, 0x00E4, 0, 0x00E6, 0,
    0x00E8, 0, 0x00EA, 0, 0x00EC, 0, 0x00EE, 0,
    0x00F0, 0, 0x00F2, 0, 0x00F4, 0, 0x00F6, 0,
    0x00F8, 0, 0x00FA, 0, 0x00FC, 0, 0x00FE, 0,
    /* Wait / BPL pointers block B */
    0xFFE1, 0xFFFE,                     /* Wait vhpos */
    0x0001, 0xFFFE,                     /* Wait */
    /* BPL pointers block B (8 planes) */
    0x00E0, 0, 0x00E2, 0, 0x00E4, 0, 0x00E6, 0,
    0x00E8, 0, 0x00EA, 0, 0x00EC, 0, 0x00EE, 0,
    0x00F0, 0, 0x00F2, 0, 0x00F4, 0, 0x00F6, 0,
    0x00F8, 0, 0x00FA, 0, 0x00FC, 0, 0x00FE, 0,
    0xFFFF, 0xFFFE                      /* End of copper list */
#else
    /* ===== OCS copper list template (4 planes, 16 colors) ===== */
    0x1401, 0xFFFE, 0x0120, 0x0000, 0x0122, 0x0000, 0x0124, 0x0000,
    0x0126, 0x0000, 0x0128, 0x0000, 0x012A, 0x0000, 0x012C, 0x0000,
    0x012E, 0x0000, 0x0130, 0x0000, 0x0132, 0x0000, 0x0134, 0x0000,
    0x0136, 0x0000, 0x0138, 0x0000, 0x013A, 0x0000, 0x013C, 0x0000,
    0x013E, 0x0000, 0x0100, 0x4200, 0x0102, 0x0000, 0x0104, 0x0024,
    0x01FE, 0x0000, 0x01FE, 0x0000, 0x0108, 0x0000, 0x010A, 0x0000,
    0x0092, 0x0028, 0x0094, 0x00D0, 0x008E, 0x2471, 0x0090, 0x34C1,
    0x0180, 0x0000, 0x0182, 0x0000, 0x0184, 0x0000, 0x0186, 0x0000,
    0x0188, 0x0000, 0x018A, 0x0000, 0x018C, 0x0000, 0x018E, 0x0000,
    0x0190, 0x0000, 0x0192, 0x0000, 0x0194, 0x0000, 0x0196, 0x0000,
    0x0198, 0x0000, 0x019A, 0x0000, 0x019C, 0x0000, 0x019E, 0x0000,
    0x01A0, 0x0000, 0x01A2, 0x0000, 0x01A4, 0x0000, 0x01A6, 0x0000,
    0x01A8, 0x0000, 0x01AA, 0x0000, 0x01AC, 0x0000, 0x01AE, 0x0000,
    0x01B0, 0x0000, 0x01B2, 0x0000, 0x01B4, 0x0000, 0x01B6, 0x0000,
    0x01B8, 0x0000, 0x01BA, 0x0000, 0x01BC, 0x0000, 0x01BE, 0x0000,
    0x2401, 0xFFFE, 0x00E0, 0x0000, 0x00E2, 0x0000, 0x00E4, 0x0000,
    0x00E6, 0x0000, 0x00E8, 0x0000, 0x00EA, 0x0000, 0x00EC, 0x0000,
    0x00EE, 0x0000, 0xFFE1, 0xFFFE, 0x0001, 0xFFFE, 0x00E0, 0x0000,
    0x00E2, 0x0000, 0x00E4, 0x0000, 0x00E6, 0x0000, 0x00E8, 0x0000,
    0x00EA, 0x0000, 0x00EC, 0x0000, 0x00EE, 0x0000, 0xFFFF, 0xFFFE
#endif
};

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
    frontbufferBasePtr = (uintptr_t)frontbufferBase;
    frontbufferBaseAltPtr = (uintptr_t)frontbufferBaseAlt;

    /* Initialize sprite blit queue sentinels */
    kSpriteBlitQueueSentinel_A = 0xFFFFFFFFu;
    kSpriteBlitQueueSentinel_B = 0xFFFFFFFFu;

    /* Copy copper list template → buffer */
    {
        const uint8_t *src = (const uint8_t *)copperListTemplate;
        uint8_t *dst = copperListBuffer;
        for (i = COPPERLIST_SIZE_BYTES - 1; i >= 0; i--)
        {
            *dst++ = *src++;
        }
    }

    /* Clear copperZeroLong */
    copperZeroLong = 0;

    /* Disable interrupts during vblank sync */
    custom->intena = 0x4000u;

    /* Wait for beam position <= line 10 (top of frame) */
    {
        uint32_t vpos;
        do
        {
            vpos = *(volatile uint32_t *)&custom->vposr;
            vpos = (vpos & 0x1FFFFu) >> 8;
        } while (vpos > 0x0Au);
    }

    /* Re-enable interrupts */
    custom->intena = 0xC000u;

    /* Write copperZeroLong pointer into first 8 sprite pointer entries */
    {
        uint8_t *entry = copperListBuffer + COP_SPR_B0_OFF;
        uint32_t ptr = (uintptr_t)&copperZeroLong;
        for (i = 7; i >= 0; i--)
        {
            copper_write_ptr(entry, ptr);
            entry += 8;
        }
    }

    /* Clear palette banks in copper list */
    {
#ifdef AGA
        /* AGA: clear all 8 banks x 32 colors */
        static const uint16_t bankOffsets[8] = {
            COP_PAL_B0_C0_VAL_OFF, COP_PAL_B1_C0_VAL_OFF,
            COP_PAL_B2_C0_VAL_OFF, COP_PAL_B3_C0_VAL_OFF,
            COP_PAL_B4_C0_VAL_OFF, COP_PAL_B5_C0_VAL_OFF,
            COP_PAL_B6_C0_VAL_OFF, COP_PAL_B7_C0_VAL_OFF
        };
        int16_t bank;
        for (bank = 0; bank < NUM_PAL_BANKS; bank++)
        {
            uint8_t *p = copperListBuffer + bankOffsets[bank];
            for (i = COLORS_PER_BANK - 1; i >= 0; i--)
            {
                copper_write_word(p, 0);
                p += 4;
            }
        }
#else
        /* OCS: clear 2 blocks x 16 colors */
        uint8_t *p;
        p = copperListBuffer + COP_PAL_B0_C0_VAL_OFF;
        for (i = 15; i >= 0; i--)
        {
            copper_write_word(p, 0);
            p += 4;
        }
        p = copperListBuffer + COP_PAL_B1_C0_VAL_OFF;
        for (i = 15; i >= 0; i--)
        {
            copper_write_word(p, 0);
            p += 4;
        }
#endif
    }

    /* Set bitplane base pointers */
    bitPlane0BasePtr = (uintptr_t)bitPlane0Base;
    bitPlane1BasePtr = (uintptr_t)bitPlane1Base;
    bitPlane2BasePtr = (uintptr_t)bitPlane2Base;
    bitPlane3BasePtr = (uintptr_t)bitPlane3Base;
#ifdef AGA
    bitPlane4BasePtr = (uint32_t)bitPlane4Base;
    bitPlane5BasePtr = (uint32_t)bitPlane5Base;
    bitPlane6BasePtr = (uint32_t)bitPlane6Base;
    bitPlane7BasePtr = (uint32_t)bitPlane7Base;
#endif

    /* Clear first long of each bitplane */
    *(volatile uint32_t *)bitPlane0Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane1Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane2Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane3Base = copperZeroLong;
#ifdef AGA
    *(volatile uint32_t *)bitPlane4Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane5Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane6Base = copperZeroLong;
    *(volatile uint32_t *)bitPlane7Base = copperZeroLong;
#endif

    /* Update plane pointers and blit parameters in copper list */
    _UpdatePlanePtrsAndBlitParams();

    /* Install copper list */
    custom->cop1lc = (uintptr_t)copperListBuffer;

    /* Strobe copper to start — original asm does: move.w (COPJMP1).l,d0
     * This is a pure read strobe; any access triggers copper PC reload. */
    {
        volatile uint16_t dummy = custom->copjmp1;
        (void)dummy;
    }

    /* Enable blitter DMA, wait for idle, disable */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14))
    {
        /* wait for blitter idle */
    }
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Enable copper + bitplane DMA */
    custom->dmacon = 0x8280u;
    /* Enable sprite DMA */
    custom->dmacon = 0x8220u;

    /* Re-strobe copper after DMA is fully enabled.
     * Some emulators (FS-UAE) may ignore the pre-DMA strobe,
     * so re-trigger to ensure the copper starts from COP1LC. */
    {
        volatile uint16_t dummy = custom->copjmp1;
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

    /* Write bitplane base pointers into copper list sprite pointer slots */
    copper_write_ptr(copperListBuffer + COP_SPR_B0_OFF, bitPlane0BasePtr);
    copper_write_ptr(copperListBuffer + COP_SPR_B0_OFF + 8, bitPlane1BasePtr);
    copper_write_ptr(copperListBuffer + COP_SPR_B0_OFF + 16, bitPlane2BasePtr);
    copper_write_ptr(copperListBuffer + COP_SPR_B0_OFF + 24, bitPlane3BasePtr);
#ifdef AGA
    copper_write_ptr(copperListBuffer + COP_SPR_B0_OFF + 32, bitPlane4BasePtr);
    copper_write_ptr(copperListBuffer + COP_SPR_B0_OFF + 40, bitPlane5BasePtr);
    copper_write_ptr(copperListBuffer + COP_SPR_B0_OFF + 48, bitPlane6BasePtr);
    copper_write_ptr(copperListBuffer + COP_SPR_B0_OFF + 56, bitPlane7BasePtr);
#endif

    /* Handle horizontal scroll alignment */
    d1 = cameraXMod320 & 0x0Fu;
    if (d1 == 0)
    {
        /* Aligned case */
        copper_write_word(copperListBuffer + COP_BPL1MOD_VAL_OFF, PITCH_DEST_MOD_ALIGNED);
        copper_write_word(copperListBuffer + COP_BPL2MOD_VAL_OFF, PITCH_DEST_MOD_ALIGNED);
        copper_write_word(copperListBuffer + COP_DDFSTRT_VAL_OFF, 0x0030u);
        copper_write_word(copperListBuffer + COP_BPLCON1_VAL_OFF, 0);
    }
    else
    {
        /* Unaligned case */
        uint16_t shift = 0x10u - d1;
        copper_write_word(copperListBuffer + COP_BPL1MOD_VAL_OFF, PITCH_DEST_MOD_UNALIGNED);
        copper_write_word(copperListBuffer + COP_BPL2MOD_VAL_OFF, PITCH_DEST_MOD_UNALIGNED);
        copper_write_word(copperListBuffer + COP_DDFSTRT_VAL_OFF, 0x0028u);
        copper_write_word(copperListBuffer + COP_BPLCON1_VAL_OFF, (uint16_t)(shift | (shift << 4)));
    }

    /* Build plane pointer tables at copper BPL pointer blocks A and B */
    {
        uint8_t *tableA = copperListBuffer + COP_BPLPTR_BLOCKA_OFF;
        uint8_t *tableB = copperListBuffer + COP_BPLPTR_BLOCKB_OFF;
        uint32_t baseOffset;
        uint32_t altOffset;

        /* Compute Y offset */
        baseOffset = (uint32_t)cameraYMod320 * ROW_STRIDE_BYTES;

        /* Select base address */
        if (usePlaneOffsetFlag != 0)
        {
            baseOffset += (uintptr_t)frontbufferMaskBuffer;
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
            altOffset = (uintptr_t)d1 + (uintptr_t)frontbufferMaskBuffer;
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
            copper_write_word(copperListBuffer + COP_WAIT_VHPOS, 0xFFE1u);
        }
        else
        {
            copper_write_word(copperListBuffer + COP_WAIT_VHPOS, 1);
        }
        lineCount &= 0xFFu;
        copper_write_word(copperListBuffer + COP_WAIT_WAIT, (uint16_t)((lineCount << 8) | 1u));
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
    if (frontbufferBasePtr == (uintptr_t)frontbufferBase)
    {
        /* Currently using primary → swap to alt */
        bitPlane0BasePtr = (uintptr_t)bitPlane0BaseAlt;
        bitPlane1BasePtr = (uintptr_t)bitPlane1BaseAlt;
        bitPlane2BasePtr = (uintptr_t)bitPlane2BaseAlt;
        bitPlane3BasePtr = (uintptr_t)bitPlane3BaseAlt;
#ifdef AGA
        bitPlane4BasePtr = (uint32_t)bitPlane4BaseAlt;
        bitPlane5BasePtr = (uint32_t)bitPlane5BaseAlt;
        bitPlane6BasePtr = (uint32_t)bitPlane6BaseAlt;
        bitPlane7BasePtr = (uint32_t)bitPlane7BaseAlt;
#endif

        *(volatile uint32_t *)bitPlane0BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane1BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane2BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane3BaseAlt = copperZeroLong;
#ifdef AGA
        *(volatile uint32_t *)bitPlane4BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane5BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane6BaseAlt = copperZeroLong;
        *(volatile uint32_t *)bitPlane7BaseAlt = copperZeroLong;
#endif

        frontbufferBasePtr = (uintptr_t)frontbufferBaseAlt;
        frontbufferBaseAltPtr = (uintptr_t)frontbufferBase;
        spriteBlitQueue = (uintptr_t)&kSpriteBlitQueueSentinel_B;

        /* Save current camera state to slot 0 */
        dstXByteOffsetSaved0 = cameraXMod320;
        dstYStartLinesSaved0 = cameraYMod320;
        oldCameraXSaved0 = oldCameraX;
        oldCameraYSaved0 = oldCameraY;
        cameraXSaved0 = cameraX.s.i;
        cameraYSaved0 = cameraY.s.i;

        /* Restore camera state from slot 1 */
        cameraXMod320 = dstXByteOffsetSaved1;
        cameraYMod320 = dstYStartLinesSaved1;
        oldCameraX = oldCameraXSaved1;
        oldCameraY = oldCameraYSaved1;
        cameraX.s.i = cameraXSaved1;
        cameraY.s.i = cameraYSaved1;
    }
    else
    {
        /* Currently using alt → swap to primary */
        bitPlane0BasePtr = (uintptr_t)bitPlane0Base;
        bitPlane1BasePtr = (uintptr_t)bitPlane1Base;
        bitPlane2BasePtr = (uintptr_t)bitPlane2Base;
        bitPlane3BasePtr = (uintptr_t)bitPlane3Base;
#ifdef AGA
        bitPlane4BasePtr = (uint32_t)bitPlane4Base;
        bitPlane5BasePtr = (uint32_t)bitPlane5Base;
        bitPlane6BasePtr = (uint32_t)bitPlane6Base;
        bitPlane7BasePtr = (uint32_t)bitPlane7Base;
#endif

        *(volatile uint32_t *)bitPlane0Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane1Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane2Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane3Base = copperZeroLong;
#ifdef AGA
        *(volatile uint32_t *)bitPlane4Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane5Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane6Base = copperZeroLong;
        *(volatile uint32_t *)bitPlane7Base = copperZeroLong;
#endif

        frontbufferBasePtr = (uintptr_t)frontbufferBase;
        frontbufferBaseAltPtr = (uintptr_t)frontbufferBaseAlt;
        spriteBlitQueue = (uintptr_t)&kSpriteBlitQueueSentinel_A;

        /* Save current camera state to slot 1 */
        dstXByteOffsetSaved1 = cameraXMod320;
        dstYStartLinesSaved1 = cameraYMod320;
        oldCameraXSaved1 = oldCameraX;
        oldCameraYSaved1 = oldCameraY;
        cameraXSaved1 = cameraX.s.i;
        cameraYSaved1 = cameraY.s.i;

        /* Restore camera state from slot 0 */
        cameraXMod320 = dstXByteOffsetSaved0;
        cameraYMod320 = dstYStartLinesSaved0;
        oldCameraX = oldCameraXSaved0;
        oldCameraY = oldCameraYSaved0;
        cameraX.s.i = cameraXSaved0;
        cameraY.s.i = cameraYSaved0;
    }
}

/* ================================================================
 * SetCameraCoordinates  (SWOS2.s line 6158)
 *
 * Iteratively moves cameraX.s.i/cameraXMod320 toward targetX and
 * cameraY.s.i/cameraYMod320 toward targetY, one pixel per iteration,
 * with wrapping at 352/272.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetCameraCoordinates(int16_t targetX, int16_t targetY)
{
    /* Move X toward target */
    while (cameraX.s.i != targetX)
    {
        if (cameraX.s.i < targetX)
        {
            cameraX.s.i += 1;
            cameraXMod320 += 1;
            if (cameraXMod320 == 352)
                cameraXMod320 = 0;
        }
        else
        {
            cameraX.s.i -= 1;
            cameraXMod320 -= 1;
            if ((int16_t)cameraXMod320 < 0)
                cameraXMod320 = 351;
        }
    }

    /* Move Y toward target */
    while (cameraY.s.i != targetY)
    {
        if (cameraY.s.i < targetY)
        {
            cameraY.s.i += 1;
            cameraYMod320 += 1;
            if (cameraYMod320 == 272)
                cameraYMod320 = 0;
        }
        else
        {
            cameraY.s.i -= 1;
            cameraYMod320 -= 1;
            if ((int16_t)cameraYMod320 < 0)
                cameraYMod320 = 271;
        }
    }

    /* FixedPoint union handles the integer/FP sync implicitly: writing
     * cameraX.s.i updates the high word of the shared storage, so
     * cameraX.raw (the int32 view) stays consistent without any manual
     * sync. This replaces the earlier split-variable hack. */
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
#ifdef SWOS_SDL_BUILD
#include <string.h>
__attribute__((used, externally_visible))
void _BlitterCopyPanelToScreen2(void)
{
    /* Paint the menu BG into BOTH buffers, not just the drawing target.
     *
     * Long-standing flicker bug (since the C port — present on both the
     * Amiga C port and this SDL port): on some menu transitions and
     * within-menu redraws (e.g. play-match → result, lineup formation
     * change), the user briefly sees the OLD menu's content showing
     * under the new menu's entries for 1–2 frames. Two screenshots
     * confirmed: mid-transition shows new top + bottom strips with the
     * old menu's middle visible, and a formation change momentarily
     * loses the tactic grid entirely.
     *
     * Mechanism: _DrawMenu paints panel BG into the drawing target
     * (basePtr), draws entries on top, then _WaitForVSync swaps. The
     * just-drawn buffer becomes displayed. _BlitterCopyOverlayToScreen
     * copies the displayed frame back to the new drawing target so both
     * stay in sync — but until that copy runs, the OTHER buffer still
     * holds whatever the previous menu painted. Any draw path or async
     * present that touches the other buffer in this brief window
     * (timer-thread render, DrawSelectedMenuItemFrame, partial
     * _CallDrawMenuItem on either side of a swap) shows the stale
     * content.
     *
     * The original Amiga code relied on hardware bitplane-pointer
     * swap semantics that made the same flow atomic; the C port
     * doesn't get that for free. The cheapest defensive fix is to
     * paint the BG into BOTH buffers up front — neither side can show
     * old content because both are now the same clean BG.
     *
     * Cost: one extra full-frame memcpy on every menu draw (~91 KB).
     * Negligible at 50 Hz. */
    /* (frontbufferMaskBuffer is a static array, so its address is always
     * non-null — historical NULL guard removed.) */
    uint8_t *dst = (uint8_t *)frontbufferBasePtr;
    if (dst)
        memcpy(dst, frontbufferMaskBuffer, FRONTBUFFER_SIZE_TOTAL_BYTES);
    uint8_t *altDst = (uint8_t *)frontbufferBaseAltPtr;
    if (altDst && altDst != dst)
        memcpy(altDst, frontbufferMaskBuffer, FRONTBUFFER_SIZE_TOTAL_BYTES);
}
#else
__attribute__((used, externally_visible))
void _BlitterCopyPanelToScreen2(void)
{
    /* Enable blitter DMA and wait for idle */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14))
    {
        /* wait */
    }
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Configure blitter for A→D copy */
    BLT_W(custom->bltalwm, BLT_FWM_ALL);
    BLT_W(custom->bltafwm, BLT_FWM_ALL);
    BLT_W(custom->bltcon1, 0);
    BLT_W(custom->bltcon0, (uint16_t)BLTCON0_USEA_USED_LF_F0);
#ifdef AGA
    /* AGA: per-plane blit with interleaved stride */
    BLT_W(custom->bltamod, (uint16_t)(ROW_STRIDE_BYTES - PLANE_STRIDE));
    BLT_W(custom->bltdmod, (uint16_t)(ROW_STRIDE_BYTES - PLANE_STRIDE));
    {
        uint8_t *srcPlane = frontbufferMaskBuffer;
        uint8_t *dstPlane = (uint8_t *)frontbufferBasePtr;
        int16_t plane;
        for (plane = NUM_PLANES_MINUS1; plane >= 0; plane--)
        {
            custom->dmacon = (uint16_t)DMACON_BLT_ON;
            while (custom->dmaconr & (1u << 14)) {}
            custom->dmacon = (uint16_t)DMACON_BLT_OFF;

            BLT_L(custom->bltapt, (uint32_t)srcPlane);
            BLT_L(custom->bltdpt, (uint32_t)dstPlane);
            BLT_W(custom->bltsize, (uint16_t)BLTSIZE_OVERLAY_TO_SCREEN);
            srcPlane += PLANE_STRIDE;
            dstPlane += PLANE_STRIDE;
        }
    }
    /* Wait for last blit */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    while (custom->dmaconr & (1u << 14)) {}
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;
#else
    /* OCS: single full-buffer blit */
    BLT_W(custom->bltamod, 0);
    BLT_W(custom->bltdmod, 0);
    /* Set source and destination pointers */
    BLT_L(custom->bltapt, (uint32_t)frontbufferMaskBuffer);
    BLT_L(custom->bltdpt, frontbufferBasePtr);
    /* Kick the blit (writing BLTSIZE starts it) */
    BLT_W(custom->bltsize, (uint16_t)BLTSIZE_OVERLAY_PANEL);
    /* Wait for blit to complete, then disable blitter DMA */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14))
    {
        /* wait */
    }
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Reset blit size to small default */
    BLT_W(custom->bltsize, 0x0016u);
#endif
}
#endif /* SWOS_SDL_BUILD */

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
    volatile uint32_t *queue;

    /* Enable blitter and wait idle */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14)) {}
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Set blitter config: A→D copy, full masks, no shift */
    BLT_W(custom->bltafwm, BLT_FWM_ALL);
    BLT_W(custom->bltalwm, BLT_FWM_ALL);
    BLT_W(custom->bltcon1, 0);
    BLT_W(custom->bltcon0, (uint16_t)BLTCON0_USEA_USED_LF_F0);
    queue = (volatile uint32_t *)spriteBlitQueue;

    for (;;)
    {
        uint32_t srcPtr;
        uint16_t *entry16;

        /* Wait for blitter idle */
        custom->dmacon = (uint16_t)DMACON_BLT_ON;
        (void)(custom->dmaconr & (1u << 14));
        while (custom->dmaconr & (1u << 14)) {}
        custom->dmacon = (uint16_t)DMACON_BLT_OFF;

        srcPtr = queue[0];
        if ((int32_t)srcPtr < 0)
            break; /* sentinel: end of queue */

        BLT_L(custom->bltapt, srcPtr);
        BLT_L(custom->bltdpt, queue[1]);
        entry16 = (uint16_t *)queue;
        BLT_W(custom->bltamod, entry16[4]); /* +8 */
        BLT_W(custom->bltdmod, entry16[4]); /* same mod for A and D */
        BLT_W(custom->bltsize, entry16[5]); /* +10, kicks blit */

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
#ifdef SWOS_SDL_BUILD
__attribute__((used, externally_visible))
void _BlitterCopyPanelToScreen(void)
{
    uint8_t *dst = (uint8_t *)frontbufferBasePtr;
    /* (frontbufferMaskBuffer is a static array, address always non-null.) */
    if (dst)
        memcpy(dst, frontbufferMaskBuffer, FRONTBUFFER_SIZE_TOTAL_BYTES);
}
#else
__attribute__((used, externally_visible))
void _BlitterCopyPanelToScreen(void)
{
    /* Wait for blitter idle */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14)) {}
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Configure: A→D copy, full masks */
    BLT_W(custom->bltalwm, BLT_FWM_ALL);
    BLT_W(custom->bltafwm, BLT_FWM_ALL);
    BLT_W(custom->bltcon1, 0);
    BLT_W(custom->bltcon0, (uint16_t)BLTCON0_USEA_USED_LF_F0);
#ifdef AGA
    /* AGA: per-plane blit with interleaved stride */
    BLT_W(custom->bltamod, (uint16_t)(ROW_STRIDE_BYTES - PLANE_STRIDE));
    BLT_W(custom->bltdmod, (uint16_t)(ROW_STRIDE_BYTES - PLANE_STRIDE));
    {
        uint8_t *srcPlane = frontbufferMaskBuffer;
        uint8_t *dstPlane = (uint8_t *)frontbufferBasePtr;
        int16_t plane;
        for (plane = NUM_PLANES_MINUS1; plane >= 0; plane--)
        {
            custom->dmacon = (uint16_t)DMACON_BLT_ON;
            while (custom->dmaconr & (1u << 14)) {}
            custom->dmacon = (uint16_t)DMACON_BLT_OFF;

            BLT_L(custom->bltapt, (uint32_t)srcPlane);
            BLT_L(custom->bltdpt, (uint32_t)dstPlane);
            BLT_W(custom->bltsize, (uint16_t)BLTSIZE_OVERLAY_TO_SCREEN);
            srcPlane += PLANE_STRIDE;
            dstPlane += PLANE_STRIDE;
        }
    }
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    while (custom->dmaconr & (1u << 14)) {}
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;
#else
    /* OCS: single full-buffer blit */
    BLT_W(custom->bltamod, 0);
    BLT_W(custom->bltdmod, 0);
    /* Source = frontbufferMaskBuffer, Dest = frontbuffer */
    BLT_L(custom->bltapt, (uint32_t)frontbufferMaskBuffer);
    BLT_L(custom->bltdpt, frontbufferBasePtr);
    BLT_W(custom->bltsize, (uint16_t)BLTSIZE_OVERLAY_PANEL);
    /* Wait, then kick small trailing blit */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14)) {}
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;
    BLT_W(custom->bltsize, 0x0016u);
#endif
}
#endif /* SWOS_SDL_BUILD */

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
#ifdef SWOS_SDL_BUILD
    /* Chunky rewrite: zero the sprite's pixel buffer so subsequent
     * _BlitGlyphToSpriteBitmap calls stamp onto a clean background.
     * The Amiga version programs bltdpt + D-only fill; our sprite
     * data is a flat 8bpp byte array of `wquads * 16 * nlines` bytes. */
    SpriteGraphics **table = (SpriteGraphics **)(uintptr_t)spriteGraphicsPtr;
    SpriteGraphics *spr = table[spriteIndex];
    if (!spr) return;
    uint8_t *data = spr->sprData;
    if (!data) return;
    size_t bytes = (size_t)spr->wquads * 16 * (size_t)spr->nlines;
    memset(data, 0, bytes);
#else
    uint32_t *gfxTable = (uint32_t *)spriteGraphicsPtr;
    uint8_t *sprEntry;
    uint32_t destPtr;
    uint16_t height;
    uint16_t stride;
    uint16_t blitSize;

    sprEntry = (uint8_t *)gfxTable[spriteIndex];

    /* Wait for blitter idle */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14)) {}
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Configure: D-only fill (LF=F0 with no source channels) */
    BLT_W(custom->bltalwm, BLT_FWM_ALL);
    BLT_W(custom->bltafwm, BLT_FWM_ALL);
    BLT_W(custom->bltcon1, 0);
    BLT_W(custom->bltcon0, (uint16_t)BLTCON0_USED_LF_F0);
    BLT_W(custom->bltadat, 0); /* fill with zeros */
    BLT_W(custom->bltdmod, 0);
    /* Dest = sprite data pointer (first long of sprite entry) */
    destPtr = *(uint32_t *)sprEntry;
    BLT_L(custom->bltdpt, destPtr);
    /* BLTSIZE = (height << 8) | stride */
    height = *(uint16_t *)(sprEntry + 12);
    stride = *(uint16_t *)(sprEntry + 14); /* TILE_STRIDE offset */
    blitSize = (uint16_t)((height << (NUM_PLANES_SHIFT + 6)) | stride);
    BLT_W(custom->bltsize, blitSize);
#endif
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
    volatile uint16_t *copperColor0 = (volatile uint16_t *)(copperListBuffer + COP_PAL_B0_C0_VAL_OFF);

    custom->color[0] = color;
    while (--loopCount != 0)
    {
        custom->color[0] = color; /* keep writing to prevent optimizer from removing */
    }
    custom->color[0] = *copperColor0;
}

/* ================================================================
 * WriteCopperPalette
 *
 * Writes a full palette into the copperListBuffer color value slots.
 * AGA: 256 colors across 8 banks of 32 colors each.
 * OCS: 16 colors into both palette blocks (B0 and B1).
 * ================================================================ */
__attribute__((used, externally_visible))
void _WriteCopperPalette(const uint16_t *palette)
{
#ifdef AGA
    static const uint16_t bankValOffsets[8] = {
        COP_PAL_B0_C0_VAL_OFF, COP_PAL_B1_C0_VAL_OFF,
        COP_PAL_B2_C0_VAL_OFF, COP_PAL_B3_C0_VAL_OFF,
        COP_PAL_B4_C0_VAL_OFF, COP_PAL_B5_C0_VAL_OFF,
        COP_PAL_B6_C0_VAL_OFF, COP_PAL_B7_C0_VAL_OFF
    };
    int16_t bank;
    for (bank = 0; bank < 8; bank++)
    {
        uint8_t *dst = copperListBuffer + bankValOffsets[bank];
        const uint16_t *src = palette + (bank * 32);
        int16_t i;
        for (i = 31; i >= 0; i--)
        {
            copper_write_word(dst, *src++);
            dst += 4; /* stride: reg+val pairs = 4 bytes */
        }
    }
#else
    uint8_t *palA = copperListBuffer + COP_PAL_B0_C0_VAL_OFF;
    uint8_t *palB = copperListBuffer + COP_PAL_B1_C0_VAL_OFF;
    int16_t i;
    for (i = 15; i >= 0; i--)
    {
        uint16_t color = *palette++;
        copper_write_word(palA, color);
        copper_write_word(palB, color);
        palA += 4;
        palB += 4;
    }
#endif
}

#ifdef AGA
/* ================================================================
 * WriteCopperPitchPalette (AGA only)
 *
 * Applies 9 pitch-specific color entries into gamePalette using
 * paletteIndices, then pushes the full 256-color gamePalette into
 * the copper buffer via WriteCopperPalette.
 * ================================================================ */
extern uint16_t gamePalette[];
extern uint16_t pitchNumberAndType;
extern const uint16_t *pitchPalettePtrs[];
extern uint8_t paletteIndices[];

__attribute__((used, externally_visible))
void _WriteCopperPitchPalette(void)
{
    /* pitchNumberAndType packs pitch *number* in the high byte and pitch
     * *type* in the low byte. The palette lookup uses only the type, so
     * mask to 0xFF — matches asm `andi.w #$FF,d1` at SWOS2.s line 15938.
     * Without the mask, any non-zero pitch number makes the uint16 > 6
     * and the clamp below silently substitutes the default "normal"
     * (type 4) palette — symptom: HIL replays after a CAREER session
     * show normal grass even when the saved HIL specified a different
     * pitch type. Fresh boot worked only because pitch number happened
     * to be 0 then. */
    uint16_t pitchIdx = pitchNumberAndType & 0xFF;
    const uint16_t *pitchPal;
    int16_t i;

    if (pitchIdx > 6)
        pitchIdx = 4; /* default to "normal" if out of range */

    pitchPal = pitchPalettePtrs[pitchIdx];

    /* Apply 9 entries into gamePalette */
    for (i = 0; i < 9; i++)
    {
        uint16_t idx = paletteIndices[i];
        gamePalette[idx] = pitchPal[i];
    }

    _WriteCopperPalette(gamePalette);
}
#endif
