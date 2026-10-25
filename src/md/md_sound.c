/* md_sound.c -- the match's sound, on the Mega Drive's PSG.
 *
 * The engine (sfx_crowd.c and the match code) says WHAT to play, exactly as on the Amiga:
 * a wave id on a channel (AudioChannelQueueNextWaveIfEnabled: a kick, a whistle, a
 * bounce, the crowd's "oooh"), a preset (AudioApplyPresetToChannels: the crowd at kick-off,
 * the cheer after a goal) and channel volumes (the chants' fades, the end-of-match fade).
 * The Amiga plays its samples for those (sound/sfx.sng); the cartridge carries none, so
 * this file makes each one on the PSG -- three square waves and a noise generator -- from
 * the 50 Hz game tick (md_sound_tick, called by md_isr.c):
 *
 *   tone 0   the referee's whistle: a pea whistle's trill, the blasts each whistle has
 *   tone 1   the ball: a short falling thump for a kick, a header, a bounce
 *   tone 2   the crowd's voice: the "oooh" and "aaah", the hum under a chant
 *   noise    the crowd: the murmur all match, swelling for a goal, a near miss, a card;
 *            a chant's beat; and the strike of a kick, over the crowd for a moment
 *
 * None of this is match state: the engine never reads anything back.
 */
#include "md_hw.h"
#include "md_game.h"
#include "sfx_ids.h"

extern volatile uint16_t _audioActive;

#define PSG(b) (MD_PSG = (uint8_t)(b))

uint8_t md_soundOn = 1;

/* ---- what is playing (written by the engine's calls, read by the tick) ---- */
static volatile uint8_t s_master = 0x40;        /* AudioSetChannelVolume(all): 0..0x40 */
static volatile uint8_t s_chantVol;             /* channel 2's volume: 0..0x40 */
static volatile uint8_t s_crowdOn;              /* the murmur */

/* the whistle: blasts as (on, off) tick pairs, ended by an `on` of 0 */
static const uint8_t kBlastKickoff[] = { 38, 0, 0 };
static const uint8_t kBlastFoul[]    = { 14, 0, 0 };
static const uint8_t kBlastEnd[]     = { 14, 8, 14, 8, 42, 0, 0 };
static const uint8_t *volatile s_blast;
static volatile uint8_t s_blastT, s_blastOn;

/* the ball: a thump of `len` ticks from level `lvl`, and a noise strike of `strike` ticks */
static volatile uint8_t s_ballT, s_ballLen, s_ballLvl, s_strikeT, s_strikeLvl;

/* the crowd's swell: up `rise` ticks to `peak`, held `hold`, down `fall` */
typedef struct { uint8_t rise, peak, hold, fall; } Swell;
static const Swell kCheer   = { 10, 13, 110, 120 };
static const Swell kStart   = { 16, 11, 40, 90 };
static const Swell kOooh    = { 8, 10, 22, 45 };
static const Swell kCard    = { 15, 12, 50, 60 };
static const Swell kEndHalf = { 20, 11, 60, 100 };
static const Swell *volatile s_swell;
static volatile uint16_t s_swellT;
static volatile uint8_t s_voiceKind;            /* 1 "oooh", 2 "aaah" on tone 2 */
static volatile uint8_t s_voiceT;

/* a chant: a beat pattern on the noise (1 = a shout), 16 steps of `step` ticks */
static const uint16_t kChantBeat[4] = { 0xA0A8, 0x8888, 0xAA80, 0x9292 };
static volatile uint8_t s_chant;                /* 1..4, 0 none */
static uint8_t s_chantStep, s_chantTick;

static uint8_t s_noiseMode = 0xFF;

/* ---- the PSG ---- */

static uint16_t tone_n(uint16_t hz)              /* the PSG's divider for a frequency */
{
    uint32_t clk = md_is_pal() ? 3546893u : 3579545u;
    uint32_t n = clk / (32u * hz);
    return (uint16_t)(n > 1023 ? 1023 : n < 1 ? 1 : n);
}

static void psg_tone(uint8_t ch, uint16_t n)
{
    PSG(0x80 | (ch << 5) | (n & 15));
    PSG((n >> 4) & 0x3F);
}

/* level 0 (off) .. 15 (loudest), scaled by the master volume */
static void psg_level(uint8_t ch, uint8_t lvl)
{
    uint16_t l = (uint16_t)(lvl * s_master / 0x40);
    if (!md_soundOn || !_audioActive) l = 0;
    PSG(0x90 | (ch << 5) | (15 - (l > 15 ? 15 : l)));
}

static void psg_noise(uint8_t mode)              /* white noise, rate 0..2 */
{
    if (mode == s_noiseMode) return;            /* a write restarts the noise: only on change */
    s_noiseMode = mode;
    PSG(0xE0 | 4 | mode);
}

void md_sound_init(void)
{
    for (uint8_t ch = 0; ch < 4; ch++) PSG(0x90 | (ch << 5) | 15);
    s_noiseMode = 0xFF;
}

static void silence(void)
{
    s_blast = 0; s_ballT = 0; s_strikeT = 0; s_swell = 0; s_voiceKind = 0; s_chant = 0;
    s_crowdOn = 0;
    md_sound_init();
}

/* ---- the engine's calls (audio.c on the Amiga) ---- */

static void whistle(const uint8_t *pattern)
{
    s_blast = pattern;
    s_blastT = pattern[0];
    s_blastOn = 1;
}

static void ball(uint8_t len, uint8_t lvl, uint8_t strike, uint8_t strikeLvl)
{
    s_ballLen = s_ballT = len;
    s_ballLvl = lvl;
    if (strike) { s_strikeT = strike; s_strikeLvl = strikeLvl; }
}

static void swell(const Swell *sw)
{
    s_swell = sw;
    s_swellT = 0;
}

static void voice(uint8_t kind)
{
    s_voiceKind = kind;
    s_voiceT = 0;
}

void AudioChannelQueueNextWaveIfEnabled(uint16_t channel, uint16_t waveId)
{
    if (channel == 2) {                         /* the chants (PlayCrowdChants) */
        s_chant = waveId == SFX_CROWD_CHANT_1 ? 1 : waveId == SFX_CROWD_CHANT_2 ? 2 :
                  waveId == SFX_CROWD_CHANT_3 ? 3 : waveId == SFX_CROWD_CHANT_4 ? 4 : 0;
        s_chantStep = 0; s_chantTick = 0;
        return;
    }
    switch (waveId) {
    case SFX_CROWD_BG_1: case SFX_CROWD_BG_2:            s_crowdOn = 1; break;
    case SFX_WHISTLE_FOUL_1: case SFX_WHISTLE_FOUL_2:    whistle(kBlastFoul); break;
    case SFX_WHISTLE_KICKOFF:                            whistle(kBlastKickoff); break;
    case SFX_WHISTLE_ENDGAME: case SFX_WHISTLE_HALFTIME: whistle(kBlastEnd); break;
    case SFX_CROWD_GOAL_CHEER:                           s_crowdOn = 1; swell(&kCheer); break;
    case SFX_CROWD_MATCH_ENGINE_START:                   s_crowdOn = 1; swell(&kStart); break;
    case SFX_CROWD_REFEREE_CARD_FADEOUT: case SFX_CROWD_CHEER_FADEOUT:
        if (s_swell && s_swellT < (uint16_t)(s_swell->rise + s_swell->hold))
            s_swellT = (uint16_t)(s_swell->rise + s_swell->hold);
        break;
    case SFX_CROWD_KEEPERSAVES:                          swell(&kOooh); voice(2); break;
    case SFX_CROWD_NEARMISS:                             swell(&kOooh); voice(1); break;
    case SFX_CROWD_REFEREE_CARD:                         swell(&kCard); break;
    case SFX_BALL_SHOT_HEADER_LOW: case SFX_BALL_KEEPER_SAFE_1: case SFX_BALL_KEEPER_SAFE_2:
        ball(5, 13, 3, 12); break;
    case SFX_BALL_PASSED_OR_TACKLED:                     ball(4, 10, 2, 9); break;
    case SFX_BALL_HEADER_STANDING:                       ball(4, 9, 1, 7); break;
    case SFX_BALL_TACKLED_FOUL:                          ball(4, 9, 3, 10); break;
    case SFX_BALL_BOUNCE_1: case SFX_BALL_BOUNCE_2: case SFX_BALL_BOUNCE_3:
    case SFX_BALL_BOUNCE_4: case SFX_BALL_BOUNCE_5: case 0x11:
        if (!s_ballT) ball(3, 7, 0, 0);
        break;
    default:
        break;
    }
}

/* The presets (sfx_crowd.c PlaySample): 1 kick-off's crowd, 6 and 7 the crowd at the end
 * of a half and after, 3 and 4 the crowd after a goal. */
void AudioApplyPresetToChannels(uint16_t presetId)
{
    switch (presetId & 0x3F) {
    case 0:  break;
    case 1:  s_crowdOn = 1; swell(&kStart); break;
    case 3: case 4: s_crowdOn = 1; swell(&kCheer); break;
    case 6:  s_crowdOn = 1; swell(&kEndHalf); break;
    default: s_crowdOn = 1; break;
    }
}

void AudioSetChannelVolume(uint16_t channelOrAll, uint16_t volume)
{
    uint8_t v = (uint8_t)(volume > 0x40 ? 0x40 : volume);
    if (channelOrAll == 0xFFFF) s_master = v;
    else if (channelOrAll == 2) s_chantVol = v;
}

void ClearAudioActiveFlag(void) { _audioActive = 0; }
void StopAudioAndClearFlags(void) { _audioActive = 0; silence(); }
void LoadSfxResources(void) {}
void EnsureSfxLoadedAndStart(void) { _audioActive = 1; s_master = 0x40; }

void md_sound_stop(void) { silence(); }

/* ---- the tick ---- */

static uint8_t swell_level(void)
{
    const Swell *sw = s_swell;
    if (!sw) return 0;
    uint16_t t = s_swellT++;
    if (t < sw->rise) return (uint8_t)(sw->peak * (t + 1) / sw->rise);
    t = (uint16_t)(t - sw->rise);
    if (t < sw->hold) return sw->peak;
    t = (uint16_t)(t - sw->hold);
    if (t < sw->fall) return (uint8_t)(sw->peak * (sw->fall - t) / sw->fall);
    s_swell = 0;
    return 0;
}

void md_sound_tick(void)
{
    static uint8_t trill;
    uint8_t noiseLvl = 0, noiseMode = 1;

    /* the whistle */
    if (s_blast) {
        if (s_blastOn) {
            trill ^= 1;
            psg_tone(0, tone_n(trill ? 2900 : 2500));
            psg_level(0, s_blastT > 3 ? 12 : (uint8_t)(s_blastT * 4));
        } else {
            psg_level(0, 0);
        }
        if (--s_blastT == 0) {
            if (s_blastOn && s_blast[1]) {
                s_blastOn = 0;
                s_blastT = s_blast[1];
            } else {                            /* the next blast, or the end */
                s_blast += 2;
                if (s_blast[0]) { s_blastOn = 1; s_blastT = s_blast[0]; }
                else { s_blast = 0; psg_level(0, 0); }
            }
        }
    } else {
        psg_level(0, 0);
    }

    /* the ball: the pitch falls through the thump */
    if (s_ballT) {
        uint8_t k = (uint8_t)(s_ballLen - s_ballT);
        psg_tone(1, tone_n((uint16_t)(260 - k * 35)));
        psg_level(1, (uint8_t)(s_ballLvl * s_ballT / s_ballLen));
        s_ballT--;
    } else {
        psg_level(1, 0);
    }

    /* the crowd: the murmur, a swell, a chant's beat, and a kick's strike over them */
    if (s_crowdOn) noiseLvl = 5;
    {
        uint8_t sw = swell_level();
        if (sw > noiseLvl) { noiseLvl = sw; if (sw > 9) noiseMode = 0; }
    }
    if (s_chant && s_chantVol) {
        uint16_t beat = kChantBeat[s_chant - 1];
        uint8_t shout = (uint8_t)((beat >> (15 - s_chantStep)) & 1);
        uint8_t lvl = (uint8_t)((s_chantVol * 10) / 0x40);
        if (shout && s_chantTick < 7) {
            uint8_t c = (uint8_t)(lvl + 1 - s_chantTick / 3);
            if (c > noiseLvl) noiseLvl = c;
        }
        if (++s_chantTick >= 10) { s_chantTick = 0; s_chantStep = (uint8_t)((s_chantStep + 1) & 15); }
    }
    if (s_strikeT) {
        if (s_strikeLvl > noiseLvl) noiseLvl = s_strikeLvl;
        noiseMode = 0;
        s_strikeT--;
    }
    psg_noise(noiseMode);
    psg_level(3, noiseLvl);

    /* the crowd's voice */
    if (s_voiceKind) {
        uint8_t t = s_voiceT++;
        uint16_t hz = s_voiceKind == 1 ? (uint16_t)(t < 12 ? 300 + t * 6 : 372 - (t - 12) * 3)
                                       : (uint16_t)(320 - t * 2);
        uint8_t lvl = t < 6 ? (uint8_t)(t + 2) : t < 30 ? 7 : (uint8_t)(t < 44 ? (44 - t) / 2 : 0);
        psg_tone(2, tone_n(hz < 150 ? 150 : hz));
        psg_level(2, lvl);
        if (t >= 44) s_voiceKind = 0;
    } else if (s_chant && s_chantVol) {
        psg_tone(2, tone_n(s_chantStep & 4 ? 196 : 247));
        psg_level(2, (uint8_t)((s_chantVol * 5) / 0x40));
    } else {
        psg_level(2, 0);
    }
}

void md_sound_frame(void) {}
