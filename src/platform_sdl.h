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
/* `start_fullscreen` selects fullscreen-desktop mode at window creation —
 * the SDL renderer's logical size handles letterboxing automatically so
 * the original 336x272 frame stays pixel-perfect inside whatever
 * resolution the desktop is running at. The user can also toggle at
 * runtime via Alt+Enter (handled in this file). */
int  Platform_Init(int win_scale, int start_fullscreen);
void Platform_Shutdown(void);

/* Toggle / set fullscreen at runtime. Alt+Enter calls this internally;
 * exposed here in case a future settings menu needs a programmatic hook. */
void Platform_ToggleFullscreen(void);

/* Full-fat vblank: pump events, render the bitplanes, present, and
 * pace to the next 50 Hz boundary. Intended to run on the main thread
 * from game-loop waits. Returns non-zero if the user closed the window. */
int  Platform_VBlank(void);

/* Just pump SDL events, render bitplanes, and present. No pacing.
 * Safe to call from the SDL_AddTimer thread. */
void Platform_RenderFrame(void);

/* Run the Amiga Level-3 VBlank ISR body, throttled to ~50 Hz max and
 * mutex-guarded so concurrent calls from the timer thread and the main
 * thread serialise. Use this from main-thread spin loops on flags the
 * ISR sets (fadeState, currentGameTick, waitingInterrupt) when calling
 * _WaitForVSync isn't appropriate (e.g. you don't want to render a
 * frame). On WSL2 the SDL_Timer thread can be starved indefinitely by
 * a busy main thread, so the spin needs to drive the ISR itself. */
void _RunInt3HandlerThrottled(void);

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
