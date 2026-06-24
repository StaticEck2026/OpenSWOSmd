/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_setup.c — UNTANGLE step 3 (overlay retirement, region A4).
 *
 * The career-setup run — world-cup host/qualifying teams, euro-championship
 * qualifier, the selected team/coach-or-player choice, league/division indices
 * and the promote/relegate animation toggles — lifted out of the careerFileBuffer
 * overlay into a standalone typed `CareerSetup` struct. Real labels (not .equ)
 * so every reader (career_season / match_sim / career_status) + the save FX(...)
 * descriptors keep working unchanged. Overlay run becomes a .skip 20 hole (no
 * cascade). Audited: all 10 are ds.b $2 (no unnamed holes), all FX FLD_U16
 * addressed via &symbol (save format unchanged); the only &scalar use is the
 * in-bounds `*(uint8_t *)&selTeamNumber` low-byte read. See
 * project_career_overlay_extraction_recipe.
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint16_t worldCupHostTeam;
    uint16_t worldCupQualifyingTeam;
    uint16_t euroChampionshipQualTeam;
    uint16_t selTeamNumber;
    uint16_t coachOrPlayer;
    uint16_t careerTeamLeague;
    uint16_t currentDivisionIndex;
    uint16_t leagueFirstRoundNumber;
    uint16_t playPromotedAnimation;
    uint16_t playRelegatedAnimation;
} CareerSetup;

_Static_assert(sizeof(CareerSetup) == 20,
               "CareerSetup must stay 20 bytes (10 x uint16, overlay-exact)");
_Static_assert(offsetof(CareerSetup, worldCupHostTeam)      ==  0, "cs layout");
_Static_assert(offsetof(CareerSetup, selTeamNumber)         ==  6, "cs layout");
_Static_assert(offsetof(CareerSetup, playRelegatedAnimation) == 18, "cs layout");

extern CareerSetup _careerSetup;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "_careerSetup\n"
    _ASM_PFX "_careerSetup:\n"
    ".globl " _ASM_PFX "_worldCupHostTeam\n"
    _ASM_PFX "_worldCupHostTeam:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_worldCupQualifyingTeam\n"
    _ASM_PFX "_worldCupQualifyingTeam:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_euroChampionshipQualTeam\n"
    _ASM_PFX "_euroChampionshipQualTeam:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_selTeamNumber\n"
    _ASM_PFX "_selTeamNumber:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_coachOrPlayer\n"
    _ASM_PFX "_coachOrPlayer:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_careerTeamLeague\n"
    _ASM_PFX "_careerTeamLeague:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_currentDivisionIndex\n"
    _ASM_PFX "_currentDivisionIndex:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_leagueFirstRoundNumber\n"
    _ASM_PFX "_leagueFirstRoundNumber:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_playPromotedAnimation\n"
    _ASM_PFX "_playPromotedAnimation:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_playRelegatedAnimation\n"
    _ASM_PFX "_playRelegatedAnimation:\n"
    ".skip 2\n"
    ".text\n"
);
