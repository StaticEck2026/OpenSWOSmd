/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef STATISTICS_H
#define STATISTICS_H

#include "game_types.h"
#include "menu_system.h"

/* Update + draw match statistics overlay */
void UpdateAndDrawStatistics(void);

/* Draw full match stats display */
void DrawStatistics(void);

/* Draw darkened grid background */
void DrawGrid(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/* Management Record — Chunk 137+138 */
uint8_t *GetCurrentSeasonPointer(void);
void ManagementRecordInit(void);
void ManagementArrowUpSelect(void);
void ManagementArrowDownSelect(void);
void Career_StorePromotionRelegationInSeasonInfo(void);
void Career_FillCurrentSeasonInfo(void);
void FillManagementRecordInfo(void);

/* Top Goal Scorers — Chunk 134 */
void TopGoalScorersMenu_0(void);
void StatsHighestScorerList(void);
void CompetitionTopGoalScorersInit(void);
void TopGoalScorersInit(void);
void TopGoalScorersBeforeDrawCommon(MenuEntry *entry);

#endif
