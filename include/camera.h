#ifndef CAMERA_H
#define CAMERA_H

#include "game_types.h"

/* Camera movement */
void _InitCameraMoveView(int16_t targetX, int16_t targetY);
void _MoveView(int16_t newScreenX, int16_t newScreenY);
void _ScrollToCurrent(void);
void _MoveCamera(void);

#endif
