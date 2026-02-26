#ifndef TEAM_CONTROL_H
#define TEAM_CONTROL_H

#include "game_types.h"

/* Chunk 54 — Team Controls + Player Selection */

void _UpdateControlledPlayer(TeamGeneralInfo *team);
void _UpdatePlayerBeingPassedTo(TeamGeneralInfo *team);
void _UpdateAndApplyTeamControls(void);

#endif /* TEAM_CONTROL_H */
