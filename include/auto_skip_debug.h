/*
 * auto_skip_debug.h -- temporary instrumentation for the
 * "CONTINUE CAREER doesn't auto-skip the season-transition menu chain"
 * investigation.
 *
 * Define SWOS_DEBUG_AUTOSKIP at compile time (e.g. -DSWOS_DEBUG_AUTOSKIP)
 * to enable. Otherwise every macro expands to nothing.
 *
 * Grep for SWOS_AUTOSKIP_LOG to find / remove all hooks once the bug
 * is identified.
 */
#ifndef SWOS_AUTO_SKIP_DEBUG_H
#define SWOS_AUTO_SKIP_DEBUG_H

#ifdef SWOS_DEBUG_AUTOSKIP

#include <stdio.h>

#define SWOS_AUTOSKIP_LOG(fmt, ...) \
    do { fprintf(stderr, "[autoskip] " fmt "\n", ##__VA_ARGS__); } while (0)

/* Resolve a MenuEntry pointer back to its ordinal (0..numEntries-1) by
 * subtracting the menuEntries base. Returns -1 if the pointer doesn't
 * land on a valid stride boundary (i.e. it's already a small int from
 * before _ActivateMenu resolved it, or it's NULL). */
static inline int swos_autoskip_entry_ordinal(const void *entry_ptr,
                                              const void *menu_entries_base)
{
    if (entry_ptr == 0) return -1;
    /* Entry index encoded as small int (pre-resolve) — see _ActivateMenu's
     * `selEntry <= 0xFFFF` check. */
    if ((unsigned long)entry_ptr <= 0xFFFFu) return (int)(unsigned long)entry_ptr;
    long delta = (const char *)entry_ptr - (const char *)menu_entries_base;
    if (delta < 0) return -2;
    /* MENUENTRY_STRIDE = 56 */
    if (delta % 56 != 0) return -3;
    return (int)(delta / 56);
}

#else  /* !SWOS_DEBUG_AUTOSKIP */

#define SWOS_AUTOSKIP_LOG(fmt, ...) ((void)0)

static inline int swos_autoskip_entry_ordinal(const void *entry_ptr,
                                              const void *menu_entries_base)
{
    (void)entry_ptr; (void)menu_entries_base;
    return -1;
}

#endif

#endif /* SWOS_AUTO_SKIP_DEBUG_H */
