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

#include <stddef.h>  /* offsetof */
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

/* Direct stdio load helper. JSON saves can grow to several hundred
 * KB (>3x the legacy binary size from base64 expansion + key text),
 * which overflows the careerFileBuffer destination if we route
 * through _LoadFileEx. The save directory is `save/<filename>` per
 * resload.c's resolve_path; replicate that path construction here
 * so we can bypass the asset-loader staging and read straight into
 * a heap buffer sized to the file. */
static int read_save_file_heap(const uint8_t *filename,
                               uint8_t **out_buf, uint32_t *out_size)
{
    char path[512];
    const char *src = (const char *)filename;
    const char *colon = strchr(src, ':');
    if (colon) src = colon + 1;
    snprintf(path, sizeof(path), "save/%s", src);

    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 1; }

    uint8_t *buf = (uint8_t *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 1; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { free(buf); return 1; }
    buf[n] = '\0';

    *out_buf  = buf;
    *out_size = (uint32_t)n;
    return 0;
}

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

/* Heuristic for sentinel pointer values used in competition tables:
 * `(CompetitionCup *)-1`, `-2`, `0xFC`, etc. These aren't real
 * addresses — they're flags meaning "end of list" / "skip" / etc.,
 * and they MUST round-trip bit-exact. Any unsigned value at the
 * extreme top of the 32-bit range or below the lowest plausible
 * mapped address is treated as a sentinel. */
static int looks_sentinel(uint32_t val)
{
    return val >= 0xFFFFFF00u || val < 0x00001000u;
}

/* Emit a single 32-bit pointer field as one of:
 *   - JSON null                          (val == 0)
 *   - `{ "sym": "name" [, "off": N] }`   (resolved via g_amiga_addr_map)
 *   - `{ "str": "<content>" }`           (string pointer not in LUT —
 *                                         covers the ~114 comp-name
 *                                         strings)
 *   - `{ "raw": N }`                     (sentinel value used by
 *                                         comp tables: -1, -2, 0xFC,
 *                                         etc.)
 *   - JSON null                          (stale cross-build pointer)
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
    /* Sentinel before string-readability — sentinels could
     * accidentally look "readable" by the ±16MB heuristic and emit
     * garbage. */
    if (looks_sentinel(val)) {
        return mjson_printf(fn, fnd, "%Q:{%Q:%u}", key, "raw", (unsigned)val);
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

    /* "str" form — copy content into our pool. */
    snprintf(child_path, sizeof(child_path), "%s.str", path);
    if (mjson_get_string(buf, buflen, child_path, tmp, sizeof(tmp)) > 0) {
        *out = string_pool_intern(tmp);
        return;
    }

    /* "raw" form — bit-exact sentinel passthrough. */
    snprintf(child_path, sizeof(child_path), "%s.raw", path);
    double raw_val = 0.0;
    if (mjson_get_number(buf, buflen, child_path, &raw_val) > 0) {
        *out = (uint32_t)raw_val;
        return;
    }
    /* None of the forms present — null / unresolved. *out stays 0. */
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

/* ---------- careerFileBuffer top-level pointer fields ----------
 *
 * Bytes that need symbol-stable encoding so saves survive rebuilds.
 * Offsets are into careerFileBuffer (= careerFileBufferBuf).
 *
 *   currentCompetitionPtr            +63244  (uint32_t)
 *   secondaryCompetitionPtr          +63248
 *   tertiaryCompetitionPtr           +63252
 *   quaternaryCompetitionPtr         +63256
 *   diyCompetitionPtr                +63260
 *   diyCompetitionTablePtr           +63266
 *   careerCompetitionSlotPtrTable[5] +63320  (5 × uint32_t)
 *   careerCupPickList[5]             +55086  (5 × uint32_t, terminator-
 *                                              based comp table)
 *   primaryCompStringOffset          +93348
 *   secondaryCompStringOffset        +63380
 *   tertiaryCompStringOffset         +66280
 *   quaternaryCompStringOffset       +68464
 */

#define CFB_CURRENT_COMP_PTR_OFF        63244u
#define CFB_SECONDARY_COMP_PTR_OFF      63248u
#define CFB_TERTIARY_COMP_PTR_OFF       63252u
#define CFB_QUATERNARY_COMP_PTR_OFF     63256u
#define CFB_DIY_COMP_PTR_OFF            63260u
#define CFB_DIY_COMP_TABLE_PTR_OFF      63266u
#define CFB_CAREER_COMP_SLOT_TBL_OFF    63320u
#define CFB_CAREER_CUP_PICK_LIST_OFF    55086u
#define CFB_PRIMARY_COMP_STR_OFF        93348u
#define CFB_SECONDARY_COMP_STR_OFF      63380u
#define CFB_TERTIARY_COMP_STR_OFF       66280u
#define CFB_QUATERNARY_COMP_STR_OFF     68464u

/* Convenience to read a uint32 from careerFileBuffer + off. */
static uint32_t cfb_u32(uint32_t off)
{
    return *(uint32_t *)((&careerFileBuffer) + off);
}

static void cfb_set_u32(uint32_t off, uint32_t val)
{
    *(uint32_t *)((&careerFileBuffer) + off) = val;
}

static void emit_career_pointers(mjson_print_fn_t fn, void *fnd)
{
    emit_ptr_field(fn, fnd, "current_competition_ptr",
                   cfb_u32(CFB_CURRENT_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "secondary_competition_ptr",
                   cfb_u32(CFB_SECONDARY_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "tertiary_competition_ptr",
                   cfb_u32(CFB_TERTIARY_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "quaternary_competition_ptr",
                   cfb_u32(CFB_QUATERNARY_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "diy_competition_ptr",
                   cfb_u32(CFB_DIY_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "diy_competition_table_ptr",
                   cfb_u32(CFB_DIY_COMP_TABLE_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "primary_comp_string_offset",
                   cfb_u32(CFB_PRIMARY_COMP_STR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "secondary_comp_string_offset",
                   cfb_u32(CFB_SECONDARY_COMP_STR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "tertiary_comp_string_offset",
                   cfb_u32(CFB_TERTIARY_COMP_STR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field(fn, fnd, "quaternary_comp_string_offset",
                   cfb_u32(CFB_QUATERNARY_COMP_STR_OFF));

    /* careerCompetitionSlotPtrTable[5] */
    mjson_printf(fn, fnd, ",%Q:[", "career_competition_slot_ptr_table");
    for (uint32_t i = 0; i < 5; i++) {
        if (i > 0) mjson_printf(fn, fnd, "%s", ",");
        /* emit_ptr_field requires a key — for array elements we
         * inline the same logic minus the key prefix. */
        uint32_t val = cfb_u32(CFB_CAREER_COMP_SLOT_TBL_OFF + i * 4);
        uint32_t off = 0;
        const AmigaAddrLutEntry *e = sym_from_ptr(val, &off);
        if (val == 0u) {
            mjson_printf(fn, fnd, "%s", "null");
        } else if (e) {
            if (off == 0) mjson_printf(fn, fnd, "{%Q:%Q}", "sym", e->name);
            else mjson_printf(fn, fnd, "{%Q:%Q,%Q:%u}",
                              "sym", e->name, "off", (unsigned)off);
        } else if (looks_sentinel(val)) {
            mjson_printf(fn, fnd, "{%Q:%u}", "raw", (unsigned)val);
        } else if (looks_readable(val)) {
            const char *s = (const char *)(uintptr_t)val;
            size_t n = 0; while (n < 128u && s[n] != '\0') n++;
            if (n > 0 && n < 128u)
                mjson_printf(fn, fnd, "{%Q:%.*Q}", "str", (int)n, s);
            else
                mjson_printf(fn, fnd, "%s", "null");
        } else {
            mjson_printf(fn, fnd, "%s", "null");
        }
    }
    mjson_printf(fn, fnd, "%s", "]");

    /* careerCupPickList[5] — same shape as the slot table. */
    mjson_printf(fn, fnd, ",%Q:[", "career_cup_pick_list");
    for (uint32_t i = 0; i < 5; i++) {
        if (i > 0) mjson_printf(fn, fnd, "%s", ",");
        uint32_t val = cfb_u32(CFB_CAREER_CUP_PICK_LIST_OFF + i * 4);
        uint32_t off = 0;
        const AmigaAddrLutEntry *e = sym_from_ptr(val, &off);
        if (val == 0u) {
            mjson_printf(fn, fnd, "%s", "null");
        } else if (e) {
            if (off == 0) mjson_printf(fn, fnd, "{%Q:%Q}", "sym", e->name);
            else mjson_printf(fn, fnd, "{%Q:%Q,%Q:%u}",
                              "sym", e->name, "off", (unsigned)off);
        } else if (looks_sentinel(val)) {
            mjson_printf(fn, fnd, "{%Q:%u}", "raw", (unsigned)val);
        } else if (looks_readable(val)) {
            const char *s = (const char *)(uintptr_t)val;
            size_t n = 0; while (n < 128u && s[n] != '\0') n++;
            if (n > 0 && n < 128u)
                mjson_printf(fn, fnd, "{%Q:%.*Q}", "str", (int)n, s);
            else
                mjson_printf(fn, fnd, "%s", "null");
        } else {
            mjson_printf(fn, fnd, "%s", "null");
        }
    }
    mjson_printf(fn, fnd, "%s", "]");
}

static void load_career_pointers(const char *buf, int buflen)
{
    uint32_t val = 0;

    load_ptr_field(buf, buflen, "$.current_competition_ptr", &val);
    cfb_set_u32(CFB_CURRENT_COMP_PTR_OFF, val);
    load_ptr_field(buf, buflen, "$.secondary_competition_ptr", &val);
    cfb_set_u32(CFB_SECONDARY_COMP_PTR_OFF, val);
    load_ptr_field(buf, buflen, "$.tertiary_competition_ptr", &val);
    cfb_set_u32(CFB_TERTIARY_COMP_PTR_OFF, val);
    load_ptr_field(buf, buflen, "$.quaternary_competition_ptr", &val);
    cfb_set_u32(CFB_QUATERNARY_COMP_PTR_OFF, val);
    load_ptr_field(buf, buflen, "$.diy_competition_ptr", &val);
    cfb_set_u32(CFB_DIY_COMP_PTR_OFF, val);
    load_ptr_field(buf, buflen, "$.diy_competition_table_ptr", &val);
    cfb_set_u32(CFB_DIY_COMP_TABLE_PTR_OFF, val);
    load_ptr_field(buf, buflen, "$.primary_comp_string_offset", &val);
    cfb_set_u32(CFB_PRIMARY_COMP_STR_OFF, val);
    load_ptr_field(buf, buflen, "$.secondary_comp_string_offset", &val);
    cfb_set_u32(CFB_SECONDARY_COMP_STR_OFF, val);
    load_ptr_field(buf, buflen, "$.tertiary_comp_string_offset", &val);
    cfb_set_u32(CFB_TERTIARY_COMP_STR_OFF, val);
    load_ptr_field(buf, buflen, "$.quaternary_comp_string_offset", &val);
    cfb_set_u32(CFB_QUATERNARY_COMP_STR_OFF, val);

    char path[64];
    for (uint32_t i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), "$.career_competition_slot_ptr_table[%u]", i);
        load_ptr_field(buf, buflen, path, &val);
        cfb_set_u32(CFB_CAREER_COMP_SLOT_TBL_OFF + i * 4, val);

        snprintf(path, sizeof(path), "$.career_cup_pick_list[%u]", i);
        load_ptr_field(buf, buflen, path, &val);
        cfb_set_u32(CFB_CAREER_CUP_PICK_LIST_OFF + i * 4, val);
    }
}

/* ---------- table-driven scalar/array fields ----------
 *
 * Most of careerFileBuffer is plain typed data — flags, counters,
 * balances, identifiers. Rather than writing a `mjson_printf` line
 * per field (hundreds of them), we describe each field with a
 * struct entry and iterate at save/load time. Saves the JSON
 * structure self-describing, keeps the code small, and adding a
 * field is a one-line table extension.
 *
 * Field types:
 *   FLD_U8 / FLD_S8 / FLD_U16 / FLD_S16 / FLD_U32 / FLD_S32  — typed
 *     scalar at `offset`, emitted as a JSON number.
 *   FLD_STR — char[`size`] NUL-terminated string at `offset`.
 *   FLD_BYTES — `size` bytes at `offset`, emitted as a JSON array of
 *     unsigned ints (small) so it stays human-readable.
 *
 * The table covers fields we've semantically modeled; everything
 * else still rides along in opaque_data until we promote it.
 */
typedef enum {
    FLD_END = 0,
    FLD_U8, FLD_S8,
    FLD_U16, FLD_S16,
    FLD_U32, FLD_S32,
    FLD_STR,
    FLD_BYTES,
} FieldType;

typedef struct {
    const char *name;
    void       *addr;       /* absolute address — taken via &symbol;
                             * 64-bit-stable for standalone overlay
                             * globals, and survives layout shifts in
                             * the careerFileBuffer overlay */
    uint32_t    size;       /* for FLD_STR / FLD_BYTES; ignored for scalars */
    FieldType   type;
} FieldDesc;

/* extern declarations for all overlay symbols referenced by the
 * tables below. The .equ aliases in swos_globals_career_overlay.c
 * make these visible to the linker; taking their address gives a
 * stable relocatable address resolvable at link time, so the table
 * entries below remain valid static initializers. */
extern uint16_t currentSeasonShowing;
extern uint16_t stayingInOldClub;
extern uint16_t overdraftStreak;
extern uint16_t overdraftWarningState;
extern uint16_t performanceWarningState;
extern uint16_t overdraftWarningCooldown;
extern uint16_t performanceWarningCooldown;
extern uint16_t youreSacked;
extern uint16_t retired;
extern uint16_t competitionSlotUpdatedFlag;
extern uint16_t careerTeamLostMatch;
extern uint16_t clubBusinessActive;
extern int32_t  newBalance;
extern int32_t  old_balance;
extern int32_t  playerPurchases;
extern int32_t  playerSales;
extern int32_t  generalRunningCosts;
extern int32_t  playerWagesBill;
extern int32_t  gateReceipts;
extern int32_t  startingBalance;
extern int32_t  startingBalanceCopy;
extern int32_t  competitionBonuses;
extern int32_t  sensisoftPlcSponsorship;
extern uint16_t financeTotalsNeedResetFlag;
extern uint32_t weeklyRunningCosts;
extern uint32_t weeklyPlayerWages;
extern int32_t  totalTeamValue;
extern uint8_t  weeklyWageByPlayerIndex[];
extern uint8_t  playerFormModifiers[];
extern uint16_t sensisoftPlcSponsorshipFactor;
extern int32_t  additionalInvestments;
extern uint16_t teamValueDivisor;
extern uint16_t expectedLeagueRankIndex;
extern uint16_t boardConfidenceHistory0;
extern uint16_t boardConfidenceScore;
extern uint16_t previousBoardConfidence;
extern uint16_t playWinCupAnimation;
extern uint16_t minorTrophyBonusActive;
extern uint16_t leagueTitleBonusActive;
extern uint16_t majorTrophyBonusActive;
extern uint16_t cupBonus1Active;
extern uint16_t cupBonus2Active;
extern uint16_t wonWorldCup;
extern uint16_t wonContinentalCup;
extern uint16_t wonInvitationTournament;
extern uint16_t qualifyingStage1PendingFlag0;
extern uint16_t qualifyingStage1PendingFlag1;
extern uint16_t qualifyingStage2PendingFlag0;
extern uint16_t qualifyingStage2PendingFlag1;
extern uint16_t isCareer;
extern uint16_t careerMatchSimulatedFlag;
extern uint16_t gamesPlayed;
extern uint16_t numWins;
extern uint16_t numDrawn;
extern uint16_t numLost;
extern uint16_t goalsScored;
extern uint16_t goalsTaken;
extern uint16_t boardConfidenceMod0;
extern uint16_t boardConfidenceValue0;
extern uint16_t boardConfidenceValue1;
extern uint16_t contractNegotiationActive;
extern uint16_t wageScalingDivisor;
extern uint16_t teamNoOfferAccepted;
extern int32_t  teamFundsAccepted;
extern uint16_t isNationalSelector;
extern uint16_t someTeamFlags;
extern uint16_t lastNationalityCall;
extern uint8_t  nationalTeamNationality;
extern uint8_t  nationalTeamFileId;
extern uint16_t sameNationalCallFlag;
extern uint16_t sackedReputationScore;
extern uint16_t nationalSelectorReputationPoints;
extern uint16_t nationalTeamFileCode;
extern uint16_t seasonReview;
extern uint16_t worldCupHostTeam;
extern uint16_t worldCupQualifyingTeam;
extern uint16_t euroChampionshipQualTeam;
extern uint16_t selTeamNumber;
extern uint16_t coachOrPlayer;
extern uint16_t careerTeamLeague;
extern uint16_t currentDivisionIndex;
extern uint16_t leagueFirstRoundNumber;
extern uint16_t playPromotedAnimation;
extern uint16_t playRelegatedAnimation;
extern uint16_t mrOrMs;
extern uint8_t  managerSexBuffer[];
extern uint8_t  managerNameBuffer[];
extern uint8_t  managerSurnameBuffer[];
extern uint16_t plNationality;
extern uint16_t winCupBonusCount;
extern uint16_t minorTrophyBonusCount;
extern uint16_t majorTrophyBonusCount;
extern int16_t  careerReputationPoints;
extern uint16_t removedOfferPlayersFromSelectedTeamCount;
extern uint16_t removedOfferPlayersFromOtherTeamsCount;
extern uint16_t homeMarketUpdateBacklog;
extern uint16_t foreignMarketUpdateBacklog;
extern uint16_t offerExpiryIterationsRemaining;
extern uint16_t numForeignMarketPlayers;
extern uint16_t numHomeMarketPlayers;
extern uint16_t numOffers;
extern uint16_t careerSeasonsCompleted;
extern uint16_t numForeignMarketPlayers2;
extern uint16_t pendingForeignPlayerCount;
extern uint16_t seasonTransferQuota;
extern uint16_t transferPoolSlotLimit;
extern uint16_t offerSwapSlotLimit;
extern uint16_t transferPoolSlotsUsed;
extern uint16_t offerSwapSlotsUsed;
extern uint16_t numNewJobOffers;
extern uint16_t numJobOffers;
extern uint16_t timeToNegotiate;
extern uint16_t numPlayersOnTransferMarket;
extern uint16_t lastOfferPrimaryPlayerOrdinal;
extern uint16_t lastOfferExtraPlayersWanted;
extern uint16_t lastOfferSwapPlayerOrdinal;
extern uint16_t lastOfferExtraPlayerOrdinal;
extern uint16_t offerNegotiationAttemptCount;
extern uint16_t transferMarketRandOffset0;
extern uint16_t transferMarketRandOffset1;
extern uint16_t transferMarketRandOffset2;
extern uint16_t transferMarketPhase;
extern uint16_t careerNumPlayers;
extern uint16_t selTeamSelectedPlayerPosition;
extern uint16_t lastFinancialWeekIndex;
extern uint16_t numPlayers;
extern uint16_t currentTeamNumber;
extern uint16_t currentTeamSelectedPlayerPosition;
extern uint16_t trainingMatchSavedPlayerPos;
extern uint16_t lastMatchSavedPlayerPos;
extern uint16_t lastMatchSavedCoachOrPlayer;
extern uint8_t  currentTactics;
extern uint8_t  lastMatchSavedTeamControls;
extern uint16_t seasonFinished;
extern uint16_t careerFinished;
extern uint16_t euroCupSlotIndex;
extern uint16_t seasonNumberOfSquads;
extern uint16_t teamSelectionKeyCacheIndex;
extern uint16_t teamSelectionKeyCacheOffset;
extern uint16_t teamSelectionKeyCacheCount;
extern uint16_t careerSlotProcessingInProgress;
extern uint16_t teamSelectionKeyCacheEnd;
extern uint16_t competitionOver;
extern uint8_t  diyPresetNameOfCompetition[];
extern uint16_t majorTrophyBonusValue;
extern uint16_t numDiyTeams;
extern uint16_t primaryCompWinnerTeam;
extern uint16_t primaryCompSquadViewParam;
extern uint16_t primaryCompSlotStateA;
extern uint16_t primaryCompSlotStateB;
extern uint16_t primaryCompSlotStateC;
extern uint16_t diyTeamMinDivisionIndex;
extern uint16_t diyTeamMaxDivisionOffset;
extern uint16_t leaguePromotionCount;
extern uint16_t leagueRelegationCount;
extern uint16_t leagueWarningsEnabled;
extern uint8_t  viewRandomPhase;
extern uint8_t  seasonPlaying;
extern uint16_t inFriendlyMenu;
extern uint16_t isNationalTeam;
extern uint16_t gameType;
extern uint16_t contestStarted;
extern uint16_t careerLoadedFlag;
extern uint16_t gameLength;
extern uint16_t autoReplays;
extern uint16_t menuMusic;
extern uint16_t autoSaveHighlights;
extern uint16_t allPlayerTeamsEqual;
extern uint16_t pitchType;
extern uint8_t  seed2;
extern uint8_t  randXorIndex2;
extern uint8_t  randXorKey2;
extern uint8_t  newRandomSeedCopy;
extern uint16_t randomSeed;
extern uint16_t saveFileVersion;
extern uint32_t saveFileAmigaType;
/* numSelectedTeams already declared at file top. */

extern uint8_t  secondaryCompetitionName[];
extern uint16_t minorTrophyBonusValue;
extern uint16_t secondaryCompWinnerTeam;
extern uint16_t secondaryCompSlotStateA;
extern uint16_t secondaryCompSlotStateB;
extern uint16_t secondaryCompSlotStateC;
extern uint16_t secondaryCompFinished;
extern uint8_t  tertiaryCompetitionName[];
extern uint16_t leagueTitleBonusValue;
extern uint16_t tertiaryCompWinnerTeam;
extern uint16_t tertiaryCompSlotStateA;
extern uint16_t tertiaryCompSlotStateB;
extern uint16_t tertiaryCompSlotStateC;
extern uint16_t tertiaryCompFinished;
extern uint16_t diyCompSlotStateA;
extern uint16_t diyCompSlotStateB;
extern uint16_t diyCompSlotStateC;
extern uint8_t  quaternaryCompetitionName[];
extern uint16_t majorTrophyOrCupBonusValue;
extern uint16_t quaternaryCompWinnerTeam;
extern uint16_t quaternaryCompSlotStateA;
extern uint16_t quaternaryCompSlotStateB;
extern uint16_t quaternaryCompSlotStateC;
extern uint16_t removedPlayerStatAccum;
extern uint8_t  careerUnavailablePlayerList[];
extern uint8_t  currentMatchPlayers[];
extern uint8_t  seasonSquadTeamNumbers[];
extern uint8_t  playerPositions[];
extern uint8_t  trainingMatchSavedPlayers[];
extern uint8_t  trainingMatchSavedShirtNums[];
extern uint8_t  trainingMatchSavedTactics[];
extern uint8_t  lastMatchPlayers[];
extern uint8_t  lastMatchSavedShirtNums[];
extern uint8_t  careerLastMatchIndexByCompetitionSlot[];
extern uint8_t  teamBudgetRankTable[];
extern uint8_t  playersOnTransferMarket[];
extern uint8_t  foreignMarketPlayers[];
extern uint8_t  homeMarketPlayers[];
extern uint8_t  transferOfferTable[];
extern uint8_t  newJobOffers[];
extern uint8_t  jobOffers[];

/* Career meta / flags / finance / market state.
 * Address-based: each entry takes &symbol so the table is
 * resilient to overlay layout shifts (e.g. eventual 64-bit move)
 * and self-documenting. The compile-time address-constant rule
 * applies — `&overdraftStreak` is valid in a static initializer. */
#define FX(name, sym, type) { name, (void *)&sym, 0, type }
#define FA(name, sym, len, type) { name, (void *)&sym, len, type }

static const FieldDesc s_career_scalars[] = {
    FX("current_season_showing",          currentSeasonShowing,        FLD_U16),
    FX("staying_in_old_club",             stayingInOldClub,            FLD_U16),
    FX("overdraft_streak",                overdraftStreak,             FLD_U16),
    FX("overdraft_warning_state",         overdraftWarningState,       FLD_U16),
    FX("performance_warning_state",       performanceWarningState,     FLD_U16),
    FX("overdraft_warning_cooldown",      overdraftWarningCooldown,    FLD_U16),
    FX("performance_warning_cooldown",    performanceWarningCooldown,  FLD_U16),
    FX("youre_sacked",                    youreSacked,                 FLD_U16),
    FX("retired",                         retired,                     FLD_U16),
    FX("competition_slot_updated_flag",   competitionSlotUpdatedFlag,  FLD_U16),
    FX("career_team_lost_match",          careerTeamLostMatch,         FLD_U16),
    FX("club_business_active",            clubBusinessActive,          FLD_U16),
    FX("new_balance",                     newBalance,                  FLD_S32),
    FX("old_balance",                     old_balance,                 FLD_S32),
    FX("player_purchases",                playerPurchases,             FLD_S32),
    FX("player_sales",                    playerSales,                 FLD_S32),
    FX("general_running_costs",           generalRunningCosts,         FLD_S32),
    FX("player_wages_bill",               playerWagesBill,             FLD_S32),
    FX("gate_receipts",                   gateReceipts,                FLD_S32),
    FX("starting_balance",                startingBalance,             FLD_S32),
    FX("starting_balance_copy",           startingBalanceCopy,         FLD_S32),
    FX("competition_bonuses",             competitionBonuses,          FLD_S32),
    FX("sensisoft_plc_sponsorship",       sensisoftPlcSponsorship,     FLD_S32),
    FX("finance_totals_need_reset_flag",  financeTotalsNeedResetFlag,  FLD_U16),
    FX("weekly_running_costs",            weeklyRunningCosts,          FLD_U32),
    FX("weekly_player_wages",             weeklyPlayerWages,           FLD_U32),
    FX("total_team_value",                totalTeamValue,              FLD_S32),
    FA("weekly_wage_by_player_index",     weeklyWageByPlayerIndex, 128,FLD_BYTES),
    FA("player_form_modifiers",           playerFormModifiers,     32, FLD_BYTES),
    FX("sensisoft_plc_sponsorship_factor",sensisoftPlcSponsorshipFactor,FLD_U16),
    FX("additional_investments",          additionalInvestments,       FLD_S32),
    FX("team_value_divisor",              teamValueDivisor,            FLD_U16),
    FX("expected_league_rank_index",      expectedLeagueRankIndex,     FLD_U16),
    FA("board_confidence_history",        boardConfidenceHistory0, 8,  FLD_BYTES),
    FX("board_confidence_score",          boardConfidenceScore,        FLD_U16),
    FX("previous_board_confidence",       previousBoardConfidence,     FLD_U16),
    FX("play_win_cup_animation",          playWinCupAnimation,         FLD_U16),
    FX("minor_trophy_bonus_active",       minorTrophyBonusActive,      FLD_U16),
    FX("league_title_bonus_active",       leagueTitleBonusActive,      FLD_U16),
    FX("major_trophy_bonus_active",       majorTrophyBonusActive,      FLD_U16),
    FX("cup_bonus_1_active",              cupBonus1Active,             FLD_U16),
    FX("cup_bonus_2_active",              cupBonus2Active,             FLD_U16),
    FX("won_world_cup",                   wonWorldCup,                 FLD_U16),
    FX("won_continental_cup",             wonContinentalCup,           FLD_U16),
    FX("won_invitation_tournament",       wonInvitationTournament,     FLD_U16),
    FX("qualifying_stage_1_pending_flag0",qualifyingStage1PendingFlag0,FLD_U16),
    FX("qualifying_stage_1_pending_flag1",qualifyingStage1PendingFlag1,FLD_U16),
    FX("qualifying_stage_2_pending_flag0",qualifyingStage2PendingFlag0,FLD_U16),
    FX("qualifying_stage_2_pending_flag1",qualifyingStage2PendingFlag1,FLD_U16),
    FX("is_career",                       isCareer,                    FLD_U16),
    FX("career_match_simulated_flag",     careerMatchSimulatedFlag,    FLD_U16),
    FX("games_played",                    gamesPlayed,                 FLD_U16),
    FX("num_wins",                        numWins,                     FLD_U16),
    FX("num_drawn",                       numDrawn,                    FLD_U16),
    FX("num_lost",                        numLost,                     FLD_U16),
    FX("goals_scored",                    goalsScored,                 FLD_U16),
    FX("goals_taken",                     goalsTaken,                  FLD_U16),
    FA("board_confidence_mods",           boardConfidenceMod0, 20,     FLD_BYTES),
    FX("board_confidence_value_0",        boardConfidenceValue0,       FLD_U16),
    FX("board_confidence_value_1",        boardConfidenceValue1,       FLD_U16),
    FX("contract_negotiation_active",     contractNegotiationActive,   FLD_U16),
    FX("wage_scaling_divisor",            wageScalingDivisor,          FLD_U16),
    FX("team_no_offer_accepted",          teamNoOfferAccepted,         FLD_U16),
    FX("team_funds_accepted",             teamFundsAccepted,           FLD_S32),
    FX("is_national_selector",            isNationalSelector,          FLD_U16),
    FX("some_team_flags",                 someTeamFlags,               FLD_U16),
    FX("last_nationality_call",           lastNationalityCall,         FLD_U16),
    FX("national_team_nationality",       nationalTeamNationality,     FLD_U8 ),
    FX("national_team_file_id",           nationalTeamFileId,          FLD_U8 ),
    FX("same_national_call_flag",         sameNationalCallFlag,        FLD_U16),
    FX("sacked_reputation_score",         sackedReputationScore,       FLD_U16),
    FX("national_selector_reputation_points",nationalSelectorReputationPoints,FLD_U16),
    FX("national_team_file_code",         nationalTeamFileCode,        FLD_U16),
    FX("season_review",                   seasonReview,                FLD_U16),
    FX("world_cup_host_team",             worldCupHostTeam,            FLD_U16),
    FX("world_cup_qualifying_team",       worldCupQualifyingTeam,      FLD_U16),
    FX("euro_championship_qual_team",     euroChampionshipQualTeam,    FLD_U16),
    FX("sel_team_number",                 selTeamNumber,               FLD_U16),
    FX("coach_or_player",                 coachOrPlayer,               FLD_U16),
    FX("career_team_league",              careerTeamLeague,            FLD_U16),
    FX("current_division_index",          currentDivisionIndex,        FLD_U16),
    FX("league_first_round_number",       leagueFirstRoundNumber,      FLD_U16),
    FX("play_promoted_animation",         playPromotedAnimation,       FLD_U16),
    FX("play_relegated_animation",        playRelegatedAnimation,      FLD_U16),
    FX("mr_or_ms",                        mrOrMs,                      FLD_U16),
    FA("manager_sex_buffer",              managerSexBuffer, 4,         FLD_STR ),
    FA("manager_name_buffer",             managerNameBuffer, 9,        FLD_STR ),
    FA("manager_surname_buffer",          managerSurnameBuffer, 13,    FLD_STR ),
    FX("pl_nationality",                  plNationality,               FLD_U16),
    FX("win_cup_bonus_count",             winCupBonusCount,            FLD_U16),
    FX("minor_trophy_bonus_count",        minorTrophyBonusCount,       FLD_U16),
    FX("major_trophy_bonus_count",        majorTrophyBonusCount,       FLD_U16),
    FX("career_reputation_points",        careerReputationPoints,      FLD_S16),
    FX("removed_offer_players_from_selected_team_count",removedOfferPlayersFromSelectedTeamCount,FLD_U16),
    FX("removed_offer_players_from_other_teams_count",  removedOfferPlayersFromOtherTeamsCount,  FLD_U16),
    FX("home_market_update_backlog",      homeMarketUpdateBacklog,     FLD_U16),
    FX("foreign_market_update_backlog",   foreignMarketUpdateBacklog,  FLD_U16),
    FX("offer_expiry_iterations_remaining",offerExpiryIterationsRemaining,FLD_U16),
    FX("num_foreign_market_players",      numForeignMarketPlayers,     FLD_U16),
    FX("num_home_market_players",         numHomeMarketPlayers,        FLD_U16),
    FX("num_offers",                      numOffers,                   FLD_U16),
    FX("career_seasons_completed",        careerSeasonsCompleted,      FLD_U16),
    FX("num_foreign_market_players_2",    numForeignMarketPlayers2,    FLD_U16),
    FX("pending_foreign_player_count",    pendingForeignPlayerCount,   FLD_U16),
    FX("season_transfer_quota",           seasonTransferQuota,         FLD_U16),
    FX("transfer_pool_slot_limit",        transferPoolSlotLimit,       FLD_U16),
    FX("offer_swap_slot_limit",           offerSwapSlotLimit,          FLD_U16),
    FX("transfer_pool_slots_used",        transferPoolSlotsUsed,       FLD_U16),
    FX("offer_swap_slots_used",           offerSwapSlotsUsed,          FLD_U16),
    FX("num_new_job_offers",              numNewJobOffers,             FLD_U16),
    FX("num_job_offers",                  numJobOffers,                FLD_U16),
    FX("time_to_negotiate",               timeToNegotiate,             FLD_U16),
    FX("num_players_on_transfer_market",  numPlayersOnTransferMarket,  FLD_U16),
    FX("last_offer_primary_player_ordinal",lastOfferPrimaryPlayerOrdinal,FLD_U16),
    FX("last_offer_extra_players_wanted", lastOfferExtraPlayersWanted, FLD_U16),
    FX("last_offer_swap_player_ordinal",  lastOfferSwapPlayerOrdinal,  FLD_U16),
    FX("last_offer_extra_player_ordinal", lastOfferExtraPlayerOrdinal, FLD_U16),
    FX("offer_negotiation_attempt_count", offerNegotiationAttemptCount,FLD_U16),
    FX("transfer_market_rand_offset_0",   transferMarketRandOffset0,   FLD_U16),
    FX("transfer_market_rand_offset_1",   transferMarketRandOffset1,   FLD_U16),
    FX("transfer_market_rand_offset_2",   transferMarketRandOffset2,   FLD_U16),
    FX("transfer_market_phase",           transferMarketPhase,         FLD_U16),
    FX("career_num_players",              careerNumPlayers,            FLD_U16),
    FX("sel_team_selected_player_position",selTeamSelectedPlayerPosition,FLD_U16),
    FX("last_financial_week_index",       lastFinancialWeekIndex,      FLD_U16),
    FX("num_players",                     numPlayers,                  FLD_U16),
    FX("current_team_number",             currentTeamNumber,           FLD_U16),
    FX("current_team_selected_player_position",currentTeamSelectedPlayerPosition,FLD_U16),
    FX("training_match_saved_player_pos", trainingMatchSavedPlayerPos, FLD_U16),
    FX("last_match_saved_player_pos",     lastMatchSavedPlayerPos,     FLD_U16),
    FX("last_match_saved_coach_or_player",lastMatchSavedCoachOrPlayer, FLD_U16),
    FX("current_tactics",                 currentTactics,              FLD_U8 ),
    FX("last_match_saved_team_controls",  lastMatchSavedTeamControls,  FLD_U8 ),
    FX("season_finished",                 seasonFinished,              FLD_U16),
    FX("career_finished",                 careerFinished,              FLD_U16),
    FX("euro_cup_slot_index",             euroCupSlotIndex,            FLD_U16),
    FX("season_number_of_squads",         seasonNumberOfSquads,        FLD_U16),
    FX("team_selection_key_cache_index",  teamSelectionKeyCacheIndex,  FLD_U16),
    FX("team_selection_key_cache_offset", teamSelectionKeyCacheOffset, FLD_U16),
    FX("team_selection_key_cache_count",  teamSelectionKeyCacheCount,  FLD_U16),
    FX("career_slot_processing_in_progress",careerSlotProcessingInProgress,FLD_U16),
    FX("team_selection_key_cache_end",    teamSelectionKeyCacheEnd,    FLD_U16),
    FX("competition_over",                competitionOver,             FLD_U16),
    FA("diy_preset_name_of_competition",  diyPresetNameOfCompetition, 36, FLD_STR),
    FX("major_trophy_bonus_value",        majorTrophyBonusValue,       FLD_U16),
    FX("num_diy_teams",                   numDiyTeams,                 FLD_U16),
    FX("primary_comp_winner_team",        primaryCompWinnerTeam,       FLD_U16),
    FX("primary_comp_squad_view_param",   primaryCompSquadViewParam,   FLD_U16),
    FX("primary_comp_slot_state_a",       primaryCompSlotStateA,       FLD_U16),
    FX("primary_comp_slot_state_b",       primaryCompSlotStateB,       FLD_U16),
    FX("primary_comp_slot_state_c",       primaryCompSlotStateC,       FLD_U16),
    FX("diy_team_min_division_index",     diyTeamMinDivisionIndex,     FLD_U16),
    FX("diy_team_max_division_offset",    diyTeamMaxDivisionOffset,    FLD_U16),
    FX("league_promotion_count",          leaguePromotionCount,        FLD_U16),
    FX("league_relegation_count",         leagueRelegationCount,       FLD_U16),
    FX("league_warnings_enabled",         leagueWarningsEnabled,       FLD_U16),
    FX("view_random_phase",               viewRandomPhase,             FLD_U8 ),
    FX("season_playing",                  seasonPlaying,               FLD_U8 ),
    FX("in_friendly_menu",                inFriendlyMenu,              FLD_U16),
    FX("is_national_team",                isNationalTeam,              FLD_U16),
    FX("game_type",                       gameType,                    FLD_U16),
    FX("contest_started",                 contestStarted,              FLD_U16),
    FX("career_loaded_flag",              careerLoadedFlag,            FLD_U16),
    FX("game_length",                     gameLength,                  FLD_U16),
    FX("auto_replays",                    autoReplays,                 FLD_U16),
    FX("menu_music",                      menuMusic,                   FLD_U16),
    FX("auto_save_highlights",            autoSaveHighlights,          FLD_U16),
    FX("all_player_teams_equal",          allPlayerTeamsEqual,         FLD_U16),
    FX("pitch_type",                      pitchType,                   FLD_U16),
    FX("seed_2",                          seed2,                       FLD_U8 ),
    FX("rand_xor_index_2",                randXorIndex2,               FLD_U8 ),
    FX("rand_xor_key_2",                  randXorKey2,                 FLD_U8 ),
    FX("new_random_seed_copy",            newRandomSeedCopy,           FLD_U8 ),
    FX("random_seed",                     randomSeed,                  FLD_U16),
    FX("save_file_version",               saveFileVersion,             FLD_U16),
    FX("save_file_amiga_type",            saveFileAmigaType,           FLD_U32),
    FX("num_selected_teams_inner",        numSelectedTeams,            FLD_U16),

    /* Comp context names + slot states (secondary/tertiary/quaternary). */
    FA("secondary_competition_name",      secondaryCompetitionName, 36,FLD_STR),
    FX("minor_trophy_bonus_value",        minorTrophyBonusValue,       FLD_U16),
    FX("secondary_comp_winner_team",      secondaryCompWinnerTeam,     FLD_U16),
    FX("secondary_comp_slot_state_a",     secondaryCompSlotStateA,     FLD_U16),
    FX("secondary_comp_slot_state_b",     secondaryCompSlotStateB,     FLD_U16),
    FX("secondary_comp_slot_state_c",     secondaryCompSlotStateC,     FLD_U16),
    FX("secondary_comp_finished",         secondaryCompFinished,       FLD_U16),
    FA("tertiary_competition_name",       tertiaryCompetitionName, 36, FLD_STR),
    FX("league_title_bonus_value",        leagueTitleBonusValue,       FLD_U16),
    FX("tertiary_comp_winner_team",       tertiaryCompWinnerTeam,      FLD_U16),
    FX("tertiary_comp_slot_state_a",      tertiaryCompSlotStateA,      FLD_U16),
    FX("tertiary_comp_slot_state_b",      tertiaryCompSlotStateB,      FLD_U16),
    FX("tertiary_comp_slot_state_c",      tertiaryCompSlotStateC,      FLD_U16),
    FX("tertiary_comp_finished",          tertiaryCompFinished,        FLD_U16),
    FX("diy_comp_slot_state_a",           diyCompSlotStateA,           FLD_U16),
    FX("diy_comp_slot_state_b",           diyCompSlotStateB,           FLD_U16),
    FX("diy_comp_slot_state_c",           diyCompSlotStateC,           FLD_U16),
    FA("quaternary_competition_name",     quaternaryCompetitionName, 36,FLD_STR),
    FX("major_trophy_or_cup_bonus_value", majorTrophyOrCupBonusValue,  FLD_U16),
    FX("quaternary_comp_winner_team",     quaternaryCompWinnerTeam,    FLD_U16),
    FX("quaternary_comp_slot_state_a",    quaternaryCompSlotStateA,    FLD_U16),
    FX("quaternary_comp_slot_state_b",    quaternaryCompSlotStateB,    FLD_U16),
    FX("quaternary_comp_slot_state_c",    quaternaryCompSlotStateC,    FLD_U16),

    /* Small named arrays / packed records. */
    FA("removed_player_stat_accum",       removedPlayerStatAccum, 2,   FLD_BYTES),
    FA("career_unavailable_player_list",  careerUnavailablePlayerList, 16, FLD_BYTES),
    FA("current_match_players",           currentMatchPlayers, 32,     FLD_BYTES),
    FA("season_squad_team_numbers",       seasonSquadTeamNumbers, 48,  FLD_BYTES),
    FA("player_positions",                playerPositions, 32,         FLD_BYTES),
    FA("training_match_saved_players",    trainingMatchSavedPlayers, 32, FLD_BYTES),
    FA("training_match_saved_shirt_nums", trainingMatchSavedShirtNums, 32, FLD_BYTES),
    FA("training_match_saved_tactics",    trainingMatchSavedTactics, 2,FLD_BYTES),
    FA("last_match_players",              lastMatchPlayers, 32,        FLD_BYTES),
    FA("last_match_saved_shirt_nums",     lastMatchSavedShirtNums, 32, FLD_BYTES),
    FA("career_last_match_index_by_competition_slot",
                                          careerLastMatchIndexByCompetitionSlot, 162, FLD_BYTES),
    FA("team_budget_rank_table",          teamBudgetRankTable, 128,    FLD_BYTES),
    FA("players_on_transfer_market",      playersOnTransferMarket, 10, FLD_BYTES),

    /* Market player slot tables — byte arrays of compact records. */
    FA("foreign_market_players",          foreignMarketPlayers, 60,    FLD_BYTES),
    FA("home_market_players",             homeMarketPlayers, 100,      FLD_BYTES),
    FA("transfer_offer_table",            transferOfferTable, 70,      FLD_BYTES),
    FA("new_job_offers",                  newJobOffers, 130,           FLD_BYTES),
    FA("job_offers",                      jobOffers, 130,              FLD_BYTES),

    { NULL, NULL, 0, FLD_END },
};
#undef FX
#undef FA

static void emit_scalar_table(mjson_print_fn_t fn, void *fnd,
                              const char *section_key,
                              const FieldDesc *table)
{
    mjson_printf(fn, fnd, "%Q:{", section_key);
    int first = 1;
    for (const FieldDesc *f = table; f->type != FLD_END; f++) {
        if (!first) mjson_printf(fn, fnd, "%s", ",");
        first = 0;
        const uint8_t *p = (const uint8_t *)f->addr;
        switch (f->type) {
        case FLD_U8:
            mjson_printf(fn, fnd, "%Q:%u", f->name, (unsigned)*p);
            break;
        case FLD_S8:
            mjson_printf(fn, fnd, "%Q:%d", f->name, (int)*(int8_t *)p);
            break;
        case FLD_U16:
            mjson_printf(fn, fnd, "%Q:%u", f->name, (unsigned)*(uint16_t *)p);
            break;
        case FLD_S16:
            mjson_printf(fn, fnd, "%Q:%d", f->name, (int)*(int16_t *)p);
            break;
        case FLD_U32:
            mjson_printf(fn, fnd, "%Q:%lu", f->name, (long)(uint32_t)*(uint32_t *)p);
            break;
        case FLD_S32:
            mjson_printf(fn, fnd, "%Q:%ld", f->name, (long)*(int32_t *)p);
            break;
        case FLD_STR: {
            uint32_t n = 0;
            while (n < f->size && p[n] != '\0') n++;
            mjson_printf(fn, fnd, "%Q:%.*Q", f->name, (int)n, (const char *)p);
            break;
        }
        case FLD_BYTES:
            mjson_printf(fn, fnd, "%Q:[", f->name);
            for (uint32_t i = 0; i < f->size; i++) {
                if (i > 0) mjson_printf(fn, fnd, "%s", ",");
                mjson_printf(fn, fnd, "%u", (unsigned)p[i]);
            }
            mjson_printf(fn, fnd, "%s", "]");
            break;
        default:
            break;
        }
    }
    mjson_printf(fn, fnd, "%s", "}");
}

static void load_scalar_table(const char *buf, int buflen,
                              const char *section_key,
                              const FieldDesc *table)
{
    char path[160];
    char tmp[64];
    double num = 0.0;

    for (const FieldDesc *f = table; f->type != FLD_END; f++) {
        snprintf(path, sizeof(path), "$.%s.%s", section_key, f->name);
        uint8_t *p = (uint8_t *)f->addr;
        switch (f->type) {
        case FLD_U8: case FLD_S8:
            if (mjson_get_number(buf, buflen, path, &num) > 0)
                *p = (uint8_t)(int)num;
            break;
        case FLD_U16: case FLD_S16:
            if (mjson_get_number(buf, buflen, path, &num) > 0)
                *(uint16_t *)p = (uint16_t)(int)num;
            break;
        case FLD_U32: case FLD_S32:
            if (mjson_get_number(buf, buflen, path, &num) > 0)
                *(uint32_t *)p = (uint32_t)(int32_t)num;
            break;
        case FLD_STR:
            memset(p, 0, f->size);
            if (mjson_get_string(buf, buflen, path, tmp, sizeof(tmp)) > 0) {
                uint32_t n = (uint32_t)strlen(tmp);
                if (n >= f->size) n = f->size - 1;
                memcpy(p, tmp, n);
            }
            break;
        case FLD_BYTES: {
            /* Iterate the JSON array via mjson_next when available. */
#if MJSON_ENABLE_NEXT
            const char *arr = NULL;
            int arr_len = 0;
            if (mjson_find(buf, buflen, path, &arr, &arr_len) == MJSON_TOK_ARRAY) {
                int koff, klen, voff, vlen, vt;
                uint32_t i = 0;
                int off = 0;
                while (i < f->size &&
                       (off = mjson_next(arr, arr_len, off,
                                         &koff, &klen, &voff, &vlen, &vt)) != 0) {
                    if (vt == MJSON_TOK_NUMBER) {
                        char nbuf[32];
                        int n = vlen < (int)sizeof(nbuf) - 1 ? vlen : (int)sizeof(nbuf) - 1;
                        memcpy(nbuf, arr + voff, (size_t)n);
                        nbuf[n] = '\0';
                        p[i] = (uint8_t)atoi(nbuf);
                    }
                    i++;
                }
            }
#endif
            break;
        }
        default:
            break;
        }
    }
}

/* ---------- TeamFileEx semantic encoder/decoder ----------
 *
 * TeamFileEx is a 684-byte struct: 76-byte TeamFile header + 16
 * 38-byte PlayerFile entries. Used for careerTeam, currentTeam, and
 * the selTeamsPtr array (16 TeamFileEx records following
 * numSelectedTeams). Encoded as:
 *
 *   { "team": { ... 14 named fields ... },
 *     "players": [ 16 × { ... 13 named fields ... } ] }
 *
 * Offsets via offsetof() — survives the 64-bit migration because
 * both struct layouts are packed (no padding to shift).
 */

extern uint8_t careerTeam;       /* TeamFileEx at careerFileBuffer + 56190 */
extern uint8_t currentTeam;      /* TeamFileEx at careerFileBuffer + 61610 */

/* Large opaque region externs — emitted as named base64 sub-blobs
 * so the JSON has navigable structure for each region instead of
 * one big opaque_data field. Bodies stay byte-identical to legacy;
 * future promotions replace each named blob with a typed encoder. */
extern uint8_t tmdFileBuffer[];                     /* +2,     54720 B */
extern uint8_t careerUnavailablePlayerLookupTable[];/* +55738, 80 B  */
extern uint8_t careerUnavailablePlayerNames[];      /* +55818, 340 B */
extern uint8_t diyLeagueSeasonStateTable[];         /* +59610, 2000 B */
extern uint8_t competitionContext[];                /* +63340, 2900 B */
extern uint8_t tertiaryCompetitionContext[];        /* +66240, 1092 B */
extern uint8_t diyCompetitionContext[];             /* +67332, 1092 B */
extern uint8_t euroCupsCompetitionContext[];        /* +68424, 2900 B */
extern uint8_t teamSelectionKeyCache[];             /* +73292, ~1870 B */
extern uint8_t careerForeignMarketPlayers[];        /* +75162, 18144 B */
extern uint8_t diyTeamIdListType2[];                /* +93462, 148 B */
extern uint8_t diyTeamIdListType0[];                /* +93610, 288 B */
extern uint8_t diyTeamIdListType1[];                /* +93898, 2310 B */
extern uint8_t teamTacticsUSERA[];                  /* +96220, 370 B */
extern uint8_t teamTacticsUSERB[];                  /* +96590, 370 B */
extern uint8_t teamTacticsUSERC[];                  /* +96960, 370 B */
extern uint8_t teamTacticsUSERD[];                  /* +97330, 370 B */
extern uint8_t teamTacticsUSERE[];                  /* +97700, 370 B */
extern uint8_t teamTacticsUSERF[];                  /* +98070, 370 B */
extern uint8_t editTeamsBuffer[];                   /* +99846, ~43.7 KB */
extern uint8_t editTeamsSaveVarsArea[];             /* +143624, ~23.2 KB */

/* *Copy slot region — one big contiguous block at +71452, 1832 B.
 * Individual records resolvable via careerContests; we emit the
 * whole region as one blob since the nameSource pointers (the only
 * pointer-typed fields) are already promoted separately. */
extern uint8_t euroCupCopy[];                       /* +71452 */

/* Helpers for writing/reading fields at struct offsets. */
static void emit_str_at(mjson_print_fn_t fn, void *fnd,
                        const char *key, const uint8_t *base,
                        size_t off, size_t maxlen)
{
    const uint8_t *p = base + off;
    size_t n = 0;
    while (n < maxlen && p[n] != '\0') n++;
    mjson_printf(fn, fnd, "%Q:%.*Q", key, (int)n, (const char *)p);
}

static void emit_u8_at(mjson_print_fn_t fn, void *fnd,
                       const char *key, const uint8_t *base, size_t off)
{
    mjson_printf(fn, fnd, "%Q:%u", key, (unsigned)base[off]);
}

static void emit_s8_at(mjson_print_fn_t fn, void *fnd,
                       const char *key, const uint8_t *base, size_t off)
{
    mjson_printf(fn, fnd, "%Q:%d", key, (int)(int8_t)base[off]);
}

static void emit_s16_at(mjson_print_fn_t fn, void *fnd,
                        const char *key, const uint8_t *base, size_t off)
{
    mjson_printf(fn, fnd, "%Q:%d", key, (int)*(int16_t *)(base + off));
}

static void emit_bytes_at(mjson_print_fn_t fn, void *fnd,
                          const char *key, const uint8_t *base,
                          size_t off, size_t len)
{
    mjson_printf(fn, fnd, "%Q:[", key);
    for (size_t i = 0; i < len; i++) {
        if (i > 0) mjson_printf(fn, fnd, "%s", ",");
        mjson_printf(fn, fnd, "%u", (unsigned)base[off + i]);
    }
    mjson_printf(fn, fnd, "%s", "]");
}

static void load_str_at(const char *buf, int buflen, const char *path,
                        uint8_t *base, size_t off, size_t maxlen)
{
    uint8_t *p = base + off;
    memset(p, 0, maxlen);
    char tmp[64];
    if (mjson_get_string(buf, buflen, path, tmp, sizeof(tmp)) > 0) {
        size_t n = strlen(tmp);
        if (n > maxlen - 1) n = maxlen - 1;
        memcpy(p, tmp, n);
    }
}

static void load_u8_at(const char *buf, int buflen, const char *path,
                       uint8_t *base, size_t off)
{
    double num = 0.0;
    if (mjson_get_number(buf, buflen, path, &num) > 0)
        base[off] = (uint8_t)(int)num;
}

static void load_s16_at(const char *buf, int buflen, const char *path,
                        uint8_t *base, size_t off)
{
    double num = 0.0;
    if (mjson_get_number(buf, buflen, path, &num) > 0)
        *(int16_t *)(base + off) = (int16_t)num;
}

static void load_bytes_at(const char *buf, int buflen, const char *path,
                          uint8_t *base, size_t off, size_t len)
{
#if MJSON_ENABLE_NEXT
    const char *arr = NULL;
    int arr_len = 0;
    if (mjson_find(buf, buflen, path, &arr, &arr_len) == MJSON_TOK_ARRAY) {
        int koff, klen, voff, vlen, vt;
        size_t i = 0;
        int o = 0;
        while (i < len &&
               (o = mjson_next(arr, arr_len, o,
                               &koff, &klen, &voff, &vlen, &vt)) != 0) {
            if (vt == MJSON_TOK_NUMBER) {
                char nbuf[32];
                int n = vlen < (int)sizeof(nbuf) - 1 ? vlen : (int)sizeof(nbuf) - 1;
                memcpy(nbuf, arr + voff, (size_t)n);
                nbuf[n] = '\0';
                base[off + i] = (uint8_t)atoi(nbuf);
            }
            i++;
        }
    }
#else
    (void)buf; (void)buflen; (void)path; (void)base; (void)off; (void)len;
#endif
}

static void emit_team_file_ex_body(mjson_print_fn_t fn, void *fnd,
                                   const uint8_t *base)
{
    const uint8_t *team = base + offsetof(TeamFileEx, team);
    mjson_printf(fn, fnd, "{%Q:{", "team");
    emit_u8_at  (fn, fnd, "country_number",     team, offsetof(TeamFile, countryNumber));
    mjson_printf(fn, fnd, "%s", ",");
    emit_u8_at  (fn, fnd, "team_ordinal",       team, offsetof(TeamFile, teamOrdinal));
    mjson_printf(fn, fnd, "%s", ",");
    emit_s16_at (fn, fnd, "global_team_number", team, offsetof(TeamFile, globalTeamNumber));
    mjson_printf(fn, fnd, "%s", ",");
    emit_u8_at  (fn, fnd, "team_controls",      team, offsetof(TeamFile, teamControls));
    mjson_printf(fn, fnd, "%s", ",");
    emit_str_at (fn, fnd, "team_name",          team, offsetof(TeamFile, teamName), 17);
    mjson_printf(fn, fnd, "%s", ",");
    emit_u8_at  (fn, fnd, "writes_zero_here",   team, offsetof(TeamFile, writesZeroHere));
    mjson_printf(fn, fnd, "%s", ",");
    emit_u8_at  (fn, fnd, "and_with_0xfe",      team, offsetof(TeamFile, andWith0xFE));
    mjson_printf(fn, fnd, "%s", ",");
    emit_u8_at  (fn, fnd, "tactics",            team, offsetof(TeamFile, tactics));
    mjson_printf(fn, fnd, "%s", ",");
    emit_u8_at  (fn, fnd, "league",             team, offsetof(TeamFile, league));
    mjson_printf(fn, fnd, "%s", ",");
    /* 10 colors as one array — easier to read than 10 fields. */
    emit_bytes_at(fn, fnd, "kit_colors",        team, offsetof(TeamFile, prShirtType), 10);
    mjson_printf(fn, fnd, "%s", ",");
    emit_str_at (fn, fnd, "coach_name",         team, offsetof(TeamFile, coachName), 23);
    mjson_printf(fn, fnd, "%s", ",");
    emit_u8_at  (fn, fnd, "some_flag",          team, offsetof(TeamFile, someFlag));
    mjson_printf(fn, fnd, "%s", ",");
    emit_bytes_at(fn, fnd,"player_numbers",     team, offsetof(TeamFile, playerNumbers), 16);
    mjson_printf(fn, fnd, "%s", "}");

    mjson_printf(fn, fnd, ",%Q:[", "players");
    for (int i = 0; i < 16; i++) {
        const uint8_t *p = base + offsetof(TeamFileEx, players)
                                + (size_t)i * sizeof(PlayerFile);
        if (i > 0) mjson_printf(fn, fnd, "%s", ",");
        mjson_printf(fn, fnd, "%s", "{");
        emit_s8_at  (fn, fnd, "nationality",          p, offsetof(PlayerFile, nationality));
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "field_1",              p, offsetof(PlayerFile, field_1));
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "shirt_number",         p, offsetof(PlayerFile, shirtNumber));
        mjson_printf(fn, fnd, "%s", ",");
        emit_str_at (fn, fnd, "player_name",          p, offsetof(PlayerFile, playerName), 22);
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "field_19",             p, offsetof(PlayerFile, field_19));
        mjson_printf(fn, fnd, "%s", ",");
        emit_u8_at  (fn, fnd, "position_and_face",    p, offsetof(PlayerFile, positionAndFace));
        mjson_printf(fn, fnd, "%s", ",");
        emit_u8_at  (fn, fnd, "cards_injuries",       p, offsetof(PlayerFile, cardsInjuries));
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "passing",              p, offsetof(PlayerFile, passing));
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "shooting_heading",     p, offsetof(PlayerFile, shootingHeading));
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "tackling_ball_control",p, offsetof(PlayerFile, tacklingBallControl));
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "speed_finishing",      p, offsetof(PlayerFile, speedFinishing));
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "price",                p, offsetof(PlayerFile, price));
        mjson_printf(fn, fnd, "%s", ",");
        emit_bytes_at(fn, fnd,"goal_stats",           p, offsetof(PlayerFile, goalStats), 4);
        mjson_printf(fn, fnd, "%s", ",");
        emit_s8_at  (fn, fnd, "some_flag",            p, offsetof(PlayerFile, someFlag));
        mjson_printf(fn, fnd, "%s", "}");
    }
    mjson_printf(fn, fnd, "%s", "]}");
}

static void emit_team_file_ex(mjson_print_fn_t fn, void *fnd,
                              const char *key, const uint8_t *base)
{
    mjson_printf(fn, fnd, "%Q:", key);
    emit_team_file_ex_body(fn, fnd, base);
}

static void load_team_file_ex(const char *buf, int buflen,
                              const char *path_prefix, uint8_t *base)
{
    uint8_t *team = base + offsetof(TeamFileEx, team);
    char path[160];
    double num = 0.0;

#define LOAD_U8(field, jsonkey) do { \
        snprintf(path, sizeof(path), "%s.team.%s", path_prefix, jsonkey); \
        load_u8_at(buf, buflen, path, team, offsetof(TeamFile, field)); \
    } while (0)
#define LOAD_S16(field, jsonkey) do { \
        snprintf(path, sizeof(path), "%s.team.%s", path_prefix, jsonkey); \
        load_s16_at(buf, buflen, path, team, offsetof(TeamFile, field)); \
    } while (0)
#define LOAD_STR(field, jsonkey, maxlen) do { \
        snprintf(path, sizeof(path), "%s.team.%s", path_prefix, jsonkey); \
        load_str_at(buf, buflen, path, team, offsetof(TeamFile, field), maxlen); \
    } while (0)
#define LOAD_BYTES(field, jsonkey, len) do { \
        snprintf(path, sizeof(path), "%s.team.%s", path_prefix, jsonkey); \
        load_bytes_at(buf, buflen, path, team, offsetof(TeamFile, field), len); \
    } while (0)

    LOAD_U8 (countryNumber,    "country_number");
    LOAD_U8 (teamOrdinal,      "team_ordinal");
    LOAD_S16(globalTeamNumber, "global_team_number");
    LOAD_U8 (teamControls,     "team_controls");
    LOAD_STR(teamName,         "team_name", 17);
    LOAD_U8 (writesZeroHere,   "writes_zero_here");
    LOAD_U8 (andWith0xFE,      "and_with_0xfe");
    LOAD_U8 (tactics,          "tactics");
    LOAD_U8 (league,           "league");
    LOAD_BYTES(prShirtType,    "kit_colors", 10);
    LOAD_STR(coachName,        "coach_name", 23);
    LOAD_U8 (someFlag,         "some_flag");
    LOAD_BYTES(playerNumbers,  "player_numbers", 16);
#undef LOAD_U8
#undef LOAD_S16
#undef LOAD_STR
#undef LOAD_BYTES

    for (int i = 0; i < 16; i++) {
        uint8_t *p = base + offsetof(TeamFileEx, players)
                          + (size_t)i * sizeof(PlayerFile);

#define LOAD_PU8(field, jsonkey) do { \
        snprintf(path, sizeof(path), "%s.players[%d].%s", path_prefix, i, jsonkey); \
        load_u8_at(buf, buflen, path, p, offsetof(PlayerFile, field)); \
    } while (0)
#define LOAD_PSTR(field, jsonkey, maxlen) do { \
        snprintf(path, sizeof(path), "%s.players[%d].%s", path_prefix, i, jsonkey); \
        load_str_at(buf, buflen, path, p, offsetof(PlayerFile, field), maxlen); \
    } while (0)
#define LOAD_PBYTES(field, jsonkey, len) do { \
        snprintf(path, sizeof(path), "%s.players[%d].%s", path_prefix, i, jsonkey); \
        load_bytes_at(buf, buflen, path, p, offsetof(PlayerFile, field), len); \
    } while (0)

        LOAD_PU8(nationality,         "nationality");
        LOAD_PU8(field_1,             "field_1");
        LOAD_PU8(shirtNumber,         "shirt_number");
        LOAD_PSTR(playerName,         "player_name", 22);
        LOAD_PU8(field_19,            "field_19");
        LOAD_PU8(positionAndFace,     "position_and_face");
        LOAD_PU8(cardsInjuries,       "cards_injuries");
        LOAD_PU8(passing,             "passing");
        LOAD_PU8(shootingHeading,     "shooting_heading");
        LOAD_PU8(tacklingBallControl, "tackling_ball_control");
        LOAD_PU8(speedFinishing,      "speed_finishing");
        LOAD_PU8(price,               "price");
        LOAD_PBYTES(goalStats,        "goal_stats", 4);
        LOAD_PU8(someFlag,            "some_flag");
#undef LOAD_PU8
#undef LOAD_PSTR
#undef LOAD_PBYTES
    }
    (void)num;
}

/* ---------- named base64 blob regions ----------
 *
 * Mid-step between opaque_data and full semantic JSON. Each big
 * sub-region of careerFileBuffer that we haven't promoted yet gets
 * a named entry under "blobs" with a base64 payload of its bytes.
 * On load, base64-decode each into the region's memory location.
 * Promoted semantic fields apply AFTER blobs, so e.g.
 * cup_record_name_sources still overrides the relevant 4-byte
 * pointer slots inside the *Copy region's bytes.
 *
 * When every byte of careerFileBuffer is covered by either a typed
 * field or a named blob, we can drop opaque_data entirely.
 */

typedef struct {
    const char *name;
    void       *addr;
    uint32_t    size;
} BlobDesc;

static const BlobDesc s_career_blobs[] = {
    /* tmd_file_buffer extended back by 2 bytes — the symbol
     * tmdFileBuffer is at &careerFileBuffer + 2, but bytes 0..1 of
     * the buffer carry a 2-byte length/count prefix that the
     * Amiga code reads/writes alongside the TMD payload. */
    { "tmd_file_buffer",                       &careerFileBuffer,              54722 },
    /* Extra 608 bytes after careerTeam / currentTeam — these are
     * 16 additional PlayerFile entries (reserves / subs roster).
     * TeamFileEx is 684 bytes (TeamFile + 16 players); the on-disk
     * career/current team slots are 1292 bytes (TeamFile + 32
     * players). emit_team_file_ex covers the first 16, blobs
     * cover the second 16 here until a proper extended schema
     * lands. */
    { "career_team_reserves",                  (uint8_t *)&careerTeam  + 684,  608 },
    { "current_team_reserves",                 (uint8_t *)&currentTeam + 684,  608 },
    /* Padding/unnamed bytes between primaryCompStringOffset
     * (+93348..+93351) and diyTeamIdListType2 (+93462). 5 gaps
     * totalling 86 bytes between named scalars. Easiest cover is
     * one wrapper blob over the whole region (110 B) — the
     * scalar table's named fields override their bytes after the
     * blob runs in apply_semantic_load. */
    { "primary_comp_block_padding",            (uint8_t *)&careerFileBuffer + 93352, 110 },
    { "career_unavailable_player_lookup_table",careerUnavailablePlayerLookupTable, 80 },
    { "career_unavailable_player_names",       careerUnavailablePlayerNames,    340 },
    { "diy_league_season_state_table",         diyLeagueSeasonStateTable,      2000 },
    { "competition_context",                   competitionContext,             2900 },
    { "tertiary_competition_context",          tertiaryCompetitionContext,     1092 },
    { "diy_competition_context",               diyCompetitionContext,          1092 },
    { "euro_cups_competition_context",         euroCupsCompetitionContext,     2900 },
    { "team_selection_key_cache",              teamSelectionKeyCache,          1870 },
    { "career_foreign_market_players",         careerForeignMarketPlayers,    18144 },
    { "diy_team_id_list_type_2",               diyTeamIdListType2,              148 },
    { "diy_team_id_list_type_0",               diyTeamIdListType0,              288 },
    { "diy_team_id_list_type_1",               diyTeamIdListType1,             2310 },
    { "team_tactics_user_a",                   teamTacticsUSERA,                370 },
    { "team_tactics_user_b",                   teamTacticsUSERB,                370 },
    { "team_tactics_user_c",                   teamTacticsUSERC,                370 },
    { "team_tactics_user_d",                   teamTacticsUSERD,                370 },
    { "team_tactics_user_e",                   teamTacticsUSERE,                370 },
    { "team_tactics_user_f",                   teamTacticsUSERF,                370 },
    /* *Copy slot region: from euroCupCopy to oceaniaWorldCupQualificationCopy
     * +0x24. Total 1832 bytes. */
    { "comp_copy_region",                      euroCupCopy,                    1832 },
    { "edit_teams_buffer",                     editTeamsBuffer,               43778 },
    { "edit_teams_save_vars_area",             editTeamsSaveVarsArea,         23254 },
    { NULL, NULL, 0 },
};

static void emit_named_blobs(mjson_print_fn_t fn, void *fnd)
{
    mjson_printf(fn, fnd, "%Q:{", "blobs");
    int first = 1;
    for (const BlobDesc *b = s_career_blobs; b->name; b++) {
        if (!first) mjson_printf(fn, fnd, "%s", ",");
        first = 0;
        mjson_printf(fn, fnd, "%Q:%V", b->name, (int)b->size, (const char *)b->addr);
    }
    mjson_printf(fn, fnd, "%s", "}");
}

static void load_named_blobs(const char *buf, int buflen)
{
    char path[160];
    for (const BlobDesc *b = s_career_blobs; b->name; b++) {
        snprintf(path, sizeof(path), "$.blobs.%s", b->name);
        mjson_get_base64(buf, buflen, path,
                         (char *)b->addr, (int)b->size);
    }
}

/* ---------- *Copy cup-record nameSource pointers ----------
 *
 * Each entry in the static careerContests table maps a template
 * competition record to its runtime *Copy inside competitionFileBuffer.
 * Each *Copy carries a `nameSource` pointer at offset
 * `record[5] + 5` (the byte at record[5] is the offset of the
 * nameSource field within the record). Those pointers are saved with
 * the *Copy buffer and need symbolic encoding to survive rebuilds.
 *
 * Layout per careerContests entry (10 bytes):
 *   +0 src  ptr  (template static record — not saved)
 *   +4 dest ptr  (the *Copy in competitionFileBuffer — not saved)
 *   +8 size word (byte count — not saved)
 *
 * The 0xFFFFFFFF src value terminates the table.
 */

extern uint8_t careerContests[];

static void emit_cup_record_name_sources(mjson_print_fn_t fn, void *fnd)
{
    mjson_printf(fn, fnd, "%Q:{", "cup_record_name_sources");
    uint8_t *tbl = careerContests;
    int first = 1;
    for (;;) {
        uint32_t srcAddr = *(uint32_t *)tbl;
        if (srcAddr == 0xFFFFFFFFu)
            break;
        uint8_t *dest = *(uint8_t **)(tbl + 4);
        int8_t  nameOffset = (int8_t)dest[5];
        if (nameOffset != 0) {
            /* Look up the dest's symbol name so the JSON key is
             * self-describing. */
            uint32_t off = 0;
            const AmigaAddrLutEntry *e =
                sym_from_ptr((uint32_t)(uintptr_t)dest, &off);
            const char *key = e ? e->name : "unknown";
            if (!first) mjson_printf(fn, fnd, "%s", ",");
            first = 0;
            uint32_t val = *(uint32_t *)(dest + 5 + nameOffset);
            emit_ptr_field(fn, fnd, key, val);
        }
        tbl += 10;
    }
    mjson_printf(fn, fnd, "%s", "}");
}

static void load_cup_record_name_sources(const char *buf, int buflen)
{
    char path[160];
    uint8_t *tbl = careerContests;
    for (;;) {
        uint32_t srcAddr = *(uint32_t *)tbl;
        if (srcAddr == 0xFFFFFFFFu)
            break;
        uint8_t *dest = *(uint8_t **)(tbl + 4);
        int8_t  nameOffset = (int8_t)dest[5];
        if (nameOffset != 0) {
            uint32_t off = 0;
            const AmigaAddrLutEntry *e =
                sym_from_ptr((uint32_t)(uintptr_t)dest, &off);
            if (e) {
                snprintf(path, sizeof(path),
                         "$.cup_record_name_sources.%s", e->name);
                uint32_t val = 0;
                load_ptr_field(buf, buflen, path, &val);
                *(uint32_t *)(dest + 5 + nameOffset) = val;
            }
        }
        tbl += 10;
    }
}

/* ---------- .CAR writer ---------- */

int16_t SaveJson_WriteCareer(uint8_t *filename)
{
    uint32_t data_size = career_data_size();
    JsonOutBuf out = { NULL, 0, 0, 0 };

    /* Top-level object. Every byte of careerFileBuffer is now
     * covered by either a typed semantic field or a named blob
     * (verified at runtime with SWOS_SAVE_FORMAT_JSON_VERIFY before
     * opaque_data was dropped — see SAVE_FORMAT_JSON.md). Save files
     * are now pointer-portable and 64-bit-friendly by construction. */
    (void)data_size;
    mjson_printf(json_out_emit, &out,
        "{\n"
        "  %Q: %d,\n"
        "  %Q: %Q,\n"
        "  %Q: %d,\n  ",
        "schema_version", SAVE_JSON_SCHEMA_VERSION,
        "file_kind",      KIND_CAREER,
        "num_selected_teams", (int)numSelectedTeams);

    emit_season_informations(json_out_emit, &out);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_career_pointers(json_out_emit, &out);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_cup_record_name_sources(json_out_emit, &out);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_scalar_table(json_out_emit, &out, "career_scalars", s_career_scalars);

    /* Two top-level TeamFileEx records (careerTeam, currentTeam) and
     * the variable-length selTeamsPtr array following
     * numSelectedTeams in the save. */
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_team_file_ex(json_out_emit, &out, "career_team",  &careerTeam);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_team_file_ex(json_out_emit, &out, "current_team", &currentTeam);

    mjson_printf(json_out_emit, &out, ",\n  %Q:[", "selected_teams");
    for (uint16_t i = 0; i < numSelectedTeams; i++) {
        if (i > 0) mjson_printf(json_out_emit, &out, "%s", ",");
        emit_team_file_ex_body(json_out_emit, &out,
                               (&selTeamsPtr) + (uint32_t)i * 684u);
    }
    mjson_printf(json_out_emit, &out, "%s", "]");

    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_named_blobs(json_out_emit, &out);

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

/* Probe the first bytes of the named save file via stdio. Used by
 * the file_io.c dispatcher to decide which loader to invoke without
 * having to stage the whole file through _LoadFileEx (which the
 * JSON-format size would overflow). Returns 1 if the file begins
 * with `{` (after whitespace), 0 otherwise, or -1 on open error. */
int SaveJson_FileIsJson(const uint8_t *filename)
{
    char path[512];
    const char *src = (const char *)filename;
    const char *colon = strchr(src, ':');
    if (colon) src = colon + 1;
    snprintf(path, sizeof(path), "save/%s", src);

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t probe[16];
    size_t got = fread(probe, 1, sizeof(probe), f);
    fclose(f);
    if (got == 0) return 0;
    for (size_t i = 0; i < got; i++) {
        uint8_t c = probe[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        return (c == '{') ? 1 : 0;
    }
    return 0;
}

/* ---------- .CAR loader ---------- */

/* Helper that runs the named-blob + semantic field loaders in
 * dependency order. Used by both the main load path (after
 * opaque_data has been laid down) and the verify path (against a
 * zeroed buffer so we can diff). */
static void apply_semantic_load(const char *src, int src_size)
{
    load_named_blobs(src, src_size);
    load_season_informations(src, src_size);
    load_career_pointers(src, src_size);
    load_cup_record_name_sources(src, src_size);
    load_scalar_table(src, src_size, "career_scalars", s_career_scalars);

    load_team_file_ex(src, src_size, "$.career_team",  &careerTeam);
    load_team_file_ex(src, src_size, "$.current_team", &currentTeam);

    /* numSelectedTeams comes through career_scalars
     * (num_selected_teams_inner aliases &numSelectedTeams). Now use
     * it to size the selected_teams loop. */
    char selpath[80];
    for (uint16_t i = 0; i < numSelectedTeams; i++) {
        snprintf(selpath, sizeof(selpath), "$.selected_teams[%u]", (unsigned)i);
        load_team_file_ex(src, src_size, selpath,
                          (&selTeamsPtr) + (uint32_t)i * 684u);
    }
}

int16_t SaveJson_LoadCareer(uint8_t *filename, const uint8_t *src_unused,
                            uint32_t src_size_unused)
{
    /* JSON saves can be several hundred KB after base64 expansion —
     * larger than careerFileBuffer can hold as a destination — so
     * we ignore the legacy in-place `src` buffer and read the file
     * fresh from disk into a heap allocation sized to the file. */
    (void)src_unused; (void)src_size_unused;

    uint8_t *src = NULL;
    uint32_t src_size = 0;
    if (read_save_file_heap(filename, &src, &src_size) != 0)
        return 1;

    /* Verify schema_version. */
    double version = 0.0;
    if (mjson_get_number((const char *)src, (int)src_size,
                         "$.schema_version", &version) <= 0) {
        free(src);
        return 1;
    }
    if ((int)version != SAVE_JSON_SCHEMA_VERSION) {
        free(src);
        return 1;
    }

    /* Verify file_kind. */
    char kind[32] = {0};
    if (mjson_get_string((const char *)src, (int)src_size,
                         "$.file_kind", kind, sizeof(kind)) <= 0) {
        free(src);
        return 1;
    }
    if (strcmp(kind, KIND_CAREER) != 0) {
        free(src);
        return 1;
    }

    /* opaque_data has been retired (verified zero-mismatch with
     * SWOS_SAVE_FORMAT_JSON_VERIFY before removal). For
     * backwards-compat with saves written before the cutover, try
     * decoding it if present so old TEST.CAR files keep loading;
     * otherwise pre-zero the save region and let blobs+semantic
     * populate every byte from named JSON fields. */
    uint32_t want = career_data_size();
    int legacy_got = mjson_get_base64((const char *)src, (int)src_size,
                                      "$.opaque_data",
                                      (char *)&careerFileBuffer, (int)want);
    if (legacy_got <= 0) {
        memset(&careerFileBuffer, 0, want);
    }

    apply_semantic_load((const char *)src, (int)src_size);

    free(src);
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
