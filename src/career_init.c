/* career_init.c — Career mode transfer market initialization.
 *
 * Chunk 61: 3 functions ported from SWOS2.s.
 */

#include "game_types.h"
#include "random.h"

/* ---- External globals (all in SWOS2.s BSS) ---- */
extern uint16_t numPlayersOnTransferMarket;
extern uint16_t numForeignMarketPlayers2;
extern uint16_t transferMarketRandOffset0;
extern uint16_t transferMarketRandOffset1;
extern uint16_t transferMarketRandOffset2;
extern uint16_t transferMarketPhase;
extern uint16_t timeToNegotiate;
extern uint16_t numOffers;
extern uint16_t transferPoolSlotsUsed;
extern uint16_t offerSwapSlotsUsed;
extern uint16_t lastOfferPrimaryPlayerOrdinal;
extern uint16_t offerNegotiationAttemptCount;
extern uint16_t careerSeasonsCompleted;
extern uint16_t currentSeasonShowing;
extern uint16_t seasonTransferQuota;
extern uint16_t pendingForeignPlayerCount;
extern uint16_t offerSwapSlotLimit;
extern uint16_t transferPoolSlotLimit;
extern uint16_t homeMarketUpdateBacklog;
extern uint16_t foreignMarketUpdateBacklog;
extern uint16_t offerExpiryIterationsRemaining;
extern uint16_t removedOfferPlayersFromSelectedTeamCount;
extern uint16_t removedOfferPlayersFromOtherTeamsCount;

/* careerCupPickList: first element of a 5-pointer array in BSS
 * (spills into adjacent BSS_01_164B4A for remaining 4 entries) */
extern uint32_t careerCupPickList;
extern uint8_t invitationTournamentCopy[];
extern uint8_t tourCopy[];

/* ================================================================
 * Career_InitTransferMarketState  (SWOS2.s line 72836)
 *
 * Clears transfer market counters and generates three random
 * offsets (0..15) used during transfer market initialization.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_InitTransferMarketState(void)
{
    numPlayersOnTransferMarket = 0;
    numForeignMarketPlayers2 = 0;
    transferMarketRandOffset1 = _Rand2() & 0xF;
    transferMarketRandOffset2 = _Rand2() & 0xF;
    transferMarketPhase = 0;
    transferMarketRandOffset0 = _Rand2() & 0xF;
}

/* ================================================================
 * Career_InitSeasonTransferMarketState  (SWOS2.s line 72853)
 *
 * Initializes per-season transfer market state: negotiation time,
 * offer counts, pool quotas, and backlog counters.
 *
 * Season transfer quota formula:
 *   numerator = 432 - numForeignMarketPlayers2 - (7 - seasons) * 16
 *   divisor   = 20 - currentSeasonShowing
 *   quota     = (uint32_t)(int32_t)(int16_t)numerator / divisor
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_InitSeasonTransferMarketState(void)
{
    timeToNegotiate = 6;
    numOffers = 0;
    transferPoolSlotsUsed = 0;
    offerSwapSlotsUsed = 0;
    lastOfferPrimaryPlayerOrdinal = 0xFFFF;
    offerNegotiationAttemptCount = 0;

    /* Compute season transfer quota */
    uint16_t seasonsAdj = (uint16_t)(7 - careerSeasonsCompleted) * 16;  /* mulu #$10 */
    uint16_t divisor = 20 - currentSeasonShowing;
    int16_t numerator = (int16_t)(0x1B0 - numForeignMarketPlayers2 - seasonsAdj);
    /* ext.l + divu.w: sign-extend to 32-bit then unsigned divide by word */
    uint16_t quota = (uint16_t)((uint32_t)(int32_t)numerator / divisor);

    seasonTransferQuota = quota;
    pendingForeignPlayerCount = 0;

    uint16_t half = quota >> 1;   /* lsr.w #1 */
    offerSwapSlotLimit = half;
    transferPoolSlotLimit = quota - half;   /* neg.w of (half - quota) */

    homeMarketUpdateBacklog = 0;
    foreignMarketUpdateBacklog = 0;
    offerExpiryIterationsRemaining = 0;
    removedOfferPlayersFromSelectedTeamCount = 0;
    removedOfferPlayersFromOtherTeamsCount = 0;
}

/* ================================================================
 * Career_RandomizeInvitationTournamentCup  (SWOS2.s line 35893)
 *
 * Scans the 5-entry careerCupPickList for entries pointing to
 * invitationTournamentCopy. For each such entry, 50% chance to
 * replace it with tourCopy (randomized via Rand).
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_RandomizeInvitationTournamentCup(void)
{
    uint32_t *pickList = &careerCupPickList;
    int16_t i;

    for (i = 4; i >= 0; i--) {
        if (pickList[i] == (uint32_t)invitationTournamentCopy) {
            if (_Rand() & 1)
                pickList[i] = (uint32_t)tourCopy;
        }
    }
}
