/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_index_tables.c — UNTANGLE step 3 (overlay retirement, region B2).
 *
 * Three independent, self-contained career lookup/index tables lifted out of the
 * careerFileBuffer overlay into standalone storage. They live in different parts
 * of the overlay, so each is carved as its own .skip hole; they share this file
 * only for economy. Real labels (not .equ) so every reader + the save FA(...)
 * descriptors keep working unchanged. See project_career_overlay_extraction_recipe.
 *
 *   teamBudgetRankTable                  128 B  (uint8; diy_competition rank-by-budget)
 *   careerLastMatchIndexByCompetitionSlot 162 B (uint8; 0xFF-filled per-slot index)
 *   seasonSquadTeamNumbers                48 B  (uint16[24]; squad team numbers)
 *
 * Audited (no consumer edits, all &symbol-addressed FA blobs → format unchanged):
 *  - teamBudgetRankTable[idx] (idx < team count) — within 128 B.
 *  - careerLastMatchIndexByCompetitionSlot — base ptr, 162-byte 0xFF fill, indexed
 *    by slot — within 162 B.
 *  - seasonSquadTeamNumbers — uint16 walked up to seasonNumberOfSquads (<= 24) —
 *    within 48 B. In SWOS2.s its ds.b $30 (48) is followed by
 *    careerCompetitionSlotPtrTable (ds.b $14 = 20), which the overlay folded into
 *    a single .skip 68. careerCompetitionSlotPtrTable is now a SEPARATE standalone
 *    symbol (uint8_t*[5], re-derived on load), so those 20 bytes are dead overlay
 *    space; we reserve seasonSquadTeamNumbers as 48 here and leave the overlay's
 *    full 68-byte run as a hole (no cascade).
 *
 * Single standalone arrays (no sub-field aliasing), but emitted via an __asm__
 * real-label block rather than plain C definitions because these symbols carry
 * conflicting element-type externs across TUs (uint8_t[] vs uint16_t[]); raw-byte
 * BSS storage sidesteps that and matches the rest of the overlay-extraction work.
 */
#include "swos_types.h"
#include "swos_platform.h"

extern uint8_t  _teamBudgetRankTable[128];
extern uint8_t  _careerLastMatchIndexByCompetitionSlot[162];
extern uint16_t _seasonSquadTeamNumbers[24];

_Static_assert(sizeof(_teamBudgetRankTable) == 128, "teamBudgetRankTable 128 B");
_Static_assert(sizeof(_careerLastMatchIndexByCompetitionSlot) == 162,
               "careerLastMatchIndexByCompetitionSlot 162 B");
_Static_assert(sizeof(_seasonSquadTeamNumbers) == 48, "seasonSquadTeamNumbers 48 B");

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "_teamBudgetRankTable\n"
    _ASM_PFX "_teamBudgetRankTable:\n"
    ".skip 128\n"
    ".globl " _ASM_PFX "_careerLastMatchIndexByCompetitionSlot\n"
    _ASM_PFX "_careerLastMatchIndexByCompetitionSlot:\n"
    ".skip 162\n"
    ".globl " _ASM_PFX "_seasonSquadTeamNumbers\n"
    _ASM_PFX "_seasonSquadTeamNumbers:\n"
    ".skip 48\n"
    ".text\n"
);
