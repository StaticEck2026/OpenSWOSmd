/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_team_buffers.c — UNTANGLE step 3 / step-4 phase 4 (overlay retirement).
 *
 * The player's club team (careerTeam) and the in-memory working team (currentTeam)
 * lifted out of the careerFileBuffer overlay into standalone storage — the LAST
 * two big residents of the career region. Each is a TeamFileEx (16 active
 * players) followed by 16 reserve PlayerFile entries = 1292 bytes
 * (CAREER_TEAM_BUF_SIZE). Real labels (not .equ) so every reader + the save
 * (emit_player_file_body at &careerTeam / &careerTeam+684 and the currentTeam
 * equivalents) keep working unchanged. Each overlay run becomes a .skip 1292
 * hole (no cascade).
 *
 * This was only safe AFTER the step-4 rework removed both load-bearing
 * adjacencies (see docs/CAREERTEAM_STEP4_REWORK.md):
 *   - phase 1: the "is this player inside the team buffer?" range checks now
 *     bound on CAREER_TEAM_BUF_SIZE, not the next BSS symbol
 *     (careerNumPlayers / playerPositions).
 *   - phases 2-3: DIY standings / cup brackets store a team IDENTITY (-1 ==
 *     careerTeam) resolved by a branch in diy_resolve_team(), never
 *     `&selTeamsPtr + (careerTeam offset)` — so careerTeam no longer has to
 *     live adjacent below selTeamsPtr. Functionally verified on SDL + Amiga.
 *
 * Both are &symbol-addressed everywhere (the only location references left are
 * within-buffer field accesses and == &careerTeam identity checks), so this is a
 * routine format-neutral extraction. careerTeam is declared `TeamFileEx` by some
 * consumers but the storage is the full 1292 B (684 TeamFileEx + 16 reserve
 * PlayerFile) — emitted as raw-byte labels so the typed-but-undersized externs
 * resolve correctly, exactly as when it lived in the overlay.
 */
#include "swos_types.h"
#include "swos_platform.h"
#include "game_types.h"   /* CAREER_TEAM_BUF_SIZE size lock */

_Static_assert(CAREER_TEAM_BUF_SIZE == 1292,
               "careerTeam/currentTeam buffers must stay 1292 bytes (TeamFileEx + 16 reserves)");

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "careerTeam\n"
    _ASM_PFX "careerTeam:\n"
    ".skip 1292\n"
    ".globl " _ASM_PFX "currentTeam\n"
    _ASM_PFX "currentTeam:\n"
    ".skip 1292\n"
    ".text\n"
);
