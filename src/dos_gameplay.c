/*
 * dos_gameplay.c — apply the DOS (70 FPS) or Amiga (50 FPS) gameplay tuning.
 * See dos_gameplay.h / docs/DOS_GAMEPLAY.md. Everything here is gated on
 * gDosGameplay; with it 0 the Amiga values (== the globals' defaults) are written.
 */
#include "dos_gameplay.h"
#include "dos_mode.h"     /* gDosGameplay */
#include "swos_types.h"

/* ---- Keeper (already named globals in swos_amiga_data_imports.c) ---------- */
extern uint16_t kKeeperSaveDistance[1];      /* Amiga 24 / DOS 16 (dive/save threshold) */
extern uint32_t kGoalkeeperDiveDeltas[8];    /* keeper reach table; DOS = Amiga - 0x8000 */

/* ---- Ball physics — ALREADY named globals (swos_amiga_data_imports.c), read by
 * ball_physics.c (_UpdateBall) and match_setup.c. We just write the per-style value;
 * no consumer change needed. Defaults there are the Amiga values. ---------------- */
extern uint16_t kBallGroundConstant[1];      /* Amiga 16 / DOS 13 — ground friction */
extern uint16_t kBallAirConstant[1];         /* Amiga 10 / DOS 4  — air friction (z>0) */
extern uint32_t kGravityConstant[1];         /* Amiga 4608 / DOS 3291 — z-velocity decr */
extern uint16_t kPitchBallSpeedInfluence[7]; /* first 3 entries differ DOS vs Amiga */

/* ---- Match clock — GS_54_49. WIRED: game_state.c:_UpdateTime reads this for the
 * per-in-game-second accumulator reload, which sets the whole match-clock cadence. */
int16_t  gsFramesPerGameSecond = 49;         /* Amiga 49 / DOS 54 — clock reload */

/* TODO — the remaining GS_* STOPPAGE timers are NOT wired yet. Their consumers are
 * inline literals scattered across the match engine (ai.c, ball_possession.c,
 * referee.c, game_state.c, ...) and are ambiguous to map by value alone, so they
 * need identifying one by one before promotion. They affect stoppage DURATIONS only;
 * the core DOS feel (ball physics, keeper, clock cadence, 70 FPS) is complete without
 * them. Doc values (Amiga / DOS) for when each reload site is located:
 *   GS_55_50   endGameCounter reload          50 / 55
 *   GS_27_22   ball defensive-Z goalie catch  22 / 27
 *   GS_110_100 penalty timer -> NextPenalty  100 / 110
 *   GS_385_350 halftime-result stoppage      350 / 385
 *   GS_550_500 ballOutOfGameTimer            500 / 550
 *   GS_660_600 result auto-advance           600 / 660
 *   GS_825_750 kickoff stoppage              750 / 825
 *   GS_GK_55_50 stoppageEvent reload          50 / 55  */

/* GS_54_49: the doc keeps the authentic ORIGINAL DOS value 54 as the default, and
 * exposes swos-2020's deliberate retune of 69 as a tweak. Flip this to test. */
#define GS_FRAMES_PER_GAME_SECOND_DOS  54    /* original DOS = 54; swos-2020 = 69 */
#define GS_FRAMES_PER_GAME_SECOND_AMIGA 49

int DosGameplay_TargetFps(void)
{
    return gDosGameplay ? 70 : 50;
}

void ApplyGameStyle(void)
{
    const int dos = gDosGameplay;
    int i;

    /* --- Goalkeeper --- */
    kKeeperSaveDistance[0] = dos ? 16 : 24;
    /* DOS reacts at shorter range: the whole reach table shifts down by 0x8000. */
    for (i = 0; i < 8; i++) {
        uint32_t amiga = (uint32_t)(0x30000 + i * 0x8000);
        kGoalkeeperDiveDeltas[i] = dos ? (amiga - 0x8000u) : amiga;
    }

    /* --- Ball physics (existing k-globals) --- */
    kBallGroundConstant[0] = dos ? 13u : 16u;
    kBallAirConstant[0]    = dos ? 4u  : 10u;
    kGravityConstant[0]    = dos ? 3291u : 4608u;
    /* Only the first 3 entries of kPitchBallSpeedInfluence differ between styles
     * (stored as uint16 holding signed values, read back as int16). */
    kPitchBallSpeedInfluence[0] = (uint16_t)(int16_t)(dos ? -3 : -2);
    kPitchBallSpeedInfluence[1] = dos ? 4u : 2u;
    kPitchBallSpeedInfluence[2] = dos ? 1u : 3u;
    /* [3..6] = 0,0,-1,-1 in both styles (left at their defaults). */

    /* --- Match clock (GS_54_49). The other GS_* stoppage timers are not wired yet
     *     (see the TODO above); add them here as their consumers are promoted. --- */
    gsFramesPerGameSecond = dos ? GS_FRAMES_PER_GAME_SECOND_DOS : GS_FRAMES_PER_GAME_SECOND_AMIGA;
}
