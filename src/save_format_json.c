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
#include "endian_helpers.h"

#include <stddef.h>  /* offsetof */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* Profile helpers — only emit when SWOS_SAVE_FORMAT_JSON_PROFILE is
 * defined. Times each labeled phase in microseconds via clock(),
 * which is good enough resolution for the kind of work we're
 * measuring (multi-millisecond JSON parse / file I/O). */
#ifdef SWOS_SAVE_FORMAT_JSON_PROFILE
#define PROF_START(name) clock_t prof_##name = clock()
#define PROF_END(name, label) do { \
    clock_t _e = clock(); \
    double _ms = (double)(_e - prof_##name) * 1000.0 / (double)CLOCKS_PER_SEC; \
    fprintf(stderr, "[json-prof-%s] %s: %.2f ms\n", \
            (label), #name, _ms); \
    fflush(stderr); \
} while (0)
#else
#define PROF_START(name) ((void)0)
#define PROF_END(name, label) ((void)0)
#endif

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

#ifdef SWOS_SAVE_FORMAT_JSON_VERIFY
/* Generic round-trip verify: snapshot a memory region, zero it,
 * call the just-written file's loader, then diff against the
 * snapshot. Used by each Write* path to confirm the JSON schema
 * round-trips bit-exactly for that save type. */
typedef int16_t (*VerifyReloadFn)(uint8_t *filename);

static void verify_roundtrip_region(const char *label,
                                    uint8_t *region, uint32_t size,
                                    VerifyReloadFn reload,
                                    uint8_t *filename)
{
    uint8_t *snap = (uint8_t *)malloc(size);
    if (!snap) return;
    memcpy(snap, region, size);
    memset(region, 0, size);

    reload(filename);

    size_t mismatches = 0;
    size_t first = (size_t)-1;
    for (uint32_t i = 0; i < size; i++) {
        if (region[i] != snap[i]) {
            if (mismatches < 20) {
                if (first == (size_t)-1) first = i;
                fprintf(stderr,
                        "[json-verify-%s] mismatch at +%u: loaded=0x%02X saved=0x%02X\n",
                        label, (unsigned)i,
                        (unsigned)region[i], (unsigned)snap[i]);
            }
            mismatches++;
        }
    }
    fprintf(stderr,
            "[json-verify-%s] %zu mismatches across %u bytes (first at +%zu)\n",
            label, mismatches, (unsigned)size, first);

    memcpy(region, snap, size);
    free(snap);
}

/* Reload wrappers with the simpler `(filename)` signature so the
 * verify helper can take a uniform function pointer. */
static int16_t verify_reload_career(uint8_t *f)
{
    return SaveJson_LoadCareer(f, NULL, 0);
}
static int16_t verify_reload_preset(uint8_t *f)
{
    return SaveJson_LoadPreset(f, NULL, 0);
}
static int16_t verify_reload_diy(uint8_t *f)
{
    return SaveJson_LoadDIY(f, NULL, 0);
}
static int16_t verify_reload_highlights(uint8_t *f)
{
    return SaveJson_LoadHighlights(f, NULL, 0);
}
#endif

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

/* Returns 1 if `val` already points inside our static string pool —
 * we use this in normalize_ptr_for_save to avoid re-interning a
 * pool-resident string back into the pool (which would leak
 * sequential copies of the same content). */
static int looks_pool(uint32_t val)
{
    uint32_t lo = (uint32_t)(uintptr_t)&s_string_pool[0];
    uint32_t hi = lo + STRING_POOL_SIZE;
    return val >= lo && val < hi;
}

/* Heuristic readability check for `.rodata` string pointers we
 * encounter at save / template-copy time. Walks g_amiga_addr_map once
 * to find the actual min/max symbol addresses, then accepts anything
 * inside that range with a small (256 KB) padding for .rodata strings
 * that aren't in the LUT directly but live next to symbols that are.
 *
 * Earlier this used a fixed ±16 MB window around g_amiga_addr_map[0].
 * On Linux at base 0x56600000 that worked — small-integer 4-byte
 * slices (struct score data, grid bytes, etc.) sat 1.4 GB away from
 * the probe, so diff > 16 MB and they were correctly rejected. On
 * MinGW Windows at base ~0x01000000 those same slices fell INSIDE the
 * ±16 MB window and were treated as pointers, so the
 * restore_comp_copy_pointers_from_template byte-stride scan stomped
 * cupWinnersCup.unk1 (the team-grid bytes) with template defaults
 * — corrupting season-advance.
 *
 * The actual binary footprint is at most ~3 MB; bounding by the
 * actual address-map extents keeps Linux behaviour and fixes Windows. */
static uint32_t s_addr_lo, s_addr_hi;
static int s_addr_bounds_inited;

static void looks_readable_init(void)
{
    if (s_addr_bounds_inited) return;
    if (g_amiga_addr_map_count == 0) { s_addr_bounds_inited = 1; return; }
    uint32_t lo = (uint32_t)(uintptr_t)g_amiga_addr_map[0].current_addr;
    uint32_t hi = lo;
    for (size_t i = 1; i < g_amiga_addr_map_count; i++) {
        uint32_t a = (uint32_t)(uintptr_t)g_amiga_addr_map[i].current_addr;
        if (a < lo) lo = a;
        if (a > hi) hi = a;
    }
    s_addr_lo = lo;
    s_addr_hi = hi;
    s_addr_bounds_inited = 1;
}

static int looks_readable(uint32_t val)
{
    if (val == 0u || val == 0xFFFFFFFFu) return 0;
    looks_readable_init();
    if (s_addr_lo == 0u && s_addr_hi == 0u) return 0;
    const uint32_t pad = 0x40000u;  /* 256 KB */
    uint32_t lo = (s_addr_lo > pad) ? (s_addr_lo - pad) : 0u;
    uint32_t hi = s_addr_hi + pad;
    return val >= lo && val <= hi;
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

/* Forward decls — looks_sentinel and emit_ptr_field are defined
 * after the normalize helpers below but used by them. */
static int  looks_sentinel(uint32_t val);
static int  emit_ptr_field(mjson_print_fn_t fn, void *fnd,
                           const char *key, uint32_t val);

/* At save time, replace a .rodata-style string pointer with its
 * string-pool equivalent in place. This makes the address
 * deterministic across save → load → save round-trips because both
 * save and load reset the pool to the same starting offset before
 * interning, so the same string content always resolves to the
 * same pool address. Without this, the saved pointer (e.g. &"DIV 1"
 * in .rodata) and the post-load pointer (string pool copy of
 * "DIV 1") would byte-differ.
 *
 * Leaves the pointer alone if:
 *   - val is 0
 *   - val resolves through g_amiga_addr_map (handled by `sym` form)
 *   - val is already inside our string pool
 *   - val isn't in a plausible in-process address range
 *   - the bytes at *val don't look like a NUL-terminated string
 */
static void normalize_ptr_for_save(uint32_t *p)
{
    uint32_t val = *p;
    if (val == 0u) return;
    if (sym_from_ptr(val, NULL)) return;
    if (looks_pool(val)) return;
    if (looks_sentinel(val) || !looks_readable(val)) return;

    const char *s = (const char *)(uintptr_t)val;
    size_t n = 0;
    while (n < 128u && s[n] != '\0') n++;
    if (n == 0 || n >= 128u) return;

    uint32_t pool_addr = string_pool_intern(s);
    if (pool_addr) *p = pool_addr;
}

/* Convenience wrapper: normalize storage in place, then emit. Used
 * by callers that have an addressable uint32_t pointer field. */
static int emit_ptr_field_ref(mjson_print_fn_t fn, void *fnd,
                              const char *key, uint32_t *p)
{
    normalize_ptr_for_save(p);
    return emit_ptr_field(fn, fnd, key, *p);
}


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
        emit_ptr_field_ref(fn, fnd, "league_str",
                           (uint32_t *)((uint8_t *)e + SAVE_SI_LEAGUESTRPTR));
        mjson_printf(fn, fnd, "%s", ",");
        emit_ptr_field_ref(fn, fnd, "str_1",
                           (uint32_t *)((uint8_t *)e + SAVE_SI_STRPTR1));
        mjson_printf(fn, fnd, "%s", ",");
        emit_ptr_field_ref(fn, fnd, "str_2",
                           (uint32_t *)((uint8_t *)e + SAVE_SI_STRPTR2));
        mjson_printf(fn, fnd, "%s", ",");
        emit_ptr_field_ref(fn, fnd, "str_3",
                           (uint32_t *)((uint8_t *)e + SAVE_SI_STRPTR3));

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

    /* Slice the array once, then walk each entry as its own
     * sub-object. Cuts ~400 full-file mjson_get_* scans down to
     * 20 array lookups + per-entry scans of ~500 bytes. */
    const char *arr = NULL;
    int arr_len = 0;
    if (mjson_find(buf, buflen, "$.season_informations",
                   &arr, &arr_len) != MJSON_TOK_ARRAY)
        return;

    /* Walk the array via mjson_next so each iteration is O(1) in
     * array position. Indexed "$[i]" finds re-walk from the start
     * each time → O(n²) over the whole array. */
    double num = 0.0;
    int pos = 0, koff, klen, voff, vlen, vt;
    uint32_t i = 0;
    while (i < SAVE_SI_ENTRY_COUNT &&
           (pos = mjson_next(arr, arr_len, pos,
                             &koff, &klen, &voff, &vlen, &vt)) != 0) {
        if (vt != MJSON_TOK_OBJECT) { i++; continue; }
        const char *e_slice = arr + voff;
        int e_slen = vlen;

        uint8_t *e = seasonInformationsArray + i * SAVE_SI_ENTRY_SIZE;
        i++;

        if (mjson_get_number(e_slice, e_slen, "$.club_no", &num) <= 0)
            continue;
        *(uint16_t *)(e + SAVE_SI_CLUBNO) = (uint16_t)num;

        load_string_field(e_slice, e_slen, "$.club_name",
                          e + SAVE_SI_CLUBNAME, 18);

        if (mjson_get_number(e_slice, e_slen, "$.trophy_flags", &num) > 0)
            *(uint16_t *)(e + SAVE_SI_TROPHYFLAGS) = (uint16_t)num;

        load_ptr_field(e_slice, e_slen, "$.league_str",
                       (uint32_t *)(e + SAVE_SI_LEAGUESTRPTR));
        load_ptr_field(e_slice, e_slen, "$.str_1",
                       (uint32_t *)(e + SAVE_SI_STRPTR1));
        load_ptr_field(e_slice, e_slen, "$.str_2",
                       (uint32_t *)(e + SAVE_SI_STRPTR2));
        load_ptr_field(e_slice, e_slen, "$.str_3",
                       (uint32_t *)(e + SAVE_SI_STRPTR3));

        if (mjson_get_number(e_slice, e_slen, "$.best_scorer_goals", &num) > 0)
            *(uint16_t *)(e + SAVE_SI_BESTSCORERNUMGOALS) = (uint16_t)num;

        load_string_field(e_slice, e_slen, "$.best_scorer_name",
                          e + SAVE_SI_BESTSCORERNAME, 23);
        load_string_field(e_slice, e_slen, "$.second_scorer_name",
                          e + SAVE_SI_2NDSCORERNAME, 23);

#define LOAD_BYTE(key, dst_off) do { \
            if (mjson_get_number(e_slice, e_slen, "$." key, &num) > 0) \
                e[dst_off] = (uint8_t)(int8_t)(int)num; \
        } while (0)
#define LOAD_U16(key, dst_off) do { \
            if (mjson_get_number(e_slice, e_slen, "$." key, &num) > 0) \
                *(uint16_t *)(e + dst_off) = (uint16_t)num; \
        } while (0)

        LOAD_BYTE("comp_1_result",      SAVE_SI_COMP1RESULT);
        LOAD_BYTE("comp_2_result",      SAVE_SI_COMP2RESULT);
        LOAD_BYTE("comp_3_result",      SAVE_SI_COMP3RESULT);
        LOAD_BYTE("comp_4_result",      SAVE_SI_COMP4RESULT);
        LOAD_U16 ("total_goals_scored", SAVE_SI_TOTALGOALSSCORED);
        LOAD_U16 ("total_goals_taken",  SAVE_SI_TOTALGOALSTAKEN);
        LOAD_BYTE("total_games",        SAVE_SI_TOTALGAMES);
        LOAD_BYTE("total_wins",         SAVE_SI_TOTALWINS);
        LOAD_BYTE("total_drawn",        SAVE_SI_TOTALDRAWN);
        LOAD_BYTE("total_lost",         SAVE_SI_TOTALLOST);
        LOAD_BYTE("friendly_wins",      SAVE_SI_FRIENDLYWINS);
        LOAD_BYTE("friendly_draws",     SAVE_SI_FRIENDLYDRAWS);
        LOAD_BYTE("friendly_lost",      SAVE_SI_FRIENDLYLOST);
        LOAD_BYTE("tour_wins",          SAVE_SI_TOURWINS);
        LOAD_BYTE("tour_draws",         SAVE_SI_TOURDRAWS);
        LOAD_BYTE("tour_lost",          SAVE_SI_TOURLOST);
        LOAD_BYTE("tournament_status",  SAVE_SI_TOURNAMENTSTATUS);
        LOAD_BYTE("manager_status",     SAVE_SI_MANAGERSTATUS);
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
#define PTR_AT(off) ((uint32_t *)((&careerFileBuffer) + (off)))
    emit_ptr_field_ref(fn, fnd, "current_competition_ptr",       PTR_AT(CFB_CURRENT_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "secondary_competition_ptr",     PTR_AT(CFB_SECONDARY_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "tertiary_competition_ptr",      PTR_AT(CFB_TERTIARY_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "quaternary_competition_ptr",    PTR_AT(CFB_QUATERNARY_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "diy_competition_ptr",           PTR_AT(CFB_DIY_COMP_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "diy_competition_table_ptr",     PTR_AT(CFB_DIY_COMP_TABLE_PTR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "primary_comp_string_offset",    PTR_AT(CFB_PRIMARY_COMP_STR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "secondary_comp_string_offset",  PTR_AT(CFB_SECONDARY_COMP_STR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "tertiary_comp_string_offset",   PTR_AT(CFB_TERTIARY_COMP_STR_OFF));
    mjson_printf(fn, fnd, "%s", ",");
    emit_ptr_field_ref(fn, fnd, "quaternary_comp_string_offset", PTR_AT(CFB_QUATERNARY_COMP_STR_OFF));
#undef PTR_AT

    /* careerCompetitionSlotPtrTable[5] */
    mjson_printf(fn, fnd, ",%Q:[", "career_competition_slot_ptr_table");
    for (uint32_t i = 0; i < 5; i++) {
        if (i > 0) mjson_printf(fn, fnd, "%s", ",");
        /* emit_ptr_field requires a key — for array elements we
         * inline the same logic minus the key prefix. */
        uint32_t *pp = (uint32_t *)((&careerFileBuffer) + CFB_CAREER_COMP_SLOT_TBL_OFF + i * 4);
        normalize_ptr_for_save(pp);
        uint32_t val = *pp;
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
        uint32_t *pp = (uint32_t *)((&careerFileBuffer) + CFB_CAREER_CUP_PICK_LIST_OFF + i * 4);
        normalize_ptr_for_save(pp);
        uint32_t val = *pp;
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
            /* Zero post-NUL bytes so the save reflects what the
             * loader will round-trip back. Without this, garbage
             * left over from prior buffer use shows up as
             * verify-time mismatches even though it has no game
             * semantics. */
            for (uint32_t i = n; i < f->size; i++)
                ((uint8_t *)p)[i] = 0;
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
    /* Resolve the section's object slice ONCE — subsequent
     * mjson_get_* calls then scan ~5 KB instead of the entire
     * .CAR JSON (hundreds of KB). For a 200-entry table this is
     * roughly a 100× speedup. */
    char section_path[80];
    snprintf(section_path, sizeof(section_path), "$.%s", section_key);
    const char *section = NULL;
    int section_len = 0;
    if (mjson_find(buf, buflen, section_path,
                   &section, &section_len) != MJSON_TOK_OBJECT)
        return;

    char path[80];
    char tmp[64];
    double num = 0.0;

    for (const FieldDesc *f = table; f->type != FLD_END; f++) {
        snprintf(path, sizeof(path), "$.%s", f->name);
        uint8_t *p = (uint8_t *)f->addr;
        switch (f->type) {
        case FLD_U8: case FLD_S8:
            if (mjson_get_number(section, section_len, path, &num) > 0)
                *p = (uint8_t)(int)num;
            break;
        case FLD_U16: case FLD_S16:
            if (mjson_get_number(section, section_len, path, &num) > 0)
                *(uint16_t *)p = (uint16_t)(int)num;
            break;
        case FLD_U32: case FLD_S32:
            if (mjson_get_number(section, section_len, path, &num) > 0)
                *(uint32_t *)p = (uint32_t)(int32_t)num;
            break;
        case FLD_STR:
            memset(p, 0, f->size);
            if (mjson_get_string(section, section_len, path, tmp, sizeof(tmp)) > 0) {
                uint32_t n = (uint32_t)strlen(tmp);
                /* Fields can fill all `f->size` bytes — see comment
                 * in load_str_at. Don't reserve a NUL slot. */
                if (n > f->size) n = f->size;
                memcpy(p, tmp, n);
            }
            break;
        case FLD_BYTES: {
            /* Iterate the JSON array via mjson_next when available. */
#if MJSON_ENABLE_NEXT
            const char *arr = NULL;
            int arr_len = 0;
            if (mjson_find(section, section_len, path,
                           &arr, &arr_len) == MJSON_TOK_ARRAY) {
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
    uint8_t *p = (uint8_t *)base + off;
    size_t n = 0;
    while (n < maxlen && p[n] != '\0') n++;
    /* Zero post-NUL bytes — see comment in emit_scalar_table.
     * Same rationale: lose nothing semantic, gain byte-exact
     * verify round-trip. */
    for (size_t i = n; i < maxlen; i++) p[i] = 0;
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
        /* Fields can fill all `maxlen` bytes with no NUL terminator
         * (e.g. a 22-char playerName that maxes out the slot). Copy
         * up to maxlen bytes — don't reserve a NUL slot. */
        if (n > maxlen) n = maxlen;
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

/* Resolve a path against `buf` and run `body` with `slice`/`slice_len`
 * set to the matched sub-object. Skips body if not found / not an
 * object. Used by the optimised loaders to bound mjson_get_* scans
 * to a small sub-buffer instead of the whole file. */
#define WITH_OBJECT_SLICE(buf, buflen, path_str, slice_var, slicelen_var, body) \
    do { \
        const char *slice_var = NULL; int slicelen_var = 0; \
        if (mjson_find((buf), (buflen), (path_str), \
                       &slice_var, &slicelen_var) == MJSON_TOK_OBJECT) { \
            body \
        } \
    } while (0)

/* Old signature kept for callers that prefer to pass a path against
 * the full file; internally it slices then dispatches to the fast
 * path. */
static void load_team_file_ex_slice(const char *buf, int buflen,
                                    uint8_t *base);

static void load_team_file_ex(const char *buf, int buflen,
                              const char *path_prefix, uint8_t *base)
{
    WITH_OBJECT_SLICE(buf, buflen, path_prefix, slice, slice_len, {
        load_team_file_ex_slice(slice, slice_len, base);
    });
}

/* Fast variant: `buf`/`buflen` is already the TeamFileEx sub-object's
 * JSON slice. mjson_get_* calls below scan ~3 KB instead of the full
 * .CAR file — typically 100× faster for the selected_teams loop. */
static void load_team_file_ex_slice(const char *buf, int buflen,
                                    uint8_t *base)
{
    uint8_t *team = base + offsetof(TeamFileEx, team);
    char path[64];

#define LOAD_U8(field, jsonkey) do { \
        load_u8_at(buf, buflen, "$.team." jsonkey, \
                   team, offsetof(TeamFile, field)); \
    } while (0)
#define LOAD_S16(field, jsonkey) do { \
        load_s16_at(buf, buflen, "$.team." jsonkey, \
                    team, offsetof(TeamFile, field)); \
    } while (0)
#define LOAD_STR(field, jsonkey, maxlen) do { \
        load_str_at(buf, buflen, "$.team." jsonkey, \
                    team, offsetof(TeamFile, field), maxlen); \
    } while (0)
#define LOAD_BYTES(field, jsonkey, len) do { \
        load_bytes_at(buf, buflen, "$.team." jsonkey, \
                      team, offsetof(TeamFile, field), len); \
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

    /* Per-player: iterate the players array via mjson_next so each
     * step is O(1) in array position; field lookups within each
     * player run against its ~250-byte slice instead of ~3 KB. */
    const char *players_arr = NULL; int players_arr_len = 0;
    if (mjson_find(buf, buflen, "$.players",
                   &players_arr, &players_arr_len) != MJSON_TOK_ARRAY)
        return;

    int pos = 0, koff, klen, voff, vlen, vt;
    int i = 0;
    while (i < 16 &&
           (pos = mjson_next(players_arr, players_arr_len, pos,
                             &koff, &klen, &voff, &vlen, &vt)) != 0) {
        if (vt != MJSON_TOK_OBJECT) { i++; continue; }
        uint8_t *p = base + offsetof(TeamFileEx, players)
                          + (size_t)i * sizeof(PlayerFile);
        const char *ps = players_arr + voff;
        int psl = vlen;
        i++;

#define LOAD_PU8(field, jsonkey) \
        load_u8_at(ps, psl, "$." jsonkey, p, offsetof(PlayerFile, field))
#define LOAD_PSTR(field, jsonkey, maxlen) \
        load_str_at(ps, psl, "$." jsonkey, p, offsetof(PlayerFile, field), maxlen)
#define LOAD_PBYTES(field, jsonkey, len) \
        load_bytes_at(ps, psl, "$." jsonkey, p, offsetof(PlayerFile, field), len)

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
    /* Slice the blobs object once so each per-blob base64 decode
     * scans only the blobs section instead of the entire file. */
    const char *bs = NULL; int bsl = 0;
    if (mjson_find(buf, buflen, "$.blobs", &bs, &bsl) != MJSON_TOK_OBJECT)
        return;

    char path[80];
    for (const BlobDesc *b = s_career_blobs; b->name; b++) {
        snprintf(path, sizeof(path), "$.%s", b->name);
        mjson_get_base64(bs, bsl, path,
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

/* ---------- *Copy struct text[2] pointers ----------
 *
 * Each *Copy competition struct (euroCupCopy, cupWinnersCupCopy, …
 * 34 of them in careerContests) has a `text[2]` pointer pair living
 * at offset 5 + src[5] within the struct. text[0] / text[1] point
 * into .rodata (`aEuropeanChampi_1`, `aEuropeanCup`, etc.) and are
 * baked in at link time when the static template (e.g. `euroCup`) is
 * initialised. The runtime *Copy is created by memcpy-ing the
 * template; nothing ever modifies text[0]/text[1] after that.
 *
 * So there's nothing to save: the bytes are pure template state.
 * On load we just copy text[0..1] from each src template into its
 * dest *Copy, overwriting whatever stale pointer bytes the raw-byte
 * blob load left behind. The struct's own byte-5 field tells us the
 * exact offset — no scan, no heuristic, no JSON encoding needed.
 *
 * Replaces the previous cup_record_name_sources (single-pointer per
 * *Copy, encoded as JSON) and comp_copy_pointer_fixups (heuristic
 * byte-scan) — both unnecessary once we recognise the values are
 * template-static. */
static void restore_comp_copy_text_ptrs_from_template(void)
{
    uint8_t *tbl = careerContests;
    for (;;) {
        uint32_t srcAddr = *(uint32_t *)tbl;
        if (srcAddr == 0xFFFFFFFFu)
            break;
        const uint8_t *src  = (const uint8_t *)(uintptr_t)srcAddr;
        uint8_t       *dest = *(uint8_t **)(tbl + 4);
        int8_t  nameOffset = (int8_t)src[5];
        if (nameOffset != 0) {
            /* text[0] + text[1] = 8 bytes from src template to dest
             * *Copy at the struct's own text-array offset. */
            memcpy(dest + 5 + nameOffset, src + 5 + nameOffset, 8);
        }
        tbl += 10;
    }
}

/* ---------- .CAR writer ---------- */

int16_t SaveJson_WriteCareer(uint8_t *filename)
{
    uint32_t data_size = career_data_size();
    JsonOutBuf out = { NULL, 0, 0, 0 };

    /* Reset string pool — normalize_ptr_for_save will intern
     * .rodata strings into it during the emit pass. Resetting up
     * front (mirroring the load side) means string-pool pointer
     * fields land at the same pool offsets every save → byte-
     * stable round-trip with the loader. */
    string_pool_reset();

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
    /* *Copy struct text[2] pointers are template-static — see
     * restore_comp_copy_text_ptrs_from_template — so they're NOT
     * encoded in the save. They get restored from each careerContests
     * src template on load. */
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

#ifdef SWOS_SAVE_FORMAT_JSON_VERIFY
    verify_roundtrip_region("career",
                            (uint8_t *)&careerFileBuffer, data_size,
                            verify_reload_career, filename);
#endif

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
    /* Reset the string pool up front so any { str: "..." } pointer
     * fields decoded during this load start interning at pool[0].
     * Without this, repeated loads (e.g. the verify pass) intern at
     * different pool offsets and pointer fields show up as
     * byte-level mismatches. */
    string_pool_reset();

    load_named_blobs(src, src_size);
    load_season_informations(src, src_size);
    load_career_pointers(src, src_size);
    /* *Copy struct text[2] pointers are template-static (see comment
     * above restore_comp_copy_text_ptrs_from_template), so they're
     * not in the save. Apply them from each careerContests src
     * template after the blob load has put stale bytes into dest. */
    restore_comp_copy_text_ptrs_from_template();
    load_scalar_table(src, src_size, "career_scalars", s_career_scalars);

    load_team_file_ex(src, src_size, "$.career_team",  &careerTeam);
    load_team_file_ex(src, src_size, "$.current_team", &currentTeam);

    /* numSelectedTeams comes through career_scalars
     * (num_selected_teams_inner aliases &numSelectedTeams). Now use
     * it to size the selected_teams loop. Slice the array once so
     * per-team lookups scan a single team's ~3 KB slice instead of
     * the full file each time. */
    const char *teams_arr = NULL;
    int teams_arr_len = 0;
    if (mjson_find(src, src_size, "$.selected_teams",
                   &teams_arr, &teams_arr_len) == MJSON_TOK_ARRAY) {
        /* Iterate via mjson_next instead of "$[i]" indexed finds
         * — same speedup pattern as load_scene. */
        int pos = 0, koff, klen, voff, vlen, vt;
        uint16_t i = 0;
        while (i < numSelectedTeams &&
               (pos = mjson_next(teams_arr, teams_arr_len, pos,
                                 &koff, &klen, &voff, &vlen, &vt)) != 0) {
            if (vt != MJSON_TOK_OBJECT) { i++; continue; }
            load_team_file_ex_slice(teams_arr + voff, vlen,
                                    (&selTeamsPtr) + (uint32_t)i * 684u);
            i++;
        }
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
    /* careerFileBuffer is declared `extern uint8_t careerFileBuffer;`
     * (a single-byte stub aliased onto careerFileBufferBuf), so
     * passing &careerFileBuffer to memset/mjson_get_base64 makes
     * fortify_source on Linux glibc see a 1-byte destination and
     * __chk_fail on the `want > 1` write. Use the real 256 KB backing
     * buffer directly — same memory, but the compiler sees it as
     * properly sized so the fortify check passes. */
    extern uint8_t careerFileBufferBuf[];
    int legacy_got = mjson_get_base64((const char *)src, (int)src_size,
                                      "$.opaque_data",
                                      (char *)careerFileBufferBuf, (int)want);
    if (legacy_got <= 0) {
        memset(careerFileBufferBuf, 0, want);
    }

    apply_semantic_load((const char *)src, (int)src_size);

    free(src);
    return 0;
}

/* ---------- .SEA / .PRE / .DIY (preset / DIY competition) ----------
 *
 * Cover the bytes [&competitionFileBuffer, &numSelectedTeams + 2)
 * plus `numDiyTeams * 684` trailing TeamFileEx records at
 * &selTeamsPtr. Re-uses the careerFileBuffer scalar/blob tables
 * with an address range filter so we only emit entries that fall
 * inside the preset save region.
 */

extern uint8_t  competitionFileBuffer;
extern uint32_t primaryCompStringOffset;

static int field_in_range(const FieldDesc *f,
                          const uint8_t *lo, const uint8_t *hi)
{
    const uint8_t *p = (const uint8_t *)f->addr;
    return p >= lo && p < hi;
}

static int blob_in_range(const BlobDesc *b,
                         const uint8_t *lo, const uint8_t *hi)
{
    const uint8_t *p = (const uint8_t *)b->addr;
    return p >= lo && (p + b->size) <= hi;
}

static void emit_scalar_table_range(mjson_print_fn_t fn, void *fnd,
                                    const char *section_key,
                                    const FieldDesc *table,
                                    const uint8_t *lo, const uint8_t *hi)
{
    mjson_printf(fn, fnd, "%Q:{", section_key);
    int first = 1;
    for (const FieldDesc *f = table; f->type != FLD_END; f++) {
        if (!field_in_range(f, lo, hi)) continue;
        if (!first) mjson_printf(fn, fnd, "%s", ",");
        first = 0;
        const uint8_t *p = (const uint8_t *)f->addr;
        switch (f->type) {
        case FLD_U8:
            mjson_printf(fn, fnd, "%Q:%u", f->name, (unsigned)*p); break;
        case FLD_S8:
            mjson_printf(fn, fnd, "%Q:%d", f->name, (int)*(int8_t *)p); break;
        case FLD_U16:
            mjson_printf(fn, fnd, "%Q:%u", f->name, (unsigned)*(uint16_t *)p); break;
        case FLD_S16:
            mjson_printf(fn, fnd, "%Q:%d", f->name, (int)*(int16_t *)p); break;
        case FLD_U32:
            mjson_printf(fn, fnd, "%Q:%lu", f->name, (long)(uint32_t)*(uint32_t *)p); break;
        case FLD_S32:
            mjson_printf(fn, fnd, "%Q:%ld", f->name, (long)*(int32_t *)p); break;
        case FLD_STR: {
            uint32_t n = 0;
            while (n < f->size && p[n] != '\0') n++;
            /* Zero post-NUL bytes so the save reflects what the
             * loader will round-trip back. Without this, garbage
             * left over from prior buffer use shows up as
             * verify-time mismatches even though it has no game
             * semantics. */
            for (uint32_t i = n; i < f->size; i++)
                ((uint8_t *)p)[i] = 0;
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

static void emit_named_blobs_range(mjson_print_fn_t fn, void *fnd,
                                   const uint8_t *lo, const uint8_t *hi)
{
    mjson_printf(fn, fnd, "%Q:{", "blobs");
    int first = 1;
    for (const BlobDesc *b = s_career_blobs; b->name; b++) {
        if (!blob_in_range(b, lo, hi)) continue;
        if (!first) mjson_printf(fn, fnd, "%s", ",");
        first = 0;
        mjson_printf(fn, fnd, "%Q:%V", b->name,
                     (int)b->size, (const char *)b->addr);
    }
    mjson_printf(fn, fnd, "%s", "}");
}

static void load_scalar_table_range(const char *buf, int buflen,
                                    const char *section_key,
                                    const FieldDesc *table,
                                    const uint8_t *lo, const uint8_t *hi)
{
    /* Slice the section once — same speedup pattern as
     * load_scalar_table. */
    char section_path[80];
    snprintf(section_path, sizeof(section_path), "$.%s", section_key);
    const char *section = NULL; int section_len = 0;
    if (mjson_find(buf, buflen, section_path,
                   &section, &section_len) != MJSON_TOK_OBJECT)
        return;

    char path[80];
    char tmp[64];
    double num = 0.0;
    for (const FieldDesc *f = table; f->type != FLD_END; f++) {
        if (!field_in_range(f, lo, hi)) continue;
        snprintf(path, sizeof(path), "$.%s", f->name);
        uint8_t *p = (uint8_t *)f->addr;
        switch (f->type) {
        case FLD_U8: case FLD_S8:
            if (mjson_get_number(section, section_len, path, &num) > 0) *p = (uint8_t)(int)num;
            break;
        case FLD_U16: case FLD_S16:
            if (mjson_get_number(section, section_len, path, &num) > 0) *(uint16_t *)p = (uint16_t)(int)num;
            break;
        case FLD_U32: case FLD_S32:
            if (mjson_get_number(section, section_len, path, &num) > 0) *(uint32_t *)p = (uint32_t)(int32_t)num;
            break;
        case FLD_STR:
            memset(p, 0, f->size);
            if (mjson_get_string(section, section_len, path, tmp, sizeof(tmp)) > 0) {
                uint32_t n = (uint32_t)strlen(tmp);
                /* Fields can fill all `f->size` bytes — see comment
                 * in load_str_at. Don't reserve a NUL slot. */
                if (n > f->size) n = f->size;
                memcpy(p, tmp, n);
            }
            break;
        case FLD_BYTES:
            load_bytes_at(section, section_len, path, p, 0, f->size);
            break;
        default:
            break;
        }
    }
}

static void load_named_blobs_range(const char *buf, int buflen,
                                   const uint8_t *lo, const uint8_t *hi)
{
    /* Slice the blobs object once. */
    const char *bs = NULL; int bsl = 0;
    if (mjson_find(buf, buflen, "$.blobs", &bs, &bsl) != MJSON_TOK_OBJECT)
        return;

    char path[80];
    for (const BlobDesc *b = s_career_blobs; b->name; b++) {
        if (!blob_in_range(b, lo, hi)) continue;
        snprintf(path, sizeof(path), "$.%s", b->name);
        mjson_get_base64(bs, bsl, path, (char *)b->addr, (int)b->size);
    }
}

/* Shared write body for preset / DIY — they share competitionFileBuffer
 * layout, only the file_kind string differs. */
static int16_t write_competition_buf(uint8_t *filename, const char *kind)
{
    const uint8_t *lo = &competitionFileBuffer;
    const uint8_t *hi = (const uint8_t *)&numSelectedTeams +
                        sizeof(numSelectedTeams);  /* +2 for the count word */
    JsonOutBuf out = { NULL, 0, 0, 0 };

    /* Pool reset — see comment in SaveJson_WriteCareer. */
    string_pool_reset();

    mjson_printf(json_out_emit, &out,
        "{\n  %Q:%d,\n  %Q:%Q,\n  %Q:%u,\n  ",
        "schema_version", SAVE_JSON_SCHEMA_VERSION,
        "file_kind",      kind,
        "num_diy_teams",  (unsigned)numDiyTeams);

    /* primary_comp_string_offset is the only career pointer that
     * lives inside the preset range. Emit it independently. */
    emit_ptr_field_ref(json_out_emit, &out, "primary_comp_string_offset",
                       &primaryCompStringOffset);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");

    emit_scalar_table_range(json_out_emit, &out,
                            "comp_scalars", s_career_scalars, lo, hi);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_named_blobs_range(json_out_emit, &out, lo, hi);

    /* DIY teams array — numDiyTeams TeamFileEx records at
     * &selTeamsPtr. Same record layout as the career selected_teams. */
    mjson_printf(json_out_emit, &out, ",\n  %Q:[", "diy_teams");
    for (uint16_t i = 0; i < numDiyTeams; i++) {
        if (i > 0) mjson_printf(json_out_emit, &out, "%s", ",");
        emit_team_file_ex_body(json_out_emit, &out,
                               (&selTeamsPtr) + (uint32_t)i * 684u);
    }
    mjson_printf(json_out_emit, &out, "%s", "]\n}\n");

    if (out.oom || !out.ptr) {
        if (out.ptr) free(out.ptr);
        return 1;
    }
    int16_t r = _WriteFile(filename, (uint8_t *)out.ptr, (uint32_t)out.len);
    free(out.ptr);
    if (r != 0) {
        _ShowFsErrorByCode((uint16_t)r);
        return 1;
    }

#ifdef SWOS_SAVE_FORMAT_JSON_VERIFY
    {
        const uint8_t *lo = &competitionFileBuffer;
        const uint8_t *hi = (const uint8_t *)&numSelectedTeams +
                            sizeof(numSelectedTeams) +
                            (uint32_t)numDiyTeams * 684u;
        VerifyReloadFn fn = (strcmp(kind, KIND_DIY) == 0)
                              ? verify_reload_diy
                              : verify_reload_preset;
        verify_roundtrip_region(kind, (uint8_t *)lo,
                                (uint32_t)(hi - lo), fn, filename);
    }
#endif

    return 0;
}

static int16_t load_competition_buf(uint8_t *filename, const char *kind)
{
    uint8_t *src = NULL;
    uint32_t src_size = 0;
    if (read_save_file_heap(filename, &src, &src_size) != 0)
        return 1;

    double version = 0.0;
    if (mjson_get_number((const char *)src, (int)src_size,
                         "$.schema_version", &version) <= 0 ||
        (int)version != SAVE_JSON_SCHEMA_VERSION) {
        free(src);
        return 1;
    }
    char file_kind[16] = {0};
    if (mjson_get_string((const char *)src, (int)src_size,
                         "$.file_kind", file_kind, sizeof(file_kind)) <= 0 ||
        strcmp(file_kind, kind) != 0) {
        free(src);
        return 1;
    }

    const uint8_t *lo = &competitionFileBuffer;
    const uint8_t *hi = (const uint8_t *)&numSelectedTeams +
                        sizeof(numSelectedTeams);
    uint32_t want = (uint32_t)(hi - lo);
    memset((void *)lo, 0, want);

    /* Reset string pool — see comment in apply_semantic_load. */
    string_pool_reset();

    load_named_blobs_range((const char *)src, (int)src_size, lo, hi);

    /* primary_comp_string_offset */
    uint32_t pval = 0;
    load_ptr_field((const char *)src, (int)src_size,
                   "$.primary_comp_string_offset", &pval);
    primaryCompStringOffset = pval;

    load_scalar_table_range((const char *)src, (int)src_size,
                            "comp_scalars", s_career_scalars, lo, hi);

    /* DIY teams — walk array via mjson_next so each iteration is
     * O(1) in array position. */
    const char *teams_arr = NULL;
    int teams_arr_len = 0;
    if (mjson_find((const char *)src, (int)src_size, "$.diy_teams",
                   &teams_arr, &teams_arr_len) == MJSON_TOK_ARRAY) {
        int pos = 0, koff, klen, voff, vlen, vt;
        uint16_t i = 0;
        while (i < numDiyTeams &&
               (pos = mjson_next(teams_arr, teams_arr_len, pos,
                                 &koff, &klen, &voff, &vlen, &vt)) != 0) {
            if (vt != MJSON_TOK_OBJECT) { i++; continue; }
            load_team_file_ex_slice(teams_arr + voff, vlen,
                                    (&selTeamsPtr) + (uint32_t)i * 684u);
            i++;
        }
    }

    free(src);
    return 0;
}

int16_t SaveJson_WritePreset(uint8_t *filename)
{
    return write_competition_buf(filename, KIND_PRESET);
}

int16_t SaveJson_LoadPreset(uint8_t *filename, const uint8_t *src_unused,
                            uint32_t src_size_unused)
{
    (void)src_unused; (void)src_size_unused;
    return load_competition_buf(filename, KIND_PRESET);
}

int16_t SaveJson_WriteDIY(uint8_t *filename)
{
    return write_competition_buf(filename, KIND_DIY);
}

int16_t SaveJson_LoadDIY(uint8_t *filename, const uint8_t *src_unused,
                         uint32_t src_size_unused)
{
    (void)src_unused; (void)src_size_unused;
    return load_competition_buf(filename, KIND_DIY);
}

/* ---------- .HIL (highlights / match replay) ----------
 *
 * Fully semantic encoding (no opaque blobs). On-disk layout:
 *   +0      sig          u32 BE
 *   +4      max_size     u32 BE
 *   +8      topTeamInGame    1018-byte TeamGame (BE int16 header)
 *   +1026   bottomTeamInGame 1018-byte TeamGame
 *   +2044   gameName[100]
 *   +2144   gameRound[100]
 *   +2244   numGoals          u16 (BE on disk, host-native at our
 *                                  dispatch point — _DoSaveHighlights
 *                                  hasn't applied its swap yet)
 *   +2246   team1Goals        u16
 *   +2248   team2Goals        u16
 *   +2250   pitchNumber       u16
 *   +2252   maxSubstitutes    u16
 *   +2254   scenes[numGoals]  each scene = 19000-byte ring of frames
 *
 * Scene format (per _SaveCoordinatesForHighlights / _DrawReplayFrame):
 *   - Sequence of u32-aligned entries, BE on disk.
 *   - bit 31 set = frame marker:
 *       [marker u32 = 0x80000000 | (cam_x << 16) | cam_y]
 *       [u32 = (team1_goals << 16) | team2_goals]
 *       [u32 = (anim_state << 16) | anim_state]  // duplicated
 *   - bit 31 clear = sprite entry:
 *       [u32 = (sprite_index << 20) | (x << 10) | y]
 *   - 0xFFFFFFFF = end-of-segment sentinel; anything after up to
 *     19000 bytes is garbage from the ring buffer (preserved here
 *     as a "trailing" byte array so round-trip is bit-exact).
 */

extern uint8_t  hilFileBuffer[];
extern uint16_t hilNumGoals;
extern uint8_t  hilGameName[];
extern uint8_t  hilGameRound[];

/* On-disk .HIL header — 2254 bytes total. Matches the byte layout
 * the legacy save/load and _SetHilFileHeader / _SaveCoordinatesForHighlights
 * write at the start of hilFileBuffer. Using offsetof() against this
 * struct replaces every hardcoded byte offset that used to live in
 * this file. Note: signature/max_size/TeamGame kit fields are BE on
 * disk; numGoals + neighbours are host-native at our save dispatch
 * point (the legacy BE swap dance hasn't run yet). */
typedef struct __attribute__((packed)) HilFileHeader {
    uint32_t  signature;          /* +0   BE on disk */
    uint32_t  max_size;           /* +4   BE on disk */
    TeamGame  top_team;           /* +8   1018 bytes (BE int16 header) */
    TeamGame  bottom_team;        /* +1026 1018 bytes */
    uint8_t   game_name[100];     /* +2044 */
    uint8_t   game_round[100];    /* +2144 */
    uint16_t  num_goals;          /* +2244 */
    uint16_t  team_1_goals;       /* +2246 */
    uint16_t  team_2_goals;       /* +2248 */
    uint16_t  pitch_number;       /* +2250 */
    uint16_t  max_substitutes;    /* +2252 */
} HilFileHeader;

typedef char _hilheader_size_check[(sizeof(HilFileHeader) == 2254) ? 1 : -1];

#define HIL_HEADER_SIZE  ((uint32_t)sizeof(HilFileHeader))
#define HIL_SCENE_SIZE   19000u

/* ---------- TeamGame emit/load ----------
 *
 * In hilFileBuffer (memory and on disk), the 11 × int16 fields are
 * BE-encoded by _SetHilFileHeader. We read via BE_READ_U16 to get
 * the live host-native values, emit as JSON numbers, and on load
 * write back via HOST_TO_BE16.
 */
static void emit_team_game(mjson_print_fn_t fn, void *fnd,
                           const char *key, const uint8_t *tg)
{
    mjson_printf(fn, fnd, "%Q:{", key);

    mjson_printf(fn, fnd,
        "%Q:{"
        "%Q:%u,%Q:%u,%Q:%u,%Q:%u,%Q:%u,"
        "%Q:%u,%Q:%u,%Q:%u,%Q:%u,%Q:%u},",
        "kit",
        "pr_shirt_type",  (unsigned)BE_READ_U16(tg + offsetof(TeamGame, prShirtType)),
        "pr_shirt_col",   (unsigned)BE_READ_U16(tg + offsetof(TeamGame, prShirtCol)),
        "pr_stripes_col", (unsigned)BE_READ_U16(tg + offsetof(TeamGame, prStripesCol)),
        "pr_shorts_col",  (unsigned)BE_READ_U16(tg + offsetof(TeamGame, prShortsCol)),
        "pr_socks_col",   (unsigned)BE_READ_U16(tg + offsetof(TeamGame, prSocksCol)),
        "sec_shirt_type", (unsigned)BE_READ_U16(tg + offsetof(TeamGame, secShirtType)),
        "sec_shirt_col",  (unsigned)BE_READ_U16(tg + offsetof(TeamGame, secShirtCol)),
        "sec_stripes",    (unsigned)BE_READ_U16(tg + offsetof(TeamGame, secStripesCol)),
        "sec_shorts",     (unsigned)BE_READ_U16(tg + offsetof(TeamGame, secShortsCol)),
        "sec_socks",      (unsigned)BE_READ_U16(tg + offsetof(TeamGame, secSocksCol)));

    mjson_printf(fn, fnd, "%Q:%u,", "marked_player",
                 (unsigned)BE_READ_U16(tg + offsetof(TeamGame, markedPlayer)));
    emit_str_at(fn, fnd, "team_name", (uint8_t *)tg, offsetof(TeamGame, teamName), 17);
    mjson_printf(fn, fnd, ",%Q:%u,%Q:%u,%Q:%u",
                 "unk1",          (unsigned)tg[offsetof(TeamGame, unk1)],
                 "num_own_goals", (unsigned)tg[offsetof(TeamGame, numOwnGoals)],
                 "unk3",          (unsigned)tg[offsetof(TeamGame, unk3)]);

    mjson_printf(fn, fnd, ",%Q:[", "players");
    for (int i = 0; i < 16; i++) {
        if (i > 0) mjson_printf(fn, fnd, "%s", ",");
        const uint8_t *p = tg + offsetof(TeamGame, players)
                              + (size_t)i * sizeof(struct PlayerInfo);
        mjson_printf(fn, fnd,
            "{%Q:%d,%Q:%d,%Q:%d,%Q:%d,%Q:%d,%Q:%d,%Q:%d,"
            "%Q:%d,%Q:%d,%Q:%d,%Q:%d,%Q:%d,",
            "skip",          (int)(int8_t)p[offsetof(struct PlayerInfo, skip)],
            "player_index",  (int)(int8_t)p[offsetof(struct PlayerInfo, playerIndex)],
            "goals_scored",  (int)(int8_t)p[offsetof(struct PlayerInfo, goalsScored)],
            "shirt_number",  (int)(int8_t)p[offsetof(struct PlayerInfo, shirtNumber)],
            "position",      (int)(int8_t)p[offsetof(struct PlayerInfo, position)],
            "face",          (int)(int8_t)p[offsetof(struct PlayerInfo, face)],
            "is_injured",    (int)(int8_t)p[offsetof(struct PlayerInfo, isInjured)],
            "field_7",       (int)(int8_t)p[offsetof(struct PlayerInfo, field_7)],
            "field_8",       (int)(int8_t)p[offsetof(struct PlayerInfo, field_8)],
            "previous_cards",(int)(int8_t)p[offsetof(struct PlayerInfo, previousCards)],
            "num_yellow_cards",(int)(int8_t)p[offsetof(struct PlayerInfo, numYellowCards)],
            "field_b",       (int)(int8_t)p[offsetof(struct PlayerInfo, field_B)]);
        emit_str_at(fn, fnd, "short_name", (uint8_t *)p,
                    offsetof(struct PlayerInfo, shortName), 15);
        mjson_printf(fn, fnd,
            ",%Q:%d,%Q:%d,%Q:%d,%Q:%d,%Q:%d,%Q:%d,%Q:%d,"
            "%Q:%d,%Q:%d,%Q:%d,%Q:%d,",
            "passing",       (int)(int8_t)p[offsetof(struct PlayerInfo, passing)],
            "shooting",      (int)(int8_t)p[offsetof(struct PlayerInfo, shooting)],
            "heading",       (int)(int8_t)p[offsetof(struct PlayerInfo, heading)],
            "tackling",      (int)(int8_t)p[offsetof(struct PlayerInfo, tackling)],
            "ball_control",  (int)(int8_t)p[offsetof(struct PlayerInfo, ballControl)],
            "speed",         (int)(int8_t)p[offsetof(struct PlayerInfo, speed)],
            "finishing",     (int)(int8_t)p[offsetof(struct PlayerInfo, finishing)],
            "goalie_skill",  (int)(int8_t)p[offsetof(struct PlayerInfo, goalieSkill)],
            "injuries_bits", (int)(int8_t)p[offsetof(struct PlayerInfo, injuriesBitfield)],
            "half_played",   (int)(int8_t)p[offsetof(struct PlayerInfo, halfPlayed)],
            "face_2",        (int)(int8_t)p[offsetof(struct PlayerInfo, face2)]);
        emit_str_at(fn, fnd, "full_name", (uint8_t *)p,
                    offsetof(struct PlayerInfo, fullName), 23);
        mjson_printf(fn, fnd, "%s", "}");
    }
    mjson_printf(fn, fnd, "%s", "]}");
}

/* Sliced variant — caller has already located the TeamGame's JSON
 * object slice via mjson_find. Subsequent get_* lookups scan only
 * the ~10 KB slice instead of the entire .HIL JSON. */
static void load_team_game_slice(const char *buf, int buflen, uint8_t *tg)
{
    double num = 0.0;
    char path[40];

    /* Zero the buffer up front — fields not present in JSON stay
     * defined as zero. */
    memset(tg, 0, sizeof(TeamGame));

    /* Kit fields live under "$.kit" — slice the sub-object so the
     * 10 lookups don't scan the whole TeamGame slice. */
    const char *kit = NULL; int kit_len = 0;
    if (mjson_find(buf, buflen, "$.kit",
                   &kit, &kit_len) == MJSON_TOK_OBJECT) {
#define LOAD_BE16(key, field) do { \
            if (mjson_get_number(kit, kit_len, "$." key, &num) > 0) \
                BE_WRITE_U16(tg + offsetof(TeamGame, field), (uint16_t)(int)num); \
        } while (0)
        LOAD_BE16("pr_shirt_type",  prShirtType);
        LOAD_BE16("pr_shirt_col",   prShirtCol);
        LOAD_BE16("pr_stripes_col", prStripesCol);
        LOAD_BE16("pr_shorts_col",  prShortsCol);
        LOAD_BE16("pr_socks_col",   prSocksCol);
        LOAD_BE16("sec_shirt_type", secShirtType);
        LOAD_BE16("sec_shirt_col",  secShirtCol);
        LOAD_BE16("sec_stripes",    secStripesCol);
        LOAD_BE16("sec_shorts",     secShortsCol);
        LOAD_BE16("sec_socks",      secSocksCol);
#undef LOAD_BE16
    }

    if (mjson_get_number(buf, buflen, "$.marked_player", &num) > 0)
        BE_WRITE_U16(tg + offsetof(TeamGame, markedPlayer), (uint16_t)(int)num);

    load_str_at(buf, buflen, "$.team_name", tg,
                offsetof(TeamGame, teamName), 17);

#define LOAD_B(key, field) do { \
        if (mjson_get_number(buf, buflen, "$." key, &num) > 0) \
            tg[offsetof(TeamGame, field)] = (uint8_t)(int)num; \
    } while (0)
    LOAD_B("unk1",          unk1);
    LOAD_B("num_own_goals", numOwnGoals);
    LOAD_B("unk3",          unk3);
#undef LOAD_B

    /* Walk the players array via mjson_next — same O(N) pattern
     * as elsewhere. */
    const char *players = NULL; int players_len = 0;
    if (mjson_find(buf, buflen, "$.players",
                   &players, &players_len) != MJSON_TOK_ARRAY)
        return;

    int pos = 0, koff, klen, voff, vlen, vt;
    int i = 0;
    while (i < 16 &&
           (pos = mjson_next(players, players_len, pos,
                             &koff, &klen, &voff, &vlen, &vt)) != 0) {
        if (vt != MJSON_TOK_OBJECT) { i++; continue; }
        uint8_t *p = tg + offsetof(TeamGame, players)
                        + (size_t)i * sizeof(struct PlayerInfo);
        const char *ps = players + voff;
        int psl = vlen;
        i++;

#define LOAD_PB(key, field) do { \
            if (mjson_get_number(ps, psl, "$." key, &num) > 0) \
                p[offsetof(struct PlayerInfo, field)] = (uint8_t)(int)(int8_t)(int)num; \
        } while (0)
        LOAD_PB("skip",            skip);
        LOAD_PB("player_index",    playerIndex);
        LOAD_PB("goals_scored",    goalsScored);
        LOAD_PB("shirt_number",    shirtNumber);
        LOAD_PB("position",        position);
        LOAD_PB("face",            face);
        LOAD_PB("is_injured",      isInjured);
        LOAD_PB("field_7",         field_7);
        LOAD_PB("field_8",         field_8);
        LOAD_PB("previous_cards",  previousCards);
        LOAD_PB("num_yellow_cards",numYellowCards);
        LOAD_PB("field_b",         field_B);
        load_str_at(ps, psl, "$.short_name", p,
                    offsetof(struct PlayerInfo, shortName), 15);
        LOAD_PB("passing",         passing);
        LOAD_PB("shooting",        shooting);
        LOAD_PB("heading",         heading);
        LOAD_PB("tackling",        tackling);
        LOAD_PB("ball_control",    ballControl);
        LOAD_PB("speed",           speed);
        LOAD_PB("finishing",       finishing);
        LOAD_PB("goalie_skill",    goalieSkill);
        LOAD_PB("injuries_bits",   injuriesBitfield);
        LOAD_PB("half_played",     halfPlayed);
        LOAD_PB("face_2",          face2);
        load_str_at(ps, psl, "$.full_name", p,
                    offsetof(struct PlayerInfo, fullName), 23);
#undef LOAD_PB
    }
}

/* Path-based wrapper — locates the slice then delegates. */
static void load_team_game(const char *buf, int buflen,
                           const char *path_prefix, uint8_t *tg)
{
    const char *slice = NULL; int slice_len = 0;
    if (mjson_find(buf, buflen, path_prefix,
                   &slice, &slice_len) == MJSON_TOK_OBJECT)
        load_team_game_slice(slice, slice_len, tg);
    else
        memset(tg, 0, sizeof(TeamGame));
}

/* ---------- scene emit/load ----------
 *
 * A scene is 19000 bytes of 4-byte BE entries. Parse into a list of
 * frames + sprites; emit any bytes after the end-of-segment sentinel
 * as a trailing byte array so we round-trip bit-exactly. */
static void emit_scene(mjson_print_fn_t fn, void *fnd,
                       const uint8_t *scene)
{
    mjson_printf(fn, fnd, "%s", "{\"frames\":[");

    uint32_t off = 0;
    int frame_open = 0;
    int frame_first_sprite = 1;
    int frames_emitted = 0;
    uint32_t eos_off = HIL_SCENE_SIZE;

    while (off + 4 <= HIL_SCENE_SIZE) {
        uint32_t entry = BE_READ_U32(scene + off);
        if (entry == 0xFFFFFFFFu) {
            eos_off = off + 4;
            break;
        }
        if (entry & 0x80000000u) {
            /* Close previous frame */
            if (frame_open) {
                mjson_printf(fn, fnd, "%s", "]}");
            }

            /* New frame: read 3 × u32. camera x|0x8000, camera y, goals u16+u16, anim u16+u16 (dup). */
            uint16_t cam_x = (entry >> 16) & 0x7FFFu;  /* drop bit 31 */
            uint16_t cam_y = entry & 0xFFFFu;
            uint32_t goals  = (off + 8  <= HIL_SCENE_SIZE) ? BE_READ_U32(scene + off + 4) : 0;
            uint32_t anim   = (off + 12 <= HIL_SCENE_SIZE) ? BE_READ_U32(scene + off + 8) : 0;
            uint16_t team1_goals = (uint16_t)(goals >> 16);
            uint16_t team2_goals = (uint16_t)(goals & 0xFFFF);
            uint16_t anim_state  = (uint16_t)(anim >> 16);  /* low half duplicates high */

            if (frames_emitted > 0) mjson_printf(fn, fnd, "%s", ",");
            mjson_printf(fn, fnd,
                "{%Q:%u,%Q:%u,%Q:%u,%Q:%u,%Q:%u,%Q:[",
                "cam_x", (unsigned)cam_x,
                "cam_y", (unsigned)cam_y,
                "team_1_goals", (unsigned)team1_goals,
                "team_2_goals", (unsigned)team2_goals,
                "anim_patterns_state", (unsigned)anim_state,
                "sprites");
            frame_open = 1;
            frame_first_sprite = 1;
            frames_emitted++;
            off += 12;
        } else {
            /* Sprite entry: bit 31 clear, encoded (idx << 20) | (x << 10) | y. */
            uint32_t idx = (entry >> 20) & 0xFFFu;
            uint32_t x   = (entry >> 10) & 0x3FFu;
            uint32_t y   = entry & 0x3FFu;
            if (!frame_open) {
                /* Sprite without a preceding frame — preserve as a
                 * leading-sprites array on a synthetic frame. Rare but
                 * possible if the scene starts mid-frame. */
                if (frames_emitted > 0) mjson_printf(fn, fnd, "%s", ",");
                mjson_printf(fn, fnd, "%s",
                    "{\"cam_x\":0,\"cam_y\":0,\"team_1_goals\":0,"
                    "\"team_2_goals\":0,\"anim_patterns_state\":0,"
                    "\"_synthetic_no_frame_marker\":true,\"sprites\":[");
                frame_open = 1;
                frame_first_sprite = 1;
                frames_emitted++;
            }
            if (!frame_first_sprite) mjson_printf(fn, fnd, "%s", ",");
            frame_first_sprite = 0;
            mjson_printf(fn, fnd, "{%Q:%u,%Q:%u,%Q:%u}",
                         "index", (unsigned)idx,
                         "x",     (unsigned)x,
                         "y",     (unsigned)y);
            off += 4;
        }
    }

    if (frame_open) {
        mjson_printf(fn, fnd, "%s", "]}");
    }
    mjson_printf(fn, fnd, "%s", "]");

    /* Trailing bytes: everything from eos_off..HIL_SCENE_SIZE (the
     * ring-buffer garbage past the active region). Emit as a byte
     * array for bit-exact round-trip. */
    mjson_printf(fn, fnd, ",%Q:%u,%Q:[",
                 "eos_offset", (unsigned)eos_off, "trailing");
    for (uint32_t i = eos_off; i < HIL_SCENE_SIZE; i++) {
        if (i > eos_off) mjson_printf(fn, fnd, "%s", ",");
        mjson_printf(fn, fnd, "%u", (unsigned)scene[i]);
    }
    mjson_printf(fn, fnd, "%s", "]}");
}

/* Forward decl — caller-provides slice variant. */
static void load_scene_slice(const char *scene_buf, int scene_buflen,
                             uint8_t *scene);

static void load_scene(const char *buf, int buflen,
                       const char *path_prefix, uint8_t *scene)
{
    /* Path wrapper — locate slice then delegate. */
    const char *scene_buf = NULL;
    int scene_buflen = 0;
    if (mjson_find(buf, buflen, path_prefix,
                   &scene_buf, &scene_buflen) == MJSON_TOK_OBJECT)
        load_scene_slice(scene_buf, scene_buflen, scene);
    else
        memset(scene, 0, HIL_SCENE_SIZE);
}

static void load_scene_slice(const char *scene_buf, int scene_buflen,
                             uint8_t *scene)
{
    memset(scene, 0, HIL_SCENE_SIZE);

    uint32_t off = 0;
    double num = 0.0;

#if MJSON_ENABLE_NEXT
    const char *frames_arr = NULL;
    int frames_arr_len = 0;
    if (mjson_find(scene_buf, scene_buflen, "$.frames",
                   &frames_arr, &frames_arr_len) == MJSON_TOK_ARRAY) {
        int koff, klen, voff, vlen, vt;
        int pos = 0;
        while ((pos = mjson_next(frames_arr, frames_arr_len, pos,
                                 &koff, &klen, &voff, &vlen, &vt)) != 0) {
            if (vt != MJSON_TOK_OBJECT) continue;

            /* mjson_next yields the frame's slice directly — use it
             * as the buffer for the field/sprite lookups. Cuts the
             * scene load from O(F²·S²) to O(F·S). */
            const char *fb = frames_arr + voff;
            int fl = vlen;

            uint16_t cam_x = 0, cam_y = 0, t1 = 0, t2 = 0, anim = 0;
            int is_synthetic = 0;

            if (mjson_get_number(fb, fl, "$.cam_x",          &num) > 0) cam_x = (uint16_t)num;
            if (mjson_get_number(fb, fl, "$.cam_y",          &num) > 0) cam_y = (uint16_t)num;
            if (mjson_get_number(fb, fl, "$.team_1_goals",   &num) > 0) t1   = (uint16_t)num;
            if (mjson_get_number(fb, fl, "$.team_2_goals",   &num) > 0) t2   = (uint16_t)num;
            if (mjson_get_number(fb, fl, "$.anim_patterns_state", &num) > 0) anim = (uint16_t)num;
            {
                int bval = 0;
                if (mjson_get_bool(fb, fl, "$._synthetic_no_frame_marker",
                                   &bval) > 0 && bval)
                    is_synthetic = 1;
            }

            if (!is_synthetic) {
                if (off + 12 > HIL_SCENE_SIZE) break;
                uint32_t marker = 0x80000000u
                                | ((uint32_t)cam_x << 16)
                                | (uint32_t)cam_y;
                BE_WRITE_U32(scene + off, marker);
                BE_WRITE_U32(scene + off + 4,
                             ((uint32_t)t1 << 16) | (uint32_t)t2);
                BE_WRITE_U32(scene + off + 8,
                             ((uint32_t)anim << 16) | (uint32_t)anim);
                off += 12;
            }

            /* Sprites — iterate the array via mjson_next instead of
             * indexed path lookups. Each sprite decode is then O(1)
             * in array position, not O(idx²). */
            const char *sprites_arr = NULL;
            int sprites_arr_len = 0;
            if (mjson_find(fb, fl, "$.sprites",
                           &sprites_arr, &sprites_arr_len) == MJSON_TOK_ARRAY) {
                int spos = 0;
                int s_koff, s_klen, s_voff, s_vlen, s_vt;
                while ((spos = mjson_next(sprites_arr, sprites_arr_len, spos,
                                          &s_koff, &s_klen, &s_voff, &s_vlen, &s_vt)) != 0) {
                    if (s_vt != MJSON_TOK_OBJECT) continue;
                    const char *sb = sprites_arr + s_voff;
                    int sl = s_vlen;

                    uint32_t s_idx = 0, s_x = 0, s_y = 0;
                    if (mjson_get_number(sb, sl, "$.index", &num) > 0)
                        s_idx = (uint32_t)num & 0xFFFu;
                    if (mjson_get_number(sb, sl, "$.x", &num) > 0)
                        s_x = (uint32_t)num & 0x3FFu;
                    if (mjson_get_number(sb, sl, "$.y", &num) > 0)
                        s_y = (uint32_t)num & 0x3FFu;
                    if (off + 4 > HIL_SCENE_SIZE) goto sprites_done;
                    uint32_t packed = (s_idx << 20) | (s_x << 10) | s_y;
                    BE_WRITE_U32(scene + off, packed);
                    off += 4;
                }
            sprites_done:;
            }
        }
    }
#else
    (void)buf; (void)buflen; (void)path_prefix;
#endif

    /* EOS sentinel */
    uint32_t eos_off = off;
    if (mjson_get_number(scene_buf, scene_buflen, "$.eos_offset", &num) > 0) {
        uint32_t hinted = (uint32_t)num;
        if (hinted >= 4 && hinted <= HIL_SCENE_SIZE)
            eos_off = hinted;
    }
    if (eos_off >= 4 && eos_off <= HIL_SCENE_SIZE) {
        BE_WRITE_U32(scene + eos_off - 4, 0xFFFFFFFFu);
    }

    /* Trailing bytes (post-EOS garbage) */
    load_bytes_at(scene_buf, scene_buflen, "$.trailing",
                  scene, eos_off, HIL_SCENE_SIZE - eos_off);
}

int16_t SaveJson_WriteHighlights(uint8_t *filename)
{
    PROF_START(total);
    uint16_t num_goals = hilNumGoals;
    uint32_t data_size = HIL_HEADER_SIZE + (uint32_t)num_goals * HIL_SCENE_SIZE;

    JsonOutBuf out = { NULL, 0, 0, 0 };
    string_pool_reset();

    PROF_START(header);
    mjson_printf(json_out_emit, &out,
        "{\n  %Q:%d,\n  %Q:%Q,\n  ",
        "schema_version", SAVE_JSON_SCHEMA_VERSION,
        "file_kind",      KIND_HIGHLIGHTS);

    mjson_printf(json_out_emit, &out, "%Q:%lu,\n  %Q:%lu,\n  ",
                 "signature",
                 (long)(uint32_t)BE_READ_U32((uint8_t *)hilFileBuffer
                                             + offsetof(HilFileHeader, signature)),
                 "max_size",
                 (long)(uint32_t)BE_READ_U32((uint8_t *)hilFileBuffer
                                             + offsetof(HilFileHeader, max_size)));

    HilFileHeader *hdr = (HilFileHeader *)hilFileBuffer;

    emit_team_game(json_out_emit, &out, "top_team",
                   (uint8_t *)&hdr->top_team);
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_team_game(json_out_emit, &out, "bottom_team",
                   (uint8_t *)&hdr->bottom_team);

    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_str_at(json_out_emit, &out, "game_name",
                (uint8_t *)hilFileBuffer,
                offsetof(HilFileHeader, game_name),
                sizeof(hdr->game_name));
    mjson_printf(json_out_emit, &out, "%s", ",\n  ");
    emit_str_at(json_out_emit, &out, "game_round",
                (uint8_t *)hilFileBuffer,
                offsetof(HilFileHeader, game_round),
                sizeof(hdr->game_round));

    mjson_printf(json_out_emit, &out,
        ",\n  %Q:%u,%Q:%u,%Q:%u,%Q:%u,%Q:%u",
        "num_goals",       (unsigned)num_goals,
        "team_1_goals",    (unsigned)hdr->team_1_goals,
        "team_2_goals",    (unsigned)hdr->team_2_goals,
        "pitch_number",    (unsigned)hdr->pitch_number,
        "max_substitutes", (unsigned)hdr->max_substitutes);
    PROF_END(header, "highlights-write");

    PROF_START(scenes);
    mjson_printf(json_out_emit, &out, ",\n  %Q:[", "scenes");
    for (uint16_t i = 0; i < num_goals; i++) {
        if (i > 0) mjson_printf(json_out_emit, &out, "%s", ",");
        emit_scene(json_out_emit, &out,
                   hilFileBuffer + HIL_HEADER_SIZE
                                 + (uint32_t)i * HIL_SCENE_SIZE);
    }
    mjson_printf(json_out_emit, &out, "%s", "]\n}\n");
    PROF_END(scenes, "highlights-write");

    if (out.oom || !out.ptr) {
        if (out.ptr) free(out.ptr);
        return 1;
    }

    PROF_START(write_file);
    int16_t r = _WriteFile(filename, (uint8_t *)out.ptr, (uint32_t)out.len);
    PROF_END(write_file, "highlights-write");

#ifdef SWOS_SAVE_FORMAT_JSON_PROFILE
    fprintf(stderr, "[json-prof-highlights-write] file_size: %u bytes\n",
            (unsigned)out.len);
#endif
    free(out.ptr);
    if (r != 0) {
        _ShowFsErrorByCode((uint16_t)r);
        return 1;
    }

    PROF_END(total, "highlights-write");

#ifdef SWOS_SAVE_FORMAT_JSON_VERIFY
    verify_roundtrip_region("highlights",
                            hilFileBuffer, data_size,
                            verify_reload_highlights, filename);
#endif

    return 0;
}

int16_t SaveJson_LoadHighlights(uint8_t *filename, const uint8_t *src_unused,
                                uint32_t src_size_unused)
{
    (void)src_unused; (void)src_size_unused;

    PROF_START(total);

    PROF_START(read_file);
    uint8_t *src = NULL;
    uint32_t src_size = 0;
    if (read_save_file_heap(filename, &src, &src_size) != 0)
        return 1;
    PROF_END(read_file, "highlights-load");

#ifdef SWOS_SAVE_FORMAT_JSON_PROFILE
    fprintf(stderr, "[json-prof-highlights-load] file_size: %u bytes\n",
            (unsigned)src_size);
#endif

    PROF_START(validate);
    double version = 0.0;
    if (mjson_get_number((const char *)src, (int)src_size,
                         "$.schema_version", &version) <= 0 ||
        (int)version != SAVE_JSON_SCHEMA_VERSION) {
        free(src); return 1;
    }
    char kind[16] = {0};
    if (mjson_get_string((const char *)src, (int)src_size,
                         "$.file_kind", kind, sizeof(kind)) <= 0 ||
        strcmp(kind, KIND_HIGHLIGHTS) != 0) {
        free(src); return 1;
    }
    PROF_END(validate, "highlights-load");

    string_pool_reset();
    double num = 0.0;

    HilFileHeader *hdr = (HilFileHeader *)hilFileBuffer;

    PROF_START(header);
    if (mjson_get_number((const char *)src, (int)src_size,
                         "$.signature", &num) > 0)
        BE_WRITE_U32((uint8_t *)&hdr->signature, (uint32_t)num);
    if (mjson_get_number((const char *)src, (int)src_size,
                         "$.max_size", &num) > 0)
        BE_WRITE_U32((uint8_t *)&hdr->max_size,  (uint32_t)num);

    load_team_game((const char *)src, (int)src_size,
                   "$.top_team",    (uint8_t *)&hdr->top_team);
    load_team_game((const char *)src, (int)src_size,
                   "$.bottom_team", (uint8_t *)&hdr->bottom_team);
    PROF_END(header, "highlights-load");

    memset(hdr->game_name,  0, sizeof(hdr->game_name));
    memset(hdr->game_round, 0, sizeof(hdr->game_round));
    char tmp[100];
    if (mjson_get_string((const char *)src, (int)src_size,
                         "$.game_name", tmp, sizeof(tmp)) > 0) {
        size_t n = strlen(tmp);
        if (n > sizeof(hdr->game_name)) n = sizeof(hdr->game_name);
        memcpy(hdr->game_name, tmp, n);
    }
    if (mjson_get_string((const char *)src, (int)src_size,
                         "$.game_round", tmp, sizeof(tmp)) > 0) {
        size_t n = strlen(tmp);
        if (n > sizeof(hdr->game_round)) n = sizeof(hdr->game_round);
        memcpy(hdr->game_round, tmp, n);
    }

    /* The 5 u16 header fields are written host-native here. The
     * legacy load path keeps bytes BE and _LoadHilFile applies a
     * single BE16_TO_HOST swap on success (moved out of
     * highlights_menu.c so both paths converge on host-native
     * fields ready for the game to read). */
    uint16_t num_goals = 0;
    if (mjson_get_number((const char *)src, (int)src_size,
                         "$.num_goals", &num) > 0) num_goals = (uint16_t)num;
    hdr->num_goals = num_goals;
    hilNumGoals    = num_goals;

#define HDR_U16(key, field) do { \
        if (mjson_get_number((const char *)src, (int)src_size, \
                             "$." key, &num) > 0) \
            hdr->field = (uint16_t)num; \
    } while (0)
    HDR_U16("team_1_goals",    team_1_goals);
    HDR_U16("team_2_goals",    team_2_goals);
    HDR_U16("pitch_number",    pitch_number);
    HDR_U16("max_substitutes", max_substitutes);
#undef HDR_U16

    /* Zero scene area then decode each scene. Slice the scenes
     * array once so per-scene mjson_find lookups scan a single
     * scene's slice instead of the whole file each iteration. */
    memset(hilFileBuffer + HIL_HEADER_SIZE, 0,
           (size_t)num_goals * HIL_SCENE_SIZE);

    PROF_START(scenes);
    const char *scenes_arr = NULL;
    int scenes_arr_len = 0;
    if (mjson_find((const char *)src, (int)src_size, "$.scenes",
                   &scenes_arr, &scenes_arr_len) == MJSON_TOK_ARRAY) {
        /* Walk scenes via mjson_next so per-scene access is O(1)
         * in array position rather than O(i²·scene_size). */
        int pos = 0, koff, klen, voff, vlen, vt;
        uint16_t i = 0;
        while (i < num_goals &&
               (pos = mjson_next(scenes_arr, scenes_arr_len, pos,
                                 &koff, &klen, &voff, &vlen, &vt)) != 0) {
            if (vt != MJSON_TOK_OBJECT) { i++; continue; }
            load_scene_slice(scenes_arr + voff, vlen,
                             hilFileBuffer + HIL_HEADER_SIZE
                                           + (uint32_t)i * HIL_SCENE_SIZE);
            i++;
        }
    }
    PROF_END(scenes, "highlights-load");

    free(src);

    PROF_END(total, "highlights-load");
    return 0;
}
/* ---------- .TAC (tactics) ----------
 *
 * A single 370-byte TeamTactics struct: 9-byte name + 10 ×
 * PlayerPositions (each 5 × 7 bytes of packed x/y nibbles) + 10-byte
 * pairing table + 1-byte ballOutOfPlayTactics field.
 */

#define SAVE_TAC_DATA_SIZE 370u

static int16_t write_tactics_buf(uint8_t *filename, const uint8_t *src)
{
    JsonOutBuf out = { NULL, 0, 0, 0 };

    mjson_printf(json_out_emit, &out,
        "{\n  %Q:%d,\n  %Q:%Q,\n  ",
        "schema_version", SAVE_JSON_SCHEMA_VERSION,
        "file_kind",      KIND_TACTICS);

    /* name (9 bytes, NUL-padded) */
    {
        size_t n = 0;
        while (n < 9 && src[offsetof(TeamTactics, name) + n] != '\0') n++;
        mjson_printf(json_out_emit, &out, "%Q:%.*Q",
                     "name", (int)n,
                     (const char *)(src + offsetof(TeamTactics, name)));
    }

    /* 10 × 35-byte PlayerPositions arrays. Flat byte arrays for
     * compactness — interpretation is packed x/y nibbles per ball
     * zone, which the editor / match sim know how to unpack. */
    mjson_printf(json_out_emit, &out, ",\n  %Q:[", "player_positions");
    for (int i = 0; i < 10; i++) {
        if (i > 0) mjson_printf(json_out_emit, &out, "%s", ",");
        const uint8_t *p = src + offsetof(TeamTactics, positions)
                              + (size_t)i * sizeof(PlayerPositions);
        mjson_printf(json_out_emit, &out, "%s", "[");
        for (int j = 0; j < (int)sizeof(PlayerPositions); j++) {
            if (j > 0) mjson_printf(json_out_emit, &out, "%s", ",");
            mjson_printf(json_out_emit, &out, "%u", (unsigned)p[j]);
        }
        mjson_printf(json_out_emit, &out, "%s", "]");
    }
    mjson_printf(json_out_emit, &out, "%s", "]");

    /* unk_table (10 bytes) + ball_out_of_play_tactics (1 byte) */
    mjson_printf(json_out_emit, &out, ",\n  %Q:[", "unk_table");
    for (int i = 0; i < 10; i++) {
        if (i > 0) mjson_printf(json_out_emit, &out, "%s", ",");
        mjson_printf(json_out_emit, &out, "%u",
                     (unsigned)src[offsetof(TeamTactics, unkTable) + i]);
    }
    mjson_printf(json_out_emit, &out, "%s", "],\n  ");
    mjson_printf(json_out_emit, &out, "%Q:%u\n}\n",
                 "ball_out_of_play_tactics",
                 (unsigned)src[offsetof(TeamTactics, ballOutOfPlayTactics)]);

    if (out.oom || !out.ptr) {
        if (out.ptr) free(out.ptr);
        return 1;
    }
    int16_t r = _WriteFile(filename, (uint8_t *)out.ptr, (uint32_t)out.len);
    free(out.ptr);
    if (r != 0) {
        _ShowFsErrorByCode((uint16_t)r);
        return 1;
    }

#ifdef SWOS_SAVE_FORMAT_JSON_VERIFY
    /* TeamTactics is a caller-provided buffer (not a global), so we
     * snapshot the 370 bytes locally and round-trip through the
     * load path with a scratch destination. */
    {
        uint8_t scratch[SAVE_TAC_DATA_SIZE];
        memset(scratch, 0, sizeof(scratch));
        if (SaveJson_LoadTactics(filename, scratch, sizeof(scratch)) == 0) {
            size_t mismatches = 0, first = (size_t)-1;
            for (uint32_t i = 0; i < SAVE_TAC_DATA_SIZE; i++) {
                if (scratch[i] != src[i]) {
                    if (mismatches < 20) {
                        if (first == (size_t)-1) first = i;
                        fprintf(stderr,
                                "[json-verify-tactics] mismatch at +%u: loaded=0x%02X saved=0x%02X\n",
                                (unsigned)i, scratch[i], src[i]);
                    }
                    mismatches++;
                }
            }
            fprintf(stderr,
                    "[json-verify-tactics] %zu mismatches across %u bytes (first at +%zu)\n",
                    mismatches, (unsigned)SAVE_TAC_DATA_SIZE, first);
        }
    }
#endif

    return 0;
}

static int16_t load_tactics_buf(const char *src, int src_size, uint8_t *dst)
{
    double version = 0.0;
    if (mjson_get_number(src, src_size, "$.schema_version", &version) <= 0 ||
        (int)version != SAVE_JSON_SCHEMA_VERSION)
        return 1;

    char kind[16] = {0};
    if (mjson_get_string(src, src_size, "$.file_kind", kind, sizeof(kind)) <= 0 ||
        strcmp(kind, KIND_TACTICS) != 0)
        return 1;

    memset(dst, 0, SAVE_TAC_DATA_SIZE);

    /* name (9 bytes max) */
    char tmpname[16] = {0};
    if (mjson_get_string(src, src_size, "$.name", tmpname, sizeof(tmpname)) > 0) {
        size_t n = strlen(tmpname);
        if (n > 8) n = 8;
        memcpy(dst + offsetof(TeamTactics, name), tmpname, n);
    }

    /* 10 × 35-byte position arrays */
    for (int i = 0; i < 10; i++) {
        char path[64];
        snprintf(path, sizeof(path), "$.player_positions[%d]", i);
        load_bytes_at(src, src_size, path,
                      dst, offsetof(TeamTactics, positions)
                            + (size_t)i * sizeof(PlayerPositions),
                      sizeof(PlayerPositions));
    }

    load_bytes_at(src, src_size, "$.unk_table",
                  dst, offsetof(TeamTactics, unkTable), 10);

    double num = 0.0;
    if (mjson_get_number(src, src_size, "$.ball_out_of_play_tactics", &num) > 0)
        dst[offsetof(TeamTactics, ballOutOfPlayTactics)] = (uint8_t)(int)num;

    return 0;
}

int16_t SaveJson_WriteTactics(uint8_t *filename, const uint8_t *src, uint32_t src_size)
{
    if (src_size < SAVE_TAC_DATA_SIZE) return 1;
    return write_tactics_buf(filename, src);
}

int16_t SaveJson_LoadTactics(uint8_t *filename, uint8_t *dst, uint32_t dst_capacity)
{
    if (dst_capacity < SAVE_TAC_DATA_SIZE) return 1;

    uint8_t *src = NULL;
    uint32_t src_size = 0;
    if (read_save_file_heap(filename, &src, &src_size) != 0)
        return 1;

    int16_t r = load_tactics_buf((const char *)src, (int)src_size, dst);
    free(src);
    return r;
}
