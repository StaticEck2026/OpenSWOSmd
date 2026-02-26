#ifndef ADVERTS_H
#define ADVERTS_H

#include "game_types.h"

/* Advertisement system */
void _CopyAdvertisementsData(void);
void _InitAdvertisements(void);
void _SetAdvertisementsVisible(void);
void _SetAdvertisementsInvisible(void);
void _ManageAdvertisements(void);

/* In-game display sprites */
void _UpdateCornerFlagSprite(void);
void _SpinBig_S_Sprite(void);

#endif
