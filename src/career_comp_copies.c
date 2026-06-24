/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_comp_copies.c — UNTANGLE step 3 (overlay retirement, category C).
 *
 * The 34 per-career *Copy competition records (euroCupCopy ..
 * oceaniaWorldCupQualificationCopy) lifted out of the careerFileBuffer overlay
 * into standalone storage. Each is the runtime working copy of a static
 * competition template (euroCup, cupWinnersCup, ...); career_init.c:InitCareer
 * builds them by iterating the static `careerContests[]` table
 * (competition_data.c) and memcpy-ing template -> *Copy, and the JSON save
 * RECONSTRUCTS them the same way on load (reconstruct_comp_copy_region_from_-
 * templates) rather than byte-loading them — only two dynamic bytes (the
 * worldCupQualifyingGroup8Copy / oceaniaWorldCupQualificationCopy team counts at
 * offset 0xD) are carried in the save.
 *
 * Why this is format-neutral: `careerContests[].competitionCopy` is initialised
 * with the ADDRESS of each *Copy symbol (`(CompetitionCup *)&euroCupCopy`, ...),
 * so both the InitCareer fill loop and the save reconstruction write through
 * &symbol — relocating the storage here makes those initialisers resolve to the
 * new addresses automatically. Because the records are reconstructed (not
 * byte-loaded), the overlay hole left behind stays zero in both the live image
 * and the reload, so SWOS_SAVE_FORMAT_JSON_VERIFY remains 0 by construction.
 *
 * Each record carries a `text[2]` pointer pair, so its size grows by 8 bytes on
 * 64-bit (32-bit pointers -> 64-bit). The per-record reservation therefore keeps
 * the same conditional +8 pad the overlay used; the typed externs in
 * competition_data.c (CompetitionTournament euroCupCopy, etc.) resolve to these
 * raw-byte labels, exactly as they did when the storage lived in the overlay.
 * Emitted via __asm__ real labels (matching the overlay's own style) so no
 * competition-type header include is needed here (see
 * project_career_season_includes for the include-conflict hazard).
 *
 * Region size: 1832 bytes (32-bit) + 34*8 = 2104 bytes (64-bit). The overlay run
 * is replaced by the matching conditional .skip hole (no cascade).
 */
#include "swos_types.h"
#include "swos_platform.h"

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "careerCompCopies\n"
    _ASM_PFX "careerCompCopies:\n"
    ".globl " _ASM_PFX "euroCupCopy\n"
    _ASM_PFX "euroCupCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "cupWinnersCupCopy\n"
    _ASM_PFX "cupWinnersCupCopy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "uefaCupCopy\n"
    _ASM_PFX "uefaCupCopy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "invitationTournamentCopy\n"
    _ASM_PFX "invitationTournamentCopy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "tourCopy\n"
    _ASM_PFX "tourCopy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "friendlyCopy\n"
    _ASM_PFX "friendlyCopy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "europeanChampionships2Copy\n"
    _ASM_PFX "europeanChampionships2Copy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "ecQualifyingGroup1Copy\n"
    _ASM_PFX "ecQualifyingGroup1Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "ecQualifyingGroup2Copy\n"
    _ASM_PFX "ecQualifyingGroup2Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "ecQualifyingGroup3Copy\n"
    _ASM_PFX "ecQualifyingGroup3Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "ecQualifyingGroup4Copy\n"
    _ASM_PFX "ecQualifyingGroup4Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "ecQualifyingGroup5Copy\n"
    _ASM_PFX "ecQualifyingGroup5Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "ecQualifyingGroup6Copy\n"
    _ASM_PFX "ecQualifyingGroup6Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "ecQualifyingGroup7Copy\n"
    _ASM_PFX "ecQualifyingGroup7Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "ecQualifyingGroup8Copy\n"
    _ASM_PFX "ecQualifyingGroup8Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "copaAmericaCopy\n"
    _ASM_PFX "copaAmericaCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "africanNationsCupCopy\n"
    _ASM_PFX "africanNationsCupCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "northAmericanCupCopy\n"
    _ASM_PFX "northAmericanCupCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "asianCupCopy\n"
    _ASM_PFX "asianCupCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "oceaniaCupCopy\n"
    _ASM_PFX "oceaniaCupCopy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCup2Copy\n"
    _ASM_PFX "worldCup2Copy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCupQualifyingGroup1Copy\n"
    _ASM_PFX "worldCupQualifyingGroup1Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCupQualifyingGroup2Copy\n"
    _ASM_PFX "worldCupQualifyingGroup2Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCupQualifyingGroup3Copy\n"
    _ASM_PFX "worldCupQualifyingGroup3Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCupQualifyingGroup4Copy\n"
    _ASM_PFX "worldCupQualifyingGroup4Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCupQualifyingGroup5Copy\n"
    _ASM_PFX "worldCupQualifyingGroup5Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCupQualifyingGroup6Copy\n"
    _ASM_PFX "worldCupQualifyingGroup6Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCupQualifyingGroup7Copy\n"
    _ASM_PFX "worldCupQualifyingGroup7Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "worldCupQualifyingGroup8Copy\n"
    _ASM_PFX "worldCupQualifyingGroup8Copy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "northAmericaWorldCupQualificationCopy\n"
    _ASM_PFX "northAmericaWorldCupQualificationCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "southAmericaWorldCupQualificationCopy\n"
    _ASM_PFX "southAmericaWorldCupQualificationCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "africaWorldCupQualificationCopy\n"
    _ASM_PFX "africaWorldCupQualificationCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "asiaWorldCupQualificationCopy\n"
    _ASM_PFX "asiaWorldCupQualificationCopy:\n"
    ".skip 112\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".globl " _ASM_PFX "oceaniaWorldCupQualificationCopy\n"
    _ASM_PFX "oceaniaWorldCupQualificationCopy:\n"
    ".skip 92\n"
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".skip 8\n"   /* text[2] 64-bit growth */
#endif
    ".text\n"
);
