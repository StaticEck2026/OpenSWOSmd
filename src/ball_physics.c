/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "ball_physics.h"

/* Sprite image constants */
#define SPR_BALL_SHADOW             1069

/* ---- External globals (asm data) ---- */
extern Sprite ballSprite;               /* 110-byte sprite */
extern Sprite ballShadowSprite;         /* 110-byte sprite */
extern TeamGeneralInfo topTeamData;     /* TeamGeneralInfo */
extern TeamGeneralInfo bottomTeamData;  /* TeamGeneralInfo */
extern Sprite goalie1Sprite;            /* 110-byte sprite */
extern Sprite goalie2Sprite;            /* 110-byte sprite */

/* Storage is uint16_t [N] in swos_amiga_data_imports.c, but the
 * frame-table walker reads the entries as int16_t (the -1 terminator
 * needs sign semantics). Re-declared here with the matching type so
 * `(int16_t *)ballMovingFrameIndices` decays the array to a pointer
 * correctly rather than going through `&scalar` indirection. */
extern int16_t ballStaticFrameIndices[];
extern int16_t ballMovingFrameIndices[];

extern int16_t  kBallGroundConstant;
extern int16_t  kBallAirConstant;
extern int32_t  kGravityConstant;
extern uint16_t ballSpeedBounceFactor;
extern uint16_t ballBounceFactor;
extern int16_t  pitchBallSpeedFactor;
extern uint16_t writeOnlyVar04;

extern int16_t  goalkeeperDiveBallHeights[];  /* 28 entries */
extern int16_t  kGoalKeeperClaimingBallHeight[];  /* 7 entries */

extern uint16_t hideBall;
extern uint16_t gameState;
extern uint16_t gameStatePl;
extern uint16_t substituteInProgress;
extern uintptr_t teamThatSubstitutes;     /* TeamGeneralInfo*-shaped pointer; retyped uintptr_t for 64-bit. */
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak; /* pointer to team */

extern int16_t  foulXCoordinate;
extern int16_t  foulYCoordinate;

extern int16_t  ballNextX;
extern int16_t  ballNextY;

extern uint16_t currentGameTick;

extern uint16_t injuriesForever;

extern uint16_t panicReasonCode;

/* quadrant data labels (in SWOS2.s code section) */
extern int16_t  ballXQuadrantLimits[];   /* 5 entries + 1 pad */
extern int16_t  ballYQuadrantLimits[];   /* 6 entries + 1 pad */
extern int16_t  ballQuadrantIndex;
extern int16_t  ballXQuadrantDead;
extern int16_t  ballYQuadrantDead;
extern int16_t  playerXQuadrantOffset;
extern int16_t  playerYQuadrantOffset;

/* player offset tables (in SWOS2.s code section) */
extern int16_t  kBallPlOffsets[];        /* 16 entries */
extern int16_t  kPlayerWithBallOffsets[]; /* 16 entries */

/* ingameTeamPlayerOffsets (in SWOS2.s data section) */
extern uint16_t ingameTeamPlayerOffsets[];

/* ---- Ball prediction globals ---- */
extern int16_t  ballDefensiveX;
extern int16_t  ballDefensiveY;
extern int16_t  ballDefensiveZ;
extern int16_t  ballNotHighX;
extern int16_t  ballNotHighY;
extern int16_t  ballNotHighZ;
extern int16_t  strikeDestX;
extern int16_t  strikeDestY;
extern int16_t  strikeDestZ;

extern int16_t  ballNextGroundX;
extern int16_t  ballNextYGroundY;
extern int16_t  ballNextZDeadVar;

/* ---- Spin/aftertouch tables ---- */
extern int16_t  kKickSpinFactor[];         /* 8 dirs × 2 entries (left/right) × 2 (dx,dy) = 32 words.
                                             * ASM data uses dc.b but reads with move.w — byte pairs
                                             * form big-endian int16_t values. int16_t[] is correct. */
extern int16_t  kPassingSpinFactor[];      /* 8 dirs × 4 entries = 32 words */
extern int16_t  kSpinMultiplierFactor[];   /* 10 entries */
extern int32_t  kHighKickDeltaZ;
extern int16_t  kHighKickBallSpeed;
extern int32_t  kNormalKickDeltaZ;
extern int16_t  kNormalKickBallSpeed;

/* ---- Extern function declarations ---- */
extern void _CheckIfBallOutOfPlay(void);
extern void _PlaySample3IfNotTraining(void);
extern void _PlaySfxSample16(void);
extern int16_t _CalculateDeltaXAndY(int16_t destX, int16_t destY,
                                     int16_t srcX, int16_t srcY,
                                     int16_t speed,
                                     int32_t *outDeltaX, int32_t *outDeltaY);
extern void _PanicHaltLoop(void);

/* ---- Static helpers ---- */

static void reverseDestXDirection(Sprite *spr, int32_t savedX)
{
    int16_t intX = (int16_t)(savedX >> 16);
    int16_t destX = spr->destX;
    int16_t delta = destX - intX;
    spr->destX = intX - delta;
}

static void reverseDestYDirection(Sprite *spr, int32_t savedY)
{
    int16_t intY = (int16_t)(savedY >> 16);
    int16_t destY = spr->destY;
    int16_t delta = destY - intY;
    spr->destY = intY - delta;
}


/* ==================================================================
 * _ResetBothTeamSpinTimers
 * ================================================================== */
void __attribute__((used, externally_visible))
_ResetBothTeamSpinTimers(void)
{
    topTeamData.spinTimer = (int16_t)0xFFFF;
    bottomTeamData.spinTimer = (int16_t)0xFFFF;
}


/* ==================================================================
 * _GetPlayerPointerFromShirtNumber
 *   in: playerSprite (A1), teamData (A6)
 *   out: PlayerInfo pointer for this player
 *
 * ASM returns a4 = teamGamePtr + shirtIdx*61 (42 bytes before PlayerInfo).
 * C version returns the PlayerInfo directly using TeamGame struct access.
 * ================================================================== */
PlayerInfo * __attribute__((used, externally_visible))
_GetPlayerPointerFromShirtNumber(Sprite *playerSprite, TeamGeneralInfo *teamData)
{
    uint16_t ord = (uint16_t)playerSprite->playerOrdinal;
    uint16_t idx = ord - 1;
    TeamGame *tg = (TeamGame *)teamData->inGameTeamPtr;
    return &tg->players[idx];
}


/* ==================================================================
 * _UpdateControllingPlayer
 *   in: playerSprite (A1) - player that controls the ball
 *
 * Set ball coordinates based on controlling player direction.
 * Ball is placed 1 pixel in front of the player.
 * ================================================================== */
void __attribute__((used, externally_visible))
_UpdateControllingPlayer(Sprite *playerSprite)
{
    int16_t dir = playerSprite->direction;
    int16_t idx = dir * 2; /* pairs of (x,y) */
    int16_t d1 = playerSprite->x.s.i + kBallPlOffsets[idx];
    int16_t d2 = playerSprite->y.s.i + kBallPlOffsets[idx + 1];

    Sprite *ball = &ballSprite;
    ball->speed = (int16_t)0;
    ball->x.s.i = d1;
    ball->y.s.i = d2;
    ball->destX = d1;
    ball->destY = d2;
    ball->z.s.i = (int16_t)0;

    /* halve deltaZ */
    int32_t dz = ball->deltaZ.raw;
    dz >>= 1;
    ball->deltaZ.raw = (int32_t)dz;

    _ResetBothTeamSpinTimers();
}


/* ==================================================================
 * _UpdateBallWithControllingGoalkeeper
 *   in: playerSprite (A1) - goalkeeper holding the ball
 *
 * Same as UpdateControllingPlayer but deltaZ is forced negative
 * (bring the ball down).
 * ================================================================== */
void __attribute__((used, externally_visible))
_UpdateBallWithControllingGoalkeeper(Sprite *playerSprite)
{
    int16_t dir = playerSprite->direction;
    int16_t idx = dir * 2;
    int16_t d1 = playerSprite->x.s.i + kBallPlOffsets[idx];
    int16_t d2 = playerSprite->y.s.i + kBallPlOffsets[idx + 1];

    Sprite *ball = &ballSprite;
    ball->speed = (int16_t)0;
    ball->x.s.i = d1;
    ball->y.s.i = d2;
    ball->destX = d1;
    ball->destY = d2;

    /* halve deltaZ, force negative (bring ball down) */
    int32_t dz = ball->deltaZ.raw;
    dz >>= 1;
    if (dz >= 0)
        dz = -dz;
    ball->deltaZ.raw = (int32_t)dz;

    _ResetBothTeamSpinTimers();
}


/* ==================================================================
 * _UpdatePlayerWithBall
 *   in: playerSprite (A1) - player sprite
 *
 * Offset player position relative to ball by direction-based table.
 * ================================================================== */
void __attribute__((used, externally_visible))
_UpdatePlayerWithBall(Sprite *playerSprite)
{
    int16_t d1 = ballSprite.x.s.i;
    int16_t d2 = ballSprite.y.s.i;

    int16_t dir = playerSprite->direction;
    int16_t idx = dir * 2;
    d1 += kPlayerWithBallOffsets[idx];
    d2 += kPlayerWithBallOffsets[idx + 1];

    playerSprite->x.s.i = d1;
    playerSprite->y.s.i = d2;
    playerSprite->destX = d1;
    playerSprite->destY = d2;

    _ResetBothTeamSpinTimers();
}


/* ==================================================================
 * _SetThrowInPlayerDestinationCoordinates
 *   in: playerSprite (A1)
 *
 * Position throw-in player 3 pixels offset from ball.
 * ================================================================== */
void __attribute__((used, externally_visible))
_SetThrowInPlayerDestinationCoordinates(Sprite *playerSprite)
{
    int16_t d1 = ballSprite.x.s.i;
    if ((uint16_t)d1 < 336)
        d1 -= 3;
    else
        d1 += 3;

    playerSprite->x.s.i = d1;
    playerSprite->destX = d1;

    int16_t by = ballSprite.y.s.i;
    playerSprite->y.s.i = by;
    playerSprite->destY = by;
}


/* ==================================================================
 * _InjuryUpdateTickGuard
 *
 * If injuriesForever is set, skip the actual injury update (the asm
 * code after this function is dead). This is just a guard stub.
 * ================================================================== */
void __attribute__((used, externally_visible))
_InjuryUpdateTickGuard(void)
{
    /* Original: tst.w (injuriesForever); beq.w *+4; rts
       When injuriesForever == 0, falls through to injury update code.
       When non-zero, returns immediately (skips injury update).
       Since the injury update code following this is still in asm,
       this C function is the complete guard; the asm stub will handle
       the fall-through case. */
    (void)injuriesForever; /* nothing to do in C — asm stub handles flow */
}


/* ==================================================================
 * _CalculateNextBallPosition
 *
 * Predicts where the ball will land (Z becomes negative) by
 * integrating position with scaled deltas. Stores result in
 * ballNextX / ballNextY.
 * ================================================================== */
void __attribute__((used, externally_visible))
_CalculateNextBallPosition(void)
{
    Sprite *ball = &ballSprite;
    int32_t d5 = ball->x.raw;
    int32_t d6 = ball->y.raw;
    int32_t d7 = ball->z.raw;

    if ((uint16_t)ball->speed == 0)
        goto store;

    {
        int32_t d1 = ball->deltaX.raw;
        int32_t d2 = ball->deltaY.raw;
        int32_t d3 = ball->deltaZ.raw;
        int32_t d4 = kGravityConstant;

        if (d3 > 0)
            goto scale8;

        {
            int16_t zInt = ball->z.s.i;
            if ((uint16_t)zInt <= 20)
                goto scale1;
            if ((uint16_t)zInt <= 30)
                goto scale2;
            if ((uint16_t)zInt <= 35)
                goto scale4;
        }

    scale8:
        d1 <<= 3;
        d2 <<= 3;
        d4 <<= 3;
        d7 >>= 3;
        do {
            d5 += d1;
            d6 += d2;
            d3 -= d4;
            d7 += d3;
        } while (d7 >= 0);
        goto store;

    scale4:
        d1 <<= 2;
        d2 <<= 2;
        d4 <<= 2;
        d7 >>= 2;
        do {
            d5 += d1;
            d6 += d2;
            d3 -= d4;
            d7 += d3;
        } while (d7 >= 0);
        goto store;

    scale2:
        d1 <<= 1;
        d2 <<= 1;
        d4 <<= 1;
        d7 >>= 1;
        do {
            d5 += d1;
            d6 += d2;
            d3 -= d4;
            d7 += d3;
        } while (d7 >= 0);
        goto store;

    scale1:
        do {
            d5 += d1;
            d6 += d2;
            d3 -= d4;
            d7 += d3;
        } while (d7 >= 0);
    }

store:
    /* swap words to get integer part */
    ballNextX = (int16_t)((uint32_t)d5 >> 16);
    ballNextY = (int16_t)((uint32_t)d6 >> 16);
}


/* ==================================================================
 * _DoGoalkeeperSprites
 *
 * When a goalkeeper is diving, position the ball relative to the
 * goalkeeper sprite using lookup tables for ball height.
 * ================================================================== */
void __attribute__((used, externally_visible))
_DoGoalkeeperSprites(void)
{
    TeamGeneralInfo *team;
    int16_t yOff;
    Sprite *gkSprite;
    int16_t baseImgIdx;

    /* Check top team diving right */
    team = &topTeamData;
    if (team->goalkeeperDivingRight != 0) {
        yOff = 1;
        goto diving_right;
    }
    if (team->goalkeeperDivingLeft != 0)
        goto goalie1_diving_left;

    /* Check bottom team */
    team = &bottomTeamData;
    if (team->goalkeeperDivingRight != 0) {
        yOff = -1;
        goto diving_right;
    }
    if (team->goalkeeperDivingLeft != 0)
        goto goalie2_diving_left;

    return; /* no goalkeeper diving */

diving_right:
    /* Determine which goalie sprite and base image index */
    gkSprite = &goalie1Sprite;
    baseImgIdx = 0x359;
    {
        int16_t img = gkSprite->imageIndex;
        if (img >= 0x393 && img < 0x3AF)
            baseImgIdx = 0x393;
    }
    if (team->teamNumber != 1) {
        gkSprite = &goalie2Sprite;
        baseImgIdx = 0x3CD;
        {
            int16_t img = gkSprite->imageIndex;
            if (img >= 0x407 && img < 0x423)
                baseImgIdx = 0x407;
        }
    }
    goto position_ball_diving;

goalie1_diving_left:
    yOff = 1;
    goto diving_left_common;
goalie2_diving_left:
    yOff = -1;
diving_left_common:
    gkSprite = &goalie1Sprite;
    baseImgIdx = 0x359;
    if (team->teamNumber != 1) {
        gkSprite = &goalie2Sprite;
        baseImgIdx = 0x3CD;
    }
    {
        /* Ball position from keeper sprite */
        ballSprite.x.raw = gkSprite->x.raw;
        int16_t bally = gkSprite->y.s.i + yOff;
        ballSprite.y.s.i = bally;
        ballSprite.z.s.i = (int16_t)0;
        ballSprite.deltaZ.raw = (int32_t)0;
        ballSprite.speed = (int16_t)0;

        /* Look up ball height from claiming table using frameSwitchCounter */
        int16_t fsc = gkSprite->frameSwitchCounter;
        int16_t ballZ = kGoalKeeperClaimingBallHeight[fsc];
        ballSprite.z.s.i = ballZ;
    }
    return;

position_ball_diving:
    {
        /* Ball position from keeper sprite */
        ballSprite.x.raw = gkSprite->x.raw;
        int16_t bally = gkSprite->y.s.i + yOff;
        ballSprite.y.s.i = bally;
        ballSprite.z.s.i = (int16_t)0;
        ballSprite.deltaZ.raw = (int32_t)0;
        ballSprite.speed = (int16_t)0;

        /* Compute frame offset from base */
        int16_t frameOff = gkSprite->imageIndex - baseImgIdx;
        if ((uint16_t)frameOff >= 28) { /* 28 entries in goalkeeperDiveBallHeights */
            /* frame offset out of range — panic */
            panicReasonCode = 0x237;
            _PanicHaltLoop();
        }

        /* Look up ball height from dive table */
        int16_t ballZ = goalkeeperDiveBallHeights[frameOff];
        ballSprite.z.s.i = ballZ;
    }
}


/* ==================================================================
 * _UpdateBall
 *
 * Core ball physics: animation, movement, bouncing, goal collisions,
 * shadow update, quadrant calculation for AI tactics.
 *
 * RE bug fix: line 9517 in SWOS2.s has `move.l #(frontbufferBaseAlt+$1CAA),d3`
 * which is an IDA address-as-constant bug. The correct value is 0x00010002
 * (positive counterpart of 0xFFFEFFFE used on the Z>5 branch).
 * ================================================================== */
void __attribute__((used, externally_visible))
_UpdateBall(void)
{
    Sprite *ball = &ballSprite;

    /* ---- Hide ball if requested ---- */
    if (hideBall) {
        ball->imageIndex = (int16_t)-1;
        ballShadowSprite.imageIndex = (int16_t)-1;
        goto has_picture;
    }

    /* ---- Show shadow ---- */
    ballShadowSprite.imageIndex = SPR_BALL_SHADOW;

    /* ---- Select frame table (static vs moving) ---- */
    if (ball->deltaX.raw != 0 || ball->deltaY.raw != 0) {
        /* ball is moving */
        if (ball->frameIndicesTable != ballMovingFrameIndices) {
            ball->frameIndicesTable = ballMovingFrameIndices;
            ball->frameIndex = (int16_t)0;
        }
    } else {
        /* ball is stationary */
        if (ball->frameIndicesTable != ballStaticFrameIndices) {
            ball->frameIndicesTable = ballStaticFrameIndices;
            ball->frameIndex = (int16_t)0;
        }
    }

    /* ---- Advance ball animation frame ---- */
    {
        uint16_t speed = (uint16_t)ball->speed;
        if (speed != 0) {
            uint16_t decr = (speed >> 9) + 1; /* speed/512 + 1 */
            int16_t timer = ball->cycleFramesTimer - (int16_t)decr;
            ball->cycleFramesTimer = timer;
            if (timer < 0) {
                ball->frameIndex = ball->frameIndex + 1;
                ball->cycleFramesTimer = ball->frameDelay;
                goto extract_frame;
            }
        }
        /* stopped or timer not expired */
        if (ball->imageIndex >= 0)
            goto has_picture;
    }

extract_frame:
    {
        int16_t *table = ball->frameIndicesTable;
        int16_t fi = ball->frameIndex;
        int16_t frame = table[fi];
        if (frame >= 0) {
            ball->imageIndex = frame;
        } else {
            /* negative = reset loop */
            ball->frameIndex = (int16_t)0;
            frame = table[0];
            ball->imageIndex = frame;
        }
    }

has_picture:
    ;
    /* Save positions before movement for bounce recovery */
    int32_t savedX = ball->x.raw;
    int32_t savedY = ball->y.raw;
    int32_t savedZ = ball->z.raw;

    /* ---- Calculate movement direction ---- */
    {
        int16_t destX = ball->destX;
        int16_t destY = ball->destY;
        int16_t srcX  = ball->x.s.i;
        int16_t srcY  = ball->y.s.i;
        int16_t speed = ball->speed;

        int32_t outDX, outDY;
        int16_t dir = _CalculateDeltaXAndY(destX, destY, srcX, srcY, speed, &outDX, &outDY);

        ball->deltaX.raw = (int32_t)outDX;
        ball->deltaY.raw = (int32_t)outDY;

        if (dir >= 0) {
            ball->fullDirection = dir;
            dir = (int16_t)(((uint16_t)(dir + 16) & 0xFF) >> 5);
        }
        ball->direction = dir;
    }

    /* ---- Apply friction ---- */
    {
        int16_t friction = kBallGroundConstant;
        if (topTeamData.playerHasBall == 0 &&
            bottomTeamData.playerHasBall == 0) {
            friction += pitchBallSpeedFactor;
        }
        if (ball->z.s.i != 0) {
            friction = kBallAirConstant;
        }
        int16_t spd = ball->speed - friction;
        if (spd < 0) spd = 0;
        ball->speed = spd;
    }

    /* ---- Apply deltas to position ---- */
    {
        int32_t dx = ball->deltaX.raw;
        int32_t dy = ball->deltaY.raw;
        int32_t dz = ball->deltaZ.raw;

        ball->x.raw = ball->x.raw + dx;
        ball->y.raw = ball->y.raw + dy;

        /* ---- Keeper holds ball: special Z handling ---- */
        if (gameState == ST_KEEPER_HOLDS_BALL) {
            if (substituteInProgress) {
                TeamGeneralInfo *subTeam = (TeamGeneralInfo *)teamThatSubstitutes;
                if (subTeam == lastTeamPlayedBeforeBreak) {
                    Sprite *ctrlPl = subTeam->controlledPlayer;
                    if (ctrlPl == 0) {
                        ball->z.s.i = (int16_t)0;
                        goto clear_deltaz;
                    }
                }
            }

            /* Keeper holds ball: move ball with player */
            if ((uint16_t)ball->speed != 0) {
                writeOnlyVar04++;
                Sprite *ctrlPlayer = lastTeamPlayedBeforeBreak->controlledPlayer;
                _UpdateBallWithControllingGoalkeeper(ctrlPlayer);
            }

            {
                int16_t bz = ball->z.s.i;
                if (bz == 5) goto clear_deltaz;
                if ((uint16_t)bz < 6) {
                    dz = 0x00020004;
                } else {
                    dz = (int32_t)0xFFFEFFFE;
                }
            }
            goto apply_z;
        }

        /* ---- Normal ball: apply gravity ---- */
        if (dz == 0)
            goto after_z;

        dz -= kGravityConstant;
        dz |= 1;

    apply_z:
        ball->z.raw = ball->z.raw + dz;
        if (ball->z.raw >= 0)
            goto after_z;

        /* Ball hit the ground — bounce */
        {
            uint16_t spd = (uint16_t)ball->speed;
            uint32_t decr = ((uint32_t)spd * (uint32_t)ballSpeedBounceFactor) >> 8;
            ball->speed = (int16_t)(spd - (uint16_t)decr);
        }
        ball->z.s.i = (int16_t)0;

        /* Reverse and dampen deltaZ */
        dz = -dz;
        {
            int32_t frac = dz >> 8;
            uint32_t dampen = (uint32_t)(uint16_t)frac * (uint32_t)ballBounceFactor;
            dz -= (int32_t)dampen;
            dz |= 1;
        }

        if ((uint32_t)dz <= 0xA000) {
            goto clear_deltaz;
        }

        /* Play bounce sound (sample 0x10, not crowd "oooh" 0x2C) */
        _PlaySfxSample16();
        goto after_z;

    clear_deltaz:
        dz = 0;

    after_z:
        ball->deltaZ.raw = (int32_t)dz;
    }

    /* ---- Out-of-play boundary checks when game not in progress ---- */
    if (gameStatePl != ST_GAME_IN_PROGRESS) {
        /* X boundary */
        int16_t bx = ball->x.s.i;
        if (bx < 53 || bx > 618) {
            int16_t destX = ball->destX;
            int16_t delta = destX - bx;
            ball->destX = bx - delta;
            uint16_t spd = (uint16_t)ball->speed;
            ball->speed = (int16_t)(spd >> 1);
            ball->x.raw = savedX;
            ball->y.raw = savedY;
            ball->z.raw = savedZ;
        }

        /* Y boundary */
        int16_t by = ball->y.s.i;
        if (by < 100 || by > 799) {
            int16_t destY = ball->destY;
            int16_t delta = destY - by;
            ball->destY = by - delta;
            uint16_t spd = (uint16_t)ball->speed;
            ball->speed = (int16_t)(spd >> 1);
            ball->x.raw = savedX;
            ball->y.raw = savedY;
            ball->z.raw = savedZ;
        }
    }

    /* ---- Goal area collision detection ---- */
    {
        int16_t bx = ball->x.s.i - 1;
        int16_t by = ball->y.s.i;
        int16_t bz = ball->z.s.i;

        if (by >= 129 && by <= 769)
            goto skip_goal_collision;

        /* Upper goal area (Y <= 128) */
        if (by <= 128) {
            if (by <= 112)
                goto check_upper_goal_inner;
            /* Y in 113..128 */
            if (bz > 19) goto skip_goal_collision;
            if (bx <= 295 || bx > 372) goto skip_goal_collision;
            if (by > 123) {
                if (bz > 15) goto upper_crossbar;
            } else {
                /* Y in 113..123 */
                if (bz > 10) goto upper_crossbar;
            }
            /* Ball inside upper goal */
            if (bx < 302 || bx > 366) goto side_post_bounce;
            if (by < 119) goto back_net_bounce;
            goto skip_goal_collision;

        check_upper_goal_inner:
            goto skip_goal_collision; /* Y < 112, outside goal */
        }

        /* Lower goal area (Y >= 770) */
        if (by < 770) goto skip_goal_collision;
        if (by >= 785) goto skip_goal_collision;
        /* Y in 770..784 */
        if (bz > 19) goto skip_goal_collision;
        if (bx <= 295 || bx > 372) goto skip_goal_collision;
        if (bz > 15) goto lower_crossbar;
        if (bx < 302 || bx > 366) goto side_post_bounce;
        if (by > 778) goto back_net_bounce;
        goto skip_goal_collision;

    upper_crossbar:
        {
            /* Check if ball is going up (deltaZ fractional part) */
            int32_t dzFull = ball->z.raw; /* full 32-bit Z */
            int16_t dzHigh = (int16_t)(dzFull >> 16);
            if (dzHigh <= 15) {
                /* Slow stop on crossbar */
                ball->deltaZ.raw = (int32_t)1;
                ball->speed = (int16_t)0;
                ball->x.raw = savedX;
                ball->y.raw = savedY;
                ball->z.raw = savedZ;
                goto skip_goal_collision;
            }
            /* Fast deflection off crossbar */
            ball->deltaZ.raw = (int32_t)1;
            {
                int16_t newDestY = ball->y.s.i - 1000;
                ball->destY = newDestY;
            }
            ball->speed = (int16_t)512;
            ball->z.raw = savedZ;
            goto skip_goal_collision;
        }

    lower_crossbar:
        {
            int32_t dzFull = ball->z.raw;
            int16_t dzHigh = (int16_t)(dzFull >> 16);
            if (dzHigh <= 15) {
                ball->deltaZ.raw = (int32_t)1;
                ball->speed = (int16_t)0;
                ball->x.raw = savedX;
                ball->y.raw = savedY;
                ball->z.raw = savedZ;
                goto skip_goal_collision;
            }
            ball->deltaZ.raw = (int32_t)1;
            {
                int16_t newDestY = ball->y.s.i + 1000;
                ball->destY = newDestY;
            }
            ball->speed = (int16_t)512;
            ball->z.raw = savedZ;
            goto skip_goal_collision;
        }

    back_net_bounce:
        reverseDestYDirection(ball, savedY);
        _ResetBothTeamSpinTimers();
        {
            uint16_t spd = (uint16_t)ball->speed;
            ball->speed = (int16_t)(spd >> 3);
        }
        ball->x.raw = savedX;
        ball->y.raw = savedY;
        goto skip_goal_collision;

    side_post_bounce:
        reverseDestXDirection(ball, savedX);
        _ResetBothTeamSpinTimers();
        {
            uint16_t spd = (uint16_t)ball->speed;
            ball->speed = (int16_t)(spd >> 2);
        }
        ball->x.raw = savedX;
        ball->y.raw = savedY;
    }

skip_goal_collision:
    /* ---- Goalpost deflection (game in progress) ---- */
    if (gameStatePl == ST_GAME_IN_PROGRESS) {
        int16_t bx = ball->x.s.i - 1;
        int16_t by = ball->y.s.i;
        int16_t bz = ball->z.s.i;

        /* Check if ball is near goal line (Y 129..132 or 766..769) */
        if (by > 128 && by <= 132)
            goto goalline_check;
        if (by >= 766 && by < 770)
            goto goalline_check;
        goto after_goalpost;

    goalline_check:
        if (bz > 19) goto after_goalpost;
        if (bx <= 295 || bx > 372) goto after_goalpost;
        if (bz <= 15) {
            /* Ball at post level */
            if (bx >= 302 && bx <= 366)
                goto after_goalpost; /* inside goal mouth, no post */
            /* Side post hit */
            goto goalpost_hit;
        }

        /* Ball above post level but below crossbar — random deflection */
        {
            int16_t d4 = (int16_t)(currentGameTick & 0x1F);
            d4 = (int16_t)((d4 << 4) - 0x100);

            int32_t deltaY = ball->deltaY.raw;
            if (deltaY < (int32_t)0xFFFFB000 || deltaY > 0x5000) {
                /* Strong Y velocity: check direction */
                if (by <= 449) {
                    if (deltaY >= 0)
                        goto after_goalpost;
                } else {
                    if (deltaY < 0)
                        goto after_goalpost;
                }
                /* Deflect Y and add random X */
                reverseDestYDirection(ball, savedY);
                _ResetBothTeamSpinTimers();
                ball->destX = ball->destX + d4;
                goto goalpost_speed_reduce;
            }

            /* Weak Y velocity at crossbar height -> crossbar bounce.
             * ASM: neg.l SPRITE_DELTAZ, restore Z from savedZ, bump savedY +1.
             * This bounces the ball off the crossbar. */
            ball->deltaZ.raw = -ball->deltaZ.raw;
            ball->z.raw = savedZ;
            savedY += 0x10000;
            goto goalpost_speed_reduce;
        }

    goalpost_hit:
        {
            int16_t d4 = (int16_t)(currentGameTick & 0x1F);
            d4 = (int16_t)((d4 << 4) - 0x100);

            int32_t deltaY = ball->deltaY.raw;
            if (deltaY < (int32_t)0xFFFFB000 || deltaY > 0x5000) {
                /* Strong velocity */
                if (by <= 449) {
                    if (ball->deltaY.raw >= 0)
                        goto after_goalpost;
                } else {
                    if (ball->deltaY.raw < 0)
                        goto after_goalpost;
                }
                reverseDestYDirection(ball, savedY);
                _ResetBothTeamSpinTimers();
                ball->destX = ball->destX + d4;
                goto goalpost_speed_reduce;
            }

            /* Crossbar bounce */
            {
                int32_t curDZ = ball->deltaZ.raw;
                ball->deltaZ.raw = -curDZ;
            }
            ball->z.raw = savedZ;
            savedY += 0x10000; /* bump Y by 1 integer unit */
        }

    goalpost_speed_reduce:
        _PlaySample3IfNotTraining();
        {
            uint16_t spd = (uint16_t)ball->speed;
            ball->speed = (int16_t)(spd - (spd >> 2));
        }
        ball->x.raw = savedX;
        ball->y.raw = savedY;
    }

after_goalpost:
    /* ---- Check if ball out of play (game in progress only) ---- */
    if (gameStatePl == ST_GAME_IN_PROGRESS) {
        int16_t bx = ball->x.s.i;
        int16_t by = ball->y.s.i;
        if (bx < 81 || bx > 590 || by < 129 || by > 769) {
            _CheckIfBallOutOfPlay();
        }
    }

    /* ---- Update ball shadow position ---- */
    {
        Sprite *shadow = &ballShadowSprite;
        uint16_t halfZ = (uint16_t)ball->z.s.i >> 1;
        int16_t sx = ball->x.s.i + (int16_t)halfZ + 1;
        shadow->x.s.i = sx;
        uint16_t quarterZ = halfZ >> 1;
        int16_t sy = ball->y.s.i + (int16_t)quarterZ + 1 - 10;
        shadow->y.s.i = sy;
        shadow->z.s.i = (int16_t)-10;
    }

    /* ---- Calculate next ball position (for AI) ---- */
    _CalculateNextBallPosition();

    /* ---- Calculate ball quadrant for tactics ---- */
    {
        int16_t qx, qy;

        if (gameStatePl == ST_GAME_IN_PROGRESS ||
            gameState == ST_FIRST_HALF_ENDED ||
            gameState == ST_GAME_ENDED) {
            qx = ballNextX;
            qy = ballNextY;
        } else {
            qx = foulXCoordinate;
            qy = foulYCoordinate;
            if (gameState == ST_KEEPER_HOLDS_BALL ||
                gameState == ST_GOAL_OUT_LEFT ||
                gameState == ST_GOAL_OUT_RIGHT) {
                qx = 336;
                qy = 449;
            }
        }

        /* X quadrant: 4 thresholds -> quadrant 0..4
           asm pattern: cmp.w (a1)+,d1; bcs branch; addq d0/d3
           After loop: sub.w -4(a1),d1 subtracts limits[i-1] (the entry before
           the unmatched threshold). For i=0 it reads pad data (=81). */
        int16_t xQuad = 0;
        int16_t d3 = 0;
        int16_t xRefLimit;
        {
            int16_t i;
            for (i = 0; i < 4; i++) {
                if ((uint16_t)qx < (uint16_t)ballXQuadrantLimits[i])
                    break;
                xQuad++;
                d3++;
            }
            if (i == 0)
                xRefLimit = 81; /* pad_111202 data: dc.b 0,81 -> word 0x0051 */
            else if (i == 4)
                xRefLimit = ballXQuadrantLimits[3]; /* lea 2(a1),a1 then -4(a1) */
            else
                xRefLimit = ballXQuadrantLimits[i - 1];
        }
        ballXQuadrantDead = xQuad;
        playerXQuadrantOffset = (int16_t)(((int16_t)(qx - xRefLimit - 51) * 5) / 15);

        /* Y quadrant: 6 thresholds -> quadrant 0..6, d3 increments by 5 each */
        int16_t yQuad = 0;
        int16_t yRefLimit;
        {
            int16_t i;
            for (i = 0; i < 6; i++) {
                if ((uint16_t)qy < (uint16_t)ballYQuadrantLimits[i])
                    break;
                yQuad++;
                d3 += 5;
            }
            if (i == 0)
                yRefLimit = ballYQuadrantLimits[0]; /* fallback */
            else if (i == 6)
                yRefLimit = ballYQuadrantLimits[5]; /* lea 2(a1),a1 then -4(a1) */
            else
                yRefLimit = ballYQuadrantLimits[i - 1];
        }
        ballYQuadrantDead = yQuad;
        playerYQuadrantOffset = (int16_t)(((int16_t)(qy - yRefLimit - 45) * 5) / 15);
        ballQuadrantIndex = d3;
    }
}


/* ==================================================================
 * _UpdateBallVariables
 *   from: UpdateBallVariables
 *   in:  keeper (A1), ball (A2), team (A6)
 *
 * Integrates ball trajectory forward to predict where it crosses the
 * goalkeeper's Y position, then samples at fixed Y windows:
 *   Top team:    Y=keeper, Y-135, Y-129
 *   Bottom team: Y=keeper, Y+763, Y+769
 * Results stored in ballDefensiveX/Y/Z, ballNotHighX/Y/Z, strikeDestX/Y/Z.
 * For horizontal movement, predicts at keeper's X position.
 * ================================================================== */
void __attribute__((used, externally_visible))
_UpdateBallVariables(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team)
{
    int32_t d1 = ball->x.raw;
    int32_t d2 = ball->y.raw;
    int32_t d3 = ball->z.raw;
    int32_t d4 = ball->deltaX.raw;
    int32_t d5 = ball->deltaY.raw;
    int32_t d6 = ball->deltaZ.raw;
    int32_t d7 = kGravityConstant;

    /* Check ball Y velocity direction */
    if (d5 < (int32_t)0xFFFF0000)
        goto ball_moving_toward_top_goal;
    if (d5 > (int32_t)0x10000)
        goto ball_moving_toward_bottom_goal;
    /* Y velocity small, check X */
    if (d4 < (int32_t)0xFFFF0000)
        goto ball_going_left;
    if (d4 > (int32_t)0x10000)
        goto ball_going_right;
    goto sample_defense_at_current_ball_pos;

ball_moving_toward_top_goal:
    if (team != &topTeamData)
        goto sample_defense_at_current_ball_pos;
    d2 -= keeper->y.raw;
    if (d2 >= 0) {
        /* Ball is below keeper, integrate until crossing */
        do {
            d6 -= d7;
            d3 += d6;
            d1 += d4;
            d2 += d5;
        } while (d2 >= 0);
        d1 -= d4;
        d2 -= d5;
        d3 -= d6;
        d2 += keeper->y.raw;
        {
            int16_t zi = (int16_t)((uint32_t)d3 >> 16);
            ballDefensiveX = (int16_t)((uint32_t)d1 >> 16);
            ballDefensiveY = (int16_t)((uint32_t)d2 >> 16);
            if (zi < 0) zi = 0;
            ballDefensiveZ = zi;
        }
    } else {
        /* Ball already past keeper Y */
        ballDefensiveX = ball->x.s.i;
        ballDefensiveY = ball->y.s.i;
        ballDefensiveZ = ball->z.s.i;
        d2 += keeper->y.raw;
    }

    /* check_above_135px_window */
    d2 -= (int32_t)0x870000; /* subtract 135.0 */
    if (d2 >= 0) {
        /* integrate_to_135px_ahead */
        do {
            d6 -= d7;
            d3 += d6;
            d1 += d4;
            d2 += d5;
        } while (d2 >= 0);
        d1 -= d4;
        d3 -= d6;
        d2 = (int32_t)0x870000; /* set to 135.0 */
        {
            int16_t zi = (int16_t)((uint32_t)d3 >> 16);
            ballNotHighX = (int16_t)((uint32_t)d1 >> 16);
            ballNotHighY = (int16_t)((uint32_t)d2 >> 16);
            if (zi < 0) zi = 0;
            ballNotHighZ = zi;
        }
    } else {
        /* Already within 135px window */
        ballNotHighX = ballDefensiveX;
        ballNotHighY = ballDefensiveY;
        ballNotHighZ = ballDefensiveZ;
        d2 += (int32_t)0x870000;
    }

    /* keeper_attempt_static_header_catch: check 129px window */
    d2 -= (int32_t)0x810000; /* subtract 129.0 */
    if (d2 >= 0) {
        /* integrate_to_129px_ahead */
        do {
            d6 -= d7;
            d3 += d6;
            d1 += d4;
            d2 += d5;
        } while (d2 >= 0);
        d1 -= d4;
        d3 -= d6;
        d2 = (int32_t)0x810000;
        {
            int16_t zi = (int16_t)((uint32_t)d3 >> 16);
            strikeDestX = (int16_t)((uint32_t)d1 >> 16);
            strikeDestY = (int16_t)((uint32_t)d2 >> 16);
            if (zi < 0) zi = 0;
            strikeDestZ = zi;
        }
    } else {
        strikeDestX = 0;
        strikeDestY = 0;
        strikeDestZ = 0;
    }
    return;

ball_moving_toward_bottom_goal:
    if (team != &bottomTeamData)
        goto sample_defense_at_current_ball_pos;
    d2 -= keeper->y.raw;
    if (d2 < 0) {
        /* Ball is above keeper, integrate until crossing */
        do {
            d6 -= d7;
            d3 += d6;
            d1 += d4;
            d2 += d5;
        } while (d2 < 0);
        d1 -= d4;
        d2 -= d5;
        d3 -= d6;
        d2 += keeper->y.raw;
        {
            int16_t zi = (int16_t)((uint32_t)d3 >> 16);
            ballDefensiveX = (int16_t)((uint32_t)d1 >> 16);
            ballDefensiveY = (int16_t)((uint32_t)d2 >> 16);
            if (zi < 0) zi = 0;
            ballDefensiveZ = zi;
        }
    } else {
        /* Ball already past keeper Y */
        ballDefensiveX = ball->x.s.i;
        ballDefensiveY = ball->y.s.i;
        ballDefensiveZ = ball->z.s.i;
        d2 += keeper->y.raw;
    }

    /* check_plus_498px_threshold: 763.0 */
    d2 -= (int32_t)0x2FB0000; /* subtract 763.0 */
    if (d2 < 0) {
        /* integrate_to_plus_498px */
        do {
            d6 -= d7;
            d3 += d6;
            d1 += d4;
            d2 += d5;
        } while (d2 < 0);
        d1 -= d4;
        d3 -= d6;
        d2 = (int32_t)0x2FB0000; /* set to 763.0 */
        {
            int16_t zi = (int16_t)((uint32_t)d3 >> 16);
            ballNotHighX = (int16_t)((uint32_t)d1 >> 16);
            ballNotHighY = (int16_t)((uint32_t)d2 >> 16);
            if (zi < 0) zi = 0;
            ballNotHighZ = zi;
        }
    } else {
        /* Already past 763px window */
        ballNotHighX = ballDefensiveX;
        ballNotHighY = ballDefensiveY;
        ballNotHighZ = ballDefensiveZ;
        d2 += (int32_t)0x2FB0000;
    }

    /* check_between_plus_497_plus_505px_window: 769.0 */
    d2 -= (int32_t)0x3010000; /* subtract 769.0 */
    if (d2 < 0) {
        /* integrate_to_plus_505px */
        do {
            d6 -= d7;
            d3 += d6;
            d1 += d4;
            d2 += d5;
        } while (d2 < 0);
        d1 -= d4;
        d3 -= d6;
        d2 = (int32_t)0x3010000;
        {
            int16_t zi = (int16_t)((uint32_t)d3 >> 16);
            strikeDestX = (int16_t)((uint32_t)d1 >> 16);
            strikeDestY = (int16_t)((uint32_t)d2 >> 16);
            if (zi < 0) zi = 0;
            strikeDestZ = zi;
        }
    } else {
        strikeDestX = 0;
        strikeDestY = 0;
        strikeDestZ = 0;
    }
    return;

ball_going_left:
    d1 -= keeper->x.raw;
    if (d1 < 0) {
        /* Ball already left of keeper, no prediction */
        d1 += keeper->x.raw;
        goto sample_defense_at_current_ball_pos;
    }
    /* Ball right of keeper, integrate left past keeper X */
    do {
        d6 -= d7;
        d3 += d6;
        d2 += d5;
        d1 += d4;
    } while (d1 >= 0);
    d1 -= d4;
    d2 -= d5;
    d3 -= d6;
    d1 += keeper->x.raw;
    {
        int16_t zi = (int16_t)((uint32_t)d3 >> 16);
        ballDefensiveX = (int16_t)((uint32_t)d1 >> 16);
        ballDefensiveY = (int16_t)((uint32_t)d2 >> 16);
        if (zi < 0) zi = 0;
        ballDefensiveZ = zi;
    }
    goto finalize_not_high_and_clear_strike;

ball_going_right:
    d1 -= keeper->x.raw;
    if (d1 >= 0) {
        /* Ball already right of keeper, no prediction */
        d1 += keeper->x.raw;
        goto sample_defense_at_current_ball_pos;
    }
    /* Ball left of keeper, integrate right past keeper X */
    do {
        d6 -= d7;
        d3 += d6;
        d2 += d5;
        d1 += d4;
    } while (d1 < 0);
    d1 -= d4;
    d2 -= d5;
    d3 -= d6;
    d1 += keeper->x.raw;
    {
        int16_t zi = (int16_t)((uint32_t)d3 >> 16);
        ballDefensiveX = (int16_t)((uint32_t)d1 >> 16);
        ballDefensiveY = (int16_t)((uint32_t)d2 >> 16);
        if (zi < 0) zi = 0;
        ballDefensiveZ = zi;
    }
    goto finalize_not_high_and_clear_strike;

sample_defense_at_current_ball_pos:
    ballDefensiveX = ball->x.s.i;
    ballDefensiveY = ball->y.s.i;
    ballDefensiveZ = ball->z.s.i;
finalize_not_high_and_clear_strike:
    ballNotHighX = ballDefensiveX;
    ballNotHighY = ballDefensiveY;
    ballNotHighZ = ballDefensiveZ;
    strikeDestX = 0;
    strikeDestY = 0;
    strikeDestZ = 0;
}


/* ==================================================================
 * _CalculateBallNextGroundXYPositions
 *   from: CalculateBallNextGroundXYPositions
 *   in:  ball (A2)
 *
 * Predicts where the ball will land by integrating with decreasing Z
 * subtraction steps (20→13). Stores result in ballNextGroundX/Y.
 * Sets ballNextGroundX = -1 if ball is stationary.
 * ================================================================== */
void __attribute__((used, externally_visible))
_CalculateBallNextGroundXYPositions(Sprite *ball)
{
    int32_t d1 = ball->x.raw;
    int32_t d2 = ball->y.raw;
    int32_t d3 = ball->z.raw;
    int32_t d4 = ball->deltaX.raw;
    int32_t d5 = ball->deltaY.raw;

    /* Check if ball has meaningful velocity */
    if (d5 < -(int32_t)0x10000 || d5 > (int32_t)0x10000 ||
        d4 < -(int32_t)0x10000 || d4 > (int32_t)0x10000) {
        /* ball_moving */
        int32_t d6 = ball->deltaZ.raw;
        int32_t d7 = kGravityConstant;
        int32_t d0 = (int32_t)0x140000; /* 20.0 */

        /* Try decreasing Z subtraction values */
        for (;;) {
            d3 -= d0;
            if (d3 >= 0)
                break; /* z_in_range */
            if (d0 == (int32_t)0xD0000) { /* 13.0 */
                ballNextGroundX = -1;
                return;
            }
            d3 += d0;
            d0 -= (int32_t)0x10000;
        }

        /* z_in_range: integrate until z goes negative */
        do {
            d1 += d4;
            d2 += d5;
            d6 -= d7;
            d3 += d6;
        } while (d3 >= 0);

        d3 += d0;
        ballNextGroundX = (int16_t)((uint32_t)d1 >> 16);
        ballNextYGroundY = (int16_t)((uint32_t)d2 >> 16);
        ballNextZDeadVar = 0;
        return;
    }

    /* ball_standing */
    ballNextGroundX = -1;
}


/* ==================================================================
 * _ApplyBallAfterTouch
 *   from: ApplyBallAfterTouch
 *   in:  team (A6)
 *
 * Applies spin/aftertouch to the ball after a kick or pass.
 * Two main paths: kick (no passInProgress) and pass (passInProgress).
 * Spin detection: compares current direction vs controlled direction.
 * High kick detection at spinTimer == 4.
 * Timer increments to 10 then resets to 0xFFFF (disabled).
 * ================================================================== */
void __attribute__((used, externally_visible))
_ApplyBallAfterTouch(TeamGeneralInfo *team)
{
    Sprite *ball;
    int16_t spinSide;

    if (team->passInProgress != 0)
        goto passing_now;

    /* ---- Kick path ---- */
    {
        TeamGeneralInfo *oppTeam = team->opponentsTeam;
        if (oppTeam->goalkeeperSavedCommentTimer < 0)
            goto kick_reset_spin;
        if (gameStatePl != ST_GAME_IN_PROGRESS) {
            if (gameState == ST_KEEPER_HOLDS_BALL)
                goto kick_reset_spin;
        }
    }

    {
        int16_t spinTimer = team->spinTimer;
        if (spinTimer < 0)
            return;
        if (spinTimer == 0) {
            team->leftSpin = 0;
            team->rightSpin = 0;
        }
    }

    ball = &ballSprite;

    if (team->leftSpin != 0) {
        spinSide = 0;
        goto kick_calc_spin;
    }
    if (team->rightSpin != 0) {
        spinSide = 4;
        goto kick_calc_spin;
    }

    /* Detect spin start */
    {
        int16_t curDir = team->currentAllowedDirection;
        if (curDir < 0)
            goto kick_not_starting_spin;
        int16_t diff = team->controlledPlDirection - curDir;
        if (diff == 0)
            goto kick_not_starting_spin;
        diff &= 7;
        if (diff == 4)
            goto kick_not_starting_spin;
        if ((uint16_t)diff < 4) {
            team->leftSpin = 1;
            spinSide = 0;
        } else {
            team->rightSpin = 1;
            spinSide = 4;
        }
    }

kick_calc_spin:
    {
        int16_t dir = team->controlledPlDirection;
        int16_t tableIdx = dir * 4 + (spinSide >> 1);
        int16_t multiplier = kSpinMultiplierFactor[team->spinTimer];
        int16_t spinDx = kKickSpinFactor[tableIdx];
        ball->destX += (int16_t)(spinDx * multiplier);
        int16_t spinDy = kKickSpinFactor[tableIdx + 1];
        ball->destY += (int16_t)(spinDy * multiplier);
    }

kick_not_starting_spin:
    /* Check for high kick at spinTimer == 4 */
    if (team->spinTimer != 4)
        goto kick_increase_timer;

    {
        int16_t curDir = team->currentAllowedDirection;
        if (curDir < 0)
            goto kick_normal_not_high;
        int16_t diff = team->controlledPlDirection - curDir;
        if (diff == 0)
            goto kick_increase_timer;
        diff &= 7;
        if (diff == 2 || diff == 6)
            goto kick_normal_not_high;
        if ((uint16_t)diff < 3 || (uint16_t)diff > 5)
            goto kick_increase_timer;
        /* High kick */
        ball->deltaZ.raw = kHighKickDeltaZ;
        ball->speed = kHighKickBallSpeed;
        goto kick_speed_adjust;
    }

kick_normal_not_high:
    ball->deltaZ.raw = kNormalKickDeltaZ;
    ball->speed = kNormalKickBallSpeed;

kick_speed_adjust:
    {
        int16_t ctrlDir = team->controlledPlDirection;
        if (ctrlDir == 0 || ctrlDir == 4) {
            /* Decrease by quarter: speed * 3/4 */
            int16_t spd = ball->speed;
            spd = spd - ((uint16_t)spd >> 2);
            ball->speed = spd;
        } else if (ctrlDir & 1) {
            /* Odd direction: speed * 7/8 */
            int16_t spd = ball->speed;
            int16_t quarter = (uint16_t)spd >> 2;
            int16_t result = spd - quarter;
            int16_t eighth = (uint16_t)spd >> 3;
            result += eighth;
            ball->speed = result;
        }
    }

kick_increase_timer:
    team->spinTimer++;
    if (team->spinTimer == 10)
        goto kick_reset_spin;
    return;

kick_reset_spin:
    team->spinTimer = (int16_t)0xFFFF;
    return;

    /* ---- Pass path ---- */
passing_now:
    {
        TeamGeneralInfo *oppTeam = team->opponentsTeam;
        if (oppTeam->goalkeeperSavedCommentTimer < 0)
            goto pass_reset_spin;
        if (gameStatePl != ST_GAME_IN_PROGRESS) {
            if (gameState == ST_KEEPER_HOLDS_BALL)
                goto pass_reset_spin;
        }
    }

    {
        int16_t spinTimer = team->spinTimer;
        if (spinTimer < 0)
            return;
        if (spinTimer == 0) {
            team->leftSpin = 0;
            team->rightSpin = 0;
            team->longPass = 0;
            team->longSpinPass = 0;
        }
    }

    ball = &ballSprite;

    if (team->leftSpin != 0) {
        spinSide = 0;
        goto pass_calc_spin;
    }
    if (team->rightSpin != 0) {
        spinSide = 4;
        goto pass_calc_spin;
    }

    /* Detect spin start for pass */
    {
        int16_t curDir = team->currentAllowedDirection;
        if (curDir < 0)
            goto pass_no_spin;
        int16_t diff = team->controlledPlDirection - curDir;
        if (diff == 0)
            goto pass_no_spin;
        diff &= 7;
        if (diff == 4)
            goto pass_no_spin;
        if ((uint16_t)diff < 4) {
            team->leftSpin = 1;
            spinSide = 0;
        } else {
            team->rightSpin = 1;
            spinSide = 4;
        }
    }

pass_calc_spin:
    {
        int16_t ballDir = ballSprite.direction;
        int16_t tableIdx = ballDir * 4 + (spinSide >> 1);
        int16_t multiplier = kSpinMultiplierFactor[team->spinTimer];
        int16_t spinDx = kPassingSpinFactor[tableIdx];
        ball->destX += (int16_t)(spinDx * multiplier);
        int16_t spinDy = kPassingSpinFactor[tableIdx + 1];
        ball->destY += (int16_t)(spinDy * multiplier);
    }

pass_no_spin:
    /* Check for long pass */
    if (team->longPass != 0 || team->longSpinPass != 0)
        goto pass_update_timer;

    {
        int16_t curDir = team->currentAllowedDirection;
        if (curDir < 0)
            goto pass_holding_left_or_right;
        int16_t diff = team->controlledPlDirection - curDir;
        if (diff == 0)
            goto pass_update_timer;
        diff &= 7;
        if (diff == 2 || diff == 6)
            goto pass_holding_left_or_right;
        if ((uint16_t)diff < 3 || (uint16_t)diff > 5)
            goto pass_update_timer;

        /* Long spin pass */
        team->longSpinPass = 1;
        {
            int16_t spd = ball->speed;
            spd += (uint16_t)spd >> 3;
            ball->speed = spd;
        }
        goto pass_update_timer;
    }

pass_holding_left_or_right:
    team->longPass = 1;
    {
        int16_t spd = ball->speed;
        spd += (uint16_t)spd >> 3;
        ball->speed = spd;
    }

pass_update_timer:
    team->spinTimer++;
    if (team->spinTimer == 10)
        goto pass_reset_spin;
    return;

pass_reset_spin:
    team->spinTimer = (int16_t)0xFFFF;
}
