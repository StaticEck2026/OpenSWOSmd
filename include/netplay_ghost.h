/*
 * netplay_ghost.h -- measure "ghosting": what a netplay peer DREW against the match as
 * finally played. SDL builds, netplay only.
 *
 * Under rollback a peer never waits for the other player's input: when it is late it
 * guesses (GGPO repeats the last one), simulates and draws that frame, and quietly plays
 * it again when the real input arrives. What was drawn and what happened differ, and
 * both peers still end on the same state -- so every determinism test passes while the
 * players watch opponents run through each other. Players overlap in SWOS all the time
 * (there are no bodies, only the ball); a GHOST is an overlap near the ball that was
 * drawn and never happened, or one that happened and was never drawn.
 *
 * Each frame is recorded twice: at its first play (what this peer drew) and at its last
 * (what GGPO finally kept, every frame being confirmed by the match's end). The report
 * compares the two, frame by frame.
 *
 *   SWOS_GHOST_TRACE=<file>   also write every frame, drawn and final, as CSV
 */
#ifndef NETPLAY_GHOST_H
#define NETPLAY_GHOST_H

#include <stdint.h>

#include "srp_writer.h"     /* SrpPrediction */

/* A session is starting: forget the last one. `player` names the report (1 or 2). */
void Ghost_Begin(int32_t player);

/* Frame `frame` has just been played: `first` for the play this peer drew, 0 for a
 * re-simulation. Reads the players and the ball; writes nothing the match reads. */
void Ghost_Played(int32_t frame, int32_t first);

/* Every frame up to `lastFrame` is final: compare, and print the report -- the drawn-vs-
 * final summary, the rollback ghosts, the walk-throughs in the final game (an opponent on
 * top of the man on the ball who wins nothing). */
void Ghost_Report(int32_t lastFrame);

/* The windows Ghost_Report kept, as CSV text: CLIP_S seconds either side of the longest
 * events, each frame as played and, where it differed, as drawn. NULL
 * when there are none. Valid until the next Ghost_Begin -- the match report
 * (src/match_report.c) puts both players' in its zip. */
const char *Ghost_Clips(uint32_t *len);

/* ---- what a .SRP keeps of it (PRED and SEEN, include/srp_writer.h) ---- */

/* Frame `frame` is being played for the first time on `lag` guessed inputs of the other
 * player's (0: none), and `stalled` if GGPO held this peer at its prediction limit
 * first. A re-simulation later marks it SRP_PRED_REPLAYED. */
void Ghost_Lag(int32_t frame, int32_t lag, int32_t stalled);

/* The player this machine is (Ghost_Begin's), 0 before any match. */
int32_t Ghost_Player(void);

/* A frame's PRED byte, SRP_PRED_UNKNOWN for one never played. */
uint8_t Ghost_PredByte(uint32_t frame);

/* The frame's SEEN record into `out` (room for SRP_SEEN_REC_HEAD + SRP_SEEN_OBJECTS *
 * SRP_SEEN_OBJ_BYTES): its length, 0 when it was drawn as finally played. Final once the
 * frame is confirmed. */
uint32_t Ghost_SeenRecord(uint32_t frame, uint8_t *out);

/* This player's half for frames 0..frames-1. 0, or 1 out of memory. The buffers stay
 * valid until the next call or Ghost_Begin. */

int32_t Ghost_Prediction(uint32_t frames, SrpPrediction *out);

#endif /* NETPLAY_GHOST_H */
