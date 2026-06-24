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
 * ball_physics.c (UpdateBall) and match_setup.c. We just write the per-style value;
 * no consumer change needed. Defaults there are the Amiga values. ---------------- */
extern uint16_t kBallGroundConstant[1];      /* Amiga 16 / DOS 13 — ground friction */
extern uint16_t kBallAirConstant[1];         /* Amiga 10 / DOS 4  — air friction (z>0) */
extern uint32_t kGravityConstant[1];         /* Amiga 4608 / DOS 3291 — z-velocity decr */
extern uint16_t kPitchBallSpeedInfluence[7]; /* first 3 entries differ DOS vs Amiga */

/* ---- Match clock — GS_54_49. WIRED: game_state.c:UpdateTime reads this for the
 * per-in-game-second accumulator reload, which sets the whole match-clock cadence. */
int16_t  gsFramesPerGameSecond = 49;         /* Amiga 49 / DOS 54 — clock reload */

/* ---- GS_* stoppage timers — promoted globals (default = Amiga), each wired to its
 * consumer (verified site-by-site against swos-2020). ----------------------------- */
int16_t  gsEndGameCounter      = 50;   /* GS_55_50   game_state.c    last-minute reload */
int16_t  gsBallDefensiveZ      = 22;   /* GS_27_22   update_players.c goalie-catch z-thresh */
int16_t  gsPenaltyTimer        = 100;  /* GS_110_100 game_timers.c   penalty -> NextPenalty */
int16_t  gsHalftimeStoppage    = 350;  /* GS_385_350 ai.c            non-halftime result dismiss */
int16_t  gsBallOutOfGameTimer  = 500;  /* GS_550_500 game_timers.c   ball-out-of-play timer */
int16_t  gsResultAutoAdvance   = 600;  /* GS_660_600 ai.c            half/fulltime result dismiss */
int16_t  gsKickoffStoppage     = 750;  /* GS_825_750 game_timers.c   kickoff stoppage */
int16_t  gsStoppageEventReload = 50;   /* GS_GK_55_50 game_timers.c  stoppageEvent reload */

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

    /* --- Match clock + GS_* stoppage timers --- */
    gsFramesPerGameSecond = dos ? GS_FRAMES_PER_GAME_SECOND_DOS : GS_FRAMES_PER_GAME_SECOND_AMIGA;
    gsEndGameCounter      = dos ? 55  : 50;
    gsBallDefensiveZ      = dos ? 27  : 22;
    gsPenaltyTimer        = dos ? 110 : 100;
    gsHalftimeStoppage    = dos ? 385 : 350;
    gsBallOutOfGameTimer  = dos ? 550 : 500;
    gsResultAutoAdvance   = dos ? 660 : 600;
    gsKickoffStoppage     = dos ? 825 : 750;
    gsStoppageEventReload = dos ? 55  : 50;
}
