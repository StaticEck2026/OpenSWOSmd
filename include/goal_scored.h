#ifndef GOAL_SCORED_H
#define GOAL_SCORED_H

#include "game_types.h"

/* ---- GoalScored ----
 * Updates goal counters and builds scorer list sprites.
 * scoringTeam: 1 or 2
 * scorerSprite: pointer to in-game Sprite struct of the scorer
 */
void _GoalScored(int16_t scoringTeam, Sprite *scorerSprite);

#endif
