/*
 * platform_sdl.h -- SDL host services exposed to the ported Amiga code.
 *
 * Pure-C interface (no C++). Anything that used to talk to the Amiga
 * custom chips, CIA, DOS library, or AmigaOS goes through this header.
 */

#ifndef PLATFORM_SDL_H
#define PLATFORM_SDL_H

#include <SDL.h>

/* Native SWOS resolution. Matches display.h. */
#define PLATFORM_SCREEN_W 336
#define PLATFORM_SCREEN_H 272

/* ---- Lifecycle ----------------------------------------------------- */
int  Platform_Init(int win_scale);
void Platform_Shutdown(void);

/* Full-fat vblank: pump events, render the bitplanes, present, and
 * pace to the next 50 Hz boundary. Intended to run on the main thread
 * from game-loop waits. Returns non-zero if the user closed the window. */
int  Platform_VBlank(void);

/* Just pump SDL events, render bitplanes, and present. No pacing.
 * Safe to call from the SDL_AddTimer thread. */
void Platform_RenderFrame(void);

/* Force a quit (set by SDL_QUIT or escape). The Amiga code polls
 * `g_quitToDos` -- we set it from here. */
void Platform_RequestQuit(void);

/* ---- Hardware register stubs --------------------------------------- *
 * The Amiga code talks to chip registers through `volatile struct Custom *custom`.
 * We provide a real allocated struct so writes are no-ops but reads return
 * something sensible. The struct layout matches display.h.
 *
 * NOTE: display.h declares `extern volatile struct Custom *custom;` and the
 * struct itself, so we don't redeclare them here -- platform_amiga_stubs.c
 * provides the storage and pointer. */

/* ---- Misc helpers -------------------------------------------------- */
/* Returns the host's high-resolution monotonic millisecond counter. */
Uint64 Platform_NowMs(void);

#endif /* PLATFORM_SDL_H */
