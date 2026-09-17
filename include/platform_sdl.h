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
 *   that don't go through WaitForVSync. Cross-thread render is safe
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
    int                    subpixel_scroll;  /* 1 = shift the presented frame by the camera's sub-pixel
                                              * remainder so scrolling is continuous instead of stepping
                                              * 1,2,1,2 px (default 1). Quantised to whole output pixels,
                                              * so it never blurs. PC-only; see platform_sdl.c. */
    int                    scaling_smooth;   /* 1 = two-pass scaling (default): pre-multiply the frame
                                              * by a whole number with NEAREST, then let the leftover
                                              * fraction resample LINEARLY, so a non-integer fullscreen
                                              * scale stops giving one source pixel 4 output pixels and
                                              * its neighbour 3. 0 = the old single nearest copy.
                                              * No-ops when the output is already a whole multiple
                                              * (every windowed scale is). PC-only; see platform_sdl.c. */
    /* CRT presentation effects (PC-only; see include/crt_effects.h). All are
     * applied to the FINISHED frame, so none of them touch the simulation. */
    int                    crt_mask;         /* 0 off, 1 aperture, 2 slot, 3 scanlines */
    int                    crt_mask_strength;/* 0..100 — how dark the grille's off-phase is */
    int                    crt_vignette;     /* 0..100 — corner darkening, 0 = off */
    int                    crt_corners;      /* 0..100 — rounded-glass corner size, 0 = square */
    char                   render_driver[16];/* accel backend override: "" / "auto" = SDL default (D3D11 on Windows);
                                              * "opengl" / "direct3d11" / "direct3d" / "metal" force one. Windows D3D11
                                              * serialises presents on CPU-idle frames (replays crawl); "opengl" avoids it. */
    int                    res_w, res_h;     /* resolution driving the render aspect; 0,0 = auto (desktop). PC-only */
    int                    screen;           /* target display index (0 = primary). The window opens on / fullscreens on this monitor and its size is auto-fitted to that screen's usable bounds. Clamped to a valid display. PC-only */
    int                    audio_mono;       /* 1 = down-mix to mono (default), 0 = Amiga panned */
    int                    dos_graphics;     /* 1 = render DOS-version graphics. PC-only */
    int                    dos_gameplay;     /* 1 = DOS gameplay tweaks (framerate etc.). PC-only; reserved */
    int                    fire_hold_lockout;/* 1 = keep the ORIGINAL Amiga bug where an OPPONENT's tackle
                                              * kills your held fire button's shot charge for good. 0 (default)
                                              * = fixed. See _gOriginalFireHoldLockout in team_control.c */
    int                    arcade;           /* 1 = arcade/coin-op mode — boot to attract. PC-only. See docs/ARCADE.md */
    int                    arcade_mode;         /* arcade game mode index (0 = GOLDEN GOAL) */
    int                    arcade_game_seconds; /* arcade match length in seconds (default 45) */
    int                    arcade_golden_goal;  /* 1 = first goal wins the match (default 1) */
    int                    arcade_penalties;    /* 1 = penalty shootout when drawn (default 1) */
    int                    arcade_goal_replay;  /* 1 = replay goals (drives _autoReplays; default 1) */
    int                    arcade_attract_audio;/* attract audio: 0 = music (HERO), 1 = sfx (reel crowd), 2 = music+sfx (default) */
    int                    arcade_gameplay;     /* arcade gameplay feel: 0 = amiga (50fps), 1 = dos (70fps) */
    char                   arcade_key_p1_credit[16]; /* cabinet key bindings (SDL key names; */
    char                   arcade_key_p1_start[16];  /*   default MAME layout 5/6/1/2/F2). */
    char                   arcade_key_p2_credit[16]; /*   Empty -> compiled default. PC-only */
    char                   arcade_key_p2_start[16];
    char                   arcade_key_service[16];
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
/* Platform_VBlank without running the VBlank interrupt: present the finished frame and
 * pace. For a caller that runs the interrupt itself, once per simulated frame. */
int  Platform_Present(void);

/* Just pump SDL events, render bitplanes, and present. No pacing.
 * Safe to call from the SDL_AddTimer thread. */
void Platform_RenderFrame(void);

/* Run the Amiga Level-3 VBlank ISR body, throttled to ~50 Hz max and
 * mutex-guarded so concurrent calls from the timer thread and the main
 * thread serialise. Use this from main-thread spin loops on flags the
 * ISR sets (fadeState, currentGameTick, waitingInterrupt) when calling
 * WaitForVSync isn't appropriate (e.g. you don't want to render a
 * frame). On WSL2 the SDL_Timer thread can be starved indefinitely by
 * a busy main thread, so the spin needs to drive the ISR itself. */
void RunInt3HandlerThrottled(void);

/* Force a quit (set by SDL_QUIT or escape). The Amiga code polls
 * `_quitToDos` -- we set it from here. */
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

/* Ask the OS for a 1 ms sleep granularity (src/platform_timer.c). Only Windows
 * needs it; call it before anything sleeps. Returns 0 if the request failed. */
int32_t Platform_RaiseTimerResolution(void);

#endif /* PLATFORM_SDL_H */
