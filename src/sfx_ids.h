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
#define SFX_CROWD_BG_1      			0x01    /* Crowd background noise */
#define SFX_CROWD_BG_2     			    0x02    /* Crowd background noise variant */
//#define -----------------------------   0x03    /*  */
#define SFX_WHISTLE_FOUL_1    			0x04    /* Referee whistles for a foul */
#define SFX_CROWD_GOAL_CHEER		   	0x05    /* Goal scored */
#define SFX_CROWD_MATCH_ENGINE_START    0x06    /* MATCH ENGINE STARTED, you see the lower crowd and camera starts rolling */
/*      SFX_???_07              		0x07     [?] untested */
#define SFX_CROWD_REFEREE_CARD_FADEOUT	0x08    /* Crowd fadeout after referee shows yellow/red card */
#define SFX_CROWD_CHEER_FADEOUT         0x09    /* Crowd fadeout after goal cheer & match engine start */
/*      SFX_???_0A              		0x0A     [?] untested */
/*      SFX_???_0B              		0x0B     [?] untested */

/* ---- Match-event range (0x0C..0x14) ---- */
#define SFX_BALL_KEEPER_SAFE_1 0x0C    /*  */
#define SFX_BALL_BOUNCE_2      0x0D    /* Ball bounce variant 2 */
#define SFX_BALL_TACKLED_FOUL  0x0E    /* Ball sound with a tackle and foul */
#define SFX_CROWD_KEEPERSAVES  0x0F    /* Keeper saves a shot ("aaah") */
#define SFX_BALL_BOUNCE_1      0x10    /* Ball bounce default */
//#define -------------------------      0x11    /* Sample only 2kb of size, nothing to hear */
#define SFX_CROWD_NEARMISS     0x12    /* Ball goes just wide "oooh" */
#define SFX_CROWD_REFEREE_CARD 0x14    /* Crowd is in uproar, referee about to show a card */

/* ---- Whistles, end-of-game, scored (0x16..0x2C) — TODO confirm by ear ---- */
#define SFX_WHISTLE_ENDGAME           0x16    /* Halftime and Endgame whistle */
//#define -------------------------     0x21    /* Drum chant, unused in game. Recording broken. */
#define SFX_BALL_KEEPER_SAFE_2        0x25    /* Keeper saves the ball */
#define SFX_BALL_HEADER_STANDING      0x26    /* Player performs a standing header */
#define SFX_BALL_PASSED_OR_TACKLED    0x27    /* Player passes the ball or hits it with a sliding tackle */
#define SFX_BALL_SHOT_HEADER_LOW      0x28    /* Player shoots or flying-heads the ball (low shot, not high) */
#define SFX_WHISTLE_HALFTIME          0x29    /* ++Unused++ whistle, would be nice for halftime. */
#define SFX_WHISTLE_FOUL_2            0x2A    /* Ref whistles for a foul */
//#define -------------------------     0x2B    /* Unused in the game */
#define SFX_BALL_BOUNCE_3             0x2C    /* Ball bounce variant 3 */
#define SFX_BALL_BOUNCE_4             0x2D    /* Ball bounce variant 4 */
#define SFX_BALL_BOUNCE_5             0x2E    /* Ball bounce variant 5*/

/* ---- Referee whistles & chants (0x31..0x3E) ---- */
#define SFX_WHISTLE_KICKOFF    0x31    /* Referee whistles for kickoff, 0th/45th min or after a goal */
#define SFX_CROWD_CHANT_1      0x37    /* Crowd chant variant 1 */
#define SFX_CROWD_CHANT_2      0x38    /* Crowd chant variant 2 */
#define SFX_CROWD_CHANT_3      0x3A    /* Crowd chant variant 3 */
#define SFX_CROWD_CHANT_4      0x3E    /* Crowd chant variant 4 */

/* ---- Preset IDs (route through _AudioApplyPresetToChannels) ---- */
#define SFX_PRESET_NOT_TRAINING_1   1       /* [src] _PlaySample1IfNotTraining / _PlaySample1 */
#define SFX_PRESET_NOT_TRAINING_2   3       /* [src] _PlaySample2IfNotTraining */
#define SFX_PRESET_NOT_TRAINING_4   4       /* [src] _PlaySample4IfNotTraining */
#define SFX_PRESET_PENALTY          6       /* [src] _PlayPenaltySample */
#define SFX_PRESET_NOT_TRAINING_7   7       /* [src] _PlaySample7IfNotTraining */

#endif /* SWOS_SFX_IDS_H */
