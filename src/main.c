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

#ifdef _WIN32
#include <windows.h>
#include <direct.h>          /* _chdir */
#define chdir(p) _chdir(p)
#else
#include <unistd.h>          /* readlink, chdir */
#endif

#include "platform_sdl.h"
#include "swos_settings.h"
#include "save_format_json.h"   /* DataJson_SelfTest */
#include "dos_mode.h"   /* gDosGraphics/gDosGameplay — defined in dos_graphics.c */
#include "display.h"    /* g_screen_width, WIDESCREEN_WIDTH, SCREEN_WIDTH */

#define SWOS_INI_PATH "SWOS.ini"

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
    /* Anchor CWD to the binary's directory FIRST so SWOS.ini / SWOS.log
     * and the data/ grafs/ sound/ trees all resolve relative to the
     * exe regardless of where the user launched from. Done before
     * Settings_Load so SWOS.ini lookup hits the right file. */
    chdir_to_binary(argv ? argv[0] : NULL);

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
    opts.widescreen = 1;          /* default ON — wider 16:9 pitch view */
    opts.audio_mono = 1;
    opts.dos_graphics = 0;        /* default Amiga graphics */
    opts.dos_gameplay = 0;        /* default Amiga gameplay */
    snprintf(opts.dos_dir, sizeof opts.dos_dir, "%s", "grafs_dos");
    snprintf(opts.language, sizeof opts.language, "%s", "en");
    snprintf(opts.currency, sizeof opts.currency, "%s", "pound");

    /* 2) SWOS.ini overrides defaults. Missing file is fine — Settings_Save
     * at clean shutdown will create one. */
    Settings_Load(&opts, SWOS_INI_PATH);

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
    gDosGraphics = opts.dos_graphics;
    gDosGameplay = opts.dos_gameplay;
    gWidescreen  = opts.widescreen;
    /* g_screen_width (the live render width) + g_menu_draw_x_offset (UI centering)
     * are derived from the monitor's aspect ratio just after SDL_Init — see below. */
    if (opts.dos_dir[0])
        snprintf(gDosGraphicsDir, sizeof gDosGraphicsDir, "%s", opts.dos_dir);

    /* Redirect stderr to SWOS.log alongside the binary. Saves the
     * tester from having to remember `2> SWOS.log` on every launch
     * and works the same whether the program is started from gdb,
     * Explorer, or a terminal. Disable by setting SWOS_NO_LOG=1 in
     * the environment if you want stderr to go back to the
     * console. unbuffered so log lines flush immediately and we
     * don't lose context when the program crashes. */
    if (getenv("SWOS_NO_LOG") == NULL) {
        FILE *log = freopen("SWOS.log", "w", stderr);
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

    if (gDosGraphics)
        fprintf(stderr, "[dos] DOS graphics mode ON (bin/%s)\n", gDosGraphicsDir);

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS |
                  SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    /* Match the render aspect to the display: width = SCREEN_HEIGHT * (desktop w/h),
     * clamped to [SCREEN_WIDTH .. the full pitch width]. The render then fills any
     * monitor — 4:3, 16:9, 21:9, ultrawide — with no letterbox bars, and it "just works"
     * with no mode to set. widescreen=false forces the classic SCREEN_WIDTH. Needs
     * SDL_INIT_VIDEO (above); runs before the window/texture (Platform_Init below). */
    if (gWidescreen) {
        SDL_DisplayMode dm;
        int dw = 16, dh = 9;   /* 16:9 fallback if the query fails */
        if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.h > 0) { dw = dm.w; dh = dm.h; }
        int w = (int)((long)SCREEN_HEIGHT * dw / dh);
        w = (w + 1) & ~1;                          /* round up to even */
        if (w < SCREEN_WIDTH)          w = SCREEN_WIDTH;
        if (w > PITCH_VIEW_MAX_WIDTH)  w = PITCH_VIEW_MAX_WIDTH;
        g_screen_width = w;
        fprintf(stderr, "[video] render %dx%d (display %dx%d, %.2f:1)\n",
                g_screen_width, SCREEN_HEIGHT, dw, dh, (double)dw / dh);
    } else {
        g_screen_width = SCREEN_WIDTH;
    }
    g_menu_draw_x_offset = WIDE_MARGIN;

    if (Platform_Init(&opts) != 0) {
        fprintf(stderr, "Platform_Init failed\n");
        SDL_Quit();
        return 1;
    }

    /* Hand off to the (ported) Amiga bootstrap. startShim runs _start()
     * (hardware init -- now no-ops via stubs) then mainC() which is the
     * actual game loop. The game pumps SDL via Platform_VBlank() callbacks. */
    startShim();

    /* Snapshot runtime-mutable settings (window scale, fullscreen mode)
     * via the Platform_Get* getters and persist back to SWOS.ini. Done
     * BEFORE Platform_Shutdown destroys the window, since the getters
     * read SDL window flags. */
    Settings_Save(&opts, SWOS_INI_PATH);

    Platform_Shutdown();
    SDL_Quit();
    return 0;
}
