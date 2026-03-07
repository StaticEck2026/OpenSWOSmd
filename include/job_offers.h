/* job_offers.h — Job offer queue management. */
#pragma once
#include "game_types.h"
#include "menu_system.h"

/* --- Job offer search --- */
int16_t _IsOldJobOffer(uint16_t teamNumber);
int16_t _IsNewJobOffer(uint16_t teamNumber);

/* --- Job offer pointer lookup --- */
JobOffer *_GetJobOfferPtr(uint16_t index);
JobOffer *_GetNewJobOfferPtr(uint16_t index);

/* --- Job offer queue manipulation --- */
void _AddJobOffer(uint16_t teamNumber, uint32_t initialFunds);
void _RemoveJobOffer(uint16_t teamNumber);
void _JobOffers_RemoveOffer(uint16_t teamNumber);
void _JobOffers_MaybeExpireSomeNewOffers(void);
void _JobOffers_FillMissingNewOfferTeamNames(void);
void _JobOffers_RemoveExpiredOffers(void);
void _JobOffers_ConvertNewOfferToOffer(uint16_t teamNumber, uint32_t initialFunds);

/* --- Job offer menu functions (Chunk 99) --- */
void _GotJobOfferInit(void);
void _GotJobOfferFinish(void);
void _AcceptJobOffer(void);
void _j_SetExitMenuFlag_14(void);
void _RejectJobOffer(void);
void _ViewFutureTeam(void);
void _ViewFutureTeamLeague(void);
void _ConsiderJobOffer(void);

/* --- JobOffersInit, DrawOldOfferLine, AfterDrawCommon (Chunk 169) --- */
void _JobOffersInit(void);
void _JobOffers_DrawOldOfferLine(void *entryPtr);
void _JobOffersAfterDrawCommon(void *entryPtr);

/* --- Job offers menu layout/exit (Chunk 116) --- */
void _JobOffers_UpdateMenuEntryLayout(void);
void _JobOffersFinish(void);
void _JobOffersCommon(MenuEntry *entry);
void _OldJobOffersCommon(MenuEntry *entry);
void _JobOffersExit(void);

/* --- Career_MaybeGenerateJobOffer (Chunk 216) --- */
void _Career_MaybeGenerateJobOffer(void);
