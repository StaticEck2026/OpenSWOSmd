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
 *
 * The writer (Settings_Save) is NON-DESTRUCTIVE: it rewrites the file
 * line-by-line, changing only the values of known keys that actually changed
 * and appending keys the file is missing. Comments, blank lines, commented-out
 * keys (";resolution=...") and unknown keys are preserved verbatim and in place.
 */

#include "swos_settings.h"
#include "swos_io.h"   /* swos_save_commit — Switch save durability */
#include "ini.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Arcade game-mode name<->index (defined in arcade.c) for [arcade] mode. */
extern const char *Arcade_ModeName(int idx);
extern int         Arcade_ModeFromName(const char *name);

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
        } else if (eqi(name, "render_driver")) {
            snprintf(opts->render_driver, sizeof opts->render_driver, "%s", value);
        } else if (eqi(name, "fullscreen")) {
            if      (eqi(value, "off"))       opts->fullscreen = PLATFORM_FULLSCREEN_NONE;
            else if (eqi(value, "desktop"))   opts->fullscreen = PLATFORM_FULLSCREEN_DESKTOP;
            else if (eqi(value, "exclusive")) opts->fullscreen = PLATFORM_FULLSCREEN_EXCLUSIVE;
        } else if (eqi(name, "scale")) {
            int n = atoi(value);
            if (n >= 1 && n <= 4) opts->win_scale = n;
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
        } else if (eqi(name, "screen")) {
            /* Which monitor to open on (0 = primary). Out-of-range is clamped to
             * a valid display at init, so a stale index just falls back to 0. */
            int n = atoi(value);
            if (n >= 0) opts->screen = n;
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
        else if (eqi(name, "mode") && value && value[0]) {
            int m = Arcade_ModeFromName(value); if (m >= 0) opts->arcade_mode = m;
        }
        else if (eqi(name, "game_seconds")) {
            int v = atoi(value); if (v >= 10 && v <= 600) opts->arcade_game_seconds = v;
        }
        else if (eqi(name, "golden_goal") && parse_bool(value, &b)) opts->arcade_golden_goal = b;
        else if (eqi(name, "penalties_on_draw") && parse_bool(value, &b)) opts->arcade_penalties = b;
        else if (eqi(name, "goal_replay") && parse_bool(value, &b)) opts->arcade_goal_replay = b;
        else if (eqi(name, "attract_audio") && value && value[0]) {
            if (eqi(value, "music+sfx") || eqi(value, "both")) opts->arcade_attract_audio = 2;
            else if (eqi(value, "sfx")) opts->arcade_attract_audio = 1;
            else if (eqi(value, "music")) opts->arcade_attract_audio = 0;
        }
        else if (eqi(name, "gameplay") && value && value[0]) {
            if (eqi(value, "dos")) opts->arcade_gameplay = 1;
            else if (eqi(value, "amiga")) opts->arcade_gameplay = 0;
        }
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

/* ===========================================================================
 * Non-destructive writer
 *
 * settings_build() is the single source of truth for the file: section, key,
 * comment and the current value (from `opts`). Settings_Save() uses it both to
 * create a fresh file and to update an existing one in place.
 * ========================================================================= */

typedef struct { const char *section, *key, *comment, *value; } IniEntry;

#define SETTINGS_MAX 32

/* Inline comment appended to a section-header line in a freshly-created file. */
static const char *section_intro(const char *sec)
{
    if (eqi(sec, "dos"))    return "              ; PC-only extras (Amiga is the definitive version)";
    if (eqi(sec, "arcade")) return "           ; coin-op mode (PC-only). See docs/ARCADE.md";
    return "";
}

/* Fill `items` with the canonical list and return the count. `vbuf` provides
 * scratch storage for the three non-constant values (resolution, scale,
 * game_seconds) and must outlive any use of `items`. */
static int settings_build(const PlatformInitOptions *opts, IniEntry *items, char vbuf[4][24])
{
    int scale_now = Platform_GetWindowScale();
    PlatformFullscreenMode fs_now = Platform_GetFullscreenMode();
    if (scale_now < 1 || scale_now > 4) scale_now = opts->win_scale;

    if (opts->res_w > 0 && opts->res_h > 0)
        snprintf(vbuf[0], 24, "%dx%d", opts->res_w, opts->res_h);
    else
        snprintf(vbuf[0], 24, "auto");
    snprintf(vbuf[1], 24, "%d", scale_now);
    snprintf(vbuf[2], 24, "%d", opts->arcade_game_seconds > 0 ? opts->arcade_game_seconds : 45);
    snprintf(vbuf[3], 24, "%d", opts->screen > 0 ? opts->screen : 0);

    int n = 0;
    items[n++] = (IniEntry){ "video",  "renderer",   "; renderer: software | accel  (hardware-accelerated)", renderer_str(opts->renderer) };
    items[n++] = (IniEntry){ "video",  "vsync",      "; vsync: on | off  (accel renderer only)", opts->vsync ? "on" : "off" };
    items[n++] = (IniEntry){ "video",  "render_driver", "; render_driver: auto | opengl | direct3d11 | direct3d | software-backend. auto = SDL default (D3D11 on Windows). Use opengl if replays/attract run in slow motion on Windows", opts->render_driver[0] ? opts->render_driver : "auto" };
    items[n++] = (IniEntry){ "video",  "fullscreen", "; fullscreen: off | desktop | exclusive", fullscreen_str(fs_now) };
    items[n++] = (IniEntry){ "video",  "scale",      "; scale: window scale 1-4 (windowed only)", vbuf[1] };
    items[n++] = (IniEntry){ "video",  "resolution", "; resolution: WxH (e.g. 2560x1080) drives the render aspect, or 'auto' for the desktop. Any value works — it need not match the monitor", vbuf[0] };
    items[n++] = (IniEntry){ "video",  "screen",     "; screen: monitor index to open on, 0 = primary. Auto-clamped if out of range; window size auto-fits that screen", vbuf[3] };
    items[n++] = (IniEntry){ "audio",  "mode",       "; mode: mono | stereo", opts->audio_mono ? "mono" : "stereo" };
    items[n++] = (IniEntry){ "dos",    "directory",  "; directory: folder under bin/ to load the DOS assets from", opts->dos_dir[0] ? opts->dos_dir : "grafs_dos" };
    items[n++] = (IniEntry){ "dos",    "graphics",   "; graphics: true | false — render the DOS-version graphics", opts->dos_graphics ? "true" : "false" };
    items[n++] = (IniEntry){ "dos",    "gameplay",   "; gameplay: true | false — DOS 70 FPS physics/timer tuning", opts->dos_gameplay ? "true" : "false" };
    items[n++] = (IniEntry){ "arcade", "enabled",    "; enabled: true | false — boot straight to attract mode, no main menu", opts->arcade ? "true" : "false" };
    items[n++] = (IniEntry){ "arcade", "mode",       "; mode: arcade game mode (F2 service menu). Currently: GOLDEN GOAL", Arcade_ModeName(opts->arcade_mode) };
    items[n++] = (IniEntry){ "arcade", "game_seconds", "; game rules (F2 service menu). game_seconds 10..600;  golden_goal/penalties_on_draw/goal_replay: true | false", vbuf[2] };
    items[n++] = (IniEntry){ "arcade", "golden_goal", NULL, opts->arcade_golden_goal ? "true" : "false" };
    items[n++] = (IniEntry){ "arcade", "penalties_on_draw", NULL, opts->arcade_penalties ? "true" : "false" };
    items[n++] = (IniEntry){ "arcade", "goal_replay", NULL, opts->arcade_goal_replay ? "true" : "false" };
    items[n++] = (IniEntry){ "arcade", "attract_audio",
        "; attract_audio: music (HERO only) | sfx (reel crowd only) | music+sfx (both, default)",
        opts->arcade_attract_audio == 2 ? "music+sfx" : opts->arcade_attract_audio == 1 ? "sfx" : "music" };
    items[n++] = (IniEntry){ "arcade", "gameplay",   "; gameplay: amiga (50fps feel) | dos (70fps). Picks attract_mode/<mode> reels too", opts->arcade_gameplay ? "dos" : "amiga" };
    items[n++] = (IniEntry){ "arcade", "key_p1_credit", "; cabinet key bindings (SDL key names; MAME layout 5/6 = coin P1/P2, 1/2 = start, F2 = service)", opts->arcade_key_p1_credit[0] ? opts->arcade_key_p1_credit : "5" };
    items[n++] = (IniEntry){ "arcade", "key_p1_start", NULL, opts->arcade_key_p1_start[0]  ? opts->arcade_key_p1_start  : "1" };
    items[n++] = (IniEntry){ "arcade", "key_p2_credit", NULL, opts->arcade_key_p2_credit[0] ? opts->arcade_key_p2_credit : "6" };
    items[n++] = (IniEntry){ "arcade", "key_p2_start", NULL, opts->arcade_key_p2_start[0]  ? opts->arcade_key_p2_start  : "2" };
    items[n++] = (IniEntry){ "arcade", "key_service", NULL, opts->arcade_key_service[0]   ? opts->arcade_key_service   : "F2" };
    items[n++] = (IniEntry){ "general","language",   "; language: en | de | fr | es | it | nl", opts->language[0] ? opts->language : "en" };
    items[n++] = (IniEntry){ "general","currency",   "; currency: pound | dollar | euro", opts->currency[0] ? opts->currency : "pound" };
    return n;
}

/* Create a full default file. Only used when SWOS.ini doesn't exist yet. */
static void settings_write_full(const PlatformInitOptions *opts, const char *path)
{
    IniEntry items[SETTINGS_MAX];
    char vbuf[4][24];
    int n = settings_build(opts, items, vbuf);

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[settings] cannot create %s: %s\n", path, strerror(errno));
        return;
    }
    fprintf(f,
        "; SWOS — display, audio and gameplay settings.\n"
        "; Hand-edit freely: SWOS only updates values you change and adds missing\n"
        "; keys; your comments, blank lines and commented-out keys are preserved.\n"
        "; Booleans accept true/false, on/off, yes/no, 1/0 (case-insensitive).\n");
    const char *cur = NULL;
    for (int i = 0; i < n; i++) {
        if (!cur || !eqi(cur, items[i].section)) {
            cur = items[i].section;
            fprintf(f, "\n[%s]%s\n", cur, section_intro(cur));
        }
        if (items[i].comment) fprintf(f, "%s\n", items[i].comment);
        fprintf(f, "%s=%s\n", items[i].key, items[i].value);
    }
    fclose(f);
}

/* Trim a value string (after '=') into `out`: skip leading blanks, stop at
 * end-of-line, drop trailing blanks. */
static void copy_value(const char *v, char *out, size_t outsz)
{
    while (*v == ' ' || *v == '\t') v++;
    size_t len = 0;
    while (v[len] && v[len] != '\n' && v[len] != '\r' && len + 1 < outsz) len++;
    while (len > 0 && (v[len-1] == ' ' || v[len-1] == '\t')) len--;
    memcpy(out, v, len);
    out[len] = '\0';
}

/* Write every still-missing key that belongs to section `sec` (comment + value),
 * marking each seen. Called when Pass 2 leaves a section block, so a missing key
 * is inserted into the FIRST existing occurrence of its section rather than in a
 * brand-new duplicate section at the end of the file. */
static void flush_section_missing(FILE *out, const char *sec,
                                  const IniEntry *items, int n, unsigned char *seen)
{
    for (int k = 0; k < n; k++) {
        if (seen[k] || !eqi(items[k].section, sec)) continue;
        if (items[k].comment) fprintf(out, "%s\n", items[k].comment);
        fprintf(out, "%s=%s\n", items[k].key, items[k].value);
        seen[k] = 1;
    }
}

/*
 * Non-destructive save. If `path` is absent, write a full default file.
 * Otherwise rewrite it line-by-line: update ONLY the values of known keys whose
 * value changed, drop duplicate occurrences of a known key, add each missing key
 * to the end of the FIRST block of its section (a new section only if the file
 * has none), and preserve everything else (comments, blank lines, commented-out
 * keys, unknown keys, ordering) verbatim. If nothing changed, nothing is missing
 * and nothing is duplicated, the file is left untouched.
 */
void Settings_Save(const PlatformInitOptions *opts, const char *path)
{
    if (!opts || !path) return;

    IniEntry items[SETTINGS_MAX];
    char vbuf[4][24];
    int n = settings_build(opts, items, vbuf);

    /* Slurp the existing file. */
    static char lines[400][256];
    int n_lines = 0;
    FILE *f = fopen(path, "r");
    if (!f) { settings_write_full(opts, path); return; }   /* absent -> create it */
    while (n_lines < 400 && fgets(lines[n_lines], (int)sizeof lines[0], f))
        n_lines++;
    fclose(f);

    /* Pass 1: detect whether anything actually needs writing, and which keys
     * the file already has. This keeps an unchanged file (the common case)
     * byte-for-byte identical — not even the mtime moves. */
    unsigned char seen[SETTINGS_MAX];
    memset(seen, 0, sizeof seen);
    int dirty = 0;
    {
        char cur_sec[32] = "";
        for (int i = 0; i < n_lines; i++) {
            const char *t = lines[i];
            while (*t == ' ' || *t == '\t') t++;
            if (*t == '[') {
                const char *e = strchr(t, ']');
                if (e) {
                    int len = (int)(e - (t + 1));
                    if (len < 0) len = 0; else if (len > 31) len = 31;
                    memcpy(cur_sec, t + 1, (size_t)len); cur_sec[len] = '\0';
                }
                continue;
            }
            const char *eq;
            if (*t == ';' || *t == '#' || *t == '\0' || *t == '\n' || *t == '\r'
                || (eq = strchr(t, '=')) == NULL)
                continue;
            int klen = (int)(eq - t);
            while (klen > 0 && (t[klen-1] == ' ' || t[klen-1] == '\t')) klen--;
            char key[48];
            if (klen > 47) klen = 47;
            memcpy(key, t, (size_t)klen); key[klen] = '\0';
            for (int k = 0; k < n; k++) {
                if (eqi(cur_sec, items[k].section) && eqi(key, items[k].key)) {
                    if (seen[k]) { dirty = 1; break; }  /* duplicate known key -> clean it */
                    seen[k] = 1;
                    char fileval[64];
                    copy_value(eq + 1, fileval, sizeof fileval);
                    if (strcmp(fileval, items[k].value) != 0) dirty = 1;
                    break;
                }
            }
        }
        for (int k = 0; k < n; k++) if (!seen[k]) { dirty = 1; break; }
    }
    if (!dirty) return;   /* nothing to change — leave the file exactly as it is */

    /* Pass 2: rewrite, updating changed values and preserving everything else. */
    memset(seen, 0, sizeof seen);
    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "[settings] cannot write %s: %s\n", path, strerror(errno));
        return;
    }
    char cur_sec[32] = "";
    for (int i = 0; i < n_lines; i++) {
        char *ln = lines[i];
        const char *t = ln;
        while (*t == ' ' || *t == '\t') t++;

        if (*t == '[') {                                   /* section header */
            /* Leaving cur_sec: drop any of its still-missing keys in HERE, at the
             * end of this (the first) block of the section — not in a duplicate
             * section appended to the file's tail. */
            if (cur_sec[0]) flush_section_missing(out, cur_sec, items, n, seen);
            const char *e = strchr(t, ']');
            if (e) {
                int len = (int)(e - (t + 1));
                if (len < 0) len = 0; else if (len > 31) len = 31;
                memcpy(cur_sec, t + 1, (size_t)len); cur_sec[len] = '\0';
            }
            fputs(ln, out);
            continue;
        }
        const char *eq;
        if (*t == ';' || *t == '#' || *t == '\0' || *t == '\n' || *t == '\r'
            || (eq = strchr(t, '=')) == NULL) {
            fputs(ln, out);                                /* comment / blank / no '=' */
            continue;
        }
        int klen = (int)(eq - t);
        while (klen > 0 && (t[klen-1] == ' ' || t[klen-1] == '\t')) klen--;
        char key[48];
        if (klen > 47) klen = 47;
        memcpy(key, t, (size_t)klen); key[klen] = '\0';

        int idx = -1;
        for (int k = 0; k < n; k++)
            if (eqi(cur_sec, items[k].section) && eqi(key, items[k].key)) { idx = k; break; }
        if (idx < 0) { fputs(ln, out); continue; }         /* unknown key -> verbatim */

        if (seen[idx]) continue;    /* duplicate of a known key already written -> drop it */

        seen[idx] = 1;
        char fileval[64];
        copy_value(eq + 1, fileval, sizeof fileval);
        if (strcmp(fileval, items[idx].value) == 0) {
            fputs(ln, out);                                /* unchanged -> verbatim */
        } else {
            size_t prefix = (size_t)(eq - ln) + 1;         /* keep "key=" verbatim */
            fwrite(ln, 1, prefix, out);
            fprintf(out, "%s\n", items[idx].value);        /* ...replace the value */
        }
    }
    /* EOF: flush the final section's still-missing keys into its block. */
    if (cur_sec[0]) flush_section_missing(out, cur_sec, items, n, seen);

    /* Append keys whose section never appeared in the file at all (a genuinely
     * new section), with a fresh header. Keys for sections already present were
     * injected in-place above, so this never duplicates an existing section. */
    int missing = 0;
    for (int k = 0; k < n; k++) if (!seen[k]) missing++;
    if (missing) {
        fputs("\n; --- keys added by a newer SWOS build (defaults; edit as you like) ---\n", out);
        const char *cur = NULL;
        for (int k = 0; k < n; k++) {
            if (seen[k]) continue;
            if (!cur || !eqi(cur, items[k].section)) {
                cur = items[k].section;
                fprintf(out, "[%s]\n", cur);
            }
            if (items[k].comment) fprintf(out, "%s\n", items[k].comment);
            fprintf(out, "%s=%s\n", items[k].key, items[k].value);
        }
    }
    fclose(out);
    /* Closed first — the Switch's save commit requires it, and without the
     * commit SWOS.ini reads back fine all session and is gone next launch.
     * Nothing on any other target (swos_io.h). */
    swos_save_commit();
}

/* Back-compat alias for the startup/shutdown path: now identical to the
 * non-destructive Settings_Save. */
void Settings_AppendMissing(const PlatformInitOptions *opts, const char *path)
{
    Settings_Save(opts, path);
}
