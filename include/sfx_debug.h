/*
 * sfx_debug.h — temporary instrumentation for SFX-trigger debugging.
 *
 * Define SWOS_DEBUG_SFX at compile time (e.g. -DSWOS_DEBUG_SFX=1) to
 * enable; otherwise the macros expand to nothing.
 *
 * Each AudioSDL_TriggerSfx* path emits one line on stderr like:
 *
 *   [sfx] +12340ms ch=2 wave=0x0F (SFX_CROWD_KEEPERSAVES)
 *   [sfx] +12352ms ch=3 note=0x10 (SFX_BALL_BOUNCE_1)
 *   [sfx] +12410ms preset=0x01 → ch0=0x00 ch1=0x05 ch2=0x00 ch3=0x00
 *
 * Grep for SWOS_SFX_LOG to find / remove all hooks once the bug is
 * identified.
 */
#ifndef SWOS_SFX_DEBUG_H
#define SWOS_SFX_DEBUG_H

#ifdef SWOS_DEBUG_SFX

#include <stdio.h>

/* Forward decl — implemented in audio_sdl.c. Returns ms since first
 * trigger, so the timestamps in the log line up with what the user
 * was doing in-game (a throw-in, a goal, etc.). */
unsigned swos_sfx_log_now_ms(void);

/* Map a wave/note id to the macro name from sfx_ids.h, if any. The
 * mapping is non-injective (some IDs span both note and wave paths)
 * so we only annotate when there's a single confident match — see
 * implementation in src/sfx_debug.c. */
const char *swos_sfx_id_name(unsigned id);

#define SWOS_SFX_LOG(fmt, ...) \
    do { fprintf(stderr, "[sfx] " fmt "\n", ##__VA_ARGS__); } while (0)

#define SWOS_SFX_LOG_TRIGGER_NOTE(ch, id) \
    SWOS_SFX_LOG("+%ums ch=%d note=0x%02X (%s)", \
                 swos_sfx_log_now_ms(), (int)(ch), (unsigned)(id), \
                 swos_sfx_id_name((unsigned)(id)))

#define SWOS_SFX_LOG_TRIGGER_WAVE(ch, id) \
    SWOS_SFX_LOG("+%ums ch=%d wave=0x%02X (%s)", \
                 swos_sfx_log_now_ms(), (int)(ch), (unsigned)(id), \
                 swos_sfx_id_name((unsigned)(id)))

#define SWOS_SFX_LOG_TRIGGER_PRESET(id, n0, n1, n2, n3) \
    SWOS_SFX_LOG("+%ums preset=0x%02X -> ch0=0x%02X ch1=0x%02X ch2=0x%02X ch3=0x%02X", \
                 swos_sfx_log_now_ms(), (unsigned)(id), \
                 (unsigned)(n0), (unsigned)(n1), (unsigned)(n2), (unsigned)(n3))

#else  /* !SWOS_DEBUG_SFX */

#define SWOS_SFX_LOG(...)                              ((void)0)
#define SWOS_SFX_LOG_TRIGGER_NOTE(ch, id)              ((void)0)
#define SWOS_SFX_LOG_TRIGGER_WAVE(ch, id)              ((void)0)
#define SWOS_SFX_LOG_TRIGGER_PRESET(id, n0, n1, n2, n3) ((void)0)

#endif /* SWOS_DEBUG_SFX */

#endif /* SWOS_SFX_DEBUG_H */
