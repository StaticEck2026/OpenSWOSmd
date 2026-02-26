#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include "game_types.h"

/* Game loop entry points */
void _ViewMatchSequence(void);
void _ViewHighlights(void);
void _StartMatchSequence(TeamGame *topTeam, TeamGame *bottomTeam);
void _GameLoop(void);
void _KeepStatisticsOnScreen(void);
void _ViewHighLightsMain(TeamGame *leftTeam, TeamGame *rightTeam);
void _PlayInstantReplayLoop(void);
void _PlayHighlightsLoop(void);

/* Key dispatch */
void _MainKeysCheck(uint16_t key);

/* Fake goal assignment on ESC */
void _AssignFakeGoalsToScorers(uint16_t teamId, int16_t numGoals);

#endif
