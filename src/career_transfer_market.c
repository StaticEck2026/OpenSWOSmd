/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_transfer_market.c — UNTANGLE step 3 (overlay retirement, region B7).
 *
 * The transfer-market working state — the foreign/home market slot tables, the
 * transfer-offer table, the job-offer queues, the players-on-market bitset and
 * the per-offer negotiation scalars — lifted out of the careerFileBuffer overlay
 * into standalone storage. Real labels (not .equ) so every reader
 * (transfer_market / job_offers / menus) + the save descriptors keep working
 * unchanged. See project_career_overlay_extraction_recipe.
 *
 * Two contiguous overlay runs (split by the already-extracted A5b transfer
 * counters careerSeasonsCompleted..offerSwapSlotsUsed, now a 16-byte hole):
 *   run 1 (230 B): foreignMarketPlayers(60), homeMarketPlayers(100),
 *                  transferOfferTable(70)
 *   run 2 (296 B): numNewJobOffers, newJobOffers(JobOffer[5]=130), numJobOffers,
 *                  jobOffers(JobOffer[5]=130), timeToNegotiate,
 *                  numPlayersOnTransferMarket, playersOnTransferMarket(10),
 *                  lastOffer{Primary,Swap,Extra}*Ordinal / ExtraPlayersWanted,
 *                  offerNegotiationAttemptCount, transferMarketRandOffset0..2,
 *                  transferMarketPhase
 * Each run becomes its own .skip hole (no cascade; the A5b hole between them is
 * untouched).
 *
 * Format-neutral: market slots are 4-byte {word teamNum, byte ordinal, byte pad}
 * entries and JobOffer is 26 bytes — all pointer-free, no 64-bit growth. The
 * save addresses every field via &symbol: FA foreign/home_market_players +
 * players_on_transfer_market (FLD_BYTES), FX scalars, and the typed
 * s_transfer_offer_fields / s_job_offer_fields tables over &transferOfferTable /
 * &newJobOffers / &jobOffers. So relocating the storage changes nothing the save
 * sees.
 *
 * Audited: every access is bounded within-array indexing — foreignMarketPlayers /
 * homeMarketPlayers via + idx*MARKET_ENTRY_SIZE (idx < num{Foreign,Home}MarketPlayers),
 * &jobOffers[index] / &newJobOffers[i] within JobOffer[5] — no cross-array walk
 * into a neighbour. Emitted via __asm__ real labels (raw-byte storage) so the
 * conflicting element-type externs (uint8_t[] vs JobOffer[]) resolve as before.
 */
#include "swos_types.h"
#include "swos_platform.h"

extern uint8_t foreignMarketPlayers[60];
extern uint8_t homeMarketPlayers[100];
extern uint8_t transferOfferTable[70];
extern uint8_t newJobOffers[130];
extern uint8_t jobOffers[130];
extern uint8_t playersOnTransferMarket[10];
_Static_assert(sizeof(foreignMarketPlayers)==60 && sizeof(homeMarketPlayers)==100
            && sizeof(transferOfferTable)==70 && sizeof(newJobOffers)==130
            && sizeof(jobOffers)==130 && sizeof(playersOnTransferMarket)==10,
               "transfer-market array sizes must match the overlay reservation");

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    /* ---- run 1 ---- */
    ".globl " _ASM_PFX "foreignMarketPlayers\n"
    _ASM_PFX "foreignMarketPlayers:\n"
    ".skip 60\n"
    ".globl " _ASM_PFX "homeMarketPlayers\n"
    _ASM_PFX "homeMarketPlayers:\n"
    ".skip 100\n"
    ".globl " _ASM_PFX "transferOfferTable\n"
    _ASM_PFX "transferOfferTable:\n"
    ".skip 70\n"
    /* ---- run 2 ---- */
    ".globl " _ASM_PFX "numNewJobOffers\n"
    _ASM_PFX "numNewJobOffers:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "newJobOffers\n"
    _ASM_PFX "newJobOffers:\n"
    ".skip 130\n"
    ".globl " _ASM_PFX "numJobOffers\n"
    _ASM_PFX "numJobOffers:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "jobOffers\n"
    _ASM_PFX "jobOffers:\n"
    ".skip 130\n"
    ".globl " _ASM_PFX "timeToNegotiate\n"
    _ASM_PFX "timeToNegotiate:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "numPlayersOnTransferMarket\n"
    _ASM_PFX "numPlayersOnTransferMarket:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "playersOnTransferMarket\n"
    _ASM_PFX "playersOnTransferMarket:\n"
    ".skip 10\n"
    ".globl " _ASM_PFX "lastOfferPrimaryPlayerOrdinal\n"
    _ASM_PFX "lastOfferPrimaryPlayerOrdinal:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "lastOfferExtraPlayersWanted\n"
    _ASM_PFX "lastOfferExtraPlayersWanted:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "lastOfferSwapPlayerOrdinal\n"
    _ASM_PFX "lastOfferSwapPlayerOrdinal:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "lastOfferExtraPlayerOrdinal\n"
    _ASM_PFX "lastOfferExtraPlayerOrdinal:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "offerNegotiationAttemptCount\n"
    _ASM_PFX "offerNegotiationAttemptCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "transferMarketRandOffset0\n"
    _ASM_PFX "transferMarketRandOffset0:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "transferMarketRandOffset1\n"
    _ASM_PFX "transferMarketRandOffset1:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "transferMarketRandOffset2\n"
    _ASM_PFX "transferMarketRandOffset2:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "transferMarketPhase\n"
    _ASM_PFX "transferMarketPhase:\n"
    ".skip 2\n"
    ".text\n"
);
