/*
 * team_json.c -- the team database files as OpenSWOS's JSON.
 *
 * The record files (all big-endian, as authored on the Amiga; the PC game's
 * DATA folder has the same bytes unpacked):
 *
 *   TEAM.nnn       u16 count, then count TeamFileEx records (684 bytes)
 *   *.TMD          TeamFileEx records, no count (size / 684)
 *   POOLPLYR.DAT   PoolPlayer records, no count (size / 58)
 *
 *   TeamFileEx  0 country  1 ordinal  2 global team number (s16)  4 controls
 *               5 name[17]  22 writes_zero_here  23 and_with_0xfe  24 tactics
 *              25 league  26 kit: 5 primary + 5 secondary bytes  36 coach[23]
 *              59 player_values_dirty  60 player_numbers[16]  76 16 PlayerFile
 *   PlayerFile  0 nationality  1 cached match value  2 shirt number  3 name[23]
 *              26 position (bits 5-7) face (3-4) flags (0-2)
 *              27 cards (0-1) injury (2-4) status bucket (5-7)
 *              28 passing  29 velocity|heading  30 tackling|control
 *              31 speed|finishing (high|low nibble)  32 price  33 goal stats[4]
 *              37 form
 *   PoolPlayer  0 team id (s16)  2 player ordinal  3 team name[17]  20 PlayerFile
 *
 * A name is the bytes up to the first NUL (or the whole field), read as latin-1.
 *
 * The JSON is what scripts/swos_team_file.py's file_to_dict() gives, as Python's
 * json.dump(indent=2) lays it out, with no trailing newline. TEAM files were then
 * rewritten by scripts/assign_team_uuids.py, which adds a "uuid" first on every
 * team and player and a "checksum" after the team's uuid, and writes with
 * json.dumps' default ensure_ascii=True (anything outside space..'~' becomes
 * \u00XX); the TMD and pool files keep decode_data's ensure_ascii=False (latin-1
 * written as UTF-8, only control characters escaped).
 *
 *   uuid      uuid5(af19cc5a-6529-537b-935b-407bd73b75fb,
 *                   "96-97/team/<country>/<ordinal>[/player/<0..15>]")
 *   checksum  first 16 hex digits of SHA-1 over, per player in order: position,
 *             passing&15, velocity, heading, tackling, control, speed, finishing,
 *             price -- nine bytes each (docs/TEAM_IDS.md)
 */
#include "team_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEAM_REC_SIZE     684
#define PLAYER_REC_SIZE   38
#define POOL_REC_SIZE     58
#define PLAYERS_PER_TEAM  16

/* ---------------------------------------------------------------- SHA-1 */
/* After FIPS 180-1; written for this file and placed in the public domain. */

typedef struct {
    uint32_t h[5];
    uint32_t lenLo, lenHi;      /* length in bytes */
    uint8_t  block[64];
    uint32_t used;
} Sha1;

static uint32_t rol32(uint32_t v, int32_t n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(Sha1 *s, const uint8_t *p)
{
    uint32_t w[80], a, b, c, d, e, f, k, t;
    int32_t i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16)
             | ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    for (i = 16; i < 80; i++)
        w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4];
    for (i = 0; i < 80; i++) {
        if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDCu; }
        else             { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
        t = rol32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol32(b, 30); b = a; a = t;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}

static void sha1_init(Sha1 *s)
{
    s->h[0] = 0x67452301u; s->h[1] = 0xEFCDAB89u; s->h[2] = 0x98BADCFEu;
    s->h[3] = 0x10325476u; s->h[4] = 0xC3D2E1F0u;
    s->lenLo = s->lenHi = 0;
    s->used = 0;
}

static void sha1_update(Sha1 *s, const uint8_t *p, uint32_t n)
{
    uint32_t old = s->lenLo;
    s->lenLo += n;
    if (s->lenLo < old)
        s->lenHi++;
    while (n--) {
        s->block[s->used++] = *p++;
        if (s->used == 64) {
            sha1_block(s, s->block);
            s->used = 0;
        }
    }
}

static void sha1_final(Sha1 *s, uint8_t digest[20])
{
    uint8_t len[8];
    uint32_t hiBits = (s->lenHi << 3) | (s->lenLo >> 29), loBits = s->lenLo << 3;
    uint8_t pad = 0x80, zero = 0;
    int32_t i;
    for (i = 0; i < 4; i++) {
        len[i]     = (uint8_t)(hiBits >> (24 - i * 8));
        len[4 + i] = (uint8_t)(loBits >> (24 - i * 8));
    }
    sha1_update(s, &pad, 1);
    while (s->used != 56)
        sha1_update(s, &zero, 1);
    sha1_update(s, len, 8);
    for (i = 0; i < 20; i++)
        digest[i] = (uint8_t)(s->h[i / 4] >> (24 - (i % 4) * 8));
}

void Sd_Sha1(const uint8_t *data, uint32_t size, uint8_t digest[20])
{
    Sha1 s;
    sha1_init(&s);
    sha1_update(&s, data, size);
    sha1_final(&s, digest);
}

/* ---------------------------------------------------------------- uuid5 */

static const uint8_t kTeamNamespace[16] = {   /* af19cc5a-6529-537b-935b-407bd73b75fb */
    0xaf, 0x19, 0xcc, 0x5a, 0x65, 0x29, 0x53, 0x7b,
    0x93, 0x5b, 0x40, 0x7b, 0xd7, 0x3b, 0x75, 0xfb
};

static const char kHex[] = "0123456789abcdef";

void Sd_TeamUuid5(const char *name, char out[37])
{
    Sha1 s;
    uint8_t d[20];
    int32_t i, o = 0;
    sha1_init(&s);
    sha1_update(&s, kTeamNamespace, 16);
    sha1_update(&s, (const uint8_t *)name, (uint32_t)strlen(name));
    sha1_final(&s, d);
    d[6] = (uint8_t)((d[6] & 0x0F) | 0x50);
    d[8] = (uint8_t)((d[8] & 0x3F) | 0x80);
    for (i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out[o++] = '-';
        out[o++] = kHex[d[i] >> 4];
        out[o++] = kHex[d[i] & 15];
    }
    out[o] = 0;
}

/* ------------------------------------------------------------ countries */
/* The CN_* names of include/game_types.h, which decode_data.py reads. */

static const char *const kCountries[86] = {
    "ALBANIA", "AUSTRIA", "BELGIUM", "BULGARIA", "CROATIA", "CYPRUS",
    "CZECH_REPUBLIC", "DENMARK", "ENGLAND", NULL, "ESTONIA", "FAROE_ISLANDS",
    "FINLAND", "FRANCE", "GERMANY", "GREECE", "HUNGARY", "ICELAND",
    "REPUBLIC_IRELAND", "ISRAEL", "ITALY", "LATVIA", "LITHUANIA", "LUXEMBOURG",
    "MALTA", "HOLLAND", "NORTHERN_IRELAND", "NORWAY", "POLAND", "PORTUGAL",
    "ROMANIA", "RUSSIA", "SAN_MARINO", "SCOTLAND", "SLOVENIA", "SPAIN",
    "SWEDEN", "SWITZERLAND", "TURKEY", "UKRAINE", "WALES", "YUGOSLAVIA",
    "ALGERIA", "ARGENTINA", "AUSTRALIA", "BOLIVIA", "BRAZIL", NULL,
    "CHILE", "COLOMBIA", "ECUADOR", "EL_SALVADOR", NULL, NULL,
    NULL, "JAPAN", NULL, NULL, NULL, NULL,
    "MEXICO", NULL, "NEW_ZEALAND", NULL, "PARAGUAY", "PERU",
    "SURINAM", "TAIWAN", NULL, "SOUTH_AFRICA", NULL, "URUGUAY",
    "CUSTOM", "USA", NULL, "INDIA", "BELARUS", "VENEZUELA",
    "SLOVAKIA", "GHANA", "EUROPE", "AFRICA", "SOUTH_AMERICA", "NORTH_AMERICA",
    "ASIA", "OCEANIA"
};

static void country_name(int32_t n, char *out, uint32_t cap)
{
    if (n >= 0 && n < 86 && kCountries[n])
        snprintf(out, cap, "%s", kCountries[n]);
    else
        snprintf(out, cap, "COUNTRY_%d", (int)n);
}

/* ------------------------------------------------------------ file names */

static const char *base_name(const char *path)
{
    const char *b = path, *p;
    for (p = path; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            b = p + 1;
    return b;
}

static char up(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }

static int32_t ends_with_nocase(const char *s, const char *tail)
{
    size_t n = strlen(s), m = strlen(tail), i;
    if (m > n)
        return 0;
    for (i = 0; i < m; i++)
        if (up(s[n - m + i]) != tail[i])
            return 0;
    return 1;
}

/* TEAM.nnn -> nnn, or -1. */
static int32_t team_number(const char *base)
{
    int32_t i;
    for (i = 0; i < 5; i++)
        if (up(base[i]) != "TEAM."[i])
            return -1;
    for (i = 5; i < 8; i++)
        if (base[i] < '0' || base[i] > '9')
            return -1;
    if (base[8])
        return -1;
    return (base[5] - '0') * 100 + (base[6] - '0') * 10 + (base[7] - '0');
}

SdTeamFileType Sd_TeamFileType(const char *fileName)
{
    const char *b = base_name(fileName);
    if (team_number(b) >= 0)
        return SD_TEAMFILE_TEAM;
    if (ends_with_nocase(b, ".TMD"))
        return SD_TEAMFILE_TMD;
    if (ends_with_nocase(b, "POOLPLYR.DAT"))
        return SD_TEAMFILE_POOL;
    return SD_TEAMFILE_NONE;
}

int32_t Sd_TeamJsonName(const char *fileName, char *out, uint32_t cap)
{
    const char *b = base_name(fileName);
    char name[64];
    uint32_t i, n;
    switch (Sd_TeamFileType(fileName)) {
    case SD_TEAMFILE_TEAM:
        country_name(team_number(b), name, sizeof name);
        snprintf(out, cap, "%s.json", name);
        return 0;
    case SD_TEAMFILE_TMD:
    case SD_TEAMFILE_POOL:
        n = (uint32_t)strlen(b);
        for (i = n; i > 0 && b[i - 1] != '.'; i--)
            ;
        n = i ? i - 1 : n;                       /* the stem */
        if (n >= sizeof name)
            n = sizeof name - 1;
        for (i = 0; i < n; i++)
            name[i] = up(b[i]);
        name[n] = 0;
        snprintf(out, cap, "%s.json", name);
        return 0;
    default:
        return -1;
    }
}

/* ------------------------------------------------------------ JSON writer */
/* Python's json.dump(indent=2): one value a line, ", " never at a line end, an
 * empty container as [] or {}. */

#define MAX_DEPTH 8

typedef struct {
    uint8_t *data;
    uint32_t size, cap;
    int32_t  oom;
    int32_t  depth;
    int32_t  count[MAX_DEPTH];      /* items so far in each open container */
    int32_t  asciiOnly;             /* ensure_ascii */
} Jw;

static void put(Jw *w, const char *s, uint32_t n)
{
    if (w->oom)
        return;
    if (w->size + n > w->cap) {
        uint32_t cap = w->cap ? w->cap : 65536;
        uint8_t *p;
        while (cap < w->size + n)
            cap *= 2;
        p = (uint8_t *)realloc(w->data, cap);
        if (!p) {
            w->oom = 1;
            return;
        }
        w->data = p;
        w->cap = cap;
    }
    memcpy(w->data + w->size, s, n);
    w->size += n;
}

static void puts_(Jw *w, const char *s) { put(w, s, (uint32_t)strlen(s)); }

static void newline(Jw *w, int32_t depth)
{
    int32_t i;
    put(w, "\n", 1);
    for (i = 0; i < depth; i++)
        put(w, "  ", 2);
}

/* Python json string escaping of latin-1 text (each byte one code point). */
static void put_string(Jw *w, const uint8_t *s, uint32_t n)
{
    uint32_t i;
    char tmp[8];
    put(w, "\"", 1);
    for (i = 0; i < n; i++) {
        uint8_t c = s[i];
        switch (c) {
        case '"':  puts_(w, "\\\""); continue;
        case '\\': puts_(w, "\\\\"); continue;
        case '\n': puts_(w, "\\n"); continue;
        case '\r': puts_(w, "\\r"); continue;
        case '\t': puts_(w, "\\t"); continue;
        case '\b': puts_(w, "\\b"); continue;
        case '\f': puts_(w, "\\f"); continue;
        default: break;
        }
        if (c < 0x20 || (w->asciiOnly && c > 0x7E)) {
            snprintf(tmp, sizeof tmp, "\\u%04x", c);
            puts_(w, tmp);
        } else if (c < 0x80) {
            tmp[0] = (char)c;
            put(w, tmp, 1);
        } else {                                  /* U+0080..U+00FF as UTF-8 */
            tmp[0] = (char)(0xC0 | (c >> 6));
            tmp[1] = (char)(0x80 | (c & 0x3F));
            put(w, tmp, 2);
        }
    }
    put(w, "\"", 1);
}

/* Start a value: the separator, its line, and its key (NULL in an array). */
static void item(Jw *w, const char *key)
{
    if (w->depth > 0) {
        if (w->count[w->depth - 1]++)
            put(w, ",", 1);
        newline(w, w->depth);
    }
    if (key) {
        put_string(w, (const uint8_t *)key, (uint32_t)strlen(key));
        put(w, ": ", 2);
    }
}

static void open_(Jw *w, const char *key, const char *bracket)
{
    item(w, key);
    puts_(w, bracket);
    if (w->depth < MAX_DEPTH)
        w->count[w->depth] = 0;
    w->depth++;
}

static void close_(Jw *w, const char *bracket)
{
    w->depth--;
    if (w->count[w->depth])
        newline(w, w->depth);
    puts_(w, bracket);
}

static void j_int(Jw *w, const char *key, int32_t v)
{
    char tmp[16];
    item(w, key);
    snprintf(tmp, sizeof tmp, "%d", (int)v);
    puts_(w, tmp);
}

static void j_null(Jw *w, const char *key)
{
    item(w, key);
    puts_(w, "null");
}

static void j_cstr(Jw *w, const char *key, const char *s)
{
    item(w, key);
    put_string(w, (const uint8_t *)s, (uint32_t)strlen(s));
}

/* A fixed-size name field: up to its first NUL. */
static void j_field(Jw *w, const char *key, const uint8_t *s, uint32_t len)
{
    uint32_t n = 0;
    while (n < len && s[n])
        n++;
    item(w, key);
    put_string(w, s, n);
}

/* ------------------------------------------------------------ records */

static int32_t s8(uint8_t v) { return (int8_t)v; }
static int32_t s16(const uint8_t *p) { return (int16_t)Sg_Be16(p); }

static void emit_player(Jw *w, const uint8_t *b, const char *uuid)
{
    int32_t i;
    if (uuid)
        j_cstr(w, "uuid", uuid);
    j_int(w, "nationality", s8(b[0]));
    j_int(w, "cached_match_value", s8(b[1]));
    j_int(w, "shirt_number", s8(b[2]));
    j_field(w, "player_name", b + 3, 23);
    j_int(w, "position", (b[26] >> 5) & 7);
    j_int(w, "face", (b[26] >> 3) & 3);
    j_int(w, "flags", b[26] & 7);
    j_int(w, "cards", b[27] & 3);
    j_int(w, "injury", (b[27] >> 2) & 7);
    j_int(w, "status_bucket", (b[27] >> 5) & 7);
    j_int(w, "passing", s8(b[28]));
    j_int(w, "velocity", b[29] >> 4);
    j_int(w, "heading", b[29] & 15);
    j_int(w, "tackling", b[30] >> 4);
    j_int(w, "control", b[30] & 15);
    j_int(w, "speed", b[31] >> 4);
    j_int(w, "finishing", b[31] & 15);
    j_int(w, "price", s8(b[32]));
    open_(w, "goal_stats", "[");
    for (i = 0; i < 4; i++)
        j_int(w, NULL, b[33 + i]);
    close_(w, "]");
    j_int(w, "form", s8(b[37]));
}

/* assign_team_uuids.py's checksum of a team's sixteen players. */
static void team_checksum(const uint8_t *rec, char out[17])
{
    uint8_t data[PLAYERS_PER_TEAM * 9], d[20];
    int32_t i;
    for (i = 0; i < PLAYERS_PER_TEAM; i++) {
        const uint8_t *p = rec + 76 + i * PLAYER_REC_SIZE;
        uint8_t *o = data + i * 9;
        o[0] = (uint8_t)((p[26] >> 5) & 7);
        o[1] = (uint8_t)(p[28] & 15);
        o[2] = (uint8_t)(p[29] >> 4);
        o[3] = (uint8_t)(p[29] & 15);
        o[4] = (uint8_t)(p[30] >> 4);
        o[5] = (uint8_t)(p[30] & 15);
        o[6] = (uint8_t)(p[31] >> 4);
        o[7] = (uint8_t)(p[31] & 15);
        o[8] = p[32];
    }
    Sd_Sha1(data, sizeof data, d);
    for (i = 0; i < 8; i++) {
        out[i * 2] = kHex[d[i] >> 4];
        out[i * 2 + 1] = kHex[d[i] & 15];
    }
    out[16] = 0;
}

static void emit_team(Jw *w, const uint8_t *b, int32_t withIds)
{
    static const char *const kKit[10] = {
        "primary_shirt_type", "primary_stripes_color", "primary_basic_color",
        "primary_shorts_color", "primary_socks_color",
        "secondary_shirt_type", "secondary_stripes_color", "secondary_basic_color",
        "secondary_shorts_color", "secondary_socks_color"
    };
    char uuid[37], name[64], sum[17];
    int32_t i;

    open_(w, NULL, "{");
    if (withIds) {
        snprintf(name, sizeof name, "96-97/team/%d/%d", (int)b[0], (int)b[1]);
        Sd_TeamUuid5(name, uuid);
        team_checksum(b, sum);
        j_cstr(w, "uuid", uuid);
        j_cstr(w, "checksum", sum);
    }
    open_(w, "team", "{");
    j_int(w, "country_number", b[0]);
    j_int(w, "team_ordinal", b[1]);
    j_int(w, "global_team_number", s16(b + 2));
    j_int(w, "team_controls", b[4]);
    j_field(w, "team_name", b + 5, 17);
    j_int(w, "writes_zero_here", b[22]);
    j_int(w, "and_with_0xfe", b[23]);
    j_int(w, "tactics", b[24]);
    j_int(w, "league", b[25]);
    for (i = 0; i < 10; i++)
        j_int(w, kKit[i], b[26 + i]);
    j_field(w, "coach_name", b + 36, 23);
    j_int(w, "player_values_dirty", b[59]);
    open_(w, "player_numbers", "[");
    for (i = 0; i < 16; i++)
        j_int(w, NULL, b[60 + i]);
    close_(w, "]");
    close_(w, "}");

    open_(w, "players", "[");
    for (i = 0; i < PLAYERS_PER_TEAM; i++) {
        open_(w, NULL, "{");
        if (withIds) {
            snprintf(name, sizeof name, "96-97/team/%d/%d/player/%d", (int)b[0], (int)b[1], (int)i);
            Sd_TeamUuid5(name, uuid);
        }
        emit_player(w, b + 76 + i * PLAYER_REC_SIZE, withIds ? uuid : NULL);
        close_(w, "}");
    }
    close_(w, "]");
    close_(w, "}");
}

int32_t Sd_TeamFileToJson(const char *fileName, const SgBuf *raw, SgBuf *outJson, char *err)
{
    SdTeamFileType type = Sd_TeamFileType(fileName);
    const char *base = base_name(fileName);
    SgBuf blob;
    Jw w;
    uint32_t n, i;
    char fileUpper[64], cname[64];

    outJson->data = NULL;
    outJson->size = 0;
    if (type == SD_TEAMFILE_NONE) {
        snprintf(err, 256, "%s: not a team database file", base);
        return -1;
    }
    if (Sg_RncUnpack(raw, &blob, err))
        return -1;

    memset(&w, 0, sizeof w);
    for (i = 0; base[i] && i < sizeof fileUpper - 1; i++)
        fileUpper[i] = up(base[i]);
    fileUpper[i] = 0;

    open_(&w, NULL, "{");
    if (type == SD_TEAMFILE_TEAM) {
        const uint8_t *recs = blob.data + 2;
        uint32_t count = blob.size >= 2 ? Sg_Be16(blob.data) : 0;
        n = blob.size >= 2 ? (blob.size - 2) / TEAM_REC_SIZE : 0;
        if (count < n)
            n = count;                 /* parse_blob stops at a short record */
        w.asciiOnly = 1;
        j_cstr(&w, "type", "team");
        if (n) {
            j_int(&w, "country", recs[0]);
            country_name(recs[0], cname, sizeof cname);
            j_cstr(&w, "country_name", cname);
        } else {
            j_null(&w, "country");
            j_null(&w, "country_name");
        }
        j_int(&w, "count", (int32_t)n);
        open_(&w, "teams", "[");
        for (i = 0; i < n; i++)
            emit_team(&w, recs + i * TEAM_REC_SIZE, 1);
        close_(&w, "]");
    } else if (type == SD_TEAMFILE_TMD) {
        n = blob.size / TEAM_REC_SIZE;
        j_cstr(&w, "type", "tmd");
        j_cstr(&w, "file", fileUpper);
        j_int(&w, "count", (int32_t)n);
        open_(&w, "teams", "[");
        for (i = 0; i < n; i++)
            emit_team(&w, blob.data + i * TEAM_REC_SIZE, 0);
        close_(&w, "]");
    } else {
        n = blob.size / POOL_REC_SIZE;
        j_cstr(&w, "type", "pool");
        j_cstr(&w, "file", fileUpper);
        j_int(&w, "count", (int32_t)n);
        open_(&w, "players", "[");
        for (i = 0; i < n; i++) {
            const uint8_t *b = blob.data + i * POOL_REC_SIZE;
            open_(&w, NULL, "{");
            j_int(&w, "team_id", s16(b));
            j_int(&w, "player_ordinal", s8(b[2]));
            j_field(&w, "team_name", b + 3, 17);
            open_(&w, "player", "{");
            emit_player(&w, b + 20, NULL);
            close_(&w, "}");
            close_(&w, "}");
        }
        close_(&w, "]");
    }
    close_(&w, "}");
    Sg_BufFree(&blob);

    if (w.oom) {
        free(w.data);
        snprintf(err, 256, "%s: out of memory", base);
        return -1;
    }
    outJson->data = w.data;
    outJson->size = w.size;
    return 0;
}
