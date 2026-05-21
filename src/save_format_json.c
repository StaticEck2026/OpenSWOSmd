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
    uint32_t    offset;     /* relative to &careerFileBuffer */
    uint32_t    size;       /* for FLD_STR / FLD_BYTES; ignored for scalars */
    FieldType   type;
} FieldDesc;

/* Career meta / flags / finance / market state — covers offsets
 * 54722..56189 except the regions already promoted elsewhere
 * (careerCupPickList[5] at 55086, comp-string offsets, etc.). */
static const FieldDesc s_career_scalars[] = {
    { "current_season_showing",          54722, 0, FLD_U16 },
    { "staying_in_old_club",             54724, 0, FLD_U16 },
    { "overdraft_streak",                54726, 0, FLD_U16 },
    { "overdraft_warning_state",         54728, 0, FLD_U16 },
    { "performance_warning_state",       54730, 0, FLD_U16 },
    { "overdraft_warning_cooldown",      54732, 0, FLD_U16 },
    { "performance_warning_cooldown",    54734, 0, FLD_U16 },
    { "youre_sacked",                    54736, 0, FLD_U16 },
    { "retired",                         54738, 0, FLD_U16 },
    { "competition_slot_updated_flag",   54740, 0, FLD_U16 },
    { "career_team_lost_match",          54742, 0, FLD_U16 },
    { "club_business_active",            54744, 0, FLD_U16 },
    { "new_balance",                     54746, 0, FLD_S32 },
    { "old_balance",                     54750, 0, FLD_S32 },
    { "player_purchases",                54754, 0, FLD_S32 },
    { "player_sales",                    54758, 0, FLD_S32 },
    { "general_running_costs",           54762, 0, FLD_S32 },
    { "player_wages_bill",               54766, 0, FLD_S32 },
    { "gate_receipts",                   54770, 0, FLD_S32 },
    { "starting_balance",                54774, 0, FLD_S32 },
    { "starting_balance_copy",           54778, 0, FLD_S32 },
    { "competition_bonuses",             54782, 0, FLD_S32 },
    { "sensisoft_plc_sponsorship",       54786, 0, FLD_S32 },
    { "finance_totals_need_reset_flag",  54790, 0, FLD_U16 },
    { "weekly_running_costs",            54796, 0, FLD_U32 },
    { "weekly_player_wages",             54800, 0, FLD_U32 },
    { "total_team_value",                54804, 0, FLD_S32 },
    { "weekly_wage_by_player_index",     54808, 128, FLD_BYTES },
    { "player_form_modifiers",           54936, 32, FLD_BYTES },
    { "sensisoft_plc_sponsorship_factor",54968, 0, FLD_U16 },
    { "additional_investments",          54970, 0, FLD_S32 },  /* 6 bytes — actually 2x u16/u32? Use 4B for now, rest opaque */
    { "team_value_divisor",              54976, 0, FLD_U16 },
    { "expected_league_rank_index",      54978, 0, FLD_U16 },
    { "board_confidence_history",        54980, 8, FLD_BYTES }, /* 4 × u16 */
    { "board_confidence_score",          54988, 0, FLD_U16 },
    { "previous_board_confidence",       54990, 0, FLD_U16 },
    { "play_win_cup_animation",          54992, 0, FLD_U16 },
    { "minor_trophy_bonus_active",       54994, 0, FLD_U16 },
    { "league_title_bonus_active",       54996, 0, FLD_U16 },
    { "major_trophy_bonus_active",       54998, 0, FLD_U16 },
    { "cup_bonus_1_active",              55000, 0, FLD_U16 },
    { "cup_bonus_2_active",              55002, 0, FLD_U16 },
    { "won_world_cup",                   55004, 0, FLD_U16 },
    { "won_continental_cup",             55006, 0, FLD_U16 },
    { "won_invitation_tournament",       55008, 0, FLD_U16 },
    { "qualifying_stage_1_pending_flag0",55010, 0, FLD_U16 },
    { "qualifying_stage_1_pending_flag1",55012, 0, FLD_U16 },
    { "qualifying_stage_2_pending_flag0",55014, 0, FLD_U16 },
    { "qualifying_stage_2_pending_flag1",55016, 0, FLD_U16 },
    { "is_career",                       55018, 0, FLD_U16 },
    { "career_match_simulated_flag",     55020, 0, FLD_U16 },
    { "games_played",                    55022, 0, FLD_U16 },
    { "num_wins",                        55024, 0, FLD_U16 },
    { "num_drawn",                       55026, 0, FLD_U16 },
    { "num_lost",                        55028, 0, FLD_U16 },
    { "goals_scored",                    55030, 0, FLD_U16 },
    { "goals_taken",                     55032, 0, FLD_U16 },
    { "board_confidence_mods",           55034, 20, FLD_BYTES }, /* 10 × u16 — boardConfidenceMod0..9 */
    { "board_confidence_value_0",        55042, 0, FLD_U16 }, /* overlaps board_confidence_mods — kept for symmetry */
    { "board_confidence_value_1",        55044, 0, FLD_U16 },
    { "contract_negotiation_active",     55058, 0, FLD_U16 },
    { "wage_scaling_divisor",            55060, 0, FLD_U16 },
    { "team_no_offer_accepted",          55062, 0, FLD_U16 },
    { "team_funds_accepted",             55064, 0, FLD_S32 },
    { "is_national_selector",            55068, 0, FLD_U16 },
    { "some_team_flags",                 55070, 0, FLD_U16 },
    { "last_nationality_call",           55072, 0, FLD_U16 },
    { "national_team_nationality",       55074, 0, FLD_U8  },
    { "national_team_file_id",           55075, 0, FLD_U8  },
    { "same_national_call_flag",         55076, 0, FLD_U16 },
    { "sacked_reputation_score",         55078, 0, FLD_U16 },
    { "national_selector_reputation_points",55080,0,FLD_U16 },
    { "national_team_file_code",         55082, 0, FLD_U16 },
    { "season_review",                   55084, 0, FLD_U16 },
    { "world_cup_host_team",             55106, 0, FLD_U16 },
    { "world_cup_qualifying_team",       55108, 0, FLD_U16 },
    { "euro_championship_qual_team",     55110, 0, FLD_U16 },
    { "sel_team_number",                 55112, 0, FLD_U16 },
    { "coach_or_player",                 55114, 0, FLD_U16 },
    { "career_team_league",              55116, 0, FLD_U16 },
    { "current_division_index",          55118, 0, FLD_U16 },
    { "league_first_round_number",       55120, 0, FLD_U16 },
    { "play_promoted_animation",         55122, 0, FLD_U16 },
    { "play_relegated_animation",        55124, 0, FLD_U16 },
    { "mr_or_ms",                        55126, 0, FLD_U16 },
    { "manager_sex_buffer",              55128, 4,  FLD_STR },
    { "manager_name_buffer",             55132, 9,  FLD_STR },
    { "manager_surname_buffer",          55141, 13, FLD_STR },
    { "pl_nationality",                  55154, 0, FLD_U16 },
    { "win_cup_bonus_count",             55156, 0, FLD_U16 },
    { "minor_trophy_bonus_count",        55158, 0, FLD_U16 },
    { "major_trophy_bonus_count",        55160, 0, FLD_U16 },
    { "career_reputation_points",        55162, 0, FLD_S16 },
    { "removed_offer_players_from_selected_team_count",55164,0,FLD_U16 },
    { "removed_offer_players_from_other_teams_count",  55166,0,FLD_U16 },
    { "home_market_update_backlog",      55168, 0, FLD_U16 },
    { "foreign_market_update_backlog",   55170, 0, FLD_U16 },
    { "offer_expiry_iterations_remaining",55172,0,FLD_U16 },
    { "num_foreign_market_players",      55174, 0, FLD_U16 },
    { "num_home_market_players",         55176, 0, FLD_U16 },
    { "num_offers",                      55178, 0, FLD_U16 },
    { "career_seasons_completed",        55410, 0, FLD_U16 },
    { "num_foreign_market_players_2",    55412, 0, FLD_U16 },
    { "pending_foreign_player_count",    55414, 0, FLD_U16 },
    { "season_transfer_quota",           55416, 0, FLD_U16 },
    { "transfer_pool_slot_limit",        55418, 0, FLD_U16 },
    { "offer_swap_slot_limit",           55420, 0, FLD_U16 },
    { "transfer_pool_slots_used",        55422, 0, FLD_U16 },
    { "offer_swap_slots_used",           55424, 0, FLD_U16 },
    { "num_new_job_offers",              55426, 0, FLD_U16 },
    { "num_job_offers",                  55558, 0, FLD_U16 },
    { "time_to_negotiate",               55690, 0, FLD_U16 },
    { "num_players_on_transfer_market",  55692, 0, FLD_U16 },
    { "last_offer_primary_player_ordinal",55704,0,FLD_U16 },
    { "last_offer_extra_players_wanted", 55706, 0, FLD_U16 },
    { "last_offer_swap_player_ordinal",  55708, 0, FLD_U16 },
    { "last_offer_extra_player_ordinal", 55710, 0, FLD_U16 },
    { "offer_negotiation_attempt_count", 55712, 0, FLD_U16 },
    { "transfer_market_rand_offset_0",   55714, 0, FLD_U16 },
    { "transfer_market_rand_offset_1",   55716, 0, FLD_U16 },
    { "transfer_market_rand_offset_2",   55718, 0, FLD_U16 },
    { "transfer_market_phase",           55720, 0, FLD_U16 },
    { "career_num_players",              57482, 0, FLD_U16 },
    { "sel_team_selected_player_position",57484,0, FLD_U16 },
    { "last_financial_week_index",       59608, 0, FLD_U16 },
    { "num_players",                     62934, 0, FLD_U16 },
    { "current_team_number",             62936, 0, FLD_U16 },
    { "current_team_selected_player_position",62938,0,FLD_U16 },
    { "training_match_saved_player_pos", 62940, 0, FLD_U16 },
    { "last_match_saved_player_pos",     63008, 0, FLD_U16 },
    { "last_match_saved_coach_or_player",63010, 0, FLD_U16 },
    { "current_tactics",                 63076, 0, FLD_U8  },
    { "last_match_saved_team_controls",  63077, 0, FLD_U8  },
    { "season_finished",                 63240, 0, FLD_U16 },
    { "career_finished",                 63242, 0, FLD_U16 },
    { "euro_cup_slot_index",             63264, 0, FLD_U16 },
    { "season_number_of_squads",         63270, 0, FLD_U16 },
    { "team_selection_key_cache_index",  73284, 0, FLD_U16 },
    { "team_selection_key_cache_offset", 73286, 0, FLD_U16 },
    { "team_selection_key_cache_count",  73288, 0, FLD_U16 },
    { "career_slot_processing_in_progress",73290,0,FLD_U16 },
    { "team_selection_key_cache_end",    93306, 0, FLD_U16 },
    { "competition_over",                93310, 0, FLD_U16 },
    { "diy_preset_name_of_competition",  93312, 36, FLD_STR },
    { "major_trophy_bonus_value",        93356, 0, FLD_U16 },
    { "num_diy_teams",                   93358, 0, FLD_U16 },
    { "primary_comp_winner_team",        93366, 0, FLD_U16 },
    { "primary_comp_squad_view_param",   93368, 0, FLD_U16 },
    { "primary_comp_slot_state_a",       93370, 0, FLD_U16 },
    { "primary_comp_slot_state_b",       93372, 0, FLD_U16 },
    { "primary_comp_slot_state_c",       93374, 0, FLD_U16 },
    { "diy_team_min_division_index",     93392, 0, FLD_U16 },
    { "diy_team_max_division_offset",    93394, 0, FLD_U16 },
    { "league_promotion_count",          93396, 0, FLD_U16 },
    { "league_relegation_count",         93398, 0, FLD_U16 },
    { "league_warnings_enabled",         93404, 0, FLD_U16 },
    { "view_random_phase",               96208, 0, FLD_U8  },
    { "season_playing",                  96209, 0, FLD_U8  },
    { "in_friendly_menu",                96210, 0, FLD_U16 },
    { "is_national_team",                96212, 0, FLD_U16 },
    { "game_type",                       96214, 0, FLD_U16 },
    { "contest_started",                 96216, 0, FLD_U16 },
    { "career_loaded_flag",              96218, 0, FLD_U16 },
    { "game_length",                     98440, 0, FLD_U16 },
    { "auto_replays",                    98442, 0, FLD_U16 },
    { "menu_music",                      98444, 0, FLD_U16 },
    { "auto_save_highlights",            98446, 0, FLD_U16 },
    { "all_player_teams_equal",          98448, 0, FLD_U16 },
    { "pitch_type",                      98450, 0, FLD_U16 },
    { "seed_2",                          98452, 0, FLD_U8  },
    { "rand_xor_index_2",                98453, 0, FLD_U8  },
    { "rand_xor_key_2",                  98454, 0, FLD_U8  },
    { "new_random_seed_copy",            98455, 0, FLD_U8  },
    { "random_seed",                     98456, 0, FLD_U16 },
    { "save_file_version",               98458, 0, FLD_U16 },
    { "save_file_amiga_type",            98472, 0, FLD_U32 },
    { "num_selected_teams_inner",        98476, 0, FLD_U16 },

    /* Comp context names + slot states (secondary/tertiary/quaternary).
     * The +0x6E inside each comp context holds the user-facing display
     * name; +0x90 holds the string-offset pointer (already promoted as
     * a pointer above). */
    { "secondary_competition_name",      63344, 36, FLD_STR },
    { "minor_trophy_bonus_value",        63388, 0, FLD_U16 },
    { "secondary_comp_winner_team",      63398, 0, FLD_U16 },
    { "secondary_comp_slot_state_a",     63402, 0, FLD_U16 },
    { "secondary_comp_slot_state_b",     63404, 0, FLD_U16 },
    { "secondary_comp_slot_state_c",     63406, 0, FLD_U16 },
    { "secondary_comp_finished",         63416, 0, FLD_U16 },
    { "tertiary_competition_name",       66244, 36, FLD_STR },
    { "league_title_bonus_value",        66288, 0, FLD_U16 },
    { "tertiary_comp_winner_team",       66298, 0, FLD_U16 },
    { "tertiary_comp_slot_state_a",      66302, 0, FLD_U16 },
    { "tertiary_comp_slot_state_b",      66304, 0, FLD_U16 },
    { "tertiary_comp_slot_state_c",      66306, 0, FLD_U16 },
    { "tertiary_comp_finished",          66316, 0, FLD_U16 },
    { "diy_comp_slot_state_a",           67394, 0, FLD_U16 },
    { "diy_comp_slot_state_b",           67396, 0, FLD_U16 },
    { "diy_comp_slot_state_c",           67398, 0, FLD_U16 },
    { "quaternary_competition_name",     68428, 36, FLD_STR },
    { "major_trophy_or_cup_bonus_value", 68472, 0, FLD_U16 },
    { "quaternary_comp_winner_team",     68482, 0, FLD_U16 },
    { "quaternary_comp_slot_state_a",    68486, 0, FLD_U16 },
    { "quaternary_comp_slot_state_b",    68488, 0, FLD_U16 },
    { "quaternary_comp_slot_state_c",    68490, 0, FLD_U16 },

    /* Misc small arrays / records. */
    { "removed_player_stat_accum",       57486, 2, FLD_BYTES },
    { "career_unavailable_player_list",  55722, 16, FLD_BYTES },
    { "current_match_players",           56158, 32, FLD_BYTES },
    { "season_squad_team_numbers",       63272, 48, FLD_BYTES },
    { "player_positions",                62902, 32, FLD_BYTES },
    { "training_match_saved_players",    62942, 32, FLD_BYTES },
    { "training_match_saved_shirt_nums", 62974, 32, FLD_BYTES },
    { "training_match_saved_tactics",    63006, 2,  FLD_BYTES },
    { "last_match_players",              63012, 32, FLD_BYTES },
    { "last_match_saved_shirt_nums",     63044, 32, FLD_BYTES },
    { "career_last_match_index_by_competition_slot", 63078, 162, FLD_BYTES },
    { "team_budget_rank_table",          71324, 128, FLD_BYTES },
    { "players_on_transfer_market",      55694, 10, FLD_BYTES },

    /* Market player slot tables — byte arrays of compact records. */
    { "foreign_market_players",          55180, 60,  FLD_BYTES },
    { "home_market_players",             55240, 100, FLD_BYTES },
    { "transfer_offer_table",            55340, 70,  FLD_BYTES },
    { "new_job_offers",                  55428, 130, FLD_BYTES },
    { "job_offers",                      55560, 130, FLD_BYTES },

    { NULL, 0, 0, FLD_END },
};

static void emit_scalar_table(mjson_print_fn_t fn, void *fnd,
                              const char *section_key,
                              const FieldDesc *table)
{
    mjson_printf(fn, fnd, "%Q:{", section_key);
    int first = 1;
    for (const FieldDesc *f = table; f->type != FLD_END; f++) {
        if (!first) mjson_printf(fn, fnd, "%s", ",");
        first = 0;
        const uint8_t *p = (&careerFileBuffer) + f->offset;
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
        uint8_t *p = (&careerFileBuffer) + f->offset;
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
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_career_pointers(json_out_emit, &out);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_cup_record_name_sources(json_out_emit, &out);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_scalar_table(json_out_emit, &out, "career_scalars", s_career_scalars);

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
     * raw pointer values, load_* overwrites them with current-build
     * addresses resolved from stable symbol names. */
    load_season_informations((const char *)src, (int)src_size);
    load_career_pointers((const char *)src, (int)src_size);
    load_cup_record_name_sources((const char *)src, (int)src_size);
    load_scalar_table((const char *)src, (int)src_size, "career_scalars", s_career_scalars);

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
