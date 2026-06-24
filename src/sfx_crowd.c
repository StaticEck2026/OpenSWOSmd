/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "sfx_crowd.h"
#include "audio.h"

#define ST_PENALTY 14

/* ---- External globals (BSS, absolute addressing in asm) ---- */
extern uint16_t _endOfHalfWriteOnly;
extern uint16_t _goalCounter;
extern uint16_t _sfxQueueWriteIdx;
extern uint16_t _crowdEventFlags;
extern uint16_t _sfxQueueCount;
extern uint16_t _trainingGame;
extern uint16_t _gameState;
extern uint16_t _patternsGoalCounter;
extern uint16_t _showFansCounter;
extern volatile uint16_t _currentGameTick;

/* ---- Embedded data labels in SWOS2.s code section ---- */
extern uint16_t _pendingSampleId3_4;
extern uint16_t _crowdChantsOn;
extern int16_t _lastKickSampleTick;
extern uint8_t _audioDbgFlagA;
extern uint16_t _crowdSamples[4];
extern uint16_t _currentCrowdSample;
extern uint16_t _fadeInChantTimer;
extern uint16_t _nextChantTimer;
extern uint16_t _fadeOutChantTimer;
extern uint16_t _crowdChantVolume;

/* ---- External C/asm functions ---- */
extern uint16_t Rand(void);
/* AudioSetChannelVolume, AudioApplyPresetToChannels, AudioChannelQueueNextWaveIfEnabled from audio.h */

/* ================================================================
 * CrowdResetMatchState  (SWOS2.s line 9083)
 *
 * Clears all crowd/SFX state at the start of a match.
 * ================================================================ */
__attribute__((used, externally_visible))
void CrowdResetMatchState(void)
{
    _patternsGoalCounter = 0;
    _showFansCounter = 0;
    _crowdChantVolume = 0;
    _crowdEventFlags = 0;
    _sfxQueueCount = 0;
    _sfxQueueWriteIdx = 0;
    _goalCounter = 0;
    _crowdEventFlags = 0;   /* original clears this twice */
    _currentCrowdSample = 0xFFFF;
}

/* ================================================================
 * TurnCrowdChantsOn  (SWOS2.s line 9097)
 *
 * Enables crowd chants: resets timers, sets volume to 0, enables flag.
 * ================================================================ */
__attribute__((used, externally_visible))
void TurnCrowdChantsOn(void)
{
    _fadeInChantTimer = 0;
    _nextChantTimer = 0;
    _fadeOutChantTimer = 0;
    _crowdChantsOn = 1;
    AudioSetChannelVolume(2, 0);
    _crowdChantVolume = 0;
}

/* ================================================================
 * TurnCrowdChantsOff  (SWOS2.s line 9110)
 *
 * Disables crowd chants: clears flag and volume.
 * ================================================================ */
__attribute__((used, externally_visible))
void TurnCrowdChantsOff(void)
{
    _crowdChantsOn = 0;
    AudioSetChannelVolume(2, 0);
    _crowdChantVolume = 0;
}

/* ================================================================
 * PlayCrowdChants  (SWOS2.s line 9122)
 *
 * State machine for crowd chant audio:
 *   - If training game or chants off: return
 *   - If _fadeInChantTimer active: fade in
 *   - If _nextChantTimer active: cooldown tick
 *   - If _fadeOutChantTimer active: fade out
 *   - Otherwise: random chance to start new chant event
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayCrowdChants(void)
{
    uint16_t rnd, vol;

    if (_trainingGame != 0)
        return;
    if (_crowdChantsOn == 0)
        return;

    if (_fadeInChantTimer != 0)
        goto crowd_fadein_tick;
    if (_nextChantTimer != 0)
        goto crowd_cooldown_tick;
    if (_fadeOutChantTimer != 0)
        goto crowd_fadeout_tick;

    /* Idle state: random chance to start a chant */
    rnd = Rand();
    if (rnd < 128)
    {
        /* Start new chant event */
        _crowdChantVolume = 0;
        AudioSetChannelVolume(2, 0);

        /* Pick a crowd sample */
        if (_currentCrowdSample != 0xFFFF)
        {
            rnd = Rand();
            if (rnd >= 0x80)
                goto pick_new_sample;
            /* else keep current sample */
        }
        else
        {
pick_new_sample:
            rnd = Rand();
            rnd &= 6;  /* 0, 2, 4, or 6 */
            _currentCrowdSample = _crowdSamples[rnd >> 1];
        }

        AudioChannelQueueNextWaveIfEnabled(2, _currentCrowdSample);

        _fadeInChantTimer = 128;
        rnd = Rand();
        _nextChantTimer = (uint16_t)((rnd << 1) + 500);
        _fadeOutChantTimer = 128;
        return;
    }
    else
    {
        /* No chant this time, set cooldown */
        rnd = Rand();
        _nextChantTimer = (uint16_t)((rnd << 2) + 0x1F4);
        return;
    }

crowd_cooldown_tick:
    _nextChantTimer--;
    return;

crowd_fadeout_tick:
    _fadeOutChantTimer--;
    vol = _fadeOutChantTimer >> 1;
    _crowdChantVolume = vol;
    AudioSetChannelVolume(2, vol);
    return;

crowd_fadein_tick:
    _fadeInChantTimer--;
    vol = (uint16_t)(0x80 - _fadeInChantTimer) >> 1;
    _crowdChantVolume = vol;
    AudioSetChannelVolume(2, vol);
    return;
}

/* ================================================================
 * MuteAllAudioChannelsWithDelay  (SWOS2.s line 9207)
 *
 * Ramps all-channel volume from 0 to 0x40 in steps of 4,
 * waiting 1 tick per step.
 * ================================================================ */
__attribute__((used, externally_visible))
void MuteAllAudioChannelsWithDelay(void)
{
    uint16_t vol;
    uint16_t tick;

    for (vol = 0; vol <= 0x40; vol += 4)
    {
        AudioSetChannelVolume(0xFFFF, vol);

        /* Wait 1 tick (dbf d1 with d1=0 loops once) */
        tick = _currentGameTick;
        while (_currentGameTick == tick)
        {
            /* spin */
        }
    }
}

/* ================================================================
 * AudioFadeCrowdDownThenPulseMusic  (SWOS2.s line 9228)
 *
 * Phase 1: Fade crowd channel (ch2) down from _crowdChantVolume to 0.
 * Phase 2: Fade music channels (ch0,1,3) down from 0x40 to 0.
 * Final: Set all channels to 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void AudioFadeCrowdDownThenPulseMusic(void)
{
    int16_t vol;
    uint16_t tick;

    /* Phase 1: Fade crowd channel down */
    vol = (int16_t)_crowdChantVolume;
    if (vol != 0)
    {
        while (vol >= 0)
        {
            AudioSetChannelVolume(2, (uint16_t)vol);

            tick = _currentGameTick;
            while (_currentGameTick == tick)
            {
                /* spin */
            }

            vol -= 2;
        }
        AudioSetChannelVolume(2, 0);
    }

    /* Phase 2: Pulse music channels (0, 1, 3) down */
    for (vol = 0x40; vol >= 0; vol -= 2)
    {
        AudioSetChannelVolume(0, (uint16_t)vol);
        AudioSetChannelVolume(1, (uint16_t)vol);
        AudioSetChannelVolume(3, (uint16_t)vol);

        tick = _currentGameTick;
        while (_currentGameTick == tick)
        {
            /* spin */
        }
    }

    /* Final: all channels to 0 */
    AudioSetChannelVolume(0xFFFF, 0);
}

/* ================================================================
 * AudioMuteAllWithFrameRamp  (SWOS2.s line 9282)
 *
 * Ramps all-channel volume from 0x40 down to 0 in steps of 2,
 * waiting 1 tick per step. Final: set volume to 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void AudioMuteAllWithFrameRamp(void)
{
    int16_t vol;
    uint16_t tick;

    for (vol = 0x40; vol >= 0; vol -= 2)
    {
        AudioSetChannelVolume(0xFFFF, (uint16_t)vol);

        tick = _currentGameTick;
        while (_currentGameTick == tick)
        {
            /* spin */
        }
    }

    AudioSetChannelVolume(0xFFFF, 0);
}

/* ================================================================
 * PlayEnqueuedSamples  (SWOS2.s line 9353)
 *
 * Timer-driven SFX dispatcher. Decrements counters and triggers
 * samples when they reach zero.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayEnqueuedSamples(void)
{
    if (_endOfHalfWriteOnly != 0)
    {
        _endOfHalfWriteOnly--;
        if (_endOfHalfWriteOnly == 0)
        {
            PlayPenaltySample();
            _sfxQueueWriteIdx = 0xFA;
        }
    }

    if (_goalCounter != 0)
    {
        _goalCounter--;
        if (_goalCounter == 0)
            PlaySample4IfNotTraining();
    }

    if (_sfxQueueWriteIdx != 0)
    {
        _sfxQueueWriteIdx--;
        if (_sfxQueueWriteIdx == 0)
            PlaySample7IfNotTraining();
    }

    if (_crowdEventFlags != 0)
        _crowdEventFlags--;

    if (_sfxQueueCount != 0)
    {
        _sfxQueueCount--;
        if (_sfxQueueCount == 0)
            PlayCornerKickSample();
    }

    PlayCrowdChants();
}

/* ================================================================
 * PlayPenaltySample  (SWOS2.s line 9386)
 *
 * If not training: play sample preset 6.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayPenaltySample(void)
{
    if (_trainingGame != 0)
        return;
    PlaySample(6);
}

/* ================================================================
 * PlaySample7IfNotTraining  (SWOS2.s line 9396)
 *
 * If not training: play sample preset 7.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample7IfNotTraining(void)
{
    if (_trainingGame != 0)
        return;
    PlaySample(7);
}

/* ================================================================
 * PlaySfxSample16  (SWOS2.s line 9406, sub_107260)
 *
 * GateKickSampleAndCrowd with sfxId=$10, channel=3.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySfxSample16(void)
{
    GateKickSampleAndCrowd(3, 0x10);
}

/* ================================================================
 * PlayGoodPassSample  (SWOS2.s line 9414)
 *
 * GateKickSampleAndCrowd with sfxId=$27, channel=3.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayGoodPassSample(void)
{
    GateKickSampleAndCrowd(3, 0x27);
}

/* ================================================================
 * PlayKickSample  (SWOS2.s line 9422)
 *
 * Alternates between sfxId $25 and $28 based on _currentGameTick bit 1.
 * Then calls GateKickSampleAndCrowd(3, sfxId).
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayKickSample(void)
{
    uint16_t sfxId = 0x25;

    /* btst #1,(_currentGameTick+1) tests bit 1 of the low byte */
    if (((uint8_t)_currentGameTick) & 0x02)
        sfxId = 0x28;

    GateKickSampleAndCrowd(3, sfxId);
}

/* ================================================================
 * GateKickSampleAndCrowd  (SWOS2.s line 9432)
 *
 * Gates the kick sample through _audioDbgFlagA bit 7 check
 * and _lastKickSampleTick comparison. If gate passes, calls
 * HandleKickSampleAndCrowd.
 * ================================================================ */
__attribute__((used, externally_visible))
void GateKickSampleAndCrowd(uint16_t channel, uint16_t sfxId)
{
    /* If _audioDbgFlagA bit 7 is set, apply gating */
    if (_audioDbgFlagA & 0x80)
    {
        if (_lastKickSampleTick != 0x000F && _lastKickSampleTick != 0x0012)
            return;
    }

    HandleKickSampleAndCrowd(channel, sfxId);
}

/* ================================================================
 * PlayRefereeWhistleSample  (SWOS2.s line 9444)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayRefereeWhistleSample(void)
{
    HandleKickSampleAndCrowd(3, 0x31);
}

/* ================================================================
 * PlayFoulWhistleSample  (SWOS2.s line 9452)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayFoulWhistleSample(void)
{
    HandleKickSampleAndCrowd(3, 4);
}

/* ================================================================
 * PlayEndGameWhistleSample  (SWOS2.s line 9457)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayEndGameWhistleSample(void)
{
    HandleKickSampleAndCrowd(3, 0x16);
}

/* ================================================================
 * PlaySample1IfNotTraining  (SWOS2.s line 9464)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample1IfNotTraining(void)
{
    if (_trainingGame != 0)
        return;
    PlaySample(1);
}

/* ================================================================
 * PlaySample1  (SWOS2.s line 9471)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample1(void)
{
    PlaySample(1);
}

/* ================================================================
 * PlaySample2IfNotTraining  (SWOS2.s line 9477)
 *
 * If not training: set _pendingSampleId3_4=$3E8, play preset 3.
 * (Original: falls through to PlaySample3 label)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample2IfNotTraining(void)
{
    if (_trainingGame != 0)
        return;
    _pendingSampleId3_4 = 0x3E8;
    PlaySample(3);
}

/* ================================================================
 * PlaySample4IfNotTraining  (SWOS2.s line 9488)
 *
 * If not training: set _pendingSampleId3_4=$190, play preset 4.
 * (Original: falls through to PlaySample4 label)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample4IfNotTraining(void)
{
    if (_trainingGame != 0)
        return;
    _pendingSampleId3_4 = 0x190;
    PlaySample(4);
}

/* ================================================================
 * PlaySample3IfNotTraining  (SWOS2.s line 9502)
 *
 * If not training: set _sfxQueueCount=$A, GateKick($2C, 3).
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample3IfNotTraining(void)
{
    if (_trainingGame != 0)
        return;
    _sfxQueueCount = 0x0A;
    GateKickSampleAndCrowd(3, 0x2C);
}

/* ================================================================
 * PlayMissGoalSample  (SWOS2.s line 9514)
 *
 * If not training: set _sfxQueueCount=$A, HandleKick($26, 3).
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayMissGoalSample(void)
{
    if (_trainingGame != 0)
        return;
    _sfxQueueCount = 0x0A;
    HandleKickSampleAndCrowd(3, 0x26);
}

/* ================================================================
 * PlayPenalty  (SWOS2.s line 9526)
 *
 * If not training: play penalty sample, set _sfxQueueWriteIdx=$8C.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayPenalty(void)
{
    if (_trainingGame != 0)
        return;
    PlayPenaltySample();
    _sfxQueueWriteIdx = 0x8C;
}

/* ================================================================
 * PlayGameStateWhistleSample  (SWOS2.s line 9537)
 *
 * If not training:
 *   - If _gameState != ST_PENALTY: play penalty sample
 *   - If _gameState == ST_PENALTY && _sfxQueueWriteIdx == 0: play penalty sample
 *   - Set _sfxQueueWriteIdx = $96
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayGameStateWhistleSample(void)
{
    if (_trainingGame != 0)
        return;

    if (_gameState == ST_PENALTY)
    {
        if (_sfxQueueWriteIdx != 0)
            goto queue_whistle;
    }

    PlayPenaltySample();

queue_whistle:
    _sfxQueueWriteIdx = 0x96;
}

/* ================================================================
 * PlayGoalReplayBottomSample  (SWOS2.s line 9554)
 *
 * If not training: calls PlaySample2IfNotTraining.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayGoalReplayBottomSample(void)
{
    if (_trainingGame != 0)
        return;
    PlaySample2IfNotTraining();
}

/* ================================================================
 * PlayGoalKickOffSample  (SWOS2.s line 9563)
 *
 * If not training: HandleKick($12, 3). If played (return 0),
 * set _crowdEventFlags = $39.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayGoalKickOffSample(void)
{
    if (_trainingGame != 0)
        return;
    if (HandleKickSampleAndCrowd(3, 0x12) != 0)
        return;
    _crowdEventFlags = 0x39;
}

/* ================================================================
 * PlayCornerKickSample  (SWOS2.s line 9579)
 *
 * If not training: HandleKick($F, 3). If played (return 0),
 * set _crowdEventFlags = $32.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayCornerKickSample(void)
{
    if (_trainingGame != 0)
        return;
    if (HandleKickSampleAndCrowd(3, 0x0F) != 0)
        return;
    _crowdEventFlags = 0x32;
}

/* ================================================================
 * HandleKickSampleAndCrowd  (SWOS2.s line 9606)
 *
 * If sfxId == $16, always play.
 * Otherwise, if _crowdEventFlags != 0, block (return non-zero).
 * Stores sfxId to _lastKickSampleTick, calls CrowdTickJump,
 * returns 0 (played).
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t HandleKickSampleAndCrowd(uint16_t mode, uint16_t sfxId)
{
    if (sfxId != 0x16)
    {
        if (_crowdEventFlags != 0)
            return 1;  /* blocked */
    }

    _lastKickSampleTick = (int16_t)sfxId;
    AudioChannelQueueNextWaveIfEnabled(mode, sfxId);
    return 0;  /* played */
}

/* ================================================================
 * PlaySample  (SWOS2.s line 9622)
 *
 * Wrapper for AudioApplyPresetToChannels.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample(uint16_t preset)
{
    AudioApplyPresetToChannels(preset);
}
