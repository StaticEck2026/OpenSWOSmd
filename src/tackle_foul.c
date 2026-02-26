#include "tackle_foul.h"

/* Card types (not in game_types.h) */
#define CARD_YELLOW                 1
#define CARD_RED                    2
#define CARD_SECOND_YELLOW          3

/* Tackle state */
#define TS_GOOD_TACKLE              2

/* ---- External globals ---- */
extern Sprite   ballSprite;
extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;

extern int16_t  kDefaultDestinations[];     /* 8 direction deltas: dx,dy pairs */
extern uint16_t ingameTeamPlayerOffsets[];

extern int16_t  gameState;
extern int16_t  gameStatePl;
extern int16_t  breakCameraMode;
extern int16_t  gameNotInProgressCounterWriteOnly;
extern int16_t  stoppageTimerTotal;
extern int16_t  stoppageTimerActive;
extern int16_t  cameraXVelocity;
extern int16_t  cameraYVelocity;
extern int16_t  cameraDirection;
extern uint8_t  playerTurnFlags;
extern int16_t  foulXCoordinate;
extern int16_t  foulYCoordinate;
extern int16_t  forceLeftTeam;
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;

extern int16_t  plgD3Param;
extern int16_t  whichCard;
extern int16_t  refTimer;
extern TeamGeneralInfo *lastTeamBooked;
extern Sprite  *bookedPlayer;
extern int16_t  team1NumAllowedInjuries;
extern int16_t  team2NumAllowedInjuries;

extern int16_t  cardsDisallowed;
extern int16_t  trainingGame;
extern int16_t  gameLengthInGame;
extern uint16_t currentGameTick;
extern int16_t  playerCardChance;
extern void    *nearestTeammatePtr;

extern int16_t  ballNextGroundX;
extern int16_t  ballNextYGroundY;

/* Inline data tables from SWOS2.s */
extern int16_t  kPlayerTacklingDownTime[];
extern int16_t  kComputerTacklingDownTime[];
extern uint8_t  cardAnimStatesNoFlag[];
extern uint8_t  cardAnimStatesWithFlag[];

/* Animation tables */
extern uint8_t  playerTackledAnimTable[];
extern uint8_t  plTacklingAnimTable[];
extern uint8_t  goalieCatchingBallAnimTable[];
extern int16_t  kGoalkeeperCatchSpeed;
extern int16_t  kPlayerTacklingSpeed;

/* Injury lookup tables from SWOS2.s */
extern uint8_t  kInjuryLevels[];
extern uint8_t  kInjuryLevelAlreadyInjured[];
extern int16_t  kInjuryRecoveryTime[];
extern uint8_t  kTackleInjuryProbability[];
extern uint8_t  kTackleInjuryProbabilityAlreadyInjured[];

/* ---- External functions ---- */
extern void     _PlayKickSample(void);
extern void     _PlayFoulWhistleSample(void);
extern void     _PlayPenalty(void);
extern void     _ResetBothTeamSpinTimers(void);
extern void     _StopAllPlayers(void);
extern void     _ActivateReferee(void);
extern uint16_t _Rand(void);
extern uint8_t *_GetPlayerPointerFromShirtNumber(Sprite *player, TeamGeneralInfo *team);
extern void     _SetPlayerAnimationTable(uint8_t *animTable, uint8_t *sprite);

/* ========================================================================
 * PlayerTackledTheBallWeak
 * Weak tackle: ball direction adjusted by input, speed = 50% player + 50%,
 * ball speed = 150% of player speed. Marks as good tackle if opponent far.
 * ======================================================================== */
void __attribute__((used, externally_visible))
_PlayerTackledTheBallWeak(Sprite *player, TeamGeneralInfo *team)
{
    Sprite *ball = &ballSprite;
    int16_t inputDir = team->currentAllowedDirection;
    int16_t playerDir, diff, finalDir;
    int16_t dx, dy;
    int16_t speed;
    TeamGeneralInfo *oppTeam;
    Sprite *oppControlled;

    team->passInProgress = 0;

    if (inputDir < 0)
        inputDir = player->direction;

    playerDir = player->direction;
    diff = (int16_t)((playerDir - inputDir) & 0xFFFF);

    if (diff == 0 || (diff & 7) == 4) {
        finalDir = playerDir;
    } else if ((diff & 7) < 4) {
        finalDir = playerDir - 1;
    } else {
        finalDir = playerDir + 1;
    }
    finalDir &= 7;

    team->controlledPlDirection = (int16_t)finalDir;

    /* Set ball destination from direction table */
    dx = kDefaultDestinations[finalDir * 2];
    dy = kDefaultDestinations[finalDir * 2 + 1];
    ball->destX = ball->x.s.i + dx;
    ball->destY = ball->y.s.i + dy;

    /* Ball speed: player speed * 0.5, then set = (remaining + half) = 1.5x remaining */
    speed = (uint16_t)player->speed;
    speed >>= 1;
    player->speed = (int16_t)((uint16_t)player->speed - speed);
    player->speed = (int16_t)((uint16_t)player->speed | 1);
    speed = (uint16_t)player->speed;
    ball->speed = (int16_t)(speed + (speed >> 1));

    /* Mark as basic tackle */
    player->tackleState = (int16_t)1;

    /* Check if opponent's controlled player is far enough away for a "good tackle" */
    oppTeam = team->opponentsTeam;
    oppControlled = oppTeam->controlledPlayer;
    if (oppControlled != (Sprite *)0) {
        if (oppControlled->ballDistance >= 9) {
            int16_t dxp = player->x.s.i - oppControlled->x.s.i;
            int16_t dyp = player->y.s.i - oppControlled->y.s.i;
            int32_t distSq = (int32_t)dxp * dxp + (int32_t)dyp * dyp;
            if (distSq > 32) {
                player->tackleState = (int16_t)TS_GOOD_TACKLE;
            }
        }
    }

    _PlayKickSample();
    _ResetBothTeamSpinTimers();
}

/* ========================================================================
 * SetPlayerDowntimeAfterTackle
 * Sets how long a player lies on the ground based on tackling skill.
 * ======================================================================== */
void __attribute__((used, externally_visible))
_SetPlayerDowntimeAfterTackle(Sprite *player, TeamGeneralInfo *team)
{
    uint8_t *pihBase = _GetPlayerPointerFromShirtNumber(player, team);
    PlayerInfo *pi = (PlayerInfo *)(pihBase + 42);
    int16_t *table;
    int16_t tackleSkill;
    int16_t downtime;

    if ((uint16_t)player->tacklingTimer == 0xFFFF)
        table = kComputerTacklingDownTime;
    else
        table = kPlayerTacklingDownTime;

    tackleSkill = pi->tackling;
    downtime = table[tackleSkill];
    player->playerDownTimer = (int8_t)(uint8_t)downtime;
}

/* ========================================================================
 * PlayerTackled
 * Injury resolution: probabilistic injury based on game length, tackling skill,
 * existing injuries. Sets PL_TACKLED state.
 * ======================================================================== */
void __attribute__((used, externally_visible))
_PlayerTackled(Sprite *player, TeamGeneralInfo *team)
{
    PlayerInfo *pi;
    uint8_t *probTable;
    uint8_t *sevTable;
    int16_t teamNum;
    int16_t gameLen;
    int16_t injBitfield;
    uint16_t roll;
    int16_t severity;
    int16_t i;

    /* Training game: 25% chance of any injury processing */
    if (trainingGame != 0) {
        roll = _Rand();
        if ((roll & 3) != 0)
            goto set_tackled;
    }

    /* Check injury quota for this team */
    teamNum = team->teamNumber;
    if (teamNum == 1) {
        if (team1NumAllowedInjuries == 0)
            goto set_tackled;
    } else {
        if (team2NumAllowedInjuries == 0)
            goto set_tackled;
    }

    /* Get player info and check existing injury status */
    {
        int16_t ordinal = (uint16_t)player->playerOrdinal - 1;
        uint16_t offset = ingameTeamPlayerOffsets[ordinal];
        pi = (PlayerInfo *)((uint8_t *)team->inGameTeamPtr + offset + 42);
    }

    gameLen = gameLengthInGame;
    probTable = kTackleInjuryProbability;

    injBitfield = pi->injuriesBitfield & 0xE0;
    if (injBitfield == 0x20) {
        probTable = kTackleInjuryProbabilityAlreadyInjured;
    }

    /* Roll for injury */
    roll = _Rand();
    if ((uint8_t)roll >= probTable[gameLen])
        goto set_tackled;

    /* Injury! Determine severity bucket */
    sevTable = kInjuryLevels;
    injBitfield = pi->injuriesBitfield & 0xE0;
    if (injBitfield == 0x20) {
        sevTable = kInjuryLevelAlreadyInjured;
    }

    roll = _Rand();
    roll &= 0x3F;
    severity = 0x20;

    for (i = 0; i < 7; i++) {
        if ((uint8_t)roll < sevTable[i])
            goto apply_injury;
        roll -= sevTable[i];
        severity += 0x20;
    }
    /* Fall through: max severity */

apply_injury:
    pi->isInjured = 1;

    /* Only upgrade injury severity, never downgrade */
    if ((uint8_t)severity > (uint8_t)pi->injuriesBitfield) {
        pi->injuriesBitfield = (int8_t)(uint8_t)severity;
    }

    /* Map severity to recovery time */
    {
        int16_t sevIdx = (severity & 0xE0) >> 5;
        player->injuryLevel = (int16_t)kInjuryRecoveryTime[sevIdx];
    }

    /* Decrement injury quota for team */
    if (teamNum == 1)
        team1NumAllowedInjuries--;
    else
        team2NumAllowedInjuries--;

set_tackled:
    player->playerState = (uint8_t)PL_TACKLED;
    player->playerDownTimer = (int8_t)0x32;
    _SetPlayerAnimationTable(playerTackledAnimTable, (uint8_t *)player);
}

/* ========================================================================
 * TryBookingThePlayer
 * Returns 0 if card was given, 1 if not eligible.
 * ======================================================================== */
int16_t __attribute__((used, externally_visible))
_TryBookingThePlayer(Sprite *player, TeamGeneralInfo *team)
{
    PlayerInfo *pi;
    int16_t cards;
    int16_t ordinal;
    uint16_t offset;
    TeamStatisticsData *stats;

    if (plgD3Param != 0) {
        /* Highlight mode */
        if (team->playerNumber == 0 &&
            team->playerCoachNumber == 0) {
            /* Not a human-controlled team -- check cards */
            if ((uint16_t)player->cards != 0)
                return 1; /* not eligible */
        }
    } else {
        /* Normal mode -- need at least one human controller */
        if (team->playerNumber == 0 &&
            team->playerCoachNumber == 0) {
            return 1; /* not eligible */
        }
    }

    /* If player already has a card, check injury quota for second yellow / red */
    if ((uint16_t)player->cards != 0) {
        if (team->teamNumber == 1) {
            if (team1NumAllowedInjuries == 0)
                return 1;
        } else {
            if (team2NumAllowedInjuries == 0)
                return 1;
        }
    }

    /* Resolve player in-game slot */
    ordinal = (uint16_t)player->playerOrdinal - 1;
    offset = ingameTeamPlayerOffsets[ordinal];
    pi = (PlayerInfo *)((uint8_t *)team->inGameTeamPtr + offset + 42);

    cards = (uint16_t)player->cards;
    cards++;

    if (plgD3Param == 0) {
        /* Normal mode: use LUT based on previous card history */
        uint8_t *lutTable;
        if (pi->previousCards == 0)
            lutTable = cardAnimStatesNoFlag;
        else
            lutTable = cardAnimStatesWithFlag;
        pi->numYellowCards = lutTable[cards];
    } else {
        /* Highlight mode: simplified card state */
        if (pi->numYellowCards == 0)
            pi->numYellowCards = 1;
        else
            pi->numYellowCards = 3;
    }

    /* Commit booking */
    player->cards = (int16_t)((uint16_t)player->cards + 1);
    lastTeamBooked = team;
    bookedPlayer = player;
    refTimer = 0;

    if ((uint16_t)player->cards == 2) {
        /* Second yellow = red card (second yellow) */
        whichCard = CARD_SECOND_YELLOW;
        stats = team->teamStatsPtr;
        stats->bookings--;
        stats->sendingsOff++;
        if (team->teamNumber == 1)
            team1NumAllowedInjuries--;
        else
            team2NumAllowedInjuries--;
        return 0;
    }

    /* First yellow */
    whichCard = CARD_YELLOW;
    stats = team->teamStatsPtr;
    stats->bookings++;
    return 0;
}

/* ========================================================================
 * TrySendingOffThePlayer
 * Straight red card. Returns 0 if card given, 1 if not eligible.
 * ======================================================================== */
int16_t __attribute__((used, externally_visible))
_TrySendingOffThePlayer(Sprite *player, TeamGeneralInfo *team)
{
    PlayerInfo *pi;
    int16_t cards;
    int16_t ordinal;
    uint16_t offset;
    TeamStatisticsData *stats;

    /* Need at least one human controller */
    if (team->playerNumber == 0 &&
        team->playerCoachNumber == 0)
        return 1;

    /* Check injury quota */
    if (team->teamNumber == 1) {
        if (team1NumAllowedInjuries == 0)
            return 1;
    } else {
        if (team2NumAllowedInjuries == 0)
            return 1;
    }

    /* Resolve player in-game slot */
    ordinal = (uint16_t)player->playerOrdinal - 1;
    offset = ingameTeamPlayerOffsets[ordinal];
    pi = (PlayerInfo *)((uint8_t *)team->inGameTeamPtr + offset + 42);

    /* d0 = cards ^ 1 + 3 */
    cards = (uint16_t)player->cards;
    cards = (cards ^ 1) + 3;

    if (plgD3Param == 0) {
        /* Normal mode: use LUT */
        uint8_t *lutTable;
        if (pi->previousCards == 0)
            lutTable = cardAnimStatesNoFlag;
        else
            lutTable = cardAnimStatesWithFlag;
        pi->numYellowCards = lutTable[cards];
    } else {
        /* Highlight mode: force card state 3 */
        pi->numYellowCards = 3;
    }

    /* Finalize straight red */
    lastTeamBooked = team;
    bookedPlayer = player;
    refTimer = 0;
    whichCard = CARD_RED;

    stats = team->teamStatsPtr;
    if ((uint16_t)player->cards == 1) {
        /* Had one yellow, now straight red -- remove yellow from bookings */
        stats->bookings--;
    }
    stats->sendingsOff++;

    if (team->teamNumber == 1)
        team1NumAllowedInjuries--;
    else
        team2NumAllowedInjuries--;

    return 0;
}

/* ========================================================================
 * SendPlayerAway
 * Called when player gets a red card -- marks them as sent off.
 * ======================================================================== */
void __attribute__((used, externally_visible))
_SendPlayerAway(Sprite *player, TeamGeneralInfo *team)
{
    TeamGame *teamGame = team->inGameTeamPtr;
    int16_t ordinal = (uint16_t)player->playerOrdinal - 1;

    /* If this player is the marked player, clear the mark */
    if (ordinal == teamGame->markedPlayer) {
        teamGame->markedPlayer = -1;
    }

    player->cards = (int16_t)-1;
    player->sentAway = (int16_t)1;
    player->destX = (int16_t)-0x14;   /* -20 */
    player->destY = (int16_t)449;
}

/* ========================================================================
 * PlayerTacklingTestFoul
 * Tests if a tackle resulted in a foul. Orchestrates card issuance, foul
 * state, penalty/free kick. A1=tackling player, A6=tackling player's team.
 * ======================================================================== */
void __attribute__((used, externally_visible))
_PlayerTacklingTestFoul(Sprite *player, TeamGeneralInfo *team)
{
    TeamGeneralInfo *oppTeam;
    Sprite *victim;
    int16_t dx, dy;
    int32_t distSq;
    int16_t dirDiff;
    TeamStatisticsData *stats;
    uint16_t roll;

    /* Get opponent's controlled player (victim) */
    oppTeam = team->opponentsTeam;
    victim = oppTeam->controlledPlayer;

    /* Null check */
    if (victim == (Sprite *)0)
        return;

    /* Check distance between tackler and victim */
    dx = player->x.s.i - victim->x.s.i;
    dy = player->y.s.i - victim->y.s.i;
    distSq = (int32_t)(int16_t)dx * (int16_t)dx + (int32_t)(int16_t)dy * (int16_t)dy;
    if (distSq > 32)
        return;

    /* If victim is goalkeeper (ordinal 1), just slow down tackler */
    if ((uint16_t)victim->playerOrdinal == 1) {
        player->speed = (int16_t)((uint16_t)player->speed >> 1);
        player->speed = (int16_t)((uint16_t)player->speed >> 1);
        player->speed = (int16_t)((uint16_t)player->speed | 1);
        return;
    }

    /* Check victim is on pitch bounds */
    if (victim->x.s.i < 81 ||
        victim->y.s.i < 129 ||
        victim->x.s.i > 590 ||
        victim->y.s.i > 769)
        return;

    /* Slow down tackler */
    player->speed = (int16_t)((uint16_t)player->speed >> 1);
    player->speed = (int16_t)((uint16_t)player->speed >> 1);
    player->speed = (int16_t)((uint16_t)player->speed | 1);

    /* Tackle the victim (injury resolution) */
    _PlayerTackled(victim, oppTeam);

    /* If ball is far from victim, no foul */
    if (victim->ballDistance > 800)
        return;

    /* Check tackle quality -- clean tackle if tackleState==0 or good tackle */
    if ((uint16_t)player->tackleState != 0) {
        if ((uint16_t)player->tackleState == TS_GOOD_TACKLE)
            return;

        /* Check direction difference: within +/-1 means from the side = no foul */
        dirDiff = player->direction - victim->direction;
        if (dirDiff < -1 || dirDiff > 1)
            return;
    }

    /* It's a foul! Increment fouls counter */
    stats = team->teamStatsPtr;
    stats->foulsConceded++;

    /* Check if cards are allowed */
    if (cardsDisallowed != 0 || trainingGame != 0)
        goto foul_no_card;

    /* Discipline zone check: victim inside main playing area? */
    {
        int16_t vx = victim->x.s.i;
        int16_t vy = victim->y.s.i;

        if (vx >= 193 && vx <= 478 && vy >= 129 && vy <= 769) {
            /* In the main zone -- check penalty area */
            if (team == &topTeamData) {
                if (vy <= 216)
                    goto maybe_card;
                /* else fall through to nearest-teammate search */
            } else {
                if (vy >= 682)
                    goto maybe_card;
                /* else fall through to nearest-teammate search */
            }
        }

        /* Search for nearest teammate to a fixed point near goal */
        {
            int16_t refX = 336;
            int16_t refY;
            Sprite *nearest = victim;
            int32_t bestDist;
            Sprite **sprTable;
            int16_t i;

            if (team == &topTeamData)
                refY = 129;
            else
                refY = 769;

            dx = victim->x.s.i - refX;
            dy = victim->y.s.i - refY;
            bestDist = (int32_t)dx * dx + (int32_t)dy * dy;

            sprTable = team->spritesTable;
            for (i = 10; i >= 0; i--) {
                Sprite *mate = sprTable[i];
                if ((uint16_t)mate->sentAway != 0)
                    continue;
                if ((uint16_t)mate->playerOrdinal == 1)
                    continue;
                if (mate == player)
                    continue;
                dx = mate->x.s.i - refX;
                dy = mate->y.s.i - refY;
                {
                    int32_t d = (int32_t)dx * dx + (int32_t)dy * dy;
                    if (d <= bestDist) {
                        bestDist = d;
                        nearest = mate;
                    }
                }
            }

            nearestTeammatePtr = nearest;

            if (nearest == victim) {
                /* Same player is nearest: swap card probabilities */
                roll = _Rand();
                if (roll < 32)
                    goto issue_yellow;
                goto try_red;
            }
        }
    }

maybe_card:
    {
        int16_t tickChance = (currentGameTick & 0x1E) >> 1;
        if (tickChance >= playerCardChance)
            goto foul_no_card;
    }

    roll = _Rand();
    if (roll < 32)
        goto try_red;

issue_yellow:
    if (_TryBookingThePlayer(player, team) != 0)
        goto foul_no_card;
    _TestFoulForPenaltyAndFreeKick(victim, team);
    _ActivateReferee();
    return;

try_red:
    if (_TrySendingOffThePlayer(player, team) != 0)
        goto foul_no_card;
    _TestFoulForPenaltyAndFreeKick(victim, team);
    _ActivateReferee();
    return;

foul_no_card:
    _TestFoulForPenaltyAndFreeKick(victim, team);
}

/* ========================================================================
 * GoalkeeperCaughtTheBall
 * Goalkeeper dived and caught the ball. Sets catching state, animation,
 * speed and destination.
 * ======================================================================== */
void __attribute__((used, externally_visible))
_GoalkeeperCaughtTheBall(Sprite *keeper, TeamGeneralInfo *team)
{
    if (team == &topTeamData)
        keeper->direction = (int16_t)4;
    else
        keeper->direction = (int16_t)0;

    team->goalkeeperDivingLeft = 0;
    _SetPlayerAnimationTable(goalieCatchingBallAnimTable, (uint8_t *)keeper);
    keeper->playerState = (uint8_t)PL_GOALIE_CATCHING_BALL;
    keeper->playerDownTimer = (int8_t)15;
    keeper->speed = kGoalkeeperCatchSpeed;
    keeper->destX = ballNextGroundX;
    keeper->destY = ballNextYGroundY;

    /* Clamp Y destination to pitch bounds */
    if (keeper->destY < 137)
        keeper->destY = (int16_t)137;
    if (keeper->destY >= 761)
        keeper->destY = (int16_t)761;
}

/* ========================================================================
 * PlayerTackledTheBallStrong
 * Strong tackle: similar to weak but with optional speed boost (125%).
 * ======================================================================== */
void __attribute__((used, externally_visible))
_PlayerTackledTheBallStrong(Sprite *player, TeamGeneralInfo *team)
{
    Sprite *ball = &ballSprite;
    int16_t inputDir = team->currentAllowedDirection;
    int16_t playerDir, diff, finalDir;
    int16_t dx, dy;
    uint16_t speed;
    TeamGeneralInfo *oppTeam;
    Sprite *oppControlled;

    if (inputDir < 0)
        inputDir = player->direction;

    playerDir = player->direction;
    diff = (int16_t)((playerDir - inputDir) & 0xFFFF);

    if (diff == 0 || (diff & 7) == 4) {
        finalDir = playerDir;
    } else if ((diff & 7) < 4) {
        finalDir = playerDir - 1;
    } else {
        finalDir = playerDir + 1;
    }
    finalDir &= 7;

    team->controlledPlDirection = (int16_t)finalDir;

    /* Set ball destination from direction table */
    dx = kDefaultDestinations[finalDir * 2];
    dy = kDefaultDestinations[finalDir * 2 + 1];
    ball->destX = ball->x.s.i + dx;
    ball->destY = ball->y.s.i + dy;

    /* Ball speed: if PLAYERNUMBER flag set, boost to 125%; otherwise same as player */
    if (team->playerNumber != 0) {
        /* Note: original asm duplicates the computation -- faithfully reproduced */
        speed = (uint16_t)player->speed;
        speed = (speed >> 2) + speed;
        ball->speed = (int16_t)speed;
        speed = (uint16_t)player->speed;
        speed = (speed >> 2) + speed;
        ball->speed = (int16_t)speed;
    } else {
        ball->speed = player->speed;
    }

    /* Halve player speed */
    player->speed = (int16_t)((uint16_t)player->speed >> 1);

    /* Mark as basic tackle */
    player->tackleState = (int16_t)1;

    /* Check if opponent controlled player is far enough for "good tackle" */
    oppTeam = team->opponentsTeam;
    oppControlled = oppTeam->controlledPlayer;
    if (oppControlled != (Sprite *)0) {
        if (oppControlled->ballDistance >= 9) {
            int16_t dxp = player->x.s.i - oppControlled->x.s.i;
            int16_t dyp = player->y.s.i - oppControlled->y.s.i;
            int32_t distSq = (int32_t)dxp * dxp + (int32_t)dyp * dyp;
            if (distSq > 32) {
                player->tackleState = (int16_t)TS_GOOD_TACKLE;
            }
        }
    }

    _PlayKickSample();
    _ResetBothTeamSpinTimers();
}

/* ========================================================================
 * TestFoulForPenaltyAndFreeKick
 * Determines foul type (penalty, free kick zones, or simple foul) based on
 * victim position. Sets game state, foul coords, and enters stopped state.
 * ======================================================================== */
void __attribute__((used, externally_visible))
_TestFoulForPenaltyAndFreeKick(Sprite *victim, TeamGeneralInfo *team)
{
    int16_t vx, vy;

    if (gameStatePl == ST_STOPPED)
        return;

    _PlayFoulWhistleSample();
    /* nullsub_6 omitted */

    vx = victim->x.s.i;
    vy = victim->y.s.i;

    if (team == &topTeamData) {
        /* Top team fouled -- camera direction 0 (up) */
        cameraDirection = 0;

        /* Check penalty area: y <= 216, 193 <= x <= 478 */
        if (vy <= 216 && vx >= 193 && vx <= 478) {
            gameState = ST_PENALTY;
            breakCameraMode = -1;
            playerTurnFlags = 0x83;
            foulXCoordinate = 336;
            foulYCoordinate = 187;
            _PlayPenalty();
            goto apply_break;
        }

        /* Free kick band: 216 < y < 331 */
        if (vy < 216 || vy >= 331)
            goto set_foul;
        goto select_fk_quadrant;
    } else {
        /* Bottom team fouled -- camera direction 4 (down) */
        cameraDirection = 4;

        /* Check penalty area: y >= 682, 193 <= x <= 478 */
        if (vy >= 682 && vx >= 193 && vx <= 478) {
            gameState = ST_PENALTY;
            breakCameraMode = -1;
            playerTurnFlags = 0x38;
            foulXCoordinate = 336;
            foulYCoordinate = 711;
            _PlayPenalty();
            goto apply_break;
        }

        /* Free kick band: 567 <= y <= 682 */
        if (vy < 567 || vy > 682)
            goto set_foul;
    }

select_fk_quadrant:
    if (team == &bottomTeamData) {
        /* Bottom team: mirrored quadrant assignment */
        if (vx < 153) goto fk_right3;
        if (vx < 261) goto fk_right2;
        if (vx < 309) goto fk_right1;
        if (vx < 362) goto fk_center;
        if (vx < 410) goto fk_left3;
        if (vx < 518) goto fk_left2;
        goto fk_left1;
    } else {
        /* Top team */
        if (vx < 153) goto fk_left1;
        if (vx < 261) goto fk_left2;
        if (vx < 309) goto fk_left3;
        if (vx < 362) goto fk_center;
        if (vx < 410) goto fk_right1;
        if (vx < 518) goto fk_right2;
        goto fk_right3;
    }

fk_left1:  gameState = ST_FREE_KICK_LEFT1;  goto commit_foul;
fk_left2:  gameState = ST_FREE_KICK_LEFT2;  goto commit_foul;
fk_left3:  gameState = ST_FREE_KICK_LEFT3;  goto commit_foul;
fk_center: gameState = ST_FREE_KICK_CENTER; goto commit_foul;
fk_right1: gameState = ST_FREE_KICK_RIGHT1; goto commit_foul;
fk_right2: gameState = ST_FREE_KICK_RIGHT2; goto commit_foul;
fk_right3: gameState = ST_FREE_KICK_RIGHT3; goto commit_foul;

set_foul:
    gameState = ST_FOUL;

commit_foul:
    breakCameraMode = -1;
    playerTurnFlags = 0xFF;
    foulXCoordinate = vx;
    foulYCoordinate = vy;

apply_break:
    if (forceLeftTeam == 1)
        team = &bottomTeamData;

    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    lastTeamPlayedBeforeBreak = team->opponentsTeam;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    cameraXVelocity = 0;
    cameraYVelocity = 0;
}


/* ========================================================================
 * _PlayerBeginTackling
 *   from: PlayerBeginTackling
 *   in:  D0=pressedDir, A1=player, A6=team
 *
 * Sets player state to tackling, assigns animation, direction, speed,
 * and destination. Checks fasterTackle player attribute (currently dead
 * code in original: downtime always overwritten with 0xFF).
 * ======================================================================== */
void __attribute__((used, externally_visible))
_PlayerBeginTackling(int16_t pressedDir, Sprite *player, TeamGeneralInfo *team)
{
    player->tackleState = 0;
    team->controlledPlDirection = pressedDir;
    player->direction = pressedDir;

    _SetPlayerAnimationTable(plTacklingAnimTable, (uint8_t *)player);
    player->playerState = (uint8_t)PL_TACKLING;
    player->playerDownTimer = (int8_t)50;

    /* Look up player info to check fasterTackle */
    {
        int16_t ordinal = player->playerOrdinal - 1;
        uint16_t offset = ingameTeamPlayerOffsets[ordinal];
        uint8_t *pihBase = (uint8_t *)team->inGameTeamPtr + offset;
        PlayerInfo *pi = (PlayerInfo *)(pihBase + 42);
        if (pi->field_8 != 0) {
            player->playerDownTimer = (int8_t)25;
        }
    }

    /* Original asm always overwrites with 0xFF here */
    player->playerDownTimer = (int8_t)(uint8_t)0xFF;

    /* Set destination from kDefaultDestinations */
    int16_t destIdx = pressedDir * 2;
    player->destX = player->x.s.i + kDefaultDestinations[destIdx];
    player->destY = player->y.s.i + kDefaultDestinations[destIdx + 1];

    player->speed = kPlayerTacklingSpeed;
    player->tacklingTimer = 0;
}
