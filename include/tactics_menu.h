/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* tactics_menu.h — Tactics selection menu callback functions. */

#ifndef TACTICS_MENU_H
#define TACTICS_MENU_H

#include "game_types.h"

/* ---- Tactics selection callbacks ---- */
void _User_A_Tactics(void);
void _User_B_Tactics(void);
void _User_C_Tactics(void);
void _User_D_Tactics(void);
void _User_E_Tactics(void);
void _User_F_Tactics(void);
void _ExitChooseTacticsMenu(void);

#endif /* TACTICS_MENU_H */
