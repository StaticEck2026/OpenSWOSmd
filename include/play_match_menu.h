/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef PLAY_MATCH_MENU_H
#define PLAY_MATCH_MENU_H

#include "game_types.h"
#include "menu_system.h"

/* --- Color constants for menu entry backgrounds --- */
#define PINK_TO_BROWN_0          0
#define PINK_TO_BROWN_1          1
#define PINK_TO_BROWN_2          2
#define ORANGE_TO_BROWN_4        4
#define ORANGE_TO_BROWN_5        5
#define ORANGE_TO_BROWN_6        6
#define PINK_TO_BROWN_7          7
#define BLUE_TO_BLACK_8          8
#define PINK_TO_BROWN_9          9
#define RED_10                  10
#define BLUE_TO_PURPLE_11       11
#define ORANGE_TO_BROWN_12      12
#define BRIGHT_BLUE_TO_BLUE_13  13
#define YELLOW_GREEN_BLACK_14   14

/* Team control types */
#define TEAM_COMPUTER      1
#define TEAM_PLAYER_COACH  2
#define TEAM_COACH         3

/* Sprite indices for check marks */
#define SPR_CHECK_BIG        206
#define SPR_NOT_CHECK_BIG    207
#define SPR_CHECK_SMALL      208
#define SPR_NOT_CHECK_SMALL  209

/* Game types */
#define GAME_TYPE_CAREER     4

/* TeamFile offsets (byte offsets into team file record) */
#define TEAMFILE_TEAMCONTROLS    4
#define TEAMFILE_TACTICS        24
#define TEAMFILE_COACHNAME      36
#define TEAMFILE_SOMEFLAG       59
#define TEAMFILE_PLAYERNUMBERS  60

/* --- Top skills string builder (Chunk 151) --- */
void _FormTopSkillsString(uint8_t *dst, uint8_t *playerBase);

/* --- Chunk 98: Play Match Menu Functions --- */
void _PlayMatchMenuInit(void);
void _PlayMatchMenuReinit(void);
void _SetCurrentSelectedPlayerIndexPointer(void);
void _OnSwapPlayer(MenuEntry *menuEntry);
void _PlayMatchMenuSwapPlayers(TeamFile *teamRaw, int16_t pos1, int16_t pos2, int16_t tactPos1, int16_t tactPos2);
int16_t _HandleSquadSwapSelectionToggle(int16_t menuPos);
void _PlayMatchChangeTactics(MenuEntry *menuEntry);
void _ExitPlayMatch(void);
void _PlayMatchSelected(void);
void _EditTacticsInPlayMatchMenu(void);
void _ViewOpponentsSelect(void);
void _ReturnFromViewOpponent(void);
void _PlayMatchOnSelect30(void);
void _PlayMatch30AfterDraw(void);
void _UpdateTeamCheckmark(void);
void _SetGameStartButtonText(void);
void _ShowOrHidePlayMatchButton(void);
void _SetUpTacticsButtons(void);
void _SetPlayerSurnamesOnSidePitch(void);
void _ShowUseMatchTeamIfTraining(void);
void _SetVariableButtons(void);
void _DrawPlayerEntries(MenuEntry *entry);
void _RedrawPlayerNames(void);
void _UpdateLittleCheckmark(void);
void _SavePositionsAndUpdateLittleCheckmark(void);
void _UpdateCheckMarkSprite(void);
void _UseMatchTeamSelect(void);
void _TogglePlayerCoach(void);

/* --- Chunk 212: ShowPlayMatchMenu --- */
#define GAME_TYPE_SEASON     3
int16_t _ShowPlayMatchMenu(int16_t controlFlags, int16_t maxSubstitutes,
                            int16_t excludeFlags, int16_t isFirstPlayer,
                            int16_t squadChanges, TeamFile *team1, TeamFile *team2);

#endif
