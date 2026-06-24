/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* ai.h — AI helper functions for SWOS C port */
#ifndef AI_H
#define AI_H

#include "game_types.h"

/* ---- AI helper functions (Chunk 52) ---- */

void AI_SetDirectionTowardOpponentsGoal(TeamGeneralInfo *team)
    __attribute__((used, externally_visible));

void AI_Kick(Sprite *player, TeamGeneralInfo *team)
    __attribute__((used, externally_visible));

int16_t AI_DecideWhetherToTriggerFire(Sprite *player, TeamGeneralInfo *team, int16_t dir)
    __attribute__((used, externally_visible));

Sprite *FindClosestPlayerToBallFacing(uint16_t fullDir, TeamGeneralInfo *team)
    __attribute__((used, externally_visible));

/* ---- AI main controller (Chunk 54) ---- */

void AI_SetControlsDirection(TeamGeneralInfo *team)
    __attribute__((used, externally_visible));

#endif /* AI_H */
