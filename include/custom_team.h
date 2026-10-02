/* custom_team.h -- MY CUSTOM TEAM: a team of the player's own for online play, made
 * from a team of the database with its names, faces, nationalities, shirt numbers, kits
 * and line-up changed -- and nothing else (src/custom_team.c, docs/CUSTOM_TEAMS.md). SDL only. */
#ifndef CUSTOM_TEAM_H
#define CUSTOM_TEAM_H

#include "game_types.h"

#define CUSTOM_TEAM_SLOTS 3

/* The team kept in a slot (0..2), built from the database and the slot's file.
 * 0 when the slot is empty or its file cannot be used. */
int32_t CustomTeam_Load(int32_t slot, TeamFileEx *out);
int32_t CustomTeam_Save(int32_t slot, const TeamFileEx *team);

/* Is this team a team of the database with only what a custom team may change
 * changed? Every player must be one of that team's players with his position, skills
 * and value as the database has them. For a team that arrives from
 * the other player: it touches no state of the game's (it reads the database into a
 * buffer of its own), so it is safe from the lobby's polling. */
int32_t CustomTeam_Allowed(const TeamFileEx *team);

/* MY CUSTOM TEAMS, from the matchmaking menu: the three slots; choosing one edits it
 * (an empty one first asks for a team of the database to start from). */
void CustomTeam_Manage(void);

/* The same three slots as a way to pick a team for a match. 1 = a custom team was
 * picked and is in *out; 0 = the player asked for the ordinary team list instead;
 * -1 = there are no custom teams, so nothing was shown. */
int32_t CustomTeam_Pick(TeamFileEx *out);

/* While the EDIT TEAM screen is editing a custom team (edit_tactics.c asks): IMPORT
 * takes the whole team, identity included, and stays available; UNDO is LINEUP. */
int32_t CustomTeam_Editing(void);
void    CustomTeam_Lineup(void);
const char *CustomTeam_LineupLabel(void);

/* --convert-teams=<in>,<out>: every .TEAM record in <in> converted to <out>/<name>.TEAM (JSON inside). */
int32_t CustomTeam_ConvertDir(const char *in, const char *out);

/* --custom-team-selftest: every database team through the rule and through the file. */
int32_t CustomTeam_SelfTest(void);

#endif /* CUSTOM_TEAM_H */
