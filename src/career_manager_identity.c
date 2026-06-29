/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_manager_identity.c — UNTANGLE step 3 (overlay retirement, pilot 4).
 *
 * The manager-identity block (mrOrMs toggle + the MR/MS, first-name and surname
 * text buffers + player nationality) lifted out of the careerFileBuffer overlay
 * into a standalone typed `ManagerIdentity` struct. Real labels (not .equ) so
 * every reader (career_init / chairman_memo) + the save FX(...) descriptors keep
 * working unchanged. Overlay run becomes a .skip 30 hole (no cascade). Audited:
 * name/surname are SEPARATE StringCopy targets (no cross-field adjacency), no
 * raw careerFileBuffer+offset. See project_career_overlay_extraction_recipe.
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint16_t mrOrMs;
    uint8_t  managerSexBuffer[4];      /* "MR" / "MS" */
    uint8_t  managerNameBuffer[9];
    uint8_t  managerSurnameBuffer[13];
    uint16_t plNationality;
} ManagerIdentity;

_Static_assert(sizeof(ManagerIdentity) == 30,
               "ManagerIdentity must stay 30 bytes (overlay-exact)");
_Static_assert(offsetof(ManagerIdentity, mrOrMs)               ==  0, "mgr layout");
_Static_assert(offsetof(ManagerIdentity, managerSexBuffer)     ==  2, "mgr layout");
_Static_assert(offsetof(ManagerIdentity, managerNameBuffer)    ==  6, "mgr layout");
_Static_assert(offsetof(ManagerIdentity, managerSurnameBuffer) == 15, "mgr layout");
_Static_assert(offsetof(ManagerIdentity, plNationality)        == 28, "mgr layout");

/* Standalone zero-init BSS storage — pure C (was an __asm__ .skip block;
 * converted for the WASM/Emscripten target, which rejects asm-defined data
 * symbols — see docs/WASM.md §4b). Independent globals matching the old
 * member labels' names/types/sizes; the typed base + overlay pads were unused
 * scaffolding and are dropped. Each member is &symbol-addressed in the save,
 * so the format is unchanged. */
uint16_t _mrOrMs;
uint8_t  _managerSexBuffer[4];      /* "MR" / "MS" */
uint8_t  _managerNameBuffer[9];
uint8_t  _managerSurnameBuffer[13];
uint16_t _plNationality;
