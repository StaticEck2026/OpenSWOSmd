/*
 * save_format_json.c — JSON serializer/parser for SWOS save files.
 *
 * Replaces the legacy raw-memory-dump format. See SAVE_FORMAT_JSON.md
 * for design rationale, schema, and migration plan.
 *
 * STATUS: skeleton only. The mjson library (https://github.com/cesanta/mjson)
 * needs to be dropped into src/ as mjson.h + mjson.c before the bodies
 * below can be filled in. Until then, every Save/Load entry point
 * returns an error so the dispatcher in file_io.c falls back to the
 * legacy path.
 *
 * Implementation plan (multi-session):
 *
 *   1. Drop mjson.h + mjson.c into src/, add to Makefile sources.
 *   2. Author the .CAR schema in src/save_format_json_career.c —
 *      one function per major sub-region of careerFileBuffer:
 *        - season summary (currentSeasonNumber, careerTeam, ...)
 *        - seasonInformationsArray[20] with semantic comp IDs
 *        - careerCompetitions (the *Copy slots)
 *        - careerSelectedTeams (the 16 team file records)
 *        - misc career state
 *      Both write_* (state → JSON via mjson_printf) and read_*
 *      (JSON via mjson_get_* → state) per sub-region.
 *   3. .SEA / .PRE share most of the competition schema with .CAR;
 *      factor those out.
 *   4. .DIY: diyFileBufferCopy is smaller — a single object with
 *      one round of cup/league state.
 *   5. .HIL: highlight replay — sprite states, frame counts.
 *   6. Tactics (.TAC): trivial — 11-position array with a name.
 *   7. Cut over: flip SWOS_SAVE_FORMAT_JSON=1 in default Makefile
 *      flags, delete legacy paths after a release cycle of dual-
 *      format support.
 *
 * Schema conventions:
 *
 *   - All field names lowercase_with_underscores, matching the
 *     Amiga global names where possible (e.g. "current_season_number",
 *     "career_team", "season_informations").
 *   - Pointer fields are *never* stored as raw addresses. They become
 *     either:
 *       - a competition-name string ID    ("ec_qualifying_group_7")
 *       - a competition record reference  ({ "kind": "ec_qual", "index": 7 })
 *       - null when the pointer is unset
 *   - Byte/word arrays go in as JSON arrays of integers. Large
 *     packed structs (team records) get one JSON object with named
 *     sub-fields rather than a raw byte array.
 *   - Integers are JSON numbers (not strings). uint32 fits in
 *     IEEE 754 double precisely; the few uint64 fields we have can
 *     stay split into hi/lo pairs.
 *   - Versioning: top-level "schema_version" : 1. Bumped only on
 *     incompatible changes; additive changes are silent.
 */

#include "save_format_json.h"

/* Stubs — all return failure so the legacy path always wins until
 * mjson lands and we author the schema. */

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

int16_t SaveJson_WriteCareer(uint8_t *filename)     { (void)filename; return 1; }
int16_t SaveJson_LoadCareer (uint8_t *filename, const uint8_t *src, uint32_t src_size)
                                                    { (void)filename; (void)src; (void)src_size; return 1; }
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
