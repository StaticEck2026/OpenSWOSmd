/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_season_state.c — UNTANGLE step 3 (overlay retirement, region B6).
 *
 * The season/career completion + euro-cup-slot + squad-count scalars lifted out
 * of the careerFileBuffer overlay into a standalone typed `CareerSeasonState`
 * struct. Real labels (not .equ) so every reader (statistics / career_init /
 * career_season) + the save FX(...) descriptors keep working unchanged.
 *
 * In SWOS2.s these 4 uint16 fields are interleaved with the six
 * pointer-to-disk competition pointers (currentCompetitionPtr,
 * secondary/tertiary/quaternaryCompetitionPtr, diyCompetitionPtr,
 * diyCompetitionTablePtr), which the overlay folded into careerFinished's
 * .skip 22 (2 real + 5 ptrs * 4 = 20 dead) and euroCupSlotIndex's .skip 6
 * (2 real + diyCompetitionTablePtr 4 dead):
 *     seasonFinished        ds.b $2
 *     careerFinished        ds.b $2  + 5 ptr fields (dead in overlay)
 *     euroCupSlotIndex      ds.b $2  + diyCompetitionTablePtr (dead in overlay)
 *     seasonNumberOfSquads  ds.b $2
 *   = a 32-byte overlay run, of which only 8 bytes are these 4 fields.
 *
 * Those competition pointers are now SEPARATE standalone symbols (re-derived on
 * load by RebuildPointersAfterLoad, not saved), so the 24 bytes they occupied
 * in the overlay are pure dead space — nothing reads careerFinished+offset to
 * reach them. We therefore pack just the 4 real fields here (8 B) and leave the
 * historical 32-byte run reserved as an overlay hole (no cascade).
 *
 * Audited (no consumer edits): the 4 fields are accessed only as plain scalars
 * (seasonFinished != 0, seasonNumberOfSquads++, …) — no &field address-of, no
 * &field+offset walk to a pointer. All are FX FLD_U16 addressed via &symbol, so
 * the save format is unchanged.
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint16_t seasonFinished;        /* 0: season completed flag */
    uint16_t careerFinished;        /* 2: career completed flag */
    uint16_t euroCupSlotIndex;      /* 4: 0=major trophy, 1=cup bonus 1, 2=cup bonus 2 */
    uint16_t seasonNumberOfSquads;  /* 6: squad count when playing a season */
} CareerSeasonState;

_Static_assert(sizeof(CareerSeasonState) == 8,
               "CareerSeasonState must stay 8 bytes (4 x uint16)");
_Static_assert(offsetof(CareerSeasonState, seasonFinished)       == 0, "ss layout");
_Static_assert(offsetof(CareerSeasonState, seasonNumberOfSquads) == 6, "ss layout");

extern CareerSeasonState _careerSeasonState;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "_careerSeasonState\n"
    _ASM_PFX "_careerSeasonState:\n"
    ".globl " _ASM_PFX "_seasonFinished\n"
    _ASM_PFX "_seasonFinished:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_careerFinished\n"
    _ASM_PFX "_careerFinished:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_euroCupSlotIndex\n"
    _ASM_PFX "_euroCupSlotIndex:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_seasonNumberOfSquads\n"
    _ASM_PFX "_seasonNumberOfSquads:\n"
    ".skip 2\n"
    ".text\n"
);
