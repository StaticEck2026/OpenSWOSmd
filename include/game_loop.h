/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include "game_types.h"

/* One pass of the match loop (GameLoop_Frame). */
typedef enum {
    GAME_FRAME_DONE,        /* the frame ran to its end */
    GAME_FRAME_HELD,        /* the statistics overlay held it: one pass of its loop ran,
                             * the rest of the frame did not (_gFrameStepInput only) */
    GAME_FRAME_DETOUR,      /* it played the highlights or an instant replay instead */
    GAME_FRAME_MATCH_OVER,  /* the match is over: it stopped before the simulation */
    GAME_FRAME_QUIT         /* quit to DOS */
} GameFrameResult;

/* Game loop entry points */
void ViewMatchSequence(void);
void ViewHighlights(void);
void StartMatchSequence(TeamGame *topTeam, TeamGame *bottomTeam);
void GameLoop(void);
/* One frame of the match, and the vblank wait that ends it (present 0: not shown). */
/* Set while the players' input reaches the match one frame at a time from outside the
 * loop -- the replay rig, a netplay match. A loop inside a frame that waits for input then
 * makes one pass per frame instead of spinning: the input that would end it can only
 * arrive with a later frame. Never set on the Amiga or in a normal game. */
extern int32_t _gFrameStepInput;
GameFrameResult GameLoop_Frame(void);
void GameLoop_EndFrame(int32_t present);
void KeepStatisticsOnScreen(void);
void ViewHighLightsMain(TeamGame *leftTeam, TeamGame *rightTeam);
void PlayInstantReplayLoop(void);
void PlayHighlightsLoop(void);

/* Key dispatch */
void MainKeysCheck(uint16_t key);

/* Fake goal assignment on ESC */
void AssignFakeGoalsToScorers(uint16_t teamId, int16_t numGoals);

#endif
