/* custom_team.c -- MY CUSTOM TEAM. SDL only. docs/CUSTOM_TEAMS.md.
 *
 * A custom team is a team of the database that its owner has renamed and redressed:
 * the team's name and coach; the players' names, faces, nationalities and shirt
 * numbers; the two kits; and the line-up and formation it starts in. It is NOT a team
 * with different players: every player's position, skills and value are the
 * database's. That rule is the whole design, and it is kept in three places:
 *
 *   THE FILE has nowhere to put a skill. CUSTOMn.TEAM in the save folder is JSON that
 *   names the database team it came from and lists only what may be changed; each
 *   player is "the database team's player N" with a name, a face, a nationality and a
 *   number. Loading it reads the database team and applies those -- so editing the
 *   file by hand cannot make a player better, only rename him.
 *
 *   THE EDITOR is the game's own EDIT TEAM screen, which never could edit skills. Here
 *   its IMPORT takes a whole database team (identity included) and its UNDO button is
 *   LINEUP, the team screen a match shows, as SWOS 2020's custom team editor has it.
 *
 *   A TEAM FROM THE OTHER PLAYER is checked on arrival (CustomTeam_Allowed): it has to
 *   be a database team with only those things changed, or the lobby treats that player
 *   as having no team. That covers every team, custom or not.
 *
 * SWOS 2020, and the team editors, keep a team as the raw 684-byte team record (.TEAM).
 * A file like that put in the save folder under a slot's name is CONVERTED: the database
 * team it was made from is found by its players (not by the number in the record, which
 * SWOS 2020 sets to a custom team's own), what may be changed is taken from it, and the
 * slot is rewritten as JSON, the original kept beside it as CUSTOMn.TEAM.684. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <strings.h>

#include "custom_team.h"
#include "endian_helpers.h"
#include "kit_art.h"
#include "menu_load_json.h"
#include "menu_symbols.h"
#include "menu_system.h"
#include "mjson.h"
#include "netplay_lobby.h"      /* LOBBY_TACTIC_MAX */
#include "swos2020_menus.h"
#include "swos_io.h"
#include "teams.h"

extern TeamFileEx  _selectedTeams[100];
extern TeamFileEx *_selectedTeamPtr;
extern uint16_t    _selectedTeamCountryNumber;
extern uint16_t    _numSelectedTeams, _currentTeamNumber, _selTeamNumber;
extern uint16_t    _gameType, _isCareer, _trainingGame, _exitGameFlag;
extern uint8_t     _teamFileBuffer[];
extern const char *g_playMatchStartLabel;
extern int32_t     g_playMatchNoUniform;

extern int16_t WriteFile(uint8_t *filename, uint8_t *src, uint32_t size);
extern int16_t LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
extern int     DataJson_LoadTeamFileForCountry(int countryNum, unsigned char *teamFileBuf, int maxTeams);
extern void    ChooseTeamsDialog(uint16_t showClub, uint16_t showNational, uint16_t showOrigCustom,
                                 uint8_t *title, void *onSelect);
extern void    EditTeamsCallback(TeamFileEx *team);
extern int16_t ShowPlayMatchMenu(int16_t controlFlags, int16_t maxSubstitutes, int16_t excludeFlags,
                                 int16_t isFirstPlayer, int16_t squadChanges,
                                 TeamFileEx *team1, TeamFileEx *team2);
extern void    SetExitMenuFlag(void);

enum {
    kPlayers = 16,
    kTeamRecord = 684,
    kMaxTeamsInFile = 93,           /* what the game's own team buffer holds (teams.c) */
    kFaces = 3,                     /* EditTeamsChangePlayerFace: white, ginger, black */
    kNationalities = 155,           /* the three-letter names a nationality indexes (_shortCountryNames) */
    kMaxShirtNumber = 99,
    kPlayerCoach = 2,               /* TEAM_PLAYER_COACH: a side whose line-up can be arranged */
};

static const char *const k_colourNames[10] = {
    "grey", "white", "black", "orange", "red", "blue", "brown", "light_blue", "green", "yellow",
};
static const char *const k_shirtNames[4] = { "plain", "sleeves", "stripes", "hoops" };

/* ======================================================================
 * The database team a custom team is made from
 * ====================================================================== */

/* Each country's teams in memory of our own, read once: the game's buffer
 * (_teamFileBuffer) belongs to whatever menu the player is in, and this is also called
 * from the lobby's polling -- and finding the team a .TEAM record was made from reads
 * every country. s_db is the country db_load last made current. */
static uint8_t  s_dbLoad[2 + kMaxTeamsInFile * kTeamRecord];
static uint8_t *s_dbCache[256];
static uint8_t  s_dbAbsent[256];
static uint8_t *s_db;

static int32_t db_read(int32_t country)
{
    const char *json = getenv("SWOS_DATA_JSON");
    char name[32];
    uint32_t size = 0;

    /* As LoadTeamFile reads a country: the JSON database unless it is turned off or
     * has no such country, then the binary TEAM.nnn (big-endian count). */
    if (!(json && json[0] == '0' && json[1] == '\0') &&
        DataJson_LoadTeamFileForCountry(country, s_dbLoad, kMaxTeamsInFile) > 0)
        return 1;
    snprintf(name, sizeof name, "data/team.%03d", (int)country);
    {
        /* LoadFileEx asks for the disk again, for ever, when a file is not there --
         * and a country number from the other player's team may be any byte. */
        char anchored[1024];
        FILE *f = fopen(swos_data_path(name, anchored, (uint32_t)sizeof anchored), "rb");
        if (!f)
            return 0;
        fclose(f);
    }
    if (LoadFileEx((uint8_t *)name, s_dbLoad, &size) != 0 || size < 2 || size > sizeof s_dbLoad)
        return 0;
    *(uint16_t *)s_dbLoad = BE_READ_U16(s_dbLoad);
    return (uint32_t)*(uint16_t *)s_dbLoad * kTeamRecord + 2 <= size;
}

static int32_t db_load(int32_t country)
{
    size_t bytes;

    if (country < 0 || country > 255 || s_dbAbsent[country])
        return 0;
    if (!s_dbCache[country]) {
        if (!db_read(country)) {
            s_dbAbsent[country] = 1;
            return 0;
        }
        bytes = 2 + (size_t)*(uint16_t *)s_dbLoad * kTeamRecord;
        s_dbCache[country] = (uint8_t *)malloc(bytes);
        if (!s_dbCache[country])
            return 0;
        memcpy(s_dbCache[country], s_dbLoad, bytes);
    }
    s_db = s_dbCache[country];
    return 1;
}

static const TeamFileEx *db_team(int32_t country, int32_t ordinal)
{
    const TeamFileEx *rec;
    uint16_t count, i;

    if (!db_load(country))
        return NULL;
    rec = (const TeamFileEx *)(s_db + 2);
    count = *(uint16_t *)s_db;
    for (i = 0; i < count; i++)
        if (rec[i].team.countryNumber == country && rec[i].team.teamOrdinal == ordinal)
            return &rec[i];
    return NULL;
}

/* What makes a player the player he is: his position, his skills and his value --
 * everything a match plays him by. A custom team may change none of it. His name, face,
 * nationality and shirt number are the owner's: they are shown, not played (and the team
 * screen itself swaps two players' numbers when it swaps a starter and a substitute). */
static int32_t same_player(const PlayerFile *a, const PlayerFile *b)
{
    return (a->positionAndFace & PF_POSITION_MASK) == (b->positionAndFace & PF_POSITION_MASK) &&
           a->passing == b->passing && a->velocityHeading == b->velocityHeading &&
           a->tacklingControl == b->tacklingControl && a->speedFinishing == b->speedFinishing &&
           a->price == b->price;
}

/* Which of the database team's players each of `team`'s is: his own place first (the
 * usual case), else any that nobody has claimed. 0 if one of them is nobody's. */
static int32_t match_players(const TeamFileEx *team, const TeamFileEx *base, uint8_t is[kPlayers])
{
    uint8_t taken[kPlayers] = { 0 };
    int32_t i, j;

    for (i = 0; i < kPlayers; i++)
        is[i] = 0xFF;
    for (i = 0; i < kPlayers; i++)
        if (same_player(&team->players[i], &base->players[i])) {
            is[i] = (uint8_t)i;
            taken[i] = 1;
        }
    for (i = 0; i < kPlayers; i++) {
        if (is[i] != 0xFF)
            continue;
        for (j = 0; j < kPlayers; j++)
            if (!taken[j] && same_player(&team->players[i], &base->players[j])) {
                is[i] = (uint8_t)j;
                taken[j] = 1;
                break;
            }
        if (is[i] == 0xFF)
            return 0;
    }
    return 1;
}

int32_t g_customTeamQuiet;          /* the self-test's: it refuses a team per team on purpose */

int32_t CustomTeam_Allowed(const TeamFileEx *team)
{
    /* The last answer, kept: the lobby asks about the same team several times a second. */
    static TeamFileEx asked;
    static int32_t answer = -1;
    const TeamFileEx *base;
    uint8_t is[kPlayers];
    extern int32_t g_customTeamQuiet;

    if (answer >= 0 && memcmp(&asked, team, sizeof asked) == 0)
        return answer;
    asked = *team;
    base = db_team(team->team.countryNumber, team->team.teamOrdinal);
    answer = base && match_players(team, base, is);
    if (!answer && !g_customTeamQuiet)
        fprintf(stderr, "[custom-team] %.16s (team %d of country %d) is not that team of the database: %s\n",
                (const char *)team->team.teamName, team->team.teamOrdinal, team->team.countryNumber,
                base ? "a player's position, skills or value differ"
                     : "the database has no such team");
    return answer;
}

/* ======================================================================
 * What a custom team may change, taken from one team and put on another
 * ====================================================================== */

static void clean_name(int8_t *to, int32_t size, const char *from)
{
    int32_t n = 0;
    for (; *from && n < size - 1; from++) {
        unsigned char c = (unsigned char)*from;
        if (c >= 'a' && c <= 'z')
            c = (unsigned char)(c - 'a' + 'A');
        if (c >= 0x20)
            to[n++] = (int8_t)c;
    }
    while (n < size)
        to[n++] = 0;
}

static int32_t is_permutation(const uint8_t *order)
{
    uint8_t seen[kPlayers] = { 0 };
    int32_t i;
    for (i = 0; i < kPlayers; i++) {
        if (order[i] >= kPlayers || seen[order[i]])
            return 0;
        seen[order[i]] = 1;
    }
    return 1;
}

/* `out` = the database team `base`, with `from`'s changes; `is[i]` says which of the
 * base's players `from`'s i-th is. Everything else in `out` is the database's. */
static void apply_changes(const TeamFileEx *base, const TeamFileEx *from, const uint8_t is[kPlayers],
                          TeamFileEx *out)
{
    KitLook kit;
    int32_t i;

    *out = *base;
    clean_name(out->team.teamName, sizeof out->team.teamName, (const char *)from->team.teamName);
    clean_name(out->team.coachName, sizeof out->team.coachName, (const char *)from->team.coachName);
    if (from->team.tactics <= LOBBY_TACTIC_MAX)
        out->team.tactics = from->team.tactics;
    for (i = 0; i < 2; i++) {
        KitArt_FromTeam(&from->team, i, &kit);      /* brings every value into range */
        KitArt_ToTeam(&kit, i, &out->team);
    }
    if (is_permutation(from->team.playerNumbers))
        memcpy(out->team.playerNumbers, from->team.playerNumbers, sizeof out->team.playerNumbers);
    for (i = 0; i < kPlayers; i++) {
        PlayerFile *p = &out->players[i];
        *p = base->players[is[i]];
        clean_name(p->playerName, sizeof p->playerName, (const char *)from->players[i].playerName);
        if ((uint8_t)from->players[i].nationality < kNationalities)
            p->nationality = from->players[i].nationality;
        if (from->players[i].shirtNumber >= 1 && from->players[i].shirtNumber <= kMaxShirtNumber)
            p->shirtNumber = from->players[i].shirtNumber;
        /* The three faces the game has; a fourth value fits the bits and is nobody's. */
        if ((from->players[i].positionAndFace & PF_FACE_MASK) >> PF_FACE_SHIFT < kFaces)
            p->positionAndFace = (uint8_t)((p->positionAndFace & ~PF_FACE_MASK) |
                                           (from->players[i].positionAndFace & PF_FACE_MASK));
    }
}

/* The database team as the GAME hands it out (LoadTeamFile: global numbers set), for
 * a team that is going to be played. From a menu only: it loads into the game's own
 * team buffer. */
static int32_t game_team(int32_t country, int32_t ordinal, TeamFileEx *out)
{
    const TeamFileEx *rec = (const TeamFileEx *)(_teamFileBuffer + 2);
    uint16_t count, i;

    if (!db_team(country, ordinal))             /* LoadTeamFile panics on a file that is not there */
        return 0;
    if (LoadTeamFile((uint16_t)country) != 0)
        return 0;
    count = *(uint16_t *)_teamFileBuffer;
    for (i = 0; i < count; i++)
        if (rec[i].team.countryNumber == country && rec[i].team.teamOrdinal == ordinal) {
            *out = rec[i];
            return 1;
        }
    return 0;
}

/* ======================================================================
 * The file: <save folder>/CUSTOMn.TEAM
 * ====================================================================== */

static void slot_file(int32_t slot, char *name, size_t size)
{
    snprintf(name, size, "CUSTOM%d.TEAM", (int)slot + 1);
}

static int32_t name_index(const char *text, const char *const *names, int32_t count)
{
    int32_t i;
    for (i = 0; i < count; i++)
        if (strcmp(text, names[i]) == 0)
            return i;
    return -1;
}

/* A JSON value that is one of `names` or a number below `count`; -1 if neither. */
static int32_t json_choice(const char *json, int32_t length, const char *path,
                           const char *const *names, int32_t count)
{
    char text[24];
    double number;

    if (mjson_get_string(json, length, path, text, (int)sizeof text) > 0)
        return name_index(text, names, count);
    if (mjson_get_number(json, length, path, &number) && number >= 0 && number < count)
        return (int32_t)number;
    return -1;
}

/* mjson hands a \u00XX escape back as UTF-8; the game's names are one byte a letter. */
static void latin1(char *text)
{
    unsigned char *in = (unsigned char *)text, *out = (unsigned char *)text;
    while (*in) {
        if ((*in == 0xC2 || *in == 0xC3) && (in[1] & 0xC0) == 0x80) {
            *out++ = (unsigned char)(((*in & 3) << 6) | (in[1] & 0x3F));
            in += 2;
        } else {
            *out++ = *in++;
        }
    }
    *out = 0;
}

static void json_name(const char *json, int32_t length, const char *path, int8_t *to, int32_t size)
{
    char text[96];
    /* >= 0: an empty name is a name (a blank coach is common in .TEAM files). */
    if (mjson_get_string(json, length, path, text, (int)sizeof text) >= 0) {
        latin1(text);
        clean_name(to, size, text);
    }
}

static void json_kit(const char *json, int32_t length, const char *key, int32_t second, TeamFile *team)
{
    static const char *const k_fields[5] = { "shirt", "colour", "second_colour", "shorts", "socks" };
    KitLook kit;
    uint8_t *value[5] = { &kit.type, &kit.stripes, &kit.basic, &kit.shorts, &kit.socks };
    char path[48];
    int32_t i, v;

    KitArt_FromTeam(team, second, &kit);
    for (i = 0; i < 5; i++) {
        snprintf(path, sizeof path, "$.%s.%s", key, k_fields[i]);
        v = i == 0 ? json_choice(json, length, path, k_shirtNames, 4)
                   : json_choice(json, length, path, k_colourNames, 10);
        if (v >= 0)
            *value[i] = (uint8_t)v;
    }
    KitArt_ToTeam(&kit, second, team);
}

static int32_t load_json(const char *json, int32_t length, TeamFileEx *out)
{
    TeamFileEx base, changed;
    uint8_t is[kPlayers], taken[kPlayers] = { 0 }, order[kPlayers];
    double country = -1, ordinal = -1, number;
    char path[48];
    int32_t i;

    if (!mjson_get_number(json, length, "$.base.country", &country) ||
        !mjson_get_number(json, length, "$.base.ordinal", &ordinal) ||
        !game_team((int32_t)country, (int32_t)ordinal, &base)) {
        fprintf(stderr, "[custom-team] the file names no team of the database (country %d, team %d)\n",
                (int)country, (int)ordinal);
        return 0;
    }
    changed = base;
    json_name(json, length, "$.name", changed.team.teamName, sizeof changed.team.teamName);
    json_name(json, length, "$.coach", changed.team.coachName, sizeof changed.team.coachName);
    if (mjson_get_number(json, length, "$.tactics", &number) && number >= 0 && number <= LOBBY_TACTIC_MAX)
        changed.team.tactics = (uint8_t)number;
    json_kit(json, length, "first_kit", 0, &changed.team);
    json_kit(json, length, "second_kit", 1, &changed.team);

    for (i = 0; i < kPlayers; i++) {
        snprintf(path, sizeof path, "$.lineup[%d]", (int)i);
        order[i] = mjson_get_number(json, length, path, &number) && number >= 0 && number < kPlayers
                 ? (uint8_t)number : 0xFF;
    }
    if (is_permutation(order))
        memcpy(changed.team.playerNumbers, order, sizeof order);

    /* Each player: which of the database team's he is, then his name and face. A file
     * that names one twice, or none, leaves that place to the player the database has
     * there -- the team stays sixteen different players of that team whatever it says. */
    for (i = 0; i < kPlayers; i++) {
        snprintf(path, sizeof path, "$.players[%d].base", (int)i);
        is[i] = 0xFF;
        if (mjson_get_number(json, length, path, &number) && number >= 0 && number < kPlayers &&
            !taken[(int32_t)number]) {
            is[i] = (uint8_t)number;
            taken[is[i]] = 1;
        }
    }
    for (i = 0; i < kPlayers; i++) {
        int32_t j;
        if (is[i] != 0xFF)
            continue;
        for (j = 0; j < kPlayers && taken[(i + j) % kPlayers]; j++)
            ;
        is[i] = (uint8_t)((i + j) % kPlayers);
        taken[is[i]] = 1;
    }
    for (i = 0; i < kPlayers; i++) {
        changed.players[i] = base.players[is[i]];
        snprintf(path, sizeof path, "$.players[%d].name", (int)i);
        json_name(json, length, path, changed.players[i].playerName, sizeof changed.players[i].playerName);
        snprintf(path, sizeof path, "$.players[%d].nationality", (int)i);
        if (mjson_get_number(json, length, path, &number) && number >= 0 && number < kNationalities)
            changed.players[i].nationality = (int8_t)number;
        snprintf(path, sizeof path, "$.players[%d].number", (int)i);
        if (mjson_get_number(json, length, path, &number) && number >= 1 && number <= kMaxShirtNumber)
            changed.players[i].shirtNumber = (int8_t)number;
        snprintf(path, sizeof path, "$.players[%d].face", (int)i);
        if (mjson_get_number(json, length, path, &number) && number >= 0 && number < kFaces)
            changed.players[i].positionAndFace = (uint8_t)(
                (changed.players[i].positionAndFace & ~PF_FACE_MASK) | ((int32_t)number << PF_FACE_SHIFT));
    }
    apply_changes(&base, &changed, is, out);
    return 1;
}

/* SWOS 2020's file: the team record itself. Only what a custom team may change is
 * taken from it; a record whose players are not the database team's is refused. */
/* Which database team a team record was made from, judged by its PLAYERS: the number
 * in the record is no guide, since SWOS 2020 gives a custom team a number of its own
 * (PLAYAVELI.TEAM, made from BRAZIL -- country 82, team 2 -- says country 72, team 17,
 * which is GAMESMASTER here). Its own number is tried first, then every team of every
 * file. 0 if no team of the database has those sixteen players. */
static int32_t find_base(const TeamFileEx *team, int32_t *countryOut, int32_t *ordinalOut)
{
    const TeamFileEx *own = db_team(team->team.countryNumber, team->team.teamOrdinal);
    uint8_t is[kPlayers];
    int32_t country;

    if (own && match_players(team, own, is)) {
        *countryOut = team->team.countryNumber;
        *ordinalOut = team->team.teamOrdinal;
        return 1;
    }
    for (country = 0; country < 256; country++) {
        const TeamFileEx *rec;
        uint16_t count, i;
        if (country == 100 || !db_load(country))   /* 100: the player's own CUSTOMS.EDT */
            continue;
        rec = (const TeamFileEx *)(s_db + 2);
        count = *(uint16_t *)s_db;
        for (i = 0; i < count; i++)
            if (rec[i].team.countryNumber == country && match_players(team, &rec[i], is)) {
                *countryOut = country;
                *ordinalOut = rec[i].team.teamOrdinal;
                return 1;
            }
    }
    return 0;
}

/* A .TEAM record (SWOS 2020's, or an editor's): what may be changed is taken from it,
 * the rest is the database team it was made from. */
int32_t CustomTeam_Save(int32_t slot, const TeamFileEx *team);

static int32_t load_record(const uint8_t *bytes, TeamFileEx *out)
{
    TeamFileEx theirs, base;
    uint8_t is[kPlayers];
    int32_t country, ordinal;

    memcpy(&theirs, bytes, sizeof theirs);
    if (!find_base(&theirs, &country, &ordinal) || !game_team(country, ordinal, &base) ||
        !match_players(&theirs, &base, is)) {
        fprintf(stderr, "[custom-team] the team file %.16s is not a team of the database: no team here has its "
                        "players with their positions, skills and values -- not used\n",
                (const char *)theirs.team.teamName);
        return 0;
    }
    apply_changes(&base, &theirs, is, out);
    fprintf(stderr, "[custom-team] the team file %.16s was made from %.16s (country %d, team %d)\n",
            (const char *)theirs.team.teamName, (const char *)base.team.teamName, (int)country, (int)ordinal);
    return 1;
}

int32_t CustomTeam_Load(int32_t slot, TeamFileEx *out)
{
    char name[24];
    uint8_t *data = NULL;
    uint32_t size = 0;
    int32_t ok;

    if (slot < 0 || slot >= CUSTOM_TEAM_SLOTS)
        return 0;
    slot_file(slot, name, sizeof name);
    if (swos_io_load_alloc((const uint8_t *)name, &data, &size) != 0)
        return 0;
    if (size == kTeamRecord && data[0] != '{') {
        /* Converted once: the slot becomes JSON, the record is kept beside it. */
        ok = load_record(data, out);
        if (ok) {
            char kept[32];
            snprintf(kept, sizeof kept, "%s.684", name);
            if (swos_io_save((const uint8_t *)kept, data, size) == 0 && CustomTeam_Save(slot, out))
                fprintf(stderr, "[custom-team] %s converted to this game's format; the original is %s\n", name, kept);
        }
    } else {
        ok = load_json((const char *)data, (int32_t)size, out);
    }
    swos_io_free(data);
    return ok;
}

/* ---- writing ---- */

typedef struct Text { char *at; size_t left; } Text;

static void put(Text *t, const char *format, ...)
{
    va_list args;
    int n;
    va_start(args, format);
    n = vsnprintf(t->at, t->left, format, args);
    va_end(args);
    if (n > 0 && (size_t)n < t->left) {
        t->at += n;
        t->left -= (size_t)n;
    }
}

static void put_string(Text *t, const int8_t *text, int32_t size)
{
    int32_t i;
    put(t, "\"");
    for (i = 0; i < size && text[i]; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '"' || c == '\\')  put(t, "\\%c", c);
        else if (c < 0x20 || c >= 0x7F) put(t, "\\u%04x", c);
        else put(t, "%c", c);
    }
    put(t, "\"");
}

static void put_kit(Text *t, const char *key, const TeamFile *team, int32_t second)
{
    KitLook kit;
    KitArt_FromTeam(team, second, &kit);
    put(t, "  \"%s\": { \"shirt\": \"%s\", \"colour\": \"%s\", \"second_colour\": \"%s\", "
           "\"shorts\": \"%s\", \"socks\": \"%s\" },\n", key, k_shirtNames[kit.type],
        k_colourNames[kit.stripes], k_colourNames[kit.basic], k_colourNames[kit.shorts], k_colourNames[kit.socks]);
}

/* The file's text for a team; 0 if it is not a database team with only what a custom
 * team may change changed. */
static int32_t write_json(const TeamFileEx *team, char *json, size_t size, const TeamFileEx **baseOut)
{
    Text t = { json, size };
    const TeamFileEx *base;
    uint8_t is[kPlayers];
    int32_t i;

    base = db_team(team->team.countryNumber, team->team.teamOrdinal);
    if (!base || !match_players(team, base, is))
        return 0;
    if (baseOut)
        *baseOut = base;
    put(&t, "{\n  \"format\": \"openswos-team\",\n  \"version\": 1,\n");
    put(&t, "  \"base\": { \"country\": %d, \"ordinal\": %d, \"name\": ", team->team.countryNumber,
        team->team.teamOrdinal);
    put_string(&t, base->team.teamName, sizeof base->team.teamName);
    put(&t, " },\n  \"name\": ");
    put_string(&t, team->team.teamName, sizeof team->team.teamName);
    put(&t, ",\n  \"coach\": ");
    put_string(&t, team->team.coachName, sizeof team->team.coachName);
    put(&t, ",\n  \"tactics\": %d,\n", team->team.tactics);
    put_kit(&t, "first_kit", &team->team, 0);
    put_kit(&t, "second_kit", &team->team, 1);
    put(&t, "  \"lineup\": [");
    for (i = 0; i < kPlayers; i++)
        put(&t, "%s%d", i ? ", " : "", team->team.playerNumbers[i]);
    put(&t, "],\n  \"players\": [\n");
    for (i = 0; i < kPlayers; i++) {
        put(&t, "    { \"base\": %d, \"name\": ", is[i]);
        put_string(&t, team->players[i].playerName, sizeof team->players[i].playerName);
        put(&t, ", \"face\": %d, \"nationality\": %d, \"number\": %d }%s\n",
            (team->players[i].positionAndFace & PF_FACE_MASK) >> PF_FACE_SHIFT,
            (uint8_t)team->players[i].nationality, team->players[i].shirtNumber,
            i + 1 < kPlayers ? "," : "");
    }
    put(&t, "  ]\n}\n");
    return 1;
}

int32_t CustomTeam_Save(int32_t slot, const TeamFileEx *team)
{
    static char json[8192];
    const TeamFileEx *base = NULL;
    char name[24];

    if (slot < 0 || slot >= CUSTOM_TEAM_SLOTS)
        return 0;
    if (!write_json(team, json, sizeof json, &base)) {
        fprintf(stderr, "[custom-team] %.16s is not a team of the database with only its names, faces, kits and "
                        "line-up changed: not saved\n", (const char *)team->team.teamName);
        return 0;
    }
    slot_file(slot, name, sizeof name);
    if (WriteFile((uint8_t *)name, (uint8_t *)json, (uint32_t)strlen(json)) != 0) {
        fprintf(stderr, "[custom-team] %s could not be written\n", name);
        return 0;
    }
    fprintf(stderr, "[custom-team] slot %d saved: %.16s, made from %.16s\n", (int)slot + 1,
            (const char *)team->team.teamName, (const char *)base->team.teamName);
    return 1;
}

/* ======================================================================
 * The editor: the game's EDIT TEAM screen, on a custom team
 * ====================================================================== */

static struct {
    int32_t    on;
    TeamFileEx team;
} s_edit;

int32_t CustomTeam_Editing(void) { return s_edit.on; }
const char *CustomTeam_LineupLabel(void) { return "LINEUP"; }

/* A team of the database, chosen from the game's own list of clubs and national sides. */
static int32_t choose_database_team(TeamFileEx *out)
{
    static char title[] = "SELECT TEAM TO IMPORT";

    ChooseTeamsDialog(1, 1, 0, (uint8_t *)title, (void *)0);
    if ((int16_t)_selectedTeamCountryNumber == -1 || !_selectedTeamPtr)
        return 0;
    *out = *_selectedTeamPtr;
    return 1;
}

static void edit_slot(int32_t slot)
{
    TeamFileEx keep = _selectedTeams[0];

    if (!CustomTeam_Load(slot, &s_edit.team)) {
        if (!choose_database_team(&s_edit.team))
            return;
        CustomTeam_Save(slot, &s_edit.team);
    }
    s_edit.on = 1;
    EditTeamsCallback(&s_edit.team);        /* the EDIT TEAM screen; OK writes the team back */
    s_edit.on = 0;
    _selectedTeams[0] = keep;
    CustomTeam_Save(slot, &s_edit.team);
}

/* LINEUP: the team screen a match shows, on the team being edited (which the EDIT TEAM
 * screen keeps in _selectedTeams[0]). CONFIRM keeps the line-up and formation, EXIT
 * drops what was changed there. The same arrangement the online lobby makes around
 * that screen (online_menus.c lineup_begin). */
void CustomTeam_Lineup(void)
{
    static char confirm[] = "CONFIRM";
    TeamFileEx before = _selectedTeams[0], other = _selectedTeams[1];
    uint16_t numSel = _numSelectedTeams, curNo = _currentTeamNumber, selNo = _selTeamNumber;
    uint16_t gameType = _gameType, isCareer = _isCareer, training = _trainingGame;

    _selectedTeams[0].team.teamControls = kPlayerCoach;
    _selectedTeams[1] = _selectedTeams[0];
    _numSelectedTeams = 2;
    _currentTeamNumber = 0xFFFF;
    _selTeamNumber = 0xFFFF;
    _gameType = 0;
    _isCareer = 0;
    _trainingGame = 0;
    _exitGameFlag = 0;
    g_playMatchStartLabel = confirm;
    g_playMatchNoUniform = 1;               /* the kits are this editor's own business */
    ShowPlayMatchMenu(1, 5, 0, 1, 1, &_selectedTeams[0], &_selectedTeams[1]);
    g_playMatchNoUniform = 0;
    g_playMatchStartLabel = NULL;

    if (_exitGameFlag)
        _selectedTeams[0] = before;
    else
        _selectedTeams[0].team.teamControls = before.team.teamControls;
    _exitGameFlag = 0;
    _selectedTeams[1] = other;
    _numSelectedTeams = numSel; _currentTeamNumber = curNo; _selTeamNumber = selNo;
    _gameType = gameType; _isCareer = isCareer; _trainingGame = training;
}

/* ======================================================================
 * MY CUSTOM TEAMS: the three slots (swos-2020 selectMyCustomTeams.mnu)
 * ====================================================================== */

static struct {
    int32_t picking;                /* choosing a team for a match, not editing one */
    int32_t chosen;                 /* the slot fired on, -1 = EXIT */
    int32_t filled[CUSTOM_TEAM_SLOTS];
    char    label[CUSTOM_TEAM_SLOTS][40];
} s_slots;

static void read_slots(void)
{
    TeamFileEx team;
    int32_t slot;

    for (slot = 0; slot < CUSTOM_TEAM_SLOTS; slot++) {
        s_slots.filled[slot] = CustomTeam_Load(slot, &team);
        if (s_slots.filled[slot])
            snprintf(s_slots.label[slot], sizeof s_slots.label[slot], "%.16s", (const char *)team.team.teamName);
        else
            snprintf(s_slots.label[slot], sizeof s_slots.label[slot], "%s",
                     s_slots.picking ? "-" : "EMPTY - SELECT TO CREATE");
    }
}

static void selectMyCustomTeamsMenu_OnInit(void) {}

static void selectMyCustomTeamsMenu_OnDraw(void)
{
    static char pickHeader[] = "CHOOSE YOUR TEAM", pickExit[] = "OTHER TEAMS";
    int32_t slot;

    for (slot = 0; slot < CUSTOM_TEAM_SLOTS; slot++) {
        MenuEntry *e = CalcMenuEntryAddress((uint16_t)(SelectMyCustomTeams_team1 + slot));
        e->fg = (uintptr_t)s_slots.label[slot];
        e->bgColor = s_slots.filled[slot] ? 13 : 7;     /* light blue: a team; grey: none */
        e->isDisabled = s_slots.picking && !s_slots.filled[slot];
    }
    if (s_slots.picking) {
        CalcMenuEntryAddress(SelectMyCustomTeams_header)->fg = (uintptr_t)pickHeader;
        CalcMenuEntryAddress(SelectMyCustomTeams_exit)->fg = (uintptr_t)pickExit;
    }
}

static void select_slot(int32_t slot)
{
    if (s_slots.picking && !s_slots.filled[slot])
        return;
    s_slots.chosen = slot;
    SetExitMenuFlag();
}

static void selectMyCustomTeam1(void) { select_slot(0); }
static void selectMyCustomTeam2(void) { select_slot(1); }
static void selectMyCustomTeam3(void) { select_slot(2); }

#define HANDLER(fn) { #fn, (void *)fn, SYM_FUNC }
static const MenuSymbol k_handlers[] = {
    HANDLER(selectMyCustomTeamsMenu_OnInit),
    HANDLER(selectMyCustomTeamsMenu_OnDraw),
    HANDLER(selectMyCustomTeam1),
    HANDLER(selectMyCustomTeam2),
    HANDLER(selectMyCustomTeam3),
};
#undef HANDLER

static int32_t show_slots(int32_t picking)
{
    static int32_t registered;
    uint8_t *menu;

    if (!registered) {
        registered = 1;
        MenuJson_RegisterSymbols(k_handlers, (int)(sizeof k_handlers / sizeof k_handlers[0]));
    }
    menu = MenuJson_Load("swos2020/selectMyCustomTeams");
    if (!menu)
        return -1;
    s_slots.picking = picking;
    s_slots.chosen = -1;
    read_slots();
    ShowMenu(menu);
    return s_slots.chosen;
}

void CustomTeam_Manage(void)
{
    int32_t slot;
    while ((slot = show_slots(0)) >= 0)
        edit_slot(slot);
}

int32_t CustomTeam_Pick(TeamFileEx *out)
{
    TeamFileEx team;
    int32_t slot, any = 0;

    for (slot = 0; slot < CUSTOM_TEAM_SLOTS; slot++)
        any |= CustomTeam_Load(slot, &team);
    if (!any)
        return -1;
    slot = show_slots(1);
    if (slot < 0)
        return 0;
    return CustomTeam_Load(slot, out) ? 1 : 0;
}

/* ======================================================================
 * --custom-team-selftest
 *
 * Every team of the database: allowed as it is; refused with one skill raised; and,
 * renamed, redressed and rearranged, the same team after a trip through the file's
 * text -- which also may not carry a skill, so a file edited to raise one loads as the
 * database's player.
 * ====================================================================== */
int32_t CustomTeam_SelfTest(void)
{
    static char json[8192];
    static TeamFileEx teams[kMaxTeamsInFile];
    int32_t country, teamsSeen = 0, failed = 0;

    for (country = 0; country < 256; country++) {
        uint16_t count, i;
        if (country == 100 || !db_load(country))    /* 100 is the player's own CUSTOMS.EDT */
            continue;
        count = *(uint16_t *)s_db;
        memcpy(teams, s_db + 2, (size_t)count * sizeof teams[0]);
        for (i = 0; i < count; i++) {
            TeamFileEx team = teams[i], back, cheat;
            uint8_t swap;
            int32_t bad = 0;

            if (team.team.countryNumber != country)
                continue;                           /* a record filed under another country's number */
            teamsSeen++;
            if (!CustomTeam_Allowed(&team))
                bad |= 1;
            cheat = team;
            cheat.players[3].speedFinishing = (int8_t)(cheat.players[3].speedFinishing ^ 0x10);
            g_customTeamQuiet = 1;
            if (CustomTeam_Allowed(&cheat))
                bad |= 2;
            g_customTeamQuiet = 0;

            /* What an owner may do: rename, a face, both kits, the formation, two
             * players swapped in the line-up and two swapped in the record itself. */
            clean_name(team.team.teamName, sizeof team.team.teamName, "SELF TEST UNITED");
            clean_name(team.players[5].playerName, sizeof team.players[5].playerName, "A \"QUOTED\" NAME");
            team.players[5].positionAndFace = (uint8_t)((team.players[5].positionAndFace & ~PF_FACE_MASK) |
                ((((team.players[5].positionAndFace & PF_FACE_MASK) >> PF_FACE_SHIFT) + 1) % kFaces) << PF_FACE_SHIFT);
            team.team.prShirtType = 3; team.team.prStripesColor = 9; team.team.secSocksColor = 4;
            team.team.tactics = 5;
            swap = team.team.playerNumbers[1];
            team.team.playerNumbers[1] = team.team.playerNumbers[12];
            team.team.playerNumbers[12] = swap;
            { PlayerFile p = team.players[7]; team.players[7] = team.players[9]; team.players[9] = p; }
            /* What the team screen does to a starter and a substitute it swaps, and what
             * a SWOS 2020 file does to a player: a number and a nationality of his own. */
            { int8_t n = team.players[2].shirtNumber;
              team.players[2].shirtNumber = team.players[13].shirtNumber; team.players[13].shirtNumber = n; }
            team.players[4].nationality = 13;
            team.players[4].shirtNumber = 77;
            if (!CustomTeam_Allowed(&team))
                bad |= 4;
            if (!write_json(&team, json, sizeof json, NULL) ||
                !load_json(json, (int32_t)strlen(json), &back))
                bad |= 8;
            else {
                /* The team the rule makes of it: the database's players, in the places
                 * and under the names this one has them. (Two players the database gives
                 * the same skills cannot be told apart, and need not be.) */
                TeamFileEx want, base;
                uint8_t is[kPlayers];
                if (!game_team(country, team.team.teamOrdinal, &base) || !match_players(&team, &base, is))
                    bad |= 32;
                else {
                    apply_changes(&base, &team, is, &want);
                    if (memcmp(&back, &want, sizeof want) != 0)
                        bad |= 16;
                    if (memcmp(want.team.teamName, "SELF TEST UNITED", 16) != 0 || want.team.tactics != 5 ||
                        want.team.prShirtType != 3 || want.team.secSocksColor != 4 ||
                        want.team.playerNumbers[1] != team.team.playerNumbers[1] ||
                        want.players[4].nationality != 13 || want.players[4].shirtNumber != 77 ||
                        memcmp(want.players[5].playerName, "A \"QUOTED\" NAME", 15) != 0)
                        bad |= 64;
                }
            }
            /* The file cannot raise a skill: there is no such field to edit, and "base"
             * names a player of the database, so the text of a cheat is the text of the team. */
            if (bad) {
                failed++;
                fprintf(stderr, "[custom-team] self-test: country %d team %d (%.16s) failed, code %d\n",
                        (int)country, team.team.teamOrdinal, (const char *)teams[i].team.teamName, (int)bad);
            }
        }
    }
    fprintf(stderr, "[custom-team] self-test: %d teams of the database, %d failed\n", (int)teamsSeen, (int)failed);
    return teamsSeen > 0 && failed == 0;
}

/* ======================================================================
 * --convert-teams=<in>,<out>
 *
 * Every *.TEAM record in <in> through the same conversion a slot gets, written to
 * <out> under the same name -- like a career, a custom team keeps its extension and
 * holds JSON. For a record that cannot be converted, the database team whose
 * players it comes closest to, and how many of the sixteen match -- so a file whose
 * players had their skills edited can be told from one made from a database this game
 * does not have.
 * ====================================================================== */
static int32_t closest_team(const TeamFileEx *team, int32_t *countryOut, int32_t *ordinalOut)
{
    int32_t country, best = -1;

    for (country = 0; country < 256; country++) {
        const TeamFileEx *rec;
        uint16_t count, i;
        if (country == 100 || !db_load(country))
            continue;
        rec = (const TeamFileEx *)(s_db + 2);
        count = *(uint16_t *)s_db;
        for (i = 0; i < count; i++) {
            uint8_t taken[kPlayers] = { 0 };
            int32_t p, q, same = 0;
            if (rec[i].team.countryNumber != country)
                continue;
            for (p = 0; p < kPlayers; p++)
                for (q = 0; q < kPlayers; q++)
                    if (!taken[q] && same_player(&team->players[p], &rec[i].players[q])) {
                        taken[q] = 1;
                        same++;
                        break;
                    }
            if (same > best) {
                best = same;
                *countryOut = country;
                *ordinalOut = rec[i].team.teamOrdinal;
            }
        }
    }
    return best;
}

int32_t CustomTeam_ConvertDir(const char *in, const char *out)
{
    static char json[8192];
    struct dirent *de;
    DIR *dir = opendir(in);
    int32_t files = 0, converted = 0, refused = 0, unreadable = 0, mismatched = 0;
    int32_t byMatched[kPlayers + 1] = { 0 };

    if (!dir) {
        fprintf(stderr, "[convert-teams] cannot open %s\n", in);
        return 0;
    }
    while ((de = readdir(dir)) != NULL) {
        char path[1024], target[1024], stem[256];
        uint8_t bytes[kTeamRecord + 1];
        TeamFileEx team, theirs;
        size_t len = strlen(de->d_name), got;
        FILE *f;

        if (len < 6 || strcasecmp(de->d_name + len - 5, ".TEAM") != 0)
            continue;
        files++;
        snprintf(path, sizeof path, "%s/%s", in, de->d_name);
        f = fopen(path, "rb");
        got = f ? fread(bytes, 1, sizeof bytes, f) : 0;
        if (f)
            fclose(f);
        if (got != kTeamRecord) {
            fprintf(stderr, "[convert-teams] %s: not a %d-byte team record (%d bytes)\n", de->d_name,
                    kTeamRecord, (int)got);
            unreadable++;
            continue;
        }
        memcpy(&theirs, bytes, sizeof theirs);
        g_customTeamQuiet = 1;
        if (!load_record(bytes, &team) || !write_json(&team, json, sizeof json, NULL)) {
            int32_t country = -1, ordinal = -1, same = closest_team(&theirs, &country, &ordinal);
            const TeamFileEx *near = country >= 0 ? db_team(country, ordinal) : NULL;
            g_customTeamQuiet = 0;
            refused++;
            byMatched[same < 0 ? 0 : same]++;
            fprintf(stderr, "[convert-teams] %s (%.16s): REFUSED -- closest database team %.16s "
                            "(country %d, team %d), %d of 16 players match\n",
                    de->d_name, (const char *)theirs.team.teamName,
                    near ? (const char *)near->team.teamName : "-", (int)country, (int)ordinal, (int)same);
            continue;
        }
        g_customTeamQuiet = 0;
        snprintf(stem, sizeof stem, "%.*s", (int)(len - 5), de->d_name);
        snprintf(target, sizeof target, "%s/%s.TEAM", out, stem);   /* the same name, our format inside */
        f = fopen(target, "wb");
        if (!f || fwrite(json, 1, strlen(json), f) != strlen(json)) {
            fprintf(stderr, "[convert-teams] %s: cannot write %s\n", de->d_name, target);
            if (f)
                fclose(f);
            unreadable++;
            continue;
        }
        fclose(f);
        /* What was written has to load back as the team it was written from. */
        {
            TeamFileEx back;
            if (!load_json(json, (int32_t)strlen(json), &back) ||
                (back.team.globalTeamNumber = team.team.globalTeamNumber,
                 memcmp(&back, &team, sizeof team) != 0)) {
                fprintf(stderr, "[convert-teams] %s: written, but does not load back as the same team\n", de->d_name);
                mismatched++;
            }
        }
        converted++;
    }
    closedir(dir);
    fprintf(stderr, "[convert-teams] %d files: %d converted (%d of them not loading back the same), "
                    "%d refused, %d unreadable\n",
            (int)files, (int)converted, (int)mismatched, (int)refused, (int)unreadable);
    if (refused) {
        int32_t n;
        fprintf(stderr, "[convert-teams] refused, by how many of 16 players match the closest database team:");
        for (n = kPlayers; n >= 0; n--)
            if (byMatched[n])
                fprintf(stderr, " %d->%d", (int)n, (int)byMatched[n]);
        fprintf(stderr, "\n");
    }
    return files > 0 && unreadable == 0 && mismatched == 0;
}
