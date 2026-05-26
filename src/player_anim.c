/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "player_anim.h"
#include "game_types.h"

/* Player states */
/* PL_NORMAL defined in game_types.h */

/* Game states */
/* ST_GAME_IN_PROGRESS defined in game_types.h */

/* ---- External globals ---- */
extern Sprite           *team1SpritesTable[11];
extern Sprite           *team2SpritesTable[11];
extern uint16_t          goalScored;
extern uint16_t          lastTeamScoredNumber;
extern Sprite           *lastPlayerScored;
extern uint16_t          currentGameTick;
extern uint16_t          gameStatePl;
extern uint16_t          breakCameraMode;
extern AnimTable         playerNormalStandingAnimTable;
extern uint16_t          panicReasonCode;

/* ---- External C functions ---- */
extern void _PanicHaltLoop(void);

/* ================================================================
 * setNextPlayerFrame  (SWOS2.s line 9561)
 *
 * Advances animation frame for a player sprite. Handles animation
 * command words (loop, hold, relative jump, delay change) and
 * goal celebration overlay (+24 frame offset).
 * ================================================================ */
static void setNextPlayerFrame(Sprite *s)
{
    if (s->onScreen == 0)
        return;

    s->cycleFramesTimer -= 1;
    if (s->cycleFramesTimer != 0)
        return;

    s->frameIndex += 1;
    s->cycleFramesTimer = s->frameDelay;

    int16_t *frameTable = s->frameIndicesTable;

    for (;;) {
        int16_t idx = s->frameIndex;
        int16_t frameWord = frameTable[idx];

        if (frameWord >= 0) {
            /* Normal picture index */
            int16_t picIdx = frameWord;
            s->frameSwitchCounter = (int16_t)((uint16_t)s->frameSwitchCounter + 1u);
            picIdx += s->frameOffset;

            /* Goal celebration check */
            if (goalScored != 0
                && s->playerState == PL_NORMAL)
            {
                uint16_t dir = (uint16_t)s->direction;
                if (dir == 0 || dir == 4) {
                    if (lastTeamScoredNumber == (uint16_t)s->teamNumber
                        && (uint16_t)s->playerOrdinal != 1u)
                    {
                        if (s == lastPlayerScored) {
                            /* Scorer: celebrate when (tick & 0x7F) <= 100 */
                            if ((currentGameTick & 0x7Fu) <= 100u) {
                                picIdx += 24;
                            }
                        } else {
                            /* Non-scorer: staggered flash */
                            uint16_t v = (uint16_t)(((uint16_t)s->playerOrdinal << 2)
                                         + currentGameTick);
                            if ((v & 63u) <= 31u) {
                                picIdx += 24;
                            }
                        }
                    }
                }
            }

            s->imageIndex = (int16_t)(uint16_t)picIdx;
            return;
        }

        /* Animation command (negative frame word) */
        if (frameWord == (int16_t)0xFC19) {
            /* -999: reset to frame 0 */
            s->frameIndex = 0;
            continue;
        }

        if (frameWord == (int16_t)0xFF9B) {
            /* -101: step back one, hold */
            s->frameIndex -= 1;
            return;
        }

        if (frameWord <= (int16_t)0xFF9C) {
            /* <= -100: relative jump */
            s->frameIndex += (int16_t)(frameWord + 100);
            continue;
        }

        /* -99 to -1: negate = new frame delay */
        int16_t newDelay = (int16_t)(-(int16_t)frameWord);
        s->frameDelay = newDelay;
        s->cycleFramesTimer = newDelay;
        s->frameIndex += 1;
        /* continue fetching next frame word */
    }
}

/* ================================================================
 * clampAxis  (shared helper macro)
 *
 * Moves a fixed-point axis (X or Y) toward destination.
 *
 * Implemented as a macro to avoid taking the address of packed
 * FixedPoint members (which would trigger -Waddress-of-packed-member).
 * All accesses are done directly via the sprite pointer so the
 * compiler uses byte-granular packed reads/writes throughout.
 *
 * _pos and _delta are FixedPoint lvalue expressions (not pointers).
 * _dest is the int16_t destination.
 *
 * Clamp conditions (from 68k cmp.w + blt/ble):
 *   negative delta (left/up): clamp when curInt <= dest
 *   positive delta (right/down): clamp when curInt >= dest
 * ================================================================ */
#define clampAxis(_pos, _delta, _dest)                  \
    do {                                                 \
        int32_t _dv = (_delta).raw;                      \
        if (_dv != 0) {                                  \
            (_pos).raw += _dv;                           \
            int16_t _cur = (_pos).s.i;                   \
            int16_t _dst = (_dest);                      \
            if (_dv < 0) {                               \
                if (_cur <= _dst) {                      \
                    (_pos).s.i  = _dst;                  \
                    (_delta).raw = 0;                    \
                }                                        \
            } else {                                     \
                if (_cur >= _dst) {                      \
                    (_pos).s.i  = _dst;                  \
                    (_delta).raw = 0;                    \
                }                                        \
            }                                            \
        }                                                \
    } while (0)

/* ================================================================
 * movePlayer  (SWOS2.s line 9645)
 *
 * Moves player X/Y toward destination using fixed-point deltas.
 * Clamps to destination when passed. When stopped and PL_NORMAL,
 * switches to standing animation table.
 * ================================================================ */
static void movePlayer(Sprite *s)
{
    if (s->onScreen == 0) {
        /* Off-screen: only move when game in progress, no anim switch */
        if (gameStatePl == ST_GAME_IN_PROGRESS) {
            clampAxis(s->x, s->deltaX, s->destX);
            clampAxis(s->y, s->deltaY, s->destY);
            return;
        }
        /* Not in progress: fall through to normal path */
    }

    /* Normal (on-screen or non-game-in-progress) path */
    clampAxis(s->x, s->deltaX, s->destX);
    clampAxis(s->y, s->deltaY, s->destY);

    /* Check if we should switch to standing animation */
    if (s->playerState != PL_NORMAL)
        return;
    if (s->deltaX.raw != 0)
        return;
    if (s->deltaY.raw != 0)
        return;

    /* Player has stopped. Check for DESTREACHEDSTATE update. */
    if (gameStatePl != ST_GAME_IN_PROGRESS) {
        if (breakCameraMode == 3) {
            if (s->destReachedState == 2) {
                s->destReachedState = 3;
            }
        }
    }

    /* Switch to standing anim if not already using it */
    if (s->animationTable != &playerNormalStandingAnimTable) {
        _SetPlayerAnimationTable(&playerNormalStandingAnimTable, s);
    }
}

/* ================================================================
 * _MovePlayers  (SWOS2.s line 9536)
 *
 * Iterates all 22 players (team1 + team2) and calls
 * setNextPlayerFrame + movePlayer for each.
 * ================================================================ */
__attribute__((used, externally_visible))
void _MovePlayers(void)
{
    int16_t i;

    /* Team 1 sprites */
    for (i = 0; i < 11; i++) {
        Sprite *s = team1SpritesTable[i];
        setNextPlayerFrame(s);
        movePlayer(s);
    }

    /* Team 2 sprites */
    for (i = 0; i < 11; i++) {
        Sprite *s = team2SpritesTable[i];
        setNextPlayerFrame(s);
        movePlayer(s);
    }
}

/* ================================================================
 * computeAnimSlot  (shared helper)
 *
 * Computes the animation table slot index:
 *   slot = (teamIdx + gkAdj) * 8 + direction
 * Returns byte offset into animFrames array (slot * 4).
 * ================================================================ */
static uint16_t computeAnimSlot(Sprite *s)
{
    uint16_t slot = (uint16_t)s->teamNumber - 1u;
    if ((uint16_t)s->playerOrdinal == 1u)
        slot += 2u;
    slot = (uint16_t)((slot << 3) + (uint16_t)s->direction);
    return (uint16_t)(slot << 2);
}

/* ================================================================
 * _SetPlayerAnimationTable  (SWOS2.s line 9788)
 *
 * Sets animation table and resets frame state for restart.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetPlayerAnimationTable(AnimTable *animTable, Sprite *s)
{
    s->animationTable = animTable;

    uint16_t slotOffset = computeAnimSlot(s);

    s->frameDelay = animTable->frameDelay;

    int16_t *frameIndicesPtr = animTable->animFrames[slotOffset >> 2];
    s->frameIndicesTable = frameIndicesPtr;

    if (frameIndicesPtr == 0) {
        panicReasonCode = 0x000E;
        _PanicHaltLoop();
        return;
    }

    s->frameSwitchCounter = (int16_t)0xFFFF;
    s->frameIndex         = (int16_t)0xFFFF;
    s->cycleFramesTimer   = 1;
    s->startingDirection  = s->direction;
}

/* ================================================================
 * _SetPlayerAnimationTableAndPictureIndex  (SWOS2.s line 9748)
 *
 * Sets animation table without resetting frame state, and
 * immediately updates imageIndex from current frame.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetPlayerAnimationTableAndPictureIndex(AnimTable *animTable, Sprite *s)
{
    s->animationTable = animTable;

    uint16_t slotOffset = computeAnimSlot(s);

    s->frameDelay = animTable->frameDelay;

    int16_t *frameIndicesPtr = animTable->animFrames[slotOffset >> 2];
    s->frameIndicesTable = frameIndicesPtr;

    if (frameIndicesPtr == 0) {
        panicReasonCode = 0x000E;
        _PanicHaltLoop();
        return;
    }

    s->startingDirection = s->direction;

    /* Read current frame word and update imageIndex if non-negative */
    int16_t *frameTable = (int16_t *)frameIndicesPtr;
    int16_t frameIdx    = s->frameIndex;
    int16_t frameWord   = frameTable[frameIdx];

    if (frameWord >= 0) {
        s->imageIndex = (int16_t)(uint16_t)((uint16_t)frameWord + (uint16_t)s->frameOffset);
    }
}
