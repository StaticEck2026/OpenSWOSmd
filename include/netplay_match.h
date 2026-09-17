/*
 * netplay_match.h -- a match played through the netplay frame step and GGPO (SDL builds).
 * docs/NETPLAY.md, "Playing a match through GGPO".
 *
 * GGPO re-simulates frames from inside its own calls, so a netplay match runs each frame
 * as a step -- the players' inputs, GameLoop_Frame, the clock, the buffer swap and the
 * VBlank interrupt -- that GGPO can call as often as it needs, and shows only the frame it
 * ends on.
 *
 * For now the inputs come from a recording (--replay-test), which makes it a test: the
 * match played through the step and through GGPO must be the match the replay rig plays,
 * frame for frame.
 *
 *   --netplay-match=direct          the netplay step, without GGPO
 *   --netplay-match=synctest[:N]    GGPO's synctest session: every N frames (default 1, at
 *                                   most 8) it loads the state of N frames back, plays
 *                                   them again and compares their checksums
 *   --netplay-match=p2p             one of two peers, each a process with one player's
 *                                   recorded input: GGPO predicts the other player's and
 *                                   rolls back when the prediction was wrong. With
 *     --netplay-player=1|2          the local player
 *     --netplay-port=<port>         the local UDP port
 *     --netplay-peer=<IPv4>:<port>  the other peer
 *     --netplay-delay=<frames>      input delay (default 0)
 *     --netplay-stall=<ms>          pause now and then, up to this long, like a busy
 *                                   machine: the other peer plays ahead on predicted input
 *                                   and rolls back more (a test knob; default 0)
 *   --netplay-trace=<file>          every frame's state hashes as finally played, in the
 *                                   rollback rig's trace format (--rollback-trace)
 *   --netplay-timeout=<seconds>     give up after this long without progress (default 30)
 *
 * A peer plays frames ahead of its confirmed input, so its state at a frame boundary can
 * still change. It ends the match only at a frame nothing will roll back again, and its
 * trace holds every frame's last save. It plays in the rollback rig's environment
 * (--rollback=trace, and no other --rollback option).
 */
#ifndef NETPLAY_MATCH_H
#define NETPLAY_MATCH_H

#ifdef SWOS_SDL_BUILD

#include "swos_types.h"

/* A --netplay-* option other than --netplay-selftest: 1 = consumed, -1 = bad value. */
int32_t     NetplayMatch_ParseArg(const char *arg);
/* NULL, or what is wrong with the options given together. */
const char *NetplayMatch_ArgError(void);
int32_t     NetplayMatch_Enabled(void);
int32_t     NetplayMatch_IsP2P(void);

/* ReplayTest_Run, before kick-off: open the session. 0, or non-zero on failure. */
int32_t     NetplayMatch_Start(void);

/* Start a LIVE match against a peer found by the matchmaking menu (or by the
 * direct-IP one): the same p2p session the corpus tests play, with this
 * machine's pad as the local input instead of a recording.
 *
 * `delay` must be the delay BOTH peers agreed -- P2PLink_AgreedDelay(). Close
 * the p2p link first: GGPO opens its own socket on the same local port. */
int32_t     NetplayMatch_StartLive(int32_t localPlayer, const char *peerIp, uint16_t peerPort,
                                   uint16_t localPort, int32_t delay);

/* The 16-byte key the NEXT p2p session checksums its packets with, or NULL for none
 * (include/netplay.h, NetplayConfig.packetKey). Both peers must set the same one; the
 * lobby does, at kick-off. Kept until changed. */
void        NetplayMatch_SetPacketKey(const uint8_t *key);

/* GameLoop, in place of one pass of its loop: at most one frame, ended and shown. Returns a
 * GameFrameResult. */
int32_t     NetplayMatch_Frame(void);

/* A LIVE match against a real opponent (as opposed to a corpus recording played
 * through the same session). The pre-match prompts are skipped for one, the way
 * they are for the replay rig and for arcade mode. */
int32_t     NetplayMatch_IsLive(void);

/* Replace where a live match reads its LOCAL input, for testing. NULL restores
 * the pad. The returned value is MATCH_* control bits (include/match_step.h). */
void        NetplayMatch_SetInputSource(uint8_t (*fn)(void));

/* Called when a live session finishes with every frame confirmed -- the moment
 * the state is final. A headless test reports and exits from here rather than
 * returning into the post-match screens, which wait for input. */
void        NetplayMatch_SetFinishedCallback(void (*fn)(void));

/* The end of the run: report what the session did, write the trace, close the session.
 * The exit code: 0, or 2. */
int32_t     NetplayMatch_Finish(void);

#endif /* SWOS_SDL_BUILD */
#endif /* NETPLAY_MATCH_H */
