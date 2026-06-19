/*
 * swos_settings.c — INI-backed persistent display + audio settings.
 *
 * Layout (SWOS.ini):
 *
 *   [video]
 *   renderer=accel        ; software | accel
 *   vsync=off             ; on | off
 *   fullscreen=off        ; off | desktop | exclusive
 *   scale=3               ; 1..4 (windowed only)
 *
 *   [audio]
 *   mode=mono             ; mono | stereo
 *
 *   [dos]                 ; PC-only extras (Amiga is the definitive version)
 *   graphics=false        ; true | false — render DOS graphics (bin/grafs_dos)
 *   gameplay=false        ; true | false — DOS gameplay tweaks (reserved)
 *
 * Boolean values accept on/off, true/false, yes/no, 1/0 — case-insensitive.
 * Unknown keys / sections are silently ignored so the file format can grow
 * without breaking older binaries.
 */

#include "swos_settings.h"
#include "ini.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int eqi(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) return 0;
    }
    return *a == 0 && *b == 0;
}

static int parse_bool(const char *v, int *out)
{
    if (eqi(v, "on") || eqi(v, "true") || eqi(v, "yes") || eqi(v, "1")) {
        *out = 1; return 1;
    }
    if (eqi(v, "off") || eqi(v, "false") || eqi(v, "no") || eqi(v, "0")) {
        *out = 0; return 1;
    }
    return 0;
}

static int handler(void *user, const char *section,
                   const char *name, const char *value)
{
    PlatformInitOptions *opts = (PlatformInitOptions *)user;

    if (eqi(section, "video")) {
        if (eqi(name, "renderer")) {
            if      (eqi(value, "software")) opts->renderer = PLATFORM_RENDERER_SOFTWARE;
            else if (eqi(value, "accel"))    opts->renderer = PLATFORM_RENDERER_ACCEL;
        } else if (eqi(name, "vsync")) {
            int b;
            if (parse_bool(value, &b)) opts->vsync = b;
        } else if (eqi(name, "fullscreen")) {
            if      (eqi(value, "off"))       opts->fullscreen = PLATFORM_FULLSCREEN_NONE;
            else if (eqi(value, "desktop"))   opts->fullscreen = PLATFORM_FULLSCREEN_DESKTOP;
            else if (eqi(value, "exclusive")) opts->fullscreen = PLATFORM_FULLSCREEN_EXCLUSIVE;
        } else if (eqi(name, "scale")) {
            int n = atoi(value);
            if (n >= 1 && n <= 4) opts->win_scale = n;
        }
    } else if (eqi(section, "audio")) {
        if (eqi(name, "mode")) {
            if      (eqi(value, "mono"))   opts->audio_mono = 1;
            else if (eqi(value, "stereo")) opts->audio_mono = 0;
        }
    } else if (eqi(section, "dos")) {
        int b;
        if      (eqi(name, "graphics") && parse_bool(value, &b)) opts->dos_graphics = b;
        else if (eqi(name, "gameplay") && parse_bool(value, &b)) opts->dos_gameplay = b;
    }
    /* Return nonzero on success, even for ignored keys — we don't want
     * inih to flag unknown sections as parse errors. */
    return 1;
}

void Settings_Load(PlatformInitOptions *opts, const char *path)
{
    if (!opts || !path) return;
    /* ini_parse returns 0 on success, line# on parse error, -1 if the
     * file can't be opened. Missing file is fine — we'll create it on
     * Settings_Save. Parse errors don't abort load; the keys before
     * the bad line still apply. */
    int rc = ini_parse(path, handler, opts);
    if (rc > 0) {
        fprintf(stderr, "[settings] %s: parse error at line %d (continuing)\n",
                path, rc);
    }
}

static const char *renderer_str(PlatformRendererMode m)
{
    return (m == PLATFORM_RENDERER_SOFTWARE) ? "software" : "accel";
}

static const char *fullscreen_str(PlatformFullscreenMode m)
{
    switch (m) {
    case PLATFORM_FULLSCREEN_DESKTOP:   return "desktop";
    case PLATFORM_FULLSCREEN_EXCLUSIVE: return "exclusive";
    case PLATFORM_FULLSCREEN_NONE:
    default:                            return "off";
    }
}

void Settings_Save(const PlatformInitOptions *opts, const char *path)
{
    if (!opts || !path) return;

    /* Read current scale + fullscreen from the platform layer so any
     * runtime adjustments (Alt+Enter, +/-) round-trip into the file.
     * Renderer / vsync / audio_mode aren't user-toggleable at runtime
     * yet, so we trust whatever's in opts. */
    int scale_now = Platform_GetWindowScale();
    PlatformFullscreenMode fs_now = Platform_GetFullscreenMode();
    if (scale_now < 1 || scale_now > 4) scale_now = opts->win_scale;

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[settings] cannot open %s for write: %s\n",
                path, strerror(errno));
        return;
    }

    fprintf(f,
        "; SWOS — display + audio settings.\n"
        "; Auto-generated. Edit by hand or via the command line; runtime\n"
        "; changes (+/- keys, Alt+Enter) are written back here on clean\n"
        "; shutdown.\n"
        "\n"
        "[video]\n"
        "renderer=%s\n"
        "vsync=%s\n"
        "fullscreen=%s\n"
        "scale=%d\n"
        "\n"
        "[audio]\n"
        "mode=%s\n"
        "\n"
        "[dos]\n"
        "graphics=%s\n"
        "gameplay=%s\n",
        renderer_str(opts->renderer),
        opts->vsync ? "on" : "off",
        fullscreen_str(fs_now),
        scale_now,
        opts->audio_mono ? "mono" : "stereo",
        opts->dos_graphics ? "true" : "false",
        opts->dos_gameplay ? "true" : "false");

    fclose(f);
}
