/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "camera.h"
#include "game_types.h"
#include "scroll_blit.h"

/* ---- External globals ---- */
extern FixedPoint cameraX;
extern FixedPoint cameraY;
extern uint16_t cameraXMod320;
extern uint16_t cameraYMod320;
extern uint16_t cameraCoordinatesValid;

/* ---- External C functions ---- */
extern void SetCameraCoordinates(int16_t targetX, int16_t targetY);

/* ================================================================
 * ScrollToCurrent  (SWOS2.s line 5625)
 *
 * Sets cameraCoordinatesValid = 1, then calls ScrollX and ScrollY
 * to blit pitch columns/rows matching the current camera position.
 * ================================================================ */
__attribute__((used, externally_visible))
void ScrollToCurrent(void)
{
    cameraCoordinatesValid = 1;
#ifdef SWOS_SDL_BUILD
    /* SDL: incremental scroll blits are no-ops (see scroll_blit.c).
     * Instead, redraw the whole visible pitch from the chunky tile
     * bank each call — matches swos-port-c's whole-screen chunky
     * pitch redraw shape. */
    extern void DrawChunkyPitch(void);
    DrawChunkyPitch();
#else
    ScrollXUpdateColumnsAndBlit();
    ScrollYUpdateRowsAndBlit();
#endif
}

/* ================================================================
 * MoveView  (SWOS2.s line 5576)
 *
 * Iteratively moves cameraX.s.i toward newScreenX (one pixel per step,
 * with wrap), calling ScrollToCurrent each step. Once X matches,
 * does the same for cameraY.s.i toward newScreenY.
 *
 * 68k semantics: each increment/decrement adjusts both cameraX.s.i and
 * cameraXMod320 (wrapping at 352), and cameraY.s.i/cameraYMod320
 * (wrapping at 272).
 * ================================================================ */
__attribute__((used, externally_visible))
void MoveView(int16_t newScreenX, int16_t newScreenY)
{
    /* ---- Phase 1: move X ---- */
    /* Original asm uses do-while: ScrollToCurrent fires before the exit
     * check, so it always runs at least once and also runs at the final
     * target position.  This ensures the last tile boundary crossing is
     * blitted — critical for replay camera panning. */
    for (;;) {
        ScrollToCurrent();
        if (cameraX.s.i == newScreenX)
            break;
        if (cameraX.s.i < newScreenX) {
            /* Move right */
            cameraX.s.i += 1;
            cameraXMod320 += 1;
            if (cameraXMod320 == 352)
                cameraXMod320 = 0;
        } else {
            /* Move left */
            cameraX.s.i -= 1;
            cameraXMod320 -= 1;
            if ((int16_t)cameraXMod320 < 0)
                cameraXMod320 = 351;
        }
    }

    /* ---- Phase 2: move Y ---- */
    for (;;) {
        ScrollToCurrent();
        if (cameraY.s.i == newScreenY)
            break;
        if (cameraY.s.i < newScreenY) {
            /* Move down */
            cameraY.s.i += 1;
            cameraYMod320 += 1;
            if (cameraYMod320 == 272)
                cameraYMod320 = 0;
        } else {
            /* Move up */
            cameraY.s.i -= 1;
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
 * and (cameraX.s.i,0), then calls MoveView to reach the actual target.
 *
 * Register semantics from asm:
 *   D1 = target X (passed by caller)
 *   D2 = target Y (saved, then restored for MoveView)
 * ================================================================ */
__attribute__((used, externally_visible))
void InitCameraMoveView(int16_t targetX, int16_t targetY)
{
    /* First pass: set camera to (targetX, 272), scroll */
    SetCameraCoordinates(targetX, 272);
    ScrollToCurrent();

    /* Second pass: set camera to (cameraX.s.i, 0), scroll */
    SetCameraCoordinates(cameraX.s.i, 0);
    ScrollToCurrent();

    /* Final: iteratively move to (cameraX.s.i, targetY) */
    MoveView(cameraX.s.i, targetY);
}
