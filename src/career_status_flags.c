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

/* Standalone zero-init BSS storage — pure C (was an __asm__ .skip block;
 * converted for the WASM/Emscripten target, which rejects asm-defined data
 * symbols — see docs/WASM.md §4b). Independent globals matching the old
 * member labels' names/types/sizes; the typed base + overlay pads were unused
 * scaffolding and are dropped. Each member is &symbol-addressed in the save,
 * so the format is unchanged. */
uint16_t _currentSeasonShowing, _stayingInOldClub, _overdraftStreak,
         _overdraftWarningState, _performanceWarningState, _overdraftWarningCooldown,
         _performanceWarningCooldown, _youreSacked, _retired,
         _competitionSlotUpdatedFlag, _careerTeamLostMatch, _clubBusinessActive;
