/*
 * main.c -- SDL2 entry point for the SWOS C/SDL port.
 *
 * Original Amiga main.c just called _startShim() which jumped to the
 * Amiga assembly bootstrap. We replace that with an SDL2 host that:
 *   1. Initializes SDL video / audio / events / gamepad
 *   2. Creates the window + renderer + a 336x272 paletted texture
 *   3. Calls into the (ported) Amiga _start / _mainC bootstrap
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

#define WIN_SCALE 3

extern void _startShim(void);

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

int main(int argc, char **argv)
{
    (void)argc;

    chdir_to_binary(argv ? argv[0] : NULL);

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

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS |
                  SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    if (Platform_Init(WIN_SCALE) != 0) {
        fprintf(stderr, "Platform_Init failed\n");
        SDL_Quit();
        return 1;
    }

    /* Hand off to the (ported) Amiga bootstrap. _startShim runs _start()
     * (hardware init -- now no-ops via stubs) then _mainC() which is the
     * actual game loop. The game pumps SDL via Platform_VBlank() callbacks. */
    _startShim();

    Platform_Shutdown();
    SDL_Quit();
    return 0;
}
