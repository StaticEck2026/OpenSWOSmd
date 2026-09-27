/*
 * match_report.h -- after an online match, one zip of both players' logs and ratings
 * (docs/NETPLAY_REPORT.md, include/report_proto.h). SDL builds only.
 *
 * When a match is over and HOW WAS THE MATCH? has been answered, each player's game
 * sends its SWOS.log, deflated, with the rating and comment, to the other player over
 * the lobby's link. Each then writes the same zip --
 *
 *     save/netplay/2026-09-27_211405_PREDICADOR_vs_ELSHEIMY.zip
 *         SWOS_player1.log     the host's SWOS.log
 *         SWOS_player2.log     the joiner's
 *         rating.txt           the match, both ratings and comments, both builds
 *
 * -- keeps the last REPORT_KEEP of them, and sends it to the matchmaking server, which
 * files it by match id. A player that leaves before the other has answered still gets
 * a zip, with what there is.
 *
 * SEND MATCH REPORTS (MULTIPLAYER SETTINGS, [netplay] reports) is the consent: off,
 * nothing leaves the machine -- no log to the other player, nothing to the server --
 * and the zip holds only this player's own log. On by default in tester builds only.
 * Where the upload goes: SWOS_REPORTS=off|server|host[:port] for a test -- which also
 * turns reports on (or off) whatever the setting -- else the server this build logs in to. The automated tests never upload unless SWOS_REPORTS
 * says where, and the web build never does.
 */
#ifndef MATCH_REPORT_H
#define MATCH_REPORT_H

#include <stdint.h>
#include "match_record.h"   /* MatchRecordInfo: who played what */

/* [netplay] reports, published by main.c and the settings row. */
void    MatchReport_SetEnabled(int32_t on);
int32_t MatchReport_Enabled(void);

/* A live match has kicked off. Finishes a previous report still waiting. */
void MatchReport_Begin(const MatchRecordInfo *info);

/* The match is over and this player has answered: rating 1..5, 0 for none; `comment`
 * and `result` (what the rating screen showed, "FULL TIME  2-1") may be empty. Takes
 * this player's log as it is now and queues it for the other player. */
void MatchReport_Submit(int32_t rating, const char *comment, const char *result);

/* Once a frame while the lobby's link is up: sends this player's half, takes the other
 * player's, and writes the zip when both are in -- or when the link is gone, or the
 * other player has not answered within a couple of minutes. */
void MatchReport_Poll(void);

/* Leaving the lobby, or the next match starting: write the zip with what there is. */
void MatchReport_Finish(void);

/* A report is waiting for the other player's half (or for ours to reach them). */
int32_t MatchReport_Pending(void);

/* Quitting: MatchReport_Finish, then wait up to `waitMs` for uploads still under way --
 * they run on threads of their own, which exit() would cut off. */
void MatchReport_Flush(uint32_t waitMs);

#endif /* MATCH_REPORT_H */
