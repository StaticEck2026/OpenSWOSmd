/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef BALL_PHYSICS_H
#define BALL_PHYSICS_H

#include "game_types.h"

/* ---- Ball physics + helpers (Chunk 43) ---- */

void _UpdateBall(void);
void _CalculateNextBallPosition(void);
void _DoGoalkeeperSprites(void);
void _InjuryUpdateTickGuard(void);
void _ResetBothTeamSpinTimers(void);
void _UpdateControllingPlayer(Sprite *playerSprite);
void _UpdateBallWithControllingGoalkeeper(Sprite *playerSprite);
void _UpdatePlayerWithBall(Sprite *playerSprite);
void _SetThrowInPlayerDestinationCoordinates(Sprite *playerSprite);
uint8_t *_GetPlayerPointerFromShirtNumber(Sprite *playerSprite, TeamGeneralInfo *teamData);

/* ---- Chunk 53 additions ---- */
void _UpdateBallVariables(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team);
void _CalculateBallNextGroundXYPositions(Sprite *ball);
void _ApplyBallAfterTouch(TeamGeneralInfo *team);

#endif /* BALL_PHYSICS_H */
