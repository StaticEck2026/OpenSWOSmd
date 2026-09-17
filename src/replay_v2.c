/*
 * replay_v2.c -- whole-match replays in the SWRP format: recorder, file, player.
 * See include/replay_v2.h; docs/REPLAY_FORMAT_V2.md is the specification.
 *
 * What a .RPL / .HIL records is what a 336-wide screen drew: sprite positions relative to
 * the camera, packed into 10 bits, no sound. A .SRP records the match instead:
 *
 *   - every WORLD sprite in the draw list, drawn or not, at its world position -- so a
 *     replay recorded on a 4:3 screen fills a 16:9 one, and the other way round;
 *   - the SIMULATION camera (_cameraX), which is the same on every display, so playback
 *     maps it onto whatever display it is shown on exactly as a live match does
 *     (Render_CameraX) instead of guessing a camera from the ball;
 *   - every sound effect and channel volume the match set, so the kicks, whistles, goal
 *     roars and crowd chants of the match are the ones heard.
 *
 * SDL-only: the Amiga build has no file for it and no use for widescreen.
 */
#ifdef SWOS_SDL_BUILD

#include "replay_v2.h"
#include "audio_sfx_filter.h"
#include "build_info.h"
#include "display.h"
#include "endian_helpers.h"
#include "game_types.h"
#include "miniz_swos.h"
#include "swos_io.h"
#include "image_cmd.h"      /* ProcessImageCommandStream */
#include "match_sim.h"      /* InitTeams */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- the file ---------------------------------------------------------------------- */

#define SWRP_VERSION        1
#define SWRP_HEADER_BYTES   32
#define SWRP_DIR_BYTES      16
#define SWRP_FLAG_AUDIO     0x0001u
#define SWRP_FLAG_SEEK      0x0002u
#define SWRP_FLAG_CHAPTERS  0x0004u
#define SWRP_QUIRK_ORIGINAL_FIRE_HOLD_LOCKOUT 0x0001u

#define FOURCC(a, b, c, d)  ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define CHUNK_META          FOURCC('M', 'E', 'T', 'A')
#define CHUNK_FRMS          FOURCC('F', 'R', 'M', 'S')
#define CHUNK_AUDI          FOURCC('A', 'U', 'D', 'I')
#define CHUNK_SEEK          FOURCC('S', 'E', 'E', 'K')
#define CHUNK_CHAP          FOURCC('C', 'H', 'A', 'P')

#define FRAME_HEAD_BYTES    20      /* count, camera x/y (16.16), goals, crowd, minute, flags */
#define SPRITE_BYTES        6       /* image, world x, world y */
#define AUDIO_EVENT_BYTES   8       /* frame, kind, channel, id */
#define CHAPTER_BYTES       8       /* frame, kind, team, value */
#define SEEK_INTERVAL       50
#define TEAM_BYTES          1018    /* a TeamGame as the HIL header holds it */
#define TEAM_HDR_INT16S     11
#define META_SCORE_OFF      (2 + 2 + 2 + 2 + 4 + 32)    /* the final score inside META */
#define META_BYTES          (2 + 2 + 2 + 2 + 4 + 32 + 2 + 2 + 2 + 2 + 2 + 40 + 40 + 2 * TEAM_BYTES)
#define MAX_FRAME_SPRITES   78      /* _sortedSprites */

/* Sprites at z >= this are the screen-pinned HUD (sprites.c HUD_LAYER_Z_MIN): placed
 * for this display, and drawn by the viewer's own overlay instead. */
#define HUD_Z_MIN           8000

enum { CHAPTER_GOAL = 1, CHAPTER_HALF = 2 };

typedef struct {
    uint32_t frame;
    uint8_t  kind, channel;
    uint16_t id;
} AudioEvent;

typedef struct {
    uint32_t frame;
    uint8_t  kind, team;
    uint16_t value;
} Chapter;

/* ---- game state it reads and writes ------------------------------------------------ */

extern Sprite     *_sortedSprites[];
extern uint16_t    _numSpritesToRender;
extern uint8_t    *_spriteGraphicsPtr;
extern FixedPoint  _cameraX, _cameraY;
extern uint16_t    _team1TotalGoals, _team2TotalGoals;
extern uint16_t    _animPatternsState;
extern uint8_t     _gameTime[4];
extern uint16_t    _halfNumber;
extern uint16_t    _statsTeam1Goals, _statsTeam2Goals;
extern uint16_t    _pitchNumberAndType, _gameMaxSubstitutes;
extern uint8_t     _gameName[], _gameRound[];
extern TeamGame    _topTeamInGame, _bottomTeamInGame;
extern uint16_t    _trainingGame;
extern int         _gArcadeMode, _gDosGameplay, _gDosGraphics, _gOriginalFireHoldLockout;
extern int         _screen_width;

extern uint8_t     _hilFileBuffer[];
extern uint16_t    _hilNumGoals, _hilTeam1Goals, _hilTeam2Goals;
extern uint16_t    _hilPitchNumber, _hilMaxSubstitutes;
extern uint8_t     _hilGameName[], _hilGameRound[];
extern uint16_t    _hilLeftTeamGoals, _hilRightTeamGoals;
extern volatile uint16_t _numScenesInHilFile;
extern uint16_t    _replayState;
extern int16_t     _savedCameraX, _savedCameraY;
extern int         _replayPlaybackFps, _replayRecordingIsWide;

extern int         DosGameplay_TargetFps(void);
extern int         ReplayTest_Armed(void);
extern void        DosReplay_ClearRpl(void);
extern void        DarkenRectangle(uint16_t sprIdx, int16_t x, int16_t y);
extern void        MoveView(int16_t newScreenX, int16_t newScreenY);
extern void        DrawAnimatedPatterns(void);
extern void        ResetAnimatedPatternsForBothTeams(void);
extern void        SetCameraCoordinates(int16_t targetX, int16_t targetY);
extern void        InitCameraMoveView(int16_t targetX, int16_t targetY);
extern void        SwapBuffersAndCameraViewState(void);
extern void        MirrorFrontbuffer(void);
extern void        AudioApplyPresetToChannels(uint16_t presetId);
extern void        AudioChannelQueueNextWaveIfEnabled(uint16_t channel, uint16_t waveId);
extern void        AudioSetChannelVolume(uint16_t channelOrAll, uint16_t volume);

static SpriteGraphics *graphics(uint16_t imageIndex)
{
    return ((SpriteGraphics **)(void *)_spriteGraphicsPtr)[imageIndex];
}

/* ---- growable buffers --------------------------------------------------------------- */

static int32_t grow(void **buf, uint32_t *cap, uint32_t need, uint32_t elem)
{
    uint32_t n;
    void *p;
    if (need <= *cap)
        return 0;
    n = *cap ? *cap : 4096;
    while (n < need)
        n *= 2;
    p = realloc(*buf, (size_t)n * elem);
    if (!p)
        return 1;
    *buf = p;
    *cap = n;
    return 0;
}

/* ---- the trace (tests) --------------------------------------------------------------- */

/* SWOS_REPLAY_TRACE=<file>: one line per frame, "frame F sprites N hash H", and one per
 * sound, "frame F sound K C I" -- written the same way while recording (the sprites the
 * live match drew) and while playing (the sprites the player drew), so a test can require
 * the two to be identical at the same screen width. */
static FILE    *s_trace;
static int32_t  s_traceTried;
static uint32_t s_traceCount;
static uint64_t s_traceHash;

static FILE *trace_file(void)
{
    if (!s_traceTried) {
        const char *path = getenv("SWOS_REPLAY_TRACE");
        s_traceTried = 1;
        if (path && path[0])
            s_trace = fopen(path, "w");
    }
    return s_trace;
}

static void trace_sprite(int16_t ordinal, int16_t x, int16_t y)
{
    /* SWOS_REPLAY_TRACE_SPRITES=1: every sprite too, for finding the one that differs. */
    static int32_t verbose = -1;
    if (verbose < 0)
        verbose = getenv("SWOS_REPLAY_TRACE_SPRITES") != NULL;
    if (verbose && s_trace)
        fprintf(s_trace, "  sprite %d %d %d\n", ordinal, x, y);
    uint64_t h = (uint64_t)(uint16_t)ordinal * 0x9E3779B97F4A7C15ull;
    h ^= (uint64_t)(uint16_t)x * 0xC2B2AE3D27D4EB4Full;
    h ^= (uint64_t)(uint16_t)y * 0x165667B19E3779F9ull;
    h ^= h >> 29;
    /* Mixed in ORDER, not summed: index 0 is drawn first, so two frames with the same
     * sprites in a different order overlap differently and must not hash alike. */
    s_traceHash = (s_traceHash * 0xBF58476D1CE4E5B9ull) ^ h;
    s_traceCount++;
}

static void trace_frame(uint32_t frame)
{
    extern int Render_ScrollSubpixel(uint16_t *fx, uint16_t *fy);
    uint16_t fx, fy;
    Render_ScrollSubpixel(&fx, &fy);
    /* "view": the pitch's draw origin and the presenter's sub-pixel shift, as published
     * by the last ScrollToCurrent -- the picture under the sprites. */
    if (trace_file())
        fprintf(s_trace, "frame %u sprites %u hash %016llx view %d %d %u %u\n", frame,
                s_traceCount, (unsigned long long)s_traceHash, Render_CameraX(), _cameraY.s.i,
                fx, fy);
    s_traceCount = 0;
    s_traceHash = 0;
}

static void trace_sound(uint32_t frame, uint8_t kind, uint8_t channel, uint16_t id)
{
    if (trace_file())
        fprintf(s_trace, "frame %u sound %u %u %u\n", frame, kind, channel, id);
}

/* The same cull the live renderer applies before it queues a sprite
 * (BuildSpriteBandAndGeneralRenderStacks), so the trace names the sprites on screen. */
static int32_t in_render_view(const SpriteGraphics *g, int16_t sx, int16_t sy)
{
    return !(sx >= _screen_width || sy >= SCREEN_HEIGHT || sx <= -g->pixWidth || sy <= -g->nlines);
}

/* ======================================================================================
 * Recording
 * ====================================================================================== */

static struct {
    const char *path;           /* --record-replay, or NULL for LATEST.SRP */
    int32_t     autoSave;

    int32_t     on;             /* recording this match */
    int32_t     open;           /* a frame is being recorded */
    int32_t     headDone;       /* its head holds the state it was drawn with */
    uint32_t    frame;          /* the frame open, or the next one */
    uint32_t    frameStart;     /* its offset in frms */
    uint16_t    sprites;        /* its sprites so far */

    uint8_t    *frms;           /* the frame stream, big-endian, as it goes in the file */
    uint32_t    frmsLen, frmsCap;
    uint32_t   *offsets;        /* each committed frame's offset in frms */
    uint32_t    frameCount, offsetsCap;

    AudioEvent *audio;
    uint32_t    audioCount, audioCap;
    Chapter    *chapters;
    uint32_t    chapterCount, chapterCap;

    uint8_t    *meta;           /* the match's identity, taken at kick-off */
} s_rec;

void ReplayRec_Configure(const char *path, int32_t autoSave)
{
    s_rec.path = (path && path[0]) ? path : NULL;
    s_rec.autoSave = autoSave;
}

int32_t ReplayRec_Recording(void)
{
    return s_rec.on;
}

static void rec_on_sound(uint8_t kind, uint8_t channel, uint16_t id)
{
    AudioEvent *e;
    if (!s_rec.on)
        return;
    if (grow((void **)&s_rec.audio, &s_rec.audioCap, s_rec.audioCount + 1, sizeof *s_rec.audio))
        return;
    e = &s_rec.audio[s_rec.audioCount++];
    e->frame = s_rec.frame;             /* the frame open, or before the first: frame 0 */
    e->kind = kind;
    e->channel = channel;
    e->id = id;
    trace_sound(s_rec.frame, kind, channel, id);
}

static uint8_t *build_meta(void);

static void rec_free(void)
{
    free(s_rec.meta);
    s_rec.meta = NULL;
    free(s_rec.frms);
    free(s_rec.offsets);
    free(s_rec.audio);
    free(s_rec.chapters);
    s_rec.frms = NULL;
    s_rec.offsets = NULL;
    s_rec.audio = NULL;
    s_rec.chapters = NULL;
    s_rec.frmsLen = s_rec.frmsCap = 0;
    s_rec.frameCount = s_rec.offsetsCap = 0;
    s_rec.audioCount = s_rec.audioCap = 0;
    s_rec.chapterCount = s_rec.chapterCap = 0;
}

void ReplayRec_MatchStart(void)
{
    ReplayRec_MatchEnd();       /* a match that never reached its end still gets written */

    /* A loaded replay is not this match's: before this, watching a .RPL and then playing a
     * friendly left the .RPL behind for the in-match highlights (H) to play. */
    ReplayV2_Unload();
    DosReplay_ClearRpl();

    /* Not training, not the arcade cabinet, and not the replay rig's corpus runs -- a
     * test that wants a recording says so (--record-replay). */
    if (_trainingGame != 0 || _gArcadeMode || (ReplayTest_Armed() && !s_rec.path))
        return;

    rec_free();
    s_rec.on = 1;
    s_rec.open = 0;
    s_rec.frame = 0;
    /* The teams, competition and PITCH as the match was set up. At the end they are not
     * the match's any more -- SetPitchTypeAndNumber runs again for what comes next, and a
     * replay written with that value played on a different pitch than the match did. */
    s_rec.meta = build_meta();
    Audio_SetSfxObserver(rec_on_sound);
}

void ReplayRec_SetFrame(uint32_t frame)
{
    uint32_t i;
    if (!s_rec.on)
        return;
    s_rec.open = 0;
    if (frame < s_rec.frameCount) {
        /* Played again: this frame and everything after it are re-recorded. */
        s_rec.frmsLen = s_rec.offsets[frame];
        s_rec.frameCount = frame;
        while (s_rec.audioCount && s_rec.audio[s_rec.audioCount - 1].frame >= frame)
            s_rec.audioCount--;
        for (i = s_rec.chapterCount; i > 0 && s_rec.chapters[i - 1].frame >= frame; i--)
            ;
        s_rec.chapterCount = i;
    }
    /* A step that played no recorded frame (the statistics overlay held one) leaves a gap:
     * fill it with the last frame, which is what the screen showed, so a frame's number
     * stays its index -- the rewind above depends on it. */
    while (s_rec.frameCount > 0 && s_rec.frameCount < frame) {
        uint32_t from = s_rec.offsets[s_rec.frameCount - 1];
        uint32_t len = s_rec.frmsLen - from;
        if (grow((void **)&s_rec.frms, &s_rec.frmsCap, s_rec.frmsLen + len, 1) ||
            grow((void **)&s_rec.offsets, &s_rec.offsetsCap, s_rec.frameCount + 1, sizeof(uint32_t)))
            break;
        memcpy(s_rec.frms + s_rec.frmsLen, s_rec.frms + from, len);
        s_rec.offsets[s_rec.frameCount++] = s_rec.frmsLen;
        s_rec.frmsLen += len;
    }
    s_rec.frame = frame;
}

void ReplayRec_FrameBegin(void)
{
    if (!s_rec.on)
        return;
    if (s_rec.open)
        s_rec.frmsLen = s_rec.frameStart;       /* a frame that never finished */
    if (grow((void **)&s_rec.frms, &s_rec.frmsCap,
             s_rec.frmsLen + FRAME_HEAD_BYTES + MAX_FRAME_SPRITES * SPRITE_BYTES, 1)) {
        s_rec.on = 0;
        Audio_SetSfxObserver(NULL);
        fprintf(stderr, "[replay] out of memory: this match will not be recorded\n");
        return;
    }
    s_rec.frameStart = s_rec.frmsLen;
    memset(s_rec.frms + s_rec.frmsLen, 0, FRAME_HEAD_BYTES);    /* filled by write_head */
    s_rec.frmsLen += FRAME_HEAD_BYTES;
    s_rec.sprites = 0;
    s_rec.open = 1;
    s_rec.headDone = 0;
}

/* The frame's state AS DRAWN, so taken with its sprites rather than at FrameBegin (where
 * the legacy recorder takes it): the camera can move in between -- the end-of-match cut
 * does -- and DrawAnimatedPatterns sets this frame's crowd state, so both would be a
 * frame stale. */
static void write_head(void)
{
    uint8_t *h = s_rec.frms + s_rec.frameStart;
    if (s_rec.headDone)
        return;
    /* The camera in full 16.16. A wide display's draw origin scales the fraction too
     * (Render_CameraX16), so the integer part alone put playback a pixel out at 484 wide
     * on some frames. */
    BE_WRITE_U32(h + 2, (uint32_t)_cameraX.raw);
    BE_WRITE_U32(h + 6, (uint32_t)_cameraY.raw);
    BE_WRITE_U16(h + 10, _team1TotalGoals);
    BE_WRITE_U16(h + 12, _team2TotalGoals);
    BE_WRITE_U16(h + 14, _animPatternsState);
    BE_WRITE_U16(h + 16, (uint16_t)(_gameTime[1] * 100 + _gameTime[2] * 10 + _gameTime[3]));
    BE_WRITE_U16(h + 18, 0);
    s_rec.headDone = 1;
}

void ReplayRec_CaptureSprites(void)
{
    uint16_t i, n;
    if (!s_rec.on || !s_rec.open)
        return;
    write_head();
    n = _numSpritesToRender;
    if (n > MAX_FRAME_SPRITES)
        n = MAX_FRAME_SPRITES;
    /* In the order they are drawn -- index 0 first, at the back -- and every one, on
     * screen or not: a wider display than this one shows more of them. */
    for (i = 0; i < n; i++) {
        const Sprite *s = _sortedSprites[i];
        const SpriteGraphics *g;
        uint8_t *p;
        if (!s || s->imageIndex < 0 || s->z.s.i >= HUD_Z_MIN)
            continue;
        g = graphics((uint16_t)s->imageIndex);
        p = s_rec.frms + s_rec.frmsLen;
        BE_WRITE_U16(p, (uint16_t)s->imageIndex);
        BE_WRITE_U16(p + 2, (uint16_t)(s->x.s.i - g->centerX));
        BE_WRITE_U16(p + 4, (uint16_t)(s->y.s.i - g->centerY - s->z.s.i));
        s_rec.frmsLen += SPRITE_BYTES;
        s_rec.sprites++;
    }
}

/* The half the last half chapter named -- read back from the chapters, so a netplay
 * rewind that dropped some cannot leave it stale. */
static uint16_t last_half(void)
{
    uint32_t i;
    for (i = s_rec.chapterCount; i > 0; i--)
        if (s_rec.chapters[i - 1].kind == CHAPTER_HALF)
            return s_rec.chapters[i - 1].value;
    return 0xFFFF;
}

static void add_chapter(uint8_t kind, uint8_t team, uint16_t value)
{
    Chapter *c;
    if (grow((void **)&s_rec.chapters, &s_rec.chapterCap, s_rec.chapterCount + 1, sizeof *c))
        return;
    c = &s_rec.chapters[s_rec.chapterCount++];
    c->frame = s_rec.frame;
    c->kind = kind;
    c->team = team;
    c->value = value;
}

void ReplayRec_FrameEnd(void)
{
    const uint8_t *h;
    if (!s_rec.on || !s_rec.open)
        return;
    if (grow((void **)&s_rec.offsets, &s_rec.offsetsCap, s_rec.frameCount + 1, sizeof(uint32_t))) {
        s_rec.frmsLen = s_rec.frameStart;
        s_rec.open = 0;
        return;
    }
    write_head();
    BE_WRITE_U16(s_rec.frms + s_rec.frameStart, s_rec.sprites);
    h = s_rec.frms + s_rec.frameStart;

    /* Chapters: the start of each half (and of extra time), and every goal. */
    if (s_rec.frameCount == 0 || _halfNumber != last_half()) {
        add_chapter(CHAPTER_HALF, 0, _halfNumber);
    }
    if (s_rec.frameCount > 0) {
        const uint8_t *prev = s_rec.frms + s_rec.offsets[s_rec.frameCount - 1];
        if (BE_READ_U16(h + 10) > BE_READ_U16(prev + 10))
            add_chapter(CHAPTER_GOAL, 1, BE_READ_U16(h + 10));
        if (BE_READ_U16(h + 12) > BE_READ_U16(prev + 12))
            add_chapter(CHAPTER_GOAL, 2, BE_READ_U16(h + 12));
    }

    s_rec.offsets[s_rec.frameCount++] = s_rec.frameStart;
    s_rec.open = 0;
    trace_frame(s_rec.frame);
    s_rec.frame++;
}

/* sprites.c: a world sprite the live match actually drew (the trace only). */
void ReplayTrace_LiveSprite(int16_t ordinal, int16_t x, int16_t y)
{
    if (s_rec.on && s_rec.open && trace_file())
        trace_sprite(ordinal, x, y);
}

/* ---- writing ----------------------------------------------------------------------------- */

typedef struct {
    uint32_t fourcc;
    uint8_t *data;              /* owned */
    uint32_t stored, raw;
} Chunk;

/* Raw deflate when it saves anything, else stored as it is. Takes ownership of `raw`
 * (never NULL). */
static void chunk_make(Chunk *c, uint32_t fourcc, uint8_t *raw, uint32_t rawLen, int32_t compress)
{
    c->fourcc = fourcc;
    c->raw = rawLen;
    c->data = raw;
    c->stored = rawLen;
    if (compress && rawLen > 256) {
        size_t cap = (size_t)mz_compressBound(rawLen);
        uint8_t *packed = (uint8_t *)malloc(cap);
        size_t n = packed ? tdefl_compress_mem_to_mem(packed, cap, raw, rawLen,
                                  tdefl_create_comp_flags_from_zip_params(
                                      MZ_BEST_COMPRESSION, -MZ_DEFAULT_WINDOW_BITS,
                                      MZ_DEFAULT_STRATEGY)) : 0;
        if (n > 0 && n < rawLen) {
            free(raw);
            c->data = packed;
            c->stored = (uint32_t)n;
        } else {
            free(packed);
        }
    }
}

static void put_team(uint8_t *out, const TeamGame *team)
{
    const uint8_t *src = (const uint8_t *)team;
    int32_t i;
    for (i = 0; i < TEAM_HDR_INT16S; i++) {
        int16_t v;
        memcpy(&v, src + 2 * i, 2);
        BE_WRITE_U16(out + 2 * i, (uint16_t)v);
    }
    memcpy(out + 2 * TEAM_HDR_INT16S, src + 2 * TEAM_HDR_INT16S, TEAM_BYTES - 2 * TEAM_HDR_INT16S);
}

static uint8_t *build_meta(void)
{
    uint8_t *m = (uint8_t *)calloc(1, META_BYTES);
    uint8_t *q = m;
    if (!m)
        return NULL;
    BE_WRITE_U16(q, 1);                                           q += 2;  /* meta version */
    BE_WRITE_U16(q, (uint16_t)_screen_width);                     q += 2;
    BE_WRITE_U16(q, (uint16_t)((_gDosGameplay ? 1 : 0) | (_gDosGraphics ? 2 : 0) |
                               (_gArcadeMode ? 4 : 0)));          q += 2;
    BE_WRITE_U16(q, 0);                                           q += 2;
    BE_WRITE_U32(q, (uint32_t)time(NULL));                        q += 4;
    snprintf((char *)q, 32, "%s", Build_Version());               q += 32;
    q += 4;     /* the final score, patched in at the end of the match */
    BE_WRITE_U16(q, _pitchNumberAndType);                         q += 2;
    BE_WRITE_U16(q, _gameMaxSubstitutes);                         q += 2;
    BE_WRITE_U16(q, 0);                                           q += 2;
    memcpy(q, _gameName, 40);                                     q += 40;
    memcpy(q, _gameRound, 40);                                    q += 40;
    put_team(q, &_topTeamInGame);                                 q += TEAM_BYTES;
    put_team(q, &_bottomTeamInGame);
    return m;
}

static uint8_t *build_audio(void)
{
    uint8_t *a = (uint8_t *)malloc((size_t)s_rec.audioCount * AUDIO_EVENT_BYTES + 1);
    uint32_t i;
    if (!a)
        return NULL;
    for (i = 0; i < s_rec.audioCount; i++) {
        uint8_t *p = a + i * AUDIO_EVENT_BYTES;
        BE_WRITE_U32(p, s_rec.audio[i].frame);
        p[4] = s_rec.audio[i].kind;
        p[5] = s_rec.audio[i].channel;
        BE_WRITE_U16(p + 6, s_rec.audio[i].id);
    }
    return a;
}

static uint8_t *build_seek(uint32_t *len)
{
    uint32_t count = (s_rec.frameCount + SEEK_INTERVAL - 1) / SEEK_INTERVAL, i, ai = 0;
    uint8_t *s = (uint8_t *)malloc(8 + (size_t)count * 12);
    if (!s)
        return NULL;
    BE_WRITE_U16(s, SEEK_INTERVAL);
    BE_WRITE_U16(s + 2, 0);
    BE_WRITE_U32(s + 4, count);
    for (i = 0; i < count; i++) {
        uint32_t frame = i * SEEK_INTERVAL;
        uint8_t *p = s + 8 + i * 12;
        while (ai < s_rec.audioCount && s_rec.audio[ai].frame < frame)
            ai++;
        BE_WRITE_U32(p, frame);
        BE_WRITE_U32(p + 4, s_rec.offsets[frame]);
        BE_WRITE_U32(p + 8, ai);
    }
    *len = 8 + count * 12;
    return s;
}

static uint8_t *build_chapters(uint32_t *len)
{
    uint8_t *c = (uint8_t *)malloc(4 + (size_t)s_rec.chapterCount * CHAPTER_BYTES);
    uint32_t i;
    if (!c)
        return NULL;
    BE_WRITE_U32(c, s_rec.chapterCount);
    for (i = 0; i < s_rec.chapterCount; i++) {
        uint8_t *p = c + 4 + i * CHAPTER_BYTES;
        BE_WRITE_U32(p, s_rec.chapters[i].frame);
        p[4] = s_rec.chapters[i].kind;
        p[5] = s_rec.chapters[i].team;
        BE_WRITE_U16(p + 6, s_rec.chapters[i].value);
    }
    *len = 4 + s_rec.chapterCount * CHAPTER_BYTES;
    return c;
}

/* A name with a directory in it is used as it is; a bare name goes to the save folder,
 * where the highlights menu lists it. */
static void file_path(const char *name, char *out, size_t cap)
{
    if (strchr(name, '/') || strchr(name, '\\'))
        snprintf(out, cap, "%s", name);
    else
        snprintf(out, cap, "%s/%s", swos_save_dir(), name);
}

static int32_t write_file(const char *name, const uint8_t *buf, uint32_t len)
{
    char path[1024];
    FILE *f;
    size_t put;
    file_path(name, path, sizeof path);
    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[replay] cannot write %s\n", path);
        return 1;
    }
    put = fwrite(buf, 1, len, f);
    fclose(f);
    if (put != len) {
        fprintf(stderr, "[replay] short write to %s\n", path);
        return 1;
    }
    fprintf(stderr, "[replay] wrote %s (%u bytes)\n", path, len);
    return 0;
}

static int32_t write_recording(void)
{
    Chunk chunks[5];
    uint32_t n = 0, total, off, i, seekLen = 0, chapLen = 0;
    uint8_t *raw[5], *file, *p;
    int32_t rc = 0;

    /* Every chunk's raw bytes first: nothing is written unless all of them exist. */
    raw[0] = s_rec.meta;                        /* taken at kick-off; handed over here */
    s_rec.meta = NULL;
    raw[1] = s_rec.frms;                        /* handed over; freed with the chunks */
    raw[2] = build_audio();
    raw[3] = build_seek(&seekLen);
    raw[4] = build_chapters(&chapLen);
    s_rec.frms = NULL;
    s_rec.frmsCap = 0;
    memset(chunks, 0, sizeof chunks);
    for (i = 0; i < 5; i++)
        if (!raw[i])
            rc = 1;
    if (rc) {
        for (i = 0; i < 5; i++)
            free(raw[i]);
        fprintf(stderr, "[replay] out of memory: the match could not be saved\n");
        return 1;
    }
    chunk_make(&chunks[n++], CHUNK_META, raw[0], META_BYTES, 0);
    chunk_make(&chunks[n++], CHUNK_FRMS, raw[1], s_rec.frmsLen, 1);
    chunk_make(&chunks[n++], CHUNK_AUDI, raw[2], s_rec.audioCount * AUDIO_EVENT_BYTES, 1);
    chunk_make(&chunks[n++], CHUNK_SEEK, raw[3], seekLen, 0);
    chunk_make(&chunks[n++], CHUNK_CHAP, raw[4], chapLen, 0);

    total = SWRP_HEADER_BYTES + n * SWRP_DIR_BYTES;
    for (i = 0; i < n; i++)
        total += chunks[i].stored;
    file = (uint8_t *)malloc(total);
    if (file) {
        memcpy(file, "SWRP", 4);
        BE_WRITE_U16(file + 4, SWRP_VERSION);
        BE_WRITE_U16(file + 6, SWRP_FLAG_AUDIO | SWRP_FLAG_SEEK | SWRP_FLAG_CHAPTERS);
        BE_WRITE_U16(file + 8, (uint16_t)DosGameplay_TargetFps());
        BE_WRITE_U16(file + 10, _gOriginalFireHoldLockout ? SWRP_QUIRK_ORIGINAL_FIRE_HOLD_LOCKOUT : 0);
        BE_WRITE_U16(file + 12, 0x2A0);
        BE_WRITE_U16(file + 14, 0x370);
        BE_WRITE_U32(file + 16, s_rec.frameCount);
        BE_WRITE_U16(file + 20, (uint16_t)n);
        BE_WRITE_U16(file + 22, 0);
        BE_WRITE_U32(file + 28, 0);
        off = SWRP_HEADER_BYTES + n * SWRP_DIR_BYTES;
        for (i = 0; i < n; i++) {
            p = file + SWRP_HEADER_BYTES + i * SWRP_DIR_BYTES;
            BE_WRITE_U32(p, chunks[i].fourcc);
            BE_WRITE_U32(p + 4, off);
            BE_WRITE_U32(p + 8, chunks[i].stored);
            BE_WRITE_U32(p + 12, chunks[i].raw);
            memcpy(file + off, chunks[i].data, chunks[i].stored);
            off += chunks[i].stored;
        }
        BE_WRITE_U32(file + 24, (uint32_t)mz_crc32(MZ_CRC32_INIT, file + SWRP_HEADER_BYTES,
                                                   total - SWRP_HEADER_BYTES));

        rc = write_file(s_rec.path ? s_rec.path : REPLAY_V2_LATEST, file, total);
        if (!rc && s_rec.autoSave && !s_rec.path) {
            /* A dated copy: MMDDHHMM.SRP, eight characters as the file menus want. */
            char name[32];
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            if (tm) {
                snprintf(name, sizeof name, "%02d%02d%02d%02d.SRP", tm->tm_mon + 1,
                         tm->tm_mday, tm->tm_hour, tm->tm_min);
                write_file(name, file, total);
            }
        }
        free(file);
    } else {
        rc = 1;
    }
    for (i = 0; i < n; i++)
        free(chunks[i].data);
    if (rc)
        fprintf(stderr, "[replay] the match could not be saved\n");
    return rc;
}

void ReplayRec_MatchEnd(void)
{
    if (!s_rec.on)
        return;
    s_rec.on = 0;
    s_rec.open = 0;
    Audio_SetSfxObserver(NULL);
    if (s_rec.meta) {
        BE_WRITE_U16(s_rec.meta + META_SCORE_OFF, _statsTeam1Goals);
        BE_WRITE_U16(s_rec.meta + META_SCORE_OFF + 2, _statsTeam2Goals);
    }
    if (s_rec.frameCount > 0) {
        fprintf(stderr, "[replay] recorded %u frames, %u sounds, %u chapters\n",
                s_rec.frameCount, s_rec.audioCount, s_rec.chapterCount);
        write_recording();
    }
    rec_free();
    if (s_trace)
        fflush(s_trace);
}

/* ======================================================================================
 * Playback
 * ====================================================================================== */

static struct {
    int32_t     loaded;
    uint16_t    tickRate, quirks, flags;
    uint32_t    frameCount;
    uint8_t    *frms;           /* inflated frame stream */
    uint32_t    frmsLen;
    uint32_t   *offsets;
    AudioEvent *audio;
    uint32_t    audioCount;
    uint32_t    cursor, audioCursor;
} s_play;

void ReplayV2_Unload(void)
{
    Audio_SetSfxObserver(NULL);         /* the playback trace's, if it was watching */
    free(s_play.frms);
    free(s_play.offsets);
    free(s_play.audio);
    memset(&s_play, 0, sizeof s_play);
}

int32_t ReplayV2_Loaded(void)   { return s_play.loaded; }
int32_t ReplayV2_HasAudio(void) { return s_play.loaded && (s_play.flags & SWRP_FLAG_AUDIO); }

static uint8_t *read_file(const char *name, uint32_t *len)
{
    char path[1024];
    FILE *f;
    long n;
    uint8_t *buf;
    file_path(name, path, sizeof path);
    f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= SWRP_HEADER_BYTES || n > 64L * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    buf = (uint8_t *)malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *len = (uint32_t)n;
    return buf;
}

/* The chunk's bytes, inflated if they were deflated. NULL if absent or broken. */
static uint8_t *chunk_get(const uint8_t *file, uint32_t fileLen, uint32_t fourcc, uint32_t *outLen)
{
    uint16_t n = BE_READ_U16(file + 20), i;
    if (SWRP_HEADER_BYTES + (uint32_t)n * SWRP_DIR_BYTES > fileLen)
        return NULL;
    for (i = 0; i < n; i++) {
        const uint8_t *d = file + SWRP_HEADER_BYTES + i * SWRP_DIR_BYTES;
        uint32_t off = BE_READ_U32(d + 4), stored = BE_READ_U32(d + 8), raw = BE_READ_U32(d + 12);
        uint8_t *out;
        if (BE_READ_U32(d) != fourcc)
            continue;
        if (off > fileLen || stored > fileLen - off || raw > 256u * 1024 * 1024)
            return NULL;
        out = (uint8_t *)malloc(raw ? raw : 1);
        if (!out)
            return NULL;
        if (stored == raw) {
            memcpy(out, file + off, raw);
        } else if (tinfl_decompress_mem_to_mem(out, raw, file + off, stored, 0) != raw) {
            free(out);
            return NULL;
        }
        *outLen = raw;
        return out;
    }
    return NULL;
}

/* Into the HIL header the highlights viewer reads (CopyTeamsAndHeader), as a .RPL load
 * leaves it: kit words big-endian, the rest of each team byte for byte. */
static void apply_meta(const uint8_t *m)
{
    const uint8_t *q = m + 2 + 2 + 2 + 2 + 4 + 32;
    _hilTeam1Goals     = BE_READ_U16(q);        q += 2;
    _hilTeam2Goals     = BE_READ_U16(q);        q += 2;
    _hilPitchNumber    = BE_READ_U16(q);        q += 2;
    _hilMaxSubstitutes = BE_READ_U16(q);        q += 2;
    q += 2;
    memcpy(_hilGameName, q, 40);                q += 40;
    memcpy(_hilGameRound, q, 40);               q += 40;
    BE_WRITE_U32(_hilFileBuffer, 0x0000000Cu);
    BE_WRITE_U32(_hilFileBuffer + 4, 192254u);
    memcpy(_hilFileBuffer + 8, q, TEAM_BYTES);
    memcpy(_hilFileBuffer + 8 + TEAM_BYTES, q + TEAM_BYTES, TEAM_BYTES);
    _hilNumGoals = 1;
}

int32_t ReplayV2_Load(const uint8_t *name)
{
    uint32_t fileLen = 0, metaLen = 0, audioLen = 0, i, off;
    uint8_t *file = read_file((const char *)name, &fileLen);
    uint8_t *meta = NULL, *audio = NULL;
    const char *why = NULL;

    ReplayV2_Unload();
    if (!file) {
        fprintf(stderr, "[replay] cannot read %s\n", (const char *)name);
        return 1;
    }
    if (memcmp(file, "SWRP", 4) != 0)
        why = "not a SWRP replay";
    else if (BE_READ_U16(file + 4) > SWRP_VERSION)
        why = "made by a newer version";
    else if (BE_READ_U32(file + 24) !=
             (uint32_t)mz_crc32(MZ_CRC32_INIT, file + SWRP_HEADER_BYTES, fileLen - SWRP_HEADER_BYTES))
        why = "damaged (checksum)";
    if (!why) {
        s_play.flags      = BE_READ_U16(file + 6);
        s_play.tickRate   = BE_READ_U16(file + 8);
        s_play.quirks     = BE_READ_U16(file + 10);
        s_play.frameCount = BE_READ_U32(file + 16);
        meta = chunk_get(file, fileLen, CHUNK_META, &metaLen);
        s_play.frms = chunk_get(file, fileLen, CHUNK_FRMS, &s_play.frmsLen);
        if (!meta || metaLen < META_BYTES || !s_play.frms || s_play.frameCount == 0)
            why = "missing or damaged chunks";
    }
    if (!why) {
        /* Index the frames, checking every one fits. */
        s_play.offsets = (uint32_t *)malloc((size_t)s_play.frameCount * sizeof(uint32_t));
        off = 0;
        for (i = 0; s_play.offsets && i < s_play.frameCount; i++) {
            uint16_t sprites;
            if (off + FRAME_HEAD_BYTES > s_play.frmsLen)
                break;
            sprites = BE_READ_U16(s_play.frms + off);
            if (sprites > MAX_FRAME_SPRITES ||
                off + FRAME_HEAD_BYTES + (uint32_t)sprites * SPRITE_BYTES > s_play.frmsLen)
                break;
            s_play.offsets[i] = off;
            off += FRAME_HEAD_BYTES + (uint32_t)sprites * SPRITE_BYTES;
        }
        if (!s_play.offsets || i != s_play.frameCount)
            why = "the frame stream is damaged";
    }
    if (!why && (s_play.flags & SWRP_FLAG_AUDIO)) {
        audio = chunk_get(file, fileLen, CHUNK_AUDI, &audioLen);
        if (!audio && audioLen != 0)
            s_play.flags &= (uint16_t)~SWRP_FLAG_AUDIO;
        s_play.audioCount = audioLen / AUDIO_EVENT_BYTES;
        s_play.audio = (AudioEvent *)malloc((size_t)s_play.audioCount * sizeof(AudioEvent) + 1);
        for (i = 0; s_play.audio && audio && i < s_play.audioCount; i++) {
            const uint8_t *p = audio + i * AUDIO_EVENT_BYTES;
            s_play.audio[i].frame = BE_READ_U32(p);
            s_play.audio[i].kind = p[4];
            s_play.audio[i].channel = p[5];
            s_play.audio[i].id = BE_READ_U16(p + 6);
        }
    }
    free(audio);

    if (why) {
        fprintf(stderr, "[replay] %s: %s\n", (const char *)name, why);
        free(meta);
        free(file);
        ReplayV2_Unload();
        return 1;
    }

    DosReplay_ClearRpl();
    apply_meta(meta);
    free(meta);
    free(file);
    _replayPlaybackFps = s_play.tickRate ? s_play.tickRate : 50;
    _replayRecordingIsWide = 0;
    _numScenesInHilFile = 1;
    s_play.loaded = 1;
    fprintf(stderr, "[replay] %s: %u frames at %u fps, %u sounds\n", (const char *)name,
            s_play.frameCount, s_play.tickRate, s_play.audioCount);
    return 0;
}

/* Every sound the game starts while a .SRP plays, traced where the recorder traces what
 * the match started: the two must agree. Watching only the events fired below would not
 * see one the VIEWER adds -- a random crowd chant over the recorded crowd, say. */
static void play_on_sound(uint8_t kind, uint8_t channel, uint16_t id)
{
    trace_sound(s_play.cursor, kind, channel, id);
}

void ReplayV2_StartScene(void)
{
    s_play.cursor = 0;
    s_play.audioCursor = 0;
    _replayState = 1;
    if (s_trace)
        Audio_SetSfxObserver(play_on_sound);
}

static void fire_sound(const AudioEvent *e)
{
    switch (e->kind) {
    case AUDIO_SFX_PRESET:
        AudioApplyPresetToChannels(e->id);
        break;
    case AUDIO_SFX_WAVE:
        AudioChannelQueueNextWaveIfEnabled(e->channel, e->id);
        break;
    case AUDIO_SFX_VOLUME:
        AudioSetChannelVolume(e->channel == 0xFF ? 0xFFFF : e->channel, e->id);
        break;
    default:
        break;
    }
}

void ReplayV2_DrawFrame(void)
{
    const uint8_t *h, *sp;
    uint16_t sprites, i;
    int16_t origin, camY;

    if (!s_play.loaded)
        return;
    if (s_play.cursor >= s_play.frameCount) {
        _replayState = 0;
        return;
    }
    h = s_play.frms + s_play.offsets[s_play.cursor];

    if (_replayState == 1) {
        /* The first frame: the camera straight to where the match had it, both buffers. */
        int16_t x = (int16_t)(BE_READ_U32(h + 2) >> 16), y = (int16_t)(BE_READ_U32(h + 6) >> 16);
        _replayState = 2;
        _savedCameraX = _cameraX.s.i;
        _savedCameraY = _cameraY.s.i;
        ResetAnimatedPatternsForBothTeams();
        SetCameraCoordinates(x, y);
        InitCameraMoveView(x, y);
        MirrorFrontbuffer();
        SwapBuffersAndCameraViewState();
        InitCameraMoveView(x, y);
        MirrorFrontbuffer();
    }

    /* The simulation camera, which this display's renderer maps onto its own width
     * exactly as a live match does. */
    {
        /* The fractions first: MoveView pans whole pixels, and its last ScrollToCurrent
         * draws the pitch at -- and publishes the presenter's sub-pixel shift from -- the
         * camera the match drew with. Set after, the pitch sat a pixel off the sprites at
         * a scaled width, and the scroll lost its sub-pixel smoothness. */
        uint32_t cx = BE_READ_U32(h + 2), cy = BE_READ_U32(h + 6);
        _cameraX.s.frac = (uint16_t)cx;
        _cameraY.s.frac = (uint16_t)cy;
        MoveView((int16_t)(cx >> 16), (int16_t)(cy >> 16));
    }
    _hilLeftTeamGoals  = BE_READ_U16(h + 10);
    _hilRightTeamGoals = BE_READ_U16(h + 12);
    _animPatternsState = BE_READ_U16(h + 14);
    DrawAnimatedPatterns();

    origin = Render_CameraX();
    camY = _cameraY.s.i;
    sprites = BE_READ_U16(h);
    sp = h + FRAME_HEAD_BYTES;
    for (i = 0; i < sprites; i++, sp += SPRITE_BYTES) {
        uint16_t image = BE_READ_U16(sp);
        int16_t sx = (int16_t)((int16_t)BE_READ_U16(sp + 2) - origin);
        int16_t sy = (int16_t)((int16_t)BE_READ_U16(sp + 4) - camY);
        const SpriteGraphics *g = graphics(image);
        if (!in_render_view(g, sx, sy))
            continue;
        DarkenRectangle(image, sx, sy);
        if (s_trace)
            trace_sprite(g->ordinal, sx, sy);
    }

    while (s_play.audioCursor < s_play.audioCount &&
           s_play.audio[s_play.audioCursor].frame <= s_play.cursor)
        fire_sound(&s_play.audio[s_play.audioCursor++]);

    if (s_trace)
        trace_frame(s_play.cursor);
    s_play.cursor++;
}

/* ======================================================================================
 * The view test (--view-replay)
 * ====================================================================================== */

static const char *s_viewPath;

int32_t ReplayV2_ArmViewTest(const char *path)
{
    if (!path || !path[0])
        return 1;
    s_viewPath = path;
    return 0;
}

int32_t ReplayV2_ViewTestArmed(void)
{
    return s_viewPath != NULL;
}

void ReplayV2_ViewTestRun(void)
{
    extern uint16_t _menuBGImageCmdStream[];
    extern uint8_t *_menuImageStreamBaseEnd;    /* a pointer: the stream's end, not an array */
    extern void     ViewHighlights(void);
    extern uint16_t _hilStarted, _teamsLoaded, _poolplyrLoaded;
    int32_t code;

    trace_file();
    /* The front end was skipped, so the menu sprites the viewer draws were never
     * extracted -- the replay rig and the live netplay test do the same. */
    ProcessImageCommandStream(_menuBGImageCmdStream, _menuImageStreamBaseEnd);
    InitTeams();
    if (ReplayV2_Load((const uint8_t *)s_viewPath) != 0)
        exit(1);
    _hilStarted = 1;
    _gameMaxSubstitutes = _hilMaxSubstitutes;
    _teamsLoaded = 0;
    _poolplyrLoaded = 0;
    ViewHighlights();
    code = s_play.cursor == s_play.frameCount ? 0 : 2;
    fprintf(stderr, "[replay] played %u of %u frames\n", s_play.cursor, s_play.frameCount);
    if (s_trace)
        fclose(s_trace);
    fflush(stderr);
    exit(code);
}

#endif /* SWOS_SDL_BUILD */
