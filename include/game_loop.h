/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include "game_types.h"

/* Game loop entry points */
void ViewMatchSequence(void);
void ViewHighlights(void);
void StartMatchSequence(TeamGame *topTeam, TeamGame *bottomTeam);
void GameLoop(void);
void KeepStatisticsOnScreen(void);
void ViewHighLightsMain(TeamGame *leftTeam, TeamGame *rightTeam);
void PlayInstantReplayLoop(void);
void PlayHighlightsLoop(void);

/* Key dispatch */
void MainKeysCheck(uint16_t key);

/* Fake goal assignment on ESC */
void AssignFakeGoalsToScorers(uint16_t teamId, int16_t numGoals);

#endif
