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

/* A session is starting: forget the last one. `player` names the report (1 or 2). */
void Ghost_Begin(int32_t player);

/* Frame `frame` has just been played: `first` for the play this peer drew, 0 for a
 * re-simulation. Reads the players and the ball; writes nothing the match reads. */
void Ghost_Played(int32_t frame, int32_t first);

/* The player pressed "that was a ghost" (G, or the pad's BACK) at `frame`: logged now,
 * and the report looks at the seconds before it. */
void Ghost_Mark(int32_t frame);

/* Every frame up to `lastFrame` is final: compare, and print the report -- the drawn-vs-
 * final summary, the rollback ghosts, the walk-throughs in the final game (an opponent on
 * top of the man on the ball who wins nothing), and for each mark what led up to it. */
void Ghost_Report(int32_t lastFrame);

/* The windows Ghost_Report kept, as CSV text: CLIP_S seconds either side of every mark
 * and of the longest events, each frame as played and, where it differed, as drawn. NULL
 * when there are none. Valid until the next Ghost_Begin -- the match report
 * (src/match_report.c) puts both players' in its zip. */
const char *Ghost_Clips(uint32_t *len);

#endif /* NETPLAY_GHOST_H */
