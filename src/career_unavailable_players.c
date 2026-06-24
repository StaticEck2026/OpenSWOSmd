/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_unavailable_players.c — UNTANGLE step 3 (overlay retirement, region B1).
 *
 * The career "unavailable players" tracking trio — the offer/removed-player list,
 * its per-team lookup table, and the club-name strings — plus the adjacent
 * current-match player-order array, lifted out of the careerFileBuffer overlay
 * into standalone typed storage. Real labels (not .equ) so every reader
 * (career_init / career_status / career_teams / menus) + the save FA(...)
 * descriptors keep working unchanged. The 468-byte overlay run becomes a
 * .skip 468 hole (no cascade). See project_career_overlay_extraction_recipe.
 *
 * Authoritative byte layout from SWOS2.s (BSS, ds.b):
 *     careerUnavailablePlayerList        ds.b $10  (16; 4 x {u16 teamNo, u16 ord})
 *     careerUnavailablePlayerLookupTable ds.b $1 + BSS_01_164E12 ds.b $4F  (= 80)
 *     careerUnavailablePlayerNames       ds.b $1 + BSS_01_164F66 ds.b $153 (= 340;
 *                                                  20 x 17-byte club-name entries)
 *     currentMatchPlayers                ds.b $20  (32; player-order indirection)
 *   = 468 bytes total. (The lookupTable/names labels in SWOS2.s declare 1 byte +
 *   an unnamed BSS tail; the code treats each label as the base of the full
 *   80 / 340-byte array — modelled here as full-size byte arrays.)
 *
 * Audited (no consumer edits): every access is within the symbol's own bounds —
 * list via (u16*) for 4 entries, lookupTable via base+off / (u16*), names via
 * base + idx*17 (idx < 20), currentMatchPlayers[i] (i < squad size). No raw
 * careerFileBuffer+offset, no walk into the next global. All four are
 * &symbol-addressed FA blobs (list 16 / lookup 80 / names 340 / currentMatch 32,
 * FLD_BYTES), so the save format is unchanged.
 *
 * currentMatchPlayers is a distinct concept from the unavailable-players trio;
 * it is grouped into this file only because it is the contiguous neighbour, so a
 * single overlay hole preserves all later offsets. It is emitted as its own
 * standalone label, not a member of CareerUnavailablePlayers.
 */
#include "swos_types.h"
#include "swos_platform.h"

typedef struct {
    uint8_t list[16];        /*  0  careerUnavailablePlayerList */
    uint8_t lookupTable[80]; /* 16  careerUnavailablePlayerLookupTable */
    uint8_t names[340];      /* 96  careerUnavailablePlayerNames (20 x 17) */
} CareerUnavailablePlayers;

_Static_assert(sizeof(CareerUnavailablePlayers) == 436,
               "CareerUnavailablePlayers must stay 436 bytes (overlay-exact)");
_Static_assert(offsetof(CareerUnavailablePlayers, list)        ==  0, "up layout");
_Static_assert(offsetof(CareerUnavailablePlayers, lookupTable) == 16, "up layout");
_Static_assert(offsetof(CareerUnavailablePlayers, names)       == 96, "up layout");

extern CareerUnavailablePlayers _careerUnavailablePlayers;

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    ".globl " _ASM_PFX "_careerUnavailablePlayers\n"
    _ASM_PFX "_careerUnavailablePlayers:\n"
    ".globl " _ASM_PFX "_careerUnavailablePlayerList\n"
    _ASM_PFX "_careerUnavailablePlayerList:\n"
    ".skip 16\n"
    ".globl " _ASM_PFX "_careerUnavailablePlayerLookupTable\n"
    _ASM_PFX "_careerUnavailablePlayerLookupTable:\n"
    ".skip 80\n"
    ".globl " _ASM_PFX "_careerUnavailablePlayerNames\n"
    _ASM_PFX "_careerUnavailablePlayerNames:\n"
    ".skip 340\n"
    /* adjacent neighbour, separate symbol (not part of the struct) */
    ".globl " _ASM_PFX "_currentMatchPlayers\n"
    _ASM_PFX "_currentMatchPlayers:\n"
    ".skip 32\n"
    ".text\n"
);
