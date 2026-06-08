/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_board_confidence.c — UNTANGLE step 3 (overlay retirement, pilot 5).
 *
 * The board-confidence state — split across TWO non-adjacent runs in the
 * careerFileBuffer overlay (run 1: history[0..3] + score + previous; run 2:
 * mod[0..3], value[0..1], mod[4..9]) — lifted into ONE standalone typed
 * `BoardConfidenceState` struct. The two overlay runs each become a .skip hole
 * (12 + 24 bytes) so the globals between them keep their offsets (no cascade);
 * the struct packs all 18 fields contiguously. Real labels (not .equ) so every
 * reader + the save FX(...) descriptors keep working unchanged. Audited: no
 * raw careerFileBuffer+offset, no address-of/array walks (the interleaved
 * mod/value layout is fine — they are separate named globals, not an array).
 * See project_career_overlay_extraction_recipe.
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint16_t boardConfidenceHistory0;   /* --- overlay run 1 --- */
    uint16_t boardConfidenceHistory1;
    uint16_t boardConfidenceHistory2;
    uint16_t boardConfidenceHistory3;
    uint16_t boardConfidenceScore;
    uint16_t previousBoardConfidence;
    uint16_t boardConfidenceMod0;       /* --- overlay run 2 --- */
    uint16_t boardConfidenceMod1;
    uint16_t boardConfidenceMod2;
    uint16_t boardConfidenceMod3;
    uint16_t boardConfidenceValue0;
    uint16_t boardConfidenceValue1;
    uint16_t boardConfidenceMod4;
    uint16_t boardConfidenceMod5;
    uint16_t boardConfidenceMod6;
    uint16_t boardConfidenceMod7;
    uint16_t boardConfidenceMod8;
    uint16_t boardConfidenceMod9;
} BoardConfidenceState;

_Static_assert(sizeof(BoardConfidenceState) == 36,
               "BoardConfidenceState must stay 36 bytes (18 x uint16)");
_Static_assert(offsetof(BoardConfidenceState, boardConfidenceHistory0) ==  0, "bc layout");
_Static_assert(offsetof(BoardConfidenceState, previousBoardConfidence) == 10, "bc layout");
_Static_assert(offsetof(BoardConfidenceState, boardConfidenceMod0)      == 12, "bc layout");
_Static_assert(offsetof(BoardConfidenceState, boardConfidenceMod9)      == 34, "bc layout");

extern BoardConfidenceState boardConfidenceState;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "boardConfidenceState\n"
    _ASM_PFX "boardConfidenceState:\n"
    ".globl " _ASM_PFX "boardConfidenceHistory0\n"
    _ASM_PFX "boardConfidenceHistory0:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceHistory1\n"
    _ASM_PFX "boardConfidenceHistory1:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceHistory2\n"
    _ASM_PFX "boardConfidenceHistory2:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceHistory3\n"
    _ASM_PFX "boardConfidenceHistory3:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceScore\n"
    _ASM_PFX "boardConfidenceScore:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "previousBoardConfidence\n"
    _ASM_PFX "previousBoardConfidence:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod0\n"
    _ASM_PFX "boardConfidenceMod0:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod1\n"
    _ASM_PFX "boardConfidenceMod1:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod2\n"
    _ASM_PFX "boardConfidenceMod2:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod3\n"
    _ASM_PFX "boardConfidenceMod3:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceValue0\n"
    _ASM_PFX "boardConfidenceValue0:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceValue1\n"
    _ASM_PFX "boardConfidenceValue1:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod4\n"
    _ASM_PFX "boardConfidenceMod4:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod5\n"
    _ASM_PFX "boardConfidenceMod5:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod6\n"
    _ASM_PFX "boardConfidenceMod6:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod7\n"
    _ASM_PFX "boardConfidenceMod7:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod8\n"
    _ASM_PFX "boardConfidenceMod8:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "boardConfidenceMod9\n"
    _ASM_PFX "boardConfidenceMod9:\n"
    ".skip 2\n"
    ".text\n"
);
