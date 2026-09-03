/*
 * replay_test.h — recorded-game replay regression rig (SDL build only).
 *
 * Replays a `.swr` (produced by scripts/rgd_convert.py --emit from a
 * swos-port `.RGD` recording) frame by frame: inject the recorded input,
 * run one game-loop iteration, compare every recorded NAMED FIELD against
 * the matching global, and stop at the first divergence.
 *
 * See docs/TESTS.md. The rig deliberately never parses a DOS struct — the
 * converter turns the recording into named fields so the two trees' layouts
 * can keep diverging.
 */
#ifndef REPLAY_TEST_H
#define REPLAY_TEST_H

#ifdef SWOS_SDL_BUILD

#include "swos_types.h"
#include "game_types.h"

/* Run swos-port's CalculateDeltaXAndY reference vectors against ours and
 * report. Standalone (`--replay-selftest`) — no recording needed. Returns 0
 * when all vectors match, 2 otherwise. */
int  ReplayTest_MathSelfTest(void);

/* Parse `path` and arm the rig. 0 = ok, non-zero = the run should abort.
 * `level` is one of "scalars", "teams", "info", "sprites" (or NULL to take
 * the level the file was emitted at); fields above the requested level are
 * loaded but not compared. `skipPrefixes` is an optional comma-separated
 * list of field-name prefixes to mute for this run. */
int  ReplayTest_Load(const char *path, const char *level, int maxErrors,
                     const char *skipPrefixes);

/* Non-zero once ReplayTest_Load has succeeded — the front-end is skipped. */
int  ReplayTest_Armed(void);

/* Non-zero while the match is being replayed (frame hooks live). */
int  ReplayTest_Running(void);

/* Drive the whole replay: install the recorded setup, start the match, and
 * exit the process with 0 (all frames matched) or 2 (divergence). */
void ReplayTest_Run(void);

/* Menu-free player assignment, from the recording's header. Called by
 * SetupPlayers in place of the two ShowPlayMatchMenu prompts. */
void ReplayTest_SetupPlayers(TeamFileEx *team1, TeamFileEx *team2);

/* Game-loop hooks (game_loop.c): inject this frame's input, then compare
 * the resulting state. */
void ReplayTest_FrameStart(void);
void ReplayTest_FrameEnd(void);

/* The recordings pin the RNG to one value — random.c returns it for both
 * Rand and Rand2 while this is >= 0. */
extern int _replayFixedRandom;

/* While the rig owns the frame clock, the Int3 handler must not advance
 * _currentGameTick: on Amiga it ticks once per VBlank in lockstep with the
 * loop, but on SDL two threads drive it from a wall clock. The rig
 * advances it exactly once per game-loop iteration instead. */
int  ReplayTest_OwnsGameTick(void);

#endif /* SWOS_SDL_BUILD */
#endif /* REPLAY_TEST_H */
