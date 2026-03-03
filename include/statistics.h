#ifndef STATISTICS_H
#define STATISTICS_H

#include "game_types.h"
#include "menu_system.h"

/* Update + draw match statistics overlay */
void _UpdateAndDrawStatistics(void);

/* Draw full match stats display */
void _DrawStatistics(void);

/* Draw darkened grid background */
void _DrawGrid(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/* Management Record — Chunk 137+138 */
uint8_t *_GetCurrentSeasonPointer(void);
void _ManagementRecordInit(void);
void _ManagementArrowUpSelect(void);
void _ManagementArrowDownSelect(void);
void _Career_StorePromotionRelegationInSeasonInfo(void);
void _FillManagementRecordInfo(void);

/* Top Goal Scorers — Chunk 134 */
void _TopGoalScorersMenu_0(void);
void _StatsHighestScorerList(void);
void _CompetitionTopGoalScorersInit(void);
void _TopGoalScorersInit(void);
void _TopGoalScorersBeforeDrawCommon(MenuEntry *entry);

#endif
