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

/* Top Goal Scorers — Chunk 134 */
void _TopGoalScorersMenu_0(void);
void _StatsHighestScorerList(void);
void _CompetitionTopGoalScorersInit(void);
void _TopGoalScorersInit(void);
void _TopGoalScorersBeforeDrawCommon(MenuEntry *entry);

#endif
