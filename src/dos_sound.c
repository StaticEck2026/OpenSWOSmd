/*
 * dos_sound.c -- the PC game's sounds. See include/dos_sound.h and docs/DOS_SOUND.md.
 *
 * Ported from the PC game's disassembly (swos-port's swos.asm): PlayEnqueuedSamples,
 * PlayCommentarySample, PlaySampleFromTable, the Play*Comment routines,
 * LoadCrowdChantSample, LoadAndPlayEndGameComment and the Play*Sample effects, with
 * their volumes and their tables (src/dos_sound_tables.h, generated).
 *
 * Three things are not the PC game's:
 *   - the random numbers. The PC game draws its commentary lines from the game's own
 *     generator; here they come from a private one, so that the sound cannot change
 *     a match (and two players need not hear the same line);
 *   - the delays count frames this player was SHOWN, not frames simulated: under a
 *     rollback a frame is simulated more than once;
 *   - the crowd chants keep the Amiga's rhythm. Their state machine calls the game's
 *     random generator in both releases, with different numbers in each, and the
 *     match depends on those calls: the PC samples are played when the Amiga's
 *     machine starts a chant and at the volume it fades to.
 */
#include "dos_sound.h"

#ifdef SWOS_SDL_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "swos_io.h"
#include "dos_sound_tables.h"

extern void AudioSDL_PcmPlay(int voice, const uint8_t *data, uint32_t len, int rate, int volume, int loops);
extern void AudioSDL_PcmVolume(int voice, int volume);
extern void AudioSDL_PcmStop(int voice);
extern int  AudioSDL_PcmPlaying(int voice);

#define RATE            22050

/* Voices of the mixer (audio_sdl.c has eight). */
#define VOICE_CROWD     0       /* the background crowd, looping */
#define VOICE_CHANT     1
#define VOICE_COMMENT   2
#define VOICE_REACTION  3       /* goal cheer, the sigh, the end-of-game crowd */
#define VOICE_FX_FIRST  4
#define VOICE_FX_COUNT  4

/* soundEffectsTable's order. */
enum { FX_CROWD, FX_BOUNCE, FX_HOMEGOAL, FX_KICK, FX_WHISTLE, FX_MISSGOAL, FX_ENDGAME, FX_FOUL,
       FX_CHANT4L, FX_CHANT10L, FX_CHANT8L };

static int32_t s_style;                 /* 1: the PC's sound */
static int32_t s_commentary = 2;        /* 0 off, 1 on, 2 with the PC's sound only */
static int32_t s_live;                  /* a match is on with something of the PC's to play */
static int32_t s_full;                  /* ... its effects, crowd and chants: the Amiga's are not started */
static int32_t s_training;
static int32_t s_muteCommentary;        /* the PC game's F8 */

static struct { uint8_t *volatile data; uint32_t len; int8_t tried; } s_file[DOS_SOUND_FILES];
static SDL_Thread  *s_loader;           /* reads the files while the game starts */
static volatile int s_loadDone, s_loadGot;

/* What the PC game keeps between its sound calls. */
static struct {
    int16_t yellowTimer, redTimer, goodPassTimer, deletedTimer, throwInTimer, cornerTimer,
            substituteTimer, tacticsTimer;      /* -1: none waiting */
    int16_t goodPassCount;
    int32_t performingPenalty;
    int16_t last[16];                           /* the last line of each (offset, size) table */
    int32_t introChant;                         /* a file, or -1: chant4l */
    int32_t resultChant;                        /* a file, or -1 */
    int32_t interesting;                        /* itsAnInterestingGame */
    int32_t fxNext;
    /* The chant being played: the PC game's fade in, hold and fade out, in its ticks. */
    int32_t chantPhase;                         /* 0 none, 1 in, 2 hold, 3 out */
    int32_t chantTimer, chantAcc;
    uint32_t rnd;
} s;

static uint32_t rnd(void)
{
    s.rnd = s.rnd * 1664525u + 1013904223u;
    return s.rnd >> 16;
}

/* ---- files ---- */

static int32_t file_load(int32_t id)
{
    char rel[96], anchored[1024];
    FILE *f;
    int32_t size;

    if (id < 0 || id >= DOS_SOUND_FILES)
        return 0;
    if (s_file[id].tried)
        return s_file[id].data != NULL;
    s_file[id].tried = 1;
    snprintf(rel, sizeof rel, "sound_dos/%s", k_dosSoundFiles[id]);
    f = fopen(swos_data_path(rel, anchored, (uint32_t)sizeof anchored), "rb");
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    size = (int32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 0 && size < 4 * 1024 * 1024) {
        uint8_t *data = (uint8_t *)malloc((size_t)size);
        if (data && fread(data, 1, (size_t)size, f) == (size_t)size) {
            s_file[id].len = (uint32_t)size;
            s_file[id].data = data;         /* last: the game plays what has a pointer */
        } else {
            free(data);
        }
    }
    fclose(f);
    return s_file[id].data != NULL;
}

/* All 156 files, 9 MB, read on a thread of their own from the moment the PC sounds are
 * asked for -- never at kick-off. Read there, they held the first frame of a match back
 * by as long as the disk took: an online match timed out before it began, and the
 * replay rig's first frame came out different (its clock runs in real time). A sound
 * whose file has not arrived yet is not played. */
static int load_all(void *unused)
{
    int32_t i, got = 0;
    (void)unused;
    for (i = 0; i < DOS_SOUND_FILES; i++)
        got += file_load(i);
    s_loadGot = got;
    s_loadDone = 1;
    return 0;
}

static void preload(void)
{
    if (s_loader || s_loadDone)
        return;
#ifdef SWOS_WASM_BUILD
    load_all(NULL);         /* no threads, and no sound_dos/ either: the first fopen fails */
#else
    s_loader = SDL_CreateThread(load_all, "dos-sound", NULL);
    if (!s_loader)
        load_all(NULL);
#endif
}

/* SWOS_DOS_SOUND_TRACE=1: every sample started, on stderr. */
static int32_t trace_on(void)
{
    static int32_t on = -1;
    if (on < 0)
        on = getenv("SWOS_DOS_SOUND_TRACE") != NULL;
    return on;
}

static void play(int32_t voice, int32_t id, int32_t volume, int32_t loops)
{
    if (id == DOS_SOUND_NONE || id < 0 || id >= DOS_SOUND_FILES || !s_file[id].data)
        return;
    if (trace_on())
        fprintf(stderr, "[dos-sound] voice %d: %s, volume %d%s\n", voice, k_dosSoundFiles[id], volume,
                loops ? ", looped" : "");
    AudioSDL_PcmPlay(voice, s_file[id].data, s_file[id].len, RATE, volume, loops);
}

static void play_fx(int32_t fx, int32_t volume)
{
    play(VOICE_FX_FIRST + s.fxNext, k_dosEffects[fx], volume, 0);
    s.fxNext = (s.fxNext + 1) % VOICE_FX_COUNT;
}

/* ---- commentary ---- */

static int32_t commentary_wanted(void)
{
    return s_commentary == 1 || (s_commentary == 2 && s_style);
}

static int32_t commentary_on(void)
{
    return commentary_wanted() && !s_muteCommentary && !s_training;
}

/* PlayCommentarySample: a line of an (offset, size) table, never the last one played
 * from it. It replaces the line being spoken. */
static void comment(const uint8_t *table, int32_t mask, int32_t slot)
{
    int32_t pick;
    if (!commentary_on())
        return;
    pick = (int32_t)rnd() & mask;
    if (pick == s.last[slot])
        pick = (pick + 1) & mask;
    s.last[slot] = (int16_t)pick;
    play(VOICE_COMMENT, table[pick], 127, 0);
}

/* PlaySampleFromTable and its two relatives: a file of a table of names. */
static void comment_file(const uint8_t *table, int32_t mask)
{
    if (!commentary_on())
        return;
    play(VOICE_COMMENT, table[(int32_t)rnd() & mask], 127, 0);
}

enum { L_GOAL, L_SAVED, L_OWN_GOAL, L_NEAR_MISS, L_POST, L_BAR, L_CLAIMED, L_TACKLE, L_PASS,
       L_FOUL, L_DANGEROUS, L_PENALTY, L_PEN_SAVED, L_PEN_MISSED, L_PEN_GOAL };

static void penalty_goal(void)
{
    s.performingPenalty = 0;
    comment(k_dos_penaltyGoalSamples, 3, L_PEN_GOAL);
}

static void penalty_missed(void)
{
    s.performingPenalty = 0;
    comment(k_dos_missedPenaltySamples, 3, L_PEN_MISSED);
}

static void penalty_saved(void)
{
    s.performingPenalty = 0;
    comment(k_dos_savedPenaltySamples, 3, L_PEN_SAVED);
}

/* ---- chants ---- */

/* LoadCrowdChantSample: after a goal, the chant about the score, or none. */
static void result_chant(int32_t goals1, int32_t goals2)
{
    int32_t pick;

    s.interesting = 0;
    s.resultChant = -1;
    if (!s_full || s_training)
        return;
    if (goals1 != 0 && goals2 != 0) {
        if (goals1 != goals2)
            return;
        pick = 8;                           /* the equaliser */
    } else {
        if (goals1 == goals2)
            return;
        pick = goals1 ? goals1 : goals2;
        if (pick > 6)
            return;
        if (!(rnd() & 1))
            pick = 7;                       /* cheer the losing team up */
    }
    s.resultChant = k_dosResultChants[pick - 1];
    s.interesting = 1;
    AudioSDL_PcmStop(VOICE_CHANT);          /* StopChantsSample */
    s.chantPhase = 0;
}

void DosSound_ChantStart(int32_t slot)
{
    int32_t id;

    if (!s_full || s_training)
        return;
    /* playIntroChantSampleOffset, 8l, 10l (the result chant in its place), 4l -- and
     * an interesting game is given the result chant whatever was picked. */
    if (s.interesting && s.resultChant >= 0)
        id = s.resultChant;
    else if (slot == 0)
        id = s.introChant >= 0 ? s.introChant : k_dosEffects[FX_CHANT4L];
    else if (slot == 1)
        id = k_dosEffects[FX_CHANT8L];
    else if (slot == 2)
        id = k_dosEffects[FX_CHANT10L];
    else
        id = k_dosEffects[FX_CHANT4L];
    /* PlayChantSample22Khz: volume 0 and repeating until stopped (it hands AIL a loop
     * count of 0 whatever its caller asked for). The fade brings it in. */
    play(VOICE_CHANT, id, 0, -1);
    s.chantPhase = 1;
    s.chantTimer = 127;                     /* fadeInChantTimer */
    s.chantAcc = 0;
}

/* The Amiga's chant channel volume. Not followed: the Amiga holds a chant for 500 to
 * 1010 of its frames, 10 to 20 seconds, where the PC game holds one for 250 to 760 of
 * its ticks, 3.6 to 10.9 seconds -- and a PC chant is one short phrase repeated. So the
 * Amiga's machine says WHEN a chant starts, and chant_tick gives it the PC's length. */
void DosSound_ChantVolume(int32_t volume)
{
    (void)volume;
}

/* One frame shown. The PC game's PlayCrowdChants runs once a tick of its 70 a second. */
static void chant_tick(void)
{
    extern int DosGameplay_TargetFps(void);
    int32_t fps = DosGameplay_TargetFps();

    if (!s.chantPhase)
        return;
    for (s.chantAcc += 70; s.chantAcc >= fps && s.chantPhase; s.chantAcc -= fps) {
        if (s.chantPhase == 1) {            /* crowdChantVolume = 127 - fadeInChantTimer */
            AudioSDL_PcmVolume(VOICE_CHANT, 127 - --s.chantTimer);
            if (s.chantTimer == 0) {
                s.chantPhase = 2;
                s.chantTimer = (int32_t)(rnd() & 0xFF) * 2 + 250;      /* nextChantTimer */
            }
        } else if (s.chantPhase == 2) {
            if (--s.chantTimer == 0) {
                s.chantPhase = 3;
                s.chantTimer = 127;         /* fadeOutChantTimer */
            }
        } else {                            /* crowdChantVolume = fadeOutChantTimer; 0 stops it */
            if (--s.chantTimer == 0) {
                AudioSDL_PcmStop(VOICE_CHANT);
                s.chantPhase = 0;
            } else {
                AudioSDL_PcmVolume(VOICE_CHANT, s.chantTimer);
            }
        }
    }
}

/* ---- the match ---- */

static void match_start(uint16_t arg)
{
    int32_t i, colour = arg & 0x0F;

    DosSound_Stop();
    if (!s_style && !commentary_wanted())
        return;
    preload();
    if (s_loadDone && s_loadGot != DOS_SOUND_FILES) {
        fprintf(stderr, "[sound] the PC sounds are incomplete (%d of %d files in sound_dos/): "
                "the Amiga's are used\n", (int)s_loadGot, DOS_SOUND_FILES);
        return;
    }
    memset(&s, 0, sizeof s);
    s.yellowTimer = s.redTimer = s.goodPassTimer = s.deletedTimer = -1;
    s.throwInTimer = s.cornerTimer = s.substituteTimer = s.tacticsTimer = -1;
    for (i = 0; i < 16; i++)
        s.last[i] = -1;
    s.resultChant = -1;
    s.rnd = (uint32_t)SDL_GetTicks() * 2654435761u + 1;
    /* LoadIntroChant: a chant for the home side's shirt colour, where there is one. */
    s.introChant = k_dosIntroChantByColour[colour] >= 0
                 ? k_dosIntroChants[k_dosIntroChantByColour[colour]] : -1;
    s_training = (arg & 0x100) != 0;
    s_muteCommentary = 0;
    s_live = 1;
    s_full = s_style;
    if (s_full && !s_training)
        play(VOICE_CROWD, k_dosEffects[FX_CROWD], 100, -1);     /* PlayCrowdNoiseSample */
    fprintf(stderr, "[sound] %s%s\n", s_full ? "PC sounds" : "Amiga sounds",
            commentary_wanted() ? " with the PC commentary" : ", no commentary");
}

/* PlayEnqueuedSamples: at most one waiting line a frame, in the PC game's order. */
static int32_t due(int16_t *timer)
{
    if (*timer < 0)
        return 0;
    if (--*timer != 0)
        return 0;
    *timer = -1;
    return 1;
}

static void tick(uint16_t arg)
{
    chant_tick();
    if (due(&s.yellowTimer))
        comment_file(k_dos_yellowCardSamplesTable, 15);
    else if (due(&s.redTimer))
        comment_file(k_dos_redCardSamplesTable, 15);
    else if (due(&s.goodPassTimer)) {
        if (!AudioSDL_PcmPlaying(VOICE_COMMENT))        /* never over another line */
            comment(k_dos_goodPassSamples, 3, L_PASS);
    } else if (due(&s.deletedTimer))
        ;
    else if (due(&s.throwInTimer))
        comment_file(k_dos_throwInSamplesTable, 7);
    else if (due(&s.cornerTimer)) {
        if (!(arg & 1))                                 /* not in a shoot-out */
            comment_file(k_dos_cornerSamplesTable, 7);
    } else if (due(&s.substituteTimer))
        comment_file(k_dos_playerSubstitutedSamplesTable, 7);
    else if (due(&s.tacticsTimer))
        comment_file(k_dos_tacticsChangeSamplesTable, 7);
}

/* LoadAndPlayEndGameComment. */
static void game_over(int32_t goals1, int32_t goals2)
{
    int32_t diff = goals1 > goals2 ? goals1 - goals2 : goals2 - goals1;

    if (!s_training && s_full)
        play(VOICE_REACTION, k_dosEndGameCrowd[goals1 == goals2 ? 2 : goals2 < goals1 ? 0 : 1], 127, 0);
    if (diff >= 3)
        comment_file(k_dos_completeRoutSamplesTable, 7);
    /* The PC game means to say "so close" after a draw, and never does: its test
     * for a difference of 0 follows a jump taken for every difference of 0 or more
     * (swos.asm, LoadAndPlayEndGameComment, "this jump is never taken"). Kept. */
    else if (goals1 + goals2 >= 4)
        comment_file(k_dos_sensationalGameSamplesTable, 7);
}

void DosSound_Event(uint8_t event, uint16_t arg)
{
    /* The arcade cabinet's attract reels play under its title song (arcade.c). */
    extern int _gArcadeSfxSuppressed;
    if (_gArcadeSfxSuppressed)
        return;
    if (event == DOS_EV_MATCH_START) {
        match_start(arg);
        return;
    }
    if (!s_live)
        return;
    if (trace_on() && event != DOS_EV_TICK)
        fprintf(stderr, "[dos-sound] event %u arg %u\n", event, arg);

    switch (event) {
    case DOS_EV_MATCH_END:      DosSound_Stop(); break;
    case DOS_EV_TICK:           tick(arg); break;

    case DOS_EV_KICK:           if (s_full) play_fx(FX_KICK, 25); break;
    case DOS_EV_BOUNCE:         if (s_full) play_fx(FX_BOUNCE, 42); break;
    case DOS_EV_WHISTLE:        if (s_full) play_fx(FX_WHISTLE, 42); break;
    case DOS_EV_FOUL_WHISTLE:   if (s_full) play_fx(FX_FOUL, 42); break;
    case DOS_EV_END_WHISTLE:    if (s_full) play_fx(FX_ENDGAME, 42); break;
    case DOS_EV_GOAL_CROWD:
        if (s_full && !s_training)
            play(VOICE_REACTION, k_dosEffects[FX_HOMEGOAL], 127, 0);
        break;
    case DOS_EV_MISS_CROWD:
        if (s_full && !s_training)
            play(VOICE_REACTION, k_dosEffects[FX_MISSGOAL], 127, 0);
        break;

    case DOS_EV_GOAL:
        if (s.performingPenalty || (arg & 1))
            penalty_goal();
        else
            comment(k_dos_goalSamples, 7, L_GOAL);
        break;
    case DOS_EV_OWN_GOAL:       comment(k_dos_ownGoalSamples, 7, L_OWN_GOAL); break;
    case DOS_EV_NEAR_MISS:
        if (s.performingPenalty || (arg & 1))
            penalty_missed();
        else
            comment(k_dos_nearMissesSamples, 3, L_NEAR_MISS);
        break;
    case DOS_EV_KEEPER_SAVED:
        if (s.performingPenalty || (arg & 1))
            penalty_saved();
        else
            comment(k_dos_goalkeeperSavedSamples, 7, L_SAVED);
        break;
    case DOS_EV_KEEPER_CLAIMED: comment(k_dos_keeperClaimedSamples, 7, L_CLAIMED); break;
    case DOS_EV_WOODWORK:
        /* UpdateBall: half the time the post line; the other half the bar line,
         * unless the game's flag says post. */
        if (!(rnd() & 1) || arg)
            comment(k_dos_postHitSamples, 7, L_POST);
        else
            comment(k_dos_barHitSamples, 1, L_BAR);
        break;
    case DOS_EV_PENALTY:
        s.performingPenalty = 1;
        comment(k_dos_itsPenaltySamples, 3, L_PENALTY);
        break;
    case DOS_EV_FOUL:           comment(k_dos_foulSamples, 7, L_FOUL); break;
    case DOS_EV_DANGEROUS_PLAY: comment(k_dos_dangerousPlaySamples, 3, L_DANGEROUS); break;
    case DOS_EV_GOOD_TACKLE:
        if (!AudioSDL_PcmPlaying(VOICE_COMMENT))
            comment(k_dos_goodTackleSamples, 3, L_TACKLE);
        break;
    case DOS_EV_PASS:
        /* EnqueuPlayingGoodPassSample: any pass drops a waiting line; the fifth asks for one. */
        s.goodPassTimer = -1;
        if (++s.goodPassCount == 5) {
            s.goodPassCount = 0;
            s.goodPassTimer = 10;
        }
        break;
    case DOS_EV_CORNER:         s.cornerTimer = 100; break;
    case DOS_EV_THROW_IN:       s.throwInTimer = 70; break;
    case DOS_EV_YELLOW_CARD:    s.yellowTimer = 100; break;
    case DOS_EV_RED_CARD:       s.redTimer = 100; break;
    case DOS_EV_SUBSTITUTION:   s.substituteTimer = 70; break;
    case DOS_EV_TACTICS:        s.tacticsTimer = 70; break;
    case DOS_EV_SCORE:          result_chant(arg >> 8, arg & 0xFF); break;
    case DOS_EV_GAME_OVER:      game_over(arg >> 8, arg & 0xFF); break;
    default: break;
    }
}

void DosSound_Stop(void)
{
    if (s_live)
        AudioSDL_PcmStop(-1);
    s_live = 0;
    s_full = 0;
}

int32_t DosSound_Live(void)
{
    return s_full;
}

void DosSound_SetStyle(int32_t pc)
{
    s_style = pc != 0;
    if (!s_style)
        DosSound_Stop();
    if (s_style || commentary_wanted())
        preload();
}

int32_t DosSound_Style(void)
{
    return s_style;
}

void DosSound_SetCommentary(int32_t on)
{
    s_commentary = on < 0 || on > 2 ? 2 : on;
    if (commentary_wanted())
        preload();
}

/* ---- self-test ---- */

#define TABLE(t)    { #t, t, (int32_t)sizeof t }

int32_t DosSound_SelfTest(void)
{
    static const struct { const char *name; const uint8_t *ids; int32_t count; } tables[] = {
        TABLE(k_dosEffects), TABLE(k_dosIntroChants), TABLE(k_dosResultChants), TABLE(k_dosEndGameCrowd),
        TABLE(k_dos_goalSamples), TABLE(k_dos_goalkeeperSavedSamples), TABLE(k_dos_ownGoalSamples),
        TABLE(k_dos_nearMissesSamples), TABLE(k_dos_postHitSamples), TABLE(k_dos_barHitSamples),
        TABLE(k_dos_keeperClaimedSamples), TABLE(k_dos_goodTackleSamples), TABLE(k_dos_goodPassSamples),
        TABLE(k_dos_foulSamples), TABLE(k_dos_dangerousPlaySamples), TABLE(k_dos_itsPenaltySamples),
        TABLE(k_dos_savedPenaltySamples), TABLE(k_dos_missedPenaltySamples), TABLE(k_dos_penaltyGoalSamples),
        TABLE(k_dos_cornerSamplesTable), TABLE(k_dos_tacticsChangeSamplesTable),
        TABLE(k_dos_playerSubstitutedSamplesTable), TABLE(k_dos_throwInSamplesTable),
        TABLE(k_dos_redCardSamplesTable), TABLE(k_dos_yellowCardSamplesTable), TABLE(k_dos_drawSamplesTable),
        TABLE(k_dos_completeRoutSamplesTable), TABLE(k_dos_sensationalGameSamplesTable),
    };
    int32_t failures = 0, i, j;
    uint32_t bytes = 0;

    for (i = 0; i < DOS_SOUND_FILES; i++) {
        if (!file_load(i)) {
            fprintf(stderr, "[dos-sound] FAIL: sound_dos/%s is missing\n", k_dosSoundFiles[i]);
            failures++;
        } else {
            bytes += s_file[i].len;
        }
    }
    for (i = 0; i < (int32_t)(sizeof tables / sizeof tables[0]); i++) {
        for (j = 0; j < tables[i].count; j++) {
            uint8_t id = tables[i].ids[j];
            if (id != DOS_SOUND_NONE && id >= DOS_SOUND_FILES) {
                fprintf(stderr, "[dos-sound] FAIL: %s[%d] = %u names no file\n", tables[i].name, j, id);
                failures++;
            }
        }
    }
    fprintf(stderr, "[dos-sound] %d files, %u bytes (%.1f s at %d Hz), %d tables: %s\n",
            DOS_SOUND_FILES, bytes, bytes / (double)RATE, RATE, (int32_t)(sizeof tables / sizeof tables[0]),
            failures ? "FAILED" : "PASSED");
    return failures;
}

#endif /* SWOS_SDL_BUILD */
