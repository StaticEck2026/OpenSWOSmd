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

/* Standalone zero-init BSS storage — pure C (was an __asm__ .skip block;
 * converted for the WASM/Emscripten target, which rejects asm-defined data
 * symbols — see docs/WASM.md §4b). Independent globals matching the old
 * member labels' names/types/sizes; the typed base + overlay pads were unused
 * scaffolding and are dropped. Each member is &symbol-addressed in the save,
 * so the format is unchanged. */
uint16_t _worldCupHostTeam, _worldCupQualifyingTeam, _euroChampionshipQualTeam,
         _selTeamNumber, _coachOrPlayer, _careerTeamLeague, _currentDivisionIndex,
         _leagueFirstRoundNumber, _playPromotedAnimation, _playRelegatedAnimation;
