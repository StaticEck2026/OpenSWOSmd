/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "headers.h"
#include "game_types.h"

/* ---- External asm symbols ---- */
extern uint16_t currentGameTick;
extern int16_t kDefaultDestinations[16];
extern int16_t kPlayerHeaderSpeedIncrease[8];
extern uint32_t kBallJumpHeaderDeltaZ;
extern uint16_t kJumpHeaderSpeed;
extern uint32_t kHeaderLowJumpHeight;
extern uint32_t kHeaderHighJumpHeight;
extern uint16_t kStaticHeaderBaseBallSpeed;
extern uint16_t kStaticHeaderPlayerSpeed;

/* Animation tables (asm data section labels) */
extern uint8_t jumpHeaderHitAnimTable;
extern uint8_t jumpHeaderAttemptAnimTable;
extern uint8_t staticHeaderAttemptAnimTable;
extern uint8_t staticHeaderAimAnimTable;
extern uint8_t staticHeaderHitAnimTable;

/* Ball sprite */
extern Sprite ballSprite;

/* Functions from other modules */
extern void _SetPlayerAnimationTable(uint8_t *animTable, Sprite *sprite);
extern void _SetPlayerAnimationTableAndPictureIndex(uint8_t *animTable, Sprite *sprite);
extern PlayerInfo *_GetPlayerPointerFromShirtNumber(Sprite *playerSprite, TeamGeneralInfo *teamData);
extern void _PlayKickSample(void);
extern void _ResetBothTeamSpinTimers(void);


/* ==================================================================
 * setPlayerJumpHeaderHitAnimationTable (static)
 *   from: SetPlayerJumpHeaderHitAnimationTable
 *   in: player (A1)
 *
 * Sets jump-header-hit anim if frameSwitchCounter <= 2.
 * ================================================================== */
static void setPlayerJumpHeaderHitAnimationTable(Sprite *player)
{
    if ((uint16_t)player->frameSwitchCounter > 2)
        return;
    _SetPlayerAnimationTableAndPictureIndex(&jumpHeaderHitAnimTable, player);
}


/* ==================================================================
 * doFlyingHeader (static)
 *   from: DoFlyingHeader
 *   in: player (A1), ball (A2)
 *
 * Flying header: set low jump height delta-Z, reduce ball speed to 75%.
 * ================================================================== */
static void doFlyingHeader(Sprite *player, Sprite *ball)
{
    ball->deltaZ.raw = (int32_t)kHeaderLowJumpHeight;
    uint16_t spd = (uint16_t)ball->speed;
    uint16_t quarter = spd >> 2;
    ball->speed = (int16_t)(spd - quarter);
    setPlayerJumpHeaderHitAnimationTable(player);
}


/* ==================================================================
 * doLobHeader (static)
 *   from: DoLobHeader
 *   in: player (A1), ball (A2)
 *
 * Lob header: set high jump height delta-Z, reduce ball speed to ~94%.
 * ================================================================== */
static void doLobHeader(Sprite *player, Sprite *ball)
{
    ball->deltaZ.raw = (int32_t)kHeaderHighJumpHeight;
    uint16_t spd = (uint16_t)ball->speed;
    uint16_t sixteenth = spd >> 4;
    ball->speed = (int16_t)(spd - sixteenth);
    setPlayerJumpHeaderHitAnimationTable(player);
}


/* ==================================================================
 * _SetJumpHeaderHitAnimTable
 *   from: SetJumpHeaderHitAnimTable
 *   in: player (A1)
 *
 * Conditionally sets the jump-header-hit animation table on a player.
 * Checks: not already set, heading==0, playerDownTimer==40,
 * frameSwitchCounter<=2, currentGameTick bit 1 set.
 * ================================================================== */
void __attribute__((used, externally_visible))
_SetJumpHeaderHitAnimTable(Sprite *player)
{
    if (player->animationTable == (uint32_t)&jumpHeaderHitAnimTable)
        return;
    if (player->heading != 0)
        return;
    if ((uint8_t)player->playerDownTimer != 40)
        return;
    if ((uint16_t)player->frameSwitchCounter > 2)
        return;
    /* btst #1, currentGameTick+1(pc) — tests bit 1 of low byte */
    if (!(((uint8_t *)&currentGameTick)[1] & 0x02))
        return;
    _SetPlayerAnimationTableAndPictureIndex(&jumpHeaderHitAnimTable, player);
    uint16_t spd = (uint16_t)player->speed;
    player->speed = (int16_t)(spd >> 1);
}


/* ==================================================================
 * _PlayerHittingJumpHeader
 *   from: PlayerHittingJumpHeader
 *   in: player (A1), team (A6)
 *
 * Hitting flying or lob header. Determines direction deflection based
 * on input vs player direction delta (mod 8), calls DoFlyingHeader or
 * DoLobHeader accordingly, then sets ball destination and speed.
 * ================================================================== */
void __attribute__((used, externally_visible))
_PlayerHittingJumpHeader(Sprite *player, TeamGeneralInfo *team)
{
    team->passInProgress = 0;

    int16_t inputDir = team->currentAllowedDirection;
    if (inputDir < 0)
        inputDir = player->direction;

    Sprite *ball = &ballSprite;
    ball->deltaZ.raw = (int32_t)kBallJumpHeaderDeltaZ;

    /* Ball speed = player speed * 1.25 */
    uint16_t plSpeed = (uint16_t)player->speed;
    ball->speed = (int16_t)plSpeed;
    uint16_t quarter = plSpeed >> 2;
    ball->speed = (int16_t)(plSpeed + quarter);

    int16_t playerDir = player->direction;
    int16_t delta = (playerDir - inputDir);

    int16_t inputAllowed = team->currentAllowedDirection;
    int16_t finalDir;

    if (inputAllowed < 0) {
        /* No input direction: flying header, keep player direction */
        doFlyingHeader(player, ball);
        goto keep_dir;
    }

    delta &= 7;

    switch (delta) {
    case 0: /* Same direction: no height modification */
        goto keep_dir;

    case 1: /* Slight offset: decrement direction */
        goto decrement_dir;

    case 2: /* Flying header + decrement */
        doFlyingHeader(player, ball);
        goto decrement_dir;

    case 3: /* Lob header + decrement */
        doLobHeader(player, ball);
        goto decrement_dir;

    case 4: /* Opposite direction: lob header, keep direction */
        doLobHeader(player, ball);
        goto keep_dir;

    case 5: /* Lob header + increment */
        doLobHeader(player, ball);
        goto increment_dir;

    case 6: /* Flying header + increment */
        doFlyingHeader(player, ball);
        goto increment_dir;

    case 7: /* Slight offset: increment direction */
        goto increment_dir;
    }

increment_dir:
    finalDir = (player->direction + 1) & 7;
    goto apply_dir;

decrement_dir:
    finalDir = (player->direction - 1) & 7;
    goto apply_dir;

keep_dir:
    finalDir = player->direction;

apply_dir:
    finalDir &= 7;
    team->controlledPlDirection = finalDir;

    /* Set ball destination from direction lookup table */
    int16_t dirIdx = finalDir * 2; /* index into kDefaultDestinations (pairs of x,y) */
    int16_t destX = kDefaultDestinations[dirIdx] + ball->x.s.i;
    ball->destX = destX;
    int16_t destY = kDefaultDestinations[dirIdx + 1] + ball->y.s.i;
    ball->destY = destY;

    /* Apply heading attribute speed increase. */
    PlayerInfo *pi = _GetPlayerPointerFromShirtNumber(player, team);
    int8_t headingSkill = pi->heading;
    int16_t speedBonus = kPlayerHeaderSpeedIncrease[(int16_t)headingSkill];
    uint16_t ballSpeed = (uint16_t)ball->speed;
    ball->speed = (int16_t)(ballSpeed + speedBonus);

    /* Halve player speed, mark heading done */
    uint16_t pSpeed = (uint16_t)player->speed;
    player->speed = (int16_t)(pSpeed >> 1);
    player->heading = 1;

    _PlayKickSample();
    _ResetBothTeamSpinTimers();
}


/* ==================================================================
 * _SetStaticHeaderDirection
 *   from: SetStaticHeaderDirection
 *   in: player (A1), team (A6)
 *
 * Steers player direction toward input direction during static header
 * attempt. Adjusts direction by ±1 per frame.
 * ================================================================== */
void __attribute__((used, externally_visible))
_SetStaticHeaderDirection(Sprite *player, TeamGeneralInfo *team)
{
    int16_t inputDir = team->currentAllowedDirection;
    if (inputDir < 0)
        return;
    if ((uint8_t)player->playerDownTimer > 18)
        return;
    if (player->animationTable != (uint32_t)&staticHeaderAttemptAnimTable)
        return;

    int16_t delta = inputDir - player->direction;
    if (delta != 0) {
        delta &= 7;
        if (delta != 4) {
            if (delta < 4) {
                /* bcs: delta < 4 → increment */
                int16_t d = player->direction + 1;
                player->direction = d;
            } else {
                /* delta > 4 → decrement */
                int16_t d = player->direction - 1;
                player->direction = d;
            }
            int16_t d = player->direction & 7;
            player->direction = d;
        }
    }

    _SetPlayerAnimationTableAndPictureIndex(&staticHeaderAimAnimTable, player);
}


/* ==================================================================
 * _PlayerHittingStaticHeader
 *   from: PlayerHittingStaticHeader
 *   in: player (A1), team (A6)
 *
 * Handles ball contact during static header. Adjusts player direction
 * toward input (1-2 steps), sets ball destination and speed, applies
 * heading skill bonus, negates and halves deltaZ.
 * ================================================================== */
void __attribute__((used, externally_visible))
_PlayerHittingStaticHeader(Sprite *player, TeamGeneralInfo *team)
{
    team->passInProgress = 0;

    /* Skip direction adjustment if already in hit anim */
    if (player->animationTable == (uint32_t)&staticHeaderHitAnimTable)
        goto apply_hit;

    /* Compare input direction with player direction */
    int16_t inputDir = team->currentAllowedDirection;
    int16_t playerDir = player->direction;
    int16_t delta = inputDir - playerDir;
    if (delta == 0)
        goto apply_hit;

    delta &= 7;
    if (delta == 4)
        goto apply_hit;

    if (delta < 4) {
        /* bcs: delta < 4 → increment path (turn toward input) */
        inputDir = team->currentAllowedDirection;
        int16_t d = player->direction + 1;
        player->direction = (int16_t)(d & 7);
        if (player->direction != inputDir) {
            d = player->direction + 1;
            player->direction = d;
        }
    } else {
        /* delta > 4 → decrement path */
        inputDir = team->currentAllowedDirection;
        int16_t d = player->direction - 1;
        player->direction = (int16_t)(d & 7);
        if (player->direction != inputDir) {
            d = player->direction - 1;
            player->direction = d;
        }
    }

    /* Final wrap to 0-7 */
    player->direction = (int16_t)(player->direction & 7);

apply_hit:
    _SetPlayerAnimationTableAndPictureIndex(&staticHeaderHitAnimTable, player);

    /* Determine shooting direction: use input if available, else player dir */
    int16_t shootDir = team->currentAllowedDirection;
    if (shootDir < 0)
        shootDir = player->direction;

    Sprite *ball = &ballSprite;
    team->controlledPlDirection = shootDir;

    /* Set ball destination from direction table */
    int16_t dirIdx = shootDir * 2;
    int16_t destX = kDefaultDestinations[dirIdx] + ball->x.s.i;
    ball->destX = destX;
    int16_t destY = kDefaultDestinations[dirIdx + 1] + ball->y.s.i;
    ball->destY = destY;

    /* Set base ball speed for static header */
    ball->speed = (int16_t)kStaticHeaderBaseBallSpeed;

    /* Apply heading attribute speed increase. */
    PlayerInfo *pi = _GetPlayerPointerFromShirtNumber(player, team);
    int8_t headingSkill = pi->heading;
    int16_t speedBonus = kPlayerHeaderSpeedIncrease[(int16_t)headingSkill];
    uint16_t ballSpeed = (uint16_t)ball->speed;
    ball->speed = (int16_t)(ballSpeed + speedBonus);

    /* Negate and halve deltaZ */
    int32_t dz = ball->deltaZ.raw;
    dz = (-dz) >> 1;
    ball->deltaZ.raw = dz;

    /* Mark heading done */
    player->heading = 1;

    _PlayKickSample();
    _ResetBothTeamSpinTimers();
}


/* ==================================================================
 * _AttemptStaticHeader
 *   from: AttemptStaticHeader
 *   in: player (A1), dir (D0)
 *
 * Player jumps in place trying to hit static header. Sets animation,
 * destination, speed, state, and down timer.
 * ================================================================== */
void __attribute__((used, externally_visible))
_AttemptStaticHeader(Sprite *player, uint16_t dir)
{
    player->heading = 0;
    player->direction = (int16_t)dir;

    /* Set destination from direction table */
    int16_t dirIdx = (int16_t)(dir * 2);
    int16_t destX = kDefaultDestinations[dirIdx] + player->x.s.i;
    player->destX = destX;
    int16_t destY = kDefaultDestinations[dirIdx + 1] + player->y.s.i;
    player->destY = destY;

    player->speed = (int16_t)kStaticHeaderPlayerSpeed;

    _SetPlayerAnimationTable(&staticHeaderAttemptAnimTable, player);
    player->playerState = PL_STATIC_HEADING;
    player->playerDownTimer = 20;
}


/* ==================================================================
 * _PlayerAttemptingJumpHeader
 *   from: PlayerAttemptingJumpHeader
 *   in: dir (D0), player (A1)
 *
 * Player starts jumping to attempt to head the ball. Sets animation,
 * state, down timer, destination, and speed.
 * ================================================================== */
void __attribute__((used, externally_visible))
_PlayerAttemptingJumpHeader(uint16_t dir, Sprite *player)
{
    player->heading = 0;
    player->direction = (int16_t)dir;

    _SetPlayerAnimationTable(&jumpHeaderAttemptAnimTable, player);
    player->playerDownTimer = 50;
    player->playerState = PL_JUMP_HEADING;

    /* Set destination from direction table */
    int16_t dirIdx = (int16_t)(dir * 2);
    int16_t destX = kDefaultDestinations[dirIdx] + player->x.s.i;
    player->destX = destX;
    int16_t destY = kDefaultDestinations[dirIdx + 1] + player->y.s.i;
    player->destY = destY;

    player->speed = (int16_t)kJumpHeaderSpeed;
}
