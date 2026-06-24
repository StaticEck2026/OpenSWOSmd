/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "interrupts.h"
#include "game_types.h"
#include "display.h"
#include "deterministic.h"

/* ---- Hardware register addresses (interrupt-specific, not in display.h) ---- */
#define INTREQR_OFF     0x1E    /* Interrupt Request read (offset from CUSTOM) */
/* INTREQ_ADDR comes from display.h. */
#define BPLCON0_OFF     0x100   /* Bitplane control 0 (offset from CUSTOM) */

#define CIAA_ICR_ADDR   0x00BFED01u
#define CIAA_CRA_ADDR   0x00BFEE01u
#define CIAB_ICR_ADDR   0x00BFDD00u

/* Interrupt vector offsets from VBR */
#define LEVEL_1_VEC     0x64u
#define LEVEL_3_VEC     0x6Cu
#define LEVEL_5_VEC     0x74u

/* ---- External symbols from resload.c ---- */
extern void FixSnoopBug(void);
extern uint8_t KBHook(void);       /* returns raw CIAA_SDR byte in d0 */
extern uint32_t _S_VBR;

/* ---- External symbols from SWOS2.s (kept in asm) ---- */
extern void Int3Handler(void);
extern void Int5Handler(void);

/* ---- Volatile globals from SWOS2.s inline data ---- */
extern volatile int16_t _keyDelay;
extern volatile uint16_t _keyCount;
extern volatile uint8_t _keyBuffer[];
extern uint16_t _writeOnly01;
extern volatile uint16_t _lastVBlankServiceTick;
extern volatile uint8_t _keyIn;

/* ---- Volatile globals from SWOS2.s BSS ---- */
extern volatile int16_t _waitingInterrupt;
extern volatile uint16_t _currentGameTick;
extern volatile uint16_t _currentTick;
extern volatile uint16_t _didSwapThisFrame;
extern volatile uint8_t _audioDbgFlagA;
extern volatile uint16_t _audioActive;
extern volatile uint16_t debugMode;
extern volatile uint16_t _showingUserStats;
extern volatile uint16_t _paused;
extern FixedPoint _cameraX;
extern FixedPoint _cameraY;

/* ---- External C functions called from Int3TimerHandler ---- */
extern void UpdatePlanePtrsAndBlitParams(void);
extern void SwapBuffersAndCameraViewState(void);
extern void SetCameraCoordinates(int16_t targetX, int16_t targetY);
extern void FadeOutTick(void);
extern void FadeInTick(void);
extern uint8_t AudioIRQUpdateChannels(void);

/* ================================================================
 * InitHardwareAtFrameStart  (SWOS2.s:5431-5446)
 *
 * Called during startup. Waits for the raster to reach the top
 * of the frame, then disables all interrupts and DMA.
 * ================================================================ */
__attribute__((used, externally_visible))
void InitHardwareAtFrameStart(void)
{
    FixSnoopBug();

    /* Clear BPLCON0 — use absolute address (not in struct, at offset 0x100) */
    *(volatile uint16_t *)(CUSTOM_BASE + BPLCON0_OFF) = 0;

    /* Wait for raster position <= 0x0A (top of frame) */
    uint32_t pos;
    do
    {
        pos = (*(volatile uint32_t *)&_custom->vposr & 0x1FFFFu) >> 8;
    } while (pos > 0x0Au);

    /* Disable all interrupts and DMA */
    _custom->intena = 0x7FFF;
    _custom->intreq = 0x7FFF;
    _custom->dmacon = 0x7FFF;
    *(volatile uint8_t *)CIAA_ICR_ADDR = 0x7F;
    *(volatile uint8_t *)CIAB_ICR_ADDR = 0x7F;
}

/* ================================================================
 * SetupInterrupts  (SWOS2.s:6056-6065)
 *
 * Installs interrupt handlers into the vector table:
 *   - Saves old Level 3 vector to Level 1
 *   - Int3Handler at Level 3 (VBlank)
 *   - Int5Handler at Level 5 (serial port)
 * Then enables Master + RBF + VBlank + Software interrupts.
 * ================================================================ */
__attribute__((used, externally_visible))
void SetupInterrupts(void)
{
    /* S_VBR is the m68k Vector Base Register — dead address on SDL.
     * Widen through uintptr_t to silence Wint-to-pointer-cast on 64-bit. */
    volatile uint32_t *vectors = (volatile uint32_t *)(uintptr_t)_S_VBR;

    /* Save old Level 3 handler to Level 1 slot */
    vectors[LEVEL_1_VEC / 4] = vectors[LEVEL_3_VEC / 4];

    /* Install Int3Handler at Level 3 */
    vectors[LEVEL_3_VEC / 4] = (uintptr_t)Int3Handler;

    /* Install Int5Handler at Level 5 */
    vectors[LEVEL_5_VEC / 4] = (uintptr_t)Int5Handler;

    /* Enable: SET(15) + Master(14) + RBF(11) + VERTB(5) + SOFTINT(2)
     * Binary: 1100 1000 0010 0100 = 0xC824 */
    _custom->intena = 0xC824;
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
    uint16_t intreq = _custom->intreqr;
    if (!(intreq & (1u << 3)))
        return;

    /* Read CIAA interrupt control register */
    uint8_t d0 = *(volatile uint8_t *)CIAA_ICR_ADDR;
    if (!(d0 & (1u << 7)))     /* IR bit */
        goto ack;
    if (!(d0 & (1u << 3)))     /* SP bit */
        goto ack;

    /* KBHook reads CIAA_SDR, checks for DEL exit key, returns raw byte in d0 */
    d0 = KBHook();

    /* Start keyboard handshake: set bit 6 of CIAA_CRA */
    *(volatile uint8_t *)CIAA_CRA_ADDR |= (1u << 6);

    /* Timing delay: poll VPOSR 301 times */
    {
        int16_t i;
        for (i = 300; i >= 0; i--)
        {
            (void)*(volatile uint32_t *)&_custom->vposr;
        }
    }

    /* End keyboard handshake: clear bit 6 of CIAA_CRA */
    *(volatile uint8_t *)CIAA_CRA_ADDR &= ~(uint8_t)(1u << 6);

    /* Convert raw serial data to scan code: NOT then ROR.B #1 */
    d0 = ~d0;
    d0 = (uint8_t)((d0 >> 1) | (d0 << 7));

    _keyDelay = 0;
    _keyIn = d0;

    /* If high bit set (key release), skip buffer insertion */
    if (d0 & 0x80)
        goto ack;

    /* Buffer full check (max 9 entries used out of 12) */
    if (_keyCount >= 9)
        goto ack;

    /* Set key repeat delay and add to buffer */
    _keyDelay = (int16_t)0x800C;
    _keyBuffer[_keyCount] = d0;
    _keyCount++;

ack:
    /* Acknowledge I/O ports interrupt */
    _custom->intreq = 8;
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
    /* NOTE: Don't ack COPER/VERTB/BLIT (bits 4/5/6) here — the original
     * asm doesn't, neither does SWOS_C_OLD. Int3 is the SOFT (bit 2)
     * deferred-work handler; the VBL ISR (which sets waitingInterrupt
     * and triggers SOFT) handles VERTB ack itself. Acking VERTB early
     * here was suppressing the next-frame swap signal, which left one
     * buffer stale at the top of the screen and showed as flicker on
     * the menu logo. */
    if (_waitingInterrupt != 0)
    {
        uint16_t delta = (uint16_t)(_currentGameTick - _lastVBlankServiceTick);
        if (delta >= _writeOnly01)
        {
            _lastVBlankServiceTick = _currentGameTick;
            UpdatePlanePtrsAndBlitParams();

            int16_t camX = _cameraX.s.i;
            int16_t camY = _cameraY.s.i;

            SwapBuffersAndCameraViewState();
            SetCameraCoordinates(camX, camY);

            _waitingInterrupt = 0;
            _didSwapThisFrame = 1;
        }
    }

    FadeOutTick();
    FadeInTick();

    _audioDbgFlagA = 0;
    if (_audioActive != 0)
    {
        _audioDbgFlagA = AudioIRQUpdateChannels();
    }

    /* debugMode LINE-A trap skipped (dead code in non-debug builds) */

    /* isr_update_ticks_and_input */
    _currentTick++;

    if (_showingUserStats == 0 && _paused == 0)
    {
        _currentGameTick++;

        /* Key repeat logic: keyDelay is negative when counting down */
        if (_keyDelay < 0)
        {
            _keyDelay--;
            if (_keyDelay >= 0)
            {
                /* Repeat timer expired — insert key into buffer */
                uint8_t key = _keyIn;
                if (_keyCount < 9)
                {
                    _keyBuffer[_keyCount] = key;
                    _keyCount++;
                }
                _keyDelay = (int16_t)0x8004;
            }
        }
    }

    /* Clear software initiated interrupt. The asm reached this via
     * `clear_and_return:` from several early-return paths; the C
     * version structures the early-return as nested-if so the label
     * is no longer needed. */
    _custom->intreq = 0x0004;
}
