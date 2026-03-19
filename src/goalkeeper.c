/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "goalkeeper.h"
#include "game_types.h"
#include "player_anim.h"

/* ==================================================================
 * Chunk 48 — Goalkeeper Physics + Penalty System
 *
 * Ported functions:
 *   GetFramesNeededToCoverDistance  (static helper)
 *   ShouldGoalkeeperDive           (A1=keeper, A2=ball, A6=team → D0)
 *   GoalkeeperDeflectedBall        (A2=ball, A6=team)
 *   NextPenalty                     (no params)
 *   GoalkeeperClaimedTheBall       (A1=keeper, A2=ball, A6=team)
 *   CheckIfGoalkeeperClaimedTheBall (no params)
 * ================================================================== */

/* Game states — also defined in game_types.h, used locally for clarity */
/* ST_KEEPER_HOLDS_BALL == 3, ST_PENALTIES == 31, ST_STOPPED == 101 */

/* ---- External asm globals ---- */
extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;
extern Sprite          ballSprite;
extern TeamGame        topTeamInGame;
extern TeamGame        bottomTeamInGame;

extern uint16_t currentGameTick;
extern int16_t  playingPenalties;
extern int16_t  penalty;
extern int16_t  cameraDirection;
extern uint8_t  playerTurnFlags;
extern int16_t  forceLeftTeam;
extern int16_t  gameState;
extern int16_t  breakCameraMode;
extern int16_t  foulXCoordinate;
extern int16_t  foulYCoordinate;
extern int16_t  gameStatePl;
extern int16_t  gameNotInProgressCounterWriteOnly;
extern int16_t  stoppageTimerTotal;
extern int16_t  stoppageTimerActive;
extern int16_t  cameraXVelocity;
extern int16_t  cameraYVelocity;
extern int16_t  hideBall;
extern int16_t  teamPlayingUp;
extern int16_t  teamStarting;
extern int16_t  penaltiesTimer;
extern int16_t  statsTeam1Goals;
extern int16_t  statsTeam2Goals;

extern Sprite           *lastPlayerPlayed;
extern TeamGeneralInfo  *lastTeamPlayed;
extern Sprite           *lastPlayerBeforeGoalkeeper;
extern TeamGeneralInfo  *lastTeamScored;
extern TeamGeneralInfo  *lastTeamPlayedBeforeBreak;
extern TeamGame         *winningTeamPtr;
extern Sprite           *penaltyShooterSprite;

extern int16_t  team1PenaltyGoals;
extern int16_t  team2PenaltyGoals;
extern int16_t  team1PenaltyAttempts;
extern int16_t  team2PenaltyAttempts;
extern int16_t  savedTeam1Goals;
extern int16_t  savedTeam2Goals;
extern int16_t  team1PenaltyShooterIndex;
extern int16_t  team2PenaltyShooterIndex;

extern int16_t  ballDefensiveX;
extern int16_t  goalkeeperDiveAttemptCount;

extern int16_t  kDefaultDestinations[16];

extern int16_t  kGoalkeeperNearJumpSpeed;
extern int16_t  kGoalkeeperFarJumpSpeed;
extern int16_t  kGoalkeeperFarJumpSlowerSpeed;
extern int16_t  ballNotHighZ;

/* Animation tables */
extern AnimTable leftGoalieJumpingLowAnimTable;
extern AnimTable rightGoalieJumpingLowAnimTable;
extern AnimTable leftGoalieJumpingHighAnimTable;
extern AnimTable rightGoalieJumpingHighAnimTable;

/* Data tables */
extern int16_t  kKeeperPenaltySaveDistanceFar;
extern int16_t  kKeeperPenaltySaveDistanceNear;
extern int16_t  kKeeperSaveDistance;
extern int32_t  kGoalkeeperDiveDeltas[8];
extern int16_t  kGoalkeeperStrongDeflectBallSpeed;
extern int16_t  kGoalkeeperMediumDeflectBallSpeed;
extern int16_t  kGoalkeeperWeakDeflectBallSpeed;
extern int32_t  kGoalkeeperDeflectDeltaZ;

/* Functions from other modules */
extern uint16_t _Rand(void);
extern void     _PlayKickSample(void);
extern void     _ResetBothTeamSpinTimers(void);
extern void     _InitTeamsData(void);
extern void     _StopAllPlayers(void);
extern void     _UpdatePlayerWithBall(uint8_t *player);
extern void     _EndOfGame(void);


/* ==================================================================
 * getFramesNeededToCoverDistance (static)
 *   from: GetFramesNeededToCoverDistance
 *   in:  movementDelta (D0, fixed 16.16), distance (D4, integer)
 *   out: frames needed (D7, integer)
 *
 * Division D4/D0 via loop subtraction. If D0 < 1.0, scales it up
 * along with the loop step to limit iterations.
 * Returns 0 if D0 == 0.
 * ================================================================== */
static int16_t getFramesNeededToCoverDistance(int32_t movementDelta, int16_t distance)
{
    int32_t speed;
    int16_t step;
    int32_t dist32;
    int16_t frames;

    if (movementDelta == 0)
        return 0;

    speed = movementDelta;
    if (speed < 0)
        speed = -speed;

    step = 1;
    while ((uint32_t)speed < 0x10000u) {
        speed <<= 1;
        step <<= 1;
    }

    /* Convert distance to 16.16 fixed point (swap word) */
    dist32 = (int32_t)((uint32_t)(uint16_t)distance << 16);

    frames = 0;
    do {
        frames += step;
        dist32 -= speed;
    } while (dist32 >= 0);

    return frames;
}


/* ==================================================================
 * _ShouldGoalkeeperDive
 *   from: ShouldGoalkeeperDive
 *   in:  keeper (A1), ball (A2), team (A6)
 *   out: 1 = should dive, 0 = no dive
 * ================================================================== */
int16_t __attribute__((used, externally_visible))
_ShouldGoalkeeperDive(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team)
{
    int16_t dy;
    int16_t dx;
    int16_t randVal;
    int16_t threshold;
    int16_t framesY, framesX, framesDive;
    GoalieSkill *shotTable;
    int16_t skillIndex;
    int32_t diveDelta;

    /* Compute signed distance ball.Y - keeper.Y, negate for bottom team */
    dy = ball->y.s.i - keeper->y.s.i;
    if (team != &topTeamData)
        dy = -dy;

    /* Ball in front of keeper (dy < 0): if within 10 pixels, dive */
    if (dy < 0) {
        if (dy < -10)
            return 0;   /* too far in front */
        return 1;       /* close enough, dive */
    }

    /* dy >= 0 : ball is behind or level with keeper */
    if (dy < 0)
        return 0;       /* unreachable but mirrors asm bmi branch */

    /* Penalty distance gate */
    if (playingPenalties || penalty) {
        threshold = kKeeperPenaltySaveDistanceFar;
        randVal = (int16_t)_Rand();
        if (randVal & 0x18) {
            threshold = kKeeperPenaltySaveDistanceNear;
        }
        if (dy > threshold)
            return 0;
        return 1;
    }

    /* Open play distance gate */
    threshold = kKeeperSaveDistance;
    if (dy > threshold)
        return 0;

    /* Compute absolute ball dy for frames calculation */
    {
        int16_t absBallDy = ball->y.s.i - keeper->y.s.i;
        if (absBallDy < 0) absBallDy = -absBallDy;

        framesY = getFramesNeededToCoverDistance(ball->deltaY.raw, absBallDy);
        if (framesY == 0)
            return 0;
    }

    /* Compute frames for keeper to reach ballDefensiveX */
    dx = ballDefensiveX - keeper->x.s.i;
    if (dx < 0) dx = -dx;

    framesX = getFramesNeededToCoverDistance(keeper->deltaX.raw, dx);
    if (framesX == 0)
        return 0;

    /* If keeper can reach defensive X faster than ball reaches keeper Y, no dive */
    if ((uint16_t)framesX <= (uint16_t)framesY)
        return 0;

    /* Evaluate dive using goalkeeper skill from shot chance table */
    dx = ballDefensiveX - keeper->x.s.i;
    if (dx < 0) dx = -dx;

    shotTable = team->shotChanceTable;
    /* tick & 0x3C → 0,4,8,...,60; >> 2 → 0,1,2,...,15 word index into skill index table */
    skillIndex = shotTable->index[((uint16_t)currentGameTick & 0x3C) >> 2];

    diveDelta = kGoalkeeperDiveDeltas[skillIndex];
    goalkeeperDiveAttemptCount++;

    framesDive = getFramesNeededToCoverDistance(diveDelta, dx);
    if (framesDive == 0)
        return 0;
    if ((uint16_t)framesDive < (uint16_t)framesY)
        return 0;

    return 1;
}


/* ==================================================================
 * _GoalkeeperDeflectedBall
 *   from: GoalkeeperDeflectedBall
 *   in:  ball (A2), team (A6)
 *
 * Sets ball destination with random scatter, picks deflect speed based
 * on goalkeeper skill, adds random Z delta.
 * ================================================================== */
void __attribute__((used, externally_visible))
_GoalkeeperDeflectedBall(Sprite *ball, TeamGeneralInfo *team)
{
    int16_t baseDir;
    int16_t destX, destY;
    int16_t scatter;
    GoalieSkill *shotTable;
    int16_t d1;
    int16_t speed;
    int32_t deltaZ;
    int32_t randZ;

    /* Pick base direction: bottom team = index 0, top team = index 4 */
    baseDir = 0;
    if (team != &bottomTeamData)
        baseDir = 4;

    /* kDefaultDestinations: 8 pairs of (dx, dy) */
    destX = ball->x.s.i + kDefaultDestinations[baseDir * 2];
    ball->destX = destX;
    destY = ball->y.s.i + kDefaultDestinations[baseDir * 2 + 1];
    ball->destY = destY;

    /* Random X scatter: (tick & 0x1F) * 32 - 512 */
    scatter = (int16_t)(((uint16_t)currentGameTick & 0x1F) << 5) - 512;
    ball->destX = (int16_t)(ball->destX + scatter);

    /* Pick deflect speed based on goalkeeper skill */
    shotTable = team->shotChanceTable;
    d1 = (int16_t)(((uint16_t)currentGameTick & 0x3C) >> 2);
    d1 -= shotTable->field_34;
    if (d1 < 0) {
        speed = kGoalkeeperStrongDeflectBallSpeed;
    } else {
        d1 -= shotTable->field_36;
        if (d1 < 0) {
            speed = kGoalkeeperMediumDeflectBallSpeed;
        } else {
            speed = kGoalkeeperWeakDeflectBallSpeed;
        }
    }

    /* Add random speed variation: (tick & 0x1FF) - 256 + speed */
    {
        int16_t randSpeed = (int16_t)((uint16_t)currentGameTick & 0x1FF) - 256;
        ball->speed = (int16_t)(randSpeed + speed);
    }

    /* Compute delta Z with random scatter */
    deltaZ = kGoalkeeperDeflectDeltaZ;
    randZ = (int32_t)((uint32_t)currentGameTick & 0x7F) << 8;
    randZ -= 16384;
    deltaZ += randZ;
    ball->deltaZ.raw = deltaZ;

    _ResetBothTeamSpinTimers();
    _PlayKickSample();
}


/* ==================================================================
 * _NextPenalty
 *   from: NextPenalty
 *
 * Advances penalty shootout state. Checks for end conditions,
 * sets up next round, picks shooter.
 * ================================================================== */
void __attribute__((used, externally_visible))
_NextPenalty(void)
{
    int16_t totalAttempts;
    int16_t *shooterIdx;

    totalAttempts = team1PenaltyAttempts + team2PenaltyAttempts;

    if (totalAttempts >= 10) {
        /* After 10 total attempts, check if both teams have taken equal */
        if (team1PenaltyAttempts != team2PenaltyAttempts)
            goto setup_next;
        /* Equal attempts: end if goals differ */
        if (team1PenaltyGoals != team2PenaltyGoals)
            goto end_shootout;
        goto setup_next;
    }

    /* Before 10 attempts: check if either team can't catch up */
    {
        int16_t team1Remaining = 5 - team1PenaltyAttempts + team1PenaltyGoals;
        if ((uint16_t)team1Remaining < (uint16_t)team2PenaltyGoals)
            goto end_shootout;

        int16_t team2Remaining = 5 - team2PenaltyAttempts + team2PenaltyGoals;
        if ((uint16_t)team2Remaining < (uint16_t)team1PenaltyGoals)
            goto end_shootout;
    }

setup_next:
    ballSprite.z.s.i = 0;

    /* Toggle teamPlayingUp: neg then add 3 (flips 1↔2) */
    teamPlayingUp = -teamPlayingUp + 3;
    teamStarting  = -teamStarting + 3;

    hideBall = 0;
    _InitTeamsData();

    gameState = ST_PENALTIES;
    breakCameraMode = -1;
    cameraDirection = 0;          /* north */
    playerTurnFlags = 0x83;       /* NW, N, NE */
    foulXCoordinate = 336;        /* upper white spot */
    foulYCoordinate = 187;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;

    lastTeamPlayedBeforeBreak = &bottomTeamData;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    cameraXVelocity = 0;
    cameraYVelocity = 0;
    penaltiesTimer = 0;

    /* Pick shooter index: decrement, wrap 0→10 */
    shooterIdx = &team1PenaltyShooterIndex;
    if (bottomTeamData.teamNumber != 1)
        shooterIdx = &team2PenaltyShooterIndex;

    (*shooterIdx)--;
    if (*shooterIdx == 0)
        *shooterIdx = 10;

    /* Load penalty shooter sprite pointer */
    {
        int16_t idx = *shooterIdx;
        penaltyShooterSprite = bottomTeamData.spritesTable[idx];
    }

    /* Increment attempt counter for the appropriate team */
    if (bottomTeamData.teamNumber == 1)
        team1PenaltyAttempts++;
    else
        team2PenaltyAttempts++;
    return;

end_shootout:
    playingPenalties = 0;
    statsTeam1Goals = savedTeam1Goals;
    statsTeam2Goals = savedTeam2Goals;

    /* Determine winner */
    if (team1PenaltyGoals > team2PenaltyGoals)
        winningTeamPtr = &topTeamInGame;
    else
        winningTeamPtr = &bottomTeamInGame;

    _EndOfGame();
}


/* ==================================================================
 * _GoalkeeperClaimedTheBall
 *   from: GoalkeeperClaimedTheBall
 *   in:  keeper (A1), ball (A2), team (A6)
 *
 * Sets keeper-holds-ball state, camera direction, turn flags,
 * game state, stoppage, and optionally passes control to keeper.
 * ================================================================== */
void __attribute__((used, externally_visible))
_GoalkeeperClaimedTheBall(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team)
{
    lastPlayerBeforeGoalkeeper = lastPlayerPlayed;
    lastTeamScored = lastTeamPlayed;

    ball->speed = 0;

    if (team == &bottomTeamData) {
        cameraDirection = 0;         /* north */
        playerTurnFlags = 0xC7;      /* left/right/up combo */
    } else {
        cameraDirection = 4;         /* south */
        playerTurnFlags = 0x7C;      /* left/right/down combo */
    }

    /* If no human player, mask off left+right from turn flags */
    if (team->playerNumber == 0)
        playerTurnFlags &= 0xBB;     /* clear bits for left and right */

    /* forceLeftTeam override */
    if (forceLeftTeam == 1)
        team = &topTeamData;

    gameState = ST_KEEPER_HOLDS_BALL;
    breakCameraMode = -1;
    foulXCoordinate = ballSprite.x.s.i;
    foulYCoordinate = ballSprite.y.s.i;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    lastTeamPlayedBeforeBreak = team;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();

    /* If keeper is not currently diving, give control to keeper */
    if (team->goalkeeperDivingRight != 0)
        return;
    if (team->goalkeeperDivingLeft != 0)
        return;

    team->controlledPlayer = keeper;
    team->ballOutOfPlayOrKeeper = 1;
    keeper->direction = (int16_t)cameraDirection;
    team->goaliePlayingOrOut = 1;
    _UpdatePlayerWithBall((uint8_t *)keeper);

    cameraXVelocity = 0;
    cameraYVelocity = 0;
}


/* ==================================================================
 * _CheckIfGoalkeeperClaimedTheBall
 *   from: CheckIfGoalkeeperClaimedTheBall
 *
 * If gameState == ST_KEEPER_HOLDS_BALL: re-call GoalkeeperClaimedTheBall
 * with the team's goalkeeper sprite. Otherwise enter break idle state.
 * ================================================================== */
void __attribute__((used, externally_visible))
_CheckIfGoalkeeperClaimedTheBall(void)
{
    if (gameState == ST_KEEPER_HOLDS_BALL) {
        TeamGeneralInfo *team = lastTeamPlayedBeforeBreak;
        Sprite *keeper = team->spritesTable[0];  /* first sprite = goalkeeper */
        _GoalkeeperClaimedTheBall(keeper, &ballSprite, team);
        return;
    }

    /* enter_break_idle_state */
    breakCameraMode = -1;
    gameStatePl = ST_STOPPED;
    gameNotInProgressCounterWriteOnly = 0;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    _StopAllPlayers();
    cameraXVelocity = 0;
    cameraYVelocity = 0;
}


/* ==================================================================
 * _GoalkeeperJumping
 *   from: GoalkeeperJumping
 *   in:  D0=diveDir, D1=speedVariant, D3=destDir,
 *        A1=keeper, A2=ball, A6=team
 *
 * Sets goalkeeper dive speed (near/far/random), selects low or high
 * dive animation based on ballNotHighZ, sets destination from
 * kDefaultDestinations table.
 * ================================================================== */
void __attribute__((used, externally_visible))
_GoalkeeperJumping(int16_t diveDir, int16_t speedVariant, int16_t destDir,
                   Sprite *keeper, Sprite *ball, TeamGeneralInfo *team)
{
    /* Set speed based on ball distance and speed variant */
    if (keeper->ballDistance <= 128) {
        keeper->speed = kGoalkeeperNearJumpSpeed;
    } else {
        keeper->speed = kGoalkeeperFarJumpSpeed;
        if (speedVariant != 0) {
            keeper->speed = kGoalkeeperFarJumpSlowerSpeed;
            if (speedVariant != 1) {
                int16_t spd = (int16_t)(currentGameTick & 0xFF);
                spd += kGoalkeeperNearJumpSpeed;
                keeper->speed = spd;
            }
        }
    }

    /* Clear field_46 high byte (big-endian) and goalkeeperDivingRight */
    team->field_46 &= 0xFF;
    team->goalkeeperDivingRight = 0;

    /* Choose dive animation based on ballNotHighZ */
    if (ballNotHighZ > 5) {
        /* High dive */
        keeper->playerState = (uint8_t)PL_GOALIE_DIVING_HIGH;
        if (team == &bottomTeamData)
            _SetPlayerAnimationTable(&rightGoalieJumpingHighAnimTable, keeper);
        else
            _SetPlayerAnimationTable(&leftGoalieJumpingHighAnimTable, keeper);
    } else {
        /* Low dive */
        keeper->playerState = (uint8_t)PL_GOALIE_DIVING_LOW;
        if (team == &bottomTeamData)
            _SetPlayerAnimationTable(&rightGoalieJumpingLowAnimTable, keeper);
        else
            _SetPlayerAnimationTable(&leftGoalieJumpingLowAnimTable, keeper);
    }

    /* Set downtime and controlled player direction */
    keeper->playerDownTimer = (int8_t)75;
    team->controlledPlDirection = diveDir;

    /* Set destination from kDefaultDestinations using destDir */
    int16_t destIdx = destDir * 2;
    keeper->destX = keeper->x.s.i + kDefaultDestinations[destIdx];
    keeper->destY = keeper->y.s.i + kDefaultDestinations[destIdx + 1];
}
