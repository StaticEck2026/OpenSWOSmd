/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef ADVERTS_H
#define ADVERTS_H

#include "game_types.h"

/* Advertisement system */
void CopyAdvertisementsData(void);
void InitAdvertisements(void);
void SetAdvertisementsVisible(void);
void SetAdvertisementsInvisible(void);
void ManageAdvertisements(void);

/* In-game display sprites */
void UpdateCornerFlagSprite(void);
void SpinBig_S_Sprite(void);

#endif
