/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/*
 * highlight.c — Highlight/Replay save system
 * Ported from SWOS2.s (Chunk 37)
 */

#include "highlight.h"
#include "game_types.h"
#include "deterministic.h"
#include "panic.h"
#include "endian_helpers.h"
#include "display.h"    /* WIDE_MARGIN — centre the replay overlay in widescreen (0 on Amiga) */

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
extern volatile uint16_t _numScenesInHilFile;
extern uint16_t _gamePitchTypeOrSeason;
extern int16_t  _gamePitchType;
extern uint16_t _gameSeason;
extern uint16_t _currentGameTick;
extern uint16_t _plgD0Param;
extern TeamGame _topTeamInGame;
extern TeamGame _bottomTeamInGame;
extern uint16_t _trainingGame;
extern uint16_t _pitchNumberAndType;
extern uint16_t _hilNumGoals;
extern uint16_t _hilPitchNumber;
extern uint8_t  _hilFileBuffer[];
extern uint8_t  _gameName[];
extern uint8_t  _gameRound[];
extern uint8_t  _hilGameName[];
extern uint8_t  _hilGameRound[];
extern uint16_t _hilTeam1Goals;
extern uint16_t _hilTeam2Goals;
extern uint16_t _statsTeam1Goals;
extern uint16_t _statsTeam2Goals;
extern uint16_t _gameMaxSubstitutes;
extern uint16_t _hilMaxSubstitutes;
extern uint16_t _normalReplayRunning;
extern uint8_t *_goalBasePtr;
extern uint8_t *_nextGoalPtr;
extern uint8_t *_currentHilPtr;
extern uint8_t *_currentGoalPtr;
extern volatile uint16_t _goIntoHighlightsLoop;
extern uint16_t _hilSceneNumber;
extern uintptr_t _savedCurrentHilPtr;
extern uintptr_t _savedCurrentGoalPtr;
extern uintptr_t _savedGoalBasePtr;
extern uintptr_t _savedNextGoalPtr;
extern uint8_t  _hilStartOfGoalsBuffer[];
extern uint8_t  _inGameHilBuffer[];
extern uint16_t _throwInDeadVar;
extern uint16_t _team1TotalGoals;
extern uint16_t _team2TotalGoals;
extern uint16_t _animPatternsState;
extern uint16_t _replayState;
extern FixedPoint _cameraX;
extern FixedPoint _cameraY;
extern int16_t  _savedCameraX;
extern int16_t  _savedCameraY;
extern uint16_t _hilLeftTeamGoals;
extern uint16_t _hilRightTeamGoals;
extern uint16_t _paused;

/* ---- External functions ---- */
extern uint16_t Rand(void);
extern void FadeOutToBlack(void);
extern void FadeIn(void);
extern void SetCameraCoordinates(int16_t targetX, int16_t targetY);
extern void InitCameraMoveView(int16_t targetX, int16_t targetY);
extern void SwapBuffersAndCameraViewState(void);
extern void MoveView(int16_t newScreenX, int16_t newScreenY);
extern void DarkenRectangle(uint16_t sprIdx, uint16_t x, uint16_t y);
extern void ResetAnimatedPatternsForBothTeams(void);
extern void DrawAnimatedPatterns(void);
extern uint16_t _panicReasonCode;

/* Per-frame X shift that re-anchors recorded screen-space sprite coordinates
 * (which have the recording camera baked in) onto the current playback camera.
 * Set by ReplayCameraX each frame; applied in the sprite decode below. */
static int16_t s_replayShiftX = 0;
extern int _replayVertMargin;            /* DOS-replay vertical letterbox (dos_replay_load.c) */
extern volatile uint16_t _playHighlightFlag;  /* set only while a .HIL replay is active */

/* ---- Replay camera X (SDL widescreen) ----------------------------------
 * Recorded HIL/replay camera positions are in the classic 336-wide coordinate
 * space (scrolled to keep the ball centred in a 336px view). The pitch is 0x2A0
 * (672) wide, so a view of width W has only (672 - W) px of camera travel.
 *   - Near-full-pitch view (ultrawide, tiny pan range): following the recorded
 *     path only SLIDES the static scenery (net, stands) by the pan-range width
 *     — there's nothing off-screen to follow to. Lock to the pan-range centre:
 *     dead-stable, pitch-centred.
 *   - Narrower widescreen: the camera still needs to track play, so re-centre
 *     the recorded 336-space path for the view (the per-mode adjustment SWOS
 *     2020 applies: subtract WIDE_MARGIN = (_screen_width - SCREEN_WIDTH)/2) and
 *     clamp to the pan range.
 * No-op on Amiga / 4:3 (pan range == the classic range, WIDE_MARGIN == 0). */
static int16_t ReplayCameraX(int16_t recordedX)
{
#ifdef SWOS_SDL_BUILD
    int16_t panRange = (int16_t)(0x2A0 - _screen_width);
    if (panRange < 0) panRange = 0;
    int16_t out;
    if (panRange <= 64) {
        out = (int16_t)(panRange / 2);                 /* whole pitch fits -> lock */
    } else {
        out = (int16_t)(recordedX - (int16_t)WIDE_MARGIN);
        if (out < 0) out = 0;
        else if (out > panRange) out = panRange;        /* re-centre + follow */
    }
    /* Recorded sprites are in screen space relative to the recording camera;
     * shift them by (recorded - playback) so they re-anchor onto our playback
     * camera. A stationary world object has a constant world position, so its
     * shift cancels the recording camera -> it no longer slides. The recorded
     * coords already sit in the centred 336 region of a wide view (WIDE_MARGIN),
     * so subtract it to avoid double-counting the margin (otherwise everything
     * lands WIDE_MARGIN px too far right). */
    s_replayShiftX = (int16_t)(recordedX - out - WIDE_MARGIN);
    return out;
#else
    return recordedX;
#endif
}

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
    if (p == _nextGoalPtr)
        p = _goalBasePtr;
    return p;
}

/* Wrap pointer backward in ring buffer (matches ValidateHilPointerSub) */
static uint8_t *wrapPointerBack(uint8_t *p)
{
    if (p < _goalBasePtr)
        p = _nextGoalPtr;
    return p;
}

/* Wrap a1-style pointer (IncrementHilBufferPointer) — same as wrapPointerFwd */
static uint8_t *wrapHilPtr(uint8_t *p)
{
    if (p == _nextGoalPtr)
        p = _goalBasePtr;
    return p;
}

/* ============================================================
 * SetPitchTypeAndNumber
 * ============================================================ */
__attribute__((used, externally_visible))
void SetPitchTypeAndNumber(void)
{
    _numScenesInHilFile = 0;

    int16_t _pitchType;
    const uint8_t *probs;
    int needChoose = 0;

    if (_gamePitchTypeOrSeason == 0) {
        /* Fixed pitch type mode */
        _pitchType = _gamePitchType;
        if (_pitchType < 0) {
            /* Negative = auto-select from standard probabilities */
            probs = pitchTypeProbs;
            needChoose = 1;
        }
    } else {
        /* Seasonal pitch type mode */
        probs = pitchTypeSeasonalProbs + (uint16_t)_gameSeason * 7;
        needChoose = 1;
    }

    if (needChoose) {
        /* Random selection based on cumulative probabilities */
        uint16_t r = Rand();
        uint8_t d0 = (uint8_t)((uint32_t)r * 100 / 256);
        _pitchType = 0;
        while (d0 >= *probs) {
            d0 -= *probs++;
            _pitchType++;
        }
    }

    /* Determine pitch number.
     * Note: asm has 3 dead instructions here (currentGameTick & 0x10 << 4)
     * that get immediately overwritten in both branches. Omitted. */
    uint16_t pitchNum;
    if (_plgD0Param != 0) {
        pitchNum = Rand() & 0xF;
    } else {
        /* Derive from team data */
        uint16_t v = *(uint16_t *)_topTeamInGame.teamName;
        v ^= (uint16_t)_topTeamInGame.prShirtCol;
        v ^= (uint16_t)_topTeamInGame.prShortsCol;
        pitchNum = v & 0xF;
    }

    uint16_t d0 = (uint16_t)pitchNumProbs[pitchNum] << 8;

    if (_trainingGame != 0)
        d0 = 0x500;

    _pitchNumberAndType = (uint16_t)_pitchType + d0;

#ifdef DETERMINISTIC_MODE
    /* Force fixed pitch to ensure ASM and C versions match */
    pitchNumberAndType = 0x0004;  /* pitch 0, type 4 (normal) */
#endif
}

/* ============================================================
 * CopyTeamsAndHeader
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void CopyTeamsAndHeader(void)
{
    _numScenesInHilFile = _hilNumGoals;
    _pitchNumberAndType = _hilPitchNumber;

    /* Copy team data from hil file buffer (offset 8) */
    uint8_t *src = &_hilFileBuffer[8];
    uint16_t i;
    for (i = 0; i < 1018; i++) {
        ((uint8_t *)&_bottomTeamInGame)[i] = src[1018 + i];
        ((uint8_t *)&_topTeamInGame)[i] = src[i];
    }

    /* The TeamGame header has 11 int16 fields at offsets 0..21 (kit
     * colours + markedPlayer). They're stored big-endian on disk to
     * match Amiga m68k; byteswap to host order so prShirtType etc.
     * index correctly into the little-player sprite table when the
     * stadium menu draws. The remaining 996 bytes are byte-sized
     * (teamName + per-player fields) and don't need swapping.
     * BE16_TO_HOST is a no-op on the Amiga BE build. */
    {
        int16_t *topHdr = (int16_t *)&_topTeamInGame;
        int16_t *botHdr = (int16_t *)&_bottomTeamInGame;
        for (i = 0; i < 11; i++) {
            topHdr[i] = BE16_TO_HOST((uint16_t)topHdr[i]);
            botHdr[i] = BE16_TO_HOST((uint16_t)botHdr[i]);
        }
    }

    /* Copy game name and round */
    for (i = 0; i < 40; i++) {
        _gameName[i] = _hilGameName[i];
        _gameRound[i] = _hilGameRound[i];
    }
}

/* ============================================================
 * SetHilFileHeader
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void SetHilFileHeader(void)
{
    /* HIL header is big-endian on disk to match Amiga m68k. BE_WRITE_U32
     * is a no-op on Amiga BE host, byteswap on x86 LE — so saves from the
     * SDL build are now Amiga-compatible. */
    BE_WRITE_U32(_hilFileBuffer, K_HIL_FILE_SIG);
    BE_WRITE_U32(&_hilFileBuffer[4], K_HIL_FILE_MAXSZ);

    /* Pack team data into hil file buffer at offset 8. The first 22 bytes
     * of each TeamGame are 11 int16 kit/marker fields — byteswap host→BE
     * as we copy so the on-disk format matches Amiga m68k. The remaining
     * 996 bytes are byte-sized and copy verbatim. */
    uint8_t *dst = &_hilFileBuffer[8];
    uint16_t i;
    {
        int16_t *topHdr = (int16_t *)&_topTeamInGame;
        int16_t *botHdr = (int16_t *)&_bottomTeamInGame;
        for (i = 0; i < 11; i++) {
            ((uint16_t *)dst)[i]            = HOST_TO_BE16((uint16_t)topHdr[i]);
            ((uint16_t *)(dst + 1018))[i]   = HOST_TO_BE16((uint16_t)botHdr[i]);
        }
    }
    for (i = 22; i < 1018; i++) {
        dst[1018 + i] = ((uint8_t *)&_bottomTeamInGame)[i];
        dst[i] = ((uint8_t *)&_topTeamInGame)[i];
    }

    /* Copy game name and round */
    for (i = 0; i < 40; i++) {
        _hilGameName[i] = _gameName[i];
        _hilGameRound[i] = _gameRound[i];
    }

    _hilNumGoals = _numScenesInHilFile;

    _hilTeam1Goals = _statsTeam1Goals;
    _hilTeam2Goals = _statsTeam2Goals;
    _hilPitchNumber = _pitchNumberAndType;
    _hilMaxSubstitutes = _gameMaxSubstitutes;
}

/* ============================================================
 * SaveHighlightsPointers
 * ============================================================ */
__attribute__((used, externally_visible))
void SaveHighlightsPointers(void)
{
    if (_numScenesInHilFile == 0)
        return;

    _normalReplayRunning = 1;
    _savedGoalBasePtr = (uintptr_t)_goalBasePtr;
    _savedNextGoalPtr = (uintptr_t)_nextGoalPtr;
    _savedCurrentHilPtr = (uintptr_t)_currentHilPtr;
    _savedCurrentGoalPtr = (uintptr_t)_currentGoalPtr;
    _goIntoHighlightsLoop = 1;
    _hilSceneNumber = 0xFFFF;
}

/* ============================================================
 * RestoreHighlightPointers
 * ============================================================ */
__attribute__((used, externally_visible))
void RestoreHighlightPointers(void)
{
    _normalReplayRunning = 0;
    _goalBasePtr = (uint8_t *)_savedGoalBasePtr;
    _nextGoalPtr = (uint8_t *)_savedNextGoalPtr;
    _currentHilPtr = (uint8_t *)_savedCurrentHilPtr;
    _currentGoalPtr = (uint8_t *)_savedCurrentGoalPtr;
    _goIntoHighlightsLoop = 0;
}

/* ============================================================
 * FadeAndSaveHighlightScene
 * ============================================================ */
__attribute__((used, externally_visible))
void FadeAndSaveHighlightScene(void)
{
    FadeOutToBlack();
    SaveHighlightScene();
    FadeIn();
}

/* ============================================================
 * SaveHighlightScene
 * Saves current replay ring buffer segment to the hil goals buffer.
 * Maximum HIL_MAX_SCENES scenes. If buffer is full, shifts scenes
 * 1..9 down to 0..8, freeing the last slot.
 * ============================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void SaveHighlightScene(void)
{
    /* Asm: move.w #10,d0; beq locret — the beq is dead (10 != 0). Omitted. */

    if (_numScenesInHilFile == HIL_MAX_SCENES) {
        /* Buffer full — shift scenes 1..9 into slots 0..8 */
        uint32_t *src = (uint32_t *)_inGameHilBuffer;
        uint32_t *dst = (uint32_t *)_hilStartOfGoalsBuffer;
        int16_t block;
        uint16_t j;
        for (block = 8; block >= 0; block--) {
            for (j = 0; j < HIL_SCENE_LONGS; j++) {
                *dst++ = *src++;
            }
        }
        _numScenesInHilFile--;
    }

    /* Scan forward from currentHilPtr for a frame-start marker (bit 7 of first byte set).
     * Scan at most 19000 bytes (4750 longs). */
    uint32_t count = 0;
    uint8_t *p = _currentHilPtr;

    for (;;) {
        if (count == HIL_SCENE_BYTES)
            return;  /* Not found within range */
        count += 4;
        p += 4;
        if (p == _nextGoalPtr)
            p = _goalBasePtr;
        if (*p & 0x80)
            break;  /* Found frame-start marker */
    }

    /* Copy 19000 bytes from ring buffer (starting at p) into goals buffer slot */
    uint32_t *dest = (uint32_t *)(_hilStartOfGoalsBuffer +
                                   (uint32_t)_numScenesInHilFile * HIL_SCENE_BYTES);
    uint16_t i;
    for (i = 0; i < HIL_SCENE_LONGS; i++) {
        *dest++ = *(uint32_t *)p;
        p += 4;
        if (p == _nextGoalPtr)
            p = _goalBasePtr;
    }
    _numScenesInHilFile++;
}

/* ============================================================
 * ThrowInDeadProc
 * ============================================================ */
__attribute__((used, externally_visible))
void ThrowInDeadProc(void)
{
    _throwInDeadVar = 1;
}

/* ============================================================
 * SaveCoordinatesForHighlights
 * ============================================================ */
__attribute__((used, externally_visible))
void SaveCoordinatesForHighlights(int16_t spriteIndexOrMarker, uint16_t x, uint16_t y)
{
    uint8_t *a1 = _currentHilPtr;

    if (spriteIndexOrMarker >= 0) {
        /* Pack sprite data: (index << 20) | (x << 10) | y, with bit 31 clear.
         * Stored big-endian so the high byte (which holds bit 31, the
         * frame-marker discriminator vs sprite-data) lands at offset 0
         * regardless of host endianness — matching the Amiga byte layout. */
        uint32_t packed = (uint32_t)(uint16_t)spriteIndexOrMarker;
        packed <<= 10;
        packed |= (x & 0x3FF);
        packed <<= 10;
        packed |= (y & 0x3FF);
        packed &= ~(1u << 31);  /* bclr #31 */
        BE_WRITE_U32(a1, packed);
        a1 += 4;
        if (a1 == _nextGoalPtr)
            a1 = _goalBasePtr;
        _currentHilPtr = a1;
        return;
    }

    if ((uint16_t)spriteIndexOrMarker == 0xFFFE) {
        /* End-of-segment marker: write -1 (byte pattern is identical
         * either endian, but BE_WRITE_U32 keeps the policy consistent). */
        BE_WRITE_U32(a1, 0xFFFFFFFFu);
        return;
    }

    /* Frame-start marker (d0 < 0, not -2): write camera coords with bit 31
     * (= bit 15 of high uint16 = marker) set. Two BE_WRITE_U16 calls match
     * the original Amiga byte format exactly — on Amiga m68k the BE_WRITE
     * macros are no-ops so this is identical to the prior asm-to-C port,
     * and on x86 LE the byte-swap puts the 0x80 marker byte at offset 0
     * where HandleInstantReplayStateSwitch's `*p & 0x80` scan and
     * DrawReplayFrame's BE-uint32 bit-31 test both detect it. */
    BE_WRITE_U16(a1,     (uint16_t)x | 0x8000u);
    BE_WRITE_U16(a1 + 2, (uint16_t)y);
    a1 += 4;
    a1 = wrapHilPtr(a1);
    _currentHilPtr = a1;

    /* Write team goals (BE for cross-build .HIL file interchange) */
    BE_WRITE_U16(a1,     _team1TotalGoals);
    BE_WRITE_U16(a1 + 2, _team2TotalGoals);
    a1 += 4;
    a1 = wrapHilPtr(a1);

    /* Write animated patterns state (duplicated in both words) */
    BE_WRITE_U16(a1,     _animPatternsState);
    BE_WRITE_U16(a1 + 2, _animPatternsState);
    a1 += 4;
    a1 = wrapHilPtr(a1);
    _currentHilPtr = a1;

    /* Write end-of-frame marker */
    BE_WRITE_U32(a1, 0xFFFFFFFFu);
}

/* ============================================================
 * DrawReplayFrame
 * Complex replay frame playback state machine.
 * Goto labels map 1:1 to asm labels:
 *   sprite_decode  = loc_10C494
 *   check_next     = loc_10C4D0
 *   end_replay     = loc_10C4C4
 *   after_decode   = loc_10C4E2
 * ============================================================ */
__attribute__((used, externally_visible))
void DrawReplayFrame(void)
{
    uint8_t *a0 = _currentGoalPtr;
    uint32_t d0;

    if (_replayState == 1) {
        /* STATE 1 → 2: Initialize replay */
        _replayState = 2;
        _savedCameraX = _cameraX.s.i;
        _savedCameraY = _cameraY.s.i;

        /* Read first frame data (without advancing a0) for camera init.
         * Buffer is stored big-endian — BE_READ_U32 is a no-op on Amiga
         * and a byte-swap on x86 LE. */
        uint32_t val = BE_READ_U32(a0);
        val &= ~(1u << 31);  /* bclr #31 */

        ResetAnimatedPatternsForBothTeams();
        ReplayUpdateCameraAndSwapBuffers((int16_t)(val >> 16),
                                          (int16_t)(val & 0xFFFF));
    }

    /* loc_10C434: read entry */
    d0 = BE_READ_U32(a0);
    a0 += 4;
    if (!(d0 & (1u << 31)))
        goto sprite_decode;

    /* Frame-start marker processing */
    a0 = wrapPointerFwd(a0);
    d0 &= ~(1u << 31);
    MoveView(ReplayCameraX((int16_t)(d0 >> 16)), (int16_t)(d0 & 0xFFFF));

    /* Read team goals */
    _hilLeftTeamGoals = BE_READ_U16(a0);
    _hilRightTeamGoals = BE_READ_U16(a0 + 2);
    a0 += 4;
    a0 = wrapPointerFwd(a0);

    /* Read animated patterns state (stored as two identical words) */
    _animPatternsState = BE_READ_U16(a0);
    _animPatternsState = BE_READ_U16(a0 + 2);
    a0 += 4;
    a0 = wrapPointerFwd(a0);

    DrawAnimatedPatterns();

    /* Read next entry after frame marker */
    d0 = BE_READ_U32(a0);
    a0 += 4;
    if (d0 == 0xFFFFFFFFu)
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
#ifdef SWOS_SDL_BUILD
        sx += s_replayShiftX;     /* re-anchor recorded screen coords onto playback camera */
        sy += (_playHighlightFlag ? _replayVertMargin : 0);  /* letterbox DOS (200-tall) replays */
#endif
        DarkenRectangle((uint16_t)sprIdx, (uint16_t)(int16_t)sx,
                          (uint16_t)(int16_t)sy);
    }

    /* Read next entry after sprite */
    d0 = BE_READ_U32(a0);
    a0 += 4;
    if (d0 == 0xFFFFFFFFu)
        goto end_replay;
    /* fall through to check_next */

check_next:
    /* loc_10C4D0: check type of next entry */
    if (!(d0 & (1u << 31)))
        goto sprite_decode;  /* Another sprite — loop */

    /* Next frame-start marker — back up, save pointer for next call */
    a0 -= 4;
    a0 = wrapPointerBack(a0);
    _currentGoalPtr = a0;
    goto after_decode;

end_replay:
    /* loc_10C4C4 */
    _replayState = 0;
    return;

after_decode:
    /* loc_10C4E2: overlay drawing */

    /* Show replay icon (spinning R) when NOT in normal replay. Add WIDE_MARGIN so the icon
     * tracks the centred HUD in widescreen instead of sitting at the screen's left edge. */
    if (_normalReplayRunning == 0) {
        uint16_t frame = (_currentGameTick >> 1) & 0x1F;
        DarkenRectangle(SPR_REPLAY_FRAME_00 + frame, 0xB + WIDE_MARGIN, 0xE);
    }

    /* Show score overlay during normal replay (same playfield-relative offset). */
    if (_normalReplayRunning != 0) {
        uint16_t bx = 17 + WIDE_MARGIN;   /* d1 = 0xD + 4 */
        uint16_t by = 17;                 /* d2 = 0xD + 4 */

        /* Left team goals (using SPR_BIG_0 digit sprites) */
        uint16_t leftGoals = _hilLeftTeamGoals;
        uint16_t tens = leftGoals / 10;
        uint16_t units = leftGoals % 10;

        if (tens != 0) {
            DarkenRectangle(SPR_BIG_0 + tens, bx, by);
            bx += 12;
        }
        DarkenRectangle(SPR_BIG_0 + units, bx, by);
        bx += 15;
        by += 8;

        /* Dash separator (sprite 0xC3 = 195) */
        DarkenRectangle(0xC3, bx, by);
        by -= 8;
        bx += 8;

        /* Right team goals (using SPR_BIG_0_ALT digit sprites) */
        uint16_t rightGoals = _hilRightTeamGoals;
        tens = rightGoals / 10;
        units = rightGoals % 10;

        if (tens != 0) {
            DarkenRectangle(SPR_BIG_0_ALT + tens, bx, by);
            bx += 12;
        }
        DarkenRectangle(SPR_BIG_0_ALT + units, bx, by);
    }

    /* State 3 timing: stretch each replay frame across 2 extra ticks
     * (≈ half-speed slomo). On Amiga the busy-spin on `currentGameTick`
     * was harmless because the VBlank ISR ticked it AND drove audio
     * pre-emptively. On SDL the tick is advanced by the SDL_Timer
     * thread, but audio production runs on the *main* thread inside
     * Platform_VBlank → AudioSDL_Tick. A pure busy-spin on the main
     * thread starves the audio queue for 40 ms per replay frame,
     * which is exactly the stutter testers hear during R-then-R
     * slomo. Use Platform_VBlank instead so we both pace at 50 Hz AND
     * keep AudioSDL_Tick firing on every wait iteration. */
    if (_replayState == 3 && _paused == 0) {
#ifdef SWOS_SDL_BUILD
        extern int Platform_VBlank(void);
        Platform_VBlank();
        Platform_VBlank();
#else
        uint16_t i;
        for (i = 0; i < 2; i++) {
            volatile uint16_t *tickPtr = &_currentGameTick;
            uint16_t tick = *tickPtr;
            while (*tickPtr == tick)
                ;  /* busy-wait for next tick */
        }
#endif
    }
}

/* ============================================================
 * ReplayUpdateCameraAndSwapBuffers
 * ============================================================ */
__attribute__((used, externally_visible))
void ReplayUpdateCameraAndSwapBuffers(int16_t targetX, int16_t targetY)
{
    targetX = ReplayCameraX(targetX);
    SetCameraCoordinates(targetX, targetY);
    InitCameraMoveView(targetX, targetY);
    SwapBuffersAndCameraViewState();
    InitCameraMoveView(targetX, targetY);
}

/* ============================================================
 * HandleInstantReplayStateSwitch
 * ============================================================ */
__attribute__((used, externally_visible))
void HandleInstantReplayStateSwitch(void)
{
    if (_replayState == 0) {
        /* Search for frame-start marker in ring buffer.
         * Entries are stored big-endian (see frame-marker write in
         * SaveCoordinatesForHighlights), so the high byte of the uint32
         * — which carries bit 31 — sits at byte offset 0 regardless of
         * host endian. `*p & 0x80` therefore detects the marker on both
         * BE (Amiga) and LE (SDL) hosts. */
        uint32_t count = 0;
        uint8_t *p = _currentHilPtr;

        for (;;) {
            if (count == HIL_SCENE_BYTES)
                return;  /* Not found */
            count += 4;
            p += 4;
            if (p == _nextGoalPtr)
                p = _goalBasePtr;
            if (*p & 0x80) {
                /* Found frame start */
                _currentGoalPtr = p;
                _replayState = 1;
                return;
            }
        }
    }

    if (_replayState == 3) {
        _replayState = 2;
        return;
    }
    if (_replayState == 2) {
        _replayState = 3;
        return;
    }
}

/* ============================================================
 * RestoreCameraPosition
 * ============================================================ */
__attribute__((used, externally_visible))
void RestoreCameraPosition(void)
{
    _replayState = 0;
    int16_t x = _savedCameraX;
    int16_t y = _savedCameraY;
    ResetAnimatedPatternsForBothTeams();
    ReplayUpdateCameraAndSwapBuffers(x, y);
}
