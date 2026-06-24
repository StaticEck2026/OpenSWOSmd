/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_season_record.c — UNTANGLE step 3 (incremental overlay retirement, pilot 2).
 *
 * The 6 consecutive 16-bit career season match-record counters used to live as a
 * flat run of real labels INSIDE the careerFileBuffer overlay
 * (swos_globals_career_overlay.c): gamesPlayed, numWins, numDrawn, numLost,
 * goalsScored, goalsTaken — 12 contiguous bytes, the player's running
 * win/draw/loss + goals-for/against record.
 *
 * Lifted OUT of the 256 KB overlay into a standalone, typed `CareerSeasonRecord`
 * struct — the same move careerFinances / primaryCompetitionBuf already made (see
 * project_career_overlay_extraction_recipe). Real labels (not `.equ` aliases) so
 * the 6 legacy names stay real globals: every reader (career_init/career_status/
 * statistics/match_sim/goal_scored/game_loop) and the save FX(...) descriptors
 * (addressed via &symbol) keep working with ZERO changes. macOS-safe — see the
 * career_finances.c header comment for why real labels, not `.equ`.
 *
 * The overlay run is replaced by one `.skip 12` so every later overlay label
 * keeps its absolute offset (no cascade); numSelectedTeams stays put, so
 * career_data_size() and the on-disk save layout are unchanged.
 */

#include "swos_types.h"     /* uint16_t, offsetof */
#include "swos_platform.h"  /* _ASM_PFX */

/* Typed view over the 12-byte season-record region. Field order MUST match the
 * real-label sequence in the __asm__ block (and the historical overlay order). */
typedef struct {
    uint16_t gamesPlayed;
    uint16_t numWins;
    uint16_t numDrawn;
    uint16_t numLost;
    uint16_t goalsScored;
    uint16_t goalsTaken;
} CareerSeasonRecord;

_Static_assert(sizeof(CareerSeasonRecord) == 12,
               "CareerSeasonRecord must stay 12 bytes (6 x uint16, overlay-exact)");
_Static_assert(offsetof(CareerSeasonRecord, gamesPlayed)  ==  0, "rec layout");
_Static_assert(offsetof(CareerSeasonRecord, numWins)      ==  2, "rec layout");
_Static_assert(offsetof(CareerSeasonRecord, numDrawn)     ==  4, "rec layout");
_Static_assert(offsetof(CareerSeasonRecord, numLost)      ==  6, "rec layout");
_Static_assert(offsetof(CareerSeasonRecord, goalsScored)  ==  8, "rec layout");
_Static_assert(offsetof(CareerSeasonRecord, goalsTaken)   == 10, "rec layout");

/* Typed base symbol (offset 0, coincident with gamesPlayed). Available for
 * new/migrated code; the flat field labels below remain valid until consumers
 * move to careerSeasonRecord.<field>. */
extern CareerSeasonRecord _careerSeasonRecord;

/* Standalone backing storage — zero-init BSS, real labels at each field offset
 * (see header comment for why real labels, not .equ). Mirrors the section
 * handling of careerFileBufferBuf / careerFinances. */
__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#elif defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
    ".section .bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "_careerSeasonRecord\n"
    _ASM_PFX "_careerSeasonRecord:\n"
    ".globl " _ASM_PFX "_gamesPlayed\n"
    _ASM_PFX "_gamesPlayed:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_numWins\n"
    _ASM_PFX "_numWins:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_numDrawn\n"
    _ASM_PFX "_numDrawn:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_numLost\n"
    _ASM_PFX "_numLost:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_goalsScored\n"
    _ASM_PFX "_goalsScored:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_goalsTaken\n"
    _ASM_PFX "_goalsTaken:\n"
    ".skip 2\n"
    ".text\n"
);
