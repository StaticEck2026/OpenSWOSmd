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
