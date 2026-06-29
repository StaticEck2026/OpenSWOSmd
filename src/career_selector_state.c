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

/* Standalone zero-init BSS storage — pure C (was an __asm__ .skip block;
 * converted for the WASM/Emscripten target, which rejects asm-defined data
 * symbols — see docs/WASM.md §4b). Independent globals matching the old
 * member labels' names/types/sizes; the typed base + overlay pads were unused
 * scaffolding and are dropped. Each member is &symbol-addressed in the save,
 * so the format is unchanged. */
uint16_t _contractNegotiationActive, _wageScalingDivisor, _teamNoOfferAccepted;
int32_t  _teamFundsAccepted;
uint16_t _isNationalSelector;
uint8_t  _someTeamFlags;
uint16_t _lastNationalityCall;
uint8_t  _nationalTeamNationality, _nationalTeamFileId;
uint16_t _sameNationalCallFlag, _sackedReputationScore, _nationalSelectorReputationPoints;
uint8_t  _nationalTeamFileCode;
uint16_t _seasonReview;
