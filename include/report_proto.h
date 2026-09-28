/*
 * report_proto.h -- a netplay match report: both players' logs and ratings, in one zip.
 * docs/NETPLAY_REPORT.md has why; src/match_report.c is the game's half,
 * third_party/matchmaker/Server/ReportDesk.cpp the server's.
 *
 * Two messages, both big-endian (record_proto.h's Record_Put / Record_Get):
 *
 * REPORT_HALF -- one player's half, sent to the OTHER player over the lobby's link
 * (P2PLink channel 3) once the match is over and HOW WAS THE MATCH? answered:
 *   0  4  "SWMH"
 *   4  1  REPORT_PROTOCOL
 *   5  1  player (1 = the host, the top team)
 *   6  1  rating: 1..5 stars, 0 = none given
 *   7  1  flags: REPORT_HALF_LOG when the log is included; REPORT_HALF_OFF when that
 *         player has SEND MATCH REPORTS off, and the half says nothing but their name
 *   8  8  match id (Record_MatchId: both players compute the same one)
 *  16  4  the log's length
 *  20  4  its length deflated (raw deflate, miniz tdefl)
 *  24     strings, each a length byte then the bytes: name, build id, platform, the
 *         result as that player saw it, the comment; then the deflated log
 *         then, with REPORT_HALF_GHOST: 4 bytes the ghost windows' length, 4 bytes their
 *         length deflated, and them -- CSV from netplay_ghost.c (Ghost_Clips), each frame
 *         around a G-mark or a detected ghost as played and, where it differed, as drawn
 *
 * REPORT_UPLOAD -- one player's whole report, sent to the server on its TCP port
 * (the recorder's and the WebSocket gate's: the gate tells them apart by the magic):
 *   0  4  "SWMR"
 *   4  1  REPORT_PROTOCOL
 *   5  1  player
 *   6  1  rating
 *   7  1  0
 *   8  8  match id
 *  16  4  the zip's length, at most REPORT_ZIP_MAX
 *  20     strings: name, the other player's name, build id, comment; then the zip
 * The server answers with 5 bytes, "SWMR" and 0 for kept or 1 for refused, and closes.
 */
#ifndef REPORT_PROTO_H
#define REPORT_PROTO_H

#include <stdint.h>

#define REPORT_HALF_MAGIC    "SWMH"
#define REPORT_UPLOAD_MAGIC  "SWMR"
#define REPORT_PROTOCOL      1

#define REPORT_HALF_FIXED    24
#define REPORT_HALF_LOG      0x01
#define REPORT_HALF_OFF      0x02   /* that player has SEND MATCH REPORTS off: no more than this */
#define REPORT_HALF_GHOST    0x04   /* the ghost windows follow the log (netplay_ghost.h) */
#define REPORT_GHOST_MAX     (64u << 10)   /* their share of a half, deflated */

#define REPORT_UPLOAD_FIXED  20
#define REPORT_ZIP_MAX       (512u << 10)   /* two logs deflated fit many times over */

#endif /* REPORT_PROTO_H */
