/*
 * platform_switch.cpp — the Nintendo Switch's log sink.
 *
 * SWOS reports through fprintf(stderr, ...) and printf() everywhere, which is
 * right on the three desktops and reaches NOBODY on a console: nothing
 * connects stdio to the devkit, so a build that fails during startup produces
 * an error code and not one word about what it was doing.
 *
 * NN_LOG does reach `ControlTarget read-log`, so swos_platform.h redirects the
 * stderr side of those two calls here. That gets every diagnostic the game
 * already prints — the [video] line, the asset counts, the panics — onto the
 * target log with no call-site changes.
 *
 * This is the one C++ translation unit in the project. It exists only because
 * NN_LOG is a C++ macro (::nn::diag::), and it is deliberately tiny: it is a
 * log sink, not a logging framework, and should not grow into one.
 */

#include <nn/nn_Log.h>
#include <stdarg.h>

extern "C" {

/* Matches the fprintf/printf return contract loosely — callers ignore it. */
int SwosSwitchVLog(const char *fmt, va_list args)
{
    NN_VLOG(fmt, args);
    return 0;
}

int SwosSwitchLog(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    NN_VLOG(fmt, args);
    va_end(args);
    return 0;
}

}  /* extern "C" */
