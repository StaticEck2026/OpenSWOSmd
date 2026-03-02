#ifndef TRANSFER_MARKET_H
#define TRANSFER_MARKET_H

#include "game_types.h"

/* ---- Transfer pool slot utilities ---- */
uint16_t _GetRemainingTransferPoolSlots(void);
uint16_t _GetRemainingOfferSwapSlots(void);

/* ---- Transfer offer management ---- */
void _RemoveTransferOfferAtIndex(uint16_t offerIndex);
void _PrunePlayerOffersAgainstRosterLimits(void);

/* ---- Home market (career contest) player management ---- */
int16_t _IsPlayerInCareerContest(uint16_t teamNumber, uint16_t playerOrdinal);
uint16_t _AddPlayerToCareerContest(uint16_t teamNumber, uint16_t playerOrdinal);
void _RemovePlayerFromCareerContest(uint16_t teamNumber, uint16_t playerOrdinal);

/* ---- Foreign market player management ---- */
int16_t _FindForeignMarketPlayer(uint16_t teamId, uint16_t playerOrdinal);
uint16_t _AddForeignMarketPlayer(uint16_t teamId, uint16_t playerOrdinal);
void _RemoveForeignMarketPlayer(uint16_t teamId, uint16_t playerOrdinal);

/* ---- Combined market search ---- */
uint16_t _FindPlayerInCareerOrForeignMarket(uint16_t teamId, uint16_t playerOrdinal);

/* ---- Transfer offer cleanup ---- */
void _RemovePlayerFromTransferOffers(uint16_t teamNumber, uint16_t playerOrdinal);

/* ---- Pool player database ---- */
void _FillForeignMarketPlayersData(void);
void _LoadPoolplyrDat(void);
PoolPlayer *_FindPoolplyrDatPlayer(uint16_t teamId, uint16_t playerOrdinal);

/* ---- Market population ---- */
void _AddCareerTeamPlayer(void);
void _FillCareerTeamPlayers(void);
void _AddRandomForeignMarketPlayer(void);
void _FillForeignMarketPlayers(void);
void _UpdateHomeMarketPlayers(void);
void _UpdateForeignMarketPlayers(void);

/* ---- Transfer offer helpers (Chunk 74) ---- */
int16_t _FindTeamIdInDiyTeamsList(uint16_t *teamIdPtr);
int16_t _IsOfferTeamPlayerPairUnique(uint16_t teamId, uint16_t playerIdx);
int16_t _IsTeamEligibleForOfferByRefSkill(uint8_t *team, uint16_t refSkill);
void _RemoveOfferPlayerFromMarketsAndCount(uint16_t teamId, uint16_t playerIndex);
void _RemoveNegotiatedOfferPlayersFromMarkets(void);
int16_t _PickRandomEligibleExtraPlayerOrdinal(uint16_t refSkill);
int16_t _PickRandomEligibleSwapPlayerOrdinalFromTeam(uint8_t *teamBlock, int16_t excludeSlot);

/* ---- Transfer market list (Chunk 75) ---- */
int16_t _FindPlayerOnTransferMarket(uint16_t playerId);
void _AddPlayerToTransferMarket(uint16_t playerId);
void _RemovePlayerFromTransferMarket(uint16_t playerId);
int16_t _CheckForTransferOffers(void);

/* ---- Career finance (Chunk 75) ---- */
void _Career_CommitNewBalanceAndResetFinanceTotals(void);

/* ---- Career pool & foreign market patching (Chunk 75) ---- */
void _AddCareerPoolPlayer(uint16_t teamNumber, uint16_t playerOrdinal, uint8_t *teamFilePtr);
void _Career_PatchTeamWithForeignMarketPlayers(uint8_t *teamPtr);
void _Career_ApplyForeignMarketPlayersToSelectedTeams(void);
void _Career_ApplyPendingForeignMarketPlayersToSelectedTeams(void);

/* ---- DIY fixture & team removal (Chunk 76) ---- */
void _DIY_AdvanceFixtureIndex(uint16_t lo, uint16_t hi);
void _Career_RemovePlayerFromTeamInternal(uint16_t teamNumber, uint16_t playerOrd);

/* ---- Transfer/Offer menu callbacks (Chunk 113) ---- */
void _TransferPlayerOnSelect10(void);
void _TransferPlayerOnSelect11(void);
void _TransferPlayerOnSelect12(void);
void _AcceptOffer(void);
void _RejectOffer(void);
void _AskForMore(void);
void _ConsiderOffer(void);
void _ThirdPartyOfferMenuExit(void);

/* --- Transfer menu helpers (Chunk 117) --- */
void _TransferMenuSetBankState(void);
void _BuyPlayerInit(void);
void _TransferMarketFinish(void);
void _ShowDomesticPlayerMarket(void);

#endif
