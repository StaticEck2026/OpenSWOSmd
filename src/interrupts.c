/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "interrupts.h"
#include "display.h"
#include "deterministic.h"

/* ---- Hardware register addresses (interrupt-specific, not in display.h) ---- */
#define INTREQR_OFF     0x1E    /* Interrupt Request read (offset from CUSTOM) */
#define INTREQ_ADDR     0x00DFF09Cu
#define BPLCON0_OFF     0x100   /* Bitplane control 0 (offset from CUSTOM) */

#define CIAA_ICR_ADDR   0x00BFED01u
#define CIAA_CRA_ADDR   0x00BFEE01u
#define CIAB_ICR_ADDR   0x00BFDD00u

/* Interrupt vector offsets from VBR */
#define LEVEL_1_VEC     0x64u
#define LEVEL_3_VEC     0x6Cu
#define LEVEL_5_VEC     0x74u

/* ---- External symbols from resload.c ---- */
extern void _FixSnoopBug(void);
extern uint8_t _KBHook(void);       /* returns raw CIAA_SDR byte in d0 */
extern uint32_t S_VBR;

/* ---- External symbols from SWOS2.s (kept in asm) ---- */
extern void Int3Handler(void);
extern void Int5Handler(void);

/* ---- Volatile globals from SWOS2.s inline data ---- */
extern volatile int16_t keyDelay;
extern volatile uint16_t keyCount;
extern volatile uint8_t keyBuffer[];
extern uint16_t writeOnly01;
extern volatile uint16_t lastVBlankServiceTick;
extern volatile uint8_t keyIn;

/* ---- Volatile globals from SWOS2.s BSS ---- */
extern volatile int16_t waitingInterrupt;
extern volatile uint16_t currentGameTick;
extern volatile uint16_t currentTick;
extern volatile uint16_t didSwapThisFrame;
extern volatile uint8_t audioDbgFlagA;
extern volatile uint16_t audioActive;
extern volatile uint16_t debugMode;
extern volatile uint16_t showingUserStats;
extern volatile uint16_t paused;
extern int16_t cameraX;
extern int16_t cameraY;

/* ---- External C functions called from Int3TimerHandler ---- */
extern void _UpdatePlanePtrsAndBlitParams(void);
extern void _SwapBuffersAndCameraViewState(void);
extern void _SetCameraCoordinates(int16_t targetX, int16_t targetY);
extern void _FadeOutTick(void);
extern void _FadeInTick(void);
extern uint8_t _AudioIRQUpdateChannels(void);

/* ================================================================
 * _InitHardwareAtFrameStart  (SWOS2.s:5431-5446)
 *
 * Called during startup. Waits for the raster to reach the top
 * of the frame, then disables all interrupts and DMA.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitHardwareAtFrameStart(void)
{
    _FixSnoopBug();

    /* Clear BPLCON0 — use absolute address (not in struct, at offset 0x100) */
    *(volatile uint16_t *)(CUSTOM_BASE + BPLCON0_OFF) = 0;

    /* Wait for raster position <= 0x0A (top of frame) */
    uint32_t pos;
    do
    {
        pos = (*(volatile uint32_t *)&custom->vposr & 0x1FFFFu) >> 8;
    } while (pos > 0x0Au);

    /* Disable all interrupts and DMA */
    custom->intena = 0x7FFF;
    custom->intreq = 0x7FFF;
    custom->dmacon = 0x7FFF;
    *(volatile uint8_t *)CIAA_ICR_ADDR = 0x7F;
    *(volatile uint8_t *)CIAB_ICR_ADDR = 0x7F;
}

/* ================================================================
 * _SetupInterrupts  (SWOS2.s:6056-6065)
 *
 * Installs interrupt handlers into the vector table:
 *   - Saves old Level 3 vector to Level 1
 *   - Int3Handler at Level 3 (VBlank)
 *   - Int5Handler at Level 5 (serial port)
 * Then enables Master + RBF + VBlank + Software interrupts.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetupInterrupts(void)
{
    volatile uint32_t *vectors = (volatile uint32_t *)S_VBR;

    /* Save old Level 3 handler to Level 1 slot */
    vectors[LEVEL_1_VEC / 4] = vectors[LEVEL_3_VEC / 4];

    /* Install Int3Handler at Level 3 */
    vectors[LEVEL_3_VEC / 4] = (uint32_t)Int3Handler;

    /* Install Int5Handler at Level 5 */
    vectors[LEVEL_5_VEC / 4] = (uint32_t)Int5Handler;

    /* Enable: SET(15) + Master(14) + RBF(11) + VERTB(5) + SOFTINT(2)
     * Binary: 1100 1000 0010 0100 = 0xC824 */
    custom->intena = 0xC824;
}

/* ================================================================
 * Int2KeyboardHandler  (SWOS2.s:5256-5291)
 *
 * Level 2 keyboard ISR body. Called from asm wrapper that saves
 * all registers and uses rte. Reads CIAA keyboard serial data,
 * converts scan code, fills keyBuffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Int2KeyboardHandler(void)
{
    /* Check INTREQR bit 3 (I/O Ports and timers) */
    uint16_t intreq = custom->intreqr;
    if (!(intreq & (1u << 3)))
        return;

    /* Read CIAA interrupt control register */
    uint8_t d0 = *(volatile uint8_t *)CIAA_ICR_ADDR;
    if (!(d0 & (1u << 7)))     /* IR bit */
        goto ack;
    if (!(d0 & (1u << 3)))     /* SP bit */
        goto ack;

    /* KBHook reads CIAA_SDR, checks for DEL exit key, returns raw byte in d0 */
    d0 = _KBHook();

    /* Start keyboard handshake: set bit 6 of CIAA_CRA */
    *(volatile uint8_t *)CIAA_CRA_ADDR |= (1u << 6);

    /* Timing delay: poll VPOSR 301 times */
    {
        int16_t i;
        for (i = 300; i >= 0; i--)
        {
            (void)*(volatile uint32_t *)&custom->vposr;
        }
    }

    /* End keyboard handshake: clear bit 6 of CIAA_CRA */
    *(volatile uint8_t *)CIAA_CRA_ADDR &= ~(uint8_t)(1u << 6);

    /* Convert raw serial data to scan code: NOT then ROR.B #1 */
    d0 = ~d0;
    d0 = (uint8_t)((d0 >> 1) | (d0 << 7));

    keyDelay = 0;
    keyIn = d0;

    /* If high bit set (key release), skip buffer insertion */
    if (d0 & 0x80)
        goto ack;

    /* Buffer full check (max 9 entries used out of 12) */
    if (keyCount >= 9)
        goto ack;

    /* Set key repeat delay and add to buffer */
    keyDelay = (int16_t)0x800C;
    keyBuffer[keyCount] = d0;
    keyCount++;

ack:
    /* Acknowledge I/O ports interrupt */
    custom->intreq = 8;
}

/* ================================================================
 * Int3TimerHandler  (SWOS2.s:5305-5364)
 *
 * Level 1 ISR body (deferred VBlank work). Called from asm wrapper.
 * Handles display buffer swap, fade ticks, audio IRQ, game tick
 * counting, and keyboard repeat.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Int3TimerHandler(void)
{
    if (waitingInterrupt != 0)
    {
        uint16_t delta = (uint16_t)(currentGameTick - lastVBlankServiceTick);
        if (delta >= writeOnly01)
        {
            lastVBlankServiceTick = currentGameTick;
            _UpdatePlanePtrsAndBlitParams();

            /* Read camera position (high word of 32-bit fixed-point on big-endian) */
            int16_t camX = cameraX;
            int16_t camY = cameraY;

            _SwapBuffersAndCameraViewState();
            _SetCameraCoordinates(camX, camY);

            waitingInterrupt = 0;
            didSwapThisFrame = 1;
            /* ASM: bra.w *+4 falls through to isr_common_housekeeping
             * (does NOT skip housekeeping on swap frames) */
        }
    }

    /* isr_common_housekeeping — runs EVERY VBlank, including swap frames */
    _FadeOutTick();
    _FadeInTick();

    audioDbgFlagA = 0;
    if (audioActive != 0)
    {
        audioDbgFlagA = _AudioIRQUpdateChannels();
    }

    /* debugMode LINE-A trap skipped (dead code in non-debug builds) */

    /* isr_update_ticks_and_input */
    currentTick++;

    if (showingUserStats == 0 && paused == 0)
    {
        currentGameTick++;

        /* Key repeat logic: keyDelay is negative when counting down */
        if (keyDelay < 0)
        {
            keyDelay--;
            if (keyDelay >= 0)
            {
                /* Repeat timer expired — insert key into buffer */
                uint8_t key = keyIn;
                if (keyCount < 9)
                {
                    keyBuffer[keyCount] = key;
                    keyCount++;
                }
                keyDelay = (int16_t)0x8004;
            }
        }
    }

clear_and_return:
    /* Clear software initiated interrupt */
    custom->intreq = 0x0004;
}
