/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "sfx_crowd.h"
#include "audio.h"

#define ST_PENALTY 14

/* ---- External globals (BSS, absolute addressing in asm) ---- */
extern uint16_t endOfHalfWriteOnly;
extern uint16_t goalCounter;
extern uint16_t sfxQueueWriteIdx;
extern uint16_t crowdEventFlags;
extern uint16_t sfxQueueCount;
extern uint16_t trainingGame;
extern uint16_t gameState;
extern uint16_t patternsGoalCounter;
extern uint16_t showFansCounter;
extern volatile uint16_t currentGameTick;

/* ---- Embedded data labels in SWOS2.s code section ---- */
extern uint16_t pendingSampleId3_4;
extern uint16_t crowdChantsOn;
extern int16_t lastKickSampleTick;
extern uint8_t audioDbgFlagA;
extern uint16_t crowdSamples[4];
extern uint16_t currentCrowdSample;
extern uint16_t fadeInChantTimer;
extern uint16_t nextChantTimer;
extern uint16_t fadeOutChantTimer;
extern uint16_t crowdChantVolume;

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
    patternsGoalCounter = 0;
    showFansCounter = 0;
    crowdChantVolume = 0;
    crowdEventFlags = 0;
    sfxQueueCount = 0;
    sfxQueueWriteIdx = 0;
    goalCounter = 0;
    crowdEventFlags = 0;   /* original clears this twice */
    currentCrowdSample = 0xFFFF;
}

/* ================================================================
 * TurnCrowdChantsOn  (SWOS2.s line 9097)
 *
 * Enables crowd chants: resets timers, sets volume to 0, enables flag.
 * ================================================================ */
__attribute__((used, externally_visible))
void TurnCrowdChantsOn(void)
{
    fadeInChantTimer = 0;
    nextChantTimer = 0;
    fadeOutChantTimer = 0;
    crowdChantsOn = 1;
    AudioSetChannelVolume(2, 0);
    crowdChantVolume = 0;
}

/* ================================================================
 * TurnCrowdChantsOff  (SWOS2.s line 9110)
 *
 * Disables crowd chants: clears flag and volume.
 * ================================================================ */
__attribute__((used, externally_visible))
void TurnCrowdChantsOff(void)
{
    crowdChantsOn = 0;
    AudioSetChannelVolume(2, 0);
    crowdChantVolume = 0;
}

/* ================================================================
 * PlayCrowdChants  (SWOS2.s line 9122)
 *
 * State machine for crowd chant audio:
 *   - If training game or chants off: return
 *   - If fadeInChantTimer active: fade in
 *   - If nextChantTimer active: cooldown tick
 *   - If fadeOutChantTimer active: fade out
 *   - Otherwise: random chance to start new chant event
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayCrowdChants(void)
{
    uint16_t rnd, vol;

    if (trainingGame != 0)
        return;
    if (crowdChantsOn == 0)
        return;

    if (fadeInChantTimer != 0)
        goto crowd_fadein_tick;
    if (nextChantTimer != 0)
        goto crowd_cooldown_tick;
    if (fadeOutChantTimer != 0)
        goto crowd_fadeout_tick;

    /* Idle state: random chance to start a chant */
    rnd = Rand();
    if (rnd < 128)
    {
        /* Start new chant event */
        crowdChantVolume = 0;
        AudioSetChannelVolume(2, 0);

        /* Pick a crowd sample */
        if (currentCrowdSample != 0xFFFF)
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
            currentCrowdSample = crowdSamples[rnd >> 1];
        }

        AudioChannelQueueNextWaveIfEnabled(2, currentCrowdSample);

        fadeInChantTimer = 128;
        rnd = Rand();
        nextChantTimer = (uint16_t)((rnd << 1) + 500);
        fadeOutChantTimer = 128;
        return;
    }
    else
    {
        /* No chant this time, set cooldown */
        rnd = Rand();
        nextChantTimer = (uint16_t)((rnd << 2) + 0x1F4);
        return;
    }

crowd_cooldown_tick:
    nextChantTimer--;
    return;

crowd_fadeout_tick:
    fadeOutChantTimer--;
    vol = fadeOutChantTimer >> 1;
    crowdChantVolume = vol;
    AudioSetChannelVolume(2, vol);
    return;

crowd_fadein_tick:
    fadeInChantTimer--;
    vol = (uint16_t)(0x80 - fadeInChantTimer) >> 1;
    crowdChantVolume = vol;
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
        tick = currentGameTick;
        while (currentGameTick == tick)
        {
            /* spin */
        }
    }
}

/* ================================================================
 * AudioFadeCrowdDownThenPulseMusic  (SWOS2.s line 9228)
 *
 * Phase 1: Fade crowd channel (ch2) down from crowdChantVolume to 0.
 * Phase 2: Fade music channels (ch0,1,3) down from 0x40 to 0.
 * Final: Set all channels to 0.
 * ================================================================ */
__attribute__((used, externally_visible))
void AudioFadeCrowdDownThenPulseMusic(void)
{
    int16_t vol;
    uint16_t tick;

    /* Phase 1: Fade crowd channel down */
    vol = (int16_t)crowdChantVolume;
    if (vol != 0)
    {
        while (vol >= 0)
        {
            AudioSetChannelVolume(2, (uint16_t)vol);

            tick = currentGameTick;
            while (currentGameTick == tick)
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

        tick = currentGameTick;
        while (currentGameTick == tick)
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

        tick = currentGameTick;
        while (currentGameTick == tick)
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
    if (endOfHalfWriteOnly != 0)
    {
        endOfHalfWriteOnly--;
        if (endOfHalfWriteOnly == 0)
        {
            PlayPenaltySample();
            sfxQueueWriteIdx = 0xFA;
        }
    }

    if (goalCounter != 0)
    {
        goalCounter--;
        if (goalCounter == 0)
            PlaySample4IfNotTraining();
    }

    if (sfxQueueWriteIdx != 0)
    {
        sfxQueueWriteIdx--;
        if (sfxQueueWriteIdx == 0)
            PlaySample7IfNotTraining();
    }

    if (crowdEventFlags != 0)
        crowdEventFlags--;

    if (sfxQueueCount != 0)
    {
        sfxQueueCount--;
        if (sfxQueueCount == 0)
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
    if (trainingGame != 0)
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
    if (trainingGame != 0)
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
 * Alternates between sfxId $25 and $28 based on currentGameTick bit 1.
 * Then calls GateKickSampleAndCrowd(3, sfxId).
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayKickSample(void)
{
    uint16_t sfxId = 0x25;

    /* btst #1,(currentGameTick+1) tests bit 1 of the low byte */
    if (((uint8_t)currentGameTick) & 0x02)
        sfxId = 0x28;

    GateKickSampleAndCrowd(3, sfxId);
}

/* ================================================================
 * GateKickSampleAndCrowd  (SWOS2.s line 9432)
 *
 * Gates the kick sample through audioDbgFlagA bit 7 check
 * and lastKickSampleTick comparison. If gate passes, calls
 * HandleKickSampleAndCrowd.
 * ================================================================ */
__attribute__((used, externally_visible))
void GateKickSampleAndCrowd(uint16_t channel, uint16_t sfxId)
{
    /* If audioDbgFlagA bit 7 is set, apply gating */
    if (audioDbgFlagA & 0x80)
    {
        if (lastKickSampleTick != 0x000F && lastKickSampleTick != 0x0012)
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
    if (trainingGame != 0)
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
 * If not training: set pendingSampleId3_4=$3E8, play preset 3.
 * (Original: falls through to PlaySample3 label)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample2IfNotTraining(void)
{
    if (trainingGame != 0)
        return;
    pendingSampleId3_4 = 0x3E8;
    PlaySample(3);
}

/* ================================================================
 * PlaySample4IfNotTraining  (SWOS2.s line 9488)
 *
 * If not training: set pendingSampleId3_4=$190, play preset 4.
 * (Original: falls through to PlaySample4 label)
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample4IfNotTraining(void)
{
    if (trainingGame != 0)
        return;
    pendingSampleId3_4 = 0x190;
    PlaySample(4);
}

/* ================================================================
 * PlaySample3IfNotTraining  (SWOS2.s line 9502)
 *
 * If not training: set sfxQueueCount=$A, GateKick($2C, 3).
 * ================================================================ */
__attribute__((used, externally_visible))
void PlaySample3IfNotTraining(void)
{
    if (trainingGame != 0)
        return;
    sfxQueueCount = 0x0A;
    GateKickSampleAndCrowd(3, 0x2C);
}

/* ================================================================
 * PlayMissGoalSample  (SWOS2.s line 9514)
 *
 * If not training: set sfxQueueCount=$A, HandleKick($26, 3).
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayMissGoalSample(void)
{
    if (trainingGame != 0)
        return;
    sfxQueueCount = 0x0A;
    HandleKickSampleAndCrowd(3, 0x26);
}

/* ================================================================
 * PlayPenalty  (SWOS2.s line 9526)
 *
 * If not training: play penalty sample, set sfxQueueWriteIdx=$8C.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayPenalty(void)
{
    if (trainingGame != 0)
        return;
    PlayPenaltySample();
    sfxQueueWriteIdx = 0x8C;
}

/* ================================================================
 * PlayGameStateWhistleSample  (SWOS2.s line 9537)
 *
 * If not training:
 *   - If gameState != ST_PENALTY: play penalty sample
 *   - If gameState == ST_PENALTY && sfxQueueWriteIdx == 0: play penalty sample
 *   - Set sfxQueueWriteIdx = $96
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayGameStateWhistleSample(void)
{
    if (trainingGame != 0)
        return;

    if (gameState == ST_PENALTY)
    {
        if (sfxQueueWriteIdx != 0)
            goto queue_whistle;
    }

    PlayPenaltySample();

queue_whistle:
    sfxQueueWriteIdx = 0x96;
}

/* ================================================================
 * PlayGoalReplayBottomSample  (SWOS2.s line 9554)
 *
 * If not training: calls PlaySample2IfNotTraining.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayGoalReplayBottomSample(void)
{
    if (trainingGame != 0)
        return;
    PlaySample2IfNotTraining();
}

/* ================================================================
 * PlayGoalKickOffSample  (SWOS2.s line 9563)
 *
 * If not training: HandleKick($12, 3). If played (return 0),
 * set crowdEventFlags = $39.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayGoalKickOffSample(void)
{
    if (trainingGame != 0)
        return;
    if (HandleKickSampleAndCrowd(3, 0x12) != 0)
        return;
    crowdEventFlags = 0x39;
}

/* ================================================================
 * PlayCornerKickSample  (SWOS2.s line 9579)
 *
 * If not training: HandleKick($F, 3). If played (return 0),
 * set crowdEventFlags = $32.
 * ================================================================ */
__attribute__((used, externally_visible))
void PlayCornerKickSample(void)
{
    if (trainingGame != 0)
        return;
    if (HandleKickSampleAndCrowd(3, 0x0F) != 0)
        return;
    crowdEventFlags = 0x32;
}

/* ================================================================
 * HandleKickSampleAndCrowd  (SWOS2.s line 9606)
 *
 * If sfxId == $16, always play.
 * Otherwise, if crowdEventFlags != 0, block (return non-zero).
 * Stores sfxId to lastKickSampleTick, calls CrowdTickJump,
 * returns 0 (played).
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t HandleKickSampleAndCrowd(uint16_t mode, uint16_t sfxId)
{
    if (sfxId != 0x16)
    {
        if (crowdEventFlags != 0)
            return 1;  /* blocked */
    }

    lastKickSampleTick = (int16_t)sfxId;
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
