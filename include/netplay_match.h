/*
 * netplay_match.h -- a match played through the netplay frame step and GGPO (SDL builds).
 * docs/NETPLAY.md, "Playing a match through GGPO".
 *
 * GGPO re-simulates frames from inside its own calls, so a netplay match runs each frame
 * as a step -- the players' inputs, GameLoop_Frame, the clock, the buffer swap -- that
 * GGPO can call as often as it needs, and shows only the frame it ends on.
 *
 * For now the inputs come from a recording (--replay-test), which makes it a test: the
 * match played through the step, and through GGPO rolling it back, must be the match the
 * replay rig plays -- the same per-frame state hashes (--rollback=trace) and, compared
 * with the recording, the same result.
 *
 *   --netplay-match=direct          the netplay step, without GGPO
 *   --netplay-match=synctest[:N]    GGPO's synctest session: every N frames (default 1,
 *                                   at most 8) it loads the state of N frames back,
 *                                   re-simulates them and compares their checksums with
 *                                   the first run's
 */
#ifndef NETPLAY_MATCH_H
#define NETPLAY_MATCH_H

#ifdef SWOS_SDL_BUILD

#include "swos_types.h"

/* A --netplay-match=... argument: 1 = consumed, -1 = bad value. */
int32_t NetplayMatch_ParseArg(const char *arg);
int32_t NetplayMatch_Enabled(void);

/* ReplayTest_Run, before kick-off: open the session. 0, or non-zero on failure. */
int32_t NetplayMatch_Start(void);

/* GameLoop, in place of one pass of its loop: one frame, ended and shown. Returns a
 * GameFrameResult. */
int32_t NetplayMatch_Frame(void);

/* The end of the run: report what the session did and close it. The exit code, 0 or 2. */
int32_t NetplayMatch_Finish(void);

#endif /* SWOS_SDL_BUILD */
#endif /* NETPLAY_MATCH_H */
