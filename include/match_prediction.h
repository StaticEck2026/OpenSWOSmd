/*
 * match_prediction.h -- after an online match, each player's half of what the players SAW
 * (PRED and SEEN, include/srp_writer.h) crosses the lobby's link, so both games' .SRP
 * files hold both halves -- the same file the recorder keeps from the two streams.
 * SDL builds with netplay only. docs/REPLAY_FORMAT_V2.md §4, "Prediction".
 *
 * The message, on the link's channel 4 (P2P_NUM_CHANNELS), big-endian:
 *   0  4  "SWPD"
 *   4  1  version (2)
 *   5  1  whose half it is: player 1 or 2
 *   6  1  the input delay the READ bytes were made under
 *   7  1  1: this player had the prediction balance on
 *   8  4  frames (the PRED bytes)
 *  12  4  SEEN bytes
 *  16  4  frames of READ (SRP_READ_BYTES each; 0: none)
 *  20     the PRED bytes, the READ bytes, then the SEEN records, raw-deflated (the vendored miniz)
 * A half too big for one message (the link takes 128 KB) goes without its SEEN.
 */
#ifndef MATCH_PREDICTION_H
#define MATCH_PREDICTION_H

#include <stdint.h>

#include "srp_writer.h"     /* SrpPrediction */

/* A live match is starting: forget the last one's halves. */
void MatchPrediction_Begin(void);

/* This player's half, as its .SRP was just written with it: sent to the other player.
 * Copied; the caller keeps its own. */
void MatchPrediction_Offer(int32_t player, const SrpPrediction *own);

/* The other player's half if it has arrived, else NULL. Valid until the next Begin. */
const SrpPrediction *MatchPrediction_Peer(void);

/* The lobby's loop, as it runs MatchReport_Poll: sends, receives, and adds a half that
 * arrived to the files (ReplayRec_AddPeerPrediction). */
void MatchPrediction_Poll(void);

#endif /* MATCH_PREDICTION_H */
