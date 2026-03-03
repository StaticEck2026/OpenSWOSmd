/* career_init.h — Career mode transfer market initialization */
#pragma once
#include "game_types.h"
#include "menu_system.h"

void _Career_InitTransferMarketState(void);
void _Career_InitSeasonTransferMarketState(void);
void _Career_RandomizeInvitationTournamentCup(void);

/* ---- Manager setup menu callbacks (Chunk 110) ---- */
void _SetPlayerCoachFieldColor(void);
void _CheckSex(void);
void _ChangeSex(MenuEntry *entry);
void _SetName(MenuEntry *entry);
void _SetSurname(MenuEntry *entry);
void _CoachOrPlayerCoach(MenuEntry *entry);

/* --- ChooseTeamAndStartCareer, LoadLiverpool (Chunk 148) --- */
void _ChooseTeamAndStartCareer(void);
void _LoadLiverpool(void);

/* --- ContinueCareer (Chunk 149) --- */
void _ContinueCareer(void);

/* --- InitCompetitionTeamSelection* (Chunk 151) --- */
uint16_t _InitCompetitionTeamSelectionFromTableEntry(uint16_t idx, uint8_t *base);
uint16_t _InitCompetitionTeamSelectionFromTableHeader(uint8_t *base);
uint16_t _InitCompetitionTeamSelectionFromAltTableHeader(uint8_t *base);

/* --- Career_InitializeSelectedTeamAndStartingBalance, Career_ContinueToNextSeasonInitTeamAndFunds (Chunk 170) --- */
void _Career_InitializeSelectedTeamAndStartingBalance(uint16_t teamNumber, int32_t balance);
void _Career_ContinueToNextSeasonInitTeamAndFunds(uint16_t teamNumber, uint32_t baseFunds);
