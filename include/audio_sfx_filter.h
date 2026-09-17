/*
 * audio_sfx_filter.h -- a gate every sound effect the game starts passes through (SDL builds).
 *
 * Netplay installs one: a rollback plays frames again, and the effects those frames start
 * were heard the first time (docs/NETPLAY.md, "Sound during a rollback"). Only the effects
 * are filtered. Volume changes are left alone -- a fade played again ends on the right value.
 */
#ifndef AUDIO_SFX_FILTER_H
#define AUDIO_SFX_FILTER_H

#ifdef SWOS_SDL_BUILD

#include <stdint.h>

enum {
    AUDIO_SFX_PRESET = 1,       /* AudioApplyPresetToChannels: `id` is the preset, channel 0xFF */
    AUDIO_SFX_WAVE   = 2        /* AudioChannelQueueNextWaveIfEnabled: `id` is the wave */
};

/* Nonzero plays the effect. */
typedef int32_t (*AudioSfxFilter)(uint8_t kind, uint8_t channel, uint16_t id);

/* NULL, the default, plays everything. */
void Audio_SetSfxFilter(AudioSfxFilter filter);

#endif /* SWOS_SDL_BUILD */
#endif /* AUDIO_SFX_FILTER_H */
