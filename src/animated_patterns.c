/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
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

#include "swos_types.h"
#include "animated_patterns.h"
#include "game_types.h"
#include "display.h"

/* ---- external asm globals ---- */
extern uint16_t _replayState;
extern uint16_t _animPatternsState;
extern uint16_t _currentGameTick;
extern uint16_t _trainingGame;
extern uint16_t _goalCounter;
extern uint16_t _patternsGoalCounter;
extern uint16_t _showFansCounter;
extern uint16_t _animPatternsIndex;
extern FixedPoint _cameraX;
extern FixedPoint _cameraY;
extern uint16_t _gameStatePl;
extern uint16_t _gameState;
extern uint16_t _fansSidesSwapped;
extern uint16_t _panicReasonCode;
extern int16_t  _animatedPatterns[];
extern TeamGeneralInfo *_lastTeamPlayedBeforeBreak;
extern TeamGame *_teamScoredGamePtr;
extern TeamGame _topTeamInGame;
extern TeamGame _bottomTeamInGame;
extern uint8_t  *_spriteGraphicsPtr;
extern uint8_t  _pitchDatBuffer[];
extern volatile uintptr_t _frontbufferBasePtr;
extern uint8_t  _frontbufferMaskBuffer[];
extern uint8_t  _currentMenu[];
#ifdef SWOS_SDL_BUILD
extern uint8_t  _pitchChunkyBuffer[];
/* On SDL the pitch tile pointers live in pitchChunkyBuffer with 256-byte
 * stride per tile (vs Amiga's 128-byte bitplane stride). Pitch.c arranges
 * chunky tiles to be indexed by the *original* Amiga tile index
 * (offset / 128), so the same `(entry - base) / stride` math still
 * recovers the tile-pair index used by the static/animated swap. */
#define ANIM_TILE_BASE        ((uintptr_t)_pitchChunkyBuffer)
#define ANIM_TILE_SIZE        256u
#define ANIM_TILE_SIZE_SHIFT  8
#define ANIM_MAX_TILE_OFFSET  (MAX_FAN_TILE_INDEX * ANIM_TILE_SIZE)
#else
/* The original Amiga aliased `pitchDatBuffer` and `currentMenu` to the
 * same RAM block, so SWOS2.s could relocate each pitch slot by adding
 * `currentMenu` and the tile-data region happened to start exactly at
 * `currentMenu + PITCH_POINTER_COUNT*4` (= 9240). Our C build keeps the
 * two buffers separate, and the pitch.c relocation pass uses
 * `pitchDatBuffer + PITCH_POINTER_COUNT*4` as the slot base. The fan-
 * animation toggle here must use the SAME base, otherwise
 * `*a0 - base` is a random non-offset value, the static/animated
 * check fails, and the crowd never animates. */
#define ANIM_PITCH_POINTER_COUNT 2310u
#define ANIM_TILE_BASE        ((uint32_t)(uintptr_t)(_pitchDatBuffer + ANIM_PITCH_POINTER_COUNT * 4))
#define ANIM_TILE_SIZE        TILE_SIZE
#define ANIM_TILE_SIZE_SHIFT  TILE_SIZE_SHIFT
#define ANIM_MAX_TILE_OFFSET  MAX_FAN_TILE_OFFSET
#endif
/* animPatternsRandomTable: 64-byte permutation table (entries 0..62
 * plus a trailing 0) that `DrawAnimatedPatterns` walks to pick pitch
 * tile indices for the per-frame fan animations. The table is shuffled
 * in-place by `swapTwoRandomIndicesFromAnimPatternsTable`. Transcribed
 * from the Amiga C port's Data.c:2221 — with a zero stub, every pick
 * was index 0 so the same tile "animated" forever and the rest never
 * moved. */
uint8_t _animPatternsRandomTable[64] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,  0
};
extern uint16_t _patternStatic;
extern uint16_t _otherTeamScored;


/* ---- external functions ---- */
extern uint16_t Rand(void);
extern void PanicHaltLoop(void);
extern void DrawSprite16Pixels(uint16_t spriteIndex, int16_t x, int16_t y);

/* ================================================================
 * swapTwoRandomIndicesFromAnimPatternsTable
 * (SWOS2.s line 53512)
 * Shuffles two random entries in the 64-byte permutation table.
 * ================================================================ */
static void swapTwoRandomIndicesFromAnimPatternsTable(void)
{
    uint16_t r1, r2;
    uint8_t tmp;

    r1 = (uint16_t)((Rand() * 63) >> 8);
    r2 = (uint16_t)((Rand() * 63) >> 8);

    tmp = _animPatternsRandomTable[r2];
    _animPatternsRandomTable[r2] = _animPatternsRandomTable[r1];
    _animPatternsRandomTable[r1] = tmp;
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

    /* decompose patternIndex into row,col in a 42-column grid */
    pixelY = (int16_t)(patternIndex / 42) * 16;     /* row * 16 */
    pixelX = (int16_t)(patternIndex % 42) * 16;     /* col * 16 */

    /* Sprite 137 is the animated-fan placeholder: overwrite its
     * sprData each call with the current pitch-tile pointer, then draw
     * it at the cell's screen-space coordinates.
     *
     * Original code went through `(uint32_t *)spriteGraphicsPtr` and
     * `(uint32_t *)pitchDatBuffer` — both 4-byte-slot views. On 64-bit
     * the table slots are 8 bytes, so sprGfx[137] returned half a
     * SpriteGraphics pointer (zero-extended to an unmapped low-32
     * address) and the write through it segfaulted. Use the typed
     * pointer table for the sprite header, and the matching side table
     * for the chunky tile pointer (populated by FixupAndColorPitchFile
     * Step 1b, see pitch.c). */
#ifdef SWOS_SDL_BUILD
    {
        /* s_pitchFullPtrs is the pitch.c side table that holds the
         * full-width chunky-tile pointer per slot — populated in
         * FixupAndColorPitchFile Step 1b. The 4-byte slots in
         * pitchDatBuffer can't round-trip a 64-bit address. */
        extern uint8_t *_s_pitchFullPtrs[];
        SpriteGraphics *spr = ((SpriteGraphics **)_spriteGraphicsPtr)[137];
        if (spr && patternIndex < 2310) {
            uint8_t *tile = _s_pitchFullPtrs[patternIndex];
            if (tile) spr->sprData = tile;
        }
    }
#else
    {
        uint32_t *sprGfx     = (uint32_t *)(void *)_spriteGraphicsPtr;
        uint32_t *tileEntries = (uint32_t *)(void *)_pitchDatBuffer;
        uint32_t spriteAddr   = sprGfx[137];
        *(uint32_t *)spriteAddr = tileEntries[patternIndex];
    }
#endif

    /* compute screen-relative coordinates */
    screenX = pixelX - (int16_t)_cameraX.s.i;
    screenY = pixelY - (int16_t)_cameraY.s.i;

    /* bounds check. Widescreen draws fan cells across the full frontbuffer width, not just
     * the 4:3 336px window; g_screen_width == 336 on 4:3 / Amiga so this is unchanged there. */
#ifdef SWOS_SDL_BUILD
    if (screenX >= _g_screen_width || screenY >= SCREEN_HEIGHT)
#else
    if (screenX >= 336 || screenY >= SCREEN_HEIGHT)
#endif
        return;
    if (screenX <= (int16_t)0xFFF0 || screenY <= (int16_t)0xFFF0)
        return;

    /* draw to frontbuffer */
    DrawSprite16Pixels(137, screenX, screenY);

    /* draw to mask buffer (swap frontbuffer ptr temporarily). The
     * pointer slot is uintptr_t on both targets — save/restore at
     * native width so we don't truncate the swap. */
    {
        uintptr_t savedFbPtr = _frontbufferBasePtr;
        _frontbufferBasePtr = (uintptr_t)_frontbufferMaskBuffer;
        DrawSprite16Pixels(137, screenX, screenY);
        _frontbufferBasePtr = savedFbPtr;
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
    uint32_t baseAddr = ANIM_TILE_BASE;
#ifdef SWOS_SDL_BUILD
    extern uint8_t *_s_pitchFullPtrs[];
    extern uint8_t _pitchChunkyBuffer[];
    uint32_t *pitchBase = (uint32_t *)(void *)_pitchDatBuffer;
#endif

    for (i = count; i >= 0; i--) {
        uint32_t d0 = *a0 - baseAddr;
        if (d0 != 0 && d0 <= ANIM_MAX_TILE_OFFSET && !(d0 & ANIM_TILE_SIZE)) {
            /* animated variant (even tile index) → subtract one tile → static */
            *a0 -= ANIM_TILE_SIZE;
#ifdef SWOS_SDL_BUILD
            /* Mirror the toggle into s_pitchFullPtrs — see comment in
             * DrawAnimatedPatterns. The slot index is the byte offset
             * of `a0` from the pitchDatBuffer base divided by 4. */
            {
                ptrdiff_t slotIdx = a0 - pitchBase;
                uint32_t newTileIdx = (d0 - ANIM_TILE_SIZE) >> ANIM_TILE_SIZE_SHIFT;
                if (slotIdx >= 0 && slotIdx < 2310 && newTileIdx < 2310) {
                    _s_pitchFullPtrs[slotIdx] = _pitchChunkyBuffer + (size_t)newTileIdx * 256;
                }
            }
#endif
        }
        a0++;
    }

    /* clear animatedPatterns array */
    for (i = 0; i < 8; i++) {
        _animatedPatterns[i] = (int16_t)0xFFFF;
    }
}

/* ================================================================
 * ResetAnimatedPatternsForBothTeams
 * (SWOS2.s line 53547)
 *
 * Resets animated tiles for both top and bottom fan sections.
 * Top section: pitchDatBuffer + 0xA8/4 = entry 42, count 126
 * Bottom section: pitchDatBuffer + 0x2178/4 = entry 2142, count 126
 * ================================================================ */
void __attribute__((used, externally_visible))
ResetAnimatedPatternsForBothTeams(void)
{
    if (_trainingGame != 0)
        return;

    /* top fans section: offset 0xA8 bytes = 42 longs, 126 entries */
    resetAnimatedPatterns((uint32_t *)((uint8_t *)_pitchDatBuffer + 0xA8), 125);

    /* bottom fans section: offset 0x2178 bytes = 2142 longs, 126 entries */
    resetAnimatedPatterns((uint32_t *)((uint8_t *)_pitchDatBuffer + 0x2178), 125);
}

/* ================================================================
 * DrawAnimatedPatterns
 * (SWOS2.s line 53273)
 *
 * Per-frame animated pattern update. 8-state machine that determines
 * animation intensity based on game state (goals, ball position, etc.).
 * Selects random tiles from the pitch grid and toggles them between
 * static and animated states.
 * ================================================================ */
#ifdef SWOS_SDL_BUILD
/* drawFanMirror — widescreen-only fan-animation mirror.
 *
 * The base loop only toggles a 21-column window positioned at cameraX>>4 — the 4:3 view
 * width. When the full pitch is visible the right half never animates. For every cell the
 * loop touches we mirror it into the +21-column cell so the right half animates in lock-step.
 * do_toggle flips the mirror's OWN tile (fill path); the active path just redraws it. Both
 * bounds- and parity-checked WITHOUT PanicHaltLoop: the mirror can fall outside the 2310-slot
 * table or land on a non-fan tile near the pitch edge, so we skip rather than animate garbage.
 * Because the mirror is derived from d3 (which the base loop already tracks) and toggled with
 * the same d4, it stays in sync without its own animatedPatterns[] slot. */
static void drawFanMirror(int16_t baseIdx, int do_toggle, uint16_t d4)
{
    extern uint8_t *_s_pitchFullPtrs[];
    extern uint8_t  _pitchChunkyBuffer[];
    /* Mirror is +21 columns within the SAME 42-column row — compute via row/col, not a flat
     * +21, so a scrolled camera (16:9) can't spill the mirror into the next row's left edge. */
    int32_t row    = (int32_t)baseIdx / 42;
    int32_t colAbs = (int32_t)baseIdx % 42 + 21;
    int32_t mirrorIdx;

    if (colAbs >= 42)                  /* right mirror column is off the pitch edge */
        return;
    mirrorIdx = row * 42 + colAbs;
    if (mirrorIdx < 0 || mirrorIdx >= 2310)
        return;

    if (do_toggle) {
        uint32_t *p = ((uint32_t *)(void *)_pitchDatBuffer) + mirrorIdx;
        int32_t tileIndex =
            (int32_t)(((uint32_t)(*p - ANIM_TILE_BASE)) >> ANIM_TILE_SIZE_SHIFT);
        int32_t newIndex;

        if (tileIndex == 0 || tileIndex > MAX_FAN_TILE_INDEX)
            return;                   /* not an animatable fan tile here */
        if (tileIndex & 1) {
            if (d4 == 0) return;      /* static wanted, already static */
            newIndex = tileIndex + 1; /* odd (static) -> even (animated) */
        } else {
            if (d4 != 0) return;      /* animated wanted, already animated */
            newIndex = tileIndex - 1; /* even (animated) -> odd (static) */
        }
        *p = ANIM_TILE_BASE + ((uint32_t)newIndex << ANIM_TILE_SIZE_SHIFT);
        _s_pitchFullPtrs[mirrorIdx] = _pitchChunkyBuffer + (size_t)newIndex * 256;
    }
    drawPatternFromIndex((uint16_t)mirrorIdx);
}
#endif

void __attribute__((used, externally_visible))
DrawAnimatedPatterns(void)
{
    int16_t d6;       /* max patterns to update */
    uint16_t d0;      /* animate flag (0 = de-animate, non-zero = animate) */
    int16_t d7;       /* loop counter */
    uint32_t baseAddr = ANIM_TILE_BASE;
    (void)baseAddr;

    /* ---- state 0: replayState == 0 → no game ---- */
    if (_replayState == 0) {
        _animPatternsState = 0;

        if (_trainingGame != 0)
            return;

        /* track goal celebration timer */
        if (_patternsGoalCounter != 0) {
            _patternsGoalCounter++;
            if (_patternsGoalCounter >= 300)
                _patternsGoalCounter = 0;
        } else {
            if (_goalCounter != 0)
                _patternsGoalCounter = 1;
        }

        if (_showFansCounter != 0)
            _showFansCounter--;

        _otherTeamScored = 0;

        if (_showFansCounter != 0)
            goto game_hasnt_started;

        if (_gameStatePl == ST_GAME_IN_PROGRESS)
            goto game_in_progress;

        if (_gameState == ST_KEEPER_HOLDS_BALL) {
            TeamGeneralInfo *teamPtr = _lastTeamPlayedBeforeBreak;
            if (teamPtr != (void *)0) {
                Sprite *controlledPlayer = teamPtr->controlledPlayer;
                (void)controlledPlayer;
                /* asm: loads controlled player but then unconditionally falls through
                 * to game_in_progress regardless of whether it's null or not */
            }
            goto game_in_progress;
        }

        if (_patternsGoalCounter == 0)
            goto ball_out_of_play;

        {
            TeamGame *scoredTeamPtr = _teamScoredGamePtr;
            if ((int16_t)_cameraY.s.i > 440) {
                /* bottom of pitch */
                if (_fansSidesSwapped != 0)
                    goto check_top_team;
                goto check_bottom_team;
            } else {
                /* top of pitch */
                if (_fansSidesSwapped != 0)
                    goto check_bottom_team;
            check_top_team:
                if (scoredTeamPtr == &_topTeamInGame)
                    goto check_cheers_phase;
                goto other_team_scored_be_still;
            check_bottom_team:
                if (scoredTeamPtr == &_bottomTeamInGame)
                    goto check_cheers_phase;
                goto other_team_scored_be_still;
            }

        check_cheers_phase:
            if (_patternsGoalCounter < 25)
                goto greatest_cheers;
            if (_patternsGoalCounter < 200)
                goto middle_cheers;
            if (_patternsGoalCounter < 225)
                goto cheers_fading;
            /* patternsGoalCounter >= 225: fall through to ball_out_of_play */
        }
    } else {
        /* replayState != 0 */
        if (_animPatternsState == 0)
            return;

        _otherTeamScored = 0;

        switch (_animPatternsState) {
        case 1: goto ball_out_of_play;
        case 2: goto greatest_cheers;
        case 3: goto middle_cheers;
        case 4: goto other_team_scored_be_still;
        case 5: goto cheers_fading;
        case 6: goto game_hasnt_started;
        case 7: goto game_in_progress;
        default:
#ifdef SWOS_SDL_BUILD
            /* At match start the replay buffer hasn't been populated yet,
             * so the state word read from it may be garbage in (1..7).
             * Rather than panicking, treat unknown states as "no pattern
             * update" — matches the animPatternsState==0 early-return. */
            _animPatternsState = 0;
            return;
#else
            _panicReasonCode = 0x5665;
            PanicHaltLoop();
            return;
#endif
        }
    }

ball_out_of_play:
    _animPatternsState = 1;
    d6 = 5;
    d0 = _currentGameTick & 31;
    if (d0 <= 25)
        d0 = 0;
    goto pattern_loop;

greatest_cheers:
    _animPatternsState = 2;
    d6 = 8;
    d0 = 1;
    goto pattern_loop;

middle_cheers:
    _animPatternsState = 3;
    d6 = 8;
    d0 = _currentGameTick & 0xF;
    if (d0 <= 4)
        d0 = 0;
    goto pattern_loop;

other_team_scored_be_still:
    _animPatternsState = 4;
    d6 = 1;
    d0 = _currentGameTick & 31;
    _otherTeamScored = 1;
    if (d0 <= 15)
        d0 = 0;
    goto pattern_loop;

cheers_fading:
    _animPatternsState = 5;
    d6 = 8;
    d0 = 0;
    goto pattern_loop;

game_hasnt_started:
    _animPatternsState = 6;
    d6 = 8;
    d0 = _currentGameTick & 0x1F;
    if (d0 <= 0xF)
        d0 = 0;
    goto pattern_loop;

game_in_progress:
    _animPatternsState = 7;
    d6 = 1;
    d0 = _currentGameTick & 0x1F;
    if (d0 <= 0x19)
        d0 = 0;
    /* fall through */

pattern_loop:
    _patternStatic = d0;

    for (d7 = 7; d7 >= 0; d7--) {
        int16_t d3 = _animatedPatterns[7 - d7];

        if (d3 >= 0) {
            /* active pattern entry: draw once then clear */
            _animatedPatterns[7 - d7] = (int16_t)0xFFFF;
            drawPatternFromIndex((uint16_t)d3);
#ifdef SWOS_SDL_BUILD
            if (WIDE_MARGIN > 0)
                drawFanMirror(d3, 0, 0);   /* widescreen: redraw the right-half mirror too */
#endif
            continue;
        }

        /* empty slot (0xFFFF): try to fill if within d6 budget */
        if ((uint16_t)d6 <= (uint16_t)d7)
            continue;

        {
            uint16_t d4 = _patternStatic;
            int16_t randomVal;
            int16_t spriteIdx;
            int16_t row, col;
            int16_t pitchIdx;
            uint32_t *tileEntryPtr;
            uint32_t tileVal;
            int32_t tileOffset;
            int32_t tileIndex;

            _animPatternsIndex++;
            if (_animPatternsIndex >= 64)
                _animPatternsIndex = 0;

            /* check camera Y to determine which fan section */
            if ((int16_t)_cameraY.s.i < 64) {
                /* top of pitch: sprite range 21..83 */
                swapTwoRandomIndicesFromAnimPatternsTable();
                randomVal = (int8_t)_animPatternsRandomTable[_animPatternsIndex];

                /* if other team scored and random index is in [21,42), suppress animation */
                if (_otherTeamScored != 0 && randomVal >= 21 && randomVal < 42)
                    d4 = 0;

                spriteIdx = randomVal + 21;
            } else if ((int16_t)_cameraY.s.i >= 544) {
                /* bottom of pitch: sprite range 1071..1133 */
                swapTwoRandomIndicesFromAnimPatternsTable();
                randomVal = (int8_t)_animPatternsRandomTable[_animPatternsIndex];
                spriteIdx = randomVal + 1071;
            } else {
                /* middle of pitch: fans not visible, skip */
                continue;
            }

            /* decompose spriteIdx into row/col and compute pitchDatBuffer index */
            row = spriteIdx / 21;
            col = spriteIdx % 21;
            pitchIdx = row * 42 + col + ((int16_t)_cameraX.s.i >> 4);
            d3 = pitchIdx;

            /* bounds check: tileEntryPtr must be within the 9240-byte
             * pitch pointer table. The original asm used `currentMenu`
             * as the upper bound because Amiga BSS placed currentMenu
             * exactly at pitchDatBuffer+9240. On the C builds (SDL +
             * Amiga ports) the two symbols are independent BSS atoms
             * with linker-determined addresses, so check directly
             * against pitchDatBuffer + 9240 (= 2310 pointer-table
             * slots * 4 bytes each). Caught on aarch64 where the
             * linker happens to place currentMenu *before*
             * pitchDatBuffer, making the original check fail
             * unconditionally. */
            tileEntryPtr = ((uint32_t *)(void *)_pitchDatBuffer) + pitchIdx;
            if ((uintptr_t)tileEntryPtr < (uintptr_t)_pitchDatBuffer ||
                (uintptr_t)tileEntryPtr >= (uintptr_t)_pitchDatBuffer + 2310u * 4) {
                _panicReasonCode = 0x2772;
                PanicHaltLoop();
            }

            /* read tile entry and check if it's an animated tile */
            tileVal = *tileEntryPtr;
            tileOffset = (int32_t)(tileVal - ANIM_TILE_BASE);
            tileIndex = (int32_t)((uint32_t)tileOffset >> ANIM_TILE_SIZE_SHIFT);

            if (tileIndex == 0 || tileIndex > MAX_FAN_TILE_INDEX)
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

                _animatedPatterns[7 - d7] = d3;
                *tileEntryPtr = ANIM_TILE_BASE + ((uint32_t)newIndex << ANIM_TILE_SIZE_SHIFT);
#ifdef SWOS_SDL_BUILD
                /* On SDL `drawPatternFromIndex` renders from
                 * `s_pitchFullPtrs[d3]` (the 64-bit pitch.c side table)
                 * because the 4-byte pitchDatBuffer slots can't hold a
                 * full 64-bit pointer. The toggle above only updates
                 * pitchDatBuffer's truncated slot, so without the
                 * matching side-table update the next draw keeps
                 * rendering the *previous* tile variant — fan animation
                 * appears frozen. Mirror the toggle into the side
                 * table so reader and writer stay in sync. */
                {
                    extern uint8_t *_s_pitchFullPtrs[];
                    extern uint8_t _pitchChunkyBuffer[];
                    if (d3 >= 0 && d3 < 2310) {
                        _s_pitchFullPtrs[d3] = _pitchChunkyBuffer + (size_t)newIndex * 256;
                    }
                }
#endif
            }
#ifdef SWOS_SDL_BUILD
            if (WIDE_MARGIN > 0)
                drawFanMirror(d3, 1, d4);   /* widescreen: mirror the toggle into the right half */
#endif
        }

        drawPatternFromIndex((uint16_t)d3);
    }
}
