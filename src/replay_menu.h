/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef REPLAY_MENU_H
#define REPLAY_MENU_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef int int32_t;

void _DrawMenuBack150x20(uint16_t color, uint16_t x, uint16_t y);
void _DrawReplayContinueShiningFrame(int16_t x, int16_t y);
void _DrawReplayOrExitMenu(void);
void _ReplayExitMenuAfterFriendly(void);

#endif
