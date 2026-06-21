/* ======================================================================
 * menu_load_json.c — JSON menu loader (SDL build), docs/MENUS.md §2d
 *
 * Reads bin/data_json/menus/<name>.json and builds the packed "§0"
 * byte-stream that _UnpackMenu (menu_system.c) walks — the SAME stream the
 * compiled static `Menu_Foo fooMenu = {…}` initializers lay down. Pointer
 * payloads (callbacks, strings, string-tables, sprite-copy structs, buffers)
 * are stored in the JSON as SYMBOL NAMES and re-resolved here through the
 * generated registry (menu_symbols.c / menu_resolve) — the menu analogue of
 * the save format's "no addresses on disk" rule.
 *
 * This increment is VERIFY-FIRST (the SWOS_SAVE_FORMAT_JSON_VERIFY discipline):
 * MenuJson_VerifyAll() builds each menu from JSON and memcmp's it against the
 * compiled static bytes. A zero-diff across all 47 menus is the green light to
 * retire the static menu_data.c initializers (MENUS.md §2d/§3b). Gated on env
 * SWOS_MENU_JSON; without it nothing runs and the static menus stay
 * authoritative, so the migration is incremental and reversible.
 *
 * Byte order is HOST-NATIVE here: the SDL build reads back its own freshly
 * built buffer, and _UnpackMenu reads pointers via memcpy of sizeof(uintptr_t).
 * The Amiga (BE, 4-byte ptr) path is the Python emitter, docs/MENUS.md §3.
 * ====================================================================== */

#include "mjson.h"
#include "menu_symbols.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- packed-stream markers / opcode count (mirror _UnpackMenu) ---- */
#define MK_END        0xFC19u   /* 64537 */
#define MK_SET_ORIGIN 0xFC1Au   /* 64538 */
#define MK_TEMPLATE   0xFC1Bu   /* 64539 — start template */
#define MK_RESET_TMPL 0xFC1Cu   /* 64540 — reset template */
#define MK_COORDS_BYTE 0xFFFFu  /* compact coords */
#define MK_COORDS_WIDE 0xFFFEu  /* word coords (default) */

/* ======================================================================
 * Byte writer — host-native little-endian on the SDL build.
 * ====================================================================== */
typedef struct { uint8_t *p; uint8_t *end; int overflow; } MWriter;

static void w8(MWriter *w, unsigned v)
{
    if (w->p + 1 > w->end) { w->overflow = 1; return; }
    *w->p++ = (uint8_t)v;
}
static void w16(MWriter *w, unsigned v)
{
    if (w->p + 2 > w->end) { w->overflow = 1; return; }
    w->p[0] = (uint8_t)(v & 0xFF);
    w->p[1] = (uint8_t)((v >> 8) & 0xFF);
    w->p += 2;
}
static void w32(MWriter *w, uint32_t v)
{
    if (w->p + 4 > w->end) { w->overflow = 1; return; }
    w->p[0] = (uint8_t)(v & 0xFF);
    w->p[1] = (uint8_t)((v >> 8) & 0xFF);
    w->p[2] = (uint8_t)((v >> 16) & 0xFF);
    w->p[3] = (uint8_t)((v >> 24) & 0xFF);
    w->p += 4;
}
/* Native pointer-width slot — _UnpackMenu reads these via memcpy(sizeof(uintptr_t)). */
static void wptr(MWriter *w, void *ptr)
{
    if (w->p + sizeof(uintptr_t) > w->end) { w->overflow = 1; return; }
    memcpy(w->p, &ptr, sizeof(uintptr_t));
    w->p += sizeof(uintptr_t);
}

/* ======================================================================
 * mjson field readers — integers may be JSON numbers OR hex strings ("0x10").
 * Uses mjson_get_long (integer-only; works on the FPU-less m68k back-port too).
 * ====================================================================== */
static int mj_long(const char *j, int jl, const char *path, long *out)
{
    const char *tp; int tl;
    int t = mjson_find(j, jl, path, &tp, &tl);
    if (t == MJSON_TOK_NUMBER)
        return mjson_get_long(j, jl, path, out);
    if (t == MJSON_TOK_STRING) {
        char tmp[24];
        if (mjson_get_string(j, jl, path, tmp, (int)sizeof tmp) <= 0) return 0;
        *out = strtol(tmp, NULL, 0);        /* base 0 → 0x.. and decimal */
        return 1;
    }
    return 0;
}
static long mj_long_def(const char *j, int jl, const char *path, long def)
{
    long v;
    return mj_long(j, jl, path, &v) ? v : def;
}
static int mj_bool_def(const char *j, int jl, const char *path, int def)
{
    int v;
    return mjson_get_bool(j, jl, path, &v) ? v : def;
}

/* Emit a pointer payload from a field that is a symbol NAME (resolved via the
 * registry), a literal INTEGER (a small deferred-string sentinel 1..255 that
 * _UnpackMenu rewrites to 0xFFFFFFFF), or null/absent (a NULL pointer). */
static void wfield_ptr(MWriter *w, const char *j, int jl, const char *path,
                       const char *menuName);

/* Resolve a name field ("$.func"/"$.str"/…) to its address; NULL if the field
 * is absent / JSON null / unresolved (a missing symbol is logged, not fatal). */
static void *mj_sym(const char *j, int jl, const char *path, const char *menuName)
{
    const char *tp; int tl;
    if (mjson_find(j, jl, path, &tp, &tl) != MJSON_TOK_STRING)
        return NULL;
    char name[80];
    if (mjson_get_string(j, jl, path, name, (int)sizeof name) <= 0)
        return NULL;
    void *p = menu_resolve(name);
    if (!p)
        fprintf(stderr, "[menu-json] %s: unresolved symbol '%s'\n", menuName, name);
    return p;
}

static void wfield_ptr(MWriter *w, const char *j, int jl, const char *path,
                       const char *menuName)
{
    const char *tp; int tl;
    int t = mjson_find(j, jl, path, &tp, &tl);
    if (t == MJSON_TOK_NUMBER) {           /* literal deferred-string sentinel (e.g. 4) */
        long v = 0;
        mjson_get_long(j, jl, path, &v);
        if (w->p + sizeof(uintptr_t) > w->end) { w->overflow = 1; return; }
        uintptr_t pv = (uintptr_t)(intptr_t)v;
        memcpy(w->p, &pv, sizeof pv);
        w->p += sizeof pv;
        return;
    }
    if (t == MJSON_TOK_STRING)             /* symbol name → registry address */
        wptr(w, mj_sym(j, jl, path, menuName));
    else                                   /* null / absent → NULL pointer */
        wptr(w, NULL);
}

/* ---- opcode-name -> number (MENU_CMD_*; see _UnpackMenu cases 0..22) ---- */
static int op_num(const char *op)
{
    if (!strcmp(op, "invisible"))      return 1;
    if (!strcmp(op, "nav"))            return 2;
    if (!strcmp(op, "bg_func"))        return 3;
    if (!strcmp(op, "colors"))         return 4;
    if (!strcmp(op, "sprite_bg"))      return 5;
    if (!strcmp(op, "invalid"))        return 6;
    if (!strcmp(op, "fg_func"))        return 7;
    if (!strcmp(op, "text"))           return 8;
    if (!strcmp(op, "sprite_fg"))      return 9;
    if (!strcmp(op, "string_table"))   return 10;
    if (!strcmp(op, "multiline"))      return 11;
    if (!strcmp(op, "number"))         return 12;
    if (!strcmp(op, "onselect_fire"))  return 13;
    if (!strcmp(op, "input_func"))     return 14;
    if (!strcmp(op, "before_draw"))    return 15;
    if (!strcmp(op, "after_draw"))     return 16;
    if (!strcmp(op, "up_skip"))        return 17;   /* MENU_CMD_UP_SKIP    = 0x11 */
    if (!strcmp(op, "down_skip"))      return 18;   /* MENU_CMD_DOWN_SKIP  = 0x12 */
    if (!strcmp(op, "left_skip"))      return 19;   /* MENU_CMD_LEFT_SKIP  = 0x13 */
    if (!strcmp(op, "right_skip"))     return 20;   /* MENU_CMD_RIGHT_SKIP = 0x14 */
    if (!strcmp(op, "sprite_copy"))    return 21;
    if (!strcmp(op, "text_no_ptr"))    return 22;
    return -1;
}

/* Emit one command (op word + its payload). cmd/cmdl is the command object slice. */
static void emit_command(MWriter *w, const char *cmd, int cmdl, const char *menuName)
{
    char op[24];
    if (mjson_get_string(cmd, cmdl, "$.op", op, (int)sizeof op) <= 0) return;
    int n = op_num(op);
    if (n < 0) { fprintf(stderr, "[menu-json] %s: unknown op '%s'\n", menuName, op); return; }
    w16(w, (unsigned)n);
    switch (n) {
    case 1:  /* invisible — no payload */
        break;
    case 2:  /* nav: i8 left,right,up,down */
        w8(w, (unsigned)mj_long_def(cmd, cmdl, "$.left",  0));
        w8(w, (unsigned)mj_long_def(cmd, cmdl, "$.right", 0));
        w8(w, (unsigned)mj_long_def(cmd, cmdl, "$.up",    0));
        w8(w, (unsigned)mj_long_def(cmd, cmdl, "$.down",  0));
        break;
    case 3:  /* bg_func: ptr */
        wfield_ptr(w, cmd, cmdl, "$.func", menuName);
        break;
    case 4:  /* colors: u16 */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.value", 0));
        break;
    case 5:  /* sprite_bg: u16 */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.index", mj_long_def(cmd, cmdl, "$.value", 0)));
        break;
    case 6:  /* invalid: u16 */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.value", 0));
        break;
    case 7:  /* fg_func: ptr */
        wfield_ptr(w, cmd, cmdl, "$.func", menuName);
        break;
    case 8:  /* text: u16 flags, ptr str */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.flags", 0));
        wfield_ptr(w, cmd, cmdl, "$.str", menuName);
        break;
    case 9:  /* sprite_fg: u16 index */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.index", 0));
        break;
    case 10: /* string_table: u16 flags, ptr table */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.flags", 0));
        wfield_ptr(w, cmd, cmdl, "$.table", menuName);
        break;
    case 11: /* multiline: u16 flags, ptr str */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.flags", 0));
        wfield_ptr(w, cmd, cmdl, "$.str", menuName);
        break;
    case 12: /* number: u16 flags, u16 value */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.flags", 0));
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.value", 0));
        break;
    case 13: /* onselect_fire: ptr */
        wfield_ptr(w, cmd, cmdl, "$.func", menuName);
        break;
    case 14: /* input_func: u16 mask, ptr */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.mask", 0));
        wfield_ptr(w, cmd, cmdl, "$.func", menuName);
        break;
    case 15: /* before_draw: ptr */
        wfield_ptr(w, cmd, cmdl, "$.func", menuName);
        break;
    case 16: /* after_draw: ptr */
        wfield_ptr(w, cmd, cmdl, "$.func", menuName);
        break;
    case 17: case 18: case 19: case 20: /* *_skip: u8 skip, u8 dir */
        w8(w, (unsigned)mj_long_def(cmd, cmdl, "$.skip", 0));
        w8(w, (unsigned)mj_long_def(cmd, cmdl, "$.dir",  0));
        break;
    case 21: /* sprite_copy: ptr */
        wfield_ptr(w, cmd, cmdl, "$.sprite", menuName);
        break;
    case 22: /* text_no_ptr: u16 flags (deferred string; _UnpackMenu sets fg=0xFFFFFFFF) */
        w16(w, (unsigned)mj_long_def(cmd, cmdl, "$.flags", 0));
        break;
    default:
        break;
    }
}

/* Emit one entry's coords (current byte/word mode) + its command list + the
 * MENU_CMD_END (opcode 0) terminator. `it`/`itl` is the entry object slice. */
static void emit_entry(MWriter *w, const char *it, int itl, int byteMode, const char *menuName)
{
    long x = mj_long_def(it, itl, "$.coords.x", 0);
    long y = mj_long_def(it, itl, "$.coords.y", 0);
    long cw = mj_long_def(it, itl, "$.coords.w", 0);
    long ch = mj_long_def(it, itl, "$.coords.h", 0);
    if (byteMode) {
        w8(w, (unsigned)(x & 0xFF));  w8(w, (unsigned)(y & 0xFF));
        w8(w, (unsigned)(cw & 0xFF)); w8(w, (unsigned)(ch & 0xFF));
    } else {
        w16(w, (unsigned)(x & 0xFFFF));  w16(w, (unsigned)(y & 0xFFFF));
        w16(w, (unsigned)(cw & 0xFFFF)); w16(w, (unsigned)(ch & 0xFFFF));
    }
    const char *cmds; int cmdsl;
    if (mjson_find(it, itl, "$.commands", &cmds, &cmdsl) == MJSON_TOK_ARRAY) {
        int off = 0, ko, kl, vo, vl, vt;
        while ((off = mjson_next(cmds, cmdsl, off, &ko, &kl, &vo, &vl, &vt)) != 0) {
            if (vt != MJSON_TOK_OBJECT) continue;
            emit_command(w, cmds + vo, vl, menuName);
        }
    }
    w16(w, 0);   /* MENU_CMD_END */
}

/* ======================================================================
 * MenuJson_BuildPacked — JSON object -> packed §0 byte-stream.
 * Returns the byte length (incl. the trailing end marker), or -1 on error.
 * ====================================================================== */
int MenuJson_BuildPacked(const char *j, int jl, uint8_t *out, int outcap)
{
    MWriter w = { out, out + outcap, 0 };
    char menuName[64];
    if (mjson_get_string(j, jl, "$.name", menuName, (int)sizeof menuName) <= 0)
        menuName[0] = '\0';

    /* Header: onInit, onReturn, onDraw (native ptr), then u32 defaultEntry. */
    wptr(&w, mj_sym(j, jl, "$.onInit",   menuName));
    wptr(&w, mj_sym(j, jl, "$.onReturn", menuName));
    wptr(&w, mj_sym(j, jl, "$.onDraw",   menuName));
    w32(&w, (uint32_t)mj_long_def(j, jl, "$.defaultEntry", 0));

    const char *items; int itemsl;
    if (mjson_find(j, jl, "$.items", &items, &itemsl) != MJSON_TOK_ARRAY)
        return -1;

    int byteMode = 0;       /* 0 = word coords (default), persists like _UnpackMenu */
    int sawEnd = 0;
    int off = 0, ko, kl, vo, vl, vt;
    while ((off = mjson_next(items, itemsl, off, &ko, &kl, &vo, &vl, &vt)) != 0) {
        if (vt != MJSON_TOK_OBJECT) continue;
        const char *it = items + vo; int itl = vl;
        char kind[24];
        if (mjson_get_string(it, itl, "$.kind", kind, (int)sizeof kind) <= 0) continue;

        if (!strcmp(kind, "entry")) {
            emit_entry(&w, it, itl, byteMode, menuName);
        } else if (!strcmp(kind, "set_origin")) {
            w16(&w, MK_SET_ORIGIN);
            w16(&w, (unsigned)(mj_long_def(it, itl, "$.x", 0) & 0xFFFF));
            w16(&w, (unsigned)(mj_long_def(it, itl, "$.y", 0) & 0xFFFF));
        } else if (!strcmp(kind, "coords_mode")) {
            int wide = mj_bool_def(it, itl, "$.wide", 1);
            byteMode = !wide;
            w16(&w, wide ? MK_COORDS_WIDE : MK_COORDS_BYTE);
        } else if (!strcmp(kind, "start_template")) {
            w16(&w, MK_TEMPLATE);
        } else if (!strcmp(kind, "reset_template")) {
            w16(&w, MK_RESET_TMPL);
        } else if (!strcmp(kind, "end")) {
            w16(&w, MK_END);
            sawEnd = 1;
            break;
        } else {
            fprintf(stderr, "[menu-json] %s: unknown item kind '%s'\n", menuName, kind);
        }
    }
    if (!sawEnd)
        w16(&w, MK_END);     /* defensively terminate if the JSON omitted it */

    if (w.overflow) return -1;
    return (int)(w.p - out);
}

/* ======================================================================
 * Verify: build every menu from JSON and compare against the compiled static
 * bytes (MENUS.md §2d/§3b). Pointer slots match exactly because menu_resolve
 * returns the same link-time addresses the static initializers baked in.
 * ====================================================================== */

/* --- the 47 compiled static menus, by name (still authoritative until the
 *     round-trip is proven green and menu_data.c is retired) --- */
extern uint8_t buyOtherPlayersMenu[];       extern uint8_t buyPlayerMenu[];             extern uint8_t careerMenu[];
extern uint8_t careerOverMenu[];            extern uint8_t chairmanMemosMenu[];         extern uint8_t chooseTacticsMenu[];
extern uint8_t chooseTeamsFriendlyMenu[];   extern uint8_t chooseTeamsFriendlyMenu2[];  extern uint8_t chooseTeamsMenu[];
extern uint8_t clubBusinessMenu[];          extern uint8_t competitionStatsMenu[];      extern uint8_t competitionTopGoalScorersMenu[];
extern uint8_t contestMenu[];               extern uint8_t continueAbortMenu[];         extern uint8_t continueMenu[];
extern uint8_t designDiyCupMenu[];          extern uint8_t designDiyLeagueMenu[];       extern uint8_t designDiyTournamentMenu[];
extern uint8_t diyCupMenu[];                extern uint8_t diyLeagueTournamentMenu[];   extern uint8_t editTacticsMenu[];
extern uint8_t editTeamsMenu[];             extern uint8_t friendlyGameMenu[];          extern uint8_t gotJobOfferMenu[];
extern uint8_t highlightsMenu[];            extern uint8_t importTacticsMenu[];         extern uint8_t jobOffersMenu[];
extern uint8_t managementRecordMenu[];      extern uint8_t nationalTeamAddPlayerMenu[]; extern uint8_t offersMenu[];
extern uint8_t optionsMenu[];               extern uint8_t playMatchMenu[];             extern uint8_t playerOffersMenu[];
extern uint8_t playerTransferMenu[];        extern uint8_t selectFilesMenu[];           extern uint8_t selectNationalTeamMenu[];
extern uint8_t selectTeamsForTacticsMenu[]; extern uint8_t selectTeamsMenu[];           extern uint8_t showPlayerMarketMenu[];
extern uint8_t squadMenu[];                 extern uint8_t stadiumMenu[];               extern uint8_t swosMainMenu[];
extern uint8_t thirdPartyOfferMenu[];       extern uint8_t topGoalScorersMenu[];        extern uint8_t transferMarketMenu[];
extern uint8_t viewCompetitionsMenu[];      extern uint8_t viewSquadMenu[];

static const struct { const char *name; uint8_t *menu; } s_staticMenus[] = {
    { "buyOtherPlayersMenu", buyOtherPlayersMenu },
    { "buyPlayerMenu", buyPlayerMenu },
    { "careerMenu", careerMenu },
    { "careerOverMenu", careerOverMenu },
    { "chairmanMemosMenu", chairmanMemosMenu },
    { "chooseTacticsMenu", chooseTacticsMenu },
    { "chooseTeamsFriendlyMenu", chooseTeamsFriendlyMenu },
    { "chooseTeamsFriendlyMenu2", chooseTeamsFriendlyMenu2 },
    { "chooseTeamsMenu", chooseTeamsMenu },
    { "clubBusinessMenu", clubBusinessMenu },
    { "competitionStatsMenu", competitionStatsMenu },
    { "competitionTopGoalScorersMenu", competitionTopGoalScorersMenu },
    { "contestMenu", contestMenu },
    { "continueAbortMenu", continueAbortMenu },
    { "continueMenu", continueMenu },
    { "designDiyCupMenu", designDiyCupMenu },
    { "designDiyLeagueMenu", designDiyLeagueMenu },
    { "designDiyTournamentMenu", designDiyTournamentMenu },
    { "diyCupMenu", diyCupMenu },
    { "diyLeagueTournamentMenu", diyLeagueTournamentMenu },
    { "editTacticsMenu", editTacticsMenu },
    { "editTeamsMenu", editTeamsMenu },
    { "friendlyGameMenu", friendlyGameMenu },
    { "gotJobOfferMenu", gotJobOfferMenu },
    { "highlightsMenu", highlightsMenu },
    { "importTacticsMenu", importTacticsMenu },
    { "jobOffersMenu", jobOffersMenu },
    { "managementRecordMenu", managementRecordMenu },
    { "nationalTeamAddPlayerMenu", nationalTeamAddPlayerMenu },
    { "offersMenu", offersMenu },
    { "optionsMenu", optionsMenu },
    { "playMatchMenu", playMatchMenu },
    { "playerOffersMenu", playerOffersMenu },
    { "playerTransferMenu", playerTransferMenu },
    { "selectFilesMenu", selectFilesMenu },
    { "selectNationalTeamMenu", selectNationalTeamMenu },
    { "selectTeamsForTacticsMenu", selectTeamsForTacticsMenu },
    { "selectTeamsMenu", selectTeamsMenu },
    { "showPlayerMarketMenu", showPlayerMarketMenu },
    { "squadMenu", squadMenu },
    { "stadiumMenu", stadiumMenu },
    { "swosMainMenu", swosMainMenu },
    { "thirdPartyOfferMenu", thirdPartyOfferMenu },
    { "topGoalScorersMenu", topGoalScorersMenu },
    { "transferMarketMenu", transferMarketMenu },
    { "viewCompetitionsMenu", viewCompetitionsMenu },
    { "viewSquadMenu", viewSquadMenu },
};
#define NUM_STATIC_MENUS ((int)(sizeof s_staticMenus / sizeof s_staticMenus[0]))

static uint8_t *menu_slurp(const char *path, int *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { free(buf); return NULL; }
    *out_len = (int)n;
    return buf;
}

/* Build menu `name` from JSON and memcmp vs its compiled static bytes.
 * Returns 0 = identical, 1 = differs, -1 = error. */
static int menu_verify_one(const char *name, const uint8_t *staticMenu)
{
    char path[160];
    snprintf(path, sizeof path, "data_json/menus/%s.json", name);
    int jl = 0;
    uint8_t *j = menu_slurp(path, &jl);
    if (!j) { fprintf(stderr, "[menu-json] %s: cannot open %s\n", name, path); return -1; }

    static uint8_t packed[16384];   /* largest static menu is well under this */
    int len = MenuJson_BuildPacked((const char *)j, jl, packed, (int)sizeof packed);
    free(j);
    if (len < 0) { fprintf(stderr, "[menu-json] %s: build failed (overflow/parse)\n", name); return -1; }

    if (memcmp(packed, staticMenu, (size_t)len) != 0) {
        int off;
        for (off = 0; off < len; off++)
            if (packed[off] != staticMenu[off]) break;
        fprintf(stderr, "[menu-json] %-30s DIFF at +%d (json=%02X static=%02X) len=%d\n",
                name, off, packed[off], staticMenu[off], len);
        int lo = off - 12; if (lo < 0) lo = 0;
        fprintf(stderr, "   json  :");
        for (int k = lo; k < off + 12 && k < len; k++) fprintf(stderr, " %02X", packed[k]);
        fprintf(stderr, "\n   static:");
        for (int k = lo; k < off + 12 && k < len; k++) fprintf(stderr, " %02X", staticMenu[k]);
        fprintf(stderr, "\n");
        return 1;
    }
    return 0;
}

/* Public entry: verify every corpus menu round-trips to the static bytes.
 * Returns the number of mismatches (0 = all green). Gated by the caller on env. */
int MenuJson_VerifyAll(void)
{
    int pass = 0, diff = 0, err = 0;
    for (int i = 0; i < NUM_STATIC_MENUS; i++) {
        int rc = menu_verify_one(s_staticMenus[i].name, s_staticMenus[i].menu);
        if (rc == 0) pass++;
        else if (rc == 1) diff++;
        else err++;
    }
    printf("[menu-json] verify: %d/%d byte-identical, %d differ, %d error\n",
           pass, NUM_STATIC_MENUS, diff, err);
    return diff + err;
}
