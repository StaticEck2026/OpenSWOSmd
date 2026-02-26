#ifndef BALL_POSSESSION_H
#define BALL_POSSESSION_H

#include "game_types.h"

/* ---- Ball Possession + Pass/Kick (Chunk 50) ---- */

void _DoPass(Sprite *player, TeamGeneralInfo *team);
void _PlayerKickingBall(Sprite *player, TeamGeneralInfo *team);
void _CalculateIfPlayerWinsBall(uint16_t dir, Sprite *player, TeamGeneralInfo *team);
void _UpdatePlayerSpeed(Sprite *player, TeamGeneralInfo *team);
void _UpdatePlayerShotChanceTable(Sprite *player, TeamGeneralInfo *team);
int16_t *_GetBallDestCoordinatesTable(void);
Sprite *_GetClosestNonControlledPlayerInDirection(uint16_t direction, TeamGeneralInfo *team);

#endif /* BALL_POSSESSION_H */
