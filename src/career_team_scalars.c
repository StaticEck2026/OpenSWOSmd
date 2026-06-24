/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_team_scalars.c — UNTANGLE step 3 / step-4 phase 1 (overlay retirement).
 *
 * The three scalars that sit immediately after the careerTeam buffer
 * (careerNumPlayers, selTeamSelectedPlayerPosition, removedPlayerStatAccum)
 * lifted out of the careerFileBuffer overlay into a standalone typed
 * `CareerTeamScalars` struct. Real labels (not .equ) so every reader + the save
 * FX/FA descriptors keep working unchanged. The 6-byte overlay run becomes a
 * .skip 6 hole (no cascade); careerTeam itself STAYS in the overlay until the
 * step-4 identity rework (see docs/CAREERTEAM_STEP4_REWORK.md, phases 3-4).
 *
 * This is unblocked by step-4 phase 1: careerNumPlayers used to double as
 * careerTeam's range end-bound (menus.c / teams.c `playerFile < &careerNumPlayers`).
 * Those sites now bound on CAREER_TEAM_BUF_SIZE instead, so careerNumPlayers no
 * longer needs to sit adjacent to careerTeam and can move here.
 *
 * Audited: all three are accessed as plain scalars (careerNumPlayers += 1,
 * == 0x14; &selTeamSelectedPlayerPosition as a value pointer; removedPlayerStatAccum
 * += / value reads) — no &-address range-bound use remains. All are &symbol in
 * the save (FX career_num_players / sel_team_selected_player_position FLD_U16;
 * FA removed_player_stat_accum 2 FLD_U16_ARRAY), so the format is unchanged.
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint16_t careerNumPlayers;              /* 0 */
    uint16_t selTeamSelectedPlayerPosition; /* 2 */
    uint16_t removedPlayerStatAccum;        /* 4 */
} CareerTeamScalars;

_Static_assert(sizeof(CareerTeamScalars) == 6,
               "CareerTeamScalars must stay 6 bytes (3 x uint16)");

extern CareerTeamScalars _careerTeamScalars;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "_careerTeamScalars\n"
    _ASM_PFX "_careerTeamScalars:\n"
    ".globl " _ASM_PFX "_careerNumPlayers\n"
    _ASM_PFX "_careerNumPlayers:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_selTeamSelectedPlayerPosition\n"
    _ASM_PFX "_selTeamSelectedPlayerPosition:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_removedPlayerStatAccum\n"
    _ASM_PFX "_removedPlayerStatAccum:\n"
    ".skip 2\n"
    ".text\n"
);
