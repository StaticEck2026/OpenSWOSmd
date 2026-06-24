/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "game_utils.h"
#include "game_types.h"
#include "deterministic.h"

extern uintptr_t assetLoaderCacheFlag;
extern uintptr_t assetLoaderWorkPtr;
extern volatile int16_t waitingInterrupt;
extern uint16_t useIndividualPlayerSkinColor;
extern uint8_t goalBase[];
extern uint8_t *goalBasePtr;
extern uint16_t nextGoal;
extern uint8_t *nextGoalPtr;
extern uint8_t spriteGraphics[];
extern uint8_t *spriteGraphicsPtr;
extern volatile uint16_t currentGameTick;
extern uint16_t lastGameTick;
extern uint16_t lastFrameTicks;

__attribute__((used, externally_visible))
void AssetLoaderResetState(void)
{
    assetLoaderCacheFlag = 0;
    assetLoaderWorkPtr = 0;
}

#ifdef SWOS_SDL_BUILD
extern FixedPoint cameraX;
extern FixedPoint cameraY;
extern void SwapBuffersAndCameraViewState(void);
extern void SetCameraCoordinates(int16_t x, int16_t y);
extern int Platform_VBlank(void);
#endif

__attribute__((used, externally_visible))
void WaitForVSync(void)
{
#ifdef SWOS_SDL_BUILD
    /* On the Amiga, WaitForVSync was a busy spin on `waitingInterrupt`
     * which the Level-3 ISR cleared after running the buffer-swap +
     * camera-restore sequence. On the SDL host we run the buffer swap
     * inline on the same (main) thread, then hand control to
     * Platform_VBlank which pumps SDL events, blits the just-finished
     * bitplane frame to the SDL window, and paces to ~50 Hz.
     *
     * The SDL_Timer thread still runs `_Int3TimerHandler`, but the
     * branch that swaps buffers gates on `waitingInterrupt`, which we
     * never set here -- so the timer thread skips the swap and only
     * does fade ticks / audio / currentGameTick. */
    int16_t cx = cameraX.s.i;
    int16_t cy = cameraY.s.i;
    SwapBuffersAndCameraViewState();
    SetCameraCoordinates(cx, cy);
    Platform_VBlank();
#else
    waitingInterrupt = 1;
    while (waitingInterrupt != 0)
    {
        /* spin — cleared by VBlank ISR */
    }
#endif
}

__attribute__((used, externally_visible))
void InitSkinAndDefaultPointers(void)
{
    useIndividualPlayerSkinColor = 1;
    goalBasePtr = goalBase;
    /* On Amiga, `nextGoal` was the label immediately following the 19000-byte
     * `goalBase` buffer, so &nextGoal == goalBase+19000 was the natural
     * "end-of-buffer" sentinel for highlight pointer wrapping. On SDL the
     * linker places `nextGoal` anywhere in BSS, so the wrap test
     * `if (a1 == nextGoalPtr)` in SaveCoordinatesForHighlights never matches
     * and `currentHilPtr` walks past goalBase's end, stomping adjacent
     * globals (e.g. renderStackBase) and producing bogus sprite pointers.
     * Match HandleInstantReplayStateSwitch (game_loop.c:1005) which uses
     * baseAddr+19000 explicitly. */
    nextGoalPtr = goalBase + 19000;
    spriteGraphicsPtr = spriteGraphics;
}

/* ================================================================
 * ReadTimerDelta  (SWOS2.s line 20026)
 *
 * Calculates the number of game ticks elapsed since the last call.
 * Stores the delta in lastFrameTicks and updates lastGameTick.
 * Returns the delta in d0.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t ReadTimerDelta(void)
{
    uint16_t delta = currentGameTick - lastGameTick;
    lastFrameTicks = delta;
    lastGameTick = currentGameTick;
    return delta;
}

/* ================================================================
 * Randomize2  (SWOS2.s)
 *
 * Initializes secondary random number generator.
 * XORs input val with randomSeed, stores high byte to seed2 and
 * low byte to randXorIndex2 (original asm writes a word across both),
 * then updates randXorKey2 from kRandomTable.
 * ================================================================ */
extern uint16_t randomSeed;
extern uint8_t seed2;
extern uint8_t randXorIndex2;
extern uint8_t randXorKey2;
extern uint8_t kRandomTable[257];

__attribute__((used, externally_visible))
void Randomize2(uint16_t val)
{
    uint16_t result = val ^ randomSeed;
    /* Original asm: move.w d0,(seed2) — word write across seed2 + randXorIndex2 */
    seed2 = (uint8_t)(result >> 8);
    randXorIndex2 = (uint8_t)(result & 0xFF);
    randXorKey2 = kRandomTable[randXorIndex2];
}

__attribute__((used, externally_visible))
void BuildPaletteFromColors(const uint8_t *src, uint16_t *dst)
{
    int16_t i;
    for (i = 15; i >= 0; i--)
    {
        uint16_t color = 0;
        color += (uint16_t)(src[0] >> 4);
        color <<= 4;
        color += (uint16_t)(src[1] >> 4);
        color <<= 4;
        color += (uint16_t)(src[2] >> 4);
        src += 3;
        *dst++ = color;
    }
}
