/*
 * team_json.h -- the team database as OpenSWOS's JSON (bin/data_json/teams/).
 *
 * Converts one of the game's record files -- TEAM.nnn (a country's teams),
 * EUROCUP.TMD / EUROCWC.TMD / UEFACUP.TMD (the European cup pools) or
 * POOLPLYR.DAT (the transfer pool) -- RNC-packed (Amiga disks) or not (the PC
 * game's DATA folder), into exactly the JSON the repository carries: what
 * `scripts/decode_data.py --dump-json` writes, followed by
 * `scripts/assign_team_uuids.py` (a derived uuid5 on every team and player of a
 * TEAM file, a checksum on every team; docs/TEAM_IDS.md).
 *
 * Plain C99, fixed-width types; builds with GCC, Clang and MSVC.
 */
#ifndef SWOSDATA_TEAM_JSON_H
#define SWOSDATA_TEAM_JSON_H

#include <stdint.h>
#include "sd_util.h"

typedef enum {
    SD_TEAMFILE_NONE = 0,   /* not a team database file */
    SD_TEAMFILE_TEAM,       /* TEAM.nnn (nnn = three digits) */
    SD_TEAMFILE_TMD,        /* *.TMD */
    SD_TEAMFILE_POOL        /* POOLPLYR.DAT */
} SdTeamFileType;

/* Classify a file by its name (any case, a path is fine). */
SdTeamFileType Sd_TeamFileType(const char *fileName);

/* The JSON file name the input becomes: "<COUNTRY>.json" for TEAM.nnn (SCOTLAND.json,
 * COUNTRY_47.json for a number the game has no name for), else the stem + ".json"
 * (EUROCUP.json, POOLPLYR.json). 0 on success, -1 when it is not a team file. */
int32_t Sd_TeamJsonName(const char *fileName, char *out, uint32_t cap);

/* Convert one file. `fileName` names it ("TEAM.033", "EUROCUP.TMD", "POOLPLYR.DAT";
 * a path is fine, case does not matter), `raw` is its contents, packed or not.
 * On success returns 0 and outJson holds the JSON (no trailing newline, as committed;
 * Sd_BufFree it). On failure returns -1 and `err` (256 bytes) says why. */
int32_t Sd_TeamFileToJson(const char *fileName, const SdBuf *raw, SdBuf *outJson, char *err);

/* The building blocks, exposed for tests and the game-side checks. */
void Sd_Sha1(const uint8_t *data, uint32_t size, uint8_t digest[20]);
/* uuid5 of `name` in the team-data namespace (af19cc5a-6529-537b-935b-407bd73b75fb),
 * written as 36 lower-case characters plus a NUL. */
void Sd_TeamUuid5(const char *name, char out[37]);

#endif
