/*
 * arcade.h — SWOS arcade / coin-op mode (SDL-only, PC extra).
 *
 * When _gArcadeMode is set ([arcade] enabled=true in SWOS.ini, or --arcade),
 * SWOS boots straight to the attract loop instead of the front-end menu: the
 * cabinet shows attract reels + flashing INSERT COIN, takes coins, plays short
 * first-goal-wins matches, and (F2) opens an operator service menu. The Amiga
 * game is the definitive product; this is a separate coin-op layer on top of the
 * same engine. Default 0 = normal game, nothing here reachable. See docs/ARCADE.md.
 */
#ifndef SWOS_ARCADE_H
#define SWOS_ARCADE_H

#include <stdint.h>

extern int _gArcadeMode;   /* 1 = arcade/coin-op mode (boot to attract). SDL-only */

/* Runs in place of FrontEndMainLoop when _gArcadeMode is set (called from mainC
 * after all assets are loaded). Never returns to the normal front-end menu. */
void ArcadeMainLoop(void);

/* Drain edge-triggered cabinet input collected since the last call. Each out
 * receives the number of events. MAME convention: key 5 = insert coin P1,
 * key 6 = insert coin P2, F2 = service button. Implemented in platform_sdl.c. */
void Platform_ArcadeInput(int *coin1, int *coin2, int *service);

/* Draw the attract overlay (full-screen SWOS logo, index-0 transparent, + the
 * flashing INSERT COIN / credit state) onto the current frontbuffer. Called once
 * per frame from the replay draw loop while in arcade mode so it layers over the
 * playing reel, and from the idle screen. No-op effect unless _gArcadeMode. */
void Arcade_DrawAttractOverlay(void);

/* Composite the attract overlay (built by Arcade_DrawAttractOverlay) onto the
 * final ARGB8888 frame using the MENU palette (index 0 transparent). Called by
 * the SDL present path after the frontbuffer->RGBA conversion so the logo + text
 * keep their menu colours over a reel that is using a different palette. */
void Arcade_CompositeOverlay(uint32_t *dst, int dst_pitch_pixels, int w, int h);

#endif /* SWOS_ARCADE_H */
