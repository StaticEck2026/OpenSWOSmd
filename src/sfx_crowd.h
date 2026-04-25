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
void _CrowdResetMatchState(void);
void _TurnCrowdChantsOn(void);
void _TurnCrowdChantsOff(void);
void _PlayCrowdChants(void);
void _MuteAllAudioChannelsWithDelay(void);
void _AudioFadeCrowdDownThenPulseMusic(void);
void _AudioMuteAllWithFrameRamp(void);

/* SFX triggers */
void _PlayEnqueuedSamples(void);
void _PlayPenaltySample(void);
void _PlaySample7IfNotTraining(void);
void _PlaySfxSample16(void);
void _PlayGoodPassSample(void);
void _PlayKickSample(void);
void _GateKickSampleAndCrowd(uint16_t channel, uint16_t sfxId);
void _PlayRefereeWhistleSample(void);
void _PlayFoulWhistleSample(void);
void _PlayEndGameWhistleSample(void);
void _PlaySample1IfNotTraining(void);
void _PlaySample1(void);
void _PlaySample2IfNotTraining(void);
void _PlaySample4IfNotTraining(void);
void _PlaySample3IfNotTraining(void);
void _PlayMissGoalSample(void);
void _PlayPenalty(void);
void _PlayGameStateWhistleSample(void);
void _PlayGoalReplayBottomSample(void);
void _PlayGoalKickOffSample(void);
void _PlayCornerKickSample(void);
int16_t _HandleKickSampleAndCrowd(uint16_t mode, uint16_t sfxId);
void _PlaySample(uint16_t preset);

#endif
