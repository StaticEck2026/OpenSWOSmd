/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_status_flags.c — UNTANGLE step 3 (overlay retirement, pilot 3).
 *
 * 12 consecutive uint16 career-status flags lifted out of the careerFileBuffer
 * overlay into a standalone typed `CareerStatusFlags` struct. Real labels (not
 * .equ) so every reader + the save FX(...) descriptors keep working unchanged.
 * The overlay run becomes a .skip 24 hole (no cascade). See
 * project_career_overlay_extraction_recipe and the career_finances.c header for
 * the why (macOS-safe real labels). Audited: no raw careerFileBuffer+offset, no
 * address-of/array walks; all 12 are FX FLD_U16 (save format unchanged).
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint16_t currentSeasonShowing;
    uint16_t stayingInOldClub;
    uint16_t overdraftStreak;
    uint16_t overdraftWarningState;
    uint16_t performanceWarningState;
    uint16_t overdraftWarningCooldown;
    uint16_t performanceWarningCooldown;
    uint16_t youreSacked;
    uint16_t retired;
    uint16_t competitionSlotUpdatedFlag;
    uint16_t careerTeamLostMatch;
    uint16_t clubBusinessActive;
} CareerStatusFlags;

_Static_assert(sizeof(CareerStatusFlags) == 24,
               "CareerStatusFlags must stay 24 bytes (12 x uint16, overlay-exact)");
_Static_assert(offsetof(CareerStatusFlags, currentSeasonShowing) ==  0, "flags layout");
_Static_assert(offsetof(CareerStatusFlags, clubBusinessActive)   == 22, "flags layout");

extern CareerStatusFlags careerStatusFlags;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "careerStatusFlags\n"
    _ASM_PFX "careerStatusFlags:\n"
    ".globl " _ASM_PFX "currentSeasonShowing\n"
    _ASM_PFX "currentSeasonShowing:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "stayingInOldClub\n"
    _ASM_PFX "stayingInOldClub:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "overdraftStreak\n"
    _ASM_PFX "overdraftStreak:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "overdraftWarningState\n"
    _ASM_PFX "overdraftWarningState:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "performanceWarningState\n"
    _ASM_PFX "performanceWarningState:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "overdraftWarningCooldown\n"
    _ASM_PFX "overdraftWarningCooldown:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "performanceWarningCooldown\n"
    _ASM_PFX "performanceWarningCooldown:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "youreSacked\n"
    _ASM_PFX "youreSacked:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "retired\n"
    _ASM_PFX "retired:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "competitionSlotUpdatedFlag\n"
    _ASM_PFX "competitionSlotUpdatedFlag:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "careerTeamLostMatch\n"
    _ASM_PFX "careerTeamLostMatch:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "clubBusinessActive\n"
    _ASM_PFX "clubBusinessActive:\n"
    ".skip 2\n"
    ".text\n"
);
