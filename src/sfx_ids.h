/*
 * sfx_ids.h — SWOS sound-effect IDs.
 *
 * The values are wave-table indices (`waveId`) into `bin/sound/sfx.sng`'s
 * note/wave table. Pass them to `_AudioChannelQueueNextWaveIfEnabled`
 * (or, on SDL, `AudioSDL_TriggerSfxByWaveId`) to play the matching
 * sample.
 *
 * Confidence levels (in column comments below):
 *   [src]   id sourced from a hardcoded literal in src/sfx_crowd.c
 *   [ear]   id sourced from by-ear mapping via the SFX-test debug
 *           handler in src/platform_sdl.c
 *   [src+ear]  both confirm
 *   [?spec] proposed by spectral-fingerprint match against the SWOS
 *           2020 sample bank — needs ear confirmation before relying
 *   [?]     untested or speculative — don't rely on this yet
 *
 * See SFX.md for the full mapping table, the SWOS 2020 sample
 * manifest, and gaps that still need to be tested.
 */
#ifndef SWOS_SFX_IDS_H
#define SWOS_SFX_IDS_H

/* ---- Background ambience (crowd ID range 1..9) ---- */
#define SFX_BG_CROWD_1         0x01    /* [ear] BGCRD3L low background crowd */
#define SFX_BG_CROWD_2         0x02    /* [ear] BGCRD3L variant or duplicate */
#define SFX_CROWD_CHEER_1      0x03    /* [ear] HOMEGOAL or chant */
#define SFX_FOUL_WHISTLE       0x04    /* [src+ear] FOUL — _PlayFoulWhistleSample */
#define SFX_CROWD_CHEER_2      0x05    /* [ear] chant */
#define SFX_CROWD_CHEER_3      0x06    /* [ear] chant */
/*      SFX_???_07              0x07     [?] untested */
#define SFX_CROWD_FADE_1       0x08    /* [ear] crowd-chant tail → background */
#define SFX_CROWD_FADE_2       0x09    /* [ear] same as 0x08 */
/*      SFX_???_0A              0x0A     [?] untested */
/*      SFX_???_0B              0x0B     [?] untested */

/* ---- Match-event range (0x0C..0x14) ---- */
#define SFX_KICK_1             0x0C    /* [ear] KICKX */
#define SFX_BOUNCE_HARD        0x0D    /* [ear] BOUNCEX hard */
#define SFX_BOUNCE_CLICK       0x0E    /* [ear] BOUNCEX soft / tackle click */
#define SFX_MISSGOAL_1         0x0F    /* [src+ear] MISSGOAL ("aww") — _PlayCornerKickSample (function name suspect) */
#define SFX_KICK_SAMPLE16      0x10    /* [src+ear] _PlaySfxSample16; user heard soft bounce */
#define SFX_BOUNCE_HARD_2      0x11    /* [ear] hard bounce */
#define SFX_MISSGOAL_2         0x12    /* [src+ear] "aww" — _PlayGoalKickOffSample (function name suspect) */
/*      SFX_???_13              0x13     [?spec] silent in renderer — likely null inst */
/*      SFX_???_14              0x14     [?spec] silent in renderer — likely null inst */

/* ---- Whistles, end-of-game, scored (0x16..0x2C) — TODO confirm by ear ---- */
#define SFX_END_GAME_WHISTLE   0x16    /* [src] ENDGAMEW — _PlayEndGameWhistleSample */
#define SFX_VOICE_LINE_21      0x21    /* [?spec] 3.44s voice line — closest match COMBROWN (3.19s) */
#define SFX_MISSGOAL           0x26    /* [src] MISSGOAL — _PlayMissGoalSample */
#define SFX_GOOD_PASS          0x27    /* [src] crowd appreciation — _PlayGoodPassSample */
#define SFX_SUB_CALL           0x29    /* [?spec] 0.96s speech-like — closest match EASY/HOMESUBL */
#define SFX_REF_WHISTLE_2A     0x2A    /* [?spec] 0.20s short whistle blip */
#define SFX_REF_WHISTLE_2B     0x2B    /* [?spec] 0.22s short whistle blip */
#define SFX_NOT_TRAINING_3     0x2C    /* [src] _PlaySample3IfNotTraining (purpose unknown) */
#define SFX_BOUNCE_2D          0x2D    /* [?spec] 0.12s — fingerprint matches BOUNCEX */
#define SFX_BOUNCE_2E          0x2E    /* [?spec] 0.12s — fingerprint matches BOUNCEX */

/* ---- Goal cheer (0x32) ---- */
#define SFX_HOME_GOAL          0x32    /* [?spec] 10.92s — near-identical fingerprint to HOMEGOAL.WAV (11.48s) */

/* ---- Referee whistles & chants (0x31..0x3E) ---- */
#define SFX_REFEREE_WHISTLE    0x31    /* [src] REF_WH/WHISTLE — _PlayRefereeWhistleSample */
#define SFX_CHANT_4L           0x37    /* [src] crowdSamples[0] */
#define SFX_CHANT_8L           0x38    /* [src] crowdSamples[1] */
#define SFX_CHANT_10L          0x3A    /* [src] crowdSamples[2] */
#define SFX_EREWEGO            0x3E    /* [src] crowdSamples[3] */

/* ---- Preset IDs (route through _AudioApplyPresetToChannels) ---- */
#define SFX_PRESET_NOT_TRAINING_1   1       /* [src] _PlaySample1IfNotTraining / _PlaySample1 */
#define SFX_PRESET_NOT_TRAINING_2   3       /* [src] _PlaySample2IfNotTraining */
#define SFX_PRESET_NOT_TRAINING_4   4       /* [src] _PlaySample4IfNotTraining */
#define SFX_PRESET_PENALTY          6       /* [src] _PlayPenaltySample */
#define SFX_PRESET_NOT_TRAINING_7   7       /* [src] _PlaySample7IfNotTraining */

#endif /* SWOS_SFX_IDS_H */
