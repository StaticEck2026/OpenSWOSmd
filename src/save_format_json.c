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

/* Externs for the other save buffers — used by future sub-files but
 * referenced here so the linker keeps the symbols available. */
extern uint8_t  competitionFileBuffer;
extern uint8_t  diyFileBufferCopy[];

/* file_io.c writer helper. */
extern int16_t _WriteFile(uint8_t *filename, uint8_t *src, uint32_t size);
extern void    _ShowFsErrorByCode(uint16_t code);

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

/* ---------- .CAR writer ---------- */

int16_t SaveJson_WriteCareer(uint8_t *filename)
{
    uint32_t data_size = career_data_size();
    JsonOutBuf out = { NULL, 0, 0, 0 };

    /* Top-level object. The opaque_data field carries the legacy raw
     * bytes base64-encoded — this guarantees round-trip until each
     * sub-region is promoted to a semantic JSON field in a follow-up
     * commit. See SAVE_FORMAT_JSON.md for the promotion TODO. */
    mjson_printf(json_out_emit, &out,
        "{\n"
        "  %Q: %d,\n"
        "  %Q: %Q,\n"
        "  %Q: %d,\n"
        "  %Q: %V\n"
        "}\n",
        "schema_version", SAVE_JSON_SCHEMA_VERSION,
        "file_kind",      KIND_CAREER,
        "num_selected_teams", (int)numSelectedTeams,
        "opaque_data",    (int)data_size, (const char *)&careerFileBuffer);

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
