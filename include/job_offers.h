/* job_offers.h — Job offer queue management. */
#pragma once
#include "game_types.h"

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
