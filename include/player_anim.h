/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef PLAYER_ANIM_H
#define PLAYER_ANIM_H

#include "game_types.h"

void _MovePlayers(void);
void _SetPlayerAnimationTable(uint8_t *animTable, Sprite *sprite);
void _SetPlayerAnimationTableAndPictureIndex(uint8_t *animTable, Sprite *sprite);

#endif
