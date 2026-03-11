/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "camera.h"
#include "scroll_blit.h"

/* ---- External globals ---- */
extern int16_t  cameraX;
extern int16_t  cameraY;
extern uint16_t cameraXMod320;
extern uint16_t cameraYMod320;
extern uint16_t cameraCoordinatesValid;

/* ---- External C functions ---- */
extern void _SetCameraCoordinates(int16_t targetX, int16_t targetY);

/* ================================================================
 * ScrollToCurrent  (SWOS2.s line 5625)
 *
 * Sets cameraCoordinatesValid = 1, then calls ScrollX and ScrollY
 * to blit pitch columns/rows matching the current camera position.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ScrollToCurrent(void)
{
    cameraCoordinatesValid = 1;
    _ScrollXUpdateColumnsAndBlit();
    _ScrollYUpdateRowsAndBlit();
}

/* ================================================================
 * MoveView  (SWOS2.s line 5576)
 *
 * Iteratively moves cameraX toward newScreenX (one pixel per step,
 * with wrap), calling ScrollToCurrent each step. Once X matches,
 * does the same for cameraY toward newScreenY.
 *
 * 68k semantics: each increment/decrement adjusts both cameraX and
 * cameraXMod320 (wrapping at 352), and cameraY/cameraYMod320
 * (wrapping at 272).
 * ================================================================ */
__attribute__((used, externally_visible))
void _MoveView(int16_t newScreenX, int16_t newScreenY)
{
    /* ---- Phase 1: move X ---- */
    while (cameraX != newScreenX) {
        _ScrollToCurrent();
        if (cameraX < newScreenX) {
            /* Move right */
            cameraX += 1;
            cameraXMod320 += 1;
            if (cameraXMod320 == 352)
                cameraXMod320 = 0;
        } else {
            /* Move left */
            cameraX -= 1;
            cameraXMod320 -= 1;
            if ((int16_t)cameraXMod320 < 0)
                cameraXMod320 = 351;
        }
    }

    /* ---- Phase 2: move Y ---- */
    while (cameraY != newScreenY) {
        _ScrollToCurrent();
        if (cameraY < newScreenY) {
            /* Move down */
            cameraY += 1;
            cameraYMod320 += 1;
            if (cameraYMod320 == 272)
                cameraYMod320 = 0;
        } else {
            /* Move up */
            cameraY -= 1;
            cameraYMod320 -= 1;
            if ((int16_t)cameraYMod320 < 0)
                cameraYMod320 = 271;
        }
    }
}

/* ================================================================
 * InitCameraMoveView  (SWOS2.s line 5557)
 *
 * Initializes camera by scrolling to two known positions (targetX,272)
 * and (cameraX,0), then calls MoveView to reach the actual target.
 *
 * Register semantics from asm:
 *   D1 = target X (passed by caller)
 *   D2 = target Y (saved, then restored for MoveView)
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitCameraMoveView(int16_t targetX, int16_t targetY)
{
    /* First pass: set camera to (targetX, 272), scroll */
    _SetCameraCoordinates(targetX, 272);
    _ScrollToCurrent();

    /* Second pass: set camera to (cameraX, 0), scroll */
    _SetCameraCoordinates(cameraX, 0);
    _ScrollToCurrent();

    /* Final: iteratively move to (cameraX, targetY) */
    _MoveView(cameraX, targetY);
}
