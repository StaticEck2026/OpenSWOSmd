/* animated_patterns.c — Chunk 38a: Animated crowd fan patterns
 *
 * Ported from SWOS2.s lines 53269-53624:
 *   DrawAnimatedPatterns, SwapTwoRandomIndicesFromAnimPatternsTable,
 *   ResetAnimatedPatternsForBothTeams, ResetAnimatedPatterns,
 *   DrawPatternFromIndex
 *
 * The pitch tiles have static/animated pairs at consecutive tile indices.
 * Odd tile index = static (fans sitting), even = animated (fans jumping).
 * DrawAnimatedPatterns toggles selected tiles between states each frame.
 * ResetAnimatedPatterns forces all animated tiles back to static.
 */

#include "animated_patterns.h"
#include "game_types.h"

/* ---- external asm globals ---- */
extern uint16_t replayState;
extern uint16_t animPatternsState;
extern uint16_t currentGameTick;
extern uint16_t trainingGame;
extern uint16_t goalCounter;
extern uint16_t patternsGoalCounter;
extern uint16_t showFansCounter;
extern uint16_t animPatternsIndex;
extern uint16_t cameraX;
extern uint16_t cameraY;
extern uint16_t gameStatePl;
extern uint16_t gameState;
extern uint16_t fansSidesSwapped;
extern uint16_t panicReasonCode;
extern int16_t  animatedPatterns[];
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;
extern uint32_t teamScoredGamePtr;
extern TeamGame topTeamInGame;
extern TeamGame bottomTeamInGame;
extern uint8_t  *spriteGraphicsPtr;
extern uint8_t  pitchDatBuffer[];
extern uint32_t frontbufferBasePtr;
extern uint8_t  frontbufferMaskBuffer[];
extern uint8_t  currentMenu[];
extern uint8_t  animPatternsRandomTable[];
extern uint16_t patternStatic;
extern uint16_t otherTeamScored;

#define TILE_SIZE 128
#define SCREEN_HEIGHT 272

/* ---- external functions ---- */
extern uint16_t _Rand(void);
extern void _PanicHaltLoop(void);
extern void _DrawSprite16Pixels(uint16_t spriteIndex, int16_t x, int16_t y);

/* ================================================================
 * swapTwoRandomIndicesFromAnimPatternsTable
 * (SWOS2.s line 53512)
 * Shuffles two random entries in the 64-byte permutation table.
 * ================================================================ */
static void swapTwoRandomIndicesFromAnimPatternsTable(void)
{
    uint16_t r1, r2;
    uint8_t tmp;

    r1 = (uint16_t)((_Rand() * 63) >> 8);
    r2 = (uint16_t)((_Rand() * 63) >> 8);

    tmp = animPatternsRandomTable[r2];
    animPatternsRandomTable[r2] = animPatternsRandomTable[r1];
    animPatternsRandomTable[r1] = tmp;
}

/* ================================================================
 * drawPatternFromIndex
 * (SWOS2.s line 53590)
 *
 * Draws a single animated tile.
 * d3 = pattern index (into 42-column tile grid).
 * Computes pixel X/Y from grid position, copies tile data to sprite 137,
 * draws to frontbuffer + mask buffer.
 * ================================================================ */
static void drawPatternFromIndex(uint16_t patternIndex)
{
    int16_t pixelX, pixelY;
    int16_t screenX, screenY;
    uint32_t *sprGfx;
    uint32_t *tileEntries;
    uint32_t spriteAddr;

    /* decompose patternIndex into row,col in a 42-column grid */
    pixelY = (int16_t)(patternIndex / 42) * 16;     /* row * 16 */
    pixelX = (int16_t)(patternIndex % 42) * 16;     /* col * 16 */

    /* load sprite 137 graphics pointer and set its tile data */
    sprGfx = (uint32_t *)(void *)spriteGraphicsPtr;
    spriteAddr = sprGfx[137];

    tileEntries = (uint32_t *)(void *)pitchDatBuffer;
    *(uint32_t *)spriteAddr = tileEntries[patternIndex];

    /* compute screen-relative coordinates */
    screenX = pixelX - (int16_t)cameraX;
    screenY = pixelY - (int16_t)cameraY;

    /* bounds check */
    if (screenX >= 336 || screenY >= SCREEN_HEIGHT)
        return;
    if (screenX <= (int16_t)0xFFF0 || screenY <= (int16_t)0xFFF0)
        return;

    /* draw to frontbuffer */
    _DrawSprite16Pixels(137, screenX, screenY);

    /* draw to mask buffer (swap frontbuffer ptr temporarily) */
    {
        uint32_t savedFbPtr = frontbufferBasePtr;
        frontbufferBasePtr = (uint32_t)frontbufferMaskBuffer;
        _DrawSprite16Pixels(137, screenX, screenY);
        frontbufferBasePtr = savedFbPtr;
    }
}

/* ================================================================
 * resetAnimatedPatterns (inner loop)
 * (SWOS2.s line 53558)
 *
 * Scans pitchDatBuffer entries. For tiles whose offset from currentMenu
 * is non-zero, <= 0xB00, and even (bit 7 of offset clear = animated variant),
 * subtracts TILE_SIZE to reset them to their static counterpart.
 * Then clears the 8-entry animatedPatterns array to 0xFFFF.
 * ================================================================ */
static void resetAnimatedPatterns(uint32_t *a0, int16_t count)
{
    int16_t i;
    uint32_t baseAddr = (uint32_t)currentMenu;

    for (i = count; i >= 0; i--) {
        uint32_t d0 = *a0 - baseAddr;
        if (d0 != 0 && d0 <= 0xB00 && !(d0 & 0x80)) {
            /* animated variant (even tile index) → subtract TILE_SIZE → static */
            *a0 -= TILE_SIZE;
        }
        a0++;
    }

    /* clear animatedPatterns array */
    for (i = 0; i < 8; i++) {
        animatedPatterns[i] = (int16_t)0xFFFF;
    }
}

/* ================================================================
 * _ResetAnimatedPatternsForBothTeams
 * (SWOS2.s line 53547)
 *
 * Resets animated tiles for both top and bottom fan sections.
 * Top section: pitchDatBuffer + 0xA8/4 = entry 42, count 126
 * Bottom section: pitchDatBuffer + 0x2178/4 = entry 2142, count 126
 * ================================================================ */
void __attribute__((used, externally_visible))
_ResetAnimatedPatternsForBothTeams(void)
{
    if (trainingGame != 0)
        return;

    /* top fans section: offset 0xA8 bytes = 42 longs, 126 entries */
    resetAnimatedPatterns((uint32_t *)((uint8_t *)pitchDatBuffer + 0xA8), 125);

    /* bottom fans section: offset 0x2178 bytes = 2142 longs, 126 entries */
    resetAnimatedPatterns((uint32_t *)((uint8_t *)pitchDatBuffer + 0x2178), 125);
}

/* ================================================================
 * _DrawAnimatedPatterns
 * (SWOS2.s line 53273)
 *
 * Per-frame animated pattern update. 8-state machine that determines
 * animation intensity based on game state (goals, ball position, etc.).
 * Selects random tiles from the pitch grid and toggles them between
 * static and animated states.
 * ================================================================ */
void __attribute__((used, externally_visible))
_DrawAnimatedPatterns(void)
{
    int16_t d6;       /* max patterns to update */
    uint16_t d0;      /* animate flag (0 = de-animate, non-zero = animate) */
    int16_t d7;       /* loop counter */
    uint32_t baseAddr = (uint32_t)currentMenu;

    /* ---- state 0: replayState == 0 → no game ---- */
    if (replayState == 0) {
        animPatternsState = 0;

        if (trainingGame != 0)
            return;

        /* track goal celebration timer */
        if (patternsGoalCounter != 0) {
            patternsGoalCounter++;
            if (patternsGoalCounter >= 300)
                patternsGoalCounter = 0;
        } else {
            if (goalCounter != 0)
                patternsGoalCounter = 1;
        }

        if (showFansCounter != 0)
            showFansCounter--;

        otherTeamScored = 0;

        if (showFansCounter != 0)
            goto game_hasnt_started;

        if (gameStatePl == ST_GAME_IN_PROGRESS)
            goto game_in_progress;

        if (gameState == ST_KEEPER_HOLDS_BALL) {
            TeamGeneralInfo *teamPtr = lastTeamPlayedBeforeBreak;
            if (teamPtr != (void *)0) {
                Sprite *controlledPlayer = teamPtr->controlledPlayer;
                (void)controlledPlayer;
                /* asm: loads controlled player but then unconditionally falls through
                 * to game_in_progress regardless of whether it's null or not */
            }
            goto game_in_progress;
        }

        if (patternsGoalCounter == 0)
            goto ball_out_of_play;

        {
            uint32_t scoredTeamPtr = teamScoredGamePtr;
            if ((int16_t)cameraY > 440) {
                /* bottom of pitch */
                if (fansSidesSwapped != 0)
                    goto check_top_team;
                goto check_bottom_team;
            } else {
                /* top of pitch */
                if (fansSidesSwapped != 0)
                    goto check_bottom_team;
            check_top_team:
                if (scoredTeamPtr == (uint32_t)&topTeamInGame)
                    goto check_cheers_phase;
                goto other_team_scored_be_still;
            check_bottom_team:
                if (scoredTeamPtr == (uint32_t)&bottomTeamInGame)
                    goto check_cheers_phase;
                goto other_team_scored_be_still;
            }

        check_cheers_phase:
            if (patternsGoalCounter < 25)
                goto greatest_cheers;
            if (patternsGoalCounter < 200)
                goto middle_cheers;
            if (patternsGoalCounter < 225)
                goto cheers_fading;
            /* patternsGoalCounter >= 225: fall through to ball_out_of_play */
        }
    } else {
        /* replayState != 0 */
        if (animPatternsState == 0)
            return;

        otherTeamScored = 0;

        switch (animPatternsState) {
        case 1: goto ball_out_of_play;
        case 2: goto greatest_cheers;
        case 3: goto middle_cheers;
        case 4: goto other_team_scored_be_still;
        case 5: goto cheers_fading;
        case 6: goto game_hasnt_started;
        case 7: goto game_in_progress;
        default:
            panicReasonCode = 0x5665;
            _PanicHaltLoop();
            return;
        }
    }

ball_out_of_play:
    animPatternsState = 1;
    d6 = 5;
    d0 = currentGameTick & 31;
    if (d0 <= 25)
        d0 = 0;
    goto pattern_loop;

greatest_cheers:
    animPatternsState = 2;
    d6 = 8;
    d0 = 1;
    goto pattern_loop;

middle_cheers:
    animPatternsState = 3;
    d6 = 8;
    d0 = currentGameTick & 0xF;
    if (d0 <= 4)
        d0 = 0;
    goto pattern_loop;

other_team_scored_be_still:
    animPatternsState = 4;
    d6 = 1;
    d0 = currentGameTick & 31;
    otherTeamScored = 1;
    if (d0 <= 15)
        d0 = 0;
    goto pattern_loop;

cheers_fading:
    animPatternsState = 5;
    d6 = 8;
    d0 = 0;
    goto pattern_loop;

game_hasnt_started:
    animPatternsState = 6;
    d6 = 8;
    d0 = currentGameTick & 0x1F;
    if (d0 <= 0xF)
        d0 = 0;
    goto pattern_loop;

game_in_progress:
    animPatternsState = 7;
    d6 = 1;
    d0 = currentGameTick & 0x1F;
    if (d0 <= 0x19)
        d0 = 0;
    /* fall through */

pattern_loop:
    patternStatic = d0;

    for (d7 = 7; d7 >= 0; d7--) {
        int16_t d3 = animatedPatterns[7 - d7];

        if (d3 >= 0) {
            /* active pattern entry: draw once then clear */
            animatedPatterns[7 - d7] = (int16_t)0xFFFF;
            drawPatternFromIndex((uint16_t)d3);
            continue;
        }

        /* empty slot (0xFFFF): try to fill if within d6 budget */
        if ((uint16_t)d6 <= (uint16_t)d7)
            continue;

        {
            uint16_t d4 = patternStatic;
            int16_t randomVal;
            int16_t spriteIdx;
            int16_t row, col;
            int16_t pitchIdx;
            uint32_t *tileEntryPtr;
            uint32_t tileVal;
            int32_t tileOffset;
            int32_t tileIndex;

            animPatternsIndex++;
            if (animPatternsIndex >= 64)
                animPatternsIndex = 0;

            /* check camera Y to determine which fan section */
            if ((int16_t)cameraY < 64) {
                /* top of pitch: sprite range 21..83 */
                swapTwoRandomIndicesFromAnimPatternsTable();
                randomVal = (int8_t)animPatternsRandomTable[animPatternsIndex];

                /* if other team scored and random index is in [21,42), suppress animation */
                if (otherTeamScored != 0 && randomVal >= 21 && randomVal < 42)
                    d4 = 0;

                spriteIdx = randomVal + 21;
            } else if ((int16_t)cameraY >= 544) {
                /* bottom of pitch: sprite range 1071..1133 */
                swapTwoRandomIndicesFromAnimPatternsTable();
                randomVal = (int8_t)animPatternsRandomTable[animPatternsIndex];
                spriteIdx = randomVal + 1071;
            } else {
                /* middle of pitch: fans not visible, skip */
                continue;
            }

            /* decompose spriteIdx into row/col and compute pitchDatBuffer index */
            row = spriteIdx / 21;
            col = spriteIdx % 21;
            pitchIdx = row * 42 + col + ((int16_t)cameraX >> 4);
            d3 = pitchIdx;

            /* bounds check: &pitchDatBuffer[pitchIdx*4] must be in [pitchDatBuffer, currentMenu) */
            tileEntryPtr = ((uint32_t *)(void *)pitchDatBuffer) + pitchIdx;
            if ((uint32_t)tileEntryPtr < (uint32_t)pitchDatBuffer ||
                (uint32_t)tileEntryPtr >= (uint32_t)currentMenu) {
                panicReasonCode = 0x2772;
                _PanicHaltLoop();
            }

            /* read tile entry and check if it's an animated tile */
            tileVal = *tileEntryPtr;
            tileOffset = (int32_t)(tileVal - (uint32_t)currentMenu);
            tileIndex = (int32_t)((uint32_t)tileOffset >> 7);

            if (tileIndex == 0 || tileIndex > 22)
                continue;

            {
                int32_t newIndex = tileIndex;

                if (tileIndex & 1) {
                    /* odd tile = static variant */
                    if (d4 == 0)
                        continue;   /* static wanted but tile already static */
                    newIndex = tileIndex + 1;  /* switch to animated (even) */
                } else {
                    /* even tile = animated variant */
                    if (d4 != 0)
                        continue;   /* animated wanted but tile already animated */
                    newIndex = tileIndex - 1;  /* switch to static (odd) */
                }

                animatedPatterns[7 - d7] = d3;
                *tileEntryPtr = (uint32_t)currentMenu + ((uint32_t)newIndex << 7);
            }
        }

        drawPatternFromIndex((uint16_t)d3);
    }
}
