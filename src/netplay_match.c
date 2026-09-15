/*
 * netplay_match.c -- a match played through the netplay frame step and GGPO.
 * See include/netplay_match.h and docs/NETPLAY.md, "Playing a match through GGPO".
 */
#ifdef SWOS_SDL_BUILD

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netplay_match.h"
#include "game_loop.h"
#include "match_step.h"
#include "netplay.h"
#include "replay_test.h"
#include "sim_state.h"

extern int  Platform_Present(void);
extern void Platform_QuiesceTimerThread(void);
extern void _Int3TimerHandler(void);

typedef enum { NETPLAY_MATCH_OFF, NETPLAY_MATCH_DIRECT, NETPLAY_MATCH_SYNCTEST } NetplayMatchMode;

static const char *const k_modeName[] = { "off", "direct", "synctest" };

#define INPUTS_SIZE (MATCH_INPUT_SIZE * MATCH_PLAYERS)

static struct {
    NetplayMatchMode mode;
    int32_t          checkDistance;
    NetplaySession  *session;
    uint32_t         stateSize;
    uint32_t         frames;          /* frames played */
    uint32_t         resimulated;     /* frames GGPO played again after loading a state */
    uint32_t         saves, loads;
    uint32_t         unidentified;    /* checksums that met a pointer with no identity */
} s_np;

/* Synctest: each frame's state as first played. GGPO compares only checksums and aborts;
 * with these the report names what came out different. */
#define FIRST_RUN_FRAMES 16     /* more than the 8 frames a synctest goes back */

typedef struct {
    int32_t  frame;             /* -1: empty */
    int32_t  checksum;
    uint8_t *state;
} FirstRun;

static FirstRun s_firstRun[FIRST_RUN_FRAMES];

static void fail(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "[netplay] FAIL: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
    exit(2);
}

int32_t NetplayMatch_ParseArg(const char *arg)
{
    static const char prefix[] = "--netplay-match=";
    const char *v;

    if (strncmp(arg, prefix, sizeof prefix - 1) != 0) {
        fprintf(stderr, "--netplay-match: expected --netplay-match=direct|synctest[:N]\n");
        return -1;
    }
    v = arg + sizeof prefix - 1;
    if (strcmp(v, "direct") == 0) {
        s_np.mode = NETPLAY_MATCH_DIRECT;
        return 1;
    }
    if (strncmp(v, "synctest", 8) == 0 && (v[8] == 0 || v[8] == ':')) {
        long n = 1;
        if (v[8] == ':') {
            char *end;
            n = strtol(v + 9, &end, 10);
            if (end == v + 9 || *end != 0 || n < 1 || n > 8) {
                fprintf(stderr, "--netplay-match=synctest:<N>: N is 1..8, the frames GGPO keeps\n");
                return -1;
            }
        }
        s_np.mode = NETPLAY_MATCH_SYNCTEST;
        s_np.checkDistance = (int32_t)n;
        return 1;
    }
    fprintf(stderr, "--netplay-match: expected direct or synctest[:N], got '%s'\n", v);
    return -1;
}

int32_t NetplayMatch_Enabled(void)
{
    return s_np.mode != NETPLAY_MATCH_OFF;
}

/* Over the portable encoding, so a peer on any platform computes the same. */
static uint64_t stateHash(uint32_t tiers)
{
    uint64_t h = SimState_Hash(tiers);
    if (SimState_HashUnidentified())
        s_np.unidentified++;
    return h;
}

/* The frame's checksum, the one GGPO compares with a peer's. (Synctest compares the
 * checksums of its saves instead.) */
static uint16_t frameChecksum(void)
{
    uint64_t h = stateHash(SIM_TIER_SIM);
    return (uint16_t)(h ^ (h >> 16) ^ (h >> 32) ^ (h >> 48));
}

/* One frame: the players' inputs into the game, the frame, its ticks, its buffer swap and
 * the VBlank interrupt's work -- everything a frame does but showing it. GGPO calls it
 * again for every frame it re-simulates; `first` marks the frame as first played, the one
 * the replay compares. */
static GameFrameResult step(const uint8_t *inputs, int32_t first)
{
    MatchPlayerInput in[MATCH_PLAYERS];
    GameFrameResult r;
    int32_t p;

    for (p = 0; p < MATCH_PLAYERS; p++)
        MatchStep_UnpackInput(&in[p], inputs + p * MATCH_INPUT_SIZE);
    MatchStep_ApplyInput(in, first ? (int32_t)ReplayTest_FrameIndex() : -1);
    r = GameLoop_Frame();
    if (r != GAME_FRAME_DONE)
        return r;
    if (first)
        ReplayTest_FrameCheck();
    MatchStep_AdvanceClock();
    GameLoop_EndFrame(0);
    /* The VBlank interrupt's work -- the fade ticks among it -- is part of the frame: on the
     * Amiga it runs once after every frame, so it runs once in every step, and from nowhere
     * else while a netplay match plays (NetplayMatch_Start). */
    _Int3TimerHandler();
    return r;
}

static void reportDifference(int32_t frame, const uint8_t *first, const uint8_t *again)
{
    uint32_t offset = 0, differing = 0;
    int32_t i;

    for (i = 0; i < g_simStateRegionCount; i++) {
        const SimStateRegion *reg = &g_simStateRegions[i];
        if (!SimState_RegionActive(i, SIM_TIER_ALL))
            continue;
        if (memcmp(first + offset, again + offset, reg->size) != 0) {
            uint32_t b = 0;
            while (first[offset + b] == again[offset + b])
                b++;
            if (differing++ < 12)
                fprintf(stderr, "[netplay]   %-36s byte %u of %u: first played %02X, re-simulated %02X\n",
                        reg->name, b, reg->size, first[offset + b], again[offset + b]);
        }
        offset += reg->size;
    }
    fail("frame %d, played again from a loaded state, came out different: %u registry "
         "entries differ", frame, differing);
}

static void checkFirstRun(int32_t frame, int32_t checksum, const uint8_t *state)
{
    FirstRun *fr = &s_firstRun[(uint32_t)frame % FIRST_RUN_FRAMES];

    if (fr->frame != frame) {
        if (!fr->state && !(fr->state = (uint8_t *)malloc(s_np.stateSize)))
            fail("out of memory");
        memcpy(fr->state, state, s_np.stateSize);
        fr->frame = frame;
        fr->checksum = checksum;
    } else if (checksum != fr->checksum) {
        reportDifference(frame, fr->state, state);
    }
}

/* ---- GGPO's callbacks ---- */

static int32_t cbSave(void *ctx, uint8_t **state, int32_t *len, int32_t *checksum, int32_t frame)
{
    uint8_t *buf = (uint8_t *)malloc(s_np.stateSize);
    uint64_t h;

    (void)ctx;
    if (!buf)
        return 0;
    SimState_Save(buf, SIM_TIER_ALL);
    h = stateHash(SIM_TIER_ALL);
    *state = buf;
    *len = (int32_t)s_np.stateSize;
    *checksum = (int32_t)(uint32_t)(h ^ (h >> 32));
    s_np.saves++;
    if (s_np.mode == NETPLAY_MATCH_SYNCTEST)
        checkFirstRun(frame, *checksum, buf);
    return 1;
}

static int32_t cbLoad(void *ctx, const uint8_t *state, int32_t len, int32_t framesToRollback)
{
    (void)ctx;
    (void)framesToRollback;
    if ((uint32_t)len != s_np.stateSize)
        return 0;
    SimState_Load(state, SIM_TIER_ALL);
    s_np.loads++;
    return 1;
}

static void cbFree(void *ctx, uint8_t *state)
{
    (void)ctx;
    free(state);
}

/* A frame GGPO re-simulates, after loading an earlier state; it supplies the inputs. */
static int32_t cbAdvance(void *ctx)
{
    uint8_t inputs[INPUTS_SIZE];
    int32_t disconnect = 0, rc;
    GameFrameResult r;

    (void)ctx;
    rc = Netplay_SyncInput(s_np.session, inputs, &disconnect);
    if (rc != NETPLAY_OK)
        fail("re-simulating a frame: synchronize input: %s", Netplay_ErrorName(rc));
    r = step(inputs, 0);
    if (r != GAME_FRAME_DONE)
        fail("a re-simulated frame left the match loop (GameFrameResult %d), which it did "
             "not do when first played", (int)r);
    s_np.resimulated++;
    Netplay_AdvanceFrame(s_np.session, frameChecksum());
    return 1;
}

/* ---- the session ---- */

int32_t NetplayMatch_Start(void)
{
    NetplayCallbacks cb;
    int32_t rc;

    if (s_np.mode == NETPLAY_MATCH_OFF)
        return 0;
    /* Only the frame step may write game memory: no timer thread running the VBlank
     * interrupt behind its back (docs/ROLLBACK.md, "How the rig plays a match"). */
    Platform_QuiesceTimerThread();
    s_np.stateSize = SimState_Size(SIM_TIER_ALL);
    for (int32_t i = 0; i < FIRST_RUN_FRAMES; i++)
        s_firstRun[i].frame = -1;
    if (s_np.mode == NETPLAY_MATCH_SYNCTEST) {
        if (!Netplay_Available()) {
            fprintf(stderr, "[netplay] this build has no netplay (make NETPLAY=1)\n");
            return 1;
        }
        memset(&cb, 0, sizeof cb);
        cb.saveState = cbSave;
        cb.loadState = cbLoad;
        cb.freeState = cbFree;
        cb.advanceFrame = cbAdvance;
        rc = Netplay_StartSyncTest(&s_np.session, MATCH_INPUT_SIZE, s_np.checkDistance, &cb);
        if (rc != NETPLAY_OK) {
            fprintf(stderr, "[netplay] starting the synctest session: %s\n", Netplay_ErrorName(rc));
            return 1;
        }
    }
    fprintf(stderr, "[netplay] %s match: %d bytes of input per player, %u bytes of state per save",
            k_modeName[s_np.mode], MATCH_INPUT_SIZE, s_np.stateSize);
    if (s_np.mode == NETPLAY_MATCH_SYNCTEST)
        fprintf(stderr, ", check distance %d", s_np.checkDistance);
    fputc('\n', stderr);
    return 0;
}

int32_t NetplayMatch_Frame(void)
{
    MatchPlayerInput in[MATCH_PLAYERS];
    uint8_t local[INPUTS_SIZE], synced[INPUTS_SIZE];
    GameFrameResult r;
    int32_t p, rc, disconnect = 0;

    /* The frame boundary: the rollback rig's trace, and this frame's record. */
    ReplayTest_FrameStart();
    ReplayTest_FrameInput(in);
    for (p = 0; p < MATCH_PLAYERS; p++)
        MatchStep_PackInput(&in[p], local + p * MATCH_INPUT_SIZE);

    if (s_np.mode == NETPLAY_MATCH_DIRECT) {
        r = step(local, 1);
    } else {
        Netplay_Idle(s_np.session);
        for (p = 0; p < MATCH_PLAYERS; p++) {
            rc = Netplay_AddLocalInput(s_np.session, p + 1, local + p * MATCH_INPUT_SIZE);
            if (rc != NETPLAY_OK)
                fail("frame %u: adding player %d's input: %s", ReplayTest_FrameIndex(), p + 1,
                     Netplay_ErrorName(rc));
        }
        rc = Netplay_SyncInput(s_np.session, synced, &disconnect);
        if (rc != NETPLAY_OK)
            fail("frame %u: synchronize input: %s", ReplayTest_FrameIndex(), Netplay_ErrorName(rc));
        /* Both players are local and there is no input delay: GGPO hands back what was added. */
        if (memcmp(synced, local, sizeof local) != 0)
            fail("frame %u: GGPO returned other inputs than the frame's own", ReplayTest_FrameIndex());
        r = step(synced, 1);
        /* Where synctest rolls back and re-simulates, every checkDistance frames. */
        if (r == GAME_FRAME_DONE)
            Netplay_AdvanceFrame(s_np.session, frameChecksum());
    }

    if (r == GAME_FRAME_DONE) {
        s_np.frames++;
        ReplayTest_FrameDone();
        Platform_Present();
    } else if (r == GAME_FRAME_DETOUR) {
        /* The highlights and instant-replay loops run frames of their own, which nothing
         * can roll back. Ended like any other frame, it is reported at the next frame
         * boundary, as the replay does without netplay. */
        GameLoop_EndFrame(1);
    }
    return r;
}

int32_t NetplayMatch_Finish(void)
{
    int32_t code = 0;

    if (s_np.mode == NETPLAY_MATCH_OFF)
        return 0;
    if (s_np.mode == NETPLAY_MATCH_SYNCTEST) {
        fprintf(stderr, "[netplay] synctest: %u frames played, %u re-simulated with matching "
                        "checksums (%u saves, %u loads)\n",
                s_np.frames, s_np.resimulated, s_np.saves, s_np.loads);
        /* A session that never rolled back proved nothing. */
        if (s_np.frames > (uint32_t)s_np.checkDistance && s_np.loads == 0) {
            fprintf(stderr, "[netplay] FAIL: GGPO never rolled the match back\n");
            code = 2;
        }
        Netplay_Close(s_np.session);
        s_np.session = NULL;
        for (int32_t i = 0; i < FIRST_RUN_FRAMES; i++) {
            free(s_firstRun[i].state);
            s_firstRun[i].state = NULL;
        }
    } else {
        fprintf(stderr, "[netplay] direct: %u frames played\n", s_np.frames);
    }
    if (s_np.unidentified) {
        fprintf(stderr, "[netplay] FAIL: %u checksums met a pointer with no identity\n",
                s_np.unidentified);
        code = 2;
    }
    return code;
}

#endif /* SWOS_SDL_BUILD */
