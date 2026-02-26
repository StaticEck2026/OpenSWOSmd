/* ai.c — AI helper functions
 *
 * Chunk 52: AI_SetDirectionTowardOpponentsGoal, AI_Kick,
 *           AI_DecideWhetherToTriggerFire, FindClosestPlayerToBallFacing.
 * Chunk 54: AI_SetControlsDirection.
 *
 * AI_ResumeGameDelay is left in asm (3 instructions, carry flag return).
 */

#include "ai.h"
#include "game_types.h"
#include "math68k.h"

extern Sprite          ballSprite;
extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;
extern uint16_t        currentGameTick;
extern int16_t         aiCounter;
extern int16_t         aiAttackHalf;
extern int16_t         aiCounterWriteOnly;
extern int16_t         deadVarAlways0;
extern TeamGeneralInfo *playerKickerPtr;

/* AI_SetControlsDirection globals — declared in SWOS2.s data section */
extern int16_t  aiTurnDirection;
extern int16_t  aiResumePlayTimer;
extern int16_t  aiRand;
extern int16_t  aiRandomRotateTable[2];
extern int16_t  aiLeftSpinTable[3];
extern int16_t  aiRotateRightTable[3];
extern int16_t  aiLongKickTable[3];
extern uint8_t  aiThrowInDirections[6];
extern int16_t  AI_maxStoppageTime;
extern int16_t  gameState;
extern int16_t  gameStatePl;
extern int16_t  resultTimer;
extern int16_t  stoppageTimerActive;
extern int16_t  stoppageTimerTotal;
extern int16_t  team1Computer;
extern int16_t  team2Computer;
extern int16_t  playingPenalties;
extern int16_t  penalty;
extern int16_t  cameraDirection;
extern uint8_t  playerTurnFlags;
extern int16_t  frameCount;
extern int16_t  inSubstitutesMenu;
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;
extern TeamGeneralInfo *lastTeamPlayed;

extern uint16_t _Rand(void);

/* Written by _FindClosestPlayerToBallFacing, read by asm stub to return in d2 */
int32_t __attribute__((used, externally_visible)) findClosestBallDist;


/* ===== AI_SetDirectionTowardOpponentsGoal =====
 *
 * Sets the AI team's allowed direction toward the opposing goal based on
 * ball X position. Only acts when aiCounter is non-zero.
 *
 * Original: SWOS2.s lines 19776-19809
 */
void __attribute__((used, externally_visible))
_AI_SetDirectionTowardOpponentsGoal(TeamGeneralInfo *team)
{
    int16_t ballX, dir;

    if (aiCounter == 0)
        return;

    ballX = ballSprite.x.s.i;

    if (aiAttackHalf == 1) {
        /* Bottom team attacks upward: directions 7, 0, 1 */
        if (team != &bottomTeamData)
            return;
        if (ballX < 300)
            dir = 1;
        else if (ballX > 371)
            dir = 7;
        else
            dir = 0;
    } else {
        /* Top team attacks downward: directions 3, 4, 5 */
        if (team != &topTeamData)
            return;
        if (ballX < 300)
            dir = 3;
        else if (ballX > 371)
            dir = 5;
        else
            dir = 4;
    }

    team->currentAllowedDirection = dir;
}


/* ===== AI_Kick =====
 *
 * AI kick decision. Checks if the opponent's controlled player is facing
 * a significantly different direction from our player. If so, triggers
 * a kick (fire + direction).
 *
 * Original: SWOS2.s lines 19939-19974
 */
void __attribute__((used, externally_visible))
_AI_Kick(Sprite *player, TeamGeneralInfo *team)
{
    TeamGeneralInfo *opponent;
    Sprite *opponentPlayer;
    int16_t playerDir;
    uint8_t opponentDirScaled, playerDirScaled, diff;
    int8_t sdiff;

    /* deadVarAlways0 is always 0, so this path is never taken */
    if (deadVarAlways0 != 0) {
        if (team == (TeamGeneralInfo *)playerKickerPtr)
            return;
        opponent = team->opponentsTeam;
        if (opponent == (TeamGeneralInfo *)playerKickerPtr)
            return;
    }

    /* Ball must be close to player (distance <= 200) */
    if (player->ballDistance > 200)
        return;

    /* Save player's 8-way direction for later */
    playerDir = player->direction;

    /* Opponent must have the ball */
    opponent = team->opponentsTeam;
    if (opponent->playerHasBall == 0)
        return;

    /* Opponent must have a controlled player */
    opponentPlayer = opponent->controlledPlayer;
    if (opponentPlayer == (Sprite *)0)
        return;

    /* Compare directions: scale 0-7 to 0-224 range and do byte subtraction */
    opponentDirScaled = (uint8_t)(opponentPlayer->direction * 32);
    playerDirScaled = (uint8_t)(player->direction * 32);
    diff = opponentDirScaled - playerDirScaled;
    sdiff = (int8_t)diff;

    /* Kick when angle difference is outside +-32 (more than 1 direction step apart) */
    if (sdiff >= -32 && sdiff <= 32)
        return;

    /* Trigger kick */
    team->currentAllowedDirection = playerDir;
    team->firePressed = 1;
    team->fireThisFrame = 1;
    team->normalFire = 1;
}


/* ===== AI_DecideWhetherToTriggerFire =====
 *
 * Checks if the AI player should shoot. Tests facing direction vs goal,
 * proximity to ball, and ball height window.
 *
 * Returns 0 if firing (zero flag set), 1 if not firing (zero flag clear).
 *
 * Original: SWOS2.s lines 19819-19879
 */
int16_t __attribute__((used, externally_visible))
_AI_DecideWhetherToTriggerFire(Sprite *player, TeamGeneralInfo *team, int16_t dir)
{
    uint16_t reverseDir, roundedDir;

    /* Goalkeeper never shoots */
    if (player->playerOrdinal == 1)
        return 1;

    /* Check if player is facing toward opponent's goal */
    if (team == &topTeamData) {
        /* Top team: must face downward (directions 3, 4, 5) */
        if (dir != 3 && dir != 4 && dir != 5)
            return 1;
    } else {
        /* Bottom team: must face upward (directions 7, 0, 1) */
        if (dir != 7 && dir != 0 && dir != 1)
            return 1;
    }

    /* Player must be close to ball (distance <= 648 = 18^2 + 18^2) */
    if (player->ballDistance > 648)
        return 1;

    /* Check ball height based on vertical movement */
    if (ballSprite.deltaZ.raw < 0) {
        /* Ball descending: wider height window [12..20] */
        if (ballSprite.z.s.i < 12 || ballSprite.z.s.i > 20)
            return 1;
    } else {
        /* Ball going up or on ground: height window [8..14] */
        if (ballSprite.z.s.i < 8 || ballSprite.z.s.i > 14)
            return 1;
    }

    /* Arm the shot */
    team->fireThisFrame = 1;

    /* Calculate reverse direction: add 128 to fullDirection (flip 180 degrees) */
    reverseDir = ((uint16_t)(player->fullDirection + 128)) & 0xFF;
    reverseDir >>= 5; /* convert to 0..7 range */
    team->currentAllowedDirection = (int16_t)reverseDir;

    /* Second pass: round up by adding 16 (half a step) before converting */
    roundedDir = ((uint16_t)(player->fullDirection + 128 + 16)) & 0xFF;
    roundedDir >>= 5;

    /* Must match the current 8-way direction */
    if ((int16_t)roundedDir != dir)
        return 1;

    team->currentAllowedDirection = (int16_t)roundedDir;
    aiCounter = 15;
    aiCounterWriteOnly = (int16_t)roundedDir;
    aiAttackHalf = 2;

    if (team != &topTeamData)
        aiAttackHalf = 1;

    return 0; /* firing */
}


/* ===== FindClosestPlayerToBallFacing =====
 *
 * Finds the closest player (from both teams) who is:
 * - not the controlled player
 * - not sent away
 * - in normal state
 * - facing within +-16 of the target direction
 * - closest to the ball
 *
 * Returns the player pointer (or (Sprite*)-1 if none found).
 * Ball distance is stored in findClosestBallDist for the asm stub.
 *
 * Original: SWOS2.s lines 19986-20021
 */
Sprite * __attribute__((used, externally_visible))
_FindClosestPlayerToBallFacing(uint16_t fullDir, TeamGeneralInfo *team)
{
    Sprite *bestPlayer = (Sprite *)0xFFFFFFFF;
    int32_t bestDist = -1; /* 0xFFFFFFFF as unsigned = max value */
    Sprite **sprites;
    int16_t i, t;
    TeamGeneralInfo *scanTeam;

    scanTeam = &topTeamData;

    for (t = 0; t < 2; t++) {
        sprites = scanTeam->spritesTable;

        for (i = 0; i < 11; i++) {
            Sprite *pl = sprites[i];
            int8_t dirDiff;

            /* Skip controlled player */
            if (pl == team->controlledPlayer)
                goto next;

            /* Skip sent-away players */
            if (pl->sentAway != 0)
                goto next;

            /* Skip non-normal players */
            if (pl->playerState != PL_NORMAL)
                goto next;

            /* Check direction: must be within +-16 of target */
            dirDiff = (int8_t)((uint8_t)(pl->fullDirection) - (uint8_t)fullDir);
            if (dirDiff < -16 || dirDiff > 16)
                goto next;

            /* Check if closer to ball than current best */
            if ((uint32_t)pl->ballDistance >= (uint32_t)bestDist)
                goto next;

            bestDist = pl->ballDistance;
            bestPlayer = pl;

        next:
            ;
        }

        scanTeam = &bottomTeamData;
    }

    findClosestBallDist = bestDist;
    return bestPlayer;
}


/* ===== AI_SetControlsDirection =====
 *
 * Main AI controller for computer-controlled teams.
 * Called from UpdatePlayers once per frame for AI teams.
 *
 * Decides direction, fire buttons, aftertouch and spin for AI.
 * Handles all game states: in-progress, set pieces, penalties,
 * result display, etc.
 *
 * Original: SWOS2.s lines 17832-18668 (~836 lines of asm)
 *
 * Register mapping:
 *   a6 = team, a5 = controlledPlayer, a4 = passToPlayer
 *   d7 = controlled player direction (or -1)
 *   d6 = squared distance ball-to-goal-center
 *   d5 = angle ball-to-goal-center (0..255)
 *   d2 = direction delta (byte arithmetic)
 */
void __attribute__((used, externally_visible))
_AI_SetControlsDirection(TeamGeneralInfo *team)
{
    Sprite *a5;             /* controlled player */
    Sprite *a4;             /* pass-to player */
    Sprite *a2 = (Sprite *)0; /* opponent's controlled player (set on some paths) */
    Sprite *closestPlayer;
    int16_t d7;             /* controlled player direction, or -1 */
    int32_t d6;             /* squared distance ball to goal center */
    int16_t d5;             /* angle ball to goal center (byte range 0..255) */
    int16_t goalY;          /* opponent's goal line Y */
    int16_t ballX, ballY;
    int32_t dx32, dy32;
    int32_t outDeltaX, outDeltaY;
    int16_t direction;
    int16_t d0, d1, d2;
    int8_t d2_byte;
    int16_t wVar;

    /* Early exit if controls are reset */
    if (team->resetControls != 0)
        return;

    /* Decrement AI counters */
    if (aiCounter != 0)
        aiCounter--;
    if (aiResumePlayTimer != 0)
        aiResumePlayTimer--;

    /* Generate random value for this frame */
    aiRand = (int16_t)_Rand();

    /* Increment AI timer, clear all control outputs */
    team->AI_timer++;
    team->currentAllowedDirection = -1;
    team->firePressed = 0;
    team->fireThisFrame = 0;
    team->quickFire = 0;
    team->normalFire = 0;

    /* Don't process AI during substitutes menu */
    if (inSubstitutesMenu != 0)
        return;

    /* Load controlled player and pass target */
    d7 = -1;
    a5 = team->controlledPlayer;
    a4 = team->passToPlayerPtr;
    if (a5 != (Sprite *)0)
        d7 = a5->direction;

    /* Compute goal coordinates and squared distance */
    if (team == &bottomTeamData)
        goalY = 129;    /* upper goal line (bottom team attacks up) */
    else
        goalY = 769;    /* lower goal line (top team attacks down) */

    ballX = ballSprite.x.s.i;
    ballY = ballSprite.y.s.i;
    dx32 = (int32_t)(int16_t)(ballX - 336);
    dy32 = (int32_t)(int16_t)(ballY - goalY);
    d6 = dx32 * dx32 + dy32 * dy32;

    /* Calculate angle from ball toward center of opponent's goal */
    direction = (int16_t)_CalculateDeltaXAndY(
        336, goalY, ballX, ballY, 256, &outDeltaX, &outDeltaY);
    if (direction < 0)
        direction = 0;
    d5 = direction;

    /* ---- Game NOT in progress ---- */
    if (gameStatePl != ST_GAME_IN_PROGRESS) {

        /* Only the team that last played gets AI control */
        if (team != lastTeamPlayedBeforeBreak)
            return;

        /* Check if showing result screen (both teams computer) */
        if (gameState >= ST_STARTING_GAME && gameState <= ST_GAME_ENDED &&
            team1Computer != 0 && team2Computer != 0) {

            if (gameState == ST_RESULT_ON_HALFTIME ||
                gameState == ST_RESULT_AFTER_THE_GAME) {
                if (stoppageTimerTotal < 600)
                    return;
            } else {
                /* 350 ticks for other result states */
                if (stoppageTimerTotal < 350)
                    return;
            }
            /* Simulate fire press to dismiss result */
            team->firePressed = 1;
            return;
        }

        /* game_not_over: check stoppage timer threshold */
        d0 = (currentGameTick & 0x3F) + 100;
        if (stoppageTimerActive < (uint16_t)d0)
            goto set_direction;

        /* Check penalties */
        if (playingPenalties != 0 || gameState == ST_PENALTY)
            goto penalties_after_the_game;

        /* Keeper holds ball / goal out */
        if (gameState == ST_KEEPER_HOLDS_BALL ||
            gameState == ST_GOAL_OUT_LEFT ||
            gameState == ST_GOAL_OUT_RIGHT) {
            /* keepers_ball */
            if ((aiRand & 1) != 0) {
                if ((uint16_t)d7 == (uint16_t)cameraDirection)
                    goto apply_turn_delta_from_angle;
                if (ballSprite.x.s.i <= 336) {
                    if (d7 == 1 || d7 == 3)
                        goto apply_turn_delta_from_angle;
                } else {
                    if (d7 == 5 || d7 == 7)
                        goto apply_turn_delta_from_angle;
                }
            }
            goto check_closest_player_facing;
        }

        /* Players to initial positions (goal scored) */
        if (gameState == ST_PLAYERS_TO_INITIAL_POSITIONS) {
            /* goal_scored */
            if (stoppageTimerActive < 150)
                goto check_closest_player_facing;
            if ((uint16_t)d7 == (uint16_t)cameraDirection)
                goto apply_turn_delta_from_angle;
            goto check_closest_player_facing;
        }

        /* Free kicks */
        if (gameState >= ST_FREE_KICK_LEFT1 && gameState <= ST_FREE_KICK_RIGHT3)
            goto foul;

        /* Throw-ins */
        if (gameState >= ST_THROW_IN_FORWARD_RIGHT &&
            gameState <= ST_THROW_IN_BACK_LEFT) {
            uint8_t throwBits;
            d1 = gameState - ST_THROW_IN_FORWARD_RIGHT;
            throwBits = aiThrowInDirections[d1];
            if (team != &bottomTeamData) {
                /* ror.b #4,d1 → swap nibbles */
                throwBits = (uint8_t)((throwBits << 4) | (throwBits >> 4));
            }
            goto decide_throw_in_direction_bits_entry;

            decide_throw_in_direction_bits_entry:
            if ((aiRand & 0xF) != 0)
                goto check_closest_player_facing;
            if (d7 >= 0 && d7 <= 7 && (throwBits & (1 << (d7 & 7))) != 0)
                goto apply_turn_delta_from_angle;
            goto check_closest_player_facing;
        }

        /* Foul */
        if (gameState == ST_FOUL)
            goto foul;

        /* Default: apply turn from angle */
        goto apply_turn_delta_from_angle;
    }

    /* ---- Game in progress ---- */

    /* Penalty check */
    if (playingPenalties != 0 || penalty != 0) {
        if (team->spinTimer >= 0)
            goto we_have_ball_spin;
    }

    /* AI counter direction toward goal */
    if (aiCounter != 0)
        _AI_SetDirectionTowardOpponentsGoal(team);

    /* Need a controlled player */
    if (a5 == (Sprite *)0)
        return;

    /* Check if we're in spin mode */
    if (team->spinTimer >= 0)
        goto we_have_ball_spin;

    /* Check if a player is near the ball */
    if (team->plVeryCloseToBall != 0 || team->plCloseToBall != 0)
        goto theres_a_player_near;

    /* noone_near: */
    goto noone_near;

    /* ---- theres_a_player_near ---- */
theres_a_player_near:
    if (_AI_DecideWhetherToTriggerFire(a5, team, d7) == 0)
        return;  /* firing */

    /* deadVarAlways0 check (always 0 in practice) */
    if (deadVarAlways0 != 0) {
        if (team->opponentsTeam == (TeamGeneralInfo *)playerKickerPtr) {
            if (team->opponentsTeam->playerHasBall != 0)
                return;
        }
    }

    /* check_direction_and_field_half_gates */
    if (a5->direction == 2 || a5->direction == 6) {
        /* horizontal_dir_half_gate */
        if (team == &topTeamData) {
            if (ballSprite.y.s.i < 740)
                goto no_fire_fallback_flow;
        } else {
            if (ballSprite.y.s.i > 158)
                goto no_fire_fallback_flow;
        }
        goto distance_and_random_fire_gate;
    }

distance_and_random_fire_gate:
    if (d6 > 28800)
        goto no_fire_fallback_flow;
    if (d6 >= 12800) {
        if ((aiRand & 3) != 0)
            goto no_fire_fallback_flow;
    }

    /* set_angle_tolerance_by_distance */
    {
        int8_t tolerance = 0x0F;
        if (d6 <= 3200)
            tolerance = 0x32;

        /* compare_angle_to_tolerance */
        d2 = a5->direction;
        if (d2 < 0)
            goto no_fire_fallback_flow;
        d2_byte = (int8_t)((uint8_t)(d2 << 5) - (uint8_t)d5);
        if (d2_byte > tolerance || d2_byte < -tolerance)
            goto no_fire_fallback_flow;
    }

    /* angle_within_tolerance_action */
    if (aiResumePlayTimer != 0)
        return;
    if (team->passKickTimer >= 13)  /* AI_ResumeGameDelay: bcc = passKickTimer >= 13 */
        return;
    team->AI_afterTouchStrength = 0;
    goto resume_play_and_apply_turn_bias;

no_fire_fallback_flow:
    if (team->field_84 != 0) {
        team->field_84--;
        closestPlayer = _FindClosestPlayerToBallFacing(
            (uint16_t)(d7 << 5), team);
        if (closestPlayer != (Sprite *)0xFFFFFFFF &&
            a5->teamNumber == closestPlayer->teamNumber)
            goto our_player_closest;
        goto closest_facing_player_not_ours;
    }

    /* field84_cooldown_expired_path */
    if (d6 < 9800)
        goto choose_dir_when_not_very_close;
    if (a5 == (Sprite *)0)
        goto choose_dir_when_not_very_close;

    /* Load opponent's controlled player into a2 */
    {
        TeamGeneralInfo *opponentTeam = team->opponentsTeam;
        a2 = opponentTeam->controlledPlayer;
        if (a2 == (Sprite *)0)
            goto use_pass_target_if_valid;
        if (a2->ballDistance < 800)
            goto evaluate_closest_facing_player_midrange;
        if (a2->ballDistance < 5000)
            goto far_distance_decision_gate;
    }

use_pass_target_if_valid:
    {
        TeamGeneralInfo *opponentTeam = team->opponentsTeam;
        Sprite *passTarget = opponentTeam->passToPlayerPtr;
        if (passTarget == (Sprite *)0)
            goto choose_dir_when_not_very_close;
        if (passTarget->ballDistance < 800)
            goto evaluate_closest_facing_player_midrange;
        if (passTarget->ballDistance < 5000)
            goto far_distance_decision_gate;
        goto choose_dir_when_not_very_close;
    }

evaluate_closest_facing_player_midrange:
    if (d6 > 180000)
        goto decide_immediate_turn_or_gate;
    closestPlayer = _FindClosestPlayerToBallFacing(
        (uint16_t)(d7 << 5), team);
    if (closestPlayer == (Sprite *)0xFFFFFFFF)
        goto gate_field84_cooldown_and_set;
    if (a5->teamNumber == closestPlayer->teamNumber)
        goto our_player_closest;
    goto gate_field84_cooldown_and_set;

far_distance_decision_gate:
    if (aiRand <= 8) {
        if (d6 > 48400)
            goto decide_immediate_turn_or_gate;
    }
    /* tick_gated_closest_facing_check */
    if ((currentGameTick & 0xC) != 0)
        goto choose_dir_when_not_very_close;
    closestPlayer = _FindClosestPlayerToBallFacing(
        (uint16_t)(d7 << 5), team);
    if (closestPlayer == (Sprite *)0xFFFFFFFF)
        goto gate_field84_cooldown_and_set;
    if (a5->teamNumber == closestPlayer->teamNumber)
        goto our_player_closest;
    /* bra.w *+4 → fall through */

gate_field84_cooldown_and_set:
    if (team->field_84 != 0)
        goto choose_dir_when_not_very_close;
    if (((uint16_t)frameCount & 0x7F) >= 0x20)
        goto choose_dir_when_not_very_close;
    team->field_84 = 4;

closest_facing_player_not_ours:
    if (team->plVeryCloseToBall == 0)
        goto copy_current_direction_as_allowed;
    if ((uint8_t)(currentGameTick) & 0x80) {
        /* CW turn */
        d0 = (a5->direction + 1) & 7;
    } else {
        /* CCW turn */
        d0 = (a5->direction - 1) & 7;
    }
    team->currentAllowedDirection = d0;
    return;

choose_dir_when_not_very_close:
    if (team->plVeryCloseToBall == 0)
        goto copy_current_direction_as_allowed;
    /* set_allowed_dir_from_angle_bucket */
    goto set_allowed_dir_from_angle_bucket;

set_allowed_dir_from_angle_bucket:
    team->currentAllowedDirection = (int16_t)(((uint16_t)(d5 + 16)) & 0xFF) >> 5;
    return;

copy_current_direction_as_allowed:
    team->currentAllowedDirection = a5->direction;
    return;

derive_allowed_dir_from_full_angle:
    {
        uint16_t computedDir;
        computedDir = (uint16_t)(((uint8_t)(a5->fullDirection + 128) + 16) & 0xFF) >> 5;
        if (computedDir != (uint16_t)a5->direction &&
            a5->ballDistance >= 800 &&
            (currentGameTick & 0xE) != 0) {
            team->currentAllowedDirection = a5->direction;
            return;
        }
        team->currentAllowedDirection = (int16_t)computedDir;
        return;
    }

our_player_closest:
    team->field_84 = 0;
    if (aiResumePlayTimer != 0)
        return;
    if (team->passKickTimer >= 13)  /* AI_ResumeGameDelay */
        return;
    aiResumePlayTimer = 15;
    team->currentAllowedDirection = d7;
    team->quickFire = 1;
    if (AI_maxStoppageTime <= stoppageTimerActive)
        AI_maxStoppageTime = stoppageTimerActive;
    return;

decide_immediate_turn_or_gate:
    {
        int16_t angleBucket = (int16_t)(((uint16_t)(d5 + 16)) & 0xFF) >> 5;
        int16_t diff = (angleBucket - d7) & 7;
        if (diff == 0 || diff == 1 || diff == 7)
            goto setup_resume_play_and_turn_bias;
        goto gate_field84_cooldown_and_set;
    }

setup_resume_play_and_turn_bias:
    team->AI_afterTouchStrength = 2;
    if (d6 <= 115600)
        team->AI_afterTouchStrength = 1;

    team->currentAllowedDirection = d7;
    team->normalFire = 1;
    d2_byte = (int8_t)((uint8_t)(d7 << 5) - (uint8_t)d5);
    aiResumePlayTimer = 15;
    d0 = -1;
    if (d2_byte < 0)
        d0 = 1;

    /* adjust_turn_by_ball_motion */
    if (ballSprite.deltaY.raw < 0) {
        if (ballSprite.y.s.i < 342)
            goto commit_turn_to_ai_state;
    } else {
        if (ballSprite.y.s.i > 555)
            goto commit_turn_to_ai_state;
    }

    /* time_bucket_and_ballx_branch */
    {
        int16_t timeBucket = (currentGameTick & 0x1C) >> 2;
        int16_t bx = ballSprite.x.s.i;

        if (bx >= 193 && bx <= 478) {
            /* mid_right_x_gate */
            if (timeBucket == 0)
                goto set_zero_turn;
            if (timeBucket < 5)
                goto commit_turn_to_ai_state;
            goto invert_turn;
        }
        if (bx >= 118 && bx <= 553) {
            /* midfield_x_gate */
            if (timeBucket == 0)
                goto invert_turn;
            if (timeBucket < 4)
                goto set_zero_turn;
            goto commit_turn_to_ai_state;
        }
        /* outerwide_x_gate */
        if (timeBucket < 4)
            goto commit_turn_to_ai_state;
    }

set_zero_turn:
    d0 = 0;
    goto commit_turn_to_ai_state;

invert_turn:
    d0 = -d0;

commit_turn_to_ai_state:
    team->AI_ballSpinDirection = d0;
    return;

    /* ---- Penalty path (game not in progress) ---- */
penalties_after_the_game:
    {
        int16_t randDir = aiRand & 7;
        if (playerTurnFlags & (1 << randDir)) {
            team->currentAllowedDirection = randDir;
            return;
        }
    }
    /* random_direction_disallowed */
    if (a5->direction == 0 || a5->direction == 4)
        return;
    goto apply_turn_delta_from_angle;

foul:
    if ((aiRand & 0xF) == 0)
        goto check_closest_player_facing;
    /* compute_allowed_dir_from_angle_bucket */
    {
        int16_t angleBucket = (int16_t)(((uint16_t)(d5 + 16)) & 0xFF) >> 5;
        if (angleBucket == d7)
            goto apply_turn_delta_from_angle;
        team->currentAllowedDirection = angleBucket;
        return;
    }

apply_turn_delta_from_angle:
    if (d7 < 0)
        return;
    d2_byte = (int8_t)((uint8_t)(d7 << 5) - (uint8_t)d5);
    goto prep_aftertouch_and_fire_decision;

check_closest_player_facing:
    closestPlayer = _FindClosestPlayerToBallFacing(
        (uint16_t)(d7 << 5), team);
    if (closestPlayer != (Sprite *)0xFFFFFFFF &&
        a5 != (Sprite *)0 &&
        a5->teamNumber == closestPlayer->teamNumber)
        goto our_player_closest;

    /* no_player_facing */
    goto no_player_facing;

no_player_facing:
    if ((currentGameTick & 0xE) != 0)
        return;

    if (aiTurnDirection < 0) {
        if (aiTurnDirection != -1) {
            aiTurnDirection++;
            if (aiTurnDirection != -1)
                return;
        }
        d1 = 1;
    } else {
        if (aiTurnDirection != 1) {
            aiTurnDirection--;
            if (aiTurnDirection != 1)
                return;
        }
        d1 = -1;
    }

    /* try_apply_turn_step */
    d0 = (d7 + aiTurnDirection) & 7;
    if (playerTurnFlags & (1 << d0)) {
        team->currentAllowedDirection = d0;
        return;
    }
    /* commit_turn_direction_and_return */
    aiTurnDirection = d1;
    return;

set_direction:
    /* set_direction_core */
    if (resultTimer != 0)
        return;
    if (gameState == ST_GOAL_OUT_LEFT ||
        gameState == ST_GOAL_OUT_RIGHT ||
        gameState == ST_KEEPER_HOLDS_BALL)
        goto no_player_facing;
    if (gameState >= ST_THROW_IN_FORWARD_RIGHT &&
        gameState <= ST_THROW_IN_BACK_LEFT)
        goto no_player_facing;
    if (gameState == ST_FOUL)
        goto foul_or_free_kick_set_dir;
    if (gameState < ST_FREE_KICK_LEFT1 || gameState > ST_FREE_KICK_RIGHT3)
        return;

foul_or_free_kick_set_dir:
    /* Force direction toward opponent's goal */
    team->currentAllowedDirection = (int16_t)(((uint16_t)(d5 + 16)) & 0xFF) >> 5;
    return;

    /* ---- Aftertouch and fire decision ---- */
prep_aftertouch_and_fire_decision:
    if (AI_maxStoppageTime <= stoppageTimerActive)
        AI_maxStoppageTime = stoppageTimerActive;
    aiResumePlayTimer = 15;

    if (gameState == ST_PENALTY || gameState == ST_PENALTIES) {
        team->AI_afterTouchStrength = 0;
        goto apply_dir_and_normal_fire;
    }

    if (gameState >= ST_FREE_KICK_LEFT1 && gameState <= ST_FREE_KICK_RIGHT3)
        goto aftertouch_by_distance;

    if (gameState == ST_CORNER_LEFT || gameState == ST_CORNER_RIGHT) {
        team->AI_afterTouchStrength = 1;
        goto apply_dir_and_normal_fire;
    }

    /* non_setpiece_random_branch */
    if ((aiRand & 0x18) != 0)
        goto aftertouch_by_distance;
    /* Fall through to set_aftertouch_strong */
    team->AI_afterTouchStrength = 2;
    goto apply_dir_and_normal_fire;

aftertouch_by_distance:
    if (d6 < 28800) {
        team->AI_afterTouchStrength = 0;
    } else if (d6 < 57800) {
        team->AI_afterTouchStrength = 1;
    } else {
        team->AI_afterTouchStrength = 2;
    }

apply_dir_and_normal_fire:
    team->currentAllowedDirection = d7;
    team->normalFire = 1;

    /* Penalty/penalties: zero spin and return */
    if (gameState == ST_PENALTY || gameState == ST_PENALTIES) {
        team->AI_ballSpinDirection = 0;
        return;
    }

    /* Corner: random spin direction */
    if (gameState == ST_CORNER_LEFT || gameState == ST_CORNER_RIGHT) {
        int16_t randVal = aiRand & 7;
        int16_t spinDir;
        int16_t testDir;
        if (randVal < 3) {
            spinDir = 1;
            testDir = (d7 + 1) & 7;
        } else if (randVal < 6) {
            spinDir = -1;
            testDir = (d7 - 1) & 7;
        } else {
            team->AI_ballSpinDirection = 0;
            return;
        }
        if (playerTurnFlags & (1 << testDir)) {
            team->AI_ballSpinDirection = spinDir;
            return;
        }
        team->AI_ballSpinDirection = 0;
        return;
    }

    /* General foul/free kick: compute turn from angle sign */
    aiResumePlayTimer = 15;
    team->currentAllowedDirection = d7;
    team->normalFire = 1;
    wVar = 0;

    if (gameState == ST_FOUL && a2 != (Sprite *)0) {
        if (team == &topTeamData) {
            if (a2->y.s.i >= 682)
                goto apply_turn_and_return;
        } else {
            if (a2->y.s.i <= 216)
                goto apply_turn_and_return;
        }
    }

    /* compute_turn_from_angle_sign */
    wVar = -1;
    if (d2_byte < 0)
        wVar = 1;

apply_turn_and_return:
    team->AI_ballSpinDirection = wVar;
    return;

resume_play_and_apply_turn_bias:
    aiResumePlayTimer = 15;
    team->currentAllowedDirection = d7;
    team->normalFire = 1;
    d0 = -1;
    if (d2_byte < 0)
        d0 = 1;
    team->AI_ballSpinDirection = d0;
    return;

    /* ---- noone_near path ---- */
noone_near:
    if (_AI_DecideWhetherToTriggerFire(a5, team, d7) == 0)
        return;

    /* Check pass-to player distance difference */
    if (a4 != (Sprite *)0) {
        if (a4->ballDistance - a5->ballDistance >= 50) {
            /* Swap controlled and pass-to player */
            Sprite *tmp = team->passToPlayerPtr;
            team->passToPlayerPtr = team->controlledPlayer;
            team->controlledPlayer = tmp;
            return;
        }
    }

    /* choose_full_angle_or_random_bucket */
    if (a4 == (Sprite *)0)
        goto derive_allowed_dir_from_full_angle;

    /* deadVarAlways0 is always 0 */
    if (deadVarAlways0 != 0)
        goto set_allowed_dir_from_angle_bucket;

    if (topTeamData.playerNumber != 0 || bottomTeamData.playerNumber != 0)
        goto derive_allowed_dir_from_full_angle;

    /* random_rotate_when_no_target: both teams are computer */
    {
        d0 = -1;
        if (a5->x.s.i < 273 || a5->x.s.i > 398 ||
            a5->y.s.i <= 158 || a5->y.s.i >= 740) {
            /* Player on edges: chance of random rotate */
            if ((currentGameTick & 0x18) != 0) {
                /* 87.5%: use base direction */
                team->currentAllowedDirection = d7;
                return;
            }
            /* 12.5%: random rotate */
            {
                int16_t rotateIdx = (aiRand & 2);  /* 0 or 2 = byte offset */
                int16_t rotateOffset = aiRandomRotateTable[rotateIdx >> 1];
                uint8_t rotated = (uint8_t)(a5->fullDirection + rotateOffset);
                rotated = (uint8_t)(rotated + 128);  /* flip 180 degrees (byte add) */
                d0 = (int16_t)(((uint16_t)(rotated + 16)) & 0xFF) >> 5;
            }
        }
        /* If player is in center of pitch, d0 stays -1 (0xFFFF) */
        team->currentAllowedDirection = d0;
        return;
    }

    /* ---- Spin/aftertouch when we have ball ---- */
we_have_ball_spin:
    d1 = team->controlledPlDirection;

    if (playingPenalties == 0 && penalty == 0) {
        if ((aiRand & 1) == 0)
            goto apply_spin_quick_or_copy_dir;
        if (team->AI_ballSpinDirection < 0) {
            /* use_left_spin_table */
            d0 = (d1 - 1) & 7;
            {
                int16_t tableIdx = team->AI_afterTouchStrength;
                if (tableIdx > 2) tableIdx = 2;
                d1 = (d1 + aiLeftSpinTable[tableIdx]) & 7;
            }
            team->currentAllowedDirection = d1;
            return;
        }
        if (team->AI_ballSpinDirection > 0) {
            /* use_right_spin_table */
            d0 = (d1 + 1) & 7;
            {
                int16_t tableIdx = team->AI_afterTouchStrength;
                if (tableIdx > 2) tableIdx = 2;
                d1 = (d1 + aiRotateRightTable[tableIdx]) & 7;
            }
            team->currentAllowedDirection = d1;
            return;
        }
        /* AI_ballSpinDirection == 0: fall through to apply_spin_quick_or_copy_dir */
    }

apply_spin_quick_or_copy_dir:
    if (team->AI_afterTouchStrength == 1) {
        team->currentAllowedDirection = -1;
        return;
    }
    /* use_long_kick_table */
    {
        int16_t tableIdx = team->AI_afterTouchStrength;
        if (tableIdx > 2) tableIdx = 2;
        d1 = (d1 + aiLongKickTable[tableIdx]) & 7;
    }
    team->currentAllowedDirection = d1;
    return;
}
