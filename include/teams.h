/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef TEAMS_H
#define TEAMS_H

#include "menu_system.h"
#include "game_types.h"

/* ---- Team file loading functions ---- */
int16_t _LoadTeamFile(uint16_t extNumber);
void _SetTeamGlobalNumbers(uint16_t numTeams, uint8_t *firstTeam);

/* ---- Team identification ---- */
void _CheckIsTeamNational(void);
uint16_t _IsTeamNational(TeamFileEx *team);

/* ---- Team value / price utilities ---- */
uint16_t _GetTeamValueBase(uint16_t avgPrice);
uint16_t _CalcGateReceiptsBaseFromTeamValue(uint16_t avgPrice);
uint16_t _GetAveragePlayerPriceInSelectedTeams(void);

/* ---- Player display value utilities ---- */
uint16_t _GetPlayerRandomNum(uint8_t *teamEntry);
uint16_t _GetDisplayedPlayerValue(uint8_t *ptr);
void _InitPlayerValueDisplayStruct(uint8_t *playerEntry, uint8_t *dst);

/* ---- Value snapping utilities ---- */
uint16_t _SnapToNearestAvgTeamValueBase(uint16_t value);
uint16_t _SnapUpToAvgTeamValueBase(uint16_t value);

/* ---- Player rotation ---- */
void _RotatePlayersLeft(TeamFileEx *team, uint16_t maxSubs);

/* ---- Team buffer management ---- */
TeamFileEx *_AddTeamToBuffer(TeamFileEx *team);
TeamFileEx *_FindTeam(TeamFileEx *teamToFind, uint16_t *outRemainingCount);
void _DeselectTeam(TeamFileEx *teamToRemove);

/* ---- Player value/status text formatting (Chunk 151) ---- */
void _FormatPlayerValueOrStatusText(uint8_t *base, uint8_t *dst);

/* ---- Player record lookup (Chunk 91) ---- */
uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex);
uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
uint16_t _GetPlayerSlotIndexByPlayerNumber(uint16_t teamNumber, uint16_t playerNumber);

/* ---- SetBenchPlayersNumbers, Career_RemovePlayerFromSelectedTeamRoster (Chunk 95) ---- */
void _SetBenchPlayersNumbers(TeamFileEx *team);
void _Career_RemovePlayerFromSelectedTeamRoster(uint16_t index);
void _RemovePlayerFromTeam(uint16_t playerOrd);

/* ---- ChooseTeams layout & selection (Chunks 219-221) ---- */
void _ChooseTeamsFriendly_UpdateMenuEntryYLayout(void);
void _ViewSelTeamsSetColumns(void);
void _SetTeamEntriesStringsAndColors(void);
void _SetChooseChangeTeamsEntries(void);
void _ChooseTeamsFriendly_DrawChangeTeamsPrompt(MenuEntry *savedA5);
void _ChooseTeamsFriendlyCommon(MenuEntry *entry);
void _ChooseTeamsFriendlyCommon2(MenuEntry *entry);
void _ChooseTeamsFriendlyInit(void);
void _ChooseTeamsFriendlyInitFinish(void);
void _SetTeamsCoordinates(void);
void _SelectTeamsReinit(void);
void _SelectTeamsInit(void);
void _SelTeamForTacticsInit(void);
void _ChooseTeamsInit(void);

/* ---- Team selection menu callbacks (Chunks 107-108) ---- */
void _DeselectAllTeams(void);
void _ChangeTeams(void);
void _AbortChooseTeamsFriendly(void);
void _ChooseTeamsFriendlyOnSelect76(void);
void _ViewSelected(void);
void _ExitChooseTeams2(void);
void _ChooseTeamsPlayMatchSelected(void);

/* ---- Team navigation stack & contest checking (Chunk 109) ---- */
uint8_t *_GetCountryName(uint16_t countryId);
void _PushContinent(uint16_t countryId);
uint16_t _PopContinent(void);
void _SetSelectedTeamsStackTop(void);
void _PushLastSelectedTeam(void);
void _GetSelectedTeamsStackTop(void);
void _MarkSelectedTeamsStackLocation(void);
void _IsContestContinentalCup(void);
uint16_t _IsClubTeamsContest(uint32_t contest);
void _SetTeamsTypeForSelectedContest(void);

/* ---- SelectTeamsFinalMenu ---- */
void _SelectTeamsFinalMenu(uint16_t mode);

/* ---- National team selector utilities (Chunk 91) ---- */
uint8_t *_GetNationalityRecordPtr(uint16_t teamFileAndOrdinal);
int16_t _GetNationalTeamNumber(uint16_t nationality);
int16_t _GetNationalTeamNumberFromTeamFileId(uint16_t teamFileId);
uint32_t _GetNationalityAndTeamFile(uint16_t teamFileAndOrdinal);
int16_t _IsTeamCodeOkForNationalSelector(uint16_t code);
int16_t _IsPlayerOkForNationalSelector(void);

#endif
