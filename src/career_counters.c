/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_counters.c — UNTANGLE step 3 (overlay retirement, region A5).
 *
 * Two non-adjacent uint16 counter runs lifted out of the careerFileBuffer
 * overlay into standalone typed structs. They flank the transfer-market arrays
 * (foreignMarketPlayers / homeMarketPlayers / transferOfferTable), which STAY in
 * the overlay for now (FLD_BYTES blobs, deferred with the rest of the market
 * work — see project_cache_overlay_writer_overflow), so each run becomes its own
 * .skip hole (24 + 16 bytes; no cascade, the market arrays keep their offsets):
 *
 *   CareerSeasonCounters    (24 B) — bonus counts, removed-offer counts, market
 *                                    backlogs, offer-expiry, market populations.
 *   CareerTransferCounters  (16 B) — per-season transfer-pool / offer-swap quotas
 *                                    and usage counters.
 *
 * Real labels (not .equ) so every reader (transfer_market / career_status / etc.)
 * + the save FX(...) descriptors keep working unchanged. Audited: all fields are
 * ds.b $2 (no unnamed holes), all FX FLD_U16/FLD_S16 addressed via &symbol (save
 * format unchanged), no raw careerFileBuffer+offset, no &scalar walk.
 * careerReputationPoints is signed (int16). See
 * project_career_overlay_extraction_recipe.
 */
#include "swos_types.h"
#include "swos_platform.h"

/* ---- run 1: season / market counters (24 bytes) ---- */
typedef struct {
    uint16_t winCupBonusCount;
    uint16_t minorTrophyBonusCount;
    uint16_t majorTrophyBonusCount;
    int16_t  careerReputationPoints;
    uint16_t removedOfferPlayersFromSelectedTeamCount;
    uint16_t removedOfferPlayersFromOtherTeamsCount;
    uint16_t homeMarketUpdateBacklog;
    uint16_t foreignMarketUpdateBacklog;
    uint16_t offerExpiryIterationsRemaining;
    uint16_t numForeignMarketPlayers;
    uint16_t numHomeMarketPlayers;
    uint16_t numOffers;
} CareerSeasonCounters;

_Static_assert(sizeof(CareerSeasonCounters) == 24,
               "CareerSeasonCounters must stay 24 bytes (12 x uint16, overlay-exact)");
_Static_assert(offsetof(CareerSeasonCounters, winCupBonusCount) ==  0, "sc layout");
_Static_assert(offsetof(CareerSeasonCounters, numOffers)        == 22, "sc layout");

/* ---- run 2: per-season transfer-pool / offer-swap counters (16 bytes) ---- */
typedef struct {
    uint16_t careerSeasonsCompleted;
    uint16_t numForeignMarketPlayers2;
    uint16_t pendingForeignPlayerCount;
    uint16_t seasonTransferQuota;
    uint16_t transferPoolSlotLimit;
    uint16_t offerSwapSlotLimit;
    uint16_t transferPoolSlotsUsed;
    uint16_t offerSwapSlotsUsed;
} CareerTransferCounters;

_Static_assert(sizeof(CareerTransferCounters) == 16,
               "CareerTransferCounters must stay 16 bytes (8 x uint16, overlay-exact)");
_Static_assert(offsetof(CareerTransferCounters, careerSeasonsCompleted) ==  0, "tc layout");
_Static_assert(offsetof(CareerTransferCounters, offerSwapSlotsUsed)     == 14, "tc layout");

extern CareerSeasonCounters   _careerSeasonCounters;
extern CareerTransferCounters _careerTransferCounters;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    /* ---- run 1 ---- */
    ".globl " _ASM_PFX "_careerSeasonCounters\n"
    _ASM_PFX "_careerSeasonCounters:\n"
    ".globl " _ASM_PFX "_winCupBonusCount\n"
    _ASM_PFX "_winCupBonusCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_minorTrophyBonusCount\n"
    _ASM_PFX "_minorTrophyBonusCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_majorTrophyBonusCount\n"
    _ASM_PFX "_majorTrophyBonusCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_careerReputationPoints\n"
    _ASM_PFX "_careerReputationPoints:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_removedOfferPlayersFromSelectedTeamCount\n"
    _ASM_PFX "_removedOfferPlayersFromSelectedTeamCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_removedOfferPlayersFromOtherTeamsCount\n"
    _ASM_PFX "_removedOfferPlayersFromOtherTeamsCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_homeMarketUpdateBacklog\n"
    _ASM_PFX "_homeMarketUpdateBacklog:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_foreignMarketUpdateBacklog\n"
    _ASM_PFX "_foreignMarketUpdateBacklog:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_offerExpiryIterationsRemaining\n"
    _ASM_PFX "_offerExpiryIterationsRemaining:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_numForeignMarketPlayers\n"
    _ASM_PFX "_numForeignMarketPlayers:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_numHomeMarketPlayers\n"
    _ASM_PFX "_numHomeMarketPlayers:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_numOffers\n"
    _ASM_PFX "_numOffers:\n"
    ".skip 2\n"
    /* ---- run 2 ---- */
    ".globl " _ASM_PFX "_careerTransferCounters\n"
    _ASM_PFX "_careerTransferCounters:\n"
    ".globl " _ASM_PFX "_careerSeasonsCompleted\n"
    _ASM_PFX "_careerSeasonsCompleted:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_numForeignMarketPlayers2\n"
    _ASM_PFX "_numForeignMarketPlayers2:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_pendingForeignPlayerCount\n"
    _ASM_PFX "_pendingForeignPlayerCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_seasonTransferQuota\n"
    _ASM_PFX "_seasonTransferQuota:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_transferPoolSlotLimit\n"
    _ASM_PFX "_transferPoolSlotLimit:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_offerSwapSlotLimit\n"
    _ASM_PFX "_offerSwapSlotLimit:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_transferPoolSlotsUsed\n"
    _ASM_PFX "_transferPoolSlotsUsed:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_offerSwapSlotsUsed\n"
    _ASM_PFX "_offerSwapSlotsUsed:\n"
    ".skip 2\n"
    ".text\n"
);
