/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* chairman_memo.h — Chairman memos and club business menu functions. */

#ifndef CHAIRMAN_MEMO_H
#define CHAIRMAN_MEMO_H

#include "game_types.h"

/* ---- Chairman memo functions ---- */
void ShowChairmanMessage(void);
void ChairmanMemosInit(void);
void ChairmanMemosOnReturn(void);
void ChairmanMemosContinue(void);

/* ---- Club business ---- */
void ShowClubBussinesMenu(void);

#endif /* CHAIRMAN_MEMO_H */
