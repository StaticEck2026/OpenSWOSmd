/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_settings.c — UNTANGLE step 3 (overlay retirement, region A6).
 *
 * The settings/options tail of the careerFileBuffer overlay — the random-phase
 * and season-playing flags, friendly/national-team toggles, game type, the six
 * USER-editable tactics tables (teamTacticsUSERA..F), the saved options block
 * (gameLength..pitchType) and the Rand2 generator state (seed2..randomSeed) —
 * lifted into one standalone typed `CareerSettings` struct. Real labels (not
 * .equ) so every reader (edit_tactics / menus / game_init / random) + the save
 * FX(...)/FA(...) descriptors keep working unchanged. Overlay run becomes a
 * .skip 2250 hole (no cascade). See project_career_overlay_extraction_recipe.
 *
 * Authoritative byte layout from SWOS2.s (BSS, ds.b):
 *     viewRandomPhase    ds.b $1   seasonPlaying     ds.b $1   (uint8, FLD_U8)
 *     inFriendlyMenu     ds.b $2   isNationalTeam    ds.b $2
 *     gameType           ds.b $2   contestStarted    ds.b $2
 *     careerLoadedFlag   ds.b $2
 *     teamTacticsUSERA..F ds.b $172 each  (= 370; six TeamTactics records)
 *     gameLength autoReplays menuMusic autoSaveHighlights allPlayerTeamsEqual
 *     pitchType          ds.b $2 each  (the 6 uint16 SaveOptions/RestoreOptions
 *                                       copy as one block from &gameLength)
 *     seed2 randXorIndex2 randXorKey2 newRandomSeedCopy ds.b $1 each (uint8)
 *     randomSeed         ds.b $2
 *   = 2250 bytes total.
 *
 * Audited (no consumer edits): teamTacticsUSERA..F are addressed individually
 * (&teamTacticsUSERA … &teamTacticsUSERF collected into pointer tables in
 * menus.c / edit_tactics.c / tactics_data.c — never a &USERA + i*370 walk), so
 * no cross-table adjacency is required; the only intentional adjacency walk is
 * SaveOptions/RestoreOptions over the six gameLength..pitchType uint16, which
 * this struct keeps contiguous. All fields are &symbol-addressed in the save
 * (FX scalars + FA team_tactics_user_* 370 FLD_BYTES), so the format is
 * unchanged; no raw careerFileBuffer+offset. sizeof(TeamTactics)==370 is
 * compile-locked in game_types.h; the standalone view uses byte arrays so this
 * file stays self-contained (consumers keep the typed `TeamTactics` view via
 * their own externs). `packed` so the field offsets match the real-label byte
 * layout (locked by the _Static_asserts).
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint8_t  viewRandomPhase;            /*    0 */
    uint8_t  seasonPlaying;              /*    1 */
    uint16_t inFriendlyMenu;             /*    2 */
    uint16_t isNationalTeam;             /*    4 */
    uint16_t gameType;                   /*    6 */
    uint16_t contestStarted;             /*    8 */
    uint16_t careerLoadedFlag;           /*   10 */
    uint8_t  teamTacticsUSERA[370];      /*   12 */
    uint8_t  teamTacticsUSERB[370];      /*  382 */
    uint8_t  teamTacticsUSERC[370];      /*  752 */
    uint8_t  teamTacticsUSERD[370];      /* 1122 */
    uint8_t  teamTacticsUSERE[370];      /* 1492 */
    uint8_t  teamTacticsUSERF[370];      /* 1862 */
    uint16_t gameLength;                 /* 2232 */
    uint16_t autoReplays;                /* 2234 */
    uint16_t menuMusic;                  /* 2236 */
    uint16_t autoSaveHighlights;         /* 2238 */
    uint16_t allPlayerTeamsEqual;        /* 2240 */
    uint16_t pitchType;                  /* 2242 */
    uint8_t  seed2;                      /* 2244 */
    uint8_t  randXorIndex2;              /* 2245 */
    uint8_t  randXorKey2;                /* 2246 */
    uint8_t  newRandomSeedCopy;          /* 2247 */
    uint16_t randomSeed;                 /* 2248 */
} __attribute__((packed)) CareerSettings;

_Static_assert(sizeof(CareerSettings) == 2250,
               "CareerSettings must stay 2250 bytes (overlay-exact)");
_Static_assert(offsetof(CareerSettings, viewRandomPhase)  ==    0, "set layout");
_Static_assert(offsetof(CareerSettings, careerLoadedFlag) ==   10, "set layout");
_Static_assert(offsetof(CareerSettings, teamTacticsUSERA) ==   12, "set layout");
_Static_assert(offsetof(CareerSettings, teamTacticsUSERF) == 1862, "set layout");
_Static_assert(offsetof(CareerSettings, gameLength)       == 2232, "set layout");
_Static_assert(offsetof(CareerSettings, pitchType)        == 2242, "set layout");
_Static_assert(offsetof(CareerSettings, randomSeed)       == 2248, "set layout");

extern CareerSettings _careerSettings;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "_careerSettings\n"
    _ASM_PFX "_careerSettings:\n"
    ".globl " _ASM_PFX "_viewRandomPhase\n"
    _ASM_PFX "_viewRandomPhase:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "_seasonPlaying\n"
    _ASM_PFX "_seasonPlaying:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "_inFriendlyMenu\n"
    _ASM_PFX "_inFriendlyMenu:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_isNationalTeam\n"
    _ASM_PFX "_isNationalTeam:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_gameType\n"
    _ASM_PFX "_gameType:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_contestStarted\n"
    _ASM_PFX "_contestStarted:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_careerLoadedFlag\n"
    _ASM_PFX "_careerLoadedFlag:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_teamTacticsUSERA\n"
    _ASM_PFX "_teamTacticsUSERA:\n"
    ".skip 370\n"
    ".globl " _ASM_PFX "_teamTacticsUSERB\n"
    _ASM_PFX "_teamTacticsUSERB:\n"
    ".skip 370\n"
    ".globl " _ASM_PFX "_teamTacticsUSERC\n"
    _ASM_PFX "_teamTacticsUSERC:\n"
    ".skip 370\n"
    ".globl " _ASM_PFX "_teamTacticsUSERD\n"
    _ASM_PFX "_teamTacticsUSERD:\n"
    ".skip 370\n"
    ".globl " _ASM_PFX "_teamTacticsUSERE\n"
    _ASM_PFX "_teamTacticsUSERE:\n"
    ".skip 370\n"
    ".globl " _ASM_PFX "_teamTacticsUSERF\n"
    _ASM_PFX "_teamTacticsUSERF:\n"
    ".skip 370\n"
    ".globl " _ASM_PFX "_gameLength\n"
    _ASM_PFX "_gameLength:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_autoReplays\n"
    _ASM_PFX "_autoReplays:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_menuMusic\n"
    _ASM_PFX "_menuMusic:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_autoSaveHighlights\n"
    _ASM_PFX "_autoSaveHighlights:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_allPlayerTeamsEqual\n"
    _ASM_PFX "_allPlayerTeamsEqual:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_pitchType\n"
    _ASM_PFX "_pitchType:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_seed2\n"
    _ASM_PFX "_seed2:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "_randXorIndex2\n"
    _ASM_PFX "_randXorIndex2:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "_randXorKey2\n"
    _ASM_PFX "_randXorKey2:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "_newRandomSeedCopy\n"
    _ASM_PFX "_newRandomSeedCopy:\n"
    ".skip 1\n"
    ".globl " _ASM_PFX "_randomSeed\n"
    _ASM_PFX "_randomSeed:\n"
    ".skip 2\n"
    ".text\n"
);
