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
/* "Did this change help?" indicators drawn in the tactics editor and the squad
 * screen. The editor recomputes each player's position-value (and the team
 * average) on every move and compares it to the snapshot taken when you entered:
 *   CHECK = green tick   → value ROSE  (better fit / stronger team)
 *   CROSS = red cross    → value FELL
 *   (equal → indicator hidden).  BIG = whole-team, SMALL = per-player.
 * Sprite-index ↔ graphic verified against MENUS.bmp + SWOS2.s UpdateBigCheckMark
 * (206 = big green tick, 207 = big red cross, 208/209 the small pair). */
#define SPR_CHECK_BIG        206   /* big green tick   */
#define SPR_CROSS_BIG        207   /* big red cross    */
#define SPR_CHECK_SMALL      208   /* small green tick */
#define SPR_CROSS_SMALL      209   /* small red cross  */

/* Game types */
#define GAME_TYPE_CAREER     4

/* TeamFile offsets (byte offsets into team file record) */
#define TEAMFILE_TEAMCONTROLS    4
#define TEAMFILE_TACTICS        24
#define TEAMFILE_COACHNAME      36
#define TEAMFILE_SOMEFLAG       59
#define TEAMFILE_PLAYERNUMBERS  60

/* --- Top skills string builder (Chunk 151) --- */
void FormTopSkillsString(uint8_t *dst, PlayerFile *playerFile);

/* --- Chunk 98: Play Match Menu Functions --- */
void PlayMatchMenuInit(void);
void PlayMatchMenuReinit(void);
void SetCurrentSelectedPlayerIndexPointer(void);
void OnSwapPlayer(MenuEntry *menuEntry);
void PlayMatchMenuSwapPlayers(TeamFileEx *team, int16_t selectedSlot, int16_t destSlot);
int16_t HandleSquadSwapSelectionToggle(int16_t menuPos);
void PlayMatchChangeTactics(MenuEntry *menuEntry);
void ExitPlayMatch(void);
void PlayMatchSelected(void);
void EditTacticsInPlayMatchMenu(void);
void ViewOpponentsSelect(void);
void ReturnFromViewOpponent(void);
void PlayMatchOnSelect30(void);
void PlayMatch30AfterDraw(void);
void UpdateTeamCheckmark(void);
void SetGameStartButtonText(void);
void ShowOrHidePlayMatchButton(void);
void SetUpTacticsButtons(void);
void SetPlayerSurnamesOnSidePitch(void);
void ShowUseMatchTeamIfTraining(void);
void SetVariableButtons(void);
void DrawPlayerEntries(MenuEntry *entry);
void RedrawPlayerNames(void);
void UpdateLittleCheckmark(void);
void SavePositionsAndUpdateLittleCheckmark(void);
void UpdateCheckMarkSprite(void);
void UseMatchTeamSelect(void);
void TogglePlayerCoach(void);

/* --- Chunk 212: ShowPlayMatchMenu --- */
#define GAME_TYPE_SEASON     3
int16_t ShowPlayMatchMenu(int16_t controlFlags, int16_t _maxSubstitutes,
                            int16_t excludeFlags, int16_t isFirstPlayer,
                            int16_t squadChanges, TeamFileEx *team1, TeamFileEx *team2);

#endif
