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
/* Renderer selection.
 *
 * Software (PLATFORM_RENDERER_SOFTWARE):
 *   SDL_RENDERER_SOFTWARE. The SDL_AddTimer worker thread is allowed to
 *   call Platform_RenderFrame as a fallback during loader-intro spins
 *   that don't go through _WaitForVSync. Cross-thread render is safe
 *   because software renderers aren't context-bound. PRESENTVSYNC has
 *   no effect on software renderers — frames are not synced to monitor
 *   refresh, so 50 Hz monitor users see a slow drift / beat pattern.
 *
 * Accelerated (PLATFORM_RENDERER_ACCEL):
 *   SDL_RENDERER_ACCELERATED, optionally with SDL_RENDERER_PRESENTVSYNC.
 *   Hardware contexts (D3D / OpenGL / Metal) are bound to the creating
 *   thread, so the cross-thread fallback render is suppressed; only
 *   Platform_VBlank from the main thread issues GL calls. With vsync
 *   the monitor's vblank gates SDL_RenderPresent, giving free perfect
 *   pacing on a 50 Hz display. The 50 Hz manual SDL_Delay loop in
 *   Platform_VBlank stays as a backstop so 60+ Hz monitors still gate
 *   the game at 50 ticks/sec.
 */
typedef enum {
    PLATFORM_RENDERER_SOFTWARE = 0,
    PLATFORM_RENDERER_ACCEL    = 1,
} PlatformRendererMode;

/* Fullscreen mode selector.
 *
 * NONE:      Windowed at win_scale (Alt+Enter toggles to DESKTOP at runtime).
 * DESKTOP:   SDL_WINDOW_FULLSCREEN_DESKTOP — borderless window sized to the
 *            current desktop, letterboxed via SDL_RenderSetLogicalSize. No
 *            display-mode change. Fast to toggle, but on Windows XP the
 *            taskbar can occasionally show through above this borderless
 *            window — use EXCLUSIVE if you need the screen completely
 *            covered there.
 * EXCLUSIVE: SDL_WINDOW_FULLSCREEN — true fullscreen with display-mode
 *            change. Always covers the taskbar. Slower toggle, may flicker
 *            on dual-monitor setups, and the desktop resolution is
 *            restored on exit. */
typedef enum {
    PLATFORM_FULLSCREEN_NONE      = 0,
    PLATFORM_FULLSCREEN_DESKTOP   = 1,
    PLATFORM_FULLSCREEN_EXCLUSIVE = 2,
} PlatformFullscreenMode;

typedef struct {
    int                    win_scale;        /* 1, 2, 3, 4 */
    PlatformFullscreenMode fullscreen;       /* none / desktop / exclusive */
    PlatformRendererMode   renderer;         /* software / accelerated */
    int                    vsync;            /* 0 = off (default), 1 = on (accel only) */
    int                    widescreen;       /* 1 = wider 16:9 pitch view (default on). PC-only */
    int                    audio_mono;       /* 1 = down-mix to mono (default), 0 = Amiga panned */
    int                    dos_graphics;     /* 1 = render DOS-version graphics. PC-only */
    int                    dos_gameplay;     /* 1 = DOS gameplay tweaks (framerate etc.). PC-only; reserved */
    char                   dos_dir[256];     /* DOS asset dir (default "grafs_dos"). PC-only */
    char                   language[16];     /* UI language code, e.g. "en" — selects data_json/translations/<lang>.json */
    char                   currency[8];      /* "pound" (default) or "dollar" — money symbol */
} PlatformInitOptions;

int  Platform_Init(const PlatformInitOptions *opts);
void Platform_Shutdown(void);

/* Toggle / set fullscreen at runtime. Alt+Enter calls this internally;
 * exposed here in case a future settings menu needs a programmatic hook.
 * Toggles between windowed and the fullscreen flavour the user launched
 * with (--fullscreen=desktop or =exclusive). */
void Platform_ToggleFullscreen(void);

/* Set the windowed-mode integer scale factor (1..4) at runtime. Called
 * from the +/- key handlers. No-op while fullscreen. */
void Platform_SetWindowScale(int scale);

/* Snapshot the current windowed scale (1..4) and current fullscreen
 * mode. Used by the settings-save path so SWOS.ini round-trips reflect
 * runtime adjustments (Alt+Enter, +/-) — not just whatever was on the
 * command line at launch. */
int                    Platform_GetWindowScale(void);
PlatformFullscreenMode Platform_GetFullscreenMode(void);

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
