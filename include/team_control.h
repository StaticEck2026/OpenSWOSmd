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

/* 1 = reproduce the ORIGINAL Amiga fire-hold lockout (an OPPONENT's tackle
 * kills your held fire button's shot charge for good); 0 = the fix, the default.
 * This is a bug in the original game, not a port inaccuracy — the long comment
 * over the definition in team_control.c has the asm references, the corpus
 * measurements and the exact scope of the change. */
extern int _gOriginalFireHoldLockout;

#endif /* TEAM_CONTROL_H */
