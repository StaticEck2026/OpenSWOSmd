/* team_db.h -- the team database as CANONICAL data: every team and player by its id,
 * and what each id stands for (src/team_db.c, docs/TEAM_IDS.md). SDL only.
 *
 * A copy of the database of its own, read once per country: the game's team buffer
 * belongs to whatever menu the player is in, and this is also asked from the lobby's
 * polling. */
#ifndef TEAM_DB_H
#define TEAM_DB_H

#include "game_types.h"

#define TEAM_UUID_TEXT      37      /* 8-4-4-4-12 and its NUL */
#define TEAM_CHECKSUM_TEXT  17      /* 16 hex digits and its NUL */
#define TEAM_PLAYERS        16

/* The ids a database file states for one team ("" where it states none). */
typedef struct TeamDbIds {
    char uuid[TEAM_UUID_TEXT];
    char checksum[TEAM_CHECKSUM_TEXT];
    char player[TEAM_PLAYERS][TEAM_UUID_TEXT];
} TeamDbIds;

typedef struct TeamDbEntry {
    TeamFileEx team;                /* as the database has it */
    TeamDbIds  ids;                 /* stated by the database, else derived (96/97) */
    char       computed[TEAM_CHECKSUM_TEXT];    /* the checksum of `team` as it stands */
    int32_t    canonical;           /* the stated checksum is the computed one */
} TeamDbEntry;

/* The teams filed under a country's number (the number of its TEAM.nnn), in the file's
 * order; NULL past the last, or where there is no such file. */
const TeamDbEntry *TeamDb_Entry(int32_t country, int32_t index);
/* The team with this number, in the file of its country. */
const TeamDbEntry *TeamDb_Team(int32_t country, int32_t ordinal);
/* The team with this id, wherever it is. */
const TeamDbEntry *TeamDb_ByUuid(const char *uuid);

/* What a team's id stands for: 16 hex digits of the SHA-1 of its sixteen players'
 * position, seven skills and value -- everything a match plays it by, nothing else. */
void TeamDb_Checksum(const TeamFileEx *team, char out[TEAM_CHECKSUM_TEXT]);

/* The ids of the original 96/97 database, which are derived rather than stored:
 * uuid5 of "96-97/team/<country>/<ordinal>" and ".../player/<index>" in the project's
 * namespace (scripts/assign_team_uuids.py computes the same). */
void TeamDb_DerivedTeamUuid(int32_t country, int32_t ordinal, char out[TEAM_UUID_TEXT]);
void TeamDb_DerivedPlayerUuid(int32_t country, int32_t ordinal, int32_t index, char out[TEAM_UUID_TEXT]);

/* Every team of the database: its stated ids are the derived ones, its stated checksum
 * the computed one, and no id is used twice. Prints one line; 1 = all of that holds. */
int32_t TeamDb_SelfTest(void);

#endif /* TEAM_DB_H */
