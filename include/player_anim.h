/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef PLAYER_ANIM_H
#define PLAYER_ANIM_H

#include "game_types.h"

void MovePlayers(void);
void SetPlayerAnimationTable(AnimTable *animTable, Sprite *sprite);
void SetPlayerAnimationTableAndPictureIndex(AnimTable *animTable, Sprite *sprite);

#endif
