/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_trophy_flags.c — UNTANGLE step 3 (overlay retirement, region A2).
 *
 * 15 consecutive uint16 career-award/progress flags lifted out of the
 * careerFileBuffer overlay into a standalone typed `CareerTrophyFlags` struct:
 * the win/trophy/cup-bonus "play this animation" toggles, the won-cup markers,
 * the four euro/world qualifying-stage pending flags, plus the isCareer and
 * careerMatchSimulatedFlag run-mode flags that share the same overlay run.
 * Real labels (not .equ) so every reader + the save FX(...) descriptors keep
 * working unchanged. Overlay run becomes a .skip 30 hole (no cascade). Audited:
 * all 15 are ds.b $2 (no unnamed holes), all FX FLD_U16 addressed via &symbol
 * (save format unchanged), no raw careerFileBuffer+offset, no &scalar walk.
 * See career_finances.c / project_career_overlay_extraction_recipe.
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint16_t playWinCupAnimation;
    uint16_t minorTrophyBonusActive;
    uint16_t leagueTitleBonusActive;
    uint16_t majorTrophyBonusActive;
    uint16_t cupBonus1Active;
    uint16_t cupBonus2Active;
    uint16_t wonWorldCup;
    uint16_t wonContinentalCup;
    uint16_t wonInvitationTournament;
    uint16_t qualifyingStage1PendingFlag0;
    uint16_t qualifyingStage1PendingFlag1;
    uint16_t qualifyingStage2PendingFlag0;
    uint16_t qualifyingStage2PendingFlag1;
    uint16_t isCareer;
    uint16_t careerMatchSimulatedFlag;
} CareerTrophyFlags;

_Static_assert(sizeof(CareerTrophyFlags) == 30,
               "CareerTrophyFlags must stay 30 bytes (15 x uint16, overlay-exact)");
_Static_assert(offsetof(CareerTrophyFlags, playWinCupAnimation)      ==  0, "tf layout");
_Static_assert(offsetof(CareerTrophyFlags, careerMatchSimulatedFlag) == 28, "tf layout");

extern CareerTrophyFlags _careerTrophyFlags;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "_careerTrophyFlags\n"
    _ASM_PFX "_careerTrophyFlags:\n"
    ".globl " _ASM_PFX "_playWinCupAnimation\n"
    _ASM_PFX "_playWinCupAnimation:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_minorTrophyBonusActive\n"
    _ASM_PFX "_minorTrophyBonusActive:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_leagueTitleBonusActive\n"
    _ASM_PFX "_leagueTitleBonusActive:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_majorTrophyBonusActive\n"
    _ASM_PFX "_majorTrophyBonusActive:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_cupBonus1Active\n"
    _ASM_PFX "_cupBonus1Active:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_cupBonus2Active\n"
    _ASM_PFX "_cupBonus2Active:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_wonWorldCup\n"
    _ASM_PFX "_wonWorldCup:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_wonContinentalCup\n"
    _ASM_PFX "_wonContinentalCup:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_wonInvitationTournament\n"
    _ASM_PFX "_wonInvitationTournament:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_qualifyingStage1PendingFlag0\n"
    _ASM_PFX "_qualifyingStage1PendingFlag0:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_qualifyingStage1PendingFlag1\n"
    _ASM_PFX "_qualifyingStage1PendingFlag1:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_qualifyingStage2PendingFlag0\n"
    _ASM_PFX "_qualifyingStage2PendingFlag0:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_qualifyingStage2PendingFlag1\n"
    _ASM_PFX "_qualifyingStage2PendingFlag1:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_isCareer\n"
    _ASM_PFX "_isCareer:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_careerMatchSimulatedFlag\n"
    _ASM_PFX "_careerMatchSimulatedFlag:\n"
    ".skip 2\n"
    ".text\n"
);
