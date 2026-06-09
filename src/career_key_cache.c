/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* career_key_cache.c — UNTANGLE step 3 / teamSelectionKeyCache cluster Tier 2
 * (overlay retirement).
 *
 * The teamSelectionKeyCache cluster lifted OUT of the careerFileBuffer overlay
 * into a dedicated, bounded buffer. In SWOS2.s (129242-129261) this was one BSS
 * run where the cache OVERLAPPED careerForeignMarketPlayers: the cache builder's
 * overflow bound is `&teamSelectionKeyCacheEnd`, which sat 20014 bytes past the
 * cache start — straight through the foreign-market region. A career-mode
 * 22-byte-slot overflow could therefore stomp live player data (the documented
 * project_cache_overlay_writer_overflow bug). Here the cache gets its OWN
 * buffer and careerForeignMarketPlayers lives in career_foreign_market.c, so the
 * two can never overlap; an overflow now stays in the cache's own dead tail
 * (clamped by the stride bounds-check in the three cache walkers).
 *
 * Buffer size = 20014 bytes: the EXACT span the original allowed the cache,
 * i.e. teamSelectionKeyCache -> &teamSelectionKeyCacheEnd in SWOS2.s. Keeping it
 * identical means _TeamSelectionKeyCache_InsertSpaceSorted's `ptr >
 * &teamSelectionKeyCacheEnd` panic (0x8889) fires at exactly the same point the
 * original did — no behaviour change to the bound, we only stop foreign-market
 * sharing the slack. (Real cache usage is far below this: the high-water-mark
 * probe measured ~1188 bytes over full careers.)
 *
 * careerJobOfferPhaseBias was byte 1 of the cache buffer in SWOS2.s
 * (`teamSelectionKeyCache: ds.b $1` then `careerJobOfferPhaseBias: ds.b $1`) —
 * Amiga memory-reuse across disjoint career phases (job-offers vs cache build),
 * the cache builder overwriting it as entry-0's sortKey. Nothing reads it as
 * cache data or reads the cache byte as the bias, so it is promoted here to an
 * independent scalar (removes a latent clobber footgun). It now has its own
 * save descriptor; old JSON saves that carried it as cache-blob byte 1 load it
 * as 0 and it is re-derived in _InitCareer.
 *
 * Real labels (not .equ) so every reader and the save FX/FA descriptors
 * (addressed by &symbol) keep working unchanged.
 *
 * See project_keycache_cluster_design / docs/KEYCACHE_CLUSTER_REWORK.md.
 */
#include "swos_types.h"
#include "swos_platform.h"

#define KEY_CACHE_BUF_SIZE 20014   /* SWOS2.s teamSelectionKeyCache->End span */

extern uint8_t  teamSelectionKeyCache[KEY_CACHE_BUF_SIZE];

_Static_assert(sizeof(teamSelectionKeyCache) == 20014,
               "teamSelectionKeyCache buffer must stay 20014 bytes "
               "(SWOS2.s InsertSpaceSorted bound span)");

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    /* ---- walk cursors (4 x uint16) ---- */
    ".globl " _ASM_PFX "teamSelectionKeyCacheIndex\n"
    _ASM_PFX "teamSelectionKeyCacheIndex:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "teamSelectionKeyCacheOffset\n"
    _ASM_PFX "teamSelectionKeyCacheOffset:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "teamSelectionKeyCacheCount\n"
    _ASM_PFX "teamSelectionKeyCacheCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "careerSlotProcessingInProgress\n"
    _ASM_PFX "careerSlotProcessingInProgress:\n"
    ".skip 2\n"
    /* ---- dedicated cache buffer (20014 B) ---- */
    ".globl " _ASM_PFX "teamSelectionKeyCache\n"
    _ASM_PFX "teamSelectionKeyCache:\n"
    ".skip 20014\n"
    /* ---- end marker: &it = cache+20014 = InsertSpaceSorted bound; value = season flag ---- */
    ".globl " _ASM_PFX "teamSelectionKeyCacheEnd\n"
    _ASM_PFX "teamSelectionKeyCacheEnd:\n"
    ".skip 2\n"
    /* ---- careerJobOfferPhaseBias: promoted standalone scalar (was cache byte 1) ---- */
    ".globl " _ASM_PFX "careerJobOfferPhaseBias\n"
    _ASM_PFX "careerJobOfferPhaseBias:\n"
    ".skip 1\n"
    ".skip 1\n"   /* pad: keep block even */
    ".text\n"
);
