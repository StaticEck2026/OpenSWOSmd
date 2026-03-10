#include "camera.h"
#include "game_types.h"
#include "display.h"

/* ================================================================
 * cameraX/cameraY are 32-bit fixed-point values in memory (dc.l).
 * High word = integer position, low word = sub-pixel fraction.
 * Other files declare them as int16_t (reads only the integer part
 * on big-endian 68k). MoveCamera needs the full 32-bit value for
 * fractional accumulation, so we use __asm__ aliases to avoid LTO
 * type mismatch warnings with the int16_t declarations elsewhere.
 * ================================================================ */
extern int32_t cameraXFP __asm__("cameraX");
extern int32_t cameraYFP __asm__("cameraY");
extern uint16_t cameraCoordinatesValid;

/* ---- External globals ---- */
extern uint16_t showFansCounter;
extern Sprite   ballSprite;
extern uint16_t whichCard;
extern Sprite  *bookedPlayer;
extern uint16_t playingPenalties;
extern uint16_t waitForPlayerToGoInTimer;
extern uint16_t leavingSubsMenu;
extern uint16_t inSubstitutesMenu;
extern Sprite   goal1TopSprite;
extern Sprite   goal2BottomSprite;
extern uint16_t substituteInProgress;
extern uint16_t benchXLimit;
extern uint16_t gameStatePl;
extern uint16_t gameState;
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;
extern int16_t  cameraDirection;
extern uint16_t trainingGame;
extern int16_t  penaltiesState;
extern uint16_t cameraXVelocity;
extern uint16_t cameraYVelocity;

/* ---- External functions ---- */
extern void _SetCameraCoordinates(int16_t targetX, int16_t targetY);

/* ---- Data tables (moved from asm inline data) ---- */

/* nextCameraDirections: given direction 0..7, provides x/y sign pairs.
 * Each entry is two int32_t (x_sign, y_sign) — 8 pairs total.
 * Values: -1, 0, or 1. */
static const int32_t nextCameraDirections[16] = {
    0, -1,  1, -1,  1, 0,  1, 1,
    0, 1,  -1, 1,  -1, 0,  -1, -1
};

/* Maximum camera movement per frame in 16.16 fixed-point = 5.0 pixels */
#define MAX_CAMERA_MOVEMENT  0x00050000L

/* ================================================================
 * MoveCamera  (SWOS2.s line 9625)
 *
 * Per-frame camera tracking. Priority order:
 * 1. showFansCounter active → do nothing
 * 2. Booking → lock on booked player
 * 3. Penalties → lock on penalty spot (336, 107)
 * 4. Substitution timers → lock on bench area
 * 5. Subs menu open → goal/bench state logic
 * 6. Normal game → velocity-based ball tracking
 * 7. Set pieces → narrowed bounds
 * 8. Special states → fixed camera targets
 *
 * Uses 16.16 fixed-point for smooth sub-pixel movement.
 * SetCameraCoordinates handles the integer position update.
 * ================================================================ */
__attribute__((used, externally_visible))
void _MoveCamera(void)
{
    int16_t boundsMargin;    /* d1 — controls how tight the camera clamp is */
    int16_t targetX;         /* d3 — camera target X */
    int16_t targetY;         /* d4 — camera target Y */
    int32_t ballDeltaX;      /* d3 in velocity path — 32-bit */
    int32_t ballDeltaY;      /* d4 in velocity path — 32-bit */

    if (showFansCounter != 0)
        return;

    /* Default: load ball sprite deltas for velocity path */
    ballDeltaX = ballSprite.deltaX.raw;
    ballDeltaY = ballSprite.deltaY.raw;

    /* ---- Priority 1: Booking ---- */
    if (whichCard != 0) {
        Sprite *bookedSpr = bookedPlayer;
        targetX = bookedSpr->x.s.i;
        targetY = bookedSpr->y.s.i;
        cameraXVelocity = 0;
        cameraYVelocity = 0;
        boundsMargin = 37;
        goto offset_and_clamp_to_view;
    }

    /* ---- Priority 2: Penalties ---- */
    if (playingPenalties != 0) {
        targetX = 336;
        targetY = 107;
        cameraXVelocity = 0;
        cameraYVelocity = 0;
        boundsMargin = 0;
        goto offset_and_clamp_to_view;
    }

    /* ---- Priority 3: Substitution timers ---- */
    if (waitForPlayerToGoInTimer != 0)
        goto bench_camera_mode;

    if (leavingSubsMenu != 0) {
        boundsMargin = 37;
        targetX = 211;
        targetY = 449;
        cameraXVelocity = 0;
        cameraYVelocity = 0;
        goto offset_and_clamp_to_view;
    }

    /* ---- Priority 4: Substitutes menu open ---- */
    if (inSubstitutesMenu != 0) {
        boundsMargin = 37;
        {
            int16_t camY = (int16_t)(cameraYFP >> 16);
            if (camY >= 303 && camY <= 323) {
                /* Camera is near the right Y range */
                boundsMargin = 37;

                /* Check if goal1TopSprite is visible */
                if (goal1TopSprite.imageIndex != (int16_t)0xFFFF) {
                    if (goal1TopSprite.onScreen != 0)
                        goto bench_camera_target_and_zero_vel;
                }

                /* Check if goal2BottomSprite is visible */
                if (goal2BottomSprite.imageIndex != (int16_t)0xFFFF) {
                    if (goal2BottomSprite.onScreen != 0)
                        goto bench_camera_target_and_zero_vel;
                }

                /* Check if substitute is in progress */
                if (substituteInProgress != 0)
                    goto bench_camera_mode;

                boundsMargin = 0;
                goto bench_camera_target_and_zero_vel;
            }
        }
        goto bench_camera_target_and_zero_vel;
    }

    /* ---- Priority 5: Pre-play camera direction ---- */
    if (gameStatePl != ST_GAME_IN_PROGRESS) {
        TeamGeneralInfo *teamPtr = lastTeamPlayedBeforeBreak;
        if (teamPtr != (TeamGeneralInfo *)0) {
            Sprite *spritePtr = teamPtr->controlledPlayer;
            if (spritePtr != (Sprite *)0) {
                int16_t dir = spritePtr->direction;
                /* Look up camera direction vector from table */
                ballDeltaX = nextCameraDirections[dir * 2];
                ballDeltaY = nextCameraDirections[dir * 2 + 1];
                goto apply_accel_limits_setup;
            }
        }
        /* Fallback: use global cameraDirection */
        if (cameraDirection >= 0) {
            int16_t dir = cameraDirection;
            ballDeltaX = nextCameraDirections[dir * 2];
            ballDeltaY = nextCameraDirections[dir * 2 + 1];
        } else {
            ballDeltaX = 0;
            ballDeltaY = 0;
        }
    }

    /* ---- Velocity update: accelerate toward ball movement direction ---- */
apply_accel_limits_setup:
    {
        int16_t accel = 2;       /* d0 */
        int16_t maxXVel = 40;    /* d1 = $28 */
        int16_t maxYVel = 40;    /* d2 = $28 */

        /* Update X velocity */
        if (ballDeltaX != 0) {
            if (ballDeltaX > 0) {
                /* Ball moving right → accelerate X positive */
                if ((int16_t)cameraXVelocity != maxXVel)
                    cameraXVelocity += accel;
            } else {
                /* Ball moving left → accelerate X negative */
                if ((int16_t)cameraXVelocity != -maxXVel)
                    cameraXVelocity -= accel;
            }
        }

        /* Update Y velocity */
        if (ballDeltaY != 0) {
            if (ballDeltaY > 0) {
                /* Ball moving down → accelerate Y positive */
                if ((int16_t)cameraYVelocity != maxYVel)
                    cameraYVelocity += accel;
            } else {
                /* Ball moving up → accelerate Y negative */
                if ((int16_t)cameraYVelocity != -maxYVel)
                    cameraYVelocity -= accel;
            }
        }

        /* ---- Bounds margin selection ---- */
        boundsMargin = 63;   /* d1 = $3F, default */

        if (gameStatePl == ST_GAME_IN_PROGRESS)
            goto state_based_camera_target;

        boundsMargin = 63;   /* redundant but matches asm */

        /* Narrow bounds for set pieces (corners and throw-ins) */
        if (gameState == ST_CORNER_LEFT || gameState == ST_CORNER_RIGHT)
            goto narrow_bounds_for_set_pieces;
        if (gameState >= ST_THROW_IN_FORWARD_RIGHT && gameState <= ST_THROW_IN_BACK_LEFT)
            goto narrow_bounds_for_set_pieces;

        goto state_based_camera_target;
    }

narrow_bounds_for_set_pieces:
    boundsMargin = 37;   /* d1 = $25 */

state_based_camera_target:
    /* Check for special game states that use fixed camera targets */
    if (gameState >= ST_STARTING_GAME && gameState <= ST_GAME_ENDED) {
        if (gameState == ST_STARTING_GAME)
            goto tunnel_halftime_camera_target;
        if (gameState == ST_CAMERA_GOING_TO_SHOWERS)
            goto tunnel_halftime_camera_target;
        if (gameState == ST_FIRST_HALF_ENDED)
            goto target_ball_position;
        if (gameState == ST_GAME_ENDED) {
            if (penaltiesState < 0)
                goto center_camera_fulltime;
            goto target_ball_position;
        }
        if (gameState == ST_GOING_TO_HALFTIME)
            goto tunnel_halftime_camera_target;
        if (gameState == ST_PLAYERS_GOING_TO_SHOWER)
            goto tunnel_halftime_camera_target;
        if (gameState == ST_RESULT_AFTER_THE_GAME)
            goto results_screen_camera_target;

        /* Other states in ST_STARTING_GAME..ST_GAME_ENDED range */
center_camera_fulltime:
        targetX = 0x150;
        targetY = 0x1C1;
        cameraXVelocity = 0;
        cameraYVelocity = 0;
        goto offset_and_clamp_to_view;
    }

target_ball_position:
    /* Use ball's integer position */
    targetX = ballSprite.x.s.i;
    targetY = ballSprite.y.s.i;
    goto offset_and_clamp_to_view;

results_screen_camera_target:
    targetX = 0x150;
    targetY = 0x81;
    cameraXVelocity = 0;
    cameraYVelocity = 0;
    goto offset_and_clamp_to_view;

tunnel_halftime_camera_target:
    targetX = 0x24E;
    targetY = 0x1C1;
    cameraXVelocity = 0;
    cameraYVelocity = 0;
    goto offset_and_clamp_to_view;

bench_camera_mode:
    boundsMargin = 0x33;    /* 51 */
    goto bench_camera_target_and_zero_vel;

bench_camera_target_and_zero_vel:
    targetX = (int16_t)benchXLimit;
    targetY = 0x1C1;
    cameraXVelocity = 0;
    cameraYVelocity = 0;
    /* fall through to offset_and_clamp_to_view */

offset_and_clamp_to_view:
    /* Center the view: subtract half screen dimensions */
    targetX -= 0xA8;    /* 168 */
    targetY -= 0x88;    /* 136 */

    /* Apply camera velocity */
    targetX += (int16_t)cameraXVelocity;
    targetY += (int16_t)cameraYVelocity;

    /* Clamp X to [boundsMargin, 0x150 - boundsMargin] */
    if (targetX < boundsMargin)
        targetX = boundsMargin;
    {
        int16_t xMax = 0x150 - boundsMargin;
        if (targetX >= xMax)
            targetX = xMax;
    }

    /* Clamp Y based on game mode */
    if (trainingGame != 0) {
        if (targetY <= 0x50)
            targetY = 0x50;
        if (targetY >= 0x220)
            targetY = 0x220;
    } else {
        if (targetY <= 0x10)
            targetY = 0x10;
        if (targetY >= 0x250)
            targetY = 0x250;
    }

    /* ---- Compute fixed-point delta from current camera position ---- */
    {
        int16_t cameraIntX = (int16_t)(cameraXFP >> 16);
        int16_t cameraIntY = (int16_t)(cameraYFP >> 16);
        int16_t dx = targetX - cameraIntX;
        int16_t dy = targetY - cameraIntY;

        if (dx == 0 && dy == 0)
            return;   /* No movement needed */

        /* Scale delta to 16.16 fixed-point */
        int32_t fixedDx = 0;
        if (dx != 0) {
            int16_t scaledDx = dx >> 2;   /* asr.w #2 */
            fixedDx = (int32_t)0x4000 * (int32_t)scaledDx;
        }

        int32_t fixedDy = 0;
        if (dy != 0) {
            int16_t scaledDy = dy >> 2;   /* asr.w #2 */
            fixedDy = (int32_t)0x4000 * (int32_t)scaledDy;
        }

        /* Clamp to ±MAX_CAMERA_MOVEMENT */
        if (fixedDx >= MAX_CAMERA_MOVEMENT)
            fixedDx = MAX_CAMERA_MOVEMENT;
        if (fixedDx <= -MAX_CAMERA_MOVEMENT)
            fixedDx = -MAX_CAMERA_MOVEMENT;
        if (fixedDy >= MAX_CAMERA_MOVEMENT)
            fixedDy = MAX_CAMERA_MOVEMENT;
        if (fixedDy <= -MAX_CAMERA_MOVEMENT)
            fixedDy = -MAX_CAMERA_MOVEMENT;

        /* Add to current fixed-point camera position */
        int32_t newXFixed = fixedDx + cameraXFP;
        int32_t newYFixed = fixedDy + cameraYFP;

        /* Write fractional part only (low 16 bits of fixed-point result).
         * On big-endian 68k, cameraXFP high word = integer, low word = fraction.
         * We update only the fraction here; SetCameraCoordinates handles integer. */
        cameraXFP = (cameraXFP & (int32_t)0xFFFF0000L) | ((uint32_t)newXFixed & 0xFFFF);
        cameraYFP = (cameraYFP & (int32_t)0xFFFF0000L) | ((uint32_t)newYFixed & 0xFFFF);

        /* Extract integer part (swap d1/d2 in asm) */
        int16_t newIntX = (int16_t)(newXFixed >> 16);
        int16_t newIntY = (int16_t)(newYFixed >> 16);

        /* Final clamp of integer position */
        if (newIntX < 0)
            newIntX = 0;
        if (newIntX >= SCREEN_WIDTH)
            newIntX = SCREEN_WIDTH;

        if (newIntY < 0x10)
            newIntY = 0x10;
        if (newIntY >= 0x260)
            newIntY = 0x260;

        _SetCameraCoordinates(newIntX, newIntY);
        cameraCoordinatesValid = 0;
    }
}
