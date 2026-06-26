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
 *   directory=grafs_dos   ; folder (under bin/) to load DOS assets from
 *   graphics=false        ; true | false — render DOS graphics
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
        } else if (eqi(name, "widescreen")) {
            int b;
            if (parse_bool(value, &b)) opts->widescreen = b;
        } else if (eqi(name, "resolution")) {
            /* "WxH" (e.g. 2560x1080) drives the render aspect, for testing other
             * resolutions without changing the desktop. "auto"/"desktop"/blank
             * (or unparseable) → 0,0 = use the actual desktop resolution. */
            int w = 0, h = 0;
            if (value && sscanf(value, "%d%*[xX]%d", &w, &h) == 2 && w > 0 && h > 0) {
                opts->res_w = w; opts->res_h = h;
            } else {
                opts->res_w = 0; opts->res_h = 0;
            }
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
        else if (eqi(name, "directory") && value && value[0])
            snprintf(opts->dos_dir, sizeof opts->dos_dir, "%s", value);
    } else if (eqi(section, "arcade")) {
        int b;
        if (eqi(name, "enabled") && parse_bool(value, &b)) opts->arcade = b;
        else if (eqi(name, "key_p1_credit") && value && value[0])
            snprintf(opts->arcade_key_p1_credit, sizeof opts->arcade_key_p1_credit, "%s", value);
        else if (eqi(name, "key_p1_start") && value && value[0])
            snprintf(opts->arcade_key_p1_start, sizeof opts->arcade_key_p1_start, "%s", value);
        else if (eqi(name, "key_p2_credit") && value && value[0])
            snprintf(opts->arcade_key_p2_credit, sizeof opts->arcade_key_p2_credit, "%s", value);
        else if (eqi(name, "key_p2_start") && value && value[0])
            snprintf(opts->arcade_key_p2_start, sizeof opts->arcade_key_p2_start, "%s", value);
        else if (eqi(name, "key_service") && value && value[0])
            snprintf(opts->arcade_key_service, sizeof opts->arcade_key_service, "%s", value);
    } else if (eqi(section, "general")) {
        if (eqi(name, "language") && value && value[0])
            snprintf(opts->language, sizeof opts->language, "%s", value);
        else if (eqi(name, "currency") && value && value[0])
            snprintf(opts->currency, sizeof opts->currency, "%s", value);
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

    char res_buf[24];
    if (opts->res_w > 0 && opts->res_h > 0)
        snprintf(res_buf, sizeof res_buf, "%dx%d", opts->res_w, opts->res_h);
    else
        snprintf(res_buf, sizeof res_buf, "auto");

    fprintf(f,
        "; SWOS — display, audio and gameplay settings.\n"
        "; Auto-generated. Edit by hand or via the command line; runtime changes\n"
        "; (+/- keys, Alt+Enter) are written back here on clean shutdown. Booleans\n"
        "; accept true/false, on/off, yes/no, 1/0 (case-insensitive).\n"
        "\n"
        "[video]\n"
        "; renderer: software | accel  (hardware-accelerated)\n"
        "renderer=%s\n"
        "; vsync: on | off  (accel renderer only)\n"
        "vsync=%s\n"
        "; fullscreen: off | desktop | exclusive\n"
        "fullscreen=%s\n"
        "; scale: window scale 1-4 (windowed only)\n"
        "scale=%d\n"
        "; widescreen: true | false — true matches the render to your monitor's aspect\n"
        ";   ratio so it fills the screen (16:9 / 21:9 / ultrawide); false = classic 4:3\n"
        "widescreen=%s\n"
        "; resolution: WxH (e.g. 2560x1080) to test a specific aspect ratio, or\n"
        ";   'auto' for your monitor's native resolution. Only used when widescreen=true.\n"
        "resolution=%s\n"
        "\n"
        "[audio]\n"
        "; mode: mono | stereo\n"
        "mode=%s\n"
        "\n"
        "[dos]              ; PC-only extras (Amiga is the definitive version)\n"
        "; directory: folder under bin/ to load the DOS assets from\n"
        "directory=%s\n"
        "; graphics: true | false — render the DOS-version graphics\n"
        "graphics=%s\n"
        "; gameplay: true | false — DOS 70 FPS physics/timer tuning\n"
        "gameplay=%s\n"
        "\n"
        "[arcade]           ; coin-op mode (PC-only). See docs/ARCADE.md\n"
        "; enabled: true | false — boot straight to attract mode, no main menu\n"
        "enabled=%s\n"
        "; cabinet key bindings (SDL key names, e.g. 5, 6, 1, 2, F2; case-insensitive).\n"
        "; Default layout matches MAME: 5/6 = insert coin P1/P2, 1/2 = start P1/P2.\n"
        "key_p1_credit=%s\n"
        "key_p1_start=%s\n"
        "key_p2_credit=%s\n"
        "key_p2_start=%s\n"
        "key_service=%s\n"
        "\n"
        "[general]\n"
        "; language: en | de | fr | es | it | nl\n"
        "language=%s\n"
        "; currency: pound | dollar | euro\n"
        "currency=%s\n",
        renderer_str(opts->renderer),
        opts->vsync ? "on" : "off",
        fullscreen_str(fs_now),
        scale_now,
        opts->widescreen ? "true" : "false",
        res_buf,
        opts->audio_mono ? "mono" : "stereo",
        opts->dos_dir[0] ? opts->dos_dir : "grafs_dos",
        opts->dos_graphics ? "true" : "false",
        opts->dos_gameplay ? "true" : "false",
        opts->arcade ? "true" : "false",
        opts->arcade_key_p1_credit[0] ? opts->arcade_key_p1_credit : "5",
        opts->arcade_key_p1_start[0]  ? opts->arcade_key_p1_start  : "1",
        opts->arcade_key_p2_credit[0] ? opts->arcade_key_p2_credit : "6",
        opts->arcade_key_p2_start[0]  ? opts->arcade_key_p2_start  : "2",
        opts->arcade_key_service[0]   ? opts->arcade_key_service   : "F2",
        opts->language[0] ? opts->language : "en",
        opts->currency[0] ? opts->currency : "pound");

    fclose(f);
}
