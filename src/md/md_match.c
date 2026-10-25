/* md_match.c -- starting the engine, and a match: the shared engine's friendly-match path.
 *
 * md_engine_boot does what the Amiga's mainC (src/SWOS.c) does before the menus that
 * matters to a match; md_play_friendly what the FRIENDLY menu (src/menus.c) does when
 * both teams are picked, then InitAndPlayGame -- the same call the menu makes -- which
 * runs SetupPlayers, the pre-match screens and the match itself (game_init.c).
 */
#include "md_hw.h"
#include "md_game.h"
#include "md_data.h"
#include "game_types.h"
#include <string.h>

extern void InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2);
extern void md_fade_init(void);
extern void SetDefaultOptions(void);
extern void InitUserTactics(void);

extern uint8_t *_spriteGraphicsPtr;
extern uint16_t _useIndividualPlayerSkinColor;
extern uint16_t _hilNumGoals;
extern uint16_t _gameLength, _autoReplays, _menuMusic, _autoSaveHighlights, _pitchType;
extern uint16_t _allPlayerTeamsEqual, _gameType;
extern uint16_t _pitchTypeOrSeason[], _friendlyPitchType[], _friendlySeason[];
extern uint16_t _minSubstitutes[], _maxSubstitutes[];
extern uint16_t _gameCanceled;

#define TEAM_COMPUTER 1

static TeamFileEx s_team1, s_team2;
static uint8_t s_pad[2];        /* the pad (1, 2) that plays team 1 / team 2; 0 the computer */
uint8_t md_demo;

/* mainC (src/SWOS.c) up to the menus, without the display, loaders and intro. */
extern char _goalBase[];
extern uint8_t *_goalBasePtr, *_nextGoalPtr;

void md_engine_boot(void)
{
    /* InitSkinAndDefaultPointers (game_utils.c), with the ROM's sprite table for the
     * engine's own 1310-slot one */
    _goalBasePtr = (uint8_t *)_goalBase;
    _nextGoalPtr = (uint8_t *)_goalBase + 19000;
    /* One skin per team, not per player: a team's kit is one VDP palette line
     * (md_render.c), which has room for one set of skin and hair colours. This is the
     * Amiga game's own "same skin for the whole team" mode (color_sprites.c). */
    _useIndividualPlayerSkinColor = 0;
    _spriteGraphicsPtr = (uint8_t *)md_spriteTable;
    for (int i = 0; i < MD_SG_RAM_COUNT; i++)
        md_sgRam[i] = md_sgRom[md_sgRamIndex[i]];
    _hilNumGoals = 0;
    md_fade_init();
    /* What the Amiga does on its way to the main menu (file_io.c, with no saved
     * settings to load): the default options, and the user tactics -- which also computes
     * every formation's position-defensiveness tables the AI plays by. */
    SetDefaultOptions();
    InitUserTactics();
}

const TeamFileEx *md_rom_team(uint16_t n)
{
    return (const TeamFileEx *)(md_teamsData + (uint32_t)n * sizeof(TeamFileEx));
}

TeamFileEx *md_team_by_id(uint16_t teamId)
{
    if (PACK_BE_U16(s_team1.team.countryNumber, s_team1.team.teamOrdinal) == teamId) return &s_team1;
    if (PACK_BE_U16(s_team2.team.countryNumber, s_team2.team.teamOrdinal) == teamId) return &s_team2;
    return 0;
}

/* SetupPlayers' "which player is this team" (play_match_menu.c ShowPlayMatchMenu): on
 * the Amiga a menu where each human picks a joystick. Here the FRIENDLY screen has
 * already chosen. Player 1 is pad 1 (md_isr.c md_input_poll feeds it to joy1, the
 * Amiga's joystick port), player 2 pad 2. `team1` is the team being asked about. */
int16_t ShowPlayMatchMenu(int16_t controlFlags, int16_t maxSubs, int16_t excludeFlags,
                          int16_t isFirstPlayer, int16_t squadChanges,
                          TeamFileEx *team1, TeamFileEx *team2)
{
    (void)controlFlags; (void)maxSubs; (void)excludeFlags; (void)isFirstPlayer;
    (void)squadChanges; (void)team2;
    return s_pad[team1 == &s_team1 ? 0 : 1];
}

/* The FRIENDLY menu (src/menus.c ShowFriendlyMenu) and its PLAY button: the two teams,
 * who controls each, the pitch, then the match. Returns 0 when the match was played, 1
 * when it was abandoned (_gameCanceled). */
int md_play_friendly(const MdFriendly *f)
{
    memcpy(&s_team1, md_rom_team(f->teamA), sizeof s_team1);
    memcpy(&s_team2, md_rom_team(f->teamB), sizeof s_team2);
    if (f->teamA == f->teamB) s_team2.team.teamOrdinal ^= 0x80;   /* a team against itself: two ids */
    s_team1.team.teamControls = f->ctrlA ? 0 : TEAM_COMPUTER;
    s_team2.team.teamControls = f->ctrlB ? 0 : TEAM_COMPUTER;
    s_pad[0] = f->ctrlA;
    s_pad[1] = f->ctrlB;

    _gameType = 0;                  /* GAME_TYPE_NONE: a friendly */
    /* the pitch: a type, random, or the month's (FriendlyPitchTypeProc's two modes) */
    _pitchTypeOrSeason[0] = md_options.pitch == MD_PITCH_SEASONAL;
    _friendlyPitchType[0] = md_options.pitch == MD_PITCH_SEASONAL ? MD_PITCH_RANDOM : md_options.pitch;
    _friendlySeason[0] = md_options.month;
    _minSubstitutes[0] = 2;
    _maxSubstitutes[0] = 5;
    _gameCanceled = 0;
    md_abandoned = 0;
    InitAndPlayGame(&s_team1, &s_team2);
    return _gameCanceled != 0;
}

void md_stadium_screen(void) {}

void md_match_enter(void)
{
    md_render_init_match();
}

void md_match_leave(void)
{
    md_sound_stop();
    md_match_clock_stop();
    md_render_shutdown();
}
