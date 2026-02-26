#ifndef STATISTICS_H
#define STATISTICS_H

#include "game_types.h"

/* Update + draw match statistics overlay */
void _UpdateAndDrawStatistics(void);

/* Draw full match stats display */
void _DrawStatistics(void);

/* Draw darkened grid background */
void _DrawGrid(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

#endif
