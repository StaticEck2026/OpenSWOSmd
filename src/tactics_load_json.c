/* ======================================================================
 * tactics_load_json.c — JSON tactics loader (SDL build)
 *
 * Reads bin/data_json/tactics/<id>.json and fills the compiled
 * `TeamTactics _teamTactics442 … _teamTacticsDefend` globals IN PLACE, so
 * editing the JSON changes the running game with no call-site churn (every
 * `_teamTacticsTable[i]` keeps pointing at the same symbol). The analogue of
 * menu_load_json.c / DataJson_InstallCompetitionGraph for formation data.
 *
 * A TeamTactics is a fixed 370-byte struct of plain bytes (no pointers), so
 * loading is a straight named-field deserialization — no symbol resolution.
 * The JSON shape (export_tactics.py / emit_tactics.py):
 *     name_bytes[9]      -> int8_t  name[9]
 *     players[10][5][7]  -> PlayerPositions positions[10] (uint8_t [5][7])
 *     pair_ids[10]       -> uint8_t pairIds[10]   (signed in JSON; -1 == 0xFF)
 *     ball_out_of_play   -> uint8_t ballOutOfPlayTactics
 *
 * Gated by the caller (frontend.c) on env SWOS_TACTICS_JSON (default-ON;
 * SWOS_TACTICS_JSON=0 disables and the compiled formations stay authoritative).
 * SWOS_TACTICS_JSON_VERIFY adds the byte-exact round-trip check (build from JSON,
 * memcmp vs the compiled struct) before the in-place install — a zero-diff is the
 * green light that the JSON fully covers the compiled data. The Amiga path is the
 * Python bake (emit_tactics.py); this file is compiled there but never called.
 * ====================================================================== */

#include "tactics_data.h"   /* TeamTactics + the _teamTactics* externs */
#include "swos_io.h"   /* swos_data_path — rom:/ on the Switch */
#include "mjson.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The 12 presets in _teamTacticsTable[] order (USER tactics live in the career
 * save run and the editor copy is a scratch buffer — neither is shipped JSON). */
static const struct { const char *id; TeamTactics *t; } s_presets[] = {
    { "442", &_teamTactics442 }, { "541", &_teamTactics541 }, { "451", &_teamTactics451 },
    { "532", &_teamTactics532 }, { "352", &_teamTactics352 }, { "433", &_teamTactics433 },
    { "424", &_teamTactics424 }, { "343", &_teamTactics343 }, { "Sweep", &_teamTacticsSweep },
    { "523", &_teamTactics523 }, { "Attack", &_teamTacticsAttack }, { "Defend", &_teamTacticsDefend },
};
#define NUM_PRESETS ((int)(sizeof s_presets / sizeof s_presets[0]))

/* Parse one array-element value slice (a hex string "0x1A" or a number 0/-1) to
 * a long. mjson_next hands back the slice + token type. */
static long tok_long(const char *s, int len, int t)
{
    char buf[32];
    int n;
    if (t == MJSON_TOK_STRING) { s++; len -= 2; }   /* strip the surrounding quotes */
    n = len; if (n < 0) n = 0; if (n > (int)sizeof buf - 1) n = (int)sizeof buf - 1;
    memcpy(buf, s, (size_t)n);
    buf[n] = '\0';
    return strtol(buf, NULL, 0);                      /* base 0 -> 0x.. and decimal/sign */
}

/* Walk a leaf array of `count` byte values into `dst` (stride bytes apart).
 * Returns the number written (== count on success). */
static int read_bytes(const char *arr, int al, uint8_t *dst, int stride, int count)
{
    int off = 0, ko, kl, vo, vl, vt, i = 0;
    while ((off = mjson_next(arr, al, off, &ko, &kl, &vo, &vl, &vt)) != 0 && i < count) {
        dst[i * stride] = (uint8_t)tok_long(arr + vo, vl, vt);
        i++;
    }
    return i;
}

/* Build a TeamTactics from one tactics JSON. Returns 0 on success, -1 on a
 * malformed/short field (the compiled struct is then left untouched by callers). */
static int build_one(const char *j, int jl, TeamTactics *out)
{
    const char *a; int al;
    memset(out, 0, sizeof *out);

    if (mjson_find(j, jl, "$.name_bytes", &a, &al) != MJSON_TOK_ARRAY) return -1;
    if (read_bytes(a, al, (uint8_t *)out->name, 1, 9) != 9) return -1;

    if (mjson_find(j, jl, "$.players", &a, &al) != MJSON_TOK_ARRAY) return -1;
    {
        int poff = 0, pko, pkl, pvo, pvl, pvt, p = 0;
        while ((poff = mjson_next(a, al, poff, &pko, &pkl, &pvo, &pvl, &pvt)) != 0 && p < 10) {
            const char *pl = a + pvo; int pll = pvl;
            int zoff = 0, zko, zkl, zvo, zvl, zvt, z = 0;
            if (pvt != MJSON_TOK_ARRAY) return -1;
            while ((zoff = mjson_next(pl, pll, zoff, &zko, &zkl, &zvo, &zvl, &zvt)) != 0 && z < 5) {
                if (zvt != MJSON_TOK_ARRAY) return -1;
                if (read_bytes(pl + zvo, zvl, &out->positions[p].positions[z][0], 1, 7) != 7)
                    return -1;
                z++;
            }
            if (z != 5) return -1;
            p++;
        }
        if (p != 10) return -1;
    }

    if (mjson_find(j, jl, "$.pair_ids", &a, &al) != MJSON_TOK_ARRAY) return -1;
    if (read_bytes(a, al, out->pairIds, 1, 10) != 10) return -1;

    {
        double v = 0;
        mjson_get_number(j, jl, "$.ball_out_of_play", &v);
        out->ballOutOfPlayTactics = (uint8_t)(long)v;
    }
    return 0;
}

static uint8_t *tactics_slurp(const char *path, int *out_len)
{
    /* Anchored at the data root — rom:/ on the Switch (swos_io.h). */
    char anchored[1024];
    FILE *f = fopen(swos_data_path(path, anchored, (uint32_t)sizeof anchored), "rb");
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

/* Verify: build each formation from JSON and memcmp vs its compiled struct.
 * Returns the number of mismatches/errors (0 = every byte covered). */
int TacticsJson_VerifyAll(void)
{
    int pass = 0, diff = 0, err = 0;
    for (int i = 0; i < NUM_PRESETS; i++) {
        char path[160];
        snprintf(path, sizeof path, "data_json/tactics/%s.json", s_presets[i].id);
        int jl = 0;
        uint8_t *j = tactics_slurp(path, &jl);
        if (!j) { fprintf(stderr, "[tactics-json] %s: cannot open %s\n", s_presets[i].id, path); err++; continue; }
        TeamTactics t;
        int rc = build_one((const char *)j, jl, &t);
        free(j);
        if (rc != 0) { fprintf(stderr, "[tactics-json] %s: build failed\n", s_presets[i].id); err++; continue; }
        if (memcmp(&t, s_presets[i].t, sizeof t) != 0) {
            const uint8_t *aa = (const uint8_t *)&t, *bb = (const uint8_t *)s_presets[i].t;
            int off; for (off = 0; off < (int)sizeof t && aa[off] == bb[off]; off++) {}
            fprintf(stderr, "[tactics-json] %-7s DIFF at +%d (json=%02X compiled=%02X)\n",
                    s_presets[i].id, off, aa[off], bb[off]);
            diff++;
        } else {
            pass++;
        }
    }
    fprintf(stderr, "[tactics-json] verify: %d/%d byte-identical, %d differ, %d error\n",
            pass, NUM_PRESETS, diff, err);
    return diff + err;
}

/* Build every formation from JSON and overwrite the compiled `_teamTactics*`
 * struct IN PLACE (each is a fixed 370 bytes, so no overflow risk). Returns the
 * number skipped/errored. Gated by the caller on env SWOS_TACTICS_JSON. */
int TacticsJson_InstallAll(void)
{
    fprintf(stderr, "[tactics-json] SWOS_TACTICS_JSON — installing formations from "
                    "data_json/tactics/*.json (relative to the binary dir)\n");
    if (getenv("SWOS_TACTICS_JSON_VERIFY")) TacticsJson_VerifyAll();

    int installed = 0, changed = 0, err = 0;
    for (int i = 0; i < NUM_PRESETS; i++) {
        char path[160];
        snprintf(path, sizeof path, "data_json/tactics/%s.json", s_presets[i].id);
        int jl = 0;
        uint8_t *j = tactics_slurp(path, &jl);
        if (!j) { fprintf(stderr, "[tactics-json] %s: cannot open %s\n", s_presets[i].id, path); err++; continue; }
        TeamTactics t;
        int rc = build_one((const char *)j, jl, &t);
        free(j);
        if (rc != 0) { fprintf(stderr, "[tactics-json] %s: build failed\n", s_presets[i].id); err++; continue; }
        int edited = memcmp(&t, s_presets[i].t, sizeof t) != 0;
        memcpy(s_presets[i].t, &t, sizeof t);     /* JSON is now authoritative */
        installed++;
        if (edited) {
            changed++;
            fprintf(stderr, "[tactics-json]   %s: loaded from JSON (EDITED vs compiled)\n", s_presets[i].id);
        }
    }
    fprintf(stderr, "[tactics-json] install: %d/%d formations loaded from JSON in place "
            "(%d edited vs compiled, %d error)\n", installed, NUM_PRESETS, changed, err);
    fflush(stderr);
    return err;
}
