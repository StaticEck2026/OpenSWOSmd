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
void _ViewDomesticPlayerMarket(void);
void _ShowForeignPlayerMarket(void);
void _ViewForeignPlayerMarket(void);

/* --- Player offer menu helpers (Chunk 145) --- */
void _PlayerOffersUpdateOfferAmountText(void);
void _UpdatePlayerOfferMenuIndicators(void);
void _PlayerOffersInit(void);

/* --- Offer expiry & random offers (Chunk 146) --- */
void _GenerateRandomTransferOffers(void);
void _UpdateAndExpireTransferOffers(void);
void _ProcessOfferExpiryCountdown(void);

/* --- Buy Other Players menu (Chunk 147) --- */
void _BuyOtherPlayersInit(void);
void _BuyOtherPlayersFinish(void);

/* --- View Offers menu (Chunk 161) --- */
void _ViewOffersMenu(void);

/* --- Transfer Market menu helpers (Chunk 162) --- */
void _CheckNumPlayersOnTransferMarket(void);
void _CheckIfPlayerSelected(void);
void _EnableDisableOffers(void);
void _TransferMarketInit(void);
void _BuyPlayerMenu(void);
void _GiveFreeTransfer(void);
void _TakeOffMarketMenu(void);
void _TransferMarketCommon(void *entry);

/* --- BuildTransferPlayerMenuEntry (Chunk 163) --- */
void _BuildTransferPlayerMenuEntry(void *item, uint8_t *player,
                                    uint16_t y, int16_t idx,
                                    uint16_t style, uint8_t *extra);
void _TransferMarketAfterDrawCommon(void *entry);

/* --- ShowPlayerOfferMenuForPlayer + market helpers (Chunk 163 cont.) --- */
void _ShowPlayerOfferMenuForPlayer(uint16_t teamNumber, uint16_t playerOrdinal);
void _ShowPlayerMarketInit(void);
void _ShowPlayerMarket(void);
void _PlayerMarketUpdateFilterMenuEntryState(void);
void _PlayerMarketUpdateMoreListLabel(void);
void _PlayerMarketResetListState(void);
void _BuyOtherPlayersCommon(void *entry);
void _BuyOtherPlayersAfterDrawCommon(void *entry);

/* --- ShowSquad callers + OffersInit (Chunk 164) --- */
void _ThirdPartyOfferViewSquad(void);
void _BuyPlayerViewSquad(void);
void _OffersInit(void);

/* --- AfterDrawCommon + OffersCommon + PlayerOffers callbacks (Chunk 165) --- */
void _TransferPlayerAfterDrawCommon(void *entry);
void _ThirdPartyOfferMenuAfterDrawCommon(void *entry);
void _PlayerOffersAfterDrawCommon(void *entry);
void _j_SetExitMenuFlag_11(void);
void _PlayerOffersOnSelect13(void);
void _OffersCommon(void *entry);
void _PlayerOffersOnSelect8(void);
void _PlayerOffersOnSelect10(void);
void _PlayerOffersOnSelect11(void);
void _PlayerOffersOnSelect12(void);
void _PlayerOffersAdjustOfferAmount(void);

#endif
