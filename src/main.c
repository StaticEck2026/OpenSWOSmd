/*
 * main.c -- process entry point for BOTH targets of the united SWOS port.
 *
 * The two hosts share no startup code at all, so the file is split on
 * SWOS_SDL_BUILD (defined by include/swos_platform.h, which the SDL build
 * force-includes via -include; the m68k build never defines it):
 *
 *   SWOS_SDL_BUILD  -> the SDL2 host: init video/audio/events/gamepad, create
 *                      the window + paletted texture, then call the ported
 *                      Amiga _start / mainC bootstrap.
 *   else            -> the Amiga host: the -nostdlib libc stubs the m68k
 *                      build needs, then startShim() into the original
 *                      assembly bootstrap.
 */

#ifdef SWOS_SDL_BUILD


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
#include "replay_test.h"        /* --replay-test: recorded-game regression rig */
#include "online_menus.h"       /* --menu, --menu-selftest */
#include "netplay.h"            /* --netplay-selftest */
#include "build_info.h"         /* --version, and the [build] line in SWOS.log */
#include "p2p_link.h"           /* --netplay-selftest: the MULTIPLAYER menu's link */
#include "netplay_match.h"      /* --netplay-match */
#include "rollback_rig.h"       /* --rollback */

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
        "  --subpixel=<on|off>       Smooth sub-pixel scrolling (default on). The game\n"
        "                            draws at whole-pixel camera positions, so a true\n"
        "                            scroll of 1.5 px/frame steps 1,2,1,2. This shifts\n"
        "                            the presented frame by the remainder. Quantised to\n"
        "                            whole output pixels, so it never blurs. Turn it off\n"
        "                            to compare.\n"
        "  --scaling=<sharp|smooth>  How the frame is scaled to the window (default\n"
        "                            smooth). A fullscreen scale is rarely a whole\n"
        "                            number (1080/272 = 3.97), so a plain nearest copy\n"
        "                            gives one source pixel 4 output pixels and its\n"
        "                            neighbour 3 — uneven text and shirt numbers.\n"
        "                            'smooth' pre-multiplies the frame by a whole\n"
        "                            number first, then resamples only the leftover\n"
        "                            fraction. 'sharp' is the old single nearest copy.\n"
        "                            Both are no-ops at a whole-number scale.\n"
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
        "  --dos-gameplay=<on|off>   Use the DOS version's gameplay tuning: a 70 Hz\n"
        "                            game tick (movement scaled 41/64 so it doesn't\n"
        "                            play faster), plus the DOS goalkeeper reach and\n"
        "                            ball-physics constants. Default: off (Amiga\n"
        "                            tuning). PC-only extra.\n"
        "  --fire-hold-lockout=<on|off>\n"
        "                            Restore an original-Amiga BUG: an OPPONENT's\n"
        "                            tackle wipes your held fire button's shot charge,\n"
        "                            and it cannot restart while the button is down, so\n"
        "                            holding fire through a challenge never shoots —\n"
        "                            you have to release and press again. Default off\n"
        "                            (fixed). Your OWN tackle still spends the press\n"
        "                            in both modes: that part is by design.\n"
        "  --dos-dir=<folder>        Folder (under bin/) to load DOS assets from.\n"
        "                            Default: grafs_dos.\n"
        "  --replay-test=<file.swr>  Replay a recorded match and assert every frame\n"
        "                            against the recording (docs/TESTS.md). Skips the\n"
        "                            front end, runs unpaced, and exits 0 on a full\n"
        "                            match, 2 on the first divergence. Build a .swr\n"
        "                            with scripts/rgd_convert.py --emit.\n"
        "  --replay-level=<scalars|teams|info|sprites>\n"
        "                            How much of each frame to compare. Default: the\n"
        "                            level the .swr was emitted at.\n"
        "  --replay-width=<N>        Render width for --replay-test. Default 336,\n"
        "                            the geometry the corpus was recorded in. Any\n"
        "                            other value runs the SAME simulation with a\n"
        "                            wider render, which is how a render/simulation\n"
        "                            leak shows up: the compared fields must not\n"
        "                            move. See scripts/replay_widecheck.sh.\n"
        "  --replay-window           Open a window during --replay-test. Off by\n"
        "                            default: a replay is a batch run and a full\n"
        "                            matrix would open one window per cell.\n"
        "  --replay-selftest         Check CalculateDeltaXAndY against swos-port's\n"
        "                            reference vectors and exit. No recording needed.\n"
        "  --replay-skip=<a,b,...>   Mute every field whose name contains one of these\n"
        "                            substrings, e.g. '.starting_direction' (all 33\n"
        "                            sprites) or 'top_info.'.\n"
        "  --replay-max-errors=<N>   Keep going past a divergence and report up to N\n"
        "                            mismatching fields. Default: 1 (stop at the\n"
        "                            first — gameplay is chaotic, so everything after\n"
        "                            it is noise).\n"
        "  --menu=<name>             Open a menu that exists only as JSON before the\n"
        "                            main menu, e.g. swos2020/logInMenu (a file under\n"
        "                            data_json/menus/, docs/MENUS.md section 10).\n"
        "  --menu-selftest           Load and unpack every imported SWOS 2020 menu,\n"
        "                            check it against its JSON, and exit.\n"
        "  --version                 Print the build (commit and date) and exit.\n"
        "  --netplay-selftest        Run GGPO-X with a toy game: two peers over\n"
        "                            loopback, then a synctest session, then the\n"
        "                            MULTIPLAYER menu's own link. Exits.\n"
        "  --netplay-match=<mode>    With --replay-test: play the match through the\n"
        "                            netplay frame step on the recording's inputs\n"
        "                            (docs/NETPLAY.md): direct (no GGPO), synctest[:N]\n"
        "                            (every N frames GGPO loads the state of N frames\n"
        "                            back, re-simulates, compares; 1..8), or p2p (one\n"
        "                            of two peers, with --rollback=trace and:)\n"
        "  --netplay-player=1|2      p2p: the local player.\n"
        "  --netplay-port=<port>     p2p: the local UDP port.\n"
        "  --netplay-peer=<ip:port>  p2p: the other peer.\n"
        "  --netplay-delay=<frames>  p2p: input delay. Default 0.\n"
        "  --netplay-stall=<ms>      p2p: pause now and then, up to this long (test).\n"
        "  --netplay-trace=<file>    Per-frame state hashes as finally played.\n"
        "  --netplay-timeout=<s>     Give up after this long without progress (30).\n"
        "  --rollback=<mode>         With --replay-test: roll the match back to test\n"
        "                            the rollback state registry (docs/ROLLBACK.md).\n"
        "                            check | oracle | discover | scan | trace. The\n"
        "                            recording supplies setup and inputs only.\n"
        "  --rollback-window=<K>     Frames re-simulated per window. Default 8\n"
        "                            (discover: 32).\n"
        "  --rollback-every=<N>      Frames between window starts. Default 64\n"
        "                            (discover: 1024).\n"
        "  --rollback-first=<F>      No window before frame F. Default 0.\n"
        "  --rollback-seed=<n>       RNG seed for the match. Default 0x5EED5EED.\n"
        "  --rollback-report=<file>  JSON report (scripts/rollback_report.py).\n"
        "  --rollback-trace=<file>   Per-frame state hashes, for comparing two runs.\n"
        "  --rollback-restore=all|sim  Restore both registry tiers (default) or only\n"
        "                            the simulation tier (the resync minimum).\n"
        "  --rollback-disable=<a,b>  Leave these registry entries out (trial removal).\n"
        "  --rollback-portable       Restore through the resync (portable) encoding.\n"
        "  --rollback-dump=<file>    The registry, per region, at one frame; set the\n"
        "                            frame with --rollback-dump-frame=<n> (default 0).\n"
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
/* Switch: the headers define SIGHUP but nnSdk exports no signal(), so the
 * #ifdef alone is not the right test — the link fails on `undefined symbol:
 * signal`. There is no controlling terminal on a console either, which is what
 * this is for. */
#if defined(SIGHUP) && !defined(SWOS_SWITCH_BUILD)
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
     * /data, /grafs, … and cwd is already "/", so skip the chdir.
     * Switch: the content is a read-only mount at rom:/ and there is no chdir
     * to make; swos_data_path() (swos_io.h) anchors the data reads instead. */
#if !defined(SWOS_WASM_BUILD) && !defined(SWOS_SWITCH_BUILD)
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
    opts.subpixel_scroll = 1;     /* default on — smooth sub-pixel scrolling */
    opts.scaling_smooth  = 1;     /* default on — even pixels at a non-integer scale */
    opts.crt_mask = 0;            /* CRT effects all default OFF — opt-in */
    opts.crt_mask_strength = 45;
    opts.crt_vignette = 0;
    opts.crt_corners = 0;
    opts.render_driver[0] = '\0'; /* "" = auto (SDL default backend) */
    opts.res_w = 0; opts.res_h = 0;  /* 0,0 = auto (fill the desktop aspect) */
    opts.screen = 0;                 /* target display index (0 = primary) */
    opts.audio_mono = 1;
    opts.dos_graphics = 0;        /* default Amiga graphics */
    opts.dos_gameplay = 0;        /* default Amiga gameplay */
    opts.fire_hold_lockout = 0;   /* default: the original's fire-hold bug is FIXED */
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
    const char *replay_file = NULL, *replay_level = NULL, *replay_skip = NULL;
    int replay_max_errors = 1;
    int replay_window = 0;
    int replay_width = 0;   /* 0 = the corpus geometry (SCREEN_WIDTH) */
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
    /* Say WHICH file, and whether it was there. swos_save_dir_init() falls back
     * to "." if the per-user folder cannot be resolved or created, and silently
     * — so an edited SWOS.ini that "does nothing" is usually the game reading a
     * different one. Print it before loading so the answer is in the log even
     * if the load then fails. */
    {
        FILE *probe = fopen(ini_path, "rb");
        fprintf(stderr, "[settings] SWOS.ini: %s (%s)\n",
                ini_path, probe ? "found" : "not found, using defaults");
        if (probe) fclose(probe);
    }
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
        } else if ((v = match_kv(a, "subpixel")) != NULL) {
            if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0)       opts.subpixel_scroll = 1;
            else if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0) opts.subpixel_scroll = 0;
            else {
                fprintf(stderr, "Invalid --subpixel value '%s' (expected on|off)\n", v);
                return 1;
            }
        } else if ((v = match_kv(a, "scaling")) != NULL) {
            if (strcmp(v, "smooth") == 0)     opts.scaling_smooth = 1;
            else if (strcmp(v, "sharp") == 0) opts.scaling_smooth = 0;
            else {
                fprintf(stderr, "Invalid --scaling value '%s' (expected sharp|smooth)\n", v);
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
        } else if ((v = match_kv(a, "fire-hold-lockout")) != NULL) {
            if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0)       opts.fire_hold_lockout = 1;
            else if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0) opts.fire_hold_lockout = 0;
            else {
                fprintf(stderr, "Invalid --fire-hold-lockout value '%s' (expected on|off)\n", v);
                return 1;
            }
        } else if ((v = match_kv(a, "dos-gameplay")) != NULL) {
            if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0)       opts.dos_gameplay = 1;
            else if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0) opts.dos_gameplay = 0;
            else {
                fprintf(stderr, "Invalid --dos-gameplay value '%s' (expected on|off)\n", v);
                return 1;
            }
        } else if (strcmp(a, "--version") == 0) {
            /* So a tester can paste one line instead of describing their download. */
            printf("%s\n", Build_Version());
            return 0;
        } else if ((v = match_kv(a, "dos-dir")) != NULL) {
            snprintf(opts.dos_dir, sizeof opts.dos_dir, "%s", v);
        } else if ((v = match_kv(a, "replay-test")) != NULL) {
            replay_file = v;
        } else if ((v = match_kv(a, "replay-level")) != NULL) {
            replay_level = v;
        } else if ((v = match_kv(a, "replay-width")) != NULL) {
            replay_width = atoi(v);
        } else if (strcmp(a, "--replay-window") == 0) {
            replay_window = 1;
        } else if (strcmp(a, "--replay-selftest") == 0) {
            return ReplayTest_MathSelfTest();
        } else if ((v = match_kv(a, "menu")) != NULL) {
            OnlineMenus_SetStartMenu(v);
        } else if (strcmp(a, "--menu-selftest") == 0) {
            return OnlineMenus_SelfTest();
        } else if (strcmp(a, "--netplay-selftest") == 0) {
            /* The library, then the connection the MULTIPLAYER menu makes over it.
             * Both run whatever the other did, so one report covers both. */
            int32_t ggpoFailed = Netplay_SelfTest();
            int32_t linkFailed = P2PLink_SelfTest();
            return (ggpoFailed || linkFailed) ? 1 : 0;
        } else if (strncmp(a, "--netplay-", 10) == 0) {
            if (NetplayMatch_ParseArg(a) != 1) {
                fprintf(stderr, "Invalid argument: %s\n", a);
                return 1;
            }
        } else if ((v = match_kv(a, "replay-skip")) != NULL) {
            replay_skip = v;
        } else if ((v = match_kv(a, "replay-max-errors")) != NULL) {
            replay_max_errors = atoi(v);
            if (replay_max_errors < 1) {
                fprintf(stderr, "Invalid --replay-max-errors value '%s' (expected >= 1)\n", v);
                return 1;
            }
        } else if (strncmp(a, "--rollback", 10) == 0) {
            if (Rollback_ParseArg(a) != 1) {
                fprintf(stderr, "Invalid argument: %s\n", a);
                return 1;
            }
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", a);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* A replay test is a batch run: unpace the frame loop (the game clock is
     * driven per-iteration by the rig, not by the wall clock), skip the loader
     * intro, and keep the log on the terminal where the test output belongs.
     * Both are read inside Platform_Init, so they must be set before it. */
    if (Rollback_Enabled() && !replay_file) {
        fprintf(stderr, "--rollback needs --replay-test=<file.swr> (the match's setup and inputs)\n");
        return 1;
    }
    if (NetplayMatch_Enabled() && !replay_file) {
        fprintf(stderr, "--netplay-match needs --replay-test=<file.swr> (the match's setup and inputs)\n");
        return 1;
    }
    if (NetplayMatch_Enabled() && Rollback_Intervenes()) {
        fprintf(stderr, "--netplay-match takes only the rollback rig's watching options "
                        "(--rollback=trace, -trace, -dump, -save-state): the others rewind the "
                        "match GGPO is playing\n");
        return 1;
    }
    if (NetplayMatch_ArgError()) {
        fprintf(stderr, "%s\n", NetplayMatch_ArgError());
        return 1;
    }
    if (NetplayMatch_IsP2P() && (!Rollback_Enabled() || Rollback_WatchesFrames())) {
        fprintf(stderr, "--netplay-match=p2p plays in the rollback rig's environment: give it "
                        "--rollback=trace and no other --rollback option (--netplay-trace writes "
                        "the peer's trace)\n");
        return 1;
    }
    if (replay_file) {
#ifdef _WIN32
        _putenv("SWOS_HARNESS_TURBO=1");
        _putenv("SWOS_NO_LOG=1");
        if (!replay_window && !getenv("SDL_VIDEODRIVER"))
            _putenv("SDL_VIDEODRIVER=dummy");
        if (!getenv("SDL_AUDIODRIVER"))
            _putenv("SDL_AUDIODRIVER=dummy");
#else
        setenv("SWOS_HARNESS_TURBO", "1", 1);
        setenv("SWOS_NO_LOG", "1", 1);
        /* Headless by default: a replay is a batch run, and a full matrix opens
         * one window per cell. The dummy video driver keeps every buffer and
         * blit real — the game still renders into the frontbuffer, which is
         * what the simulation reads back — it just never opens a window.
         * --replay-window opts back in to watching it. Respect an
         * SDL_VIDEODRIVER the caller set explicitly. */
        if (!replay_window && !getenv("SDL_VIDEODRIVER"))
            setenv("SDL_VIDEODRIVER", "dummy", 1);
        /* Silent too, and unconditionally — a replay never wants to be heard,
         * and a matrix run is 48 of them. The dummy driver still opens a
         * device and still drains the queue on its own clock, so the audio
         * pump and everything it shares a tick with behave exactly as they do
         * with a real card; it just goes nowhere. Respect an explicit
         * SDL_AUDIODRIVER from the caller. */
        if (!getenv("SDL_AUDIODRIVER"))
            setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif
        /* The rollback rig plays its own match and leaves gameplay, the fire-hold
         * lockout and audio to the command line -- and needs no audio device or pump
         * thread writing game memory behind the game loop's back (docs/ROLLBACK.md).
         * Nor does a netplay match, which re-simulates frames. */
        if (Rollback_Enabled() || NetplayMatch_Enabled())
            SDL_setenv("SWOS_AUDIO_OFF", "1", 1);
        if (!Rollback_Enabled())
        opts.dos_gameplay = 1;   /* the corpus is DOS gameplay (docs/TESTS.md 5c) */
        /* The recordings were made against a reference implementation of the
         * ORIGINAL, so a replay has to PLAY the original — including its
         * fire-hold lockout bug. Without this the rig diverges on quick_fire /
         * normal_fire the first time a tackle lands on a held button, which is
         * ~55 times a match. Not overridable: a replay is a comparison, not a
         * game. See _gOriginalFireHoldLockout in team_control.c.
         *
         * WHEN THE NEW RECORDING FORMAT LANDS, THIS BECOMES PER FILE.
         * The owner's call (2026-09-13): new recordings are made with the fix
         * ACTIVE, because fixed is the new norm. So this constant is only right
         * for the .RGD corpus — read the setting out of the recording's own
         * header instead and fall back to 1 for a format that has no such
         * field (every .RGD). Do NOT decide it by format: that exact shortcut
         * was already paid for once with the replay framerate, where the
         * authored rate turned out to be per FILE (a gameStyle byte) and
         * hardcoding it per format broke the reels that disagreed —
         * see project_replay_framerate_decouple. */
        if (!Rollback_Enabled())
        opts.fire_hold_lockout = 1;
        /* Replay at the port's OWN native view, not the tester's monitor. The
         * original reason given here — "SWOS's camera constants are all derived
         * from the view size" — no longer holds: 22f370a pinned _sim_view_width
         * to SCREEN_WIDTH (below), so the SIMULATION is supposed to be
         * width-independent and only the renderer draws wider. The pin stays as
         * the DEFAULT because that is the geometry the corpus was recorded in.
         *
         * --replay-width=N lifts it deliberately. That is not a curiosity: if
         * the simulation really is width-independent then every compared field
         * must be identical at any N, so a divergence at N != 336 is a
         * render/simulation leak and nothing else. Three tester-reported bugs in
         * one day were widescreen-only and invisible here. */
        opts.res_w = (replay_width > 0) ? replay_width : SCREEN_WIDTH;
        opts.res_h = SCREEN_HEIGHT;
    }

    /* Publish DOS-mode flags for the game code (default = Amiga graphics). */
    _gDosGraphics = opts.dos_graphics;
    { extern int _gArcadeMode; _gArcadeMode = opts.arcade; }
    _gDosGameplay = opts.dos_gameplay;
    { extern int _gOriginalFireHoldLockout;
      _gOriginalFireHoldLockout = opts.fire_hold_lockout; }
    /* The OPTIONS menu draws its GAMEPLAY row from a uint16_t mirror (the
     * string-table header reads its index as one). Seed it here so the row
     * shows what SWOS.ini / --dos-gameplay actually selected; the toggle
     * itself keeps the two in step (ChangeGameplayStyle in menus.c). */
    { extern uint16_t _gameplayStyleOption;
      _gameplayStyleOption = (uint16_t)(opts.dos_gameplay != 0); }

    /* And say which style is actually in force, so "it ran in Amiga mode" is
     * something a tester can read off rather than infer from feel. */
    fprintf(stderr, "[gameplay] %s\n", _gDosGameplay
            ? "DOS (70fps physics and timers, DOS formations)"
            : "AMIGA (the shipped default)");
    /* Say it out loud — this one is a deliberate departure from the original,
     * so a tester comparing against real SWOS can see which half he is on. */
    fprintf(stderr, "[gameplay] fire-hold lockout: %s\n", opts.fire_hold_lockout
            ? "ON — original Amiga behaviour (an opponent's tackle kills your shot charge)"
            : "off — fixed (an opponent's tackle no longer kills your shot charge)");
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
#ifdef SWOS_SWITCH_BUILD
    /* Switch: don't redirect. stderr on a devkit reaches the target log through
     * the SDK, and a log FILE on the save: mount would burn the user's save
     * quota and need committing to survive anyway. `ControlTarget read-log`
     * is where the output goes — start it BEFORE launching (docs/SWITCH.md). */
#else
    if (getenv("SWOS_NO_LOG") == NULL) {
        char log_path[1088];
        snprintf(log_path, sizeof log_path, "%s/SWOS.log", swos_user_dir());
        FILE *log = freopen(log_path, "w", stderr);
        if (log) {
            setvbuf(log, NULL, _IONBF, 0);
        }
    }
#endif

    /* First line of SWOS.log: which build wrote it. Deliberately AFTER the
     * redirect above -- printed any earlier it goes to a terminal the tester
     * does not have, and the log they actually send us is the thing that has to
     * answer "which build is this?" on its own. */
    fprintf(stderr, "[build] %s\n", Build_Version());

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
    /* The SIMULATION view (display.h) is the port's own fixed geometry, NOT the
     * render width. SWOS reads the view size from inside the simulation — the
     * onScreen cull decides which players auto-selection may pick — so tying
     * it to the render width made a widescreen display change CPU behaviour:
     * a 650-wide view admits men the Amiga culls, and the nearest-to-the-ball
     * choice changes. Pinning it here means every display plays the same game
     * and the renderer simply draws wider (GAMEPLAY.md §25.4/§25.5).
     * The replay rig overrides it to the geometry a recording was made in. */
    _sim_view_width  = SCREEN_WIDTH;
    _sim_view_height = SCREEN_HEIGHT;

    _menu_draw_x_offset = WIDE_MARGIN;

    /* Arm the replay rig before the bootstrap: SWOS.c enters it in place of
     * the front-end loop once the game data is loaded. Parsing here means a
     * bad path or a stale .swr fails immediately, not after the intro. */
    if (replay_file &&
        ReplayTest_Load(replay_file, replay_level, replay_max_errors, replay_skip) != 0) {
        SDL_Quit();
        return 1;
    }

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

#else /* !SWOS_SDL_BUILD -- Amiga m68k host */

/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "swos_asm.h"

__attribute__((used, externally_visible)) volatile uint16_t *_pPanicReasonCode = &_panicReasonCode;

/* Minimal libc stubs needed when LTO/Ofast optimizes loops into library calls */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memcpy(void *dest, const void *src, unsigned long len) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (len--)
        *d++ = *s++;
    return dest;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memset(void *dest, int val, unsigned long len) {
    unsigned char *ptr = (unsigned char *)dest;
    while (len-- > 0)
        *ptr++ = (unsigned char)val;
    return dest;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memmove(void *dest, const void *src, unsigned long len) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    if (d < s) {
        while (len--)
            *d++ = *s++;
    } else {
        const char *lasts = s + (len - 1);
        char *lastd = d + (len - 1);
        while (len--)
            *lastd-- = *lasts--;
    }
    return dest;
}

int main(void)
{
    startShim();
    return 0;
}
#endif /* SWOS_SDL_BUILD */
