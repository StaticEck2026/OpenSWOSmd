/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef CAMERA_H
#define CAMERA_H

#include "game_types.h"

/* Camera movement */
void InitCameraMoveView(int16_t targetX, int16_t targetY);
void MoveView(int16_t newScreenX, int16_t newScreenY);
void ScrollToCurrent(void);
void MoveCamera(void);

#endif
