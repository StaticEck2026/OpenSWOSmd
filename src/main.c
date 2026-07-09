/*
 * main.c -- SDL2 entry point for the SWOS C/SDL port.
 *
 * Original Amiga main.c just called startShim() which jumped to the
 * Amiga assembly bootstrap. We replace that with an SDL2 host that:
 *   1. Initializes SDL video / audio / events / gamepad
 *   2. Creates the window + renderer + a 336x272 paletted texture
 *   3. Calls into the (ported) Amiga _start / mainC bootstrap
 *
 * Graphics: the game still draws into the Amiga 4/8-bitplane framebuffers.
 * Each frame, we convert those bitplanes to chunky 8-bit pixels and upload
 * to an SDL streaming texture.
 *
 * Audio:    SDL_OpenAudioDevice + a callback that mixes the four virtual
 *           Amiga audio channels (handled by audio_sdl.c).
 *
 * Input:    SDL_Event polling translated into the Amiga keyboard / joystick
 *           state bytes the original ISRs would have written (input_sdl.c).
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>          /* SIGHUP — see main() (Cygwin-under-gdb startup) */

#ifdef _WIN32
#include <windows.h>
#include <direct.h>          /* _chdir */
#define chdir(p) _chdir(p)
#else
#include <unistd.h>          /* readlink, chdir */
#endif

#include "platform_sdl.h"
#include "swos_settings.h"
#include "swos_io.h"            /* swos_save_dir_init() — macOS-writable save dir */
#include "save_format_json.h"   /* DataJson_SelfTest */
#include "dos_mode.h"   /* gDosGraphics/gDosGameplay — defined in dos_graphics.c */
#include "display.h"    /* _screen_width, WIDESCREEN_WIDTH, SCREEN_WIDTH */

#define WIN_SCALE 3

extern void startShim(void);

/* Anchor CWD to the directory containing the binary. The asset and
 * save paths are all relative ("data/foo.dat", "grafs/...", "save/X.CAR")
 * and assume the game directory is the working directory — matching
 * the Amiga build that always ran from its own folder. Without this,
 * launching SWOS from anywhere other than bin/ would miss every asset. */
static void chdir_to_binary(const char *argv0)
{
    char path[4096];
    char *dir;
    int got_path = 0;

#ifdef _WIN32
    /* Windows: GetModuleFileNameA(NULL, ...) returns the running .exe path. */
    DWORD n = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path) - 1);
    if (n > 0 && n < sizeof(path)) {
        path[n] = 0;
        got_path = 1;
    }
#else
    /* Linux: /proc/self/exe is a symlink to the running binary. */
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n > 0) {
        path[n] = 0;
        got_path = 1;
    }
#endif

    if (!got_path) {
        if (!argv0) return;
        /* Fallback: copy argv[0] (may be relative). */
        snprintf(path, sizeof(path), "%s", argv0);
    }

    /* Strip the trailing component. Windows accepts both '/' and '\\'
     * in paths, and GetModuleFileNameA returns backslashes — handle
     * either, taking whichever appears later in the string. */
    {
        char *slash = strrchr(path, '/');
#ifdef _WIN32
        char *bslash = strrchr(path, '\\');
        if (bslash && (!slash || bslash > slash))
            slash = bslash;
#endif
        dir = slash;
    }
    if (!dir)
        return;
    *dir = 0;
    if (chdir(path) != 0) {
        fprintf(stderr, "warning: chdir(%s) failed\n", path);
    }
}

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -f, --fullscreen          Shorthand for --fullscreen=desktop. Off by\n"
        "                            default. Toggle at runtime with Alt+Enter.\n"
        "  --fullscreen=<desktop|exclusive>\n"
        "                            Fullscreen mode at startup.\n"
        "                            'desktop' = borderless letterbox, no display-\n"
        "                            mode change (default flavour). On Windows XP\n"
        "                            the taskbar can sometimes show through this\n"
        "                            borderless window.\n"
        "                            'exclusive' = true fullscreen with display-\n"
        "                            mode change, always covers the taskbar (the\n"
        "                            XP fix). Slower toggle.\n"
        "  --scale=<1|2|3|4>         Windowed scale at startup. Default: 3.\n"
        "                            Adjust at runtime with + and - keys.\n"
        "  --renderer=<software|accel>\n"
        "                            Choose SDL renderer. Default: accel.\n"
        "                            'accel' uses the platform's hardware backend\n"
        "                            (D3D/OpenGL/Metal); 'software' is a CPU-only\n"
        "                            fallback for hosts where the hardware path\n"
        "                            misbehaves.\n"
        "  --vsync=<on|off>          Sync frame presents to monitor vblank.\n"
        "                            Default: off. Only applies to --renderer=accel.\n"
        "                            Turn on if you see screen tearing; the manual\n"
        "                            50 Hz game-tick pace runs as a backstop\n"
        "                            regardless.\n"
        "  --audio=<mono|stereo>     Audio output mode. Default: mono.\n"
        "                            The Amiga's 4 audio channels are hard-panned\n"
        "                            (1,2 → left, 0,3 → right) which sounds\n"
        "                            lopsided on most modern playback setups.\n"
        "                            'stereo' restores the original Amiga panning.\n"
        "  --dos-graphics=<on|off>   Render the DOS-version graphics (bin/grafs_dos)\n"
        "                            instead of the Amiga graphics. Default: off.\n"
        "                            PC-only extra; the Amiga look is the default.\n"
        "  --dos-gameplay=<on|off>   DOS gameplay tweaks (framerate etc.). Default:\n"
        "                            off. Reserved — not wired up yet.\n"
        "  --dos-dir=<folder>        Folder (under bin/) to load DOS assets from.\n"
        "                            Default: grafs_dos.\n"
        "  -h, --help                Show this help and exit.\n",
        argv0 ? argv0 : "SWOS");
}

/* Parse `--key=value` from `arg` against expected `key`. Returns the
 * value-side string, or NULL if `arg` doesn't start with `--key=`. */
static const char *match_kv(const char *arg, const char *key)
{
    size_t klen = strlen(key);
    if (strncmp(arg, "--", 2) != 0) return NULL;
    if (strncmp(arg + 2, key, klen) != 0) return NULL;
    if (arg[2 + klen] != '=') return NULL;
    return arg + 2 + klen + 1;
}

int main(int argc, char **argv)
{
#ifdef SIGHUP
    /* Cygwin under the VS Code gdb launcher wires stdio as pipes (not a Cygwin
     * pty), so at startup the runtime sees the controlling terminal "hang up"
     * and delivers SIGHUP — whose default action kills us before the window
     * even opens (gdb reports "terminated with signal SIGHUP", exit code 0).
     * This is a GUI app and doesn't need the terminal, so ignore it. No effect
     * on a normal run, which never receives SIGHUP. */
    signal(SIGHUP, SIG_IGN);
#endif

    /* Anchor CWD to the binary's directory FIRST so the data/ grafs/ sound/
     * trees resolve relative to the exe regardless of where the user launched
     * from. (SWOS.ini and SWOS.log no longer live here — they go to the per-user
     * game folder; see swos_save_dir_init() + the freopen below.)
     * WASM: there is no binary dir — the data is --preload-file'd into MEMFS at
     * /data, /grafs, … and cwd is already "/", so skip the chdir. */
#ifndef SWOS_WASM_BUILD
    chdir_to_binary(argv ? argv[0] : NULL);
#endif

    /* Resolve the per-user game folder + save dir NOW, before Settings_Load, so
     * SWOS.ini reads/writes from the same per-user location as the save/ folder
     * (~/.swos, Documents/SWOS, ~/Library/Application Support/SWOS). Creates the
     * folder if missing; if it can't be resolved, falls back to the exe dir.
     * (Was called later, after SDL_Init — moved up so the ini lookup uses it.) */
    swos_save_dir_init();

#ifdef SWOS_SDL_BUILD
    /* Data-file JSON loader self-test (docs/DATA_INTERNAL_JSON.md). Runs before
     * any SDL/game init and exits — `SWOS_DATA_JSON_SELFTEST=1 ./bin/SWOS`. */
    if (getenv("SWOS_DATA_JSON_SELFTEST"))
        return DataJson_SelfTest();
#endif

    /* 1) Hard-coded defaults — used when SWOS.ini doesn't exist yet
     * and no CLI flags override. */
    PlatformInitOptions opts;
    opts.win_scale  = WIN_SCALE;
    opts.fullscreen = PLATFORM_FULLSCREEN_NONE;
    opts.renderer   = PLATFORM_RENDERER_ACCEL;
    opts.vsync      = 0;          /* default off — opt-in only */
    opts.res_w = 0; opts.res_h = 0;  /* 0,0 = auto (fill the desktop aspect) */
    opts.screen = 0;                 /* target display index (0 = primary) */
    opts.audio_mono = 1;
    opts.dos_graphics = 0;        /* default Amiga graphics */
    opts.dos_gameplay = 0;        /* default Amiga gameplay */
    opts.arcade = 0;              /* default off — normal front-end menu */
    opts.arcade_mode = 0;         /* GOLDEN GOAL */
    opts.arcade_game_seconds = 45;
    opts.arcade_golden_goal = 1;
    opts.arcade_penalties = 1;
    opts.arcade_goal_replay = 1;
    opts.arcade_attract_audio = 2;   /* music+sfx (HERO + reel crowd/SFX mixed) */
    opts.arcade_gameplay = 0;        /* amiga gameplay feel */
    snprintf(opts.arcade_key_p1_credit, sizeof opts.arcade_key_p1_credit, "%s", "5");
    snprintf(opts.arcade_key_p1_start,  sizeof opts.arcade_key_p1_start,  "%s", "1");
    snprintf(opts.arcade_key_p2_credit, sizeof opts.arcade_key_p2_credit, "%s", "6");
    snprintf(opts.arcade_key_p2_start,  sizeof opts.arcade_key_p2_start,  "%s", "2");
    snprintf(opts.arcade_key_service,   sizeof opts.arcade_key_service,   "%s", "F2");
    snprintf(opts.dos_dir, sizeof opts.dos_dir, "%s", "grafs_dos");
    snprintf(opts.language, sizeof opts.language, "%s", "en");
    snprintf(opts.currency, sizeof opts.currency, "%s", "pound");

#ifdef SWOS_WASM_BUILD
    /* Web defaults (SWOS.ini still overrides these): native render scale — the
     * canvas CSS upscales to the viewport, so a high SDL win_scale just wastes
     * GPU — and vsync off (we pace via emscripten_sleep, not SDL vsync). */
    opts.win_scale = 1;
    opts.vsync     = 0;
#endif

    /* 2) SWOS.ini (in the per-user game folder — swos_ini_path()) overrides
     * defaults. On the web it's copied from localStorage into /SWOS.ini by
     * wasm/pre.js before we get here (edit it via the in-page settings panel).
     * Missing file is fine — defaults are used, and step 4 writes a fresh one. */
    const char *ini_path = swos_ini_path();
    Settings_Load(&opts, ini_path);

    /* Expose the live options + ini path to the arcade service menu so its
     * settings (game rules) can be written straight back to SWOS.ini. */
    { extern PlatformInitOptions *g_arcadeOpts; extern const char *g_arcadeIniPath;
      g_arcadeOpts = &opts; g_arcadeIniPath = ini_path; }

    /* 3) CLI overrides INI (highest precedence — explicit user intent). */
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        const char *v;
        if (strcmp(a, "-f") == 0 || strcmp(a, "--fullscreen") == 0) {
            opts.fullscreen = PLATFORM_FULLSCREEN_DESKTOP;
        } else if ((v = match_kv(a, "fullscreen")) != NULL) {
            if (strcmp(v, "desktop") == 0)        opts.fullscreen = PLATFORM_FULLSCREEN_DESKTOP;
            else if (strcmp(v, "exclusive") == 0) opts.fullscreen = PLATFORM_FULLSCREEN_EXCLUSIVE;
            else if (strcmp(v, "off") == 0)       opts.fullscreen = PLATFORM_FULLSCREEN_NONE;
            else {
                fprintf(stderr, "Invalid --fullscreen value '%s' (expected desktop|exclusive|off)\n", v);
                return 1;
            }
        } else if ((v = match_kv(a, "scale")) != NULL) {
            int n = atoi(v);
            if (n < 1 || n > 4) {
                fprintf(stderr, "Invalid --scale value '%s' (expected 1|2|3|4)\n", v);
                return 1;
            }
            opts.win_scale = n;
        } else if ((v = match_kv(a, "renderer")) != NULL) {
            if (strcmp(v, "software") == 0)      opts.renderer = PLATFORM_RENDERER_SOFTWARE;
            else if (strcmp(v, "accel") == 0)    opts.renderer = PLATFORM_RENDERER_ACCEL;
            else {
                fprintf(stderr, "Invalid --renderer value '%s' (expected software|accel)\n", v);
                return 1;
            }
        } else if ((v = match_kv(a, "vsync")) != NULL) {
            if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0)       opts.vsync = 1;
            else if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0) opts.vsync = 0;
            else {
                fprintf(stderr, "Invalid --vsync value '%s' (expected on|off)\n", v);
                return 1;
            }
        } else if ((v = match_kv(a, "audio")) != NULL) {
            if (strcmp(v, "mono") == 0)        opts.audio_mono = 1;
            else if (strcmp(v, "stereo") == 0) opts.audio_mono = 0;
            else {
                fprintf(stderr, "Invalid --audio value '%s' (expected mono|stereo)\n", v);
                return 1;
            }
        } else if ((v = match_kv(a, "dos-graphics")) != NULL) {
            if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0)       opts.dos_graphics = 1;
            else if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0) opts.dos_graphics = 0;
            else {
                fprintf(stderr, "Invalid --dos-graphics value '%s' (expected on|off)\n", v);
                return 1;
            }
        } else if ((v = match_kv(a, "dos-gameplay")) != NULL) {
            if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0)       opts.dos_gameplay = 1;
            else if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0) opts.dos_gameplay = 0;
            else {
                fprintf(stderr, "Invalid --dos-gameplay value '%s' (expected on|off)\n", v);
                return 1;
            }
        } else if ((v = match_kv(a, "dos-dir")) != NULL) {
            snprintf(opts.dos_dir, sizeof opts.dos_dir, "%s", v);
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", a);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Publish DOS-mode flags for the game code (default = Amiga graphics). */
    _gDosGraphics = opts.dos_graphics;
    { extern int _gArcadeMode; _gArcadeMode = opts.arcade; }
    _gDosGameplay = opts.dos_gameplay;
    /* _screen_width (the live render width) + _menu_draw_x_offset (UI centering)
     * are derived from the monitor's aspect ratio just after SDL_Init — see below. */
    if (opts.dos_dir[0])
        snprintf(_gDosGraphicsDir, sizeof _gDosGraphicsDir, "%s", opts.dos_dir);

    /* Redirect stderr to SWOS.log in the per-user game folder (beside SWOS.ini
     * and save/ — swos_user_dir(), resolved above). Saves the tester from having
     * to remember `2> SWOS.log`, and keeps the log out of a possibly read-only
     * install dir. Works the same from gdb, Explorer, or a terminal. Disable by
     * setting SWOS_NO_LOG=1 to send stderr back to the console. Unbuffered so
     * lines flush immediately and we don't lose context on a crash. */
    if (getenv("SWOS_NO_LOG") == NULL) {
        char log_path[1088];
        snprintf(log_path, sizeof log_path, "%s/SWOS.log", swos_user_dir());
        FILE *log = freopen(log_path, "w", stderr);
        if (log) {
            setvbuf(log, NULL, _IONBF, 0);
        }
    }

    /* Load the UI translation table for the selected language
     * (data_json/translations/<lang>.json) now that the CWD is the binary dir
     * and stderr is the log. tr(symbol) resolves menu/UI text through it; a
     * missing file just leaves every lookup falling back to the compiled
     * string symbol. Must run before the menu/competition install reads it. */
    {
        extern void Translations_Load(const char *lang);
        extern void Countries_Localize(void);
        extern void Currency_Set(const char *name);
        Translations_Load(opts.language);
        Countries_Localize();
        Currency_Set(opts.currency);   /* pound (stock £) | dollar ($) | euro (€) */
    }

    if (_gDosGraphics)
        fprintf(stderr, "[dos] DOS graphics mode ON (bin/%s)\n", _gDosGraphicsDir);

    SDL_SetMainReady();

    /* Become per-monitor DPI-aware on Windows BEFORE SDL_Init (Windows reads the
     * process DPI awareness at init). Without this SDL2 runs DPI-UNAWARE, so on a
     * scaled display (a 16" laptop at 150% is the Windows default) Windows
     * virtualizes coordinates: SDL_CreateWindow sizes are LOGICAL pixels that
     * Windows then scales UP — a 3x window (1452x816 logical) becomes ~2178x1224
     * physical and overflows a 1920x1080 panel, hiding the title bar. It also
     * broke our fit clamp: SDL_GetDisplayUsableBounds reports PHYSICAL pixels
     * while the window is sized in LOGICAL pixels, so the two never agree. Making
     * the process DPI-aware puts everything in physical pixels, so win_scale and
     * the usable-bounds clamp are consistent and the window fits its monitor.
     * Set via the raw hint name so it compiles on any SDL2 (the macro is 2.24+)
     * and is ignored on Linux/WASM/older SDL. */
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS |
                  SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    /* (swos_save_dir_init() now runs earlier, before Settings_Load — see above.) */

    /* Match the render aspect to `resolution` (or the desktop when resolution=auto):
     * width = SCREEN_HEIGHT * (w/h), clamped to [SCREEN_WIDTH .. the full pitch width].
     * The render fills any aspect — 4:3, 16:9, 21:9, ultrawide — with no letterbox bars,
     * and any arbitrary resolution works (it need not match the monitor). Needs
     * SDL_INIT_VIDEO (above); runs before the window/texture (Platform_Init below). */
    {
        /* Clamp the requested display index ([video] screen=N) to a valid one —
         * 0 = primary. The SAME index is used here for the render aspect AND by
         * Platform_Init to place + size the window, so the aspect always matches
         * the monitor the window opens on (fixes "sized from monitor 0, dropped
         * on a different screen, overflows it" on multi-monitor rigs). */
        int nd = SDL_GetNumVideoDisplays();
        if (nd < 1) nd = 1;
        if (opts.screen < 0 || opts.screen >= nd) opts.screen = 0;

        int dw, dh;
        if (opts.res_w > 0 && opts.res_h > 0) {
            /* SWOS.ini [video] resolution=WxH — any aspect, monitor-independent. */
            dw = opts.res_w; dh = opts.res_h;
        } else {
            /* 'auto': derive the render aspect from the chosen display's desktop
             * mode, but DON'T store it back into opts — leaving res_w/res_h at 0
             * keeps the file's "resolution=auto" intact when Settings_Save runs
             * (otherwise 'auto' would be rewritten as the resolved WxH). */
            SDL_DisplayMode dm;
            dw = 16; dh = 9;   /* 16:9 fallback if the query fails */
            if (SDL_GetDesktopDisplayMode(opts.screen, &dm) == 0 && dm.h > 0) { dw = dm.w; dh = dm.h; }
        }
        int w = (int)((long)SCREEN_HEIGHT * dw / dh);
        w = (w + 1) & ~1;                          /* round up to even */
        if (w < SCREEN_WIDTH)          w = SCREEN_WIDTH;
        if (w > PITCH_VIEW_MAX_WIDTH)  w = PITCH_VIEW_MAX_WIDTH;
        _screen_width = w;
        fprintf(stderr, "[video] render %dx%d (screen %d/%d, resolution %dx%d, %.2f:1)\n",
                _screen_width, SCREEN_HEIGHT, opts.screen, nd, dw, dh, (double)dw / dh);
    }
    _menu_draw_x_offset = WIDE_MARGIN;

    if (Platform_Init(&opts) != 0) {
        fprintf(stderr, "Platform_Init failed\n");
        SDL_Quit();
        return 1;
    }

    /* Hand off to the (ported) Amiga bootstrap. startShim runs _start()
     * (hardware init -- now no-ops via stubs) then mainC() which is the
     * actual game loop. The game pumps SDL via Platform_VBlank() callbacks. */
    startShim();

    /* Persist settings WITHOUT clobbering a hand-edited SWOS.ini: create the
     * file if it doesn't exist, otherwise only add keys it's missing (e.g. new
     * settings from a newer build). Existing values/comments are left as the
     * user wrote them. Done BEFORE Platform_Shutdown destroys the window, since
     * the getters used for any newly-added scale/fullscreen keys read SDL flags. */
    Settings_AppendMissing(&opts, ini_path);

    Platform_Shutdown();
    SDL_Quit();
    return 0;
}
