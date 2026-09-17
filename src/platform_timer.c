/*
 * platform_timer.c -- ask Windows for a 1 ms scheduler tick.
 *
 * Every sleep we make (the vsync-off frame limiter in Platform_Present, the
 * netplay waits, the self-tests' pacing) is SDL_Delay. On Windows a sleep is
 * rounded up to the scheduler tick, which is 15.6 ms unless a process asks for
 * less, so SDL_Delay(18) sleeps 31 ms and SDL_Delay(1) sleeps 15.6.
 *
 * SDL_HINT_TIMER_RESOLUTION used to do this for us, and on the Cygwin build it
 * no longer does. Cygwin now ships SDL2 as sdl2-compat on top of SDL3, SDL3
 * removed the hint, and SDL3 built for Cygwin sleeps through Cygwin's
 * nanosleep, not the high-resolution waitable timer its native Windows build
 * uses. Measured there, with the hint set and SDL initialised:
 *
 *     SDL_Delay(18): min 30.10  avg 31.21  max 32.95 ms   (default tick)
 *     SDL_Delay(18): min 17.99  avg 18.45  max 19.27 ms   (after timeBeginPeriod(1))
 *
 * So the call is made here, directly, at the top of main() -- before the
 * self-tests, which run before Platform_Init.
 *
 * Its own file because <windows.h> must not meet the game's headers.
 */
#include "platform_sdl.h"

#if defined(_WIN32) || defined(__CYGWIN__)

#include <stdlib.h>
#include <windows.h>

static void restoreTimerResolution(void)
{
    timeEndPeriod(1);
}

int32_t Platform_RaiseTimerResolution(void)
{
    if (timeBeginPeriod(1) != TIMERR_NOERROR)
        return 0;
    /* Windows drops the request when the process exits anyway; this keeps the
     * begin/end pairing the API documents. */
    atexit(restoreTimerResolution);
    return 1;
}

#else

int32_t Platform_RaiseTimerResolution(void)
{
    return 1;   /* nothing to raise: Linux, macOS and the rest sleep to ~1 ms already */
}

#endif
