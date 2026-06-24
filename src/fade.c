/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "fade.h"
#include "display.h"

#ifdef SWOS_SDL_BUILD
#include "platform_sdl.h"
#include <SDL.h>
#endif

/* COPPER_WRITE_W: write a 16-bit value into the copper list buffer.
 * On Amiga m68k, GCC decomposes plain `*(volatile uint16_t *)addr = val`
 * into byte-level move.b pairs which corrupts copper-list word slots
 * (the copper expects atomic word reads). Force a real move.w instruction
 * via inline asm. On the SDL host the copper list is a regular RAM buffer
 * so a plain store is fine. */
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
#define COPPER_WRITE_W(addr, val) \
    __asm__ volatile ("move.w %0,(%1)" : : "d"((uint16_t)(val)), "a"((volatile uint16_t *)(addr)) : "memory")
#else
#define COPPER_WRITE_W(addr, val) (*(volatile uint16_t *)(addr) = (uint16_t)(val))
#endif

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef short int16_t;

#ifdef SWOS_SDL_BUILD
/* Helper: yield CPU briefly while driving the Amiga Level-3 ISR. The
 * fade-in / fade-out spin loops below wait for fadeState to flip,
 * which is updated by FadeInTick / FadeOutTick inside the ISR. On
 * WSL2 the SDL_Timer thread that normally runs the ISR can be starved
 * by main-thread spinning, so we run the ISR ourselves (throttled to
 * 50 Hz) and SDL_Delay to yield CPU. */
static inline void fade_wait_one_isr_tick(void)
{
    RunInt3HandlerThrottled();
    SDL_Delay(2);
}
#define FADE_SPIN_BODY() fade_wait_one_isr_tick()
#else
#define FADE_SPIN_BODY() ((void)0)
#endif

extern volatile uint16_t fadeState;
extern volatile uint16_t fadeCommand;
extern volatile uint16_t fadeInitFlag;
extern uint16_t g_quitToDos;
extern uint16_t loaderModeFlag;
extern uint16_t paletteIndex;
extern uint8_t copperListBuffer[];
extern void *paletteTable[];   /* array of 7 pointers to uint16_t[16] palettes */

/* FadeOutToBlack (SWOS2.s line 6781)
 *
 * 1. If fadeInitFlag != SENTINEL, spin until fadeState != 0 (wait for fade-in to finish)
 * 2. If fadeState == 0, return (already black/idle)
 * 3. If fadeCommand == SENTINEL, set fadeCommand = 0xFFFF (trigger fade-out in ISR)
 * 4. Spin until fadeState == 0 (wait for fade-out to complete)
 */
__attribute__((used, externally_visible))
void FadeOutToBlack(void)
{
    if (fadeInitFlag != FADE_SENTINEL)
    {
        while (fadeState == 0)
        {
            if (g_quitToDos) return;
            FADE_SPIN_BODY();
        }
    }

    if (fadeState == 0)
        return;

    if (fadeCommand == FADE_SENTINEL)
        fadeCommand = 0xFFFFu;

    while (fadeState != 0)
    {
        if (g_quitToDos) return;
        FADE_SPIN_BODY();
    }
}

/* FadeIn (SWOS2.s line 6760)
 *
 * 1. If fadeCommand != SENTINEL, spin until fadeState == 0, then call FadeOutToBlack
 * 2. If fadeState != 0, return (already active)
 * 3. If fadeInitFlag == SENTINEL, set fadeInitFlag = 0xFFFF (trigger fade-in in ISR)
 * 4. Spin until fadeState != 0 (wait for fade-in to complete)
 */
__attribute__((used, externally_visible))
void FadeIn(void)
{
    if (fadeCommand != FADE_SENTINEL)
    {
        while (fadeState != 0)
        {
            if (g_quitToDos) return;
            FADE_SPIN_BODY();
        }
        FadeOutToBlack();
    }

    if (fadeState != 0)
        return;

    if (fadeInitFlag == FADE_SENTINEL)
        fadeInitFlag = 0xFFFFu;

    while (fadeState == 0)
    {
        if (g_quitToDos) return;
        FADE_SPIN_BODY();
    }
}

/* FadeOutAndIn (SWOS2.s line 6801) */
__attribute__((used, externally_visible))
void FadeOutAndIn(void)
{
    FadeOutToBlack();
    FadeIn();
}

/* ================================================================
 * Fade shift/mask lookup tables (from SWOS2.s lines 6136, 6170)
 *
 * Each entry is 4 bytes: [shift_amount:16][mask:16]
 * Index = fadeLevel >> 5 (0..4 for fadeLevel 0x00..0x80)
 * ================================================================ */
static const uint16_t kFadeShiftMaskTable[10] = {
    0x0004, 0x0000,   /* shift=4, mask=0x000 */
    0x0003, 0x0111,   /* shift=3, mask=0x111 */
    0x0002, 0x0333,   /* shift=2, mask=0x333 */
    0x0001, 0x0777,   /* shift=1, mask=0x777 */
    0x0000, 0x0FFF    /* shift=0, mask=0xFFF */
};

/* Loader-path variant: same structure but last entry has mask=0x0FFF → clamped below */
static const uint16_t fadeShiftMaskTable[10] = {
    0x0004, 0x0000,
    0x0003, 0x0111,
    0x0002, 0x0333,
    0x0001, 0x0777,
    0x0000, 0x0FFF
};

/* ================================================================
 * ApplyFadeStep  (SWOS2.s line 6110)
 *
 * Applies a fade level (d2) to the palette currently selected by
 * paletteIndex, writing results to copperListBuffer palette slots.
 *
 * Two paths: normal (just shift+mask) or loader (clamp blue >= 2).
 * ================================================================ */
__attribute__((used, externally_visible))
void ApplyFadeStep(uint16_t fadeLevel)
{
    uint16_t idx;
    uint16_t shift;
    uint16_t mask;
    const uint16_t *palette;
#ifndef AGA
    volatile uint16_t *palA;
    volatile uint16_t *palB;
#endif
    int16_t i;

    idx = (fadeLevel >> 5) * 2; /* index into shift/mask pairs */

    if (loaderModeFlag == 0)
    {
        /* Normal path */
        shift = kFadeShiftMaskTable[idx];
        mask  = kFadeShiftMaskTable[idx + 1];
    }
    else
    {
        /* Loader path — uses separate table + blue channel clamping */
        shift = fadeShiftMaskTable[idx];
        mask  = fadeShiftMaskTable[idx + 1];
    }

    palette = (const uint16_t *)paletteTable[paletteIndex];

#ifdef AGA
    {
        /* AGA: apply fade to 256 colors across 8 banks of 32 */
        static const uint16_t bankValOffsets[8] = {
            COP_PAL_B0_C0_VAL_OFF, COP_PAL_B1_C0_VAL_OFF,
            COP_PAL_B2_C0_VAL_OFF, COP_PAL_B3_C0_VAL_OFF,
            COP_PAL_B4_C0_VAL_OFF, COP_PAL_B5_C0_VAL_OFF,
            COP_PAL_B6_C0_VAL_OFF, COP_PAL_B7_C0_VAL_OFF
        };
        int16_t bank;
        for (bank = 0; bank < 8; bank++)
        {
            volatile uint16_t *dst = (volatile uint16_t *)(copperListBuffer + bankValOffsets[bank]);
            for (i = 31; i >= 0; i--)
            {
                uint16_t color = *palette++;
                color = (color >> shift) & mask;
                if (loaderModeFlag != 0)
                {
                    uint16_t blue = color & 0x000F;
                    if (blue < 2)
                        color = (color & 0x0FF0) | 2;
                }
                COPPER_WRITE_W(dst, color);
                dst += 2; /* stride 4 bytes */
            }
        }
    }
#else
    palA = (volatile uint16_t *)(copperListBuffer + COP_PAL_B0_C0_VAL_OFF);
    palB = (volatile uint16_t *)(copperListBuffer + COP_PAL_B1_C0_VAL_OFF);

    if (loaderModeFlag == 0)
    {
        /* Normal: shift and mask each color */
        for (i = 15; i >= 0; i--)
        {
            uint16_t color = *palette++;
            color = (color >> shift) & mask;
            COPPER_WRITE_W(palA, color);
            COPPER_WRITE_W(palB, color);
            palA += 2;  /* stride 4 bytes */
            palB += 2;
        }
    }
    else
    {
        /* Loader: shift/mask, then clamp blue nibble to minimum 2 */
        for (i = 15; i >= 0; i--)
        {
            uint16_t color = *palette++;
            uint16_t blue;
            color = (color >> shift) & mask;
            blue = color & 0x000F;
            if (blue < 2)
            {
                color = (color & 0x0FF0) | 2;
            }
            COPPER_WRITE_W(palA, color);
            COPPER_WRITE_W(palB, color);
            palA += 2;
            palB += 2;
        }
    }
#endif
}

/* ================================================================
 * FadeOutTick  (SWOS2.s line 6176)
 *
 * Called each frame from ISR. Decrements fadeCommand toward 0,
 * applying the fade at each step. When done, sets fadeState = 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void FadeOutTick(void)
{
    uint16_t level;

    if (fadeCommand == FADE_SENTINEL)
        return;

    level = fadeCommand;
    if ((int16_t)level < 0)
    {
        /* First tick: start at FADE_MAX */
        fadeCommand = FADE_MAX;
        level = FADE_MAX;
    }

    ApplyFadeStep(level);

    fadeCommand -= FADE_STEP;
    if ((int16_t)fadeCommand < 0)
    {
        fadeCommand = FADE_SENTINEL;
        fadeState = 0;
    }
}

/* ================================================================
 * FadeInTick  (SWOS2.s line 6195)
 *
 * Called each frame from ISR. Increments fadeInitFlag toward 0x80,
 * applying the fade at each step. When done, sets fadeState = 1.
 * ================================================================ */
__attribute__((used, externally_visible))
void FadeInTick(void)
{
    uint16_t level;

    if (fadeInitFlag == FADE_SENTINEL)
        return;

    level = fadeInitFlag;
    if ((int16_t)level < 0)
    {
        /* First tick: start at 0 */
        fadeInitFlag = 0;
        level = 0;
    }

    ApplyFadeStep(level);

    fadeInitFlag += FADE_STEP;
    if (fadeInitFlag > FADE_MAX)
    {
        fadeInitFlag = FADE_SENTINEL;
        fadeState = 1;
    }
}
