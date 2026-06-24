/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef BALL_PHYSICS_H
#define BALL_PHYSICS_H

#include "game_types.h"

/* ---- Ball physics + helpers (Chunk 43) ---- */

void UpdateBall(void);
void CalculateNextBallPosition(void);
void DoGoalkeeperSprites(void);
void InjuryUpdateTickGuard(void);
void ResetBothTeamSpinTimers(void);
void UpdateControllingPlayer(Sprite *playerSprite);
void UpdateBallWithControllingGoalkeeper(Sprite *playerSprite);
void UpdatePlayerWithBall(Sprite *playerSprite);
void SetThrowInPlayerDestinationCoordinates(Sprite *playerSprite);
PlayerInfo *GetPlayerPointerFromShirtNumber(Sprite *playerSprite, TeamGeneralInfo *teamData);

/* ---- Chunk 53 additions ---- */
void UpdateBallVariables(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team);
void CalculateBallNextGroundXYPositions(Sprite *ball);
void ApplyBallAfterTouch(TeamGeneralInfo *team);

#endif /* BALL_PHYSICS_H */
