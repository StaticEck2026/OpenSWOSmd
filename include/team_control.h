/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef TEAM_CONTROL_H
#define TEAM_CONTROL_H

#include "game_types.h"

/* Chunk 54 — Team Controls + Player Selection */

void UpdateControlledPlayer(TeamGeneralInfo *team);
void UpdatePlayerBeingPassedTo(TeamGeneralInfo *team);
void UpdateAndApplyTeamControls(void);

#endif /* TEAM_CONTROL_H */
