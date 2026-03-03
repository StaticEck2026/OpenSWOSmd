#include "transfer_market.h"
#include "game_types.h"
#include "menu_system.h"
#include "menus.h"
#include "string_utils.h"
#include "career_teams.h"
#include "input_text.h"

/* Transfer offer entry size: 14 bytes ($E) */
#define OFFER_ENTRY_SIZE    14
/* Home market player entry: 4 bytes (word teamNum + byte playerOrd + pad) */
#define MARKET_ENTRY_SIZE   4
#define HOME_MARKET_MAX     25
#define FOREIGN_MARKET_MAX  15

#define POOLPLAYER_COUNT        900

#define GAME_TYPE_CAREER        4

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint16_t transferPoolSlotLimit;
extern uint16_t transferPoolSlotsUsed;
extern uint16_t offerSwapSlotLimit;
extern uint16_t offerSwapSlotsUsed;
extern uint16_t numOffers;
extern uint8_t  transferOfferTable[];
extern uint16_t careerNumPlayers;
extern uint16_t numHomeMarketPlayers;
extern uint8_t  homeMarketPlayers[];
extern uint16_t numForeignMarketPlayers;
extern uint8_t  foreignMarketPlayers[];
extern uint16_t selTeamNumber;
extern uint16_t gameType;
extern uint16_t numForeignMarketPlayers2;
extern MarketPlayer careerForeignMarketPlayers[];
extern PoolPlayer   poolplyrDatBuffer[];
extern uint16_t poolplyrLoaded;
extern uint8_t  aDataPoolplyrDa[];
extern PoolPlayer   poolPlayerStrideRef[];
extern uint16_t isNationalSelector;
extern uint16_t numSelectedTeams;
extern uint8_t  selTeamsPtr;
extern uint16_t homeMarketUpdateBacklog;
extern uint16_t foreignMarketUpdateBacklog;
extern uint16_t homeMarketRandomPickRetries;
extern uint16_t foreignMarketRandomPickRetries;
extern uint16_t selTeamSelectedPlayerPosition;

/* Already-ported functions called from this module */
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
extern uint16_t _Rand2(void);
extern uint16_t _GetRandomWord(void);
extern uint32_t _GetLowestAndHighestPlayerPrice(void);

/* ---- Additional externs for Chunk 74/75 ---- */
extern uint16_t competitionFileBuffer;
extern uint16_t diyTeamIdListType0[];
extern uint16_t diyTeamIdListType1[];
extern uint16_t diyTeamIdListType2[];
extern uint16_t numDiyTeams;
extern uint16_t teamOffering;
extern uint16_t offerPrimaryPlayerOrdinal;
extern int16_t  numExtraPlayersWanted;
extern int16_t  offerSwapPlayerOrdinal;
extern int16_t  offerExtraPlayerOrdinal;
extern uint16_t offerAmount;
extern uint16_t removedOfferPlayersFromSelectedTeamCount;
extern uint16_t removedOfferPlayersFromOtherTeamsCount;

/* ---- Additional externs for Chunk 75 ---- */
extern uint16_t numPlayersOnTransferMarket;
extern uint16_t playersOnTransferMarket[];
extern uint16_t financeTotalsNeedResetFlag;
extern int32_t  newBalance;
extern int32_t  old_balance;
extern int32_t  playerPurchases;
extern int32_t  playerSales;
extern int32_t  playerWagesBill;
extern int32_t  generalRunningCosts;
extern int32_t  gateReceipts;
extern int32_t  startingBalanceCopy;
extern int32_t  competitionBonuses;
extern uint16_t pendingForeignPlayerCount;
extern uint8_t  lastLoadedExtNumber;
extern uint8_t  viewRandomPhase;

/* C-callable bridge for asm GetPlayerAtIndex (in swos2_unit.s) */
extern uint8_t *_CGetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);

/* File index output from _CGetPlayerAtIndex — written by asm bridge, read by C */
uint16_t _cGetPlayerAtIndexFileIdx __attribute__((used, externally_visible));

/* ================================================================
 * GetRemainingTransferPoolSlots  (SWOS2.s line 72720)
 *
 * Returns the number of remaining transfer pool slots.
 *
 * Asm register convention:
 *   out: d2.w = transferPoolSlotLimit - transferPoolSlotsUsed
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetRemainingTransferPoolSlots(void)
{
    return transferPoolSlotLimit - transferPoolSlotsUsed;
}

/* ================================================================
 * GetRemainingOfferSwapSlots  (SWOS2.s line 72727)
 *
 * Returns the number of remaining offer swap slots.
 *
 * Asm register convention:
 *   out: d2.w = offerSwapSlotLimit - offerSwapSlotsUsed
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetRemainingOfferSwapSlots(void)
{
    return offerSwapSlotLimit - offerSwapSlotsUsed;
}

/* ================================================================
 * RemoveTransferOfferAtIndex  (SWOS2.s line 72848)
 *
 * Removes the transfer offer at the given index by shifting all
 * subsequent entries left by one. Decrements numOffers.
 *
 * Asm register convention:
 *   in:  d0.w = offerIndex
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _RemoveTransferOfferAtIndex(uint16_t offerIndex)
{
    int32_t remaining;
    uint8_t *dst;
    int32_t i, j;

    remaining = (int16_t)((int16_t)numOffers - (int16_t)offerIndex - 2);  /* sub.w d0,d1; subq.w #2,d1 */

    if (remaining >= 0) {
        /* Point to the offer entry to remove */
        dst = transferOfferTable + (uint16_t)((uint32_t)offerIndex * OFFER_ENTRY_SIZE);

        /* Shift subsequent entries left */
        for (i = remaining; i >= 0; i--) {                     /* dbf d1,outer */
            for (j = OFFER_ENTRY_SIZE - 1; j >= 0; j--) {     /* dbf d0,inner: move.w #$D,d0 */
                *dst = *(dst + OFFER_ENTRY_SIZE);              /* move.b $E(a0),(a0)+ */
                dst++;
            }
        }
    }

    numOffers--;                                                /* subq.w #1,(numOffers) */
}

/* ================================================================
 * PrunePlayerOffersAgainstRosterLimits  (SWOS2.s line 72734)
 *
 * Iterates through transfer offers and removes any that would
 * violate roster/slot limits:
 * Phase 1: Remove offers that exceed swap or pool slot limits.
 * Phase 2: If roster is full (>= 32), remove offers that buy
 *          players without selling (byte 3 negative + byte 5 positive).
 *
 * Transfer offer entry layout (14 bytes):
 *   +3: offered player ordinal (bit 7 set = no player)
 *   +4: secondary player (bit 7 set = none)
 *   +5: extra player (bit 7 set = none)
 * ================================================================ */
__attribute__((used, externally_visible))
void _PrunePlayerOffersAgainstRosterLimits(void)
{
    uint16_t swapSlots, poolSlots;
    uint16_t idx;
    uint8_t *offer;

    /* Phase 1: Check swap slot and pool slot limits */
restart_phase1:
    if (numOffers == 0)
        goto phase2;

    swapSlots = _GetRemainingOfferSwapSlots();
    if (swapSlots == 0) {
        numOffers = 0;                              /* no swap slots → clear all offers */
        goto phase2;
    }

    poolSlots = _GetRemainingTransferPoolSlots();
    idx = 0;
    offer = transferOfferTable;

    while (idx != numOffers) {
        /* Check if offer byte 3 is non-negative (player being offered) */
        if ((int8_t)offer[3] >= 0) {
            if (swapSlots == 1)                     /* only 1 swap slot left → remove */
                goto remove_phase1;
        }

        /* Check if offer byte 4 is non-negative (secondary player) */
        if ((int8_t)offer[4] >= 0) {
            if (poolSlots == 0)                     /* no pool slots → remove */
                goto remove_phase1;
            if (poolSlots < 2) {                    /* only 1 pool slot */
                if ((int8_t)offer[5] >= 0)          /* and byte 5 also non-negative → remove */
                    goto remove_phase1;
            }
        }

        /* Keep this offer, advance */
        offer += OFFER_ENTRY_SIZE;
        idx++;
        continue;

    remove_phase1:
        _RemoveTransferOfferAtIndex(idx);
        goto restart_phase1;                        /* restart scan after removal */
    }

    /* Phase 2: If career roster full (>= 32 players), remove buy-only offers */
phase2:
    if (careerNumPlayers < 0x20)
        return;

restart_phase2:
    if (numOffers == 0)
        return;

    idx = 0;
    offer = transferOfferTable;

    while (idx != numOffers) {
        /* Remove offers where byte 5 >= 0 (buying) AND byte 3 < 0 (not selling) */
        if ((int8_t)offer[5] >= 0 && (int8_t)offer[3] < 0) {
            _RemoveTransferOfferAtIndex(idx);
            goto restart_phase2;
        }

        offer += OFFER_ENTRY_SIZE;
        idx++;
    }
}

/* ================================================================
 * IsPlayerInCareerContest  (SWOS2.s line 73096)
 *
 * Searches homeMarketPlayers for a (teamNumber, playerOrdinal) pair.
 * Returns the index if found, or -1 if not found.
 *
 * Asm register convention:
 *   in:  d0 = teamNumber, d1 = playerOrdinal
 *   out: Z flag (set = found, clear = not found), d2 = index, a1 = entry pointer
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _IsPlayerInCareerContest(uint16_t teamNumber, uint16_t playerOrdinal)
{
    uint8_t *entry;
    uint16_t i;

    if (numHomeMarketPlayers == 0)
        return -1;

    entry = homeMarketPlayers;
    for (i = 0; i < numHomeMarketPlayers; i++) {
        if (*(uint16_t *)entry == teamNumber && entry[2] == (uint8_t)playerOrdinal)
            return (int16_t)i;                      /* found at index i */
        entry += MARKET_ENTRY_SIZE;
    }

    return -1;                                      /* not found */
}

/* ================================================================
 * AddPlayerToCareerContest  (SWOS2.s line 73053)
 *
 * Appends a player to the homeMarketPlayers array if not full.
 * Returns non-zero if added, 0 if full (matches Z flag convention).
 *
 * Asm register convention:
 *   in:  d0 = teamNumber, d1 = playerOrdinal
 *   out: Z flag (set = full/can't add, clear = added OK)
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _AddPlayerToCareerContest(uint16_t teamNumber, uint16_t playerOrdinal)
{
    uint8_t *entry;
    uint32_t offset;

    if (numHomeMarketPlayers >= HOME_MARKET_MAX)
        return 0;                                   /* full */

    offset = (uint32_t)numHomeMarketPlayers * MARKET_ENTRY_SIZE;
    entry = homeMarketPlayers + offset;
    *(uint16_t *)entry = teamNumber;                /* move.w d0,(a1) */
    entry[2] = (uint8_t)playerOrdinal;              /* move.b d1,2(a1) */
    numHomeMarketPlayers++;                          /* addq.w #1 → clears Z */

    return 1;                                       /* added OK (non-zero → Z clear) */
}

/* ================================================================
 * RemovePlayerFromCareerContest  (SWOS2.s line 73068)
 *
 * Removes a player from homeMarketPlayers by searching for it
 * then shifting subsequent entries left.
 *
 * Asm register convention:
 *   in:  d0 = teamNumber, d1 = playerOrdinal
 * ================================================================ */
__attribute__((used, externally_visible))
void _RemovePlayerFromCareerContest(uint16_t teamNumber, uint16_t playerOrdinal)
{
    int16_t idx = _IsPlayerInCareerContest(teamNumber, playerOrdinal);

    if (idx < 0)
        return;                                     /* not found */

    /* If not the last entry, shift remaining entries left */
    if ((uint16_t)(idx + 1) != numHomeMarketPlayers) {
        uint16_t entriesToMove = numHomeMarketPlayers - (uint16_t)(idx + 1);
        volatile uint32_t *ldst = (volatile uint32_t *)(homeMarketPlayers + (uint16_t)idx * MARKET_ENTRY_SIZE);
        volatile uint32_t *lsrc = ldst + 1;
        uint16_t i;
        /* Each entry is 4 bytes = 1 longword; use volatile to prevent memmove */
        for (i = 0; i < entriesToMove; i++) {
            ldst[i] = lsrc[i];
        }
    }

    numHomeMarketPlayers--;
}

/* ================================================================
 * FindForeignMarketPlayer  (SWOS2.s line 73264)
 *
 * Searches foreignMarketPlayers for a (teamId, playerOrdinal) pair.
 * Returns the index if found, or -1 if not found.
 *
 * Asm register convention:
 *   in:  d0 = teamId, d1 = playerOrdinal
 *   out: Z flag (set = found, clear = not found), d2 = index, a1 = entry pointer
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _FindForeignMarketPlayer(uint16_t teamId, uint16_t playerOrdinal)
{
    uint8_t *entry;
    uint16_t i;

    if (numForeignMarketPlayers == 0)
        return -1;

    entry = foreignMarketPlayers;
    for (i = 0; i < numForeignMarketPlayers; i++) {
        if (*(uint16_t *)entry == teamId && entry[2] == (uint8_t)playerOrdinal)
            return (int16_t)i;                      /* found at index i */
        entry += MARKET_ENTRY_SIZE;
    }

    return -1;                                      /* not found */
}

/* ================================================================
 * AddForeignMarketPlayer  (SWOS2.s line 73227)
 *
 * Appends a player to the foreignMarketPlayers array if not full.
 * Returns non-zero if added, 0 if full.
 *
 * Asm register convention:
 *   in:  d0 = teamId, d1 = playerOrdinal
 *   out: Z flag (set = full, clear = added OK)
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _AddForeignMarketPlayer(uint16_t teamId, uint16_t playerOrdinal)
{
    uint8_t *entry;
    uint32_t offset;

    if (numForeignMarketPlayers >= FOREIGN_MARKET_MAX)
        return 0;                                   /* full */

    offset = (uint32_t)numForeignMarketPlayers * MARKET_ENTRY_SIZE;
    entry = foreignMarketPlayers + offset;
    *(uint16_t *)entry = teamId;                    /* move.w d0,(a1) */
    entry[2] = (uint8_t)playerOrdinal;              /* move.b d1,2(a1) */
    numForeignMarketPlayers++;

    return 1;                                       /* added OK */
}

/* ================================================================
 * RemoveForeignMarketPlayer  (SWOS2.s line 73242)
 *
 * Removes a player from foreignMarketPlayers by searching for it
 * then shifting subsequent entries left.
 *
 * Asm register convention:
 *   in:  d0 = teamId, d1 = playerOrdinal
 * ================================================================ */
__attribute__((used, externally_visible))
void _RemoveForeignMarketPlayer(uint16_t teamId, uint16_t playerOrdinal)
{
    int16_t idx = _FindForeignMarketPlayer(teamId, playerOrdinal);

    if (idx < 0)
        return;                                     /* not found */

    /* If not the last entry, shift remaining entries left */
    if ((uint16_t)(idx + 1) != numForeignMarketPlayers) {
        uint16_t entriesToMove = numForeignMarketPlayers - (uint16_t)(idx + 1);
        volatile uint32_t *ldst = (volatile uint32_t *)(foreignMarketPlayers + (uint16_t)idx * MARKET_ENTRY_SIZE);
        volatile uint32_t *lsrc = ldst + 1;
        uint16_t i;
        /* Each entry is 4 bytes = 1 longword; use volatile to prevent memmove */
        for (i = 0; i < entriesToMove; i++) {
            ldst[i] = lsrc[i];
        }
    }

    numForeignMarketPlayers--;
}

/* ================================================================
 * FindPlayerInCareerOrForeignMarket  (SWOS2.s line 73286)
 *
 * Searches for a player in both the home career contest and the
 * foreign market. Returns 0 if found in either, 1 if not found.
 *
 * Asm register convention:
 *   in:  d0 = teamId, d1 = playerOrdinal
 *   out: Z flag (set = found, clear = not found)
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _FindPlayerInCareerOrForeignMarket(uint16_t teamId, uint16_t playerOrdinal)
{
    if (_IsPlayerInCareerContest(teamId, playerOrdinal) >= 0)
        return 0;                                   /* found in home market → Z set */

    if (_FindForeignMarketPlayer(teamId, playerOrdinal) >= 0)
        return 0;                                   /* found in foreign market → Z set */

    return 1;                                       /* not found → Z clear */
}

/* ================================================================
 * RemovePlayerFromTransferOffers  (SWOS2.s line 72807)
 *
 * Removes all transfer offers involving a specific player
 * (as primary, secondary, or counter-offer player).
 *
 * Asm register convention:
 *   in:  d0 = teamNumber, d1.b = playerOrdinal
 * ================================================================ */
__attribute__((used, externally_visible))
void _RemovePlayerFromTransferOffers(uint16_t teamNumber, uint16_t playerOrdinal)
{
    uint8_t ord = (uint8_t)playerOrdinal;
    uint16_t idx;
    uint8_t *offer;

restart:
    if (numOffers == 0)
        return;

    idx = 0;
    offer = transferOfferTable;

    while (idx != numOffers) {
        /* Check if this is an offer from selTeamNumber */
        if (teamNumber == selTeamNumber) {
            /* Check bytes 2 and 3 (offered players) */
            if (offer[2] == ord || offer[3] == ord) {
                _RemoveTransferOfferAtIndex(idx);
                goto restart;
            }
        }

        /* Check if offer is TO this team (offer[0..1] = teamNumber word) */
        if (*(uint16_t *)offer == teamNumber) {
            /* Check bytes 4 and 5 (counter-offer/swap players) */
            if (offer[4] == ord || offer[5] == ord) {
                _RemoveTransferOfferAtIndex(idx);
                goto restart;
            }
        }

        offer += OFFER_ENTRY_SIZE;
        idx++;
    }
}

/* ================================================================
 * FillForeignMarketPlayersData  (SWOS2.s line 71546)
 *
 * For each foreign market player in careerForeignMarketPlayers,
 * finds the matching entry in poolplyrDatBuffer and copies
 * 38 bytes (19 words) of player data from market entry to pool entry.
 *
 * Only runs in career mode.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FillForeignMarketPlayersData(void)
{
    uint16_t count, i, j;
    uint8_t countrySel;
    MarketPlayer *marketEntry;
    PoolPlayer *poolEntry;

    if (gameType != GAME_TYPE_CAREER)
        return;

    count = numForeignMarketPlayers2;
    if (count == 0)
        return;

    countrySel = (uint8_t)selTeamNumber;        /* move.b (selTeamNumber).l,d5 */
    marketEntry = careerForeignMarketPlayers;

    for (i = count - 1; ; i--) {
        /* Skip if same country as selected team */
        if (*(uint8_t *)&marketEntry->teamId != countrySel) {
            uint16_t teamId = marketEntry->teamId;
            int8_t playerOrd = marketEntry->playerOrdinal;

            /* Search poolplyrDatBuffer for matching player */
            poolEntry = poolplyrDatBuffer;
            for (j = POOLPLAYER_COUNT - 1; ; j--) {
                if (poolEntry->teamId == teamId &&
                    poolEntry->playerOrdinal == playerOrd) {
                    /* Found match: copy PlayerFile (19 words) from market to pool entry */
                    volatile uint16_t *src = (volatile uint16_t *)&marketEntry->player;
                    volatile uint16_t *dst = (volatile uint16_t *)&poolEntry->player;
                    int16_t k;
                    for (k = 18; k >= 0; k--) {  /* move.w #18,d0; dbf loop */
                        *dst++ = *src++;
                    }
                    break;
                }
                poolEntry++;
                if (j == 0)
                    break;
            }
        }

        marketEntry++;
        if (i == 0)
            break;
    }
}

/* ================================================================
 * LoadPoolplyrDat  (SWOS2.s line 72843)
 *
 * Loads the pool player database file if not already loaded,
 * then calls FillForeignMarketPlayersData.
 * ================================================================ */
__attribute__((used, externally_visible))
void _LoadPoolplyrDat(void)
{
    if (poolplyrLoaded == 0) {
        uint32_t fileSize;
        poolplyrLoaded = 1;
        _LoadFileEx(aDataPoolplyrDa, (uint8_t *)poolplyrDatBuffer, &fileSize);
    }
    _FillForeignMarketPlayersData();
}

/* ================================================================
 * FindPoolplyrDatPlayer  (SWOS2.s line 72856)
 *
 * Searches poolplyrDatBuffer for a player matching (teamId, playerOrdinal).
 * Scans PoolPlayer entries starting from poolPlayerStrideRef + 1.
 *
 * WARNING: No bounds check — loops until match found (assumes match exists).
 *
 * Asm register convention:
 *   in:  d0 = teamId, d1.b = playerOrdinal
 *   out: a0 = pointer to matching pool entry
 * ================================================================ */
__attribute__((used, externally_visible))
PoolPlayer *_FindPoolplyrDatPlayer(uint16_t teamId, uint16_t playerOrdinal)
{
    PoolPlayer *entry = poolPlayerStrideRef;

    for (;;) {
        entry++;                                /* lea 58(a0),a0 (advance first) */
        if (entry->teamId == teamId &&
            entry->playerOrdinal == (int8_t)playerOrdinal)
            return entry;
    }
}

/* ================================================================
 * AddCareerTeamPlayer  (SWOS2.s line 72936)
 *
 * Selects a random player from a random non-home team among the
 * selected teams and adds them to the home market (career contest).
 * Uses price range filtering and duplicate checking.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AddCareerTeamPlayer(void)
{
    uint32_t priceRange = _GetLowestAndHighestPlayerPrice();
    uint8_t lowestPrice = (uint8_t)(priceRange >> 16);     /* d4 = lowest */
    uint8_t highestPrice = (uint8_t)(priceRange & 0xFFFF); /* d5 = highest */
    uint8_t homeCountry;
    uint16_t attemptsLeft;
    uint16_t rand;
    uint16_t teamIdx;
    uint8_t *teamEntry;
    uint16_t teamNum;
    uint16_t playerOrd;

    highestPrice += 3;                                      /* addq.w #3,d5 */
    homeCountry = (uint8_t)selTeamNumber;                   /* move.b (selTeamNumber).l,d3 */

    attemptsLeft = 1;                                       /* move.w #1,d6 */
    rand = _Rand2();
    if ((rand & 1) == 0) {                                  /* btst #0,d0; beq */
        attemptsLeft = numSelectedTeams >> 1;               /* lsr.w #1,d6 */
    }

retry:
    attemptsLeft--;
    if (attemptsLeft == 0) {                                /* tst.w d6; bne skip */
        homeMarketRandomPickRetries++;
    }

    /* Pick a random team from selected teams */
    rand = _Rand2();
    teamIdx = (uint16_t)(((uint32_t)rand * numSelectedTeams) >> 8);  /* mulu+lsr */
    teamIdx = (uint16_t)((uint32_t)teamIdx * sizeof(TeamFileEx));
    teamEntry = &selTeamsPtr + teamIdx;

    /* Must be same country but different team */
    if (*teamEntry != homeCountry)                          /* cmp.b (a0),d3 */
        goto retry;
    teamNum = *(uint16_t *)teamEntry;                       /* move.w (a0),d1 */
    if (teamNum == selTeamNumber)                            /* cmp.w (selTeamNumber).l,d1 */
        goto retry;

    /* Pick a random player (0-15) */
    rand = _Rand2();
    playerOrd = rand & 0xF;                                 /* andi.w #$F,d0 */
    /* Now d0 = teamNum, d1 = playerOrd (after exg) */

    /* If not first attempt (attemptsLeft was > 0), check price range */
    if (attemptsLeft != 0) {
        TeamFileEx *teamEx = (TeamFileEx *)teamEntry;
        uint8_t price = (uint8_t)teamEx->players[playerOrd].price;
        if (lowestPrice > price)                            /* cmp.b ... bhi retry */
            goto retry;
        if (highestPrice < price)                           /* cmp.b ... bcs retry */
            goto retry;
        /* Check position bit 2 (goalkeeper flag?) */
        if (teamEx->players[playerOrd].positionAndFace & 0x04)
            goto retry;                                     /* btst #2,... bne retry */
    }

    /* Check if player is already in contest */
    if (_IsPlayerInCareerContest(teamNum, playerOrd) >= 0)
        goto retry;                                         /* beq retry (Z set = found) */

    _AddPlayerToCareerContest(teamNum, playerOrd);
}

/* ================================================================
 * FillCareerTeamPlayers  (SWOS2.s line 72868)
 *
 * Populates the home market with random players from selected teams.
 * Number of players is based on numSelectedTeams, with random reduction.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FillCareerTeamPlayers(void)
{
    uint16_t numToAdd;
    uint16_t rand;

    numHomeMarketPlayers = 0;

    if (isNationalSelector != 0)
        return;

    numToAdd = (uint16_t)(((uint32_t)numSelectedTeams * 16) / 40);  /* mulu #$10; divu #$28 */
    if (numToAdd == 0)
        return;

    rand = _Rand2();
    rand &= 0x1F;
    numToAdd -= (uint16_t)(((uint32_t)rand * numToAdd) >> 8);       /* mulu d7,d0; lsr.l #8 */

    if (numToAdd > 25) {                                    /* cmpi.w #$19,d7 */
        numToAdd = 25;
        rand = _Rand2();
        numToAdd -= (rand & 7);                             /* andi.w #7,d0; sub.w */
    }

    {
        int16_t i;
        for (i = (int16_t)numToAdd - 1; i >= 0; i--) {     /* subq.w #1; dbf */
            _AddCareerTeamPlayer();
        }
    }
}

/* ================================================================
 * AddRandomForeignMarketPlayer  (SWOS2.s line 73108)
 *
 * Adds a random foreign player to the foreign market from the
 * pool player database. 25% chance to enforce price range.
 * ================================================================ */
__attribute__((used, externally_visible))
void _AddRandomForeignMarketPlayer(void)
{
    uint32_t priceRange = _GetLowestAndHighestPlayerPrice();
    uint8_t lowestPrice = (uint8_t)(priceRange >> 16);     /* d4 */
    uint8_t highestPrice = (uint8_t)(priceRange & 0xFFFF); /* d5 */
    uint8_t homeCountry;
    uint16_t attemptsLeft;
    uint16_t rand;
    uint16_t poolIdx;
    PoolPlayer *poolEntry;
    uint16_t teamId;
    uint8_t playerOrd;

    highestPrice += 3;                                      /* addq.w #3,d5 */
    homeCountry = (uint8_t)selTeamNumber;

    attemptsLeft = 1;                                       /* move.w #1,d6 */
    rand = _Rand2();
    if ((rand & 3) != 0)                                    /* andi.w #3,d0; bne */
        attemptsLeft = 0xB4;                                /* move.w #$B4,d6 */

retry:
    attemptsLeft--;
    if (attemptsLeft == 0)                                  /* tst.w d6; bne skip */
        foreignMarketRandomPickRetries++;

    /* Pick random pool entry using GetRandomWord for full 16-bit random */
    rand = _GetRandomWord();
    poolIdx = (uint16_t)((uint32_t)rand * POOLPLAYER_COUNT >> 16);  /* mulu #900; lsr.l #8; lsr.l #8 */
    poolEntry = &poolplyrDatBuffer[poolIdx];

    /* Must be different country */
    if (*(uint8_t *)&poolEntry->teamId == homeCountry)      /* cmp.b (a0),d3 */
        goto retry;

    teamId = poolEntry->teamId;
    playerOrd = (uint8_t)poolEntry->playerOrdinal;

    /* If attempts remain (price filtering), check price range */
    if (attemptsLeft != 0) {
        uint8_t price = (uint8_t)poolEntry->player.price;   /* player.price at PoolPlayer offset 52 */
        if (lowestPrice > price)                             /* bhi retry */
            goto retry;
        if (highestPrice < price)                            /* bcs retry */
            goto retry;
    }

    /* Check if already in foreign market */
    if (_FindForeignMarketPlayer(teamId, playerOrd) >= 0)
        goto retry;                                         /* beq retry */

    _AddForeignMarketPlayer(teamId, playerOrd);
}

/* ================================================================
 * FillForeignMarketPlayers  (SWOS2.s line 73048)
 *
 * Populates the foreign market with random players from the pool
 * player database. Adds 8-15 random foreign players.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FillForeignMarketPlayers(void)
{
    uint16_t numToAdd;
    int16_t i;

    _LoadPoolplyrDat();

    numForeignMarketPlayers = 0;

    if (isNationalSelector != 0)
        return;

    numToAdd = 15;                                          /* move.w #$F,d7 */
    numToAdd -= (_Rand2() & 7);                             /* andi.w #7,d0; sub.w */
    if ((int16_t)numToAdd <= 0)
        return;

    for (i = (int16_t)numToAdd - 1; i >= 0; i--) {         /* subq.w #1; dbf */
        _AddRandomForeignMarketPlayer();
    }
}

/* ================================================================
 * UpdateHomeMarketPlayers  (SWOS2.s line 72897)
 *
 * Randomly removes and adds players in the home market.
 * Number of changes is based on current market size.
 * Re-enters itself until homeMarketUpdateBacklog reaches 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateHomeMarketPlayers(void)
{
restart:
    if (isNationalSelector != 0)
        return;
    if (homeMarketUpdateBacklog == 0)
        return;

    homeMarketUpdateBacklog--;

    {
        uint16_t numChanges = numHomeMarketPlayers;
        uint16_t numRemoves;
        int16_t i;
        uint16_t rand;

        if (numChanges == 0)
            return;

        numChanges += 15;
        numChanges >>= 4;                                  /* lsr.w #4 */
        if (numChanges == 0)
            return;

        /* Remove phase: randomly pick and remove players */
        numRemoves = numChanges;
        for (i = (int16_t)numRemoves - 1; i >= 0; i--) {
            if (numHomeMarketPlayers == 0)
                goto add_phase;
            rand = _Rand2();
            {
                uint16_t pickIdx = (uint16_t)(((uint32_t)rand * numHomeMarketPlayers) >> 8);
                uint8_t *entry = homeMarketPlayers + (uint32_t)pickIdx * MARKET_ENTRY_SIZE;
                uint16_t teamNum = *(uint16_t *)entry;
                uint8_t playerOrd = entry[2];
                _RemovePlayerFromCareerContest(teamNum, playerOrd);
            }
        }

    add_phase:
        /* Add phase: add random players until full or done */
        for (i = (int16_t)numChanges - 1; i >= 0; i--) {
            if (numHomeMarketPlayers >= HOME_MARKET_MAX)
                goto restart;                               /* beq.s → re-enter */
            _AddCareerTeamPlayer();
        }
        goto restart;                                       /* bra.s UpdateHomeMarketPlayers */
    }
}

/* ================================================================
 * UpdateForeignMarketPlayers  (SWOS2.s line 73067)
 *
 * Randomly removes and adds players in the foreign market.
 * Number of changes is based on current market size.
 * Re-enters itself until foreignMarketUpdateBacklog reaches 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateForeignMarketPlayers(void)
{
restart:
    if (isNationalSelector != 0)
        return;
    if (foreignMarketUpdateBacklog == 0)
        return;

    foreignMarketUpdateBacklog--;

    {
        uint16_t numChanges = numForeignMarketPlayers;
        uint16_t numRemoves;
        int16_t i;
        uint16_t rand;

        if (numChanges == 0)
            return;

        numChanges += 15;
        numChanges >>= 4;
        if (numChanges == 0)
            return;

        /* Remove phase */
        numRemoves = numChanges;
        for (i = (int16_t)numRemoves - 1; i >= 0; i--) {
            if (numForeignMarketPlayers == 0)
                goto add_phase;
            rand = _Rand2();
            {
                uint16_t pickIdx = (uint16_t)(((uint32_t)rand * numForeignMarketPlayers) >> 8);
                uint8_t *entry = foreignMarketPlayers + (uint32_t)pickIdx * MARKET_ENTRY_SIZE;
                uint16_t teamId = *(uint16_t *)entry;
                uint8_t playerOrd = entry[2];
                _RemoveForeignMarketPlayer(teamId, playerOrd);
            }
        }

    add_phase:
        /* Add phase */
        for (i = (int16_t)numChanges - 1; i >= 0; i--) {
            if (numForeignMarketPlayers >= FOREIGN_MARKET_MAX)
                goto restart;
            _AddRandomForeignMarketPlayer();
        }
        goto restart;
    }
}

/* ================================================================
 * FindTeamIdInDiyTeamsList  (SWOS2.s line 78693)
 *
 * Searches the DIY team ID list for a team number.
 * List selected by competition type (0/1/2 → diyTeamIdListType0/1/2).
 * Returns 0 if found, non-zero if not found (maps to Z flag in asm).
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _FindTeamIdInDiyTeamsList(uint16_t *teamIdPtr)
{
    uint16_t *list;
    int16_t count;
    uint16_t target;

    if (competitionFileBuffer == 1)
        list = diyTeamIdListType1;
    else if (competitionFileBuffer == 2)
        list = diyTeamIdListType2;
    else
        list = diyTeamIdListType0;

    count = (int16_t)(numDiyTeams - 1);
    target = *teamIdPtr;

    for (; count >= 0; count--) {
        if (*list++ == target)
            return 0;  /* found */
    }
    return 1;  /* not found */
}

/* ================================================================
 * IsOfferTeamPlayerPairUnique  (SWOS2.s line 79294)
 *
 * Checks whether (teamId, playerIdx) pair already exists in the
 * transfer offer table. Compares teamId as word, playerIdx as byte
 * against offer bytes 2 (primary) and 3 (extra).
 * Returns non-zero if unique (no match), 0 if duplicate.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _IsOfferTeamPlayerPairUnique(uint16_t teamId, uint16_t playerIdx)
{
    int16_t count = (int16_t)numOffers;
    uint8_t *entry;
    int16_t i;
    uint8_t pidx = (uint8_t)playerIdx;

    if (count == 0)
        return 1;  /* no offers → unique */

    entry = transferOfferTable;
    for (i = count - 1; i >= 0; i--) {
        if (*(uint16_t *)entry == teamId) {
            if (entry[2] == pidx)
                return 0;  /* duplicate */
            if (entry[3] == pidx)
                return 0;  /* duplicate */
        }
        entry += OFFER_ENTRY_SIZE;
    }
    return 1;  /* unique */
}

/* ================================================================
 * IsTeamEligibleForOfferByRefSkill  (SWOS2.s line 79189)
 *
 * Checks if a team has players spanning the reference skill level.
 * Eligible if: any player has exact match, OR team has players both
 * stronger AND weaker than ref skill.
 * Not eligible if: team == selected team, OR league type == 4.
 * Returns 0 if eligible, non-zero if not (maps to Z flag in asm).
 * team points to a team file entry (team number at offset 0,
 * league type at 0x19, player skills at 0x6C with 0x26 stride).
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _IsTeamEligibleForOfferByRefSkill(uint8_t *team, uint16_t refSkill)
{
    uint16_t teamNum = *(uint16_t *)team;
    uint16_t flags;
    uint8_t *p;
    int16_t i;
    uint8_t ref = (uint8_t)refSkill;

    /* Same as selected team → not eligible */
    if (teamNum == selTeamNumber)
        return 1;

    /* League type 4 → not eligible */
    if (*(team + 0x19) == 4)
        return 1;

    flags = 0;
    p = team;
    for (i = 15; i >= 0; i--) {
        uint8_t skill = *(p + 0x6C);
        if (skill == ref)
            return 0;  /* exact match → eligible */
        if (ref > skill)
            flags |= 2;  /* player weaker than ref */
        if (ref < skill)
            flags |= 1;  /* player stronger than ref */
        if (flags == 3)
            return 0;  /* skill range spans ref → eligible */
        p += 0x26;  /* next player (38 bytes) */
    }
    return 1;  /* not eligible */
}

/* ================================================================
 * RemoveOfferPlayerFromMarketsAndCount  (SWOS2.s line 79607)
 *
 * Removes a player from both career contest and foreign market,
 * then increments the appropriate removal counter (selected team
 * or other teams) based on country match.
 * ================================================================ */
__attribute__((used, externally_visible))
void _RemoveOfferPlayerFromMarketsAndCount(uint16_t teamId, uint16_t playerIndex)
{
    _RemovePlayerFromCareerContest(teamId, playerIndex);
    _RemoveForeignMarketPlayer(teamId, playerIndex);

    if ((teamId >> 8) == (selTeamNumber >> 8))
        removedOfferPlayersFromSelectedTeamCount++;
    else
        removedOfferPlayersFromOtherTeamsCount++;
}

/* ================================================================
 * RemoveNegotiatedOfferPlayersFromMarkets  (SWOS2.s line 79592)
 *
 * Removes the primary (and optionally extra) offer player from
 * both career contest and foreign market, counting removals.
 * Uses globals: teamOffering, offerPrimaryPlayerOrdinal,
 * numExtraPlayersWanted.
 * ================================================================ */
__attribute__((used, externally_visible))
void _RemoveNegotiatedOfferPlayersFromMarkets(void)
{
    _RemoveOfferPlayerFromMarketsAndCount(teamOffering, offerPrimaryPlayerOrdinal);
    if ((int16_t)numExtraPlayersWanted >= 0)
        _RemoveOfferPlayerFromMarketsAndCount(teamOffering, (uint16_t)numExtraPlayersWanted);
}

/* ================================================================
 * PickRandomEligibleExtraPlayerOrdinal  (SWOS2.s line 79219)
 *
 * Picks a random eligible extra player from the selected team.
 * Player must: not be the primary offer player, not on transfer
 * list, not sold/transferred, and have skill <= refSkill.
 * Returns file index (from GetPlayerAtIndex) or -1 if not found.
 * Up to 16 random attempts.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _PickRandomEligibleExtraPlayerOrdinal(uint16_t refSkill)
{
    int16_t retries;
    uint8_t ref = (uint8_t)refSkill;

    for (retries = 15; retries >= 0; retries--) {
        uint16_t rand = _Rand2();
        uint16_t ordinal = (uint16_t)(((uint32_t)rand * careerNumPlayers) >> 8);
        uint8_t *player;
        uint16_t fileIdx;

        player = _CGetPlayerAtIndex(selTeamNumber, ordinal);
        fileIdx = _cGetPlayerAtIndexFileIdx;

        /* Skip if same as primary player */
        if (fileIdx == offerPrimaryPlayerOrdinal)
            continue;

        /* Skip if on transfer list (bit 1 of byte at offset 0x66) */
        if (player[0x66] & 0x02)
            continue;

        /* Skip if sold (0x9B) or transferred (0x9A) */
        if (player[0x71] == 0x9B)
            continue;
        if (player[0x71] == 0x9A)
            continue;

        /* Skill must be <= refSkill (unsigned) */
        if (player[0x6C] > ref)
            continue;

        return (int16_t)fileIdx;
    }
    return -1;  /* not found */
}

/* ================================================================
 * PickRandomEligibleSwapPlayerOrdinalFromTeam  (SWOS2.s line 79246)
 *
 * Phase 1: Scans selected team to find min/max skill range.
 * Phase 2: Picks a random player (0-15) from the offering team
 * whose skill falls within [min, max] of the selected team.
 * Player must not: be excluded slot, be on transfer list,
 * be sold/transferred, or have skill outside the range.
 * Returns player index in offering team or -1 if not found.
 * Up to 16 random attempts in Phase 2.
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _PickRandomEligibleSwapPlayerOrdinalFromTeam(uint8_t *teamBlock, int16_t excludeSlot)
{
    uint8_t minSkill = 0xFF;
    uint8_t maxSkill = 0;
    int16_t i;
    int16_t retries;

    /* Phase 1: Find min/max skill in selected team */
    for (i = (int16_t)(careerNumPlayers - 1); i >= 0; i--) {
        uint8_t *player = _CGetPlayerAtIndex(selTeamNumber, (uint16_t)i);
        uint8_t skill = player[0x6C];
        if (skill < minSkill)
            minSkill = skill;
        if (skill > maxSkill)
            maxSkill = skill;
    }

    /* Phase 2: Pick random player from offering team within skill range */
    for (retries = 15; retries >= 0; retries--) {
        uint16_t rand = _Rand2();
        uint16_t idx = (uint16_t)((rand * 16) >> 8);
        uint8_t *player;
        uint8_t skill;

        if ((int16_t)idx == excludeSlot)
            continue;

        player = teamBlock + (uint32_t)idx * 0x26;

        /* Skip if on transfer list */
        if (player[0x66] & 0x02)
            continue;

        /* Skip if sold or transferred */
        if (player[0x71] == 0x9B)
            continue;
        if (player[0x71] == 0x9A)
            continue;

        /* Must be within selected team's skill range */
        skill = player[0x6C];
        if (skill < minSkill)
            continue;
        if (skill > maxSkill)
            continue;

        return (int16_t)idx;
    }
    return -1;  /* not found */
}

/* ================================================================
 * FindPlayerOnTransferMarket  (SWOS2.s line 73022)
 *
 * Linear search in playersOnTransferMarket[] for playerId.
 *
 * Asm register convention:
 *   in:  d1.w = playerId
 *   out: d0.w = 0 (found, Z=1), 1 (not found, Z=0)
 *        d2.w = index of found entry  (only valid when found)
 *        a1   = pointer to found entry (only valid when found)
 *
 * C returns: 0 = found, 1 = not found
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _FindPlayerOnTransferMarket(uint16_t playerId)
{
    uint16_t count = numPlayersOnTransferMarket;
    uint16_t *p;
    uint16_t i;

    if (count == 0)
        return 1;

    p = playersOnTransferMarket;
    for (i = 0; i < count; i++) {
        if (p[i] == playerId)
            return 0;   /* found */
    }
    return 1;   /* not found */
}

/* ================================================================
 * AddPlayerToTransferMarket  (SWOS2.s line 72992)
 *
 * Appends a playerId to the transfer market list (max 5 entries).
 *
 * Asm register convention:
 *   in:  d0.w = playerId
 * ================================================================ */
__attribute__((used, externally_visible))
void _AddPlayerToTransferMarket(uint16_t playerId)
{
    uint16_t count;

    if (numPlayersOnTransferMarket >= 5)
        return;

    count = numPlayersOnTransferMarket;
    playersOnTransferMarket[count] = playerId;
    numPlayersOnTransferMarket = count + 1;
}

/* ================================================================
 * RemovePlayerFromTransferMarket  (SWOS2.s line 73005)
 *
 * Finds and removes a playerId from the transfer market list.
 * Uses shift-left to compact the array.
 *
 * Asm register convention:
 *   in:  d1.w = playerId
 * ================================================================ */
__attribute__((used, externally_visible))
void _RemovePlayerFromTransferMarket(uint16_t playerId)
{
    uint16_t count = numPlayersOnTransferMarket;
    uint16_t *p = playersOnTransferMarket;
    uint16_t i;
    int16_t foundIdx = -1;

    /* Find the player */
    if (count == 0)
        return;
    for (i = 0; i < count; i++) {
        if (p[i] == playerId) {
            foundIdx = (int16_t)i;
            break;
        }
    }
    if (foundIdx < 0)
        return;

    /* Shift entries left to fill gap */
    {
        uint16_t remaining = count - (uint16_t)foundIdx - 1;
        volatile uint16_t *dst = (volatile uint16_t *)&p[foundIdx];
        uint16_t j;
        for (j = 0; j < remaining; j++)
            dst[j] = dst[j + 1];
    }
    numPlayersOnTransferMarket = count - 1;
}

/* ================================================================
 * CheckForTransferOffers  (SWOS2.s line 72748)
 *
 * Searches transferOfferTable for any offer with byte 0x0D == 0.
 *
 * Asm register convention:
 *   out: d0.w = 0 (got offers, Z=1), 1 (no offers, Z=0)
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _CheckForTransferOffers(void)
{
    uint16_t count = numOffers;
    uint8_t *p;
    uint16_t i;

    if (count == 0)
        return 1;

    p = transferOfferTable;
    for (i = 0; i < count; i++) {
        if (p[0x0D] == 0)
            return 0;   /* found an active offer */
        p += OFFER_ENTRY_SIZE;
    }
    return 1;   /* no active offers */
}

/* ================================================================
 * Career_CommitNewBalanceAndResetFinanceTotals  (SWOS2.s line 86770)
 *
 * If financeTotalsNeedResetFlag is set, clears it, copies newBalance
 * to old_balance, and zeroes all financial tracking variables.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_CommitNewBalanceAndResetFinanceTotals(void)
{
    if (financeTotalsNeedResetFlag == 0)
        return;

    financeTotalsNeedResetFlag = 0;
    old_balance = newBalance;
    playerPurchases = 0;
    playerSales = 0;
    playerWagesBill = 0;
    generalRunningCosts = 0;
    gateReceipts = 0;
    startingBalanceCopy = 0;
    competitionBonuses = 0;
}

/* ================================================================
 * AddCareerPoolPlayer  (SWOS2.s line 71391)
 *
 * Adds a player to careerForeignMarketPlayers pool.
 * Entry: 42 bytes (word teamNum + byte playerOrd + byte randomPhase
 *        + 38 bytes player data from players[]).
 *
 * Asm register convention:
 *   in:  d0.w = teamNumber, d1.b = playerOrdinal, a0 = teamFilePtr
 * ================================================================ */
__attribute__((used, externally_visible))
void _AddCareerPoolPlayer(uint16_t teamNumber, uint16_t playerOrdinal, uint8_t *teamFilePtr)
{
    uint16_t idx = numForeignMarketPlayers2;
    MarketPlayer *entry = &careerForeignMarketPlayers[idx];
    volatile uint16_t *dst;
    uint16_t *src;
    uint16_t i;

    /* Header: team number (word), player ordinal (byte), randomPhase (byte) */
    entry->teamId = teamNumber;
    entry->playerOrdinal = (int8_t)playerOrdinal;
    entry->padding = (int8_t)viewRandomPhase;

    /* Copy 19 words (38 bytes) of player data from players[] offset */
    dst = (volatile uint16_t *)&entry->player;
    src = (uint16_t *)((TeamFileEx *)teamFilePtr)->players;
    for (i = 0; i < 19; i++)
        dst[i] = src[i];

    numForeignMarketPlayers2 = idx + 1;
    pendingForeignPlayerCount = pendingForeignPlayerCount + 1;
    lastLoadedExtNumber = 255;
}

/* ================================================================
 * Career_PatchTeamWithForeignMarketPlayers  (SWOS2.s line 71515)
 *
 * Iterates careerForeignMarketPlayers pool; for each entry whose
 * team number matches the target team, copies the 19-word player
 * data into the team's player slot.
 *
 * Asm register convention:
 *   in:  a0 = teamPtr (pointer to TeamFile header)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_PatchTeamWithForeignMarketPlayers(uint8_t *teamPtr)
{
    uint16_t count;
    MarketPlayer *entry;
    uint16_t i;
    uint8_t teamHi, teamLo;

    if (gameType != GAME_TYPE_CAREER)
        return;

    count = numForeignMarketPlayers2;
    if (count == 0)
        return;

    /* Team number is first word — compare bytes (big-endian) */
    teamHi = teamPtr[0];
    teamLo = teamPtr[1];

    entry = careerForeignMarketPlayers;
    for (i = 0; i < count; i++) {
        if (*(uint8_t *)&entry->teamId == teamHi && *((uint8_t *)&entry->teamId + 1) == teamLo) {
            uint8_t playerOrd = (uint8_t)entry->playerOrdinal;
            volatile uint16_t *dst = (volatile uint16_t *)&((TeamFileEx *)teamPtr)->players[playerOrd];
            uint16_t *src = (uint16_t *)&entry->player;
            uint16_t j;
            for (j = 0; j < 19; j++)
                dst[j] = src[j];
        }
        entry++;
    }
}

/* ================================================================
 * Static helper: applyForeignMarketPlayersToTeams
 *
 * Shared loop for Career_ApplyForeignMarketPlayersToSelectedTeams
 * and Career_ApplyPendingForeignMarketPlayersToSelectedTeams.
 * ================================================================ */
static void applyForeignMarketPlayersToTeams(uint16_t count)
{
    MarketPlayer *entry;
    uint16_t i;

    if (gameType != GAME_TYPE_CAREER)
        return;

    if (count == 0)
        return;

    entry = careerForeignMarketPlayers;
    for (i = 0; i < count; i++) {
        uint8_t entryHi = *(uint8_t *)&entry->teamId;
        uint8_t entryLo = *((uint8_t *)&entry->teamId + 1);
        uint8_t *team = &selTeamsPtr;
        uint16_t t;

        for (t = 0; t < numSelectedTeams; t++) {
            if (team[0] == entryHi && team[1] == entryLo) {
                uint8_t playerOrd = (uint8_t)entry->playerOrdinal;
                volatile uint16_t *dst = (volatile uint16_t *)&((TeamFileEx *)team)->players[playerOrd];
                uint16_t *src = (uint16_t *)&entry->player;
                uint16_t j;
                for (j = 0; j < 19; j++)
                    dst[j] = src[j];
                break;
            }
            team += sizeof(TeamFileEx);
        }
        entry++;
    }
}

/* ================================================================
 * Career_ApplyForeignMarketPlayersToSelectedTeams  (SWOS2.s line 71459)
 *
 * Applies ALL foreign market player entries to selected teams.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_ApplyForeignMarketPlayersToSelectedTeams(void)
{
    applyForeignMarketPlayersToTeams(numForeignMarketPlayers2);
}

/* ================================================================
 * Career_ApplyPendingForeignMarketPlayersToSelectedTeams  (SWOS2.s line 71469)
 *
 * Applies only the non-pending foreign market player entries
 * to selected teams (total minus pending count).
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_ApplyPendingForeignMarketPlayersToSelectedTeams(void)
{
    uint16_t total = numForeignMarketPlayers2;
    uint16_t pending = pendingForeignPlayerCount;
    uint16_t applied = total - pending;
    applyForeignMarketPlayersToTeams(applied);
}

/* ================================================================
 * DIY_AdvanceFixtureIndex  (SWOS2.s line 71405)
 *
 * Applies pending foreign market players to selected teams for
 * players whose randomPhase falls within [lo..hi].
 * Only runs in career mode (gameType == 4).
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_AdvanceFixtureIndex(uint16_t lo, uint16_t hi)
{
    if (gameType != GAME_TYPE_CAREER)
        return;

    uint16_t pending = pendingForeignPlayerCount;
    if (pending == 0)
        return;

    /* Point to the start of pending entries (after non-pending ones) */
    uint16_t nonPending = numForeignMarketPlayers2 - pendingForeignPlayerCount;
    MarketPlayer *entry = &careerForeignMarketPlayers[nonPending];

    uint16_t i;
    for (i = 0; i < pending; i++) {
        uint8_t phase = (uint8_t)entry->padding;  /* randomPhase byte */

        /* Check if phase is within [lo..hi] range */
        if (phase >= (uint8_t)lo && phase <= (uint8_t)hi) {
            uint8_t entryHi = *(uint8_t *)&entry->teamId;       /* team number high byte */
            uint8_t entryLo = *((uint8_t *)&entry->teamId + 1); /* team number low byte */
            uint8_t *team = &selTeamsPtr;
            uint16_t t;

            for (t = 0; t < numSelectedTeams; t++) {
                if (team[0] == entryHi && team[1] == entryLo) {
                    /* Found matching team — copy player data */
                    int8_t playerOrd = entry->playerOrdinal;
                    int16_t ord = (int16_t)playerOrd;
                    volatile uint16_t *dst = (volatile uint16_t *)&((TeamFileEx *)team)->players[(uint16_t)ord];
                    uint16_t *src = (uint16_t *)&entry->player;
                    uint16_t j;
                    for (j = 0; j < 19; j++)
                        dst[j] = src[j];
                    break;
                }
                team += sizeof(TeamFileEx);
            }
        }
        entry++;
    }
}

/* ================================================================
 * Career_RemovePlayerFromTeamInternal  (SWOS2.s line 71521)
 *
 * Removes a player from transfer offers, and then either from
 * the transfer market list (if it's the selected team) or from
 * the career contest and foreign market (if another team).
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_RemovePlayerFromTeamInternal(uint16_t teamNumber, uint16_t playerOrd)
{
    _RemovePlayerFromTransferOffers(teamNumber, playerOrd);

    if (teamNumber == selTeamNumber) {
        _RemovePlayerFromTransferMarket(playerOrd);
        if (playerOrd == selTeamSelectedPlayerPosition) {
            selTeamSelectedPlayerPosition = 0xFFFF;
        }
    } else {
        _RemovePlayerFromCareerContest(teamNumber, playerOrd);
        _RemoveForeignMarketPlayer(teamNumber, playerOrd);
    }
}

/* ================================================================
 * Transfer/Offer menu callbacks (Chunk 113, SWOS2.s lines 71942-72592)
 * ================================================================ */

extern uint16_t transferPlayerMenuResult;
extern uint16_t thirdPartyOfferResult;

/* TransferPlayerOnSelect10: reject transfer (0xFFFF) */
__attribute__((used, externally_visible))
void _TransferPlayerOnSelect10(void)
{
    transferPlayerMenuResult = 0xFFFF;
    _SetExitMenuFlag();
}

/* TransferPlayerOnSelect11: accept transfer (0) */
__attribute__((used, externally_visible))
void _TransferPlayerOnSelect11(void)
{
    transferPlayerMenuResult = 0;
    _SetExitMenuFlag();
}

/* TransferPlayerOnSelect12: counter-offer transfer (1) */
__attribute__((used, externally_visible))
void _TransferPlayerOnSelect12(void)
{
    transferPlayerMenuResult = 1;
    _SetExitMenuFlag();
}

/* AcceptOffer: accept third-party offer (1) */
__attribute__((used, externally_visible))
void _AcceptOffer(void)
{
    thirdPartyOfferResult = 1;
    _SetExitMenuFlag();
}

/* RejectOffer: reject third-party offer (2) */
__attribute__((used, externally_visible))
void _RejectOffer(void)
{
    thirdPartyOfferResult = 2;
    _SetExitMenuFlag();
}

/* AskForMore: negotiate third-party offer (3) */
__attribute__((used, externally_visible))
void _AskForMore(void)
{
    thirdPartyOfferResult = 3;
    _SetExitMenuFlag();
}

/* ConsiderOffer: consider third-party offer (0) */
__attribute__((used, externally_visible))
void _ConsiderOffer(void)
{
    thirdPartyOfferResult = 0;
    _SetExitMenuFlag();
}

/* ThirdPartyOfferMenuExit: exit offer menu (0) */
__attribute__((used, externally_visible))
void _ThirdPartyOfferMenuExit(void)
{
    thirdPartyOfferResult = 0;
    _SetExitMenuFlag();
}

/* ================================================================
 * Chunk 117: Transfer Menu Helpers
 * ================================================================ */

extern MenuEntry menuEntries[];
extern void _GetBankBalance(uint8_t *dst);
extern void CheckNumPlayersOnTransferMarket(void);  /* still in asm */
extern void CheckIfPlayerSelected(void);             /* still in asm */
extern void EnableDisableOffers(void);               /* still in asm */

/* TransferMenuSetBankState — write bank balance to entry 1's string (SWOS2.s line 76190) */
__attribute__((used, externally_visible))
void _TransferMenuSetBankState(void)
{
    uint8_t *str = (uint8_t *)menuEntries[1].fg;
    _GetBankBalance(str);
}

/* BuyPlayerInit — write bank balance to entry 1's string (SWOS2.s line 75647) */
__attribute__((used, externally_visible))
void _BuyPlayerInit(void)
{
    uint8_t *str = (uint8_t *)menuEntries[1].fg;
    _GetBankBalance(str);
}

/* TransferMarketFinish — refresh all transfer menu state (SWOS2.s line 76181) */
__attribute__((used, externally_visible))
void _TransferMarketFinish(void)
{
    _TransferMenuSetBankState();
    CheckNumPlayersOnTransferMarket();
    CheckIfPlayerSelected();
    EnableDisableOffers();
}

/* ShowDomesticPlayerMarket — update home market, show market menu (SWOS2.s line 70638) */
extern uint16_t showDomesticPlayerMarket;
extern uint8_t showPlayerMarketMenu[];

__attribute__((used, externally_visible))
void _ShowDomesticPlayerMarket(void)
{
    _UpdateHomeMarketPlayers();
    showDomesticPlayerMarket = 1;
    _ShowMenu(showPlayerMarketMenu);
}

/* ---- Chunk 142: ViewDomesticPlayerMarket ---- */
/* SWOS2.s line 71089 — show domestic market, check if transfer completed */
extern uint16_t transferCompleted;

__attribute__((used, externally_visible))
void _ViewDomesticPlayerMarket(void)
{
    _ShowDomesticPlayerMarket();
    /* Original asm has duplicate check — preserve as-is */
    if (transferCompleted != 0) {
        _BuyPlayerExit();
        return;
    }
    if (transferCompleted != 0) {
        _BuyPlayerExit();
        return;
    }
}

/* ---- Chunk 144: ShowForeignPlayerMarket ---- */
/* SWOS2.s line 66050 — load pool data, maybe add random player, show foreign market menu */
__attribute__((used, externally_visible))
void _ShowForeignPlayerMarket(void)
{
    _LoadPoolplyrDat();
    if (removedOfferPlayersFromOtherTeamsCount != 0) {
        removedOfferPlayersFromOtherTeamsCount--;
        _AddRandomForeignMarketPlayer();
    }
    _UpdateForeignMarketPlayers();
    showDomesticPlayerMarket = 0;
    _ShowMenu(showPlayerMarketMenu);
}

/* ---- Chunk 144: ViewForeignPlayerMarket ---- */
/* SWOS2.s line 70889 — show foreign market, check if transfer completed */
__attribute__((used, externally_visible))
void _ViewForeignPlayerMarket(void)
{
    _ShowForeignPlayerMarket();
    /* Original asm has duplicate check — preserve as-is */
    if (transferCompleted != 0) {
        _BuyPlayerExit();
        return;
    }
    if (transferCompleted != 0) {
        _BuyPlayerExit();
        return;
    }
}

/* ---- Chunk 145: PlayerOffersUpdateOfferAmountText ---- */
/* SWOS2.s line 68515 — format offer amount * 1000 into menu entry 9's text string */
__attribute__((used, externally_visible))
void _PlayerOffersUpdateOfferAmountText(void)
{
    MenuEntry *entry = &menuEntries[9]; /* 0x1F8 / 56 = 9 */
    uint8_t *dst = (uint8_t *)entry->fg;
    uint32_t amount = (uint32_t)offerAmount * 1000;
    _Int2Pounds(amount, dst);
}

/* ---- Chunk 145: UpdatePlayerOfferMenuIndicators ---- */
/* SWOS2.s line 68243 — update +/- indicators and visibility for offer menu entries */
__attribute__((used, externally_visible))
void _UpdatePlayerOfferMenuIndicators(void)
{
    extern uint8_t aPlus[];   /* "+" */
    extern uint8_t aMinus[];  /* "-" */

    /* Entry 8 (0x1C0/56): toggle +/- based on numExtraPlayersWanted sign */
    MenuEntry *entry8 = &menuEntries[8];
    entry8->fg = (uint32_t)aPlus;
    if (numExtraPlayersWanted >= 0) {
        entry8->fg = (uint32_t)aMinus;
    }

    /* Entries 10,11 (0x230/56, 0x268/56): toggle +/- based on swap/extra player state */
    MenuEntry *entry10 = &menuEntries[10];
    MenuEntry *entry11 = &menuEntries[11];
    entry10->fg = (uint32_t)aPlus;
    entry11->fg = (uint32_t)aPlus;
    if (offerSwapPlayerOrdinal >= 0) {
        entry10->fg = (uint32_t)aMinus;
        if (offerExtraPlayerOrdinal >= 0) {
            entry11->fg = (uint32_t)aMinus;
        }
    }

    /* Entry 5 (0x118/56): hide if numExtraPlayersWanted < 0 */
    menuEntries[5].isInvisible = 1;
    if (numExtraPlayersWanted >= 0) {
        menuEntries[5].isInvisible = 0;
    }

    /* Entries 6,11 (0x150/56, 0x268/56): hide if offerSwapPlayerOrdinal < 0 */
    menuEntries[6].isInvisible = 1;
    menuEntries[11].isInvisible = 1;
    if (offerSwapPlayerOrdinal >= 0) {
        menuEntries[6].isInvisible = 0;
        menuEntries[11].isInvisible = 0;
    }

    /* Entry 7 (0x188/56): hide if offerExtraPlayerOrdinal < 0 */
    menuEntries[7].isInvisible = 1;
    if (offerExtraPlayerOrdinal >= 0) {
        menuEntries[7].isInvisible = 0;
    }
}

/* ---- Chunk 145: PlayerOffersInit ---- */
/* SWOS2.s line 68219 — initialize player offers menu: amount text, bank balance,
 * team names in formatted strings, update indicators */
__attribute__((used, externally_visible))
void _PlayerOffersInit(void)
{
    _PlayerOffersUpdateOfferAmountText();

    /* Entry 1 (0x38/56): bank balance text */
    MenuEntry *entry1 = &menuEntries[1];
    uint8_t *bankDst = (uint8_t *)entry1->fg;
    _GetBankBalance(bankDst);

    /* Entry 3 (0xA8/56): "WE HERE AT %a WISH TO BUY" with our team name */
    uint8_t *ourTeam = _GetTeamPointer(selTeamNumber);
    uint8_t *ourName = ourTeam + 5;
    MenuEntry *entry3 = &menuEntries[3];
    uint8_t *dst3 = (uint8_t *)entry3->fg;
    _PrintFormatted((const uint8_t *)"WE HERE AT %a WISH TO BUY", dst3,
                    0, 0, 0, 0, 0, 0, 0, ourName, 0, 0);

    /* Entry 4 (0xE0/56): "FROM %a IN EXCHANGE FOR" with offering team name */
    uint8_t *offeringTeam = _GetTeamPointer(teamOffering);
    uint8_t *offeringName = offeringTeam + 5;
    MenuEntry *entry4 = &menuEntries[4];
    uint8_t *dst4 = (uint8_t *)entry4->fg;
    _PrintFormatted((const uint8_t *)"FROM %a IN EXCHANGE FOR", dst4,
                    0, 0, 0, 0, 0, 0, 0, offeringName, 0, 0);

    _UpdatePlayerOfferMenuIndicators();
}

