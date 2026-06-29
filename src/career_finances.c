/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_finances.c — UNTANGLE step 3 (incremental overlay retirement, pilot 1).
 *
 * The 11 consecutive 32-bit club-finance totals used to live as a flat run
 * of real labels INSIDE the careerFileBuffer overlay
 * (swos_globals_career_overlay.c): newBalance, old_balance, playerPurchases,
 * playerSales, generalRunningCosts, playerWagesBill, gateReceipts,
 * startingBalance, startingBalanceCopy, competitionBonuses,
 * sensisoftPlcSponsorship — 44 contiguous bytes, the club-finances summary.
 *
 * This is the first region lifted OUT of the 256 KB overlay into its own
 * standalone, typed home — the same move primaryCompetitionBuf /
 * careerCupPickList already made. The region is now backed by a single
 * `CareerFinances` struct so the memory is named and self-describing
 * (long-term roadmap step 3), and isolated so future overlay layout shifts
 * can never slide it.
 *
 * Why an __asm__ block with real labels instead of a plain C struct + `.equ`
 * aliases (the swos_competition_slots.c style): the macOS assembler silently
 * drops `.equ alias-with-offset` against an external/relocatable carrier,
 * leaving each alias undefined at link time (see the swos_globals_career_overlay.c
 * header comment). Emitting `careerFinances` and the 11 legacy field names as
 * REAL labels at their offsets inside one BSS reservation avoids that entirely
 * and works on ELF (Linux x86, m68k-amiga-elf) AND Mach-O.
 *
 * The 11 legacy names stay real globals, so every existing reader
 * (career_status.c, transfer_market.c, career_init.c, string_utils.c) and the
 * save-format descriptor table (save_format_json.c FX(...) entries, addressed
 * via &symbol) keep working with ZERO changes — `&newBalance` simply resolves
 * to its new address. `careerFinances` is the typed base for new/migrated code
 * to use `careerFinances.<field>` directly; the flat aliases retire field-by-
 * field once consumers migrate.
 *
 * The overlay run is replaced by one `.skip 44` so every later overlay label
 * keeps its absolute careerFileBufferBuf offset — no cascade, save layout
 * unchanged (numSelectedTeams stays put, so career_data_size() is identical).
 */

#include "swos_types.h"     /* int32_t, offsetof, UINTPTR_MAX */
#include "swos_platform.h"  /* _ASM_PFX */

/* Typed view over the 44-byte finance region. Field order MUST match the
 * real-label sequence in the __asm__ block below (and the historical overlay
 * order) — the labels define the storage, this struct is the typed window
 * onto the same bytes. All fields are int32 so the layout is padding-free and
 * identical on 32- and 64-bit. */
typedef struct {
    int32_t newBalance;
    int32_t old_balance;
    int32_t playerPurchases;
    int32_t playerSales;
    int32_t generalRunningCosts;
    int32_t playerWagesBill;
    int32_t gateReceipts;
    int32_t startingBalance;
    int32_t startingBalanceCopy;
    int32_t competitionBonuses;
    int32_t sensisoftPlcSponsorship;
} CareerFinances;

/* Lock the layout: the struct must be exactly the 44 bytes the overlay
 * reserved, with each field at the offset its legacy label sits at, or the
 * typed view and the flat aliases would disagree. */
_Static_assert(sizeof(CareerFinances) == 44,
               "CareerFinances must stay 44 bytes (11 x int32, overlay-exact)");
_Static_assert(offsetof(CareerFinances, newBalance)              ==  0, "fin layout");
_Static_assert(offsetof(CareerFinances, old_balance)             ==  4, "fin layout");
_Static_assert(offsetof(CareerFinances, playerPurchases)         ==  8, "fin layout");
_Static_assert(offsetof(CareerFinances, playerSales)             == 12, "fin layout");
_Static_assert(offsetof(CareerFinances, generalRunningCosts)     == 16, "fin layout");
_Static_assert(offsetof(CareerFinances, playerWagesBill)         == 20, "fin layout");
_Static_assert(offsetof(CareerFinances, gateReceipts)            == 24, "fin layout");
_Static_assert(offsetof(CareerFinances, startingBalance)         == 28, "fin layout");
_Static_assert(offsetof(CareerFinances, startingBalanceCopy)     == 32, "fin layout");
_Static_assert(offsetof(CareerFinances, competitionBonuses)      == 36, "fin layout");
_Static_assert(offsetof(CareerFinances, sensisoftPlcSponsorship) == 40, "fin layout");

/* Typed base symbol (offset 0, coincident with newBalance). Available for
 * new/migrated code; flat aliases below remain valid until consumers move. */
/* Standalone backing storage — zero-init BSS, real labels at each field
 * offset (see header comment for why real labels, not .equ). Mirrors the
 * section/alignment handling of careerFileBufferBuf in
 * swos_globals_career_overlay.c. */
/* Standalone zero-init BSS storage — pure C (was an __asm__ .skip block;
 * converted for the WASM/Emscripten target, which rejects asm-defined data
 * symbols — see docs/WASM.md §4b). Independent globals matching the old
 * member labels' names/types/sizes; the typed base + overlay pads were unused
 * scaffolding and are dropped. Each member is &symbol-addressed in the save,
 * so the format is unchanged. */
int32_t _newBalance, _old_balance, _playerPurchases, _playerSales,
        _generalRunningCosts, _playerWagesBill, _gateReceipts, _startingBalance,
        _startingBalanceCopy, _competitionBonuses, _sensisoftPlcSponsorship;
