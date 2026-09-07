/*
 * platform_sdl.c -- SDL2 host implementation.
 *
 * Owns the SDL window, renderer, framebuffer texture, and the per-frame
 * 8bpp-chunky → ARGB8888 palette lookup.
 *
 * Bitplane-to-chunky conversion happens at load time (see
 * `LoadOverlayToFrontbuffer` in frontend.c and `blitDispatch` in
 * image_cmd.c) so the per-frame work here is just one palette lookup
 * per pixel into the streaming SDL texture.
 */

#include <stdint.h>
#include "platform_sdl.h"
#include "display.h"
#include "dos_mode.h"        /* gDosGraphics */
#include "dos_gameplay.h"    /* DosGameplay_TargetFps — 70 (DOS) vs 50 (Amiga) */
#include "dos_graphics.h"    /* DosGfx_BuildPalette */

#include <SDL.h>
#include <stdio.h>
#ifdef SWOS_WASM_BUILD
#include <emscripten.h>      /* emscripten_sleep — the ASYNCIFY yield point */
#endif
#include <stdlib.h>
#include <string.h>

/* Window-title arch suffix so you can tell builds apart at a glance while
 * cross-target testing (e.g. "SWOS (x86 64-bit)" vs "SWOS (x86 32-bit)"). */
#if   defined(__x86_64__)
#  define SWOS_ARCH_STR "x86 64-bit"
#elif defined(__i386__)
#  define SWOS_ARCH_STR "x86 32-bit"
#elif defined(__aarch64__)
#  define SWOS_ARCH_STR "ARM 64-bit"
#elif defined(__arm__)
#  define SWOS_ARCH_STR "ARM 32-bit"
#elif defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8)
#  define SWOS_ARCH_STR "64-bit"
#else
#  define SWOS_ARCH_STR "32-bit"
#endif
#define SWOS_WINDOW_TITLE "SWOS (" SWOS_ARCH_STR ")"

/* ---- Forward decls from audio_sdl.c / input_sdl.c ----------------- */
int  AudioSDL_Init(int mono);
void AudioSDL_Shutdown(void);
void AudioSDL_Tick(void);
void AudioSDL_PauseAndClear(void);

int  InputSDL_Init(void);
void InputSDL_Shutdown(void);
void InputSDL_HandleEvent(const SDL_Event *e);
void InputSDL_PostFrame(void);
void InputSDL_ClearKeyState(void);

/* ---- Game-side globals we read each frame --------------------------
 * The displayed image lives in `frontbufferBase` (or `Alt` after a
 * swap) as 8bpp chunky pixels — 1 byte per pixel = palette index, row
 * stride = SCREEN_WIDTH. `frontbufferBasePtr` is the live pointer to
 * the buffer the game last drew into. */
extern volatile uintptr_t _frontbufferBasePtr;
extern volatile uintptr_t _frontbufferBaseAltPtr;
extern uint8_t  _frontbufferBase[];
extern uint8_t  _frontbufferBaseAlt[];
extern uint8_t  _frontbufferMaskBuffer[];
extern uint16_t _cameraXMod320;
extern uint16_t _cameraYMod320;
extern uint16_t _useAltBaseFlag;
extern uint16_t _usePlaneOffsetFlag;

/* The copperlist holds the live palette in Amiga 0xRGB nibbles. We
 * snapshot it into a 256-entry RGBA palette each frame. */
extern uint8_t _copperListBuffer[];

/* _quitToDos lives in platform_amiga_stubs.c */
extern uint16_t _quitToDos;

/* ---- SDL state ----------------------------------------------------- */
static SDL_Window   *s_window;
static SDL_Renderer *s_renderer;
/* A small RING of streaming textures. We upload into a DIFFERENT one each frame so
 * we never lock/refill the texture the GPU is still sampling from a recent frame.
 * That per-frame resource contention is what stalls SDL's D3D11 texture-upload
 * path (~45 ms/frame) when the frames are too cheap to hide it (arcade replays);
 * separating the CPU's upload target from the GPU's read target keeps the pipeline
 * flowing. Ring depth 4 stays a frame clear of D3D11's default 3-frame latency
 * queue. Harmless on OpenGL (which never stalled). */
#define PLATFORM_TEX_RING 4
static SDL_Texture  *s_textures[PLATFORM_TEX_RING];  /* RGBA8888 streaming, _screen_width x 272 */
static int           s_tex_idx;
/* Cached-RAM staging buffer: ALL per-pixel work (8bpp→RGBA, overlay blend) happens
 * here, then one SDL_UpdateTexture uploads it. Writing/reading SDL_LockTexture's
 * mapped memory directly is ~100× slower on D3D11 (write-combined/uncached), which
 * made the render 43 ms/frame; normal RAM makes it ~sub-ms. Sized _screen_width×272. */
static uint32_t     *s_frame_rgba;
static SDL_Color     s_palette[256];
static SDL_mutex    *s_render_mtx;       /* serializes Platform_RenderFrame calls */

static Uint64        s_perf_freq;
static Uint64        s_next_frame_perf;  /* high-res counter for 50 Hz pacing */
static double        k_frame_secs = 1.0 / 50.0;  /* refreshed per frame from the
                                                  * active game style (DOS 70 / Amiga 50) */
/* `s_last_main_render_ms` is the SDL_GetTicks() value of the last
 * render driven by Platform_VBlank (main/game thread). The SDL timer
 * callback uses this to skip its own render when the main thread is
 * already presenting complete frames — if the timer renders while the
 * main thread is mid-draw (between DrawChunkyPitch and DrawSprites),
 * the partial frontbuffer gets presented and sprites flicker in/out
 * underneath the freshly-redrawn pitch. The timer is only the primary
 * renderer during loader/intro code that spins on currentGameTick
 * without calling WaitForVSync. */
static volatile Uint32 s_last_main_render_ms;

/* Screenshot triggers:
 *   - Hotkeys F9 / F11 / F12 / PrintScreen — set s_screenshot_request from
 *     pump_events_any_thread on KEYDOWN. F12 is the historical default but
 *     gets intercepted by some window managers (WSL, GNOME); F9/F11/PrtSc
 *     are alternates that survive most setups.
 *   - Env var SWOS_SCREENSHOT_INTERVAL=N — write every Nth rendered frame
 *     to swos_shotNNN.png, capped at 200 files. Useful when no hotkey
 *     reaches the app (in-match focus, screen recorder grabbing all
 *     function keys, headless test runs).
 * In all cases the request is processed inside Platform_RenderFrame while
 * the render mutex is held, so the captured ARGB buffer is consistent. */
static volatile int s_screenshot_request;
static int          s_screenshot_index;
static int          s_screenshot_interval;   /* 0 = disabled, N>0 = every N frames */
static int          s_screenshot_auto_count; /* number written by interval mode */

/* Arcade coin/service input (MAME keys 5/6/F2). Edge-counted in the event pump,
 * drained by Platform_ArcadeInput. Only consumed when _gArcadeMode is set. */
static volatile int s_arcade_coin1;
static volatile int s_arcade_coin2;
static volatile int s_arcade_service;
static volatile int s_arcade_start1;   /* P1 start button */
static volatile int s_arcade_start2;   /* P2 start button */

/* Resolved cabinet key bindings — set from opts at Platform_Init via
 * SDL_GetKeyFromName; defaults are the MAME layout (5/6 coin, 1/2 start, F2 svc). */
static SDL_Keycode s_arcade_key_p1_credit = SDLK_5;
static SDL_Keycode s_arcade_key_p2_credit = SDLK_6;
static SDL_Keycode s_arcade_key_p1_start  = SDLK_1;
static SDL_Keycode s_arcade_key_p2_start  = SDLK_2;
static SDL_Keycode s_arcade_key_service   = SDLK_F2;

static void arcade_resolve_key(SDL_Keycode *dst, const char *name)
{
    if (name && name[0]) {
        SDL_Keycode k = SDL_GetKeyFromName(name);
        if (k != SDLK_UNKNOWN) *dst = k;
    }
}

void Platform_ArcadeInput(int *coin1, int *coin2, int *service,
                          int *start1, int *start2)
{
    if (coin1)   { *coin1   = s_arcade_coin1;   s_arcade_coin1   = 0; }
    if (coin2)   { *coin2   = s_arcade_coin2;   s_arcade_coin2   = 0; }
    if (service) { *service = s_arcade_service; s_arcade_service = 0; }
    if (start1)  { *start1  = s_arcade_start1;  s_arcade_start1  = 0; }
    if (start2)  { *start2  = s_arcade_start2;  s_arcade_start2  = 0; }
}

/* DEBUG: F10 key for the arcade — returns 1 once per press (edge-triggered). Polled
 * from Arcade_MatchTick (in-match: jump to the name-entry flow) and from
 * Arcade_DrawAttractOverlay (attract: step the screen cycler). The keyboard state is
 * refreshed by the event pump in Platform_VBlank. SDL-only. */
int Platform_ArcadeDebugWin(void)
{
    static int prev = 0;
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    int now  = ks ? ks[SDL_SCANCODE_F10] : 0;
    int edge = now && !prev;
    prev = now;
    return edge;
}

/* Load an indexed (8bpp paletted) BMP — the arcade FLAGS sheet / SWCPICH pitches.
 * Copies up to out_cap index bytes into out8 (row-packed at width *outW) and the
 * 256-entry palette as ARGB8888 into outPal. Returns 0 on success. SDL-only. */
int Platform_LoadIndexedBMP(const char *path, uint8_t *out8, int out_cap,
                            uint32_t *outPal, int *outW, int *outH)
{
    SDL_Surface *s = SDL_LoadBMP(path);
    if (!s) { fprintf(stderr, "[arcade] SDL_LoadBMP(%s): %s\n", path, SDL_GetError()); return 1; }

    int rc = 1;
    if (s->format->BytesPerPixel == 1 && s->format->palette &&
        s->w * s->h <= out_cap) {
        for (int y = 0; y < s->h; y++)
            memcpy(out8 + (size_t)y * s->w,
                   (const uint8_t *)s->pixels + (size_t)y * s->pitch, s->w);
        int n = s->format->palette->ncolors;
        for (int i = 0; i < 256; i++) {
            if (i < n) {
                SDL_Color c = s->format->palette->colors[i];
                outPal[i] = ((uint32_t)0xFFu << 24) | ((uint32_t)c.r << 16) |
                            ((uint32_t)c.g << 8) | (uint32_t)c.b;
            } else {
                outPal[i] = 0xFF000000u;
            }
        }
        if (outW) *outW = s->w;
        if (outH) *outH = s->h;
        rc = 0;
    } else {
        fprintf(stderr, "[arcade] %s not an indexed BMP that fits (bpp=%d %dx%d)\n",
                path, s->format->BytesPerPixel, s->w, s->h);
    }
    SDL_FreeSurface(s);
    return rc;
}

/* Headless smoke-test: dump every Nth converted frame to /tmp as a PPM. */
static int           s_dump_frames;
static int           s_frame_count;

/* Renderer mode chosen at Platform_Init. The SDL_AddTimer worker only
 * issues a fallback Platform_RenderFrame when the renderer is software —
 * hardware (D3D / GL / Metal) contexts are thread-bound. */
static int           s_renderer_is_hw;
static int           s_vsync_enabled;
static int           s_subpixel_scroll = 1;   /* [video] subpixel_scroll — see present_frame_copy */

/* Harness TURBO (env SWOS_HARNESS_TURBO, read once in Platform_Init): when
 * set, the inferior runs frames CPU-bound instead of paced to a 50 Hz wall
 * clock. The gdb test harness drives the game frame-by-frame via `continue`,
 * so per-frame real-time waits are pure overhead — and under WSL2/gdb the
 * timer thread starves, ballooning those waits from tens-of-ms to 10–120 s
 * per cont (driver then over-runs its timeout and false-reports "stuck").
 * Turbo removes every wall-clock wait (Platform_VBlank pacing, the
 * RunInt3HandlerThrottled throttle, the 20 ms timer interval) so a cont is
 * bound by CPU work, not by a clock the scheduler stretches. No effect on
 * normal play (default off). SDL-only file; nothing to back-port to Amiga. */
static int           s_harness_turbo;

/* ---- VBlank tick driver ------------------------------------------- *
 * The Amiga VBlank ISR (Int3TimerHandler) was responsible for bumping
 * `currentGameTick` once per frame. The game polls that counter in
 * busy-wait loops (delay_ticks, asset-retry loops, ...). Since we do
 * not run the ISR on SDL, those busy-waits would spin forever.
 *
 * We register an SDL_AddTimer callback that ticks the counter at 50 Hz,
 * matching the PAL VBlank rate SWOS was designed for. The timer runs
 * on SDL's own thread, so the busy-wait loops in the game code see
 * the value change. */
extern volatile uint16_t _currentGameTick;
extern void _Int3TimerHandler(void);   /* the real Amiga VBlank ISR body */

static void pump_quit_only(void);      /* defined below; used by tick_timer_cb */
static void render_frame_impl(int pump_full);   /* below; pump_full=0 on the SDL timer thread */

static SDL_TimerID s_tick_timer;
static SDL_mutex  *s_isr_mtx;          /* serialises _Int3TimerHandler */
static Uint32      s_last_isr_ms;      /* last ISR fire time, for throttle */

/* Active host pacing rate (Hz). Normally the gameplay-mode target
 * (DosGameplay_TargetFps: 70 DOS / 50 Amiga). BUT while a LOADED replay is
 * playing (_playHighlightFlag), pace at the replay's OWN recorded framerate
 * (_replayPlaybackFps) so a DOS RPL always plays at 70 Hz and a native Amiga
 * .HIL at 50 Hz, independent of the current gameplay setting. The in-match
 * instant replay (PlayInstantReplayLoop) does NOT set _playHighlightFlag, so it
 * correctly stays on the gameplay rate — it was recorded at that rate. Every
 * per-frame pacing site below reads this so the main frame clock, the worker
 * tick, and the ISR throttle all agree during replay playback. */
static int Platform_ActivePacingFps(void)
{
    extern volatile uint16_t _playHighlightFlag;
    extern int _replayPlaybackFps;
    if (_playHighlightFlag != 0 && _replayPlaybackFps > 0)
        return _replayPlaybackFps;
    return DosGameplay_TargetFps();
}

/* Run the Amiga Level-3 VBlank ISR body, throttled to ~50 Hz max and
 * guarded by a mutex so callers from different threads serialise. The
 * Amiga IRQ was hardware-preemptive at 50 Hz; on SDL we approximate that
 * by calling this from BOTH the SDL_Timer worker thread (every 20 ms by
 * default) AND from Platform_VBlank on the main thread (so any
 * WaitForVSync also drives the ISR). Whichever thread fires first wins
 * for that 20 ms window; the other call returns immediately via the
 * throttle. This guarantees fade ticks, currentGameTick, and audio
 * housekeeping make progress even when one thread is starved (most
 * commonly the SDL_Timer thread on WSL2, where main-thread spin loops
 * can monopolise CPU). */
void RunInt3HandlerThrottled(void)
{
    /* Turbo collapses the 18 ms throttle to 0 so currentGameTick advances on
     * every call — tick-counting waits (fades, holds, the loader-intro
     * timeout) drain at CPU speed and stop depending on the starved timer
     * thread. The loop that calls this exits as soon as the tick advances, so
     * it runs once or twice, not unbounded. */
    /* Throttle just under one frame interval: 18 ms @ 50 Hz, ~12 ms @ 70 Hz (DOS
     * or a 70 Hz replay). */
    const Uint32 thr = s_harness_turbo ? 0u : (Uint32)(1000 / Platform_ActivePacingFps() - 2);
    Uint32 now = SDL_GetTicks();
    /* Cheap pre-check before the lock — most calls will be too soon. */
    if (now - s_last_isr_ms < thr) return;
    if (!s_isr_mtx) return;            /* not initialised yet */
    if (SDL_TryLockMutex(s_isr_mtx) != 0) return;  /* other thread already in */
    /* Re-check after lock acquired (the other thread may have just run it). */
    now = SDL_GetTicks();
    if (now - s_last_isr_ms >= thr) {
        s_last_isr_ms = now;
        _Int3TimerHandler();
    }
    SDL_UnlockMutex(s_isr_mtx);
}

static Uint32 SDLCALL tick_timer_cb(Uint32 interval, void *param)
{
    (void)param;
    /* The original Amiga Int3 (Level-3 VBlank) ISR did all per-frame
     * housekeeping: bump currentGameTick / currentTick, run fade ticks,
     * pump audio, do buffer swap if waitingInterrupt was set, etc.
     * On the SDL host we run it from the SDL_Timer thread at 50 Hz, with
     * Platform_VBlank on the main thread also driving it as a fallback
     * for when the timer thread gets starved (common on WSL2). */
    RunInt3HandlerThrottled();

    /* Pump audio production from the worker thread regardless of
     * renderer mode. With --vsync=on the main thread can occasionally
     * spend longer than one game frame inside SDL_RenderPresent (the
     * irreducible 50→60 pulldown jitter on a 60 Hz monitor); without an
     * independent producer the audio queue would drain during those
     * long presents and you'd hear short pauses in the music. The
     * queue-fill check inside AudioSDL_Tick is mutex-guarded so
     * main + worker calling it concurrently doesn't double-advance the
     * RJP1 song state. */
    AudioSDL_Tick();

    /* The loader-intro path waits on `currentGameTick` rather than
     * `WaitForVSync`, so the main thread never reaches our render
     * hook. Drive a render directly from the timer thread only when
     * the main thread hasn't rendered recently — otherwise we race
     * the main thread's pitch/sprite draw and present a torn frame
     * (you see pitch tiles mid-update stomping over sprites).
     *
     * Regardless of whether we render, *always* pump SDL events here:
     * otherwise SDL_QUIT from the window's X button sits in the queue
     * until main happens to call Platform_RenderFrame again, which can
     * take many frames if main is mid-draw or blocked. Decoupling the
     * pump from the render means the X button responds within 20 ms. */
    Uint32 now_ms = SDL_GetTicks();
    Uint32 since  = now_ms - s_last_main_render_ms;
    /* Only software renderers tolerate render calls from this worker
     * thread. Hardware (D3D / OpenGL / Metal) contexts are bound to the
     * creating thread and silently drop or corrupt cross-thread calls;
     * for hardware mode the loader-intro path is the cost — the screen
     * may briefly look frozen during big asset decompresses, but that
     * is purely cosmetic. */
    if (!s_renderer_is_hw &&
        (s_last_main_render_ms == 0 || since > 60)) {
        /* Worker-thread render: pump_full=0 — do NOT call SDL_PollEvent/SDL_PumpEvents here.
         * Those pull from the OS window message queue and are main-thread-only on Windows;
         * doing them from this worker deadlocks the main thread. QUIT/WINDOW are drained via
         * pump_quit_only (SDL_PeepEvents, thread-safe) inside render_frame_impl. */
        render_frame_impl(0);
    } else {
        pump_quit_only();
    }

    /* Turbo: fire as fast as SDL allows (1 ms) so the loader-intro path — a
     * pure spin on currentGameTick that never calls Platform_VBlank, advanced
     * ONLY from this thread — drains in a fraction of a second instead of
     * ~1–2 min. With the RunInt3HandlerThrottled throttle also at 0, every
     * fire advances the tick. */
    /* Re-arm at the active rate so a DOS-gameplay toggle — or a loaded replay's
     * own framerate — re-paces the worker tick: 1000/70 ≈ 14 ms (DOS / 70 Hz
     * replay) or 1000/50 = 20 ms (Amiga). */
    (void)interval;
    return s_harness_turbo ? (Uint32)1 : (Uint32)(1000 / Platform_ActivePacingFps());
}

extern void Platform_RewireCustom(void);
extern void Platform_PreArmIsrGlobals(void);

/* ---- Lifecycle ----------------------------------------------------- */
/* Persisted across the run so Alt+Enter / +/- keys can read them. */
static int                    s_win_scale_current = 1;   /* clamp 1..4 */
static PlatformFullscreenMode s_fullscreen_mode   = PLATFORM_FULLSCREEN_NONE;

int Platform_Init(const PlatformInitOptions *opts)
{
    int win_scale = opts ? opts->win_scale : 1;
    PlatformFullscreenMode fs_mode =
        opts ? opts->fullscreen : PLATFORM_FULLSCREEN_NONE;
    PlatformRendererMode renderer_mode =
        opts ? opts->renderer : PLATFORM_RENDERER_ACCEL;
    int vsync = opts ? opts->vsync : 0;

    if (win_scale < 1) win_scale = 1;
    if (win_scale > 4) win_scale = 4;

    /* Which monitor to open on ([video] screen=N, 0 = primary). */
    int screen = opts ? opts->screen : 0;
    if (screen < 0) screen = 0;
#ifndef SWOS_WASM_BUILD
    /* Clamp the display index to a real one, then shrink win_scale so the
     * windowed size fits that monitor's USABLE bounds (desktop minus the
     * taskbar/dock). Without this the fixed default (WIN_SCALE=3 →
     * _screen_width*3 × 272*3) overflows a laptop/small screen — the "window is
     * bigger than my monitor" report. Windowed only; fullscreen covers the
     * display regardless. The user can still scale up with +/- to what fits. */
    {
        int nd = SDL_GetNumVideoDisplays();
        if (nd < 1) nd = 1;
        if (screen >= nd) screen = 0;
        if (fs_mode == PLATFORM_FULLSCREEN_NONE) {
            SDL_Rect ub;
            if (SDL_GetDisplayUsableBounds(screen, &ub) == 0 && ub.w > 0 && ub.h > 0) {
                while (win_scale > 1 &&
                       ((int)_screen_width * win_scale > ub.w ||
                        PLATFORM_SCREEN_H * win_scale > ub.h)) {
                    win_scale--;
                }
            }
        }
    }
#endif
#ifdef SWOS_WASM_BUILD
    /* Web: fullscreen is browser-managed (F11) — SDL desktop-fullscreen at boot
     * is user-gesture-gated and would stretch the canvas — so force windowed
     * mode regardless of SWOS.ini. win_scale + vsync come from SWOS.ini (main.c
     * defaults: native scale, vsync off — the canvas CSS upscales to the
     * viewport, and we pace via emscripten_sleep). */
    fs_mode = PLATFORM_FULLSCREEN_NONE;
#endif
    s_win_scale_current = win_scale;

    if (opts) {
        arcade_resolve_key(&s_arcade_key_p1_credit, opts->arcade_key_p1_credit);
        arcade_resolve_key(&s_arcade_key_p2_credit, opts->arcade_key_p2_credit);
        arcade_resolve_key(&s_arcade_key_p1_start,  opts->arcade_key_p1_start);
        arcade_resolve_key(&s_arcade_key_p2_start,  opts->arcade_key_p2_start);
        arcade_resolve_key(&s_arcade_key_service,   opts->arcade_key_service);
    }
    s_fullscreen_mode   = fs_mode;
    /* Repoint `custom` at our stub storage *before* the tick timer
     * starts -- the first VBlank ISR firing reads custom->intreqr. */
    Platform_RewireCustom();
    Platform_PreArmIsrGlobals();

    /* SDL_WINDOW_RESIZABLE: lets the user resize/maximise the desktop window (content
     * is letterboxed by SDL_RenderSetLogicalSize below), and on macOS it's what enables
     * the green title-bar zoom/fullscreen button (greyed out without it).
     * NOT on WASM: emscripten's SDL2 sizes the <canvas> element to the SDL window when
     * it's resizable, which overrides the shell.html responsive CSS (height:100vh /
     * object-fit:contain) and pins the canvas to a small fixed size letterboxed in the
     * page. On the web the page CSS owns the canvas display size, so leave it off. */
    Uint32 wflags = SDL_WINDOW_SHOWN;
#ifndef SWOS_WASM_BUILD
    wflags |= SDL_WINDOW_RESIZABLE;
#endif
    if (fs_mode == PLATFORM_FULLSCREEN_DESKTOP) {
        /* Borderless window sized to the desktop, no display-mode
         * change. SDL_RenderSetLogicalSize letterboxes 336x272 inside
         * whatever resolution the desktop is at. Fast to toggle. On
         * Windows XP the taskbar can occasionally show through above
         * a borderless window — use --fullscreen=exclusive there. */
        wflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    } else if (fs_mode == PLATFORM_FULLSCREEN_EXCLUSIVE) {
        /* True fullscreen with display-mode change. Always covers the
         * taskbar (even XP). Slower toggle, may flicker on multi-monitor
         * rigs, and the desktop resolution is restored on exit. */
        wflags |= SDL_WINDOW_FULLSCREEN;
    }

    /* Open (and, for fullscreen, cover) the requested monitor. CENTERED_DISPLAY
     * places the window on display `screen`; on WASM SDL only has display 0. */
    s_window = SDL_CreateWindow(
        SWOS_WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED_DISPLAY(screen),
        SDL_WINDOWPOS_CENTERED_DISPLAY(screen),
        _screen_width * win_scale, PLATFORM_SCREEN_H * win_scale,
        wflags);
    if (!s_window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return -1;
    }

#ifndef SWOS_WASM_BUILD
    /* Post-creation window fit (windowed only). The pre-creation scale clamp
     * uses SDL_GetDisplayUsableBounds, which can under-shrink (DPI virtualization
     * on Windows, work-area quirks) and CANNOT account for the title bar — that's
     * added by the OS OUTSIDE the client rect. CENTERED_DISPLAY then centers the
     * CLIENT rect, so a tall window pushes its title bar above the screen top:
     * "the window opens inside the screen but I can't see the close/maximize
     * buttons" (16" laptop, 1920x1080). Redo the fit with the REAL decorated
     * geometry: shrink the integer scale until the framed window fits the usable
     * area, then move it so the whole frame — title bar included — is on-screen. */
    if (fs_mode == PLATFORM_FULLSCREEN_NONE) {
        int disp = SDL_GetWindowDisplayIndex(s_window);
        if (disp < 0) disp = screen;
        SDL_Rect ub, db;
        int have_ub = (SDL_GetDisplayUsableBounds(disp, &ub) == 0 && ub.w > 0 && ub.h > 0);
        int have_db = (SDL_GetDisplayBounds(disp, &db) == 0 && db.w > 0 && db.h > 0);
        if (!have_ub && have_db) { ub = db; have_ub = 1; }   /* usable query failed */
        if (have_ub && have_db) {
            /* Never trust usable bounds bigger than the physical display — some
             * multi-monitor drivers return the whole virtual desktop for a single
             * display index, which is why the tester's 4x (1936x1088) window was
             * NOT shrunk below his 1920x1080 panel. Cap to the display's real
             * size/origin so the fit reflects the monitor the window is on. */
            if (ub.w > db.w) ub.w = db.w;
            if (ub.h > db.h) ub.h = db.h;
            if (ub.x < db.x) ub.x = db.x;
            if (ub.y < db.y) ub.y = db.y;
        }
        if (have_ub) {
            /* Border thicknesses; bt = title-bar height. 0 if unsupported. */
            int bt = 0, bl = 0, bb = 0, br = 0;
            SDL_GetWindowBordersSize(s_window, &bt, &bl, &bb, &br);

            /* Shrink until the DECORATED window fits the usable area. */
            while (win_scale > 1 &&
                   ((int)_screen_width * win_scale + bl + br > ub.w ||
                    PLATFORM_SCREEN_H * win_scale + bt + bb > ub.h)) {
                win_scale--;
            }
            SDL_SetWindowSize(s_window,
                              _screen_width * win_scale,
                              PLATFORM_SCREEN_H * win_scale);
            s_win_scale_current = win_scale;

            /* Reposition so the full frame (client + borders) sits inside the
             * usable bounds; guarantee the title bar is never above the top. */
            int wx, wy, ww, wh;
            SDL_GetWindowPosition(s_window, &wx, &wy);
            SDL_GetWindowSize(s_window, &ww, &wh);
            int outerW = ww + bl + br, outerH = wh + bt + bb;
            if (wx - bl < ub.x)                    wx = ub.x + bl;
            if (wy - bt < ub.y)                    wy = ub.y + bt;
            if (wx - bl + outerW > ub.x + ub.w)    wx = ub.x + ub.w - outerW + bl;
            if (wy - bt + outerH > ub.y + ub.h)    wy = ub.y + ub.h - outerH + bt;
            if (wx - bl < ub.x)                    wx = ub.x + bl;   /* re-pin if too wide */
            if (wy - bt < ub.y)                    wy = ub.y + bt;   /* title bar on-screen */
            SDL_SetWindowPosition(s_window, wx, wy);
        }

        /* Diagnostic: the actual window geometry + the monitor it fits, so a
         * tester's SWOS.log tells us whether the fit worked (and DPI is sane). */
        {
            int wx = 0, wy = 0, ww = 0, wh = 0;
            SDL_GetWindowPosition(s_window, &wx, &wy);
            SDL_GetWindowSize(s_window, &ww, &wh);
            SDL_Rect ub2 = { 0, 0, 0, 0 };
            SDL_GetDisplayUsableBounds(disp, &ub2);
            fprintf(stderr,
                    "[video] window %dx%d @%d,%d scale=%d display=%d usable=%dx%d\n",
                    ww, wh, wx, wy, s_win_scale_current, disp, ub2.w, ub2.h);
        }
    }
#endif

    /* Renderer choice. Hardware (accelerated) is the default — vsync
     * gives a 50 Hz monitor user free perfect pacing. Software is
     * available as an opt-in for hosts where the hardware backend
     * misbehaves; it permits cross-thread RenderFrame calls from the
     * SDL_AddTimer worker (used as a fallback during loader-intro
     * spins). */
    Uint32 rflags;
    if (renderer_mode == PLATFORM_RENDERER_SOFTWARE) {
        rflags = SDL_RENDERER_SOFTWARE;
        s_renderer_is_hw = 0;
        s_vsync_enabled  = 0;            /* software ignores PRESENTVSYNC */
    } else {
        rflags = SDL_RENDERER_ACCELERATED;
        if (vsync) rflags |= SDL_RENDERER_PRESENTVSYNC;
        s_renderer_is_hw = 1;
        s_vsync_enabled  = vsync;
    }

    /* Optional accel-backend override ([video] render_driver). SDL_CreateRenderer
     * with index -1 picks the first driver matching SDL_HINT_RENDER_DRIVER; unset
     * = SDL's platform default (D3D11 on Windows). On Windows the D3D11 backend
     * serialises presents on CPU-idle frames, so cheap-per-frame content like the
     * arcade attract replays crawls even though live play is fine — "opengl" sides
     * around it. The env var SDL_RENDER_DRIVER overrides this too. */
    if (opts && opts->render_driver[0] &&
        SDL_strcasecmp(opts->render_driver, "auto") != 0) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, opts->render_driver);
        fprintf(stderr, "[render] requested driver '%s' via [video] render_driver\n",
                opts->render_driver);
    }

    s_renderer = SDL_CreateRenderer(s_window, -1, rflags);
    if (!s_renderer) {
        /* Fall back to software if the requested mode failed. */
        fprintf(stderr,
            "SDL_CreateRenderer (%s%s) failed: %s — falling back to software\n",
            renderer_mode == PLATFORM_RENDERER_ACCEL ? "accel" : "software",
            (renderer_mode == PLATFORM_RENDERER_ACCEL && vsync) ? "+vsync" : "",
            SDL_GetError());
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
        s_renderer_is_hw = 0;
        s_vsync_enabled  = 0;
    }
    if (!s_renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return -1;
    }

    s_subpixel_scroll = opts ? opts->subpixel_scroll : 1;

    /* Report what we actually GOT, not what we asked for. Ruff n Tumble's loop
     * does the same (SDL_SetRenderVSync then SDL_GetRenderVSync to confirm, and
     * it warns when vsync is unavailable) and it is the right instinct: a
     * requested PRESENTVSYNC can be silently dropped by the driver or overridden
     * in the GPU control panel, and without this line a tester report saying
     * "vsync on, still stutters" is unfalsifiable.
     *
     * The refresh rate matters just as much for triage. Our pacing rate is 50 Hz
     * (70 in DOS mode); if the panel divides evenly (100/150/200/300 Hz at 50)
     * every game frame gets a whole number of refreshes and pacing is exact, so
     * any remaining unevenness is NOT a pacing problem and must be chased
     * elsewhere — see present_frame_copy and docs/AMIGA_60.md. */
    {
        SDL_RendererInfo info;
        if (SDL_GetRendererInfo(s_renderer, &info) == 0) {
            int got_vsync = (info.flags & SDL_RENDERER_PRESENTVSYNC) ? 1 : 0;
            fprintf(stderr, "[render] driver=%s flags=0x%x hw=%d vsync: requested=%s actual=%s\n",
                    info.name ? info.name : "?", info.flags, s_renderer_is_hw,
                    s_vsync_enabled ? "on" : "off",
                    got_vsync ? "on" : "off");
            /* Only BELIEVE the flag on a hardware renderer. The software
             * renderer reports PRESENTVSYNC (flags 0xd) while doing nothing of
             * the sort, which is why the branch above deliberately forces
             * s_vsync_enabled = 0 for it — don't let the readback undo that. */
            if (s_renderer_is_hw) {
                if (s_vsync_enabled && !got_vsync)
                    fprintf(stderr, "[render] WARNING: vsync requested but the renderer did "
                                    "not enable it — frames are paced by the timer only\n");
                s_vsync_enabled = got_vsync;
            }
        }
        SDL_DisplayMode dm;
        if (SDL_GetCurrentDisplayMode(screen, &dm) == 0 && dm.refresh_rate > 0) {
            int fps = Platform_ActivePacingFps();
            fprintf(stderr, "[render] display=%d %dx%d @%dHz, game paces at %dHz%s\n",
                    screen, dm.w, dm.h, dm.refresh_rate, fps,
                    (fps > 0 && dm.refresh_rate % fps == 0)
                        ? " (exact multiple — pacing is exact)"
                        : " (not a multiple — some pulldown judder is inherent)");
        }
        fprintf(stderr, "[render] subpixel_scroll=%s\n",
                s_subpixel_scroll ? "on" : "off");
    }

    SDL_RenderSetLogicalSize(s_renderer, _screen_width, PLATFORM_SCREEN_H);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_EventState(SDL_MOUSEMOTION, SDL_DISABLE);

    s_tex_idx = 0;
    for (int ti = 0; ti < PLATFORM_TEX_RING; ti++) {
        s_textures[ti] = SDL_CreateTexture(
            s_renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            _screen_width, PLATFORM_SCREEN_H);
        if (!s_textures[ti]) {
            fprintf(stderr, "SDL_CreateTexture[%d]: %s\n", ti, SDL_GetError());
            return -1;
        }
    }
    s_frame_rgba = (uint32_t *)SDL_malloc((size_t)_screen_width * PLATFORM_SCREEN_H * 4);
    if (!s_frame_rgba) {
        fprintf(stderr, "frame buffer alloc failed (%d x %d)\n", _screen_width, PLATFORM_SCREEN_H);
        return -1;
    }

    /* Bring up the audio + input subsystems. Failures here are non-fatal —
     * we'd still rather see graphics than nothing. */
    (void)AudioSDL_Init(opts ? opts->audio_mono : 1);
    (void)InputSDL_Init();

    /* Bump the OS timer resolution so SDL_Delay() doesn't oversleep.
     * Windows' default scheduler tick is 15.6 ms, which means a 20 ms
     * sleep often becomes 30+ ms — making the 50 Hz pacing loop alternate
     * between 20 ms and 30 ms frames and producing the "choppy / uneven
     * framerate" testers report. SDL_HINT_TIMER_RESOLUTION asks the
     * underlying platform timer to drop to 1 ms granularity (on Windows
     * this is `timeBeginPeriod(1)` under the hood). Linux is already
     * fine but the hint is harmless there. */
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    s_perf_freq = SDL_GetPerformanceFrequency();
    s_next_frame_perf = SDL_GetPerformanceCounter();

    s_render_mtx = SDL_CreateMutex();
    s_isr_mtx    = SDL_CreateMutex();
    s_dump_frames = (getenv("SWOS_DUMP_FRAMES") != NULL);

    s_harness_turbo = (getenv("SWOS_HARNESS_TURBO") != NULL);
    /* Harness fast-boot: skip the (slow) loader intro under the harness too. */
    { extern volatile uint8_t _skipLoaderIntro; if (s_harness_turbo) _skipLoaderIntro = 1; }
    if (s_harness_turbo)
        fprintf(stderr,
            "[harness] TURBO: 50 Hz pacing + ISR throttle + 20 ms timer "
            "disabled — frames run CPU-bound for the gdb harness (no "
            "wall-clock waits to balloon under WSL2/gdb starvation)\n");

    {
        const char *si = getenv("SWOS_SCREENSHOT_INTERVAL");
        if (si && *si) {
            int n = atoi(si);
            if (n > 0) {
                s_screenshot_interval = n;
                fprintf(stderr,
                    "[screenshot] interval mode: writing every %d frame(s) "
                    "(swos_shotNNN.png), capped at 200 files\n", n);
            }
        }
    }
    fprintf(stderr,
        "[screenshot] hotkeys: F9 / F11 / F12 / PrintScreen "
        "(env SWOS_SCREENSHOT_INTERVAL=N for auto-capture every N frames)\n");

    /* 20 ms = 50 Hz, the PAL VBlank rate the game expects. */
#ifndef SWOS_WASM_BUILD
    /* Single-threaded on WASM: Platform_VBlank already pumps the ISR + render +
     * audio every frame, so this fallback timer is redundant (and pthreads on
     * wasm need SharedArrayBuffer/COOP+COEP — avoided entirely). */
    SDL_InitSubSystem(SDL_INIT_TIMER);
    s_tick_timer = SDL_AddTimer((Uint32)(1000 / Platform_ActivePacingFps()), tick_timer_cb, NULL);
#endif

    return 0;
}

void Platform_Shutdown(void)
{
    if (s_tick_timer) { SDL_RemoveTimer(s_tick_timer); s_tick_timer = 0; }
    if (s_render_mtx) { SDL_DestroyMutex(s_render_mtx); s_render_mtx = NULL; }
    if (s_isr_mtx)    { SDL_DestroyMutex(s_isr_mtx);    s_isr_mtx    = NULL; }
    InputSDL_Shutdown();
    AudioSDL_Shutdown();

    for (int ti = 0; ti < PLATFORM_TEX_RING; ti++)
        if (s_textures[ti]) { SDL_DestroyTexture(s_textures[ti]); s_textures[ti] = NULL; }
    if (s_frame_rgba) { SDL_free(s_frame_rgba); s_frame_rgba = NULL; }
    if (s_renderer) { SDL_DestroyRenderer(s_renderer); s_renderer = NULL; }
    if (s_window)   { SDL_DestroyWindow(s_window);     s_window = NULL; }
}

void Platform_ToggleFullscreen(void)
{
    if (!s_window) return;
    Uint32 flags = SDL_GetWindowFlags(s_window);
    int currently_fullscreen =
        (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;

    if (currently_fullscreen) {
        SDL_SetWindowFullscreen(s_window, 0);
        /* Restore windowed size from the persisted scale. */
        SDL_SetWindowSize(s_window,
            _screen_width * s_win_scale_current,
            PLATFORM_SCREEN_H * s_win_scale_current);
        SDL_ShowCursor(SDL_ENABLE);
    } else {
        /* Toggle into the flavour the user launched with. If they
         * launched windowed, default to DESKTOP-style fullscreen since
         * that's the friendlier of the two. */
        Uint32 fs_flag = (s_fullscreen_mode == PLATFORM_FULLSCREEN_EXCLUSIVE)
                         ? SDL_WINDOW_FULLSCREEN
                         : SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_SetWindowFullscreen(s_window, fs_flag);
        SDL_ShowCursor(SDL_DISABLE);
    }
}

int Platform_GetWindowScale(void)
{
    return s_win_scale_current;
}

PlatformFullscreenMode Platform_GetFullscreenMode(void)
{
    /* Return the *current* mode, not the launch one — Alt+Enter at
     * runtime can land us in fullscreen even if we launched windowed,
     * and we want SWOS.ini to remember that. The mode value reflects
     * the SDL window flags right now. */
    if (s_window) {
        Uint32 flags = SDL_GetWindowFlags(s_window);
        if (flags & SDL_WINDOW_FULLSCREEN) {
            /* Distinguish DESKTOP (FULLSCREEN_DESKTOP includes FULLSCREEN
             * bit) from EXCLUSIVE (FULLSCREEN bit set, DESKTOP bit not). */
            if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP)
                return PLATFORM_FULLSCREEN_DESKTOP;
            return PLATFORM_FULLSCREEN_EXCLUSIVE;
        }
    }
    return PLATFORM_FULLSCREEN_NONE;
}

void Platform_SetWindowScale(int scale)
{
    if (!s_window) return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    s_win_scale_current = scale;
    /* Only resize when actually windowed — fullscreen ignores the size
     * (SDL_RenderSetLogicalSize handles letterboxing for us). */
    Uint32 flags = SDL_GetWindowFlags(s_window);
    if ((flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) == 0) {
        SDL_SetWindowSize(s_window,
            _screen_width * scale,
            PLATFORM_SCREEN_H * scale);
        /* Re-centre on the desktop after a size change so a 4× window
         * doesn't end up clipped against the bottom-right of the screen. */
        SDL_SetWindowPosition(s_window,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

/* ---- Palette refresh ---------------------------------------------- */
/* The Amiga copper list contains "MOVE COLORxx, value" pairs. The value is
 * either 0x0RGB (OCS, 12-bit) or split high/low across two MOVE pairs (AGA).
 * For now we extract OCS-style. AGA support comes when we wire up the
 * copperlist offsets COP_PAL_B*_C*_OFF. */
static void refresh_palette_from_copper(void)
{
    /* DOS graphics use the full 256-colour DOS palette, but ONLY during a
     * match. The intro, title and menu screens are still Amiga graphics and
     * need their normal Amiga (copper) palette — otherwise they render with the
     * wrong colours (olive backgrounds etc.). playGame is set at kickoff and
     * cleared when the match ends; playHighlightFlag does the same for HIL
     * highlight playback, which renders the in-match pitch/players but never
     * sets playGame. Either flag means the DOS pitch is on screen, so we must
     * use the DOS palette. (DOS menu backgrounds are a separate follow-up.) */
    extern volatile uint16_t _playGame, _playHighlightFlag;
    if (_gDosGraphics && (_playGame || _playHighlightFlag)) {
        /* The pitch-condition overlay keys off the low byte of
         * pitchNumberAndType (the high byte is which pitch graphic). */
        extern uint16_t _pitchNumberAndType;
        extern int _gArcadePitchActive;
        extern const uint32_t *Arcade_PitchPaletteArgb(void);
        uint8_t rgb[768];
        DosGfx_BuildPalette(_gArcadePitchActive ? 4 : (int)(_pitchNumberAndType & 0xFF), rgb);
        if (_gArcadePitchActive) {
            /* Themed arcade pitch: keep the DOS sprite/kit colours (slots 0..15) but
             * take the pitch colours (16..255) from the BMP's own palette. */
            const uint32_t *bp = Arcade_PitchPaletteArgb();
            for (int i = 16; i < 256; i++) {
                uint32_t a = bp[i];
                rgb[i * 3 + 0] = (uint8_t)((a >> 16) & 0xFF);
                rgb[i * 3 + 1] = (uint8_t)((a >> 8) & 0xFF);
                rgb[i * 3 + 2] = (uint8_t)(a & 0xFF);
            }
        }
        /* Apply the current fade level so fades work on the DOS palette like the
         * copper path does (g_fadeLevel: 0x80 bright .. 0 black; rgb*level/0x80).
         * DosGfx_BuildPalette rebuilds full-bright RGB each frame, so without
         * this scaling fade in/out had no visible effect on DOS pitches. */
        extern int g_fadeLevel;
        int fl = g_fadeLevel;
        if (fl < 0) fl = 0; else if (fl > 0x80) fl = 0x80;
        for (int i = 0; i < 256; ++i) {
            s_palette[i].r = (uint8_t)((rgb[i * 3 + 0] * fl) >> 7);
            s_palette[i].g = (uint8_t)((rgb[i * 3 + 1] * fl) >> 7);
            s_palette[i].b = (uint8_t)((rgb[i * 3 + 2] * fl) >> 7);
            s_palette[i].a = 255;
        }
        return;
    }
#ifdef AGA
    /* AGA: 256 colours across 8 banks of 32. WriteCopperPalette wrote each
     * bank's 32 value words starting at COP_PAL_Bx_C0_VAL_OFF with a 4-byte
     * stride (copper reg+val pairs), exactly like the OCS block below but for
     * 8 banks. Each value is a 12-bit 0x0RGB word; expand each nibble to 8-bit
     * (*17 maps 0xF→255). Fades are already baked into these copper values. */
    static const uint32_t aga_bank_val_off[8] = {
        COP_PAL_B0_C0_VAL_OFF, COP_PAL_B1_C0_VAL_OFF,
        COP_PAL_B2_C0_VAL_OFF, COP_PAL_B3_C0_VAL_OFF,
        COP_PAL_B4_C0_VAL_OFF, COP_PAL_B5_C0_VAL_OFF,
        COP_PAL_B6_C0_VAL_OFF, COP_PAL_B7_C0_VAL_OFF
    };
    for (int bank = 0; bank < 8; ++bank) {
        const uint8_t *base = _copperListBuffer + aga_bank_val_off[bank];
        for (int j = 0; j < 32; ++j) {
            uint16_t v = *(const uint16_t *)(base + j * 4);
            int idx = bank * 32 + j;
            s_palette[idx].r = (uint8_t)(((v >> 8) & 0x0F) * 17);
            s_palette[idx].g = (uint8_t)(((v >> 4) & 0x0F) * 17);
            s_palette[idx].b = (uint8_t)(((v >> 0) & 0x0F) * 17);
            s_palette[idx].a = 255;
        }
    }
#else
    /* OCS: two palette blocks of 16 colours immediately at known offsets.
     * Each MOVE pair is 4 bytes [reg_word][value_word], we want value_word. */
    const uint16_t *cop = (const uint16_t *)_copperListBuffer;
    /* Bank 0 starts at COP_PAL_B0_C0_VAL_OFF (0x72). Each entry is 4 bytes. */
    const uint16_t *bank0 = (const uint16_t *)(_copperListBuffer + 0x72);
    for (int i = 0; i < 16; ++i) {
        uint16_t v = bank0[i * 2];          /* skip the [reg] word */
        uint8_t  r = (uint8_t)(((v >> 8) & 0x0F) * 17);
        uint8_t  g = (uint8_t)(((v >> 4) & 0x0F) * 17);
        uint8_t  b = (uint8_t)(((v >> 0) & 0x0F) * 17);
        s_palette[i].r = r;
        s_palette[i].g = g;
        s_palette[i].b = b;
        s_palette[i].a = 255;
    }
    (void)cop;
#endif
}

/* ---- 8bpp chunky → RGBA ------------------------------------------- */
/* The frontbuffer is now 1 byte per pixel (palette index). Each frame
 * we walk SCREEN_WIDTH x SCREEN_HEIGHT bytes and look each one up in
 * the current palette to write an ARGB8888 pixel into the streaming
 * SDL texture. Same shape as swos-port-c's `updateScreen`. */
static void chunky_to_rgba(uint32_t *dst, int dst_pitch_pixels)
{
    /* Double-buffer semantics — read the *completed* frame, not the
     * one the game is currently drawing into.
     *
     * `SwapBuffersAndCameraViewState()` runs at each VBlank and flips
     * the two pointers: post-swap, `frontbufferBasePtr` is the next
     * draw target (main thread is about to fill it), and
     * `frontbufferBaseAltPtr` is the just-finished frame. Reading
     * `frontbufferBasePtr` means we're reading the buffer main is
     * writing into — any async timer render or even a slow main-path
     * render will catch it half-complete and tear sprites. Reading
     * `frontbufferBaseAltPtr` gives us a stable completed frame and
     * also eliminates the 1-frame lag that the pre-swap ↔ post-swap
     * pointer aliasing hid. `useAltBaseFlag` inverts this when the
     * game code wants to display the drawing target directly (used by
     * transient overlay paths). */
    const uint8_t *fb;
    if (_usePlaneOffsetFlag != 0)
        fb = _frontbufferMaskBuffer;
    else if (_useAltBaseFlag != 0)
        fb = (const uint8_t *)(uintptr_t)_frontbufferBasePtr;
    else
        fb = (const uint8_t *)(uintptr_t)_frontbufferBaseAltPtr;
    if (!fb) return;

    /* Originally applied the Amiga copper Y offset:
     *     fb += cameraYMod320 * FRONTBUFFER_ROW_STRIDE_BYTES;
     * That simulates the Amiga's bitplane-base-pointer vertical scroll,
     * which the match code uses alongside incremental scroll-blits into
     * the wrap region (lines 200–272 of the 273-line frontbuffer).
     *
     * On SDL we now redraw the entire visible pitch to lines 0–199
     * each frame from the chunky tile bank (pitch.c:DrawChunkyPitch),
     * so applying cameraYMod320 here compounds two different scroll
     * schemes and produces "two pitch views stitched vertically"
     * flicker. Always read from line 0 on SDL; cameraYMod320 is
     * irrelevant to the chunky frontbuffer. */
    (void)_cameraYMod320;

    for (int y = 0; y < PLATFORM_SCREEN_H; ++y) {
        const uint8_t *line = fb + (size_t)y * FRONTBUFFER_ROW_STRIDE_BYTES;
        uint32_t      *out  = dst + (size_t)y * dst_pitch_pixels;
        for (int x = 0; x < _screen_width; ++x) {
            SDL_Color c = s_palette[line[x]];
            out[x] = ((uint32_t)0xFFu << 24) |
                     ((uint32_t)c.r  << 16) |
                     ((uint32_t)c.g  <<  8) |
                     ((uint32_t)c.b  <<  0);
        }
    }
}

/* Optional debug dump: every N frames write the converted chunky
 * frame to /tmp/swos_frame_NNNN.ppm. Enabled when the env var
 * SWOS_DUMP_FRAMES is set. Useful for headless smoke-testing. */
static int bytes_nonzero(const uint8_t *p, int n)
{
    int k = 0;
    for (int i = 0; i < n; ++i) if (p[i]) ++k;
    return k;
}

/* F12 screenshot writer. Called from Platform_RenderFrame while the
 * render mutex is held, so the ARGB buffer is guaranteed consistent.
 * Writes to `swos_shotNNN.png` in the current working directory.
 * PNG (via stb_image_write) is used rather than BMP so review tools
 * can read the file directly without an external converter step. */
extern int stbi_write_png(char const *filename, int w, int h, int comp,
                          const void *data, int stride_in_bytes);

static void write_screenshot(const uint32_t *argb)
{
    /* stb_image_write wants RGBA byte order in memory. Our ARGB8888 SDL
     * buffer stores pixels as 0xAARRGGBB uint32, which on LE is bytes
     * BB,GG,RR,AA — i.e. BGRA in memory. Convert to RGBA before write. */
    const int N = _screen_width * PLATFORM_SCREEN_H;
    uint8_t *rgba = (uint8_t *)malloc((size_t)N * 4);
    if (!rgba) {
        fprintf(stderr, "[screenshot] malloc failed\n");
        return;
    }
    for (int i = 0; i < N; ++i) {
        uint32_t p = argb[i];
        rgba[i * 4 + 0] = (uint8_t)(p >> 16); /* R */
        rgba[i * 4 + 1] = (uint8_t)(p >>  8); /* G */
        rgba[i * 4 + 2] = (uint8_t)(p >>  0); /* B */
        rgba[i * 4 + 3] = 0xFF;               /* A (opaque) */
    }
    char path[64];
    snprintf(path, sizeof(path), "swos_shot%03d.png", s_screenshot_index++);
    if (stbi_write_png(path, _screen_width, PLATFORM_SCREEN_H, 4,
                       rgba, _screen_width * 4))
        fprintf(stderr, "[screenshot] wrote %s\n", path);
    else
        fprintf(stderr, "[screenshot] stbi_write_png %s failed\n", path);
    free(rgba);
}

static void maybe_dump_frame(const uint32_t *argb)
{
    if (!s_dump_frames) return;
    int n = s_frame_count++;
    if ((n & 0x0F) != 0) return;            /* every 16th frame */
    if (n > 16 * 24) return;                 /* cap at ~24 dumps */
    char path[64];
    snprintf(path, sizeof(path), "/tmp/swos_frame_%04d.ppm", n);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", _screen_width, PLATFORM_SCREEN_H);
    for (int i = 0; i < _screen_width * PLATFORM_SCREEN_H; ++i) {
        uint32_t p = argb[i];
        uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);

    /* Single-line summary: count distinct colors on a mid-screen row. */
    int d = 0;
    uint32_t seen[16] = {0}; int sn = 0;
    const uint32_t *mid = argb + (PLATFORM_SCREEN_H / 2) * _screen_width;
    for (int i = 0; i < _screen_width; ++i) {
        uint32_t p = mid[i] & 0xFFFFFF;
        int found = 0;
        for (int j = 0; j < sn; ++j) if (seen[j] == p) { found = 1; break; }
        if (!found && sn < 16) { seen[sn++] = p; ++d; }
    }
    fprintf(stderr, "[dump] frame=%d mid-row-distinct=%d\n", n, d);
}

/* ---- Per-frame pump (no pacing). May be called from any thread.
 * The s_render_mtx serializes the renderer + texture access between
 * the game (main) thread and the SDL_AddTimer thread. */
/* Drain SDL's event queue. Called from both the main thread (via
 * Platform_VBlank from WaitForVSync) AND the SDL_Timer thread. The
 * loader-intro spin loops on currentGameTick and never touches
 * WaitForVSync, so main-thread-only polling misses the X-button
 * SDL_QUIT during the intro. In practice SDL_PollEvent is safe to
 * call from multiple threads: the queue is internally locked, events
 * are consumed by whichever pump grabs them first, and Platform_RequestQuit
 * is safe from any thread. */
static void pump_events_any_thread(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            Platform_RequestQuit();
            /* Hide the window immediately so the main-menu unwind
             * after a mid-match X-press happens off-screen instead
             * of flashing the menu UI for a frame or two. The audio
             * gate in TryPlayMusic stops the menu music from
             * starting; this kills the visual half. */
            if (s_window) SDL_HideWindow(s_window);
            /* Pause + clear audio at the driver. SDL queues ~80-250 ms
             * of samples ahead of the playback cursor, so without this
             * the crowd chant or whatever's playing leaks past the X
             * click while the game loop unwinds. */
            AudioSDL_PauseAndClear();
            break;
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                Platform_RequestQuit();
                if (s_window) SDL_HideWindow(s_window);
                AudioSDL_PauseAndClear();
            } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                /* Alt-Tab takes focus before SDL delivers KEYUP for the
                 * Alt that triggered the switch — so without clearing
                 * here our keyState[AKEY_LALT] / keyBuffer FIFO entry
                 * stay "pressed" indefinitely. On focus return the
                 * loader-intro key-skip and the menu fire-poll see that
                 * stuck press and (a) skip the intro and (b) fire the
                 * highlighted menu entry. Drop everything; new presses
                 * after focus returns will repopulate via KEYDOWN. */
                InputSDL_ClearKeyState();
            } else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                /* Defence-in-depth: also clear on regain in case any
                 * KEYDOWN slipped in between FOCUS_LOST and the actual
                 * loss of input focus. Cheap to do. */
                InputSDL_ClearKeyState();
            }
            break;
        case SDL_KEYDOWN:
            /* ESC at the top-level main menu quits the game; everywhere
             * else it stays passed through to the menu/game-loop layer
             * (in-game pause/end-match menu, sub-menus' EXIT, etc.). */
            if (ev.key.repeat == 0 && ev.key.keysym.sym == SDLK_ESCAPE) {
                extern volatile uintptr_t _currentMenuPtr;
                extern uint8_t _swosMainMenu[];
                if (_currentMenuPtr == (uintptr_t)_swosMainMenu) {
                    Platform_RequestQuit();
                    if (s_window) SDL_HideWindow(s_window);
                    AudioSDL_PauseAndClear();
                    continue;   /* swallow — don't also reach the menu */
                }
            }
            if (ev.key.repeat == 0 &&
                (ev.key.keysym.sym == SDLK_F9 ||
                 ev.key.keysym.sym == SDLK_F11 ||
                 ev.key.keysym.sym == SDLK_F12 ||
                 ev.key.keysym.sym == SDLK_PRINTSCREEN)) {
                s_screenshot_request = 1;
            }
            /* Alt+Enter: toggle fullscreen. Swallow the event so it
             * doesn't also reach the menu input layer as ENTER (which
             * would fire the currently-selected button). */
            if (ev.key.repeat == 0 &&
                (ev.key.keysym.sym == SDLK_RETURN ||
                 ev.key.keysym.sym == SDLK_KP_ENTER) &&
                (ev.key.keysym.mod & KMOD_ALT)) {
                Platform_ToggleFullscreen();
                continue;
            }
            /* +/- keys: cycle the windowed scale 1..4. Both the typing-
             * row keys and the keypad ones work. SDLK_EQUALS is the
             * same physical key as SDLK_PLUS on most layouts (US),
             * so we accept either. Swallowed so they don't reach the
             * menu/game layer. No-op while fullscreen (the runtime
             * scaler ignores the size change). */
            if (ev.key.repeat == 0 &&
                (ev.key.keysym.sym == SDLK_EQUALS ||
                 ev.key.keysym.sym == SDLK_PLUS   ||
                 ev.key.keysym.sym == SDLK_KP_PLUS)) {
                Platform_SetWindowScale(s_win_scale_current + 1);
                continue;
            }
            if (ev.key.repeat == 0 &&
                (ev.key.keysym.sym == SDLK_MINUS ||
                 ev.key.keysym.sym == SDLK_KP_MINUS)) {
                Platform_SetWindowScale(s_win_scale_current - 1);
                continue;
            }
            /* Arcade cabinet inputs (MAME convention): 5 = coin P1, 6 = coin P2,
             * F2 = service, 1 = P1 start, 2 = P2 start. Only when arcade mode is
             * on; swallowed so they don't also reach the game/menu layer. */
            {
                extern int _gArcadeMode;
                if (ev.key.repeat == 0 && _gArcadeMode) {
                    SDL_Keycode k = ev.key.keysym.sym;
                    if (k == s_arcade_key_p1_credit) { s_arcade_coin1++;   continue; }
                    if (k == s_arcade_key_p2_credit) { s_arcade_coin2++;   continue; }
                    if (k == s_arcade_key_service)   { s_arcade_service++; continue; }
                    if (k == s_arcade_key_p1_start)  { s_arcade_start1++;  continue; }
                    if (k == s_arcade_key_p2_start)  { s_arcade_start2++;  continue; }
                }
            }
            /* fallthrough */
        case SDL_KEYUP:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERAXISMOTION:
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_JOYDEVICEADDED:
        case SDL_JOYDEVICEREMOVED:
            InputSDL_HandleEvent(&ev);
            break;
        default:
            break;
        }
    }
    InputSDL_PostFrame();
}

/* Lightweight pump used by the tick timer when the main thread is
 * rendering — only services window/quit events and hotkeys. Leaves game
 * input (keydown/keyup into keyState + InputSDL_PostFrame) to the main
 * thread so the 50 Hz timer thread doesn't race with it. */
static void pump_quit_only(void)
{
    SDL_Event ev;
    while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT,
                          SDL_QUIT, SDL_QUIT) > 0) {
        Platform_RequestQuit();
        AudioSDL_PauseAndClear();
    }
    while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT,
                          SDL_WINDOWEVENT, SDL_WINDOWEVENT) > 0) {
        if (ev.window.event == SDL_WINDOWEVENT_CLOSE)
            Platform_RequestQuit();
    }
}

/* ---- Sub-pixel scroll presentation ---------------------------------------
 * The game draws at the INTEGER camera position, so a true scroll velocity of
 * 1.5 px/frame renders as an alternating 1,2,1,2 px step (38% of consecutive
 * moving frames change step size — measured on the corpus). Shifting the
 * finished frame by the fractional remainder makes that motion continuous.
 * Everything on screen is drawn relative to the camera, so ONE uniform shift
 * is correct for the whole scene; camera.c publishes the residual.
 *
 * TWO things this must not break:
 *
 * 1. ONE upload, ONE main copy. A previous release tanked on Windows/D3D
 *    because of how the frame reached the GPU; the fix was the single bulk
 *    SDL_UpdateTexture from cached RAM above. This adds no upload and no
 *    per-pixel work — just a float dest rect on the same copy.
 *
 * 2. NO BLUR. We render nearest-neighbour (SDL_HINT_RENDER_SCALE_QUALITY "0")
 *    and a fractional dest under nearest would either snap back to whole
 *    source pixels or, with linear filtering, smear the pixel art. So the
 *    shift is QUANTISED TO WHOLE OUTPUT PIXELS: at integer scale N we get
 *    N steps per source pixel, exactly representable, still crisp. On a 4-5x
 *    window that is a 4-5x finer scroll than today for free.
 *
 * The shift uncovers a sliver at the trailing edge (< 1 source pixel wide).
 * It is filled by re-copying the edge column/row into it — clamp-to-edge, and
 * visually indistinguishable from the neighbouring pixels. Stretching the main
 * rect instead would make nearest duplicate an interior column whose position
 * moves with the fraction, i.e. a shimmer. */
/* camera.c, SDL-only — declared here rather than pulling in game_types.h,
 * which this file deliberately does not include. */
extern int Render_ScrollSubpixel(uint16_t *fx, uint16_t *fy);

static void present_frame_copy(SDL_Texture *tex)
{
    const float w = (float)_screen_width;
    const float h = (float)PLATFORM_SCREEN_H;
    uint16_t fx = 0, fy = 0;
    float qx = 0.0f, qy = 0.0f;

    if (s_subpixel_scroll && Render_ScrollSubpixel(&fx, &fy) && (fx || fy)) {
        /* Logical->output scale, as set by SDL_RenderSetLogicalSize. This is
         * the resolution we have to work with: at scale N a source pixel is N
         * output pixels, so the shift can only take N distinct values.
         *
         * Below 2x there is NO sub-pixel resolution — the quantiser could only
         * return 0 or a whole pixel, which does not smooth anything. It merely
         * rounds the camera instead of flooring it, and rounding shifts the
         * phase of the 1,2,1,2 pattern without making it any more even, while
         * still costing the trailing-edge sliver. So leave 1x completely alone
         * and take the untouched SDL_RenderCopy path. */
        float sx = 1.0f, sy = 1.0f;
        SDL_RenderGetScale(s_renderer, &sx, &sy);
        if (sx >= 2.0f)
            qx = SDL_floorf((float)fx / 65536.0f * sx + 0.5f) / sx;
        if (sy >= 2.0f)
            qy = SDL_floorf((float)fy / 65536.0f * sy + 0.5f) / sy;
    }

    if (qx == 0.0f && qy == 0.0f) {
        SDL_RenderCopy(s_renderer, tex, NULL, NULL);   /* unchanged fast path */
        return;
    }

    /* The frame was drawn at floor(camera); the true camera is that much
     * further along, so the picture belongs that much further BACK. */
    SDL_FRect dst = { -qx, -qy, w, h };
    SDL_RenderCopyF(s_renderer, tex, NULL, &dst);

    if (qx > 0.0f) {                    /* right sliver: clamp the last column */
        SDL_Rect  src = { _screen_width - 1, 0, 1, PLATFORM_SCREEN_H };
        SDL_FRect d   = { w - qx, -qy, qx, h + qy };
        SDL_RenderCopyF(s_renderer, tex, &src, &d);
    }
    if (qy > 0.0f) {                    /* bottom sliver: clamp the last row */
        SDL_Rect  src = { 0, PLATFORM_SCREEN_H - 1, _screen_width, 1 };
        SDL_FRect d   = { -qx, h - qy, w + qx, qy };
        SDL_RenderCopyF(s_renderer, tex, &src, &d);
    }
}

static void render_frame_impl(int pump_full)
{
    /* SDL_PollEvent -> SDL_PumpEvents pulls from the OS window message queue, which on
     * Windows MUST run on the window-owning (main) thread. The SDL_AddTimer worker calls
     * this as a software-renderer fallback (tick_timer_cb) when the main thread stalls
     * >60ms, so off the main thread we pass pump_full=0 and only drain already-queued
     * QUIT/WINDOW events (pump_quit_only / SDL_PeepEvents, thread-safe). Doing the full pump
     * from the worker deadlocks the main thread; an F12 screenshot (which holds s_render_mtx
     * across the blocking PNG file write, stalling main past 60ms) was the reliable trigger. */
    if (pump_full)
        pump_events_any_thread();
    else
        pump_quit_only();
#ifdef SWOS_WASM_BUILD
    /* The ONLY audio pump on WASM (no timer/worker thread), once per jittery
     * emscripten_sleep frame. AudioSDL_Tick mixes one 20 ms tick and early-
     * returns at target, and its silence-pad path is compiled out here — so call
     * it repeatedly to fill the queue to QUEUE_TARGET_MS with REAL audio. The
     * 16 cap (>QUEUE_TARGET_MS/20) bounds the work; extra calls just no-op. */
    for (int i = 0; i < 16; ++i) AudioSDL_Tick();
#else
    AudioSDL_Tick();
#endif

    if (!s_renderer || !s_textures[0]) return;

    SDL_LockMutex(s_render_mtx);

    /* Upload into the next texture in the ring (not the one the GPU is still
     * sampling from a recent frame) — see the s_textures[] comment. */
    SDL_Texture *tex = s_textures[s_tex_idx];
    s_tex_idx = (s_tex_idx + 1) % PLATFORM_TEX_RING;

    /* Render the current Amiga framebuffer INTO CACHED RAM (s_frame_rgba), never
     * into the texture's mapped memory — that's the D3D11 write-combined trap. The
     * buffer is tightly packed so the row pitch in pixels is exactly _screen_width. */
    refresh_palette_from_copper();

    uint32_t *fb   = s_frame_rgba;
    const int fbpx = _screen_width;              /* row pitch, in pixels */

    /* Arcade flag-select / service screens render a full RGBA frame; when one is
     * active it replaces the normal 8bpp path. Otherwise convert the framebuffer
     * and (in attract) composite the logo/INSERT-COIN overlay. */
    int arcade_full = 0;
    { extern int _gArcadeMode;
      extern int Arcade_RenderFullScreen(uint32_t *, int, int, int);
      if (_gArcadeMode)
          arcade_full = Arcade_RenderFullScreen(fb, fbpx, _screen_width, PLATFORM_SCREEN_H); }
    if (!arcade_full) {
        chunky_to_rgba(fb, fbpx);
        { extern int _gArcadeMode;
          extern void Arcade_CompositeOverlay(uint32_t *, int, int, int);
          if (_gArcadeMode)
              Arcade_CompositeOverlay(fb, fbpx, _screen_width, PLATFORM_SCREEN_H); }
    }

    /* Screenshots read the packed RAM buffer directly (always the right stride now,
     * so the old pitch-mismatch skip is gone). Only write the F12 shot from the
     * main-thread render (pump_full); the timer-thread fallback would race the draw. */
    if (s_dump_frames)
        maybe_dump_frame(fb);
    if (pump_full && s_screenshot_request) {
        write_screenshot(fb);
        s_screenshot_request = 0;
    }
    if (s_screenshot_interval > 0) {
        static int frame_mod;
        if (++frame_mod >= s_screenshot_interval) {
            frame_mod = 0;
            if (s_screenshot_auto_count < 200) {
                write_screenshot(fb);
                s_screenshot_auto_count++;
            } else if (s_screenshot_auto_count == 200) {
                fprintf(stderr,
                    "[screenshot] interval mode hit 200-shot cap; disabling\n");
                s_screenshot_interval = 0;
                s_screenshot_auto_count++;
            }
        }
    }

    /* Hand the finished frame to the GPU in ONE bulk upload from cached RAM. */
    SDL_UpdateTexture(tex, NULL, fb, _screen_width * 4);

    SDL_RenderClear(s_renderer);
    present_frame_copy(tex);
    SDL_RenderPresent(s_renderer);

    SDL_UnlockMutex(s_render_mtx);
}

void Platform_RenderFrame(void)
{
    render_frame_impl(1);   /* public entry: runs on the main thread, full SDL event pump */
}

/* ---- Full vblank: render + 50 Hz pacing ---------------------------
 * Called from WaitForVSync on the game/main thread. Pumps SDL events
 * here (main thread only) before rendering the frame. */
int Platform_VBlank(void)
{
    /* Drive the Amiga Level-3 ISR from the main thread too, in case the
     * SDL_Timer worker thread is starved (common on WSL2 where main-
     * thread spin loops can monopolise CPU). The throttle inside the
     * wrapper means double-firing from both threads is harmless. */
    RunInt3HandlerThrottled();

    /* Stamp first so the async timer callback sees a recent main-thread
     * render as soon as we enter RenderFrame — prevents the timer from
     * firing a concurrent render while we're holding the mutex. */
    s_last_main_render_ms = SDL_GetTicks();
    Platform_RenderFrame();

#ifdef SWOS_WASM_BUILD
    /* The browser owns the event loop, so a C program must hand control back
     * every frame or the tab freezes. ASYNCIFY rewinds the C stack here and
     * resumes later — turning "render this frame, return to the browser" out of
     * the same blocking loop the native build runs. This ONE yield paces every
     * blocking loop in the game (match loop, menus, fades, replays), so no
     * per-loop rewrite is needed.
     *
     * Pace to a FIXED frame period that ABSORBS the frame's own work time. A plain
     * emscripten_sleep(period) sleeps the whole period AFTER rendering, so every
     * frame actually took work + period and the real rate fell well below target
     * (~35 fps). And because WASM has no timer thread, the game clock
     * (_currentGameTick) advances exactly once per Platform_VBlank — so a low frame
     * rate makes the whole game run in slow-motion. Sleep only (deadline - now), with
     * an accumulating deadline that self-corrects for setTimeout jitter; if a slow
     * frame overruns, resync instead of building up a backlog (no spiral of death). */
    {
        double period = 1000.0 / Platform_ActivePacingFps();
        static double s_frame_deadline;
        double now = emscripten_get_now();
        if (s_frame_deadline <= 0.0) s_frame_deadline = now;   /* first frame */
        s_frame_deadline += period;
        double sleep_ms = s_frame_deadline - now;
        if (sleep_ms < 1.0) {                 /* work overran the frame budget */
            sleep_ms = 1.0;                   /* still yield so the browser paints + polls input */
            s_frame_deadline = now + period;  /* drop the backlog */
        } else if (sleep_ms > period) {       /* first frame / clock jumped back */
            sleep_ms = period;
            s_frame_deadline = now + period;
        }
        emscripten_sleep((unsigned)(sleep_ms + 0.5));
    }
    return _quitToDos ? 1 : 0;
#endif

    /* 50 Hz pacing — wait until the next frame boundary on the
     * high-resolution counter. Hybrid sleep + spin: SDL_Delay() for the
     * bulk of the wait (cheap on CPU, releases the core), then a busy
     * spin for the last ~2 ms to land on the deadline accurately. Even
     * with SDL_HINT_TIMER_RESOLUTION=1, SDL_Delay can still wake up
     * 1–2 ms late depending on what the OS scheduler is doing, and that
     * jitter is exactly what looks like "uneven framerate" to a player.
     *
     * Catch-up cap: if we fell behind by more than one frame (e.g. the
     * main thread blocked on a slow asset load), reset the deadline to
     * "now" instead of trying to render N frames back-to-back to catch
     * up — which would just make the next batch of frames feel even
     * more uneven. The 50 Hz audio tick is locked to the same loop so
     * skipping a catch-up keeps music time-aligned with game state. */
    /* Turbo skips the 50 Hz real-time wait entirely: under gdb we drive the
     * inferior frame-by-frame and want frames CPU-bound, not paced to a wall
     * clock the WSL2 scheduler stretches unpredictably. Skipping the
     * SDL_Delay + spin removes the per-frame real-time cost that
     * "timer-thread starvation" was ballooning to 10–120 s per cont. */
    if (s_harness_turbo)
        return _quitToDos ? 1 : 0;

    /* Read the frame rate live so an in-game DOS-gameplay toggle — or a loaded
     * replay's own recorded framerate — re-paces each frame. */
    k_frame_secs = 1.0 / (double)Platform_ActivePacingFps();
    s_next_frame_perf += (Uint64)((double)s_perf_freq * k_frame_secs);
    Uint64 now = SDL_GetPerformanceCounter();

    if ((Sint64)(now - s_next_frame_perf) > (Sint64)((double)s_perf_freq * k_frame_secs)) {
        /* More than a full frame behind — give up on catching up. */
        s_next_frame_perf = now;
    } else {
        /* Sleep almost-to-deadline, leaving a 2 ms margin for the spin. */
        const Uint64 spin_margin = (Uint64)((double)s_perf_freq * 0.002);
        if (s_next_frame_perf > now + spin_margin) {
            double secs = (double)(s_next_frame_perf - now - spin_margin)
                          / (double)s_perf_freq;
            Uint32 ms = (Uint32)(secs * 1000.0);
            if (ms > 0) SDL_Delay(ms);
        }
        /* Spin out the remainder. SDL_Delay typically wakes 0–2 ms late
         * with TIMER_RESOLUTION=1; this loop is at most ~2 ms of spin. */
        while (SDL_GetPerformanceCounter() < s_next_frame_perf) {
            /* tight loop — SDL_GetPerformanceCounter is a CPU register read */
        }
    }

    return _quitToDos ? 1 : 0;
}

Uint64 Platform_NowMs(void)
{
    return SDL_GetTicks64();
}
