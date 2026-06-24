/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_finance_tail.c — UNTANGLE step 3 (overlay retirement, region A1).
 *
 * The "finance tail" — the run of weekly/per-player finance state that
 * immediately follows the 11 int32 finance totals (career_finances.c) inside
 * the careerFileBuffer overlay — lifted into a standalone typed
 * `CareerFinanceTail` struct. Real labels (not .equ) so every reader
 * (career_status.c wage/value code) + the save FX(...)/FA(...) descriptors keep
 * working unchanged. The overlay run becomes a .skip 190 hole (no cascade).
 * See career_finances.c / project_career_overlay_extraction_recipe for the why
 * (macOS-safe real labels, format-neutral by &symbol addressing).
 *
 * Authoritative byte layout from SWOS2.s (BSS, ds.b):
 *     financeTotalsNeedResetFlag   ds.b $2   (uint16; move.w write)
 *     BSS_01_164A14                ds.b $4   (unnamed hole)
 *     weeklyRunningCosts           ds.b $4   (uint32; move.l)
 *     weeklyPlayerWages            ds.b $4   (uint32; move.l)
 *     totalTeamValue               ds.b $4   (int32)
 *     weeklyWageByPlayerIndex      ds.b $80  (int32[32]; per-player weekly wage)
 *     playerFormModifiers          ds.b $20  (uint8[32]; per-player form recovery)
 *     sensisoftPlcSponsorshipFactor ds.b $2  (uint16)
 *     additionalInvestments        ds.b $2   (uint16)
 *     BSS_01_164AC8                ds.b $4   (unnamed hole)
 *     teamValueDivisor             ds.b $2   (uint16; divu.w divisor)
 *     expectedLeagueRankIndex      ds.b $2   (uint16)
 *   = 190 bytes total.
 *
 * Audited (no consumer edits needed): no raw careerFileBuffer+offset access in
 * the range; the two array fields are indexed strictly within bounds
 * (weeklyWageByPlayerIndex[fileIdx]/[playerIndex], playerFormModifiers[playerIdx],
 * both <= squad size); no &scalar address-of/array-walk crosses into the next
 * global. Save coverage is unchanged: finance_totals_need_reset_flag/…(FLD_*),
 * weekly_wage_by_player_index (FA 128 FLD_U32_ARRAY), player_form_modifiers
 * (FA 32 FLD_BYTES) — all addressed via &symbol, so they follow the relocation.
 *
 * The two int32-class runs land at word-aligned-but-not-long-aligned offsets
 * (6/10/14/18) because of the leading uint16 + 4-byte unnamed hole — exactly as
 * on the Amiga (even addresses, valid for m68k move.l and for x86). The typed
 * view is therefore `packed`, so its field offsets match the real-label byte
 * layout (locked by the _Static_asserts); no code uses the struct members yet
 * (every reader still uses the standalone legacy symbols), so the packed-member
 * alignment is purely a documentation/migration detail.
 */

#include "swos_types.h"     /* uint16_t/uint32_t/int32_t, offsetof, UINTPTR_MAX */
#include "swos_platform.h"  /* _ASM_PFX */

typedef struct {
    uint16_t financeTotalsNeedResetFlag;    /*   0 */
    uint8_t  _resv_164A14[4];               /*   2  unnamed BSS_01_164A14 hole */
    uint32_t weeklyRunningCosts;            /*   6 */
    uint32_t weeklyPlayerWages;             /*  10 */
    int32_t  totalTeamValue;                /*  14 */
    int32_t  weeklyWageByPlayerIndex[32];   /*  18  (128 bytes) */
    uint8_t  playerFormModifiers[32];       /* 146 */
    uint16_t sensisoftPlcSponsorshipFactor; /* 178 */
    uint16_t additionalInvestments;         /* 180 */
    uint8_t  _resv_164AC8[4];               /* 182  unnamed BSS_01_164AC8 hole */
    uint16_t teamValueDivisor;              /* 186 */
    uint16_t expectedLeagueRankIndex;       /* 188 */
} __attribute__((packed)) CareerFinanceTail;

_Static_assert(sizeof(CareerFinanceTail) == 190,
               "CareerFinanceTail must stay 190 bytes (overlay-exact)");
_Static_assert(offsetof(CareerFinanceTail, financeTotalsNeedResetFlag)    ==   0, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, weeklyRunningCosts)            ==   6, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, weeklyPlayerWages)             ==  10, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, totalTeamValue)                ==  14, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, weeklyWageByPlayerIndex)       ==  18, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, playerFormModifiers)           == 146, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, sensisoftPlcSponsorshipFactor) == 178, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, additionalInvestments)         == 180, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, teamValueDivisor)              == 186, "ft layout");
_Static_assert(offsetof(CareerFinanceTail, expectedLeagueRankIndex)       == 188, "ft layout");

extern CareerFinanceTail _careerFinanceTail;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "_careerFinanceTail\n"
    _ASM_PFX "_careerFinanceTail:\n"
    ".globl " _ASM_PFX "_financeTotalsNeedResetFlag\n"
    _ASM_PFX "_financeTotalsNeedResetFlag:\n"
    ".skip 2\n"
    ".skip 4\n"   /* BSS_01_164A14 unnamed hole */
    ".globl " _ASM_PFX "_weeklyRunningCosts\n"
    _ASM_PFX "_weeklyRunningCosts:\n"
    ".skip 4\n"
    ".globl " _ASM_PFX "_weeklyPlayerWages\n"
    _ASM_PFX "_weeklyPlayerWages:\n"
    ".skip 4\n"
    ".globl " _ASM_PFX "_totalTeamValue\n"
    _ASM_PFX "_totalTeamValue:\n"
    ".skip 4\n"
    ".globl " _ASM_PFX "_weeklyWageByPlayerIndex\n"
    _ASM_PFX "_weeklyWageByPlayerIndex:\n"
    ".skip 128\n"
    ".globl " _ASM_PFX "_playerFormModifiers\n"
    _ASM_PFX "_playerFormModifiers:\n"
    ".skip 32\n"
    ".globl " _ASM_PFX "_sensisoftPlcSponsorshipFactor\n"
    _ASM_PFX "_sensisoftPlcSponsorshipFactor:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_additionalInvestments\n"
    _ASM_PFX "_additionalInvestments:\n"
    ".skip 2\n"
    ".skip 4\n"   /* BSS_01_164AC8 unnamed hole */
    ".globl " _ASM_PFX "_teamValueDivisor\n"
    _ASM_PFX "_teamValueDivisor:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_expectedLeagueRankIndex\n"
    _ASM_PFX "_expectedLeagueRankIndex:\n"
    ".skip 2\n"
    ".text\n"
);
