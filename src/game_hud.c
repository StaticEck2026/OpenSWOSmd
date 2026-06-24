/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "game_hud.h"
#include "game_types.h"
#include "display.h"    /* _screen_width, WIDE_MARGIN — center the controlled-player name in widescreen */

/* ---- Sprite indices ---- */
#define SPR_CORNER_FLAG_FRAME_3         1073
#define SPR_TEAM1_PLAYER_NAMES_START    0x8D
#define SPR_TEAM2_PLAYER_NAMES_START    0x9D

/* ---- External globals ---- */
extern Sprite _team1CurPlayerNumSprite;
extern Sprite _team2CurPlayerNumSprite;
extern Sprite _playerMarkSprite;
extern Sprite _currentPlayerNameSprite;
extern TeamGeneralInfo _topTeamData;
extern TeamGeneralInfo _bottomTeamData;
extern TeamGame _topTeamInGame;
extern uint16_t _currentGameTick;
extern uint16_t _trainingGame;
extern uint16_t _currentPlayerIndicatorPosition;
extern uint16_t _resultTimer;
extern uintptr_t _currentScorer;
extern TeamGeneralInfo *_lastTeamScored;
extern Sprite  *_lastPlayerBeforeGoalkeeper;
extern Sprite  *_lastPlayerPlayed;
extern TeamGeneralInfo *_lastTeamPlayed;
extern uint16_t _whichCard;
extern Sprite  *_bookedPlayer;
extern TeamGeneralInfo *_lastTeamBooked;
extern uint16_t _nobodysBallTimer;
extern uint16_t _nobodysBallLastFrame;
extern FixedPoint _cameraX;
extern FixedPoint _cameraY;
extern uint8_t _plNamesLengthTableCopy[];
extern uint8_t _plOrdinalsTable[];
extern uint16_t _gameStatePl;

/* ================================================================
 * UpdateControlledPlayerNumbers  (SWOS2.s line 12642)
 *
 * Shows shirt number sprite above the player each team currently
 * controls. When the controlled player is the same as the marked
 * player, the number blinks (alternating between teams via bit 4
 * of currentGameTick).
 * ================================================================ */
__attribute__((used, externally_visible))
void UpdateControlledPlayerNumbers(void)
{
    Sprite          *sprPtr;
    TeamGeneralInfo *teamInfo;
    TeamGame        *teamGame;
    int16_t          d7;

    d7 = 1;
    sprPtr   = &_team1CurPlayerNumSprite;
    teamInfo = &_topTeamData;
    teamGame = teamInfo->inGameTeamPtr;

    for (;;) {
        /* Check if team has a human player or is in training mode */
        if ((uint16_t)teamInfo->isPlCoach != 0
            || (uint16_t)teamInfo->playerNumber != 0
            || _trainingGame != 0) {

            Sprite *controlledPlayer = teamInfo->controlledPlayer;
            if (controlledPlayer != (Sprite *)0) {
                int16_t ordinal = controlledPlayer->playerOrdinal;
                ordinal -= 1;

                /* If controlled player == marked player, apply blink */
                if (ordinal == teamGame->markedPlayer) {
                    uint8_t tickBit4 = ((uint8_t *)&_currentGameTick)[1] & 0x10;
                    if (d7 != 0) {
                        /* Team 1: hide when bit4 is clear */
                        if (!tickBit4)
                            goto hide;
                    } else {
                        /* Team 2: hide when bit4 is set */
                        if (tickBit4)
                            goto hide;
                    }
                }

                /* Place the number sprite */
                {
                    int8_t shirtNum = teamGame->players[ordinal].shirtNumber;
                    int16_t d0 = SPR_CORNER_FLAG_FRAME_3 + (int16_t)shirtNum;

                    sprPtr->imageIndex = d0;
                    sprPtr->x.s.i     = controlledPlayer->x.s.i;
                    sprPtr->z.s.i     = controlledPlayer->z.s.i + 20;
                    sprPtr->y.s.i     = controlledPlayer->y.s.i;
                }
                goto next;
            }
        }
hide:
        sprPtr->imageIndex = -1;
next:
        sprPtr   = &_team2CurPlayerNumSprite;
        teamInfo = &_bottomTeamData;
        teamGame = teamInfo->inGameTeamPtr;
        d7--;
        if (d7 < 0)
            break;
    }
}

/* ================================================================
 * MarkPlayer  (SWOS2.s line 12696)
 *
 * Shows a marker sprite above the team's marked player.
 * Alternates between top and bottom team based on currentGameTick
 * bit 4 — only one team's marker is visible at a time.
 * ================================================================ */
__attribute__((used, externally_visible))
void MarkPlayer(void)
{
    TeamGeneralInfo *teamInfo;
    TeamGame        *teamGame;
    Sprite          *spr = &_playerMarkSprite;
    int16_t          markedIdx;

    /* Select team based on tick bit 4 */
    teamInfo = &_topTeamData;
    if (((uint8_t *)&_currentGameTick)[1] & 0x10)
        teamInfo = &_bottomTeamData;

    teamGame = teamInfo->inGameTeamPtr;
    spr->imageIndex = -1;

    markedIdx = teamGame->markedPlayer;
    if (markedIdx < 0)
        return;
    if (markedIdx >= 11)
        return;

    {
        Sprite  *playerSpr = teamInfo->spritesTable[markedIdx];

        if (playerSpr->playerState != PL_NORMAL)
            return;

        spr->imageIndex = 0x442;
        spr->x.s.i      = playerSpr->x.s.i;
        spr->z.s.i      = playerSpr->z.s.i + 20;
        spr->y.s.i      = playerSpr->y.s.i;
    }
}

/* ================================================================
 * SetCurrentPlayerName  (SWOS2.s line 12727)
 *
 * Displays the current player's name sprite in one of 4 screen
 * corners based on currentPlayerIndicatorPosition:
 *   0 = disabled (hide)
 *   1 = bottom-left
 *   2 = bottom-right
 *   3 = top-right
 *   4 = top-left (default/fallthrough)
 *
 * Has a state machine for scorer display, booking display,
 * last-player-before-goalkeeper, and normal play tracking.
 * ================================================================ */
__attribute__((used, externally_visible))
void SetCurrentPlayerName(void)
{
    Sprite          *spr = &_currentPlayerNameSprite;
    TeamGeneralInfo *a1;    /* TeamGeneralInfo ptr */
    Sprite          *a2;    /* Sprite ptr */
    int16_t          d0, d1, d2;

    spr->visible = 1;

    if (_currentPlayerIndicatorPosition == 0)
        goto hide_and_clear;

    /* If result is showing and position <= 2, hide */
    if (_resultTimer != 0) {
        if (_currentPlayerIndicatorPosition <= 2)
            goto hide_and_clear;
    }

    /* Priority 1: current scorer */
    if (_currentScorer != 0) {
        a2 = (Sprite *)_currentScorer;
        a1 = _lastTeamScored;
        /* Blink: hide when bit 3 of tick LSB is clear */
        if (!(((uint8_t *)&_currentGameTick)[1] & 0x08))
            goto hide_and_clear;
        goto position_name;
    }

    /* Priority 2: booked player */
    if (_whichCard != 0) {
        a2 = _bookedPlayer;
        a1 = _lastTeamBooked;
        if (!(((uint8_t *)&_currentGameTick)[1] & 0x08))
            goto hide_and_clear;
        goto position_name;
    }

    /* Priority 3: last player before goalkeeper */
    if (_lastPlayerBeforeGoalkeeper != (Sprite *)0) {
        a2 = _lastPlayerBeforeGoalkeeper;
        a1 = _lastTeamScored;
        goto check_timer;
    }

    /* Priority 4: last player who touched ball */
    a2 = _lastPlayerPlayed;
    a1 = _lastTeamPlayed;

    if (_gameStatePl != ST_GAME_IN_PROGRESS) {
        /* Not in play — check for null pointer or no controlled player */
        if (a2 == (Sprite *)0)
            goto set_delay_and_hide;
        if (a1->controlledPlayer == (Sprite *)0)
            goto set_delay_and_hide;

        /* Wait for nobodysBallLastFrame to count down */
        if (_nobodysBallLastFrame != 0) {
            _nobodysBallLastFrame--;
            goto hide_and_clear;
        }
        goto set_timer_and_check;
    }

    /* In play: need a valid player */
    if (a2 == (Sprite *)0)
        goto hide_and_clear;

    /* If team has ball, set timer; otherwise check existing timer */
    if ((uint16_t)a1->playerHasBall != 0)
        goto check_timer;

set_timer_and_check:
    _nobodysBallTimer = 50;

check_timer:
    if (_nobodysBallTimer == 0)
        goto hide_and_clear;
    _nobodysBallTimer--;

position_name:
    /* Look up player name sprite index and width */
    d0 = a2->playerOrdinal - 1;

    d2 = (int16_t)(int8_t)_plNamesLengthTableCopy[d0];
    d1 = (int16_t)(int8_t)_plOrdinalsTable[d0];
    d1 += SPR_TEAM1_PLAYER_NAMES_START;

    /* Check if this is the bottom team */
    if (a1->inGameTeamPtr != &_topTeamInGame) {
        d2 = (int16_t)(int8_t)_plNamesLengthTableCopy[d0 + 16];
        d1 = (int16_t)(int8_t)_plOrdinalsTable[d0 + 16];
        d1 += SPR_TEAM2_PLAYER_NAMES_START;
    }

    d2 = (int16_t)(int8_t)(uint8_t)d2;     /* sign-extend from byte */
    spr->imageIndex = d1;
    /* Widescreen: shift the controlled-player name into the centered 336 region (the
     * per-corner offsets below — +10/+20 left, +326-width right — key off this base).
     * WIDE_MARGIN folds to 0 on Amiga / non-widescreen. */
    spr->x.s.i      = (int16_t)_cameraX.s.i + WIDE_MARGIN;
    spr->y.s.i      = (int16_t)_cameraY.s.i + 8000;
    spr->z.s.i      = 8000;

    switch (_currentPlayerIndicatorPosition) {
    case 1:
        /* Bottom-left */
        spr->x.s.i += 10;
        spr->y.s.i += 251;
        return;
    case 2:
        /* Bottom-right */
        spr->x.s.i += 326;
        spr->x.s.i -= d2;
        spr->y.s.i += 251;
        return;
    case 3:
        /* Top-right */
        spr->x.s.i += 326;
        spr->x.s.i -= d2;
        spr->y.s.i += 14;
        return;
    default:
        /* Top-left (position == 4 or any other) */
        spr->x.s.i += 20;
        spr->y.s.i += 14;
        return;
    }

set_delay_and_hide:
    _nobodysBallLastFrame = 1;
    /* fall through to hide_and_clear */

hide_and_clear:
    spr->imageIndex = -1;
    spr->x.s.i      = 25000;
    spr->y.s.i      = 25000;
    spr->z.s.i      = 25000;
    _lastPlayerBeforeGoalkeeper = (Sprite *)0;
}
