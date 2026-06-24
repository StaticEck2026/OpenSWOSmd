/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_selector_state.c — UNTANGLE step 3 (overlay retirement, region A3).
 *
 * The contract-negotiation / national-team-selector state run lifted out of the
 * careerFileBuffer overlay into a standalone typed `CareerSelectorState` struct.
 * Real labels (not .equ) so every reader (menus / job_offers / career_season) +
 * the save FX(...) descriptors keep working unchanged. Overlay run becomes a
 * .skip 28 hole (no cascade). See project_career_overlay_extraction_recipe.
 *
 * Authoritative byte layout from SWOS2.s (BSS, ds.b):
 *     contractNegotiationActive        ds.b $2  (uint16)
 *     wageScalingDivisor               ds.b $2  (uint16)
 *     teamNoOfferAccepted              ds.b $2  (uint16; -1 = none yet)
 *     teamFundsAccepted                ds.b $4  (int32)
 *     isNationalSelector               ds.b $2  (uint16)
 *     someTeamFlags                    ds.b $1  (uint8, FLD_U8)
 *     BSS_01_164B28                    ds.b $1  (unnamed hole)
 *     lastNationalityCall              ds.b $2  (uint16)
 *     nationalTeamNationality          ds.b $1  (uint8)
 *     nationalTeamFileId               ds.b $1  (uint8)
 *     sameNationalCallFlag             ds.b $2  (uint16)
 *     sackedReputationScore            ds.b $2  (uint16)
 *     nationalSelectorReputationPoints ds.b $2  (uint16)
 *     nationalTeamFileCode             ds.b $1  (uint8, FLD_U8)
 *     BSS_01_164B34                    ds.b $1  (unnamed hole)
 *     seasonReview                     ds.b $2  (uint16)
 *   = 28 bytes total.
 *
 * Audited (no consumer edits): teamFundsAccepted is the only multi-byte int
 * (FLD_S32, &symbol-addressed); someTeamFlags / nationalTeamFileCode are
 * single-byte (FLD_U8) each trailed by a 1-byte unnamed hole; no raw
 * careerFileBuffer+offset; the only &scalar use is `*(uint8_t *)&selTeamNumber`
 * elsewhere (an in-bounds low-byte read of a DIFFERENT symbol). teamFundsAccepted
 * lands at word-aligned offset 6 → typed view is `packed` so its offsets match
 * the real-label byte layout (locked by _Static_asserts).
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint16_t contractNegotiationActive;        /*  0 */
    uint16_t wageScalingDivisor;               /*  2 */
    uint16_t teamNoOfferAccepted;              /*  4 */
    int32_t  teamFundsAccepted;                /*  6 */
    uint16_t isNationalSelector;               /* 10 */
    uint8_t  someTeamFlags;                    /* 12 */
    uint8_t  _resv_164B28;                     /* 13  unnamed hole */
    uint16_t lastNationalityCall;              /* 14 */
    uint8_t  nationalTeamNationality;          /* 16 */
    uint8_t  nationalTeamFileId;               /* 17 */
    uint16_t sameNationalCallFlag;             /* 18 */
    uint16_t sackedReputationScore;            /* 20 */
    uint16_t nationalSelectorReputationPoints; /* 22 */
    uint8_t  nationalTeamFileCode;             /* 24 */
    uint8_t  _resv_164B34;                     /* 25  unnamed hole */
    uint16_t seasonReview;                     /* 26 */
} __attribute__((packed)) CareerSelectorState;

_Static_assert(sizeof(CareerSelectorState) == 28,
               "CareerSelectorState must stay 28 bytes (overlay-exact)");
_Static_assert(offsetof(CareerSelectorState, contractNegotiationActive)        ==  0, "ss layout");
_Static_assert(offsetof(CareerSelectorState, teamFundsAccepted)                ==  6, "ss layout");
_Static_assert(offsetof(CareerSelectorState, isNationalSelector)               == 10, "ss layout");
_Static_assert(offsetof(CareerSelectorState, someTeamFlags)                    == 12, "ss layout");
_Static_assert(offsetof(CareerSelectorState, lastNationalityCall)              == 14, "ss layout");
_Static_assert(offsetof(CareerSelectorState, nationalTeamFileCode)             == 24, "ss layout");
_Static_assert(offsetof(CareerSelectorState, seasonReview)                     == 26, "ss layout");

extern CareerSelectorState _careerSelectorState;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "_careerSelectorState\n"
    _ASM_PFX "_careerSelectorState:\n"
    ".globl " _ASM_PFX "_contractNegotiationActive\n"
    _ASM_PFX "_contractNegotiationActive:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_wageScalingDivisor\n"
    _ASM_PFX "_wageScalingDivisor:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_teamNoOfferAccepted\n"
    _ASM_PFX "_teamNoOfferAccepted:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_teamFundsAccepted\n"
    _ASM_PFX "_teamFundsAccepted:\n"
    ".skip 4\n"
    ".globl " _ASM_PFX "_isNationalSelector\n"
    _ASM_PFX "_isNationalSelector:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_someTeamFlags\n"
    _ASM_PFX "_someTeamFlags:\n"
    ".skip 1\n"
    ".skip 1\n"   /* BSS_01_164B28 unnamed hole */
    ".globl " _ASM_PFX "_lastNationalityCall\n"
    _ASM_PFX "_lastNationalityCall:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_nationalTeamNationality\n"
    _ASM_PFX "_nationalTeamNationality:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "_nationalTeamFileId\n"
    _ASM_PFX "_nationalTeamFileId:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "_sameNationalCallFlag\n"
    _ASM_PFX "_sameNationalCallFlag:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_sackedReputationScore\n"
    _ASM_PFX "_sackedReputationScore:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_nationalSelectorReputationPoints\n"
    _ASM_PFX "_nationalSelectorReputationPoints:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_nationalTeamFileCode\n"
    _ASM_PFX "_nationalTeamFileCode:\n"
    ".skip 1\n"
    ".skip 1\n"   /* BSS_01_164B34 unnamed hole */
    ".globl " _ASM_PFX "_seasonReview\n"
    _ASM_PFX "_seasonReview:\n"
    ".skip 2\n"
    ".text\n"
);
