/* md_sound.c -- the engine's sound calls, on the Mega Drive's PSG and FM chip. */
#include "md_hw.h"
#include "md_game.h"

extern volatile uint16_t _audioActive;

void AudioApplyPresetToChannels(uint16_t presetId) { (void)presetId; }
void AudioChannelQueueNextWaveIfEnabled(uint16_t channel, uint16_t waveId) { (void)channel; (void)waveId; }
void AudioSetChannelVolume(uint16_t channelOrAll, uint16_t volume) { (void)channelOrAll; (void)volume; }
void ClearAudioActiveFlag(void) { _audioActive = 0; }
void StopAudioAndClearFlags(void) { _audioActive = 0; }
void LoadSfxResources(void) {}
void EnsureSfxLoadedAndStart(void) { _audioActive = 1; }

void md_sound_init(void) {}
void md_sound_tick(void) {}
void md_sound_frame(void) {}
