#ifndef TEAMS_H
#define TEAMS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;

/* ---- Team file loading functions ---- */
int16_t _LoadTeamFile(uint16_t extNumber);
void _SetTeamGlobalNumbers(uint16_t numTeams, uint8_t *firstTeam);

/* ---- Team identification ---- */
void _CheckIsTeamNational(void);
uint16_t _IsTeamNational(uint8_t *teamGeneral);

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
void _RotatePlayersLeft(uint8_t *teamPtr, uint16_t maxSubs);

/* ---- Team buffer management ---- */
uint8_t *_AddTeamToBuffer(uint8_t *team);
uint8_t *_FindTeam(uint8_t *teamToFind, uint16_t *outRemainingCount);
void _DeselectTeam(uint8_t *teamToRemove);

/* ---- Player record lookup (Chunk 91) ---- */
uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex);
uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
uint16_t _GetPlayerSlotIndexByPlayerNumber(uint16_t teamNumber, uint16_t playerNumber);

/* ---- SetBenchPlayersNumbers, Career_RemovePlayerFromSelectedTeamRoster (Chunk 95) ---- */
void _SetBenchPlayersNumbers(uint8_t *teamPtr);
void _Career_RemovePlayerFromSelectedTeamRoster(uint16_t index);
void _RemovePlayerFromTeam(uint16_t playerOrd);

/* ---- National team selector utilities (Chunk 91) ---- */
uint8_t *_GetNationalityRecordPtr(uint16_t teamFileAndOrdinal);
int16_t _GetNationalTeamNumber(uint16_t nationality);
int16_t _GetNationalTeamNumberFromTeamFileId(uint16_t teamFileId);
uint32_t _GetNationalityAndTeamFile(uint16_t teamFileAndOrdinal);
int16_t _IsTeamCodeOkForNationalSelector(uint16_t code);
int16_t _IsPlayerOkForNationalSelector(void);

#endif
