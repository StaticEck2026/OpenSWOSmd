/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef PLAYER_SPRITES_H
#define PLAYER_SPRITES_H

#include "game_types.h"

/* Face/color helpers */
uint8_t *ExtractSurname(uint8_t *name, uint16_t maxLenAndFlags);
void CountFaces(TeamGame *team);
void ConvertColorTableToSkinColor(uint16_t color, uint16_t *destTable);
void DetermineGoalieFaces(TeamGame *team, uint8_t *gkFaceToIndex, uint8_t *gkFaceIndices);
void InitializePlayerSpriteFrameIndices(void);

/* Player name sprite construction */
void MakeSpritesFromPlayerNames(TeamGame *leftTeam, TeamGame *rightTeam);

#endif
