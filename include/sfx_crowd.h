/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef SFX_CROWD_H
#define SFX_CROWD_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;

/* Crowd management */
void CrowdResetMatchState(void);
void TurnCrowdChantsOn(void);
void TurnCrowdChantsOff(void);
void PlayCrowdChants(void);
void MuteAllAudioChannelsWithDelay(void);
void AudioFadeCrowdDownThenPulseMusic(void);
void AudioMuteAllWithFrameRamp(void);

/* SFX triggers */
void PlayEnqueuedSamples(void);
void PlayPenaltySample(void);
void PlaySample7IfNotTraining(void);
void PlaySfxSample16(void);
void PlayGoodPassSample(void);
void PlayKickSample(void);
void GateKickSampleAndCrowd(uint16_t channel, uint16_t sfxId);
void PlayRefereeWhistleSample(void);
void PlayFoulWhistleSample(void);
void PlayEndGameWhistleSample(void);
void PlaySample1IfNotTraining(void);
void PlaySample1(void);
void PlaySample2IfNotTraining(void);
void PlaySample4IfNotTraining(void);
void PlaySample3IfNotTraining(void);
void PlayMissGoalSample(void);
void PlayPenalty(void);
void PlayGameStateWhistleSample(void);
void PlayGoalReplayBottomSample(void);
void PlayGoalKickOffSample(void);
void PlayCornerKickSample(void);
int16_t HandleKickSampleAndCrowd(uint16_t mode, uint16_t sfxId);
void PlaySample(uint16_t preset);

#endif
