/*
 * dos_gameplay.c — apply the DOS (70 FPS) or Amiga (50 FPS) gameplay tuning.
 * See dos_gameplay.h / docs/DOS_GAMEPLAY.md. Everything here is gated on
 * gDosGameplay; with it 0 the Amiga values (== the globals' defaults) are written.
 */
#include "dos_gameplay.h"
#include "dos_mode.h"
#include "tactics_data.h"     /* gDosGameplay */
#include "swos_types.h"

/* ---- Keeper (already named globals in swos_amiga_data_imports.c) ---------- */
extern uint16_t _kKeeperSaveDistance[1];      /* Amiga 24 / DOS 16 (dive/save threshold) */
extern uint32_t _kGoalkeeperDiveDeltas[8];    /* keeper reach table; DOS = Amiga - 0x8000 */

/* ---- Ball physics — ALREADY named globals (swos_amiga_data_imports.c), read by
 * ball_physics.c (UpdateBall) and match_setup.c. We just write the per-style value;
 * no consumer change needed. Defaults there are the Amiga values. ---------------- */
extern uint16_t _kBallGroundConstant[1];      /* Amiga 16 / DOS 13 — ground friction */
extern uint16_t _kBallAirConstant[1];         /* Amiga 10 / DOS 4  — air friction (z>0) */
extern uint32_t _kGravityConstant[1];         /* Amiga 4608 / DOS 3291 — z-velocity decr */
extern uint16_t _kPitchBallSpeedInfluence[7]; /* first 3 entries differ DOS vs Amiga */

/* ---- Match clock — GS_54_49. WIRED: game_state.c:UpdateTime reads this for the
 * per-in-game-second accumulator reload, which sets the whole match-clock cadence. */
int16_t  _gsFramesPerGameSecond = 49;         /* Amiga 49 / DOS 54 — clock reload */

/* ---- GS_* stoppage timers — promoted globals (default = Amiga), each wired to its
 * consumer (verified site-by-site against swos-2020). ----------------------------- */
int16_t  _gsEndGameCounter      = 50;   /* GS_55_50   game_state.c    last-minute reload */
int16_t  _gsBallDefensiveZ      = 22;   /* GS_27_22   update_players.c goalie-catch z-thresh */
int16_t  _gsPenaltyTimer        = 100;  /* GS_110_100 game_timers.c   penalty -> NextPenalty */
/* The OTHER penaltiesTimer gate, and it scales the same way. During a penalty
 * shoot-out `UpdatePlayers` sends a goalkeeper straight to the speed/delta tail
 * once the timer is past this, skipping the whole keeper block:
 * `cmpi.w #50,(penaltiesTimer)` (SWOS2.s:29226) against `cmp penaltiesTimer,
 * 55` (swos.asm:112784). One site in each, and its sibling above is already
 * 100/110, so the pair scales together. Found by the replay rig:
 * zkz-10140228 gave both keepers the stopped-play speed table five frames
 * before the reference did (docs/TESTS.md). */
int16_t  _gsPenaltiesKeeperFree = 50;  /* GS_55_50   update_players.c penalty keeper tail */
int16_t  _gsHalftimeStoppage    = 350;  /* GS_385_350 ai.c            non-halftime result dismiss */
int16_t  _gsBallOutOfGameTimer  = 500;  /* GS_550_500 game_timers.c   ball-out-of-play timer */
int16_t  _gsResultAutoAdvance   = 600;  /* GS_660_600 ai.c            half/fulltime result dismiss */
int16_t  _gsKickoffStoppage     = 750;  /* GS_825_750 game_timers.c   kickoff stoppage */
int16_t  _gsStoppageEventReload = 50;   /* GS_GK_55_50 game_timers.c  stoppageEvent reload */
/* The two result-banner display lengths ShowResult (game_state.c) maps its magic
 * 30000/31000 markers to. Found by the recorded-game replay rig: a DOS recording
 * sets resultTimer to 165 at a stoppage where we set 150 (docs/TESTS.md). They fit
 * the same x1.1 DOS:Amiga family as every GS_* above, and swos-2020's DOS source
 * hard-codes 275 (game.cpp:13024). */
int16_t  _gsResultHalftimeLength = 250; /* GS_275_250 game_state.c    30000 -> banner length */
int16_t  _gsResultBreakLength    = 150; /* GS_165_150 game_state.c    31000 -> banner length */
/* The bench menu's held-direction auto-repeat delay: how many frames a held
 * up/down waits before the cursor steps again. NOT one of the x1.1 timing
 * conversions above — the DOS release doubled it (2 -> 4), which is slower than
 * the frame rate alone would justify (2 * 70/50 would be 3), so it is a feel
 * change, not a port artefact. Both are in the disassemblies:
 * `cmpi.w #2,(subsMovementDelayTimer)` (data/SWOS2.s:18639) vs
 * `cmp subsMovementDelayTimer, 4` (swos-port/swos/swos.asm:92055), routines
 * otherwise identical. Found by the replay rig: a DOS recording holds DOWN in
 * the substitutes menu for four frames and steps the cursor ONCE; at 2 we
 * stepped it twice and substituted the wrong player (docs/TESTS.md). */
int16_t  _gsSubsMovementDelay    = 2;   /* GS_4_2     bench_subs.c    menu repeat delay */
/* How long the camera takes to come back from the bench after the substitutes
 * menu closes. `move.w #50,(cameraLeavingSubsTimer)` (data/SWOS2.s:18791) vs
 * `mov g_cameraLeavingSubsTimer, 55` (swos-port/swos/swos.asm:92305) — one write
 * site in each, and 50 -> 55 is the same x1.1 family as every GS_* above, so
 * this is the conversion's systematic 50 Hz -> 70 Hz timer scaling rather than a
 * behaviour change. Found by the replay rig: our bench check resumed exactly 5
 * frames early after a substitution (docs/TESTS.md). */
int16_t  _gsCameraLeavingSubs    = 50;  /* GS_55_50   bench_subs.c    camera back from bench */
/* How long a player stays down after starting a tackle or a jumping header.
 * `move.b #50,SPRITE_PLAYERDOWNTIMER(a1)` in PlayerBeginTackling (SWOS2.s
 * 27016) and PlayerAttemptingJumpHeader (27140); the DOS build writes 55 at
 * both (swos.asm 106276, 108938). Checked site by site rather than scaled:
 * PlayerTackled's own 50 (SWOS2.s:24312 `#$32`) stays 50 in the DOS build too
 * (swos.asm:107958), and the keeper's 75/15 and the 20/25s are unchanged
 * everywhere — so this is two specific writes, not the whole family. Found by
 * the replay rig: two recordings stopped on sprite[N].player_down_timer with a
 * recorded 55 counting down against our 50 (docs/TESTS.md). */
int16_t  _gsPlayerDownTimer      = 50;  /* GS_55_50   tackle_foul.c/headers.c */

/* Throw-in: how long the restart is left alone before the result banner is
 * hidden and the fire is blocked. `cmpi.w #50,(stoppageTimerActive).l / bhi`
 * (data/SWOS2.s:30563) vs `cmp stoppageTimerActive, 55 / ja`
 * (swos-port/swos/swos.asm:115401) — one site in each, the same x1.1 family.
 * It matters well beyond the banner: hiding it NEGATES `resultTimer`, and a
 * negative `resultTimer` is what makes `ShowResult` zero it, which in turn is
 * what lets `AI_SetControlsDirection`'s `tst.w resultTimer` gate fall through
 * and start rotating the thrower. Five frames early there turned an AI
 * throw-in taker the reference left facing where he was (docs/TESTS.md). */
int16_t  _gsThrowInHideResult    = 50;  /* GS_55_50   update_players.c  throw-in UI */

/* ---- stoppageEventTimer reloads (match_setup.c) ----
 * How long each end-of-period pause runs before the state machine advances.
 * Both listings write these as literals and they are NOT one family: of the
 * twelve sites, seven scale x1.1 and five do not, so they are paired here
 * routine by routine rather than scaled wholesale.
 *
 *   routine                      SWOS2.s   swos.asm   scaled
 *   UpdateCameraBreakMode           50        55       yes -> _gsStoppageEventReload
 *   UpdateCameraBreakMode           75        75       no
 *   StartingMatch                  100       100       no
 *   SetCameraMovingToShowerState   100       110       yes
 *   EndFirstHalf                   100       100       no
 *   FirstHalfJustEnded             250       275       yes
 *   EndOfGame                      150       150       no
 *   PlayersLeavingPitch            250       275       yes
 *   GoToHalftime                   700       770       yes
 *   GameOver                      1500      1650       yes
 *   StartFirstExtraTime            100       110       yes
 *   EndFirstExtraTime              100       110       yes
 *
 * Found by the replay rig: zkz-10140228 left ST_GOING_TO_HALFTIME 25 frames
 * early because FirstHalfJustEnded armed 250 where the DOS build arms 275. */
int16_t  _gsStoppageShowerPan    = 100;  /* GS_110_100  camera to showers, extra time */
int16_t  _gsStoppageHalfEnded    = 250;  /* GS_275_250  first half / players leaving */
int16_t  _gsStoppageHalftime     = 700;  /* GS_770_700  GoToHalftime */
int16_t  _gsStoppageGameOver     = 1500; /* GS_1650_1500 GameOver */

/* ---- speed_factor — the per-frame movement-delta scale (docs/DOS_GAMEPLAY.md §8.3).
 * Reproduces swos-2020's CalculateDeltaXAndY_125 hook (symbols.txt:2083 ->
 * render.cpp:4682): the deltaX/deltaY computed by CalculateDeltaXAndY are scaled by
 * num/den so a higher frame rate doesn't play faster. DOS/PC 70fps = 41/64; Amiga
 * 50fps = 1/1 (no-op). Read at the tail of CalculateDeltaXAndY (math68k.c). Kept as
 * an integer ratio (not a float) to match swos-2020's `* 41 / 64` truncation
 * exactly. Ball trajectory is NOT scaled here — it rides the §3 physics constants;
 * callers that read only the direction (AI goal-angle, tactics arrows) are
 * unaffected. Default 1/1 so the Amiga build (dos=0) never scales. ---------------- */
int16_t  _gMoveDeltaScaleNum = 1;       /* DOS 41 / Amiga 1  (future Amiga-60: 50) */
int16_t  _gMoveDeltaScaleDen = 1;       /* DOS 64 / Amiga 1  (future Amiga-60: 60) */

/* GS_54_49 — MEASURED, not chosen. docs/DOS_GAMEPLAY.md originally took 54 from
 * swos-2020's `GS_54_49 = 69; // original = 54`, but a DOS recording settles it:
 * zkz-12065730 reaches game-minute 1 at frame 702 and minute 2 at frame 842, i.e.
 * 140 frames per game-minute. The accumulator subtracts gameLenSecondsTable[0] = 30
 * per frame and reloads by this constant, so seconds advance at 30/N per frame and
 * 60 seconds take 2*N frames: N = 70. (54 would give 108 frames/minute, 69 would
 * give 138.) swos-port agrees — gameTime.cpp kPcTicksPerGameSeconds = 70 — and its
 * replay of this same corpus passes. The Amiga 49 is unchanged and untouched. */
#define GS_FRAMES_PER_GAME_SECOND_DOS  70    /* was 54 (swos-2020 comment); see above */
#define GS_FRAMES_PER_GAME_SECOND_AMIGA 49

int DosGameplay_TargetFps(void)
{
    return _gDosGameplay ? 70 : 50;
}

void ApplyGameStyle(void)
{
    const int dos = _gDosGameplay;
    int i;

    /* --- Formations ---
     * The DOS release re-tuned 4-4-2, 4-3-3 and Sweep (21 coverage bytes; its
     * 3-4-3 is the pristine Amiga one), and the replay corpus says those rows
     * are part of what --dos-gameplay means: three recordings roughly quadruple
     * their `sprites` depth with the DOS set in. docs/TESTS.md has the
     * measurement and docs/TACTICS_POSITION_VALUE.md the byte-level diff. */
    TacticsData_SelectRelease(dos);

    /* --- Goalkeeper --- */
    _kKeeperSaveDistance[0] = dos ? 16 : 24;
    /* DOS reacts at shorter range: the whole reach table shifts down by 0x8000. */
    for (i = 0; i < 8; i++) {
        uint32_t amiga = (uint32_t)(0x30000 + i * 0x8000);
        _kGoalkeeperDiveDeltas[i] = dos ? (amiga - 0x8000u) : amiga;
    }

    /* --- Ball physics (existing k-globals) --- */
    _kBallGroundConstant[0] = dos ? 13u : 16u;
    _kBallAirConstant[0]    = dos ? 4u  : 10u;
    _kGravityConstant[0]    = dos ? 3291u : 4608u;
    /* Only the first 3 entries of kPitchBallSpeedInfluence differ between styles
     * (stored as uint16 holding signed values, read back as int16). */
    _kPitchBallSpeedInfluence[0] = (uint16_t)(int16_t)(dos ? -3 : -2);
    _kPitchBallSpeedInfluence[1] = dos ? 4u : 2u;
    _kPitchBallSpeedInfluence[2] = dos ? 1u : 3u;
    /* [3..6] = 0,0,-1,-1 in both styles (left at their defaults). */

    /* --- Match clock + GS_* stoppage timers --- */
    _gsFramesPerGameSecond = dos ? GS_FRAMES_PER_GAME_SECOND_DOS : GS_FRAMES_PER_GAME_SECOND_AMIGA;
    _gsEndGameCounter      = dos ? 55  : 50;
    _gsBallDefensiveZ      = dos ? 27  : 22;
    _gsPenaltyTimer        = dos ? 110 : 100;
    _gsPenaltiesKeeperFree = dos ?  55 :  50;
    _gsHalftimeStoppage    = dos ? 385 : 350;
    _gsBallOutOfGameTimer  = dos ? 550 : 500;
    _gsResultAutoAdvance   = dos ? 660 : 600;
    _gsKickoffStoppage     = dos ? 825 : 750;
    _gsStoppageEventReload = dos ? 55  : 50;
    _gsResultHalftimeLength = dos ? 275 : 250;
    _gsResultBreakLength    = dos ? 165 : 150;
    _gsSubsMovementDelay    = dos ? 4   : 2;
    _gsCameraLeavingSubs    = dos ? 55  : 50;
    _gsPlayerDownTimer      = dos ? 55  : 50;
    _gsThrowInHideResult    = dos ? 55  : 50;
    _gsStoppageShowerPan    = dos ? 110 : 100;
    _gsStoppageHalfEnded    = dos ? 275 : 250;
    _gsStoppageHalftime     = dos ? 770 : 700;
    _gsStoppageGameOver     = dos ? 1650: 1500;

    /* --- Movement speed_factor (swos-2020 CalculateDeltaXAndY_125, §8.3) ---
     * DOS/PC 70fps scales the per-frame movement deltas by 41/64 so players don't
     * cover 1.4x the ground per second at the higher frame rate; Amiga 50fps = 1/1. */
    _gMoveDeltaScaleNum = dos ? 41 : 1;
    _gMoveDeltaScaleDen = dos ? 64 : 1;
}
