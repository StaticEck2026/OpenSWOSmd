/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* selected_teams.c — UNTANGLE step 3 (careerFileBuffer overlay retirement).
 *
 * The career-mode selected-team records lifted out of the careerFileBuffer
 * overlay into standalone storage. `selTeamsPtr` is NOT a pointer — it is the
 * base of an inline `TeamFileEx[]` array (684-byte stride); team `i` is
 * `(TeamFileEx *)&selTeamsPtr + i`. The five `selTeam*` labels are aliases into
 * record [0]'s TeamFile fields (teamControls/teamName/league/prSocksColor/
 * playerNumbers at +4/+5/+25/+30/+60) — code derives the per-record field offset
 * as `(&selTeamX - &selTeamsPtr)`. Max 32 teams (career_teams.c:262;
 * SAVE_SIZE 0x5582 = 2 + 32*684). `selectedTeams` is a typed alias at the same
 * base so new code can use named-field access.
 *
 * The run is ONE contiguous block (68400 B) because it is a load-bearing union:
 * the 32-team array (21888 B) overruns past `selTeamsPtr` into `editTeamsBuffer`
 * for teams 2..31, and career_teams.c:1660 stores selTeamsPtr-relative offsets
 * in competition-slot fields that reach into that space (see swos_globals.c:583).
 * editTeamsBuffer (edit-teams workspace) and editTeamsSaveVarsArea therefore
 * relocate together, keeping every relative offset intact. All of these are
 * &symbol-addressed and not saved (save_format_json.c:2558 drops the scratch
 * regions), so this is a routine format-neutral extraction.
 *
 * This was only safe AFTER:
 *   - phase 1 decoupled _Save/RestoreSelectedTeams from the numSelectedTeams->
 *     array adjacency (career_teams.c), and
 *   - the careerTeam step-4 rework removed the careerTeam adjacency below
 *     selTeamsPtr (diy_resolve_team identity branch). See
 *     docs/SELECTED_TEAMS_UNTANGLE.md.
 *
 * Save-format-neutral: the JSON save already serializes a typed `selected_teams`
 * array and uses identity-based teamSel. The legacy .CAR raw reader loses these
 * rosters (they land in the overlay's .skip hole) — an accepted degradation of
 * the best-effort legacy path; JSON is the format. Emitted as raw-byte labels so
 * the typed-but-undersized externs ((TeamFileEx *)&selTeamsPtr) resolve exactly
 * as when it lived in the overlay.
 *
 * Run layout (offsets from base) — must match the old overlay run exactly:
 *   +0      selectedTeams / selTeamsPtr  (TeamFileEx[] base)
 *   +4      selTeamIsComputerFlag        (TeamFile.teamControls)
 *   +5      selTeamNameBase              (TeamFile.teamName[17])
 *   +25     selTeamImportField           (TeamFile.league)
 *   +30     selTeamSocksColor            (TeamFile.prSocksColor)
 *   +60     selTeamPlayerIndices         (TeamFile.playerNumbers[16])
 *   +1368   editTeamsBuffer              (43778 B edit-teams workspace; union)
 *   +45146  editTeamsSaveVarsArea        (23254 B edit-teams save scratch)
 *   +68400  (end)
 */
#include "swos_types.h"
#include "swos_platform.h"  /* _ASM_PFX */

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    /* base — typed alias + the legacy "pointer" name (same address) */
    ".globl " _ASM_PFX "selectedTeams\n"
    _ASM_PFX "selectedTeams:\n"
    ".globl " _ASM_PFX "selTeamsPtr\n"
    _ASM_PFX "selTeamsPtr:\n"
    ".skip 4\n"
    ".globl " _ASM_PFX "selTeamIsComputerFlag\n"
    _ASM_PFX "selTeamIsComputerFlag:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "selTeamNameBase\n"
    _ASM_PFX "selTeamNameBase:\n"
    ".skip 20\n"
    ".globl " _ASM_PFX "selTeamImportField\n"
    _ASM_PFX "selTeamImportField:\n"
    ".skip 5\n"
    ".globl " _ASM_PFX "selTeamSocksColor\n"
    _ASM_PFX "selTeamSocksColor:\n"
    ".skip 30\n"
    ".globl " _ASM_PFX "selTeamPlayerIndices\n"
    _ASM_PFX "selTeamPlayerIndices:\n"
    ".skip 1308\n"
    ".globl " _ASM_PFX "editTeamsBuffer\n"
    _ASM_PFX "editTeamsBuffer:\n"
    ".skip 43778\n"
    ".globl " _ASM_PFX "editTeamsSaveVarsArea\n"
    _ASM_PFX "editTeamsSaveVarsArea:\n"
    ".skip 23254\n"
    ".text\n"
);
