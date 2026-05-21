/*
 * save_format_json.c — JSON serializer/parser for SWOS save files.
 *
 * Status: hybrid bring-up. The careerFileBuffer is round-tripped via
 * a single base64 "opaque_data" field. Semantic field promotion
 * (turning the comp-name pointers in seasonInformations into stable
 * string IDs, breaking out the TeamFileEx records, etc.) lands in
 * follow-up commits. The format is JSON from day 1; every commit
 * shrinks the opaque blob and grows the semantic schema.
 *
 * See SAVE_FORMAT_JSON.md for the migration plan and field-by-field
 * TODO list.
 */

#include "save_format_json.h"
#include "mjson.h"
#include "game_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Externs from the careerFileBuffer overlay. */
extern uint8_t  careerFileBuffer;
extern uint16_t numSelectedTeams;
extern uint8_t  selTeamsPtr;
extern uint8_t  seasonInformationsArray[];

/* Externs for the other save buffers — used by future sub-files but
 * referenced here so the linker keeps the symbols available. */
extern uint8_t  competitionFileBuffer;
extern uint8_t  diyFileBufferCopy[];

/* file_io.c writer helper. */
extern int16_t _WriteFile(uint8_t *filename, uint8_t *src, uint32_t size);
extern void    _ShowFsErrorByCode(uint16_t code);

/* g_amiga_addr_map from generated/amiga_address_map.c — used to
 * convert pointer values to/from stable symbol-name identifiers. */
typedef struct AmigaAddrLutEntry {
    const char *name;
    void       *current_addr;
    uint32_t    amiga_addr;
    uint32_t    size;
} AmigaAddrLutEntry;
extern const AmigaAddrLutEntry g_amiga_addr_map[];
extern const size_t g_amiga_addr_map_count;

/* Resolve a current-build pointer to a stable symbol identity.
 * Returns the entry whose [current_addr, current_addr+size) range
 * contains `val`, with *out_offset set to (val - entry->current_addr).
 * Returns NULL if no entry covers `val` (stale cross-build pointer or
 * non-symbol value). */
static const AmigaAddrLutEntry *sym_from_ptr(uint32_t val, uint32_t *out_offset)
{
    for (size_t i = 0; i < g_amiga_addr_map_count; i++) {
        const AmigaAddrLutEntry *e = &g_amiga_addr_map[i];
        uint32_t base = (uint32_t)(uintptr_t)e->current_addr;
        uint32_t end  = base + (e->size ? e->size : 1u);
        if (val >= base && val < end) {
            if (out_offset) *out_offset = val - base;
            return e;
        }
    }
    return NULL;
}

/* Resolve a stable symbol name back to a current-build pointer. */
static uint32_t ptr_from_sym(const char *name, uint32_t offset)
{
    for (size_t i = 0; i < g_amiga_addr_map_count; i++) {
        const AmigaAddrLutEntry *e = &g_amiga_addr_map[i];
        if (strcmp(e->name, name) == 0) {
            return (uint32_t)(uintptr_t)e->current_addr + offset;
        }
    }
    return 0u;
}

/* String pool for save-load round-trips of .rodata string pointers
 * that aren't in g_amiga_addr_map (comp-name strings like "DIV 1",
 * "BUNDESLIGA", "EC FINALS" — 114 of these live in competition_data.c).
 * On load, JSON `{ "str": "DIV 1" }` fields get a string copied into
 * this pool and the resulting pool address is what we write into the
 * pointer slot. The display code (_StringCopy etc.) just walks bytes
 * to NUL, so any valid backing memory works. Pool is reset at the
 * start of each load. */
#define STRING_POOL_SIZE 8192u
static char     s_string_pool[STRING_POOL_SIZE];
static uint32_t s_string_pool_used = 0;

static void string_pool_reset(void)
{
    s_string_pool_used = 0;
}

static uint32_t string_pool_intern(const char *s)
{
    uint32_t len = (uint32_t)strlen(s);
    if (s_string_pool_used + len + 1 > STRING_POOL_SIZE)
        return 0u;
    char *dst = &s_string_pool[s_string_pool_used];
    memcpy(dst, s, (size_t)len + 1);
    s_string_pool_used += len + 1;
    return (uint32_t)(uintptr_t)dst;
}

/* Heuristic readability check for `.rodata` string pointers we
 * encounter at save time. We use the address of any known LUT entry
 * (`g_amiga_addr_map[0].current_addr`) as a probe and accept pointers
 * within ±16 MB of it — that comfortably covers .text + .rodata on
 * MinGW/Cygwin/Linux without risk of touching unmapped memory.
 *
 * Returns 1 if `val` looks like a valid in-process address, 0 if it
 * looks stale / unmapped (older build's address). */
static int looks_readable(uint32_t val)
{
    if (val == 0u || val == 0xFFFFFFFFu) return 0;
    if (g_amiga_addr_map_count == 0) return 0;
    uint32_t probe = (uint32_t)(uintptr_t)g_amiga_addr_map[0].current_addr;
    uint32_t diff  = (val > probe) ? (val - probe) : (probe - val);
    return diff < 0x01000000u; /* ±16 MB */
}

/* ---------- format constants ---------- */

#define SAVE_JSON_SCHEMA_VERSION 1

#define KIND_CAREER     "career"
#define KIND_PRESET     "preset"
#define KIND_DIY        "diy"
#define KIND_HIGHLIGHTS "highlights"
#define KIND_TACTICS    "tactics"

/* careerFileBuffer save region: bytes [0, numSelectedTeams_offset) +
 * numSelectedTeams * sizeof(TeamFileEx) + 2. Match the legacy
 * _SaveCareerFile sizing exactly. */
static uint32_t career_data_size(void)
{
    return (uint32_t)((uint8_t *)&numSelectedTeams - (uint8_t *)&careerFileBuffer)
         + (uint32_t)numSelectedTeams * 684 + 2;
}

/* ---------- format detection ---------- */

int SaveJson_BufferIsJson(const uint8_t *buf, uint32_t size)
{
    if (!buf) return 0;
    for (uint32_t i = 0; i < size; i++) {
        uint8_t c = buf[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            continue;
        return (c == '{') ? 1 : 0;
    }
    return 0;
}

/* ---------- mjson print sink: writes to a heap-grown buffer ---------- */

typedef struct {
    char    *ptr;
    size_t   len;
    size_t   cap;
    int      oom;
} JsonOutBuf;

static int json_out_emit(const char *chunk, int chunk_len, void *ud)
{
    JsonOutBuf *o = (JsonOutBuf *)ud;
    if (o->oom) return 0;
    if (o->len + (size_t)chunk_len + 1 > o->cap) {
        size_t want = (o->cap ? o->cap : 4096);
        while (want < o->len + (size_t)chunk_len + 1) want *= 2;
        char *grew = (char *)realloc(o->ptr, want);
        if (!grew) { o->oom = 1; return 0; }
        o->ptr = grew;
        o->cap = want;
    }
    memcpy(o->ptr + o->len, chunk, (size_t)chunk_len);
    o->len += (size_t)chunk_len;
    o->ptr[o->len] = '\0';
    return chunk_len;
}

/* ---------- seasonInformationsArray semantic encoder/decoder ----------
 *
 * Each 106-byte record is laid out per the SI_* defines in
 * statistics.c. Pointer fields (offsets 22/26/30/34) become stable
 * symbol identities in JSON — `{ "sym": "aECFinals", "off": 0 }` —
 * so saves survive rebuilds. Unrecognized pointers (stale cross-build
 * values from older binaries) emit as JSON null; the legacy code path
 * already short-circuits on NULL pointers (statistics.c:1930).
 *
 * Field offsets — kept private to this file. Mirrors the legacy
 * SI_* defines but local so we don't drag statistics.c into the
 * include graph. */
#define SAVE_SI_ENTRY_SIZE       106u
#define SAVE_SI_ENTRY_COUNT       20u
#define SAVE_SI_CLUBNO             0u
#define SAVE_SI_CLUBNAME           2u
#define SAVE_SI_TROPHYFLAGS       20u
#define SAVE_SI_LEAGUESTRPTR      22u
#define SAVE_SI_STRPTR1           26u
#define SAVE_SI_STRPTR2           30u
#define SAVE_SI_STRPTR3           34u
#define SAVE_SI_BESTSCORERNUMGOALS 38u
#define SAVE_SI_BESTSCORERNAME    40u
#define SAVE_SI_2NDSCORERNAME     63u
#define SAVE_SI_COMP1RESULT       86u
#define SAVE_SI_COMP2RESULT       87u
#define SAVE_SI_COMP3RESULT       88u
#define SAVE_SI_COMP4RESULT       89u
#define SAVE_SI_TOTALGOALSSCORED  90u
#define SAVE_SI_TOTALGOALSTAKEN   92u
#define SAVE_SI_TOTALGAMES        94u
#define SAVE_SI_TOTALWINS         95u
#define SAVE_SI_TOTALDRAWN        96u
#define SAVE_SI_TOTALLOST         97u
#define SAVE_SI_FRIENDLYWINS      98u
#define SAVE_SI_FRIENDLYDRAWS     99u
#define SAVE_SI_FRIENDLYLOST     100u
#define SAVE_SI_TOURWINS         101u
#define SAVE_SI_TOURDRAWS        102u
#define SAVE_SI_TOURLOST         103u
#define SAVE_SI_TOURNAMENTSTATUS 104u
#define SAVE_SI_MANAGERSTATUS    105u

/* Emit a single 32-bit pointer field as one of:
 *   - JSON null                          (val == 0 or unresolvable)
 *   - `{ "sym": "name" [, "off": N] }`   (resolved via g_amiga_addr_map)
 *   - `{ "str": "<content>" }`           (string pointer not in LUT —
 *                                         covers the ~114 comp-name
 *                                         strings)
 */
static int emit_ptr_field(mjson_print_fn_t fn, void *fnd, const char *key,
                          uint32_t val)
{
    if (val == 0u) {
        return mjson_printf(fn, fnd, "%Q:null", key);
    }
    uint32_t off = 0;
    const AmigaAddrLutEntry *e = sym_from_ptr(val, &off);
    if (e) {
        if (off == 0)
            return mjson_printf(fn, fnd, "%Q:{%Q:%Q}", key, "sym", e->name);
        return mjson_printf(fn, fnd, "%Q:{%Q:%Q,%Q:%u}",
                            key, "sym", e->name, "off", (unsigned)off);
    }
    /* Not in the LUT — could be a comp-name string in .rodata. If
     * the pointer falls in a plausible in-process range, dereference
     * it and emit the C string content. */
    if (looks_readable(val)) {
        const char *s = (const char *)(uintptr_t)val;
        /* Scan for NUL up to a sane length to avoid pathological
         * runaway reads on a non-string region that happens to land
         * in our heuristic range. */
        size_t n = 0;
        while (n < 128u && s[n] != '\0') n++;
        if (n > 0 && n < 128u) {
            return mjson_printf(fn, fnd, "%Q:{%Q:%.*Q}",
                                key, "str", (int)n, s);
        }
    }
    /* Truly unresolved — emit null so the loader leaves the field
     * empty. statistics.c:1930 short-circuits on NULL. */
    return mjson_printf(fn, fnd, "%Q:null", key);
}

/* Parse one of `{ "sym": ..., "off": ... }`, `{ "str": ... }`, or
 * null. Sets *out to 0 on null / unresolved / parse error. */
static void load_ptr_field(const char *buf, int buflen, const char *path,
                           uint32_t *out)
{
    *out = 0;
    char child_path[128];
    char tmp[128] = {0};

    /* Try "sym" form first. */
    snprintf(child_path, sizeof(child_path), "%s.sym", path);
    if (mjson_get_string(buf, buflen, child_path, tmp, sizeof(tmp)) > 0) {
        double off_val = 0.0;
        snprintf(child_path, sizeof(child_path), "%s.off", path);
        if (mjson_get_number(buf, buflen, child_path, &off_val) <= 0)
            off_val = 0.0;
        *out = ptr_from_sym(tmp, (uint32_t)off_val);
        return;
    }

    /* Fall through to "str" form — copy content into our pool. */
    snprintf(child_path, sizeof(child_path), "%s.str", path);
    if (mjson_get_string(buf, buflen, child_path, tmp, sizeof(tmp)) > 0) {
        *out = string_pool_intern(tmp);
        return;
    }
    /* Neither form present — null / unresolved. *out stays 0. */
}

static void emit_string_field(mjson_print_fn_t fn, void *fnd, const char *key,
                              const uint8_t *src, uint32_t maxlen)
{
    /* The SWOS strings are NUL-terminated within a fixed-width slot.
     * Trim to the shorter of strlen or maxlen for the JSON emit. */
    uint32_t n = 0;
    while (n < maxlen && src[n] != 0) n++;
    mjson_printf(fn, fnd, "%Q:%.*Q", key, (int)n, (const char *)src);
}

static void load_string_field(const char *buf, int buflen, const char *path,
                              uint8_t *dst, uint32_t maxlen)
{
    /* Zero the slot first so unset fields keep the NUL terminator. */
    memset(dst, 0, maxlen);
    char tmp[128];
    int got = mjson_get_string(buf, buflen, path,
                               tmp, (int)sizeof(tmp));
    if (got <= 0) return;
    uint32_t n = (uint32_t)got;
    if (n > maxlen - 1) n = maxlen - 1;
    memcpy(dst, tmp, n);
}

/* Write season_informations as a JSON array of 20 entries. */
static void emit_season_informations(mjson_print_fn_t fn, void *fnd)
{
    mjson_printf(fn, fnd, "%Q:[", "season_informations");
    for (uint32_t i = 0; i < SAVE_SI_ENTRY_COUNT; i++) {
        const uint8_t *e = seasonInformationsArray + i * SAVE_SI_ENTRY_SIZE;

        if (i > 0) mjson_printf(fn, fnd, "%s", ",");
        mjson_printf(fn, fnd, "%s", "{");

        /* Scalars. Multi-byte fields are read as the host stored them
         * (game writes are host-native on numSelectedTeams etc.). */
        mjson_printf(fn, fnd, "%Q:%u,",
                     "club_no", (unsigned)*(uint16_t *)(e + SAVE_SI_CLUBNO));
        emit_string_field(fn, fnd, "club_name", e + SAVE_SI_CLUBNAME, 18);
        mjson_printf(fn, fnd, ",%Q:%u,",
                     "trophy_flags",
                     (unsigned)*(uint16_t *)(e + SAVE_SI_TROPHYFLAGS));

        /* The four comp-name pointers — this is the whole point of
         * the seasonInformations promotion. */
        emit_ptr_field(fn, fnd, "league_str", *(uint32_t *)(e + SAVE_SI_LEAGUESTRPTR));
        mjson_printf(fn, fnd, "%s", ",");
        emit_ptr_field(fn, fnd, "str_1", *(uint32_t *)(e + SAVE_SI_STRPTR1));
        mjson_printf(fn, fnd, "%s", ",");
        emit_ptr_field(fn, fnd, "str_2", *(uint32_t *)(e + SAVE_SI_STRPTR2));
        mjson_printf(fn, fnd, "%s", ",");
        emit_ptr_field(fn, fnd, "str_3", *(uint32_t *)(e + SAVE_SI_STRPTR3));

        mjson_printf(fn, fnd, ",%Q:%u,",
                     "best_scorer_goals",
                     (unsigned)*(uint16_t *)(e + SAVE_SI_BESTSCORERNUMGOALS));
        emit_string_field(fn, fnd, "best_scorer_name", e + SAVE_SI_BESTSCORERNAME, 23);
        mjson_printf(fn, fnd, "%s", ",");
        emit_string_field(fn, fnd, "second_scorer_name", e + SAVE_SI_2NDSCORERNAME, 23);

        mjson_printf(fn, fnd,
            ",%Q:%d,%Q:%d,%Q:%d,%Q:%d,"
            "%Q:%u,%Q:%u,"
            "%Q:%u,%Q:%u,%Q:%u,%Q:%u,"
            "%Q:%u,%Q:%u,%Q:%u,"
            "%Q:%u,%Q:%u,%Q:%u,"
            "%Q:%u,%Q:%d}",
            "comp_1_result", (int)(int8_t)e[SAVE_SI_COMP1RESULT],
            "comp_2_result", (int)(int8_t)e[SAVE_SI_COMP2RESULT],
            "comp_3_result", (int)(int8_t)e[SAVE_SI_COMP3RESULT],
            "comp_4_result", (int)(int8_t)e[SAVE_SI_COMP4RESULT],
            "total_goals_scored",
                (unsigned)*(uint16_t *)(e + SAVE_SI_TOTALGOALSSCORED),
            "total_goals_taken",
                (unsigned)*(uint16_t *)(e + SAVE_SI_TOTALGOALSTAKEN),
            "total_games",   (unsigned)e[SAVE_SI_TOTALGAMES],
            "total_wins",    (unsigned)e[SAVE_SI_TOTALWINS],
            "total_drawn",   (unsigned)e[SAVE_SI_TOTALDRAWN],
            "total_lost",    (unsigned)e[SAVE_SI_TOTALLOST],
            "friendly_wins",  (unsigned)e[SAVE_SI_FRIENDLYWINS],
            "friendly_draws", (unsigned)e[SAVE_SI_FRIENDLYDRAWS],
            "friendly_lost",  (unsigned)e[SAVE_SI_FRIENDLYLOST],
            "tour_wins",   (unsigned)e[SAVE_SI_TOURWINS],
            "tour_draws",  (unsigned)e[SAVE_SI_TOURDRAWS],
            "tour_lost",   (unsigned)e[SAVE_SI_TOURLOST],
            "tournament_status", (unsigned)e[SAVE_SI_TOURNAMENTSTATUS],
            "manager_status", (int)(int8_t)e[SAVE_SI_MANAGERSTATUS]);
    }
    mjson_printf(fn, fnd, "%s", "]");
}

/* Parse `season_informations` array from JSON, overwriting the 20
 * records in memory at `seasonInformationsArray`. Called AFTER the
 * opaque_data base64 has been decoded so we override the affected
 * bytes (including stale pointers) with semantic values. */
static void load_season_informations(const char *buf, int buflen)
{
    string_pool_reset();

    char path[128];
    double num = 0.0;
    for (uint32_t i = 0; i < SAVE_SI_ENTRY_COUNT; i++) {
        uint8_t *e = seasonInformationsArray + i * SAVE_SI_ENTRY_SIZE;

        /* Probe the entry to see if it exists. A missing index just
         * leaves the opaque_data bytes in place. */
        snprintf(path, sizeof(path), "$.season_informations[%u].club_no",
                 (unsigned)i);
        if (mjson_get_number(buf, buflen, path, &num) <= 0)
            continue;

        *(uint16_t *)(e + SAVE_SI_CLUBNO) = (uint16_t)num;

        load_string_field(buf, buflen,
            ({ snprintf(path, sizeof(path), "$.season_informations[%u].club_name",
                       (unsigned)i); path; }),
            e + SAVE_SI_CLUBNAME, 18);

        snprintf(path, sizeof(path), "$.season_informations[%u].trophy_flags", (unsigned)i);
        if (mjson_get_number(buf, buflen, path, &num) > 0)
            *(uint16_t *)(e + SAVE_SI_TROPHYFLAGS) = (uint16_t)num;

        snprintf(path, sizeof(path), "$.season_informations[%u].league_str", (unsigned)i);
        load_ptr_field(buf, buflen, path, (uint32_t *)(e + SAVE_SI_LEAGUESTRPTR));
        snprintf(path, sizeof(path), "$.season_informations[%u].str_1", (unsigned)i);
        load_ptr_field(buf, buflen, path, (uint32_t *)(e + SAVE_SI_STRPTR1));
        snprintf(path, sizeof(path), "$.season_informations[%u].str_2", (unsigned)i);
        load_ptr_field(buf, buflen, path, (uint32_t *)(e + SAVE_SI_STRPTR2));
        snprintf(path, sizeof(path), "$.season_informations[%u].str_3", (unsigned)i);
        load_ptr_field(buf, buflen, path, (uint32_t *)(e + SAVE_SI_STRPTR3));

        snprintf(path, sizeof(path), "$.season_informations[%u].best_scorer_goals", (unsigned)i);
        if (mjson_get_number(buf, buflen, path, &num) > 0)
            *(uint16_t *)(e + SAVE_SI_BESTSCORERNUMGOALS) = (uint16_t)num;

        load_string_field(buf, buflen,
            ({ snprintf(path, sizeof(path), "$.season_informations[%u].best_scorer_name",
                       (unsigned)i); path; }),
            e + SAVE_SI_BESTSCORERNAME, 23);
        load_string_field(buf, buflen,
            ({ snprintf(path, sizeof(path), "$.season_informations[%u].second_scorer_name",
                       (unsigned)i); path; }),
            e + SAVE_SI_2NDSCORERNAME, 23);

#define LOAD_BYTE(field, dst_off)                                                    \
        do {                                                                         \
            snprintf(path, sizeof(path),                                             \
                     "$.season_informations[%u]." field, (unsigned)i);               \
            if (mjson_get_number(buf, buflen, path, &num) > 0)                       \
                e[dst_off] = (uint8_t)(int8_t)(int)num;                              \
        } while (0)

#define LOAD_U16(field, dst_off)                                                     \
        do {                                                                         \
            snprintf(path, sizeof(path),                                             \
                     "$.season_informations[%u]." field, (unsigned)i);               \
            if (mjson_get_number(buf, buflen, path, &num) > 0)                       \
                *(uint16_t *)(e + dst_off) = (uint16_t)num;                          \
        } while (0)

        LOAD_BYTE("comp_1_result",     SAVE_SI_COMP1RESULT);
        LOAD_BYTE("comp_2_result",     SAVE_SI_COMP2RESULT);
        LOAD_BYTE("comp_3_result",     SAVE_SI_COMP3RESULT);
        LOAD_BYTE("comp_4_result",     SAVE_SI_COMP4RESULT);
        LOAD_U16 ("total_goals_scored", SAVE_SI_TOTALGOALSSCORED);
        LOAD_U16 ("total_goals_taken",  SAVE_SI_TOTALGOALSTAKEN);
        LOAD_BYTE("total_games",       SAVE_SI_TOTALGAMES);
        LOAD_BYTE("total_wins",        SAVE_SI_TOTALWINS);
        LOAD_BYTE("total_drawn",       SAVE_SI_TOTALDRAWN);
        LOAD_BYTE("total_lost",        SAVE_SI_TOTALLOST);
        LOAD_BYTE("friendly_wins",     SAVE_SI_FRIENDLYWINS);
        LOAD_BYTE("friendly_draws",    SAVE_SI_FRIENDLYDRAWS);
        LOAD_BYTE("friendly_lost",     SAVE_SI_FRIENDLYLOST);
        LOAD_BYTE("tour_wins",         SAVE_SI_TOURWINS);
        LOAD_BYTE("tour_draws",        SAVE_SI_TOURDRAWS);
        LOAD_BYTE("tour_lost",         SAVE_SI_TOURLOST);
        LOAD_BYTE("tournament_status", SAVE_SI_TOURNAMENTSTATUS);
        LOAD_BYTE("manager_status",    SAVE_SI_MANAGERSTATUS);
#undef LOAD_BYTE
#undef LOAD_U16
    }
}

/* ---------- .CAR writer ---------- */

int16_t SaveJson_WriteCareer(uint8_t *filename)
{
    uint32_t data_size = career_data_size();
    JsonOutBuf out = { NULL, 0, 0, 0 };

    /* Top-level object. opaque_data carries the legacy raw bytes
     * base64-encoded for fields not yet promoted to semantic JSON.
     * Promoted fields (currently: season_informations) are also
     * written semantically and OVERRIDE the corresponding bytes when
     * loaded — see SaveJson_LoadCareer. As more fields get promoted
     * the opaque_data blob shrinks. */
    mjson_printf(json_out_emit, &out,
        "{\n"
        "  %Q: %d,\n"
        "  %Q: %Q,\n"
        "  %Q: %d,\n"
        "  %Q: %V,\n  ",
        "schema_version", SAVE_JSON_SCHEMA_VERSION,
        "file_kind",      KIND_CAREER,
        "num_selected_teams", (int)numSelectedTeams,
        "opaque_data",    (int)data_size, (const char *)&careerFileBuffer);

    emit_season_informations(json_out_emit, &out);

    mjson_printf(json_out_emit, &out, "%s", "\n}\n");

    if (out.oom || !out.ptr) {
        if (out.ptr) free(out.ptr);
        return 1;
    }

    int16_t result = _WriteFile(filename, (uint8_t *)out.ptr, (uint32_t)out.len);
    free(out.ptr);
    if (result != 0) {
        _ShowFsErrorByCode((uint16_t)result);
        return 1;
    }
    return 0;
}

/* ---------- .CAR loader ---------- */

int16_t SaveJson_LoadCareer(uint8_t *filename, const uint8_t *src, uint32_t src_size)
{
    (void)filename;

    /* Verify schema_version. */
    double version = 0.0;
    if (mjson_get_number((const char *)src, (int)src_size,
                         "$.schema_version", &version) <= 0) {
        return 1;
    }
    if ((int)version != SAVE_JSON_SCHEMA_VERSION) {
        return 1;
    }

    /* Verify file_kind. */
    char kind[32] = {0};
    if (mjson_get_string((const char *)src, (int)src_size,
                         "$.file_kind", kind, sizeof(kind)) <= 0) {
        return 1;
    }
    if (strcmp(kind, KIND_CAREER) != 0) {
        return 1;
    }

    /* Read opaque_data base64 → careerFileBuffer in place. We need a
     * temporary holding region because src points at careerFileBuffer
     * itself (the legacy load path put the file there). Allocate a
     * heap buffer sized to the on-disk data (legacy max ≈ 109 KB). */
    uint32_t want = career_data_size();
    uint8_t *tmp = (uint8_t *)malloc(want > 4 ? want : 4);
    if (!tmp) return 1;

    int got = mjson_get_base64((const char *)src, (int)src_size,
                               "$.opaque_data", (char *)tmp, (int)want);
    if (got <= 0) {
        free(tmp);
        return 1;
    }

    /* numSelectedTeams isn't yet populated — figure out how many bytes
     * the writer actually emitted. The base64 decoder returns the
     * decoded byte count; trust that as the canonical extent. */
    if ((uint32_t)got > want) {
        free(tmp);
        return 1;
    }
    memcpy(&careerFileBuffer, tmp, (size_t)got);
    free(tmp);

    /* Apply semantic field overrides AFTER the opaque blob, so the
     * semantic representation wins. This is what makes saves stable
     * across rebuilds: even though the opaque blob has the original
     * raw pointer values, load_season_informations overwrites them
     * with currentbuild addresses resolved from stable symbol names. */
    load_season_informations((const char *)src, (int)src_size);

    return 0;
}

/* ---------- other save types: still stubbed ---------- */

int16_t SaveJson_WritePreset(uint8_t *filename)     { (void)filename; return 1; }
int16_t SaveJson_LoadPreset (uint8_t *filename, const uint8_t *src, uint32_t src_size)
                                                    { (void)filename; (void)src; (void)src_size; return 1; }
int16_t SaveJson_WriteDIY   (uint8_t *filename)     { (void)filename; return 1; }
int16_t SaveJson_LoadDIY    (uint8_t *filename, const uint8_t *src, uint32_t src_size)
                                                    { (void)filename; (void)src; (void)src_size; return 1; }
int16_t SaveJson_WriteHighlights(uint8_t *filename) { (void)filename; return 1; }
int16_t SaveJson_LoadHighlights (uint8_t *filename, const uint8_t *src, uint32_t src_size)
                                                    { (void)filename; (void)src; (void)src_size; return 1; }
int16_t SaveJson_WriteTactics(uint8_t *filename, const uint8_t *src, uint32_t src_size)
                                                    { (void)filename; (void)src; (void)src_size; return 1; }
int16_t SaveJson_LoadTactics (uint8_t *filename, uint8_t *dst, uint32_t dst_capacity)
                                                    { (void)filename; (void)dst; (void)dst_capacity; return 1; }
