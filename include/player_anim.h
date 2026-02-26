#ifndef PLAYER_ANIM_H
#define PLAYER_ANIM_H

#include "game_types.h"

void _MovePlayers(void);
void _SetPlayerAnimationTable(uint8_t *animTable, Sprite *sprite);
void _SetPlayerAnimationTableAndPictureIndex(uint8_t *animTable, Sprite *sprite);

#endif
