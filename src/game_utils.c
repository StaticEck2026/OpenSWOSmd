/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "game_utils.h"
#include "deterministic.h"

extern uint32_t assetLoaderCacheFlag;
extern uint32_t assetLoaderWorkPtr;
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
void _AssetLoaderResetState(void)
{
    assetLoaderCacheFlag = 0;
    assetLoaderWorkPtr = 0;
}

__attribute__((used, externally_visible))
void _WaitForVSync(void)
{
    waitingInterrupt = 1;
    while (waitingInterrupt != 0)
    {
        /* spin — cleared by VBlank ISR */
    }
}

__attribute__((used, externally_visible))
void _InitSkinAndDefaultPointers(void)
{
    useIndividualPlayerSkinColor = 1;
    goalBasePtr = goalBase;
    nextGoalPtr = (uint8_t *)&nextGoal;
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
uint16_t _ReadTimerDelta(void)
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
void _Randomize2(uint16_t val)
{
    uint16_t result = val ^ randomSeed;
    /* Original asm: move.w d0,(seed2) — word write across seed2 + randXorIndex2 */
    seed2 = (uint8_t)(result >> 8);
    randXorIndex2 = (uint8_t)(result & 0xFF);
    randXorKey2 = kRandomTable[randXorIndex2];
}

__attribute__((used, externally_visible))
void _BuildPaletteFromColors(const uint8_t *src, uint16_t *dst)
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
