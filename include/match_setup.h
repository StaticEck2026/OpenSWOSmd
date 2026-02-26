#ifndef MATCH_SETUP_H
#define MATCH_SETUP_H

#include "game_types.h"

/* Ball */
void _SetBallPosition(int16_t ballX, int16_t ballY);

/* Player state resets */
void _ResetBothTeamsPlayerPassingKicking(void);
void _StopAllPlayers(void);

/* Referee */
void _RemoveReferee(void);

/* Animated patterns */
void _InitAnimatedPatterns(void);

/* Match state transitions */
void _InitPlayersBeforeEnteringPitch(void);
void _StartingMatch(void);
void _SetCameraMovingToShowerState(void);
void _EndFirstHalf(void);
void _FirstHalfJustEnded(void);
void _EndOfGame(void);
void _PlayersLeavingPitch(void);
void _GoToHalftime(void);
void _GameOver(void);
void _StartFirstExtraTime(void);
void _EndFirstExtraTime(void);
void _PrepareForInitialKick(void);
void _StartPenalties(void);

/* Game restore */
void _InitGameRestoreTeams(void);

#endif
