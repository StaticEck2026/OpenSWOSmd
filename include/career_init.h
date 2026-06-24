/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* career_init.h — Career mode transfer market initialization */
#pragma once
#include "game_types.h"
#include "menu_system.h"

void Career_InitTransferMarketState(void);
void Career_InitSeasonTransferMarketState(void);
void Career_RandomizeInvitationTournamentCup(void);

/* ---- Manager setup menu callbacks (Chunk 110) ---- */
void SetPlayerCoachFieldColor(void);
void CheckSex(void);
void ChangeSex(MenuEntry *entry);
void SetName(MenuEntry *entry);
void SetSurname(MenuEntry *entry);
void CoachOrPlayerCoach(MenuEntry *entry);

/* --- ChooseTeamAndStartCareer, LoadLiverpool (Chunk 148) --- */
void ChooseTeamAndStartCareer(void);
void LoadLiverpool(void);

/* --- ContinueCareer (Chunk 149) --- */
void ContinueCareer(void);

/* --- InitCompetitionTeamSelection* (Chunk 151) --- */
uint16_t InitCompetitionTeamSelectionFromTableEntry(uint16_t idx, uint8_t *base);
uint16_t InitCompetitionTeamSelectionFromTableHeader(uint8_t *base);
uint16_t InitCompetitionTeamSelectionFromAltTableHeader(uint8_t *base);

/* --- Career_InitializeSelectedTeamAndStartingBalance, Career_ContinueToNextSeasonInitTeamAndFunds (Chunk 170) --- */
void Career_InitializeSelectedTeamAndStartingBalance(uint16_t teamNumber, int32_t balance);
void Career_ContinueToNextSeasonInitTeamAndFunds(uint16_t teamNumber, uint32_t baseFunds);

/* --- Chunk 197: InitCareer, ContinueCareerNextSeason, InitializeNewSeason, InitNewSeason,
 *     Career_SelectEuroCupSlotIfTeamParticipates, DIY_SelectCompetitionSlot4FromCurrentDivision --- */
void InitCareer(uint16_t teamNo);
void ContinueCareerNextSeason(void);
void InitializeNewSeason(void);
void InitNewSeason(uint8_t *src, uint16_t divisionIndex);
uint16_t Career_SelectEuroCupSlotIfTeamParticipates(uint8_t *cupCopy, uint16_t euroCupIndex);
void DIY_SelectCompetitionSlot4FromCurrentDivision(void);

/* --- Chunk 198: Career_ViewOrEditCompetitionSlot --- */
void Career_ViewOrEditCompetitionSlot(uint8_t *comp, uint16_t sel);
