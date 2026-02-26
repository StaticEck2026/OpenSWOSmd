/*
 * highlight.c — Highlight/Replay save system
 * Ported from SWOS2.s (Chunk 37)
 */

#include "highlight.h"
#include "game_types.h"

/* ---- Sprite constants ---- */
#define SPR_REPLAY_FRAME_00  1095
#define SPR_BIG_0            173
#define SPR_BIG_0_ALT        183   /* IDA: SPR_SCROLL_ARROW_UP — actually right-team big digits */

/* ---- Hil buffer size per scene ---- */
#define HIL_SCENE_LONGS   4750    /* 4750 x 4 = 19000 bytes */
#define HIL_SCENE_BYTES   19000
#define HIL_MAX_SCENES    10
#define K_HIL_FILE_SIG    0x0000000Cu
#define K_HIL_FILE_MAXSZ  192254u

/* ---- External globals ---- */
extern uint16_t numScenesInHilFile;
extern uint16_t gamePitchTypeOrSeason;
extern int16_t  gamePitchType;
extern uint16_t gameSeason;
extern uint16_t currentGameTick;
extern uint16_t plgD0Param;
extern TeamGame topTeamInGame;
extern TeamGame bottomTeamInGame;
extern uint16_t trainingGame;
extern uint16_t pitchNumberAndType;
extern uint16_t hilNumGoals;
extern uint16_t hilPitchNumber;
extern uint8_t  hilFileBuffer[];
extern uint8_t  gameName[];
extern uint8_t  gameRound[];
extern uint8_t  hilGameName[];
extern uint8_t  hilGameRound[];
extern uint16_t hilTeam1Goals;
extern uint16_t hilTeam2Goals;
extern uint16_t statsTeam1Goals;
extern uint16_t statsTeam2Goals;
extern uint16_t gameMaxSubstitutes;
extern uint16_t hilMaxSubstitutes;
extern uint16_t normalReplayRunning;
extern uint8_t *goalBasePtr;
extern uint8_t *nextGoalPtr;
extern uint8_t *currentHilPtr;
extern uint8_t *currentGoalPtr;
extern uint16_t goIntoHighlightsLoop;
extern uint16_t hilSceneNumber;
extern uint32_t savedCurrentHilPtr;
extern uint32_t savedCurrentGoalPtr;
extern uint32_t savedGoalBasePtr;
extern uint32_t savedNextGoalPtr;
extern uint8_t  hilStartOfGoalsBuffer[];
extern uint8_t  inGameHilBuffer[];
extern uint16_t throwInDeadVar;
extern uint16_t team1TotalGoals;
extern uint16_t team2TotalGoals;
extern uint16_t animPatternsState;
extern uint16_t replayState;
extern int16_t  cameraX;
extern int16_t  cameraY;
extern int16_t  savedCameraX;
extern int16_t  savedCameraY;
extern uint16_t hilLeftTeamGoals;
extern uint16_t hilRightTeamGoals;
extern uint16_t paused;

/* ---- External functions ---- */
extern uint16_t _Rand(void);
extern void _FadeOutToBlack(void);
extern void _FadeIn(void);
extern void _SetCameraCoordinates(int16_t targetX, int16_t targetY);
extern void _InitCameraMoveView(int16_t targetX, int16_t targetY);
extern void _SwapBuffersAndCameraViewState(void);
extern void _MoveView(int16_t newScreenX, int16_t newScreenY);
extern void _CDarkenRectangle(uint16_t sprIdx, uint16_t x, uint16_t y);
extern void _ResetAnimatedPatternsForBothTeams(void);
extern void _DrawAnimatedPatterns(void);

/* ---- Local probability tables (matching asm data) ---- */

/* pitchTypeSeasonalProbabilities: 12 seasons * 7 types = 84 bytes
 * Original at line 10919 in SWOS2.s */
static const uint8_t pitchTypeSeasonalProbs[] = {
    30,20,30,20, 0, 0, 0,
    20,30,20,20,10, 0, 0,
    10,30,10,30,20, 0, 0,
     0,10,10,30,40,10, 0,
     0, 0, 0,10,40,40,10,
     0, 0, 0, 0,40,40,20,
     0, 0, 0, 0,30,30,40,
     0, 0, 0, 0,50,30,20,
     0, 0, 0,20,40,30,10,
     0,20, 0,40,30,10, 0,
    10,30,10,40,10, 0, 0,
    20,30,20,30, 0, 0, 0,
};

/* pitchTypeProbabilities: 8 bytes (standard, non-seasonal)
 * Original at line 10927 in SWOS2.s */
static const uint8_t pitchTypeProbs[] = {
    5, 5, 10, 20, 30, 20, 10, 0
};

/* pitchNumberProbabilities: 16-byte lookup
 * Original at line 11971 in SWOS2.s */
static const uint8_t pitchNumProbs[] = {
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 4, 4, 2, 2, 3, 3
};

/* ============================================================
 * Internal helpers (not exported to asm)
 * ============================================================ */

/* Wrap pointer forward in ring buffer (matches ValidateHilPointerAdd) */
static uint8_t *wrapPointerFwd(uint8_t *p)
{
    if (p == nextGoalPtr)
        p = goalBasePtr;
    return p;
}

/* Wrap pointer backward in ring buffer (matches ValidateHilPointerSub) */
static uint8_t *wrapPointerBack(uint8_t *p)
{
    if (p < goalBasePtr)
        p = nextGoalPtr;
    return p;
}

/* Wrap a1-style pointer (IncrementHilBufferPointer) — same as wrapPointerFwd */
static uint8_t *wrapHilPtr(uint8_t *p)
{
    if (p == nextGoalPtr)
        p = goalBasePtr;
    return p;
}

/* ============================================================
 * _SetPitchTypeAndNumber
 * ============================================================ */
__attribute__((used, externally_visible))
void _SetPitchTypeAndNumber(void)
{
    numScenesInHilFile = 0;

    int16_t pitchType;
    const uint8_t *probs;
    int needChoose = 0;

    if (gamePitchTypeOrSeason == 0) {
        /* Fixed pitch type mode */
        pitchType = gamePitchType;
        if (pitchType < 0) {
            /* Negative = auto-select from standard probabilities */
            probs = pitchTypeProbs;
            needChoose = 1;
        }
    } else {
        /* Seasonal pitch type mode */
        probs = pitchTypeSeasonalProbs + (uint16_t)gameSeason * 7;
        needChoose = 1;
    }

    if (needChoose) {
        /* Random selection based on cumulative probabilities */
        uint16_t r = _Rand();
        uint8_t d0 = (uint8_t)((uint32_t)r * 100 / 256);
        pitchType = 0;
        while (d0 >= *probs) {
            d0 -= *probs++;
            pitchType++;
        }
    }

    /* Determine pitch number.
     * Note: asm has 3 dead instructions here (currentGameTick & 0x10 << 4)
     * that get immediately overwritten in both branches. Omitted. */
    uint16_t pitchNum;
    if (plgD0Param != 0) {
        pitchNum = _Rand() & 0xF;
    } else {
        /* Derive from team data */
        uint16_t v = *(uint16_t *)topTeamInGame.teamName;
        v ^= (uint16_t)topTeamInGame.prShirtCol;
        v ^= (uint16_t)topTeamInGame.prShortsCol;
        pitchNum = v & 0xF;
    }

    uint16_t d0 = (uint16_t)pitchNumProbs[pitchNum] << 8;

    if (trainingGame != 0)
        d0 = 0x500;

    pitchNumberAndType = (uint16_t)pitchType + d0;
}

/* ============================================================
 * _CopyTeamsAndHeader
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _CopyTeamsAndHeader(void)
{
    numScenesInHilFile = hilNumGoals;
    pitchNumberAndType = hilPitchNumber;

    /* Copy team data from hil file buffer (offset 8) */
    uint8_t *src = &hilFileBuffer[8];
    uint16_t i;
    for (i = 0; i < 1018; i++) {
        ((uint8_t *)&bottomTeamInGame)[i] = src[1018 + i];
        ((uint8_t *)&topTeamInGame)[i] = src[i];
    }

    /* Copy game name and round */
    for (i = 0; i < 40; i++) {
        gameName[i] = hilGameName[i];
        gameRound[i] = hilGameRound[i];
    }
}

/* ============================================================
 * _SetHilFileHeader
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _SetHilFileHeader(void)
{
    *(uint32_t *)hilFileBuffer = K_HIL_FILE_SIG;
    *(uint32_t *)&hilFileBuffer[4] = K_HIL_FILE_MAXSZ;

    /* Pack team data into hil file buffer at offset 8 */
    uint8_t *dst = &hilFileBuffer[8];
    uint16_t i;
    for (i = 0; i < 1018; i++) {
        dst[1018 + i] = ((uint8_t *)&bottomTeamInGame)[i];
        dst[i] = ((uint8_t *)&topTeamInGame)[i];
    }

    /* Copy game name and round */
    for (i = 0; i < 40; i++) {
        hilGameName[i] = gameName[i];
        hilGameRound[i] = gameRound[i];
    }

    hilNumGoals = numScenesInHilFile;
    hilTeam1Goals = statsTeam1Goals;
    hilTeam2Goals = statsTeam2Goals;
    hilPitchNumber = pitchNumberAndType;
    hilMaxSubstitutes = gameMaxSubstitutes;
}

/* ============================================================
 * _SaveHighlightsPointers
 * ============================================================ */
__attribute__((used, externally_visible))
void _SaveHighlightsPointers(void)
{
    if (numScenesInHilFile == 0)
        return;

    normalReplayRunning = 1;
    savedGoalBasePtr = (uint32_t)goalBasePtr;
    savedNextGoalPtr = (uint32_t)nextGoalPtr;
    savedCurrentHilPtr = (uint32_t)currentHilPtr;
    savedCurrentGoalPtr = (uint32_t)currentGoalPtr;
    goIntoHighlightsLoop = 1;
    hilSceneNumber = 0xFFFF;
}

/* ============================================================
 * _RestoreHighlightPointers
 * ============================================================ */
__attribute__((used, externally_visible))
void _RestoreHighlightPointers(void)
{
    normalReplayRunning = 0;
    goalBasePtr = (uint8_t *)savedGoalBasePtr;
    nextGoalPtr = (uint8_t *)savedNextGoalPtr;
    currentHilPtr = (uint8_t *)savedCurrentHilPtr;
    currentGoalPtr = (uint8_t *)savedCurrentGoalPtr;
    goIntoHighlightsLoop = 0;
}

/* ============================================================
 * _FadeAndSaveHighlightScene
 * ============================================================ */
__attribute__((used, externally_visible))
void _FadeAndSaveHighlightScene(void)
{
    _FadeOutToBlack();
    _SaveHighlightScene();
    _FadeIn();
}

/* ============================================================
 * _SaveHighlightScene
 * Saves current replay ring buffer segment to the hil goals buffer.
 * Maximum HIL_MAX_SCENES scenes. If buffer is full, shifts scenes
 * 1..9 down to 0..8, freeing the last slot.
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _SaveHighlightScene(void)
{
    /* Asm: move.w #10,d0; beq locret — the beq is dead (10 != 0). Omitted. */

    if (numScenesInHilFile == HIL_MAX_SCENES) {
        /* Buffer full — shift scenes 1..9 into slots 0..8 */
        uint32_t *src = (uint32_t *)inGameHilBuffer;
        uint32_t *dst = (uint32_t *)hilStartOfGoalsBuffer;
        int16_t block;
        uint16_t j;
        for (block = 8; block >= 0; block--) {
            for (j = 0; j < HIL_SCENE_LONGS; j++) {
                *dst++ = *src++;
            }
        }
        numScenesInHilFile--;
    }

    /* Scan forward from currentHilPtr for a frame-start marker (bit 7 of first byte set).
     * Scan at most 19000 bytes (4750 longs). */
    uint32_t count = 0;
    uint8_t *p = currentHilPtr;

    for (;;) {
        if (count == HIL_SCENE_BYTES)
            return;  /* Not found within range */
        count += 4;
        p += 4;
        if (p == nextGoalPtr)
            p = goalBasePtr;
        if (*p & 0x80)
            break;  /* Found frame-start marker */
    }

    /* Copy 19000 bytes from ring buffer (starting at p) into goals buffer slot */
    uint32_t *dest = (uint32_t *)(hilStartOfGoalsBuffer +
                                   (uint32_t)numScenesInHilFile * HIL_SCENE_BYTES);
    uint16_t i;
    for (i = 0; i < HIL_SCENE_LONGS; i++) {
        *dest++ = *(uint32_t *)p;
        p += 4;
        if (p == nextGoalPtr)
            p = goalBasePtr;
    }
    numScenesInHilFile++;
}

/* ============================================================
 * _ThrowInDeadProc
 * ============================================================ */
__attribute__((used, externally_visible))
void _ThrowInDeadProc(void)
{
    throwInDeadVar = 1;
}

/* ============================================================
 * _SaveCoordinatesForHighlights
 * ============================================================ */
__attribute__((used, externally_visible))
void _SaveCoordinatesForHighlights(int16_t spriteIndexOrMarker, uint16_t x, uint16_t y)
{
    uint8_t *a1 = currentHilPtr;

    if (spriteIndexOrMarker >= 0) {
        /* Pack sprite data: (index << 20) | (x << 10) | y, with bit 31 clear */
        uint32_t packed = (uint32_t)(uint16_t)spriteIndexOrMarker;
        packed <<= 10;
        packed |= (x & 0x3FF);
        packed <<= 10;
        packed |= (y & 0x3FF);
        packed &= ~(1u << 31);  /* bclr #31 */
        *(uint32_t *)a1 = packed;
        a1 += 4;
        if (a1 == nextGoalPtr)
            a1 = goalBasePtr;
        currentHilPtr = a1;
        return;
    }

    if ((uint16_t)spriteIndexOrMarker == 0xFFFE) {
        /* End-of-segment marker: write -1 */
        *(uint32_t *)a1 = 0xFFFFFFFF;
        return;
    }

    /* Frame-start marker (d0 < 0, not -2): write camera coords with bit 15 set in x */
    *(uint16_t *)a1 = x | 0x8000;
    *(uint16_t *)(a1 + 2) = y;
    a1 += 4;
    a1 = wrapHilPtr(a1);
    currentHilPtr = a1;

    /* Write team goals */
    *(uint16_t *)a1 = team1TotalGoals;
    *(uint16_t *)(a1 + 2) = team2TotalGoals;
    a1 += 4;
    a1 = wrapHilPtr(a1);

    /* Write animated patterns state (duplicated in both words) */
    *(uint16_t *)a1 = animPatternsState;
    *(uint16_t *)(a1 + 2) = animPatternsState;
    a1 += 4;
    a1 = wrapHilPtr(a1);
    currentHilPtr = a1;

    /* Write end-of-frame marker */
    *(uint32_t *)a1 = 0xFFFFFFFF;
}

/* ============================================================
 * _DrawReplayFrame
 * Complex replay frame playback state machine.
 * Goto labels map 1:1 to asm labels:
 *   sprite_decode  = loc_10C494
 *   check_next     = loc_10C4D0
 *   end_replay     = loc_10C4C4
 *   after_decode   = loc_10C4E2
 * ============================================================ */
__attribute__((used, externally_visible))
void _DrawReplayFrame(void)
{
    uint8_t *a0 = currentGoalPtr;
    uint32_t d0;

    if (replayState == 1) {
        /* STATE 1 → 2: Initialize replay */
        replayState = 2;
        savedCameraX = cameraX;
        savedCameraY = cameraY;

        /* Read first frame data (without advancing a0) for camera init */
        uint32_t val = *(uint32_t *)a0;
        val &= ~(1u << 31);  /* bclr #31 */

        _ResetAnimatedPatternsForBothTeams();
        _ReplayUpdateCameraAndSwapBuffers((int16_t)(val >> 16),
                                          (int16_t)(val & 0xFFFF));
    }

    /* loc_10C434: read entry */
    d0 = *(uint32_t *)a0;
    a0 += 4;
    if (!(d0 & (1u << 31)))
        goto sprite_decode;

    /* Frame-start marker processing */
    a0 = wrapPointerFwd(a0);
    d0 &= ~(1u << 31);
    _MoveView((int16_t)(d0 >> 16), (int16_t)(d0 & 0xFFFF));

    /* Read team goals */
    hilLeftTeamGoals = *(uint16_t *)a0;
    hilRightTeamGoals = *(uint16_t *)(a0 + 2);
    a0 += 4;
    a0 = wrapPointerFwd(a0);

    /* Read animated patterns state (stored as two identical words) */
    animPatternsState = *(uint16_t *)a0;
    animPatternsState = *(uint16_t *)(a0 + 2);
    a0 += 4;
    a0 = wrapPointerFwd(a0);

    _DrawAnimatedPatterns();

    /* Read next entry after frame marker */
    d0 = *(uint32_t *)a0;
    a0 += 4;
    if (d0 == 0xFFFFFFFF)
        goto end_replay;
    goto check_next;

sprite_decode:
    /* loc_10C494: sprite data processing */
    a0 = wrapPointerFwd(a0);
    {
        /* Unpack: bits 20-31 = index, bits 10-19 = x (signed 10-bit),
         * bits 0-9 = y (signed 10-bit).
         * 68k: lsl.l #12 then asr.l #22 sign-extends the 10-bit field. */
        int32_t sx = (int32_t)(d0 << 12) >> 22;
        int32_t sy = (int32_t)(d0 << 22) >> 22;
        uint32_t sprIdx = d0 >> 20;
        _CDarkenRectangle((uint16_t)sprIdx, (uint16_t)(int16_t)sx,
                          (uint16_t)(int16_t)sy);
    }

    /* Read next entry after sprite */
    d0 = *(uint32_t *)a0;
    a0 += 4;
    if (d0 == 0xFFFFFFFF)
        goto end_replay;
    /* fall through to check_next */

check_next:
    /* loc_10C4D0: check type of next entry */
    if (!(d0 & (1u << 31)))
        goto sprite_decode;  /* Another sprite — loop */

    /* Next frame-start marker — back up, save pointer for next call */
    a0 -= 4;
    a0 = wrapPointerBack(a0);
    currentGoalPtr = a0;
    goto after_decode;

end_replay:
    /* loc_10C4C4 */
    replayState = 0;
    return;

after_decode:
    /* loc_10C4E2: overlay drawing */

    /* Show replay icon (spinning R) when NOT in normal replay */
    if (normalReplayRunning == 0) {
        uint16_t frame = (currentGameTick >> 1) & 0x1F;
        _CDarkenRectangle(SPR_REPLAY_FRAME_00 + frame, 0xB, 0xE);
    }

    /* Show score overlay during normal replay */
    if (normalReplayRunning != 0) {
        uint16_t bx = 17;   /* d1 = 0xD + 4 */
        uint16_t by = 17;   /* d2 = 0xD + 4 */

        /* Left team goals (using SPR_BIG_0 digit sprites) */
        uint16_t leftGoals = hilLeftTeamGoals;
        uint16_t tens = leftGoals / 10;
        uint16_t units = leftGoals % 10;

        if (tens != 0) {
            _CDarkenRectangle(SPR_BIG_0 + tens, bx, by);
            bx += 12;
        }
        _CDarkenRectangle(SPR_BIG_0 + units, bx, by);
        bx += 15;
        by += 8;

        /* Dash separator (sprite 0xC3 = 195) */
        _CDarkenRectangle(0xC3, bx, by);
        by -= 8;
        bx += 8;

        /* Right team goals (using SPR_BIG_0_ALT digit sprites) */
        uint16_t rightGoals = hilRightTeamGoals;
        tens = rightGoals / 10;
        units = rightGoals % 10;

        if (tens != 0) {
            _CDarkenRectangle(SPR_BIG_0_ALT + tens, bx, by);
            bx += 12;
        }
        _CDarkenRectangle(SPR_BIG_0_ALT + units, bx, by);
    }

    /* State 3 timing: wait 2 game ticks when not paused */
    if (replayState == 3 && paused == 0) {
        uint16_t i;
        for (i = 0; i < 2; i++) {
            volatile uint16_t *tickPtr = &currentGameTick;
            uint16_t tick = *tickPtr;
            while (*tickPtr == tick)
                ;  /* busy-wait for next tick */
        }
    }
}

/* ============================================================
 * _ReplayUpdateCameraAndSwapBuffers
 * ============================================================ */
__attribute__((used, externally_visible))
void _ReplayUpdateCameraAndSwapBuffers(int16_t targetX, int16_t targetY)
{
    _SetCameraCoordinates(targetX, targetY);
    _InitCameraMoveView(targetX, targetY);
    _SwapBuffersAndCameraViewState();
    _InitCameraMoveView(targetX, targetY);
}

/* ============================================================
 * _HandleInstantReplayStateSwitch
 * ============================================================ */
__attribute__((used, externally_visible))
void _HandleInstantReplayStateSwitch(void)
{
    if (replayState == 0) {
        /* Search for frame-start marker in ring buffer */
        uint32_t count = 0;
        uint8_t *p = currentHilPtr;

        for (;;) {
            if (count == HIL_SCENE_BYTES)
                return;  /* Not found */
            count += 4;
            p += 4;
            if (p == nextGoalPtr)
                p = goalBasePtr;
            if (*p & 0x80) {
                /* Found frame start */
                currentGoalPtr = p;
                replayState = 1;
                return;
            }
        }
    }

    if (replayState == 3) {
        replayState = 2;
        return;
    }
    if (replayState == 2) {
        replayState = 3;
        return;
    }
}

/* ============================================================
 * _RestoreCameraPosition
 * ============================================================ */
__attribute__((used, externally_visible))
void _RestoreCameraPosition(void)
{
    replayState = 0;
    int16_t x = savedCameraX;
    int16_t y = savedCameraY;
    _ResetAnimatedPatternsForBothTeams();
    _ReplayUpdateCameraAndSwapBuffers(x, y);
}
