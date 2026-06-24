/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_season_info.c — UNTANGLE step 3 (overlay retirement, region B4).
 *
 * The per-season club history table, seasonInformationsArray[20]
 * (SeasonInformations), lifted out of the careerFileBuffer overlay into
 * standalone storage. Real label (not .equ) so every reader (statistics /
 * job_offers / menus) + the save's semantic season-info encoder keep working
 * unchanged; the overlay run becomes a conditional .skip hole (no cascade).
 * See project_career_overlay_extraction_recipe.
 *
 * Each SeasonInformations row is 106 bytes on 32-bit and 122 bytes on 64-bit
 * (it has 4 comp-name pointer fields — leagueStrPtr / strPtr1..3 — that widen
 * from 4 to 8 bytes), so the array is 20*106 = 2120 B (32-bit) / 20*122 = 2440 B
 * (64-bit). The reservation keeps the same conditional +320 (= 20*16) pad the
 * overlay used; the _Static_assert below locks the reservation to
 * 20*sizeof(SeasonInformations) on both targets.
 *
 * Format / VERIFY: the save serialises this array via emit_season_informations,
 * addressed by &seasonInformationsArray (the 4 pointer slots are emitted as
 * re-interned string pool content), so relocating the storage is format-neutral.
 * Those 4 pointer-to-disk slots used to be the ONLY bytes career_verify_ignore
 * exempted from the byte-diff; now that the array lives outside the
 * [careerFileBuffer, numSelectedTeams) VERIFY region, there is nothing left to
 * exempt — the verify call drops the ignore predicate (passes NULL) and
 * career_verify_ignore is retired (see save_format_json.c). The overlay hole
 * stays zero in both the live image and the reload, so VERIFY remains 0.
 *
 * Audited: all access is bounded row-addressing — seasonInformationsArray + off
 * (job_offers / statistics) and ((SeasonInformations *)…)[currentSeasonNumber]
 * (currentSeasonNumber < CAREER_MAX_SEASONS = 35, bounded by currentSeasonShowing) —
 * nothing walks past the array into
 * lastFinancialWeekIndex. Emitted via __asm__ real label (overlay style) so the
 * conflicting element-type externs across TUs (uint8_t[] vs SeasonInformations*)
 * resolve to raw-byte storage, exactly as when it lived in the overlay.
 */
#include "swos_types.h"
#include "swos_platform.h"
#include "game_types.h"   /* SeasonInformations — for the size lock below */

/* CAREER_MAX_SEASONS (35) seasons of history; num_seasons (career_params) selects 20 or 35
 * at runtime. The array is sized to the max; the reservation below must match. */
_Static_assert(sizeof(SeasonInformations) * 35 ==
               (UINTPTR_MAX != 0xFFFFFFFFu ? 4270u : 3710u),
               "seasonInformationsArray reservation must equal 35 * sizeof(SeasonInformations) (CAREER_MAX_SEASONS)");

extern SeasonInformations _seasonInformationsArray[35];

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 2\n"
    ".globl " _ASM_PFX "_seasonInformationsArray\n"
    _ASM_PFX "_seasonInformationsArray:\n"
    ".skip 3710\n"   /* 35 * 106 (CAREER_MAX_SEASONS rows, 32-bit struct _size) */
#if UINTPTR_MAX != 0xFFFFFFFFu
    /* SeasonInformations grows 106 -> 122 bytes per row on 64-bit (4 pointer
     * fields); reserve 35 * 16 = 560 extra bytes for the full struct size. */
    ".skip 560\n"
#endif
    ".text\n"
);
