/*
 * save_format_json.h — JSON-based SWOS save file format.
 *
 * See SAVE_FORMAT_JSON.md for design rationale and schema.
 *
 * The JSON path is selected at compile time by `SWOS_SAVE_FORMAT_JSON`
 * (see Makefile). When ON, all SWOS_Save_* functions write JSON via
 * mjson. The load path always auto-detects: if the first non-whitespace
 * byte of the file is '{', JSON is parsed; otherwise the legacy
 * binary loader runs. This lets existing .CAR/.SEA/.PRE/.DIY/.HIL
 * files keep loading until the user re-saves and upgrades to JSON.
 *
 * Public API mirrors the legacy file_io.c entry points one-for-one so
 * the dispatcher in file_io.c is a simple "is it JSON?" branch.
 */
#ifndef SAVE_FORMAT_JSON_H
#define SAVE_FORMAT_JSON_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 if the buffer's first non-whitespace byte is '{', else 0.
 * Used by file_io.c to dispatch JSON vs legacy after _LoadFileEx
 * reads the file into the destination buffer. */
int  SaveJson_BufferIsJson(const uint8_t *buf, uint32_t size);

/* Same check via stdio on `save/<filename>` — used when the file
 * size could exceed careerFileBuffer's capacity (JSON saves can be
 * several hundred KB). Returns 1 if JSON, 0 if not, -1 on open
 * error so the caller can fall back to the legacy load path. */
int  SaveJson_FileIsJson(const uint8_t *filename);

/* Per-file-type save/load. Each returns 0 on success, non-zero on
 * error (caller shows the appropriate error menu). */
int16_t SaveJson_WriteCareer(uint8_t *filename);
int16_t SaveJson_LoadCareer (uint8_t *filename, const uint8_t *src, uint32_t src_size);

int16_t SaveJson_WritePreset(uint8_t *filename);
int16_t SaveJson_LoadPreset (uint8_t *filename, const uint8_t *src, uint32_t src_size);

int16_t SaveJson_WriteDIY   (uint8_t *filename);
int16_t SaveJson_LoadDIY    (uint8_t *filename, const uint8_t *src, uint32_t src_size);

int16_t SaveJson_WriteHighlights(uint8_t *filename);
int16_t SaveJson_LoadHighlights (uint8_t *filename, const uint8_t *src, uint32_t src_size);

int16_t SaveJson_WriteTactics(uint8_t *filename, const uint8_t *src, uint32_t src_size);
int16_t SaveJson_LoadTactics (uint8_t *filename, uint8_t *dst, uint32_t dst_capacity);

/* Country/league byte from the last loaded career save's primary
 * competition. Read by _Rebuild_CompetitionSlotPointers to recover
 * the competitionsTable[] index when selTeamNumber is 0xFFFF (season
 * mode with all-CPU teams). Returns 0xFF when no save has been
 * loaded or the save was from before this field existed. */
uint8_t SaveJson_GetLoadedPrimaryCountryCode(void);

#ifdef __cplusplus
}
#endif

#endif /* SAVE_FORMAT_JSON_H */
