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
 * Buffer size = 8192 bytes: a tight, HONEST bound. The original gave the cache
 * the full 20014-byte span to &teamSelectionKeyCacheEnd (straight through the
 * foreign-market region) as accidental slack; once the two no longer share
 * memory that 18 KB of dead tail only served to SWALLOW a genuine overflow
 * instead of tripping TeamSelectionKeyCache_InsertSpaceSorted's `ptr >
 * &teamSelectionKeyCacheEnd` panic (0x8889) — counter to "panics are signals".
 * The high-water-mark probe measured real usage at ~1188 bytes over full
 * careers, and the analytical heavy case (24-team league + domestic cup +
 * European cup, KEYCACHE_CLUSTER_REWORK.md §8.1) tops out ~2.5 KB. 8192 is ~7x
 * the measured HWM / ~3x the analytical worst case — ample headroom — while
 * making the 0x8889 bound fire loudly on any real overflow rather than eating
 * 18 KB of player-data-free slack first. The 1870-byte save blob (TSKC_SIZE)
 * and the stride-clamped walkers are unaffected (both << 8192). This is the
 * Tier 2 §8.3 finish; the deferred per-record writer fix is still a follow-up.
 *
 * careerJobOfferPhaseBias was byte 1 of the cache buffer in SWOS2.s
 * (`teamSelectionKeyCache: ds.b $1` then `careerJobOfferPhaseBias: ds.b $1`) —
 * Amiga memory-reuse across disjoint career phases (job-offers vs cache build),
 * the cache builder overwriting it as entry-0's sortKey. Nothing reads it as
 * cache data or reads the cache byte as the bias, so it is promoted here to an
 * independent scalar (removes a latent clobber footgun). It now has its own
 * save descriptor; old JSON saves that carried it as cache-blob byte 1 load it
 * as 0 and it is re-derived in InitCareer.
 *
 * Real labels (not .equ) so every reader and the save FX/FA descriptors
 * (addressed by &symbol) keep working unchanged.
 *
 * See project_keycache_cluster_design / docs/KEYCACHE_CLUSTER_REWORK.md.
 */
#include "swos_types.h"
#include "swos_platform.h"

/* Tightened from the SWOS2.s 20014-byte teamSelectionKeyCache->End span to an
 * honest bound now that foreign-market no longer shares the slack — see the
 * file header. ~7x the measured ~1188 B HWM. */
#define KEY_CACHE_BUF_SIZE 8192
/* Stringize so the BSS `.skip` below reserves EXACTLY KEY_CACHE_BUF_SIZE and
 * can't drift from the declared array size. */
#define KCBS_STR2(x) #x
#define KCBS_STR(x)  KCBS_STR2(x)

extern uint8_t  _teamSelectionKeyCache[KEY_CACHE_BUF_SIZE];

_Static_assert(sizeof(_teamSelectionKeyCache) == 8192,
               "teamSelectionKeyCache buffer must stay 8192 bytes "
               "(tightened InsertSpaceSorted bound; KEYCACHE_CLUSTER_REWORK.md §8.3)");

__asm__(
#if defined(__APPLE__)
    ".section __DATA,__bss\n"
#else
    ".section .bss\n"
#endif
    ".p2align 1\n"
    /* ---- walk cursors (4 x uint16) ---- */
    ".globl " _ASM_PFX "_teamSelectionKeyCacheIndex\n"
    _ASM_PFX "_teamSelectionKeyCacheIndex:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_teamSelectionKeyCacheOffset\n"
    _ASM_PFX "_teamSelectionKeyCacheOffset:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_teamSelectionKeyCacheCount\n"
    _ASM_PFX "_teamSelectionKeyCacheCount:\n"
    ".skip 2\n"
    ".globl " _ASM_PFX "_careerSlotProcessingInProgress\n"
    _ASM_PFX "_careerSlotProcessingInProgress:\n"
    ".skip 2\n"
    /* ---- dedicated cache buffer (KEY_CACHE_BUF_SIZE B) ---- */
    ".globl " _ASM_PFX "_teamSelectionKeyCache\n"
    _ASM_PFX "_teamSelectionKeyCache:\n"
    ".skip " KCBS_STR(KEY_CACHE_BUF_SIZE) "\n"
    /* ---- end marker: &it = cache+KEY_CACHE_BUF_SIZE = InsertSpaceSorted bound; value = season flag ---- */
    ".globl " _ASM_PFX "_teamSelectionKeyCacheEnd\n"
    _ASM_PFX "_teamSelectionKeyCacheEnd:\n"
    ".skip 2\n"
    /* ---- _careerJobOfferPhaseBias: promoted standalone scalar (was cache byte 1) ---- */
    ".globl " _ASM_PFX "_careerJobOfferPhaseBias\n"
    _ASM_PFX "_careerJobOfferPhaseBias:\n"
    ".skip 1\n"
    ".skip 1\n"   /* pad: keep block even */
    ".text\n"
);
