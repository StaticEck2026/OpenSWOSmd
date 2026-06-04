/*
 * save_pointer_lut.c — runtime translator for save-file pointer fields.
 *
 * The original Amiga binary writes `.CAR` / `.SEA` / `.PRE` / `.DIY`
 * files containing absolute Amiga addresses for every pointer-typed
 * field. Our SDL build's BSS lives at completely different addresses,
 * so dereferencing a saved value would either fault or pick up
 * garbage. This module translates each such pointer through the
 * generated LUT in `src/generated/amiga_address_map.c` — an array of
 * { name, &symbol_in_this_build, amiga_addr, size } triples — back to
 * the equivalent address in the current build's BSS.
 *
 * Read-only for now: we only translate on load. The save path keeps
 * writing current-build addresses (so SDL→SDL round-trips still work
 * via the existing `_RebuildPointersAfterLoad` heuristics). When we
 * add write support, _SaveCareerFile will translate
 * current→amiga before writing, and the saved file becomes truly
 * portable across all binaries (Amiga / SDL / DOS).
 *
 * See SAVE_DATA.md "Cross-build pointer LUT" for the design.
 */
#include "swos_types.h"

#include "game_types.h"

#ifndef SWOS_SDL_BUILD
/* On Amiga the BSS *is* the Amiga BSS — saved pointers already
 * resolve to live symbols, no translation needed. Provide an empty
 * stub for the one public entrypoint so file_io.c links. */
void _TranslateAmigaAddressesAfterLoad(void) { }
#else

typedef struct AmigaAddrLutEntry {
    const char *name;
    void       *current_addr;
    uint32_t    amiga_addr;
    uint32_t    size;
} AmigaAddrLutEntry;

extern const AmigaAddrLutEntry g_amiga_addr_map[];
extern const size_t g_amiga_addr_map_count;

/* Heuristic: an Amiga `.CAR` written by the original (or by a
 * pre-LUT SDL build) puts pointer values in the original Amiga
 * address space. Section 01 (text/data) lives in [0x00100000,
 * 0x00200000) per SWOS2.lst's section header. Section 02 (bss) is in
 * the same range. SDL/Linux build addresses are typically much
 * higher (e.g. 0x55xxxxxx user-space) and won't collide. We only
 * try to translate when the saved value falls in the Amiga window,
 * so SDL→SDL saves pass through the LUT untouched. */
static int looks_like_amiga_address(uint32_t v)
{
    return v >= 0x00100000u && v < 0x00200000u;
}

/* Map an Amiga address to the current build's address of the same
 * symbol. Handles both exact-label hits and offsets within a
 * labeled block (pointer = block_base + N, where N < block.size).
 * Returns NULL if no match — caller should leave the pointer alone. */
static void *current_for_amiga(uint32_t amiga_addr)
{
    /* Linear scan over ~230 entries. Could be sorted+bsearch later;
     * load happens once per career open, so it's not hot. */
    for (size_t i = 0; i < g_amiga_addr_map_count; i++) {
        const AmigaAddrLutEntry *e = &g_amiga_addr_map[i];
        if (amiga_addr == e->amiga_addr) {
            return e->current_addr;
        }
        if (e->size > 0 &&
            amiga_addr > e->amiga_addr &&
            amiga_addr < e->amiga_addr + e->size) {
            uint32_t off = amiga_addr - e->amiga_addr;
            return (void *)((uint8_t *)e->current_addr + off);
        }
    }
    return (void *)0;
}

/* Translate one saved 32-bit pointer field in place. Leaves the
 * value unchanged if it doesn't look like an Amiga address or if no
 * matching symbol is in the LUT (in which case the legacy
 * _RebuildPointersAfterLoad heuristics still run as a fallback). */
static void translate_one(uint32_t *p)
{
    if (!p) return;
    uint32_t val = *p;
    if (val == 0u || val == 0xFFFFFFFFu)
        return;
    if (!looks_like_amiga_address(val))
        return;
    void *current = current_for_amiga(val);
    if (current) {
        *p = (uintptr_t)current;
    }
}

/* ---- External symbols we walk after load ---- */
/* Phase 2 Wave 2: these are now uint8_t * but save_pointer_lut treats
 * every 4-byte slot as an opaque address — translate_one rewrites the
 * value via the LUT. The (uint32_t *) casts at the call sites below
 * are valid because both types are 4 bytes on 32-bit hosts. */
extern uint8_t  *currentCompetitionPtr;
extern uint8_t  *secondaryCompetitionPtr;
extern uint8_t  *tertiaryCompetitionPtr;
extern uint8_t  *quaternaryCompetitionPtr;
extern uint8_t  *diyCompetitionPtr;
extern uint8_t  *careerCompetitionSlotPtrTable[];
extern uint8_t  *primaryCompStringOffset;
extern uint8_t  *secondaryCompStringOffset;
extern uint8_t  *tertiaryCompStringOffset;
extern uint8_t  *quaternaryCompStringOffset;
/* careerCupPickList moved to a standalone void *[5] (re-derived on load) —
 * no longer translated here; see _TranslateAmigaAddressesAfterLoad. */
extern uint32_t *diyCompetitionTablePtr;
extern CompetitionCareer careerContests[];
extern uint16_t  numSelectedTeams;

/*
 * _TranslateAmigaAddressesAfterLoad
 *
 * Walks every pointer field in the just-loaded careerFileBuffer
 * overlay and translates Amiga-range values via the LUT. Runs BEFORE
 * `_RebuildPointersAfterLoad` so the rebuild step sees correctly-
 * translated pointers and only has to fix up the few fields the LUT
 * can't reach (runtime ptrs in TeamFile slots, *Copy embedded
 * nameSource, etc.).
 *
 * Per-field strategy:
 *   - top-level competition pointers: in-place translate.
 *   - careerCompetitionSlotPtrTable[5]: in-place translate.
 *   - careerCupPickList[5]: NOT translated (standalone, re-derived on load).
 *   - *CompStringOffset (4 fields): in-place translate.
 *   - diyCompetitionTablePtr: in-place translate.
 *   - cup-record `nameSource` ptrs inside *Copy slots: walk via
 *     careerContests, translate each copy's (5+nameOffset) field.
 *
 * NOT translated here (handled by _RebuildPointersAfterLoad):
 *   - TeamFile runtime pointers in the body region — zeroed at match
 *     start anyway.
 */
__attribute__((used, externally_visible))
void _TranslateAmigaAddressesAfterLoad(void)
{
    /* Top-level slot pointers. */
    translate_one((uint32_t *)&currentCompetitionPtr);
    translate_one((uint32_t *)&secondaryCompetitionPtr);
    translate_one((uint32_t *)&tertiaryCompetitionPtr);
    translate_one((uint32_t *)&quaternaryCompetitionPtr);
    translate_one((uint32_t *)&diyCompetitionPtr);

    /* careerCompetitionSlotPtrTable is uint8_t*[5]. */
    for (int i = 0; i < 5; i++) {
        translate_one((uint32_t *)&careerCompetitionSlotPtrTable[i]);
    }

    /* careerCupPickList is NOT translated here: it is now a standalone
     * `void *[5]` (moved out of the careerFileBuffer overlay) holding host
     * competition pointers, and is re-derived from non-pointer state by
     * _RebuildPointersAfterLoad. Legacy Amiga-pointer translation of it is
     * both unnecessary (re-derived) and type-mismatched (pointer-sized now,
     * not uint32). */

    /* String-table offsets — saved as raw pointers on Amiga. */
    translate_one((uint32_t *)&primaryCompStringOffset);
    translate_one((uint32_t *)&secondaryCompStringOffset);
    translate_one((uint32_t *)&tertiaryCompStringOffset);
    translate_one((uint32_t *)&quaternaryCompStringOffset);

    /* diyCompetitionTablePtr is `uint32_t **` but holds a single
     * pointer value — translate the value field. */
    translate_one((uint32_t *)&diyCompetitionTablePtr);

    /* Cup-record `nameSource` pointers inside each *Copy slot.
     * Each careerContests entry is { src_static, dest_copy, len }
     * (10 bytes) terminated by 0xFFFFFFFF. The cup record's
     * `nameOffset` byte at +5 says where the 4-byte `nameSource`
     * pointer field lives within the record (record + 5 +
     * nameOffset). Translate in-place; this also makes the legacy
     * `_Rebuild_CupRecordNameSourcePointers` step idempotent. */
    {
        /* Iterate CompetitionCareer[] as the typed array; stride
         * differs by bitness (10 B / 18 B). */
        extern CompetitionCareer careerContests[];
        for (CompetitionCareer *e = careerContests;
             (uintptr_t)e->competition != 0xFFFFFFFFu; e++) {
            uint8_t *dest = (uint8_t *)e->competitionCopy;
            int8_t nameOffset = (int8_t)dest[5];
            if (nameOffset != 0) {
                translate_one((uint32_t *)(dest + 5 + nameOffset));
            }
        }
    }
}

#endif /* SWOS_SDL_BUILD */
