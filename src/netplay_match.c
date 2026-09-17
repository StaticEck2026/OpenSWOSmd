/*
 * netplay_match.c -- a match played through the netplay frame step and GGPO.
 * See include/netplay_match.h and docs/NETPLAY.md, "Playing a match through GGPO".
 */
#ifdef SWOS_SDL_BUILD

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>        /* SDL_GetTicks, SDL_Delay */

#include "netplay_match.h"
#include "game_loop.h"
#include "match_step.h"
#include "netplay_netem.h"
#include "netplay.h"
#include "replay_test.h"
#include "sim_state.h"

extern int      Platform_Present(void);
extern void     Platform_QuiesceTimerThread(void);
extern void     _Int3TimerHandler(void);
extern uint16_t _playGame[1];

typedef enum {
    NETPLAY_MATCH_OFF, NETPLAY_MATCH_DIRECT, NETPLAY_MATCH_SYNCTEST, NETPLAY_MATCH_P2P
} NetplayMatchMode;

static const char *const k_modeName[] = { "off", "direct", "synctest", "p2p" };

#define INPUTS_SIZE         (MATCH_INPUT_SIZE * MATCH_PLAYERS)
#define MAX_PREDICTION      8           /* frames a peer may play ahead of confirmed input */
#define DEFAULT_TIMEOUT_MS  30000
#define LINGER_MS           3000        /* after the end: how long to wait for the peer's acks */

static struct {
    /* options */
    NetplayMatchMode mode;
    int32_t          checkDistance;       /* synctest */
    int32_t          player;              /* p2p: the local player, 1 or 2 */
    int32_t          port;                /* p2p: the local UDP port */
    char             peerAddress[32];     /* p2p */
    int32_t          peerPort;
    int32_t          delay;               /* p2p: input delay in frames */
    int32_t          stallMs;             /* p2p: the longest pause of a busy machine */
    int32_t          live;                /* p2p: input from this machine's pad, not a recording */
    int32_t          liveFrameLimit;      /* live: stop here (SWOS_LIVE_FRAMES), 0 = play it out */
    int32_t          timeoutMs;
    const char      *tracePath;

    /* the session */
    NetplaySession  *session;
    uint32_t         stateSize;
    int32_t          running;             /* GGPO reports the session running */
    int32_t          prepared;            /* p2p: the recording's kick-off state is applied */
    int32_t          peerGone;            /* p2p: the peer left after the end was known */
    uint32_t         frames;              /* frames played for the first time */
    uint32_t         resimulated;         /* frames GGPO played again after loading a state */
    uint32_t         saves, loads;
    uint32_t         unidentified;        /* checksums that met a pointer with no identity */
    int32_t          lastConfirmed;       /* p2p */
    uint32_t         lastProgressMs;      /* p2p: when a frame was last played or confirmed */
    int32_t          endFrame;            /* p2p: the frame the match ends at, once final; -1 */
    uint32_t         endScanFrom;         /* p2p: every frame below this is final, not over */
    uint32_t         lingerStartMs;       /* p2p: 0 until the end is known */
    uint32_t         stallRng;            /* p2p: when to pause -- never the game's generator */

    /* per frame, from its last save: the state hashes, and whether the match is over */
    uint64_t        *traceSim, *traceView, *traceObs;
    uint8_t         *over;
    uint32_t         traceLen, traceCap;
} s_np;

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

/* ---- options ---- */

static const char *after(const char *arg, const char *prefix)
{
    size_t n = strlen(prefix);
    return strncmp(arg, prefix, n) == 0 ? arg + n : NULL;
}

static int32_t parseInt(const char *v, long lo, long hi, const char *what, int32_t *out)
{
    char *end;
    long n = strtol(v, &end, 10);
    if (end == v || *end != 0 || n < lo || n > hi) {
        fprintf(stderr, "%s: expected %ld..%ld, got '%s'\n", what, lo, hi, v);
        return -1;
    }
    *out = (int32_t)n;
    return 1;
}

int32_t NetplayMatch_ParseArg(const char *arg)
{
    const char *v;

    if ((v = after(arg, "--netplay-match="))) {
        if (strcmp(v, "direct") == 0) {
            s_np.mode = NETPLAY_MATCH_DIRECT;
            return 1;
        }
        if (strcmp(v, "p2p") == 0) {
            s_np.mode = NETPLAY_MATCH_P2P;
            return 1;
        }
        if (strncmp(v, "synctest", 8) == 0 && (v[8] == 0 || v[8] == ':')) {
            s_np.checkDistance = 1;
            if (v[8] == ':' && parseInt(v + 9, 1, MAX_PREDICTION, "--netplay-match=synctest:<N>",
                                        &s_np.checkDistance) != 1)
                return -1;
            s_np.mode = NETPLAY_MATCH_SYNCTEST;
            return 1;
        }
        fprintf(stderr, "--netplay-match: expected direct, synctest[:N] or p2p, got '%s'\n", v);
        return -1;
    }
    if ((v = after(arg, "--netplay-player=")))
        return parseInt(v, 1, MATCH_PLAYERS, "--netplay-player", &s_np.player);
    if ((v = after(arg, "--netplay-port=")))
        return parseInt(v, 1, 65535, "--netplay-port", &s_np.port);
    if ((v = after(arg, "--netplay-peer="))) {
        const char *colon = strrchr(v, ':');
        size_t n = colon ? (size_t)(colon - v) : 0;
        if (n == 0 || n >= sizeof s_np.peerAddress) {
            fprintf(stderr, "--netplay-peer: expected <IPv4>:<port>, got '%s'\n", v);
            return -1;
        }
        memcpy(s_np.peerAddress, v, n);
        s_np.peerAddress[n] = 0;
        return parseInt(colon + 1, 1, 65535, "--netplay-peer's port", &s_np.peerPort);
    }
    if ((v = after(arg, "--netplay-delay=")))
        return parseInt(v, 0, MAX_PREDICTION - 1, "--netplay-delay", &s_np.delay);
    if ((v = after(arg, "--netplay-stall=")))
        return parseInt(v, 0, 1000, "--netplay-stall", &s_np.stallMs);
    if ((v = after(arg, "--netplay-timeout="))) {
        int32_t seconds;
        if (parseInt(v, 1, 3600, "--netplay-timeout", &seconds) != 1)
            return -1;
        s_np.timeoutMs = seconds * 1000;
        return 1;
    }
    if ((v = after(arg, "--netplay-trace="))) {
        s_np.tracePath = v;
        return 1;
    }
    fprintf(stderr, "unknown option %s\n", arg);
    return -1;
}

const char *NetplayMatch_ArgError(void)
{
    int32_t p2pOptions = s_np.player || s_np.port || s_np.peerPort || s_np.delay || s_np.stallMs;

    if (s_np.mode == NETPLAY_MATCH_OFF && (p2pOptions || s_np.tracePath || s_np.timeoutMs))
        return "the --netplay-* options need --netplay-match";
    if (s_np.mode != NETPLAY_MATCH_P2P && p2pOptions)
        return "--netplay-player, -port, -peer, -delay and -stall are for --netplay-match=p2p";
    if (s_np.mode == NETPLAY_MATCH_P2P && (!s_np.player || !s_np.port || !s_np.peerPort))
        return "--netplay-match=p2p needs --netplay-player=1|2, --netplay-port=<local UDP port> "
               "and --netplay-peer=<IPv4>:<port>";
    return NULL;
}

int32_t NetplayMatch_Enabled(void)
{
    return s_np.mode != NETPLAY_MATCH_OFF;
}

/* Where a live match's LOCAL input comes from. NULL = this machine's pad, which
 * is what a real match uses. A test installs its own so the peer has something
 * it cannot predict -- with both sides sending nothing, "same as last frame" is
 * always right, no prediction ever misses and no rollback ever happens, so a
 * lossy-network test proves nothing. */
static uint8_t (*s_inputSource)(void);

/* Called when a LIVE session has finished and every frame is confirmed -- the
 * one moment the state is final and two peers can be compared. A test uses it
 * to report and exit; leaving the match loop instead lands in the post-match
 * screens, which wait for input a headless peer never gives. */
static void (*s_liveFinished)(void);

void NetplayMatch_SetFinishedCallback(void (*fn)(void))
{
    s_liveFinished = fn;
}

void NetplayMatch_SetInputSource(uint8_t (*fn)(void))
{
    s_inputSource = fn;
}

int32_t NetplayMatch_IsLive(void)
{
    return s_np.mode == NETPLAY_MATCH_P2P && s_np.live;
}

int32_t NetplayMatch_IsP2P(void)
{
    return s_np.mode == NETPLAY_MATCH_P2P;
}

/* ---- the state, hashed ---- */

/* Over the portable encoding, so a peer on any platform computes the same. */
static uint64_t stateHash(uint32_t tiers)
{
    uint64_t h = SimState_Hash(tiers);
    if (SimState_HashUnidentified())
        s_np.unidentified++;
    return h;
}

/* The frame's checksum, the one GGPO-X compares with the peer's once the frame is
 * confirmed. (Synctest compares the checksums of its saves instead.) */
static uint16_t frameChecksum(void)
{
    uint64_t h = stateHash(SIM_TIER_SIM);
    return (uint16_t)(h ^ (h >> 16) ^ (h >> 32) ^ (h >> 48));
}

static void traceGrow(uint32_t frame)
{
    uint32_t cap;

    if (frame < s_np.traceCap)
        return;
    cap = s_np.traceCap ? s_np.traceCap : 65536;
    while (cap <= frame)
        cap *= 2;
    s_np.traceSim = (uint64_t *)realloc(s_np.traceSim, cap * sizeof(uint64_t));
    s_np.traceView = (uint64_t *)realloc(s_np.traceView, cap * sizeof(uint64_t));
    s_np.traceObs = (uint64_t *)realloc(s_np.traceObs, cap * sizeof(uint64_t));
    s_np.over = (uint8_t *)realloc(s_np.over, cap);
    if (!s_np.traceSim || !s_np.traceView || !s_np.traceObs || !s_np.over)
        fail("out of memory for the frame trace");
    memset(s_np.over + s_np.traceCap, 0, cap - s_np.traceCap);
    s_np.traceCap = cap;
}

/* Frame `frame` as it stands now: its hashes, and whether the match is over at it. A later
 * save of the same frame overwrites them, so once every frame is confirmed each entry is
 * the frame as finally played. */
static void traceRecord(uint32_t frame, uint64_t sim, uint64_t view)
{
    traceGrow(frame);
    s_np.traceSim[frame] = sim;
    s_np.traceView[frame] = view;
    s_np.traceObs[frame] = s_np.tracePath ? ReplayTest_ObservableHash() : 0;
    s_np.over[frame] = _playGame[0] == 0;
    if (frame >= s_np.traceLen)
        s_np.traceLen = frame + 1;
}

static void traceRecordNow(uint32_t frame)
{
    uint64_t sim = stateHash(SIM_TIER_SIM);
    traceRecord(frame, sim, stateHash(SIM_TIER_VIEW));
}

/* The rollback rig's trace format (Rollback_Finish), so the two compare byte for byte. */
static int32_t writeTrace(uint32_t frames)
{
    FILE *f;
    uint32_t i;

    if (!s_np.tracePath)
        return 0;
    f = fopen(s_np.tracePath, "w");
    if (!f) {
        fprintf(stderr, "[netplay] cannot write %s\n", s_np.tracePath);
        return 2;
    }
    for (i = 0; i < frames && i < s_np.traceLen; i++)
        fprintf(f, "%u %016llx %016llx %016llx\n", i, (unsigned long long)s_np.traceSim[i],
                (unsigned long long)s_np.traceView[i], (unsigned long long)s_np.traceObs[i]);
    fclose(f);
    fprintf(stderr, "[netplay] wrote %s (%u frames)\n", s_np.tracePath, i);
    return 0;
}

/* Synctest: each frame's state as first played. GGPO compares only checksums and aborts;
 * with these the report names what came out different. */
#define FIRST_RUN_FRAMES 16     /* more than the 8 frames a synctest goes back */

typedef struct {
    int32_t  frame;             /* -1: empty */
    int32_t  checksum;
    uint8_t *state;
} FirstRun;

static FirstRun s_firstRun[FIRST_RUN_FRAMES];

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

/* ---- the step ---- */

/* One frame: the players' inputs into the game, the frame, its ticks, its buffer swap and
 * the VBlank interrupt's work -- everything a frame does but showing it. GGPO calls it
 * again for every frame it re-simulates. `logFrame` numbers the input's log lines (-1
 * keeps them quiet); `compare` compares the frame with the recording, for a frame played
 * once and never rolled back. */
static GameFrameResult step(const uint8_t *inputs, int32_t logFrame, int32_t compare)
{
    MatchPlayerInput in[MATCH_PLAYERS];
    GameFrameResult r;
    int32_t p;

    /* Between peers a frame past the end of the match gets played: predicted, or
     * re-simulated after a correction that ended the match sooner. The match is over at
     * it, and the frame changes nothing. */
    if (s_np.mode == NETPLAY_MATCH_P2P && _playGame[0] == 0)
        return GAME_FRAME_MATCH_OVER;

    for (p = 0; p < MATCH_PLAYERS; p++)
        MatchStep_UnpackInput(&in[p], inputs + p * MATCH_INPUT_SIZE);
    MatchStep_ApplyInput(in, logFrame);
    r = GameLoop_Frame();
    if (r != GAME_FRAME_DONE && r != GAME_FRAME_HELD)
        return r;
    if (compare)
        ReplayTest_FrameCheck();
    MatchStep_AdvanceClock();
    GameLoop_EndFrame(0);
    /* The VBlank interrupt's work -- the fade ticks among it -- is part of the frame: on the
     * Amiga it runs once after every frame, so it runs once in every step, and from nowhere
     * else while a netplay match plays (NetplayMatch_Start). */
    _Int3TimerHandler();
    return GAME_FRAME_DONE;     /* a frame the statistics overlay held is a frame too */
}

/* ---- GGPO's callbacks ---- */

static int32_t cbSave(void *ctx, uint8_t **state, int32_t *len, int32_t *checksum, int32_t frame)
{
    uint8_t *buf = (uint8_t *)malloc(s_np.stateSize);
    uint64_t sim, view, both;

    (void)ctx;
    if (!buf)
        return 0;
    SimState_Save(buf, SIM_TIER_ALL);
    sim = stateHash(SIM_TIER_SIM);
    view = stateHash(SIM_TIER_VIEW);
    both = sim ^ ((view << 1) | (view >> 63));
    *state = buf;
    *len = (int32_t)s_np.stateSize;
    *checksum = (int32_t)(uint32_t)(both ^ (both >> 32));
    s_np.saves++;
    if (frame >= 0)
        traceRecord((uint32_t)frame, sim, view);
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
    r = step(inputs, -1, 0);
    if (r != GAME_FRAME_DONE && !(s_np.mode == NETPLAY_MATCH_P2P && r == GAME_FRAME_MATCH_OVER))
        fail("a re-simulated frame left the match loop (GameFrameResult %d)", (int)r);
    s_np.resimulated++;
    Netplay_AdvanceFrame(s_np.session, frameChecksum());
    return 1;
}

static void cbEvent(void *ctx, const NetplayEvent *ev)
{
    (void)ctx;
    switch (ev->code) {
    case NETPLAY_EVENT_RUNNING:
        if (!s_np.running)
            fprintf(stderr, "[netplay] synchronized with the peer\n");
        s_np.running = 1;
        break;
    case NETPLAY_EVENT_DESYNC:
        fail("GGPO reports a desync at frame %d (local checksum %04X, remote %04X)",
             ev->desyncFrame, ev->localChecksum, ev->remoteChecksum);
        break;
    case NETPLAY_EVENT_DISCONNECTED:
        if (s_np.endFrame < 0)
            fail("the peer (player %d) disconnected", ev->player);
        s_np.peerGone = 1;
        break;
    case NETPLAY_EVENT_NETWORK_ERROR:
        fail("network error %d", ev->errorCode);
        break;
    default:
        break;
    }
}

/* ---- the session ---- */

int32_t NetplayMatch_Start(void)
{
    NetplayCallbacks cb;
    int32_t rc, i;

    if (s_np.mode == NETPLAY_MATCH_OFF)
        return 0;
    /* Only the frame step may write game memory: no timer thread running the VBlank
     * interrupt behind its back (docs/ROLLBACK.md, "How the rig plays a match"). */
    Platform_QuiesceTimerThread();
    _gFrameStepInput = 1;      /* input a frame at a time (game_loop.h) */
    s_np.stateSize = SimState_Size(SIM_TIER_ALL);
    s_np.endFrame = -1;
    s_np.lastConfirmed = -1;
    if (!s_np.timeoutMs)
        s_np.timeoutMs = DEFAULT_TIMEOUT_MS;
    for (i = 0; i < FIRST_RUN_FRAMES; i++)
        s_firstRun[i].frame = -1;

    memset(&cb, 0, sizeof cb);
    cb.saveState = cbSave;
    cb.loadState = cbLoad;
    cb.freeState = cbFree;
    cb.advanceFrame = cbAdvance;
    cb.onEvent = cbEvent;
    if (s_np.mode != NETPLAY_MATCH_DIRECT && !Netplay_Available()) {
        fprintf(stderr, "[netplay] this build has no netplay (make NETPLAY=1)\n");
        return 1;
    }
    if (s_np.mode == NETPLAY_MATCH_SYNCTEST) {
        rc = Netplay_StartSyncTest(&s_np.session, MATCH_INPUT_SIZE, s_np.checkDistance, &cb);
        if (rc != NETPLAY_OK) {
            fprintf(stderr, "[netplay] starting the synctest session: %s\n", Netplay_ErrorName(rc));
            return 1;
        }
    } else if (s_np.mode == NETPLAY_MATCH_P2P) {
        NetplayConfig cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.localPort = (uint16_t)s_np.port;
        cfg.localPlayer = s_np.player;
        snprintf(cfg.remoteAddress, sizeof cfg.remoteAddress, "%s", s_np.peerAddress);
        cfg.remotePort = (uint16_t)s_np.peerPort;
        cfg.inputSize = MATCH_INPUT_SIZE;
        cfg.frameDelay = s_np.delay;
        cfg.maxPrediction = MAX_PREDICTION;
        cfg.disconnectTimeoutMs = s_np.timeoutMs;
        cfg.disconnectNotifyMs = 1000;
        s_np.stallRng = 0x9E3779B9u ^ ((uint32_t)s_np.player * 0x85EBCA6Bu);
        rc = Netplay_StartP2P(&s_np.session, &cfg, &cb);
        if (rc != NETPLAY_OK) {
            fprintf(stderr, "[netplay] starting the p2p session: %s\n", Netplay_ErrorName(rc));
            return 1;
        }
    }

    if (Netem_Enabled())
        fprintf(stderr, "[netplay] network emulation: %s\n", Netem_Describe());
    fprintf(stderr, "[netplay] %s match: %d bytes of input per player, %u bytes of state per save",
            k_modeName[s_np.mode], MATCH_INPUT_SIZE, s_np.stateSize);
    if (s_np.mode == NETPLAY_MATCH_SYNCTEST)
        fprintf(stderr, ", check distance %d", s_np.checkDistance);
    if (s_np.mode == NETPLAY_MATCH_P2P)
        fprintf(stderr, "; player %d on UDP port %d, peer %s:%d, input delay %d, pauses up to %d ms",
                s_np.player, s_np.port, s_np.peerAddress, s_np.peerPort, s_np.delay, s_np.stallMs);
    fputc('\n', stderr);
    return 0;
}

/* Start a LIVE match against the peer the matchmaking menu found (or the
 * direct-IP menu connected to). Everything the corpus p2p mode does, with this
 * machine's pad as the local input.
 *
 * `delay` MUST be the delay BOTH peers agreed (P2PLink_AgreedDelay), not one
 * this machine picked: an asymmetric delay is what makes one player feel they
 * are chasing the other, and in a close contest it decides the ball before the
 * game's own roll is reached.
 *
 * The p2p link has to be CLOSED before this is called. GGPO opens its own
 * socket, and on the same local port -- which is also what keeps a NAT mapping
 * open across the handover. */
int32_t NetplayMatch_StartLive(int32_t localPlayer, const char *peerIp, uint16_t peerPort,
                               uint16_t localPort, int32_t delay)
{
    if (localPlayer != 1 && localPlayer != 2)
        return 1;
    if (!peerIp || !peerIp[0])
        return 1;
    if (delay < 0)
        delay = 0;
    if (delay > MAX_PREDICTION - 1)
        delay = MAX_PREDICTION - 1;

    /* Both peers must start the match from the same clock.
     *
     * The tick counters have been running since each process booted, so they
     * hold whatever the wall clock had reached -- a different number on each
     * machine, and the state hash covers them, so GGPO sees a desync on frame 0
     * before a single input has been exchanged. Measured: of 355 registry
     * regions exactly these two differed between two peers.
     *
     * _lastGameTick starts one BEHIND the tick, not equal to it: ReadTimerDelta
     * turns the difference into _lastFrameTicks, and leaving them equal gives
     * frame 0 a delta of 0, which lands every timer-driven transition in the
     * match one frame late. The replay rig documents the same values and the
     * same reason (applyFirstFrameSetup in src/replay_test.c). */
    {
        extern volatile uint16_t _currentGameTick;
        extern volatile uint16_t _currentTick;
        extern uint16_t          _lastGameTick;
        _currentGameTick = 0;
        _currentTick     = 0;
        _lastGameTick    = 0xFFFF;
        MatchStep_Reset();          /* no button held, no bench call pending */
    }

    /* And from released controls. From here InputSDL_PostFrame leaves the joystick
     * and fire registers to the session, so the pre-match screens would otherwise
     * keep reading whatever was held when the match started -- including the fire
     * press on START, which hung the stadium screen's wait for a release
     * (InputSDL_ReleaseAll). */
    {
        extern void InputSDL_ReleaseAll(void);
        InputSDL_ReleaseAll();
    }

    memset(&s_np, 0, sizeof s_np);
    s_np.mode        = NETPLAY_MATCH_P2P;
    s_np.live        = 1;
    s_np.player      = localPlayer;
    s_np.port        = localPort;
    s_np.peerPort    = peerPort;
    s_np.delay       = delay;
    s_np.timeoutMs   = DEFAULT_TIMEOUT_MS;
    {
        const char *lim = getenv("SWOS_LIVE_FRAMES");
        s_np.liveFrameLimit = (lim && lim[0]) ? atoi(lim) : 0;
    }
    snprintf(s_np.peerAddress, sizeof s_np.peerAddress, "%s", peerIp);

    return NetplayMatch_Start();
}

/* direct and synctest: one process, both players' inputs, one frame per pass. */
static int32_t singleProcessFrame(void)
{
    MatchPlayerInput in[MATCH_PLAYERS];
    uint8_t local[INPUTS_SIZE], synced[INPUTS_SIZE];
    uint32_t frame;
    GameFrameResult r;
    int32_t p, rc, disconnect = 0;

    /* The frame boundary: the rollback rig's trace, and this frame's record. */
    ReplayTest_FrameStart();
    frame = ReplayTest_FrameIndex();
    ReplayTest_RecordedInput(frame, in);
    for (p = 0; p < MATCH_PLAYERS; p++)
        MatchStep_PackInput(&in[p], local + p * MATCH_INPUT_SIZE);

    if (s_np.mode == NETPLAY_MATCH_DIRECT) {
        if (frame == 0 && s_np.tracePath)
            traceRecordNow(0);
        r = step(local, (int32_t)frame, 1);
        if (r == GAME_FRAME_DONE && s_np.tracePath)
            traceRecordNow(frame + 1);
    } else {
        Netplay_Idle(s_np.session);
        for (p = 0; p < MATCH_PLAYERS; p++) {
            rc = Netplay_AddLocalInput(s_np.session, p + 1, local + p * MATCH_INPUT_SIZE);
            if (rc != NETPLAY_OK)
                fail("frame %u: adding player %d's input: %s", frame, p + 1, Netplay_ErrorName(rc));
        }
        rc = Netplay_SyncInput(s_np.session, synced, &disconnect);
        if (rc != NETPLAY_OK)
            fail("frame %u: synchronize input: %s", frame, Netplay_ErrorName(rc));
        /* Both players are local and there is no input delay: GGPO hands back what was added. */
        if (memcmp(synced, local, sizeof local) != 0)
            fail("frame %u: GGPO returned other inputs than the frame's own", frame);
        r = step(synced, (int32_t)frame, 1);
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

/* The joystick history the game keeps between frames -- the registry's "controls"
 * block (src/sim_state.c) -- back to its boot state, all zero.
 *
 * Player1StatusProc/Player2StatusProc carry a fire counter, a last-fired flag and the
 * was/press edge detectors from one call to the next, and they run in the MENUS too. A
 * player who pressed START left their counter mid-count: two peers came out of the
 * lobby with _pl1FireCounter 2 against 0, and desynced on frame 1. The directions
 * themselves are recomputed from the registers on every call; they are cleared only
 * so the whole block starts the same. The live test never met this -- it skips the
 * front end, so its controls were still at boot. */
static void resetControlHistory(void)
{
    extern uint16_t _pl1Direction[1], _pl2Direction[1];
    extern uint8_t  _pl1Fire, _pl2Fire, _pl1ShortFire, _pl2ShortFire;
    extern uint8_t  _pl1Up, _pl1Down, _pl1Left, _pl1Right, _pl2Up, _pl2Down, _pl2Left, _pl2Right;
    extern uint8_t  _pl1UpRight, _pl1UpLeft, _pl1DownLeft, _pl1DownRight;
    extern uint8_t  _pl2UpRight, _pl2UpLeft, _pl2DownLeft, _pl2DownRight;
    extern uint8_t  _pl1WasUp, _pl1WasDown, _pl1WasLeft, _pl1WasRight;
    extern uint8_t  _pl2WasUp, _pl2WasDown, _pl2WasLeft, _pl2WasRight;
    extern uint8_t  _pl1UpPress, _pl1DownPress, _pl1LeftPress, _pl1RightPress;
    extern uint8_t  _pl2UpPress, _pl2DownPress, _pl2LeftPress, _pl2RightPress;
    extern uint16_t _pl1FireCounter[1], _pl2FireCounter[1];
    extern uint8_t  _pl1LastFired, _pl2LastFired, _subsFire;

    _pl1Direction[0] = _pl2Direction[0] = 0;
    _pl1Fire = _pl2Fire = _pl1ShortFire = _pl2ShortFire = 0;
    _pl1Up = _pl1Down = _pl1Left = _pl1Right = _pl2Up = _pl2Down = _pl2Left = _pl2Right = 0;
    _pl1UpRight = _pl1UpLeft = _pl1DownLeft = _pl1DownRight = 0;
    _pl2UpRight = _pl2UpLeft = _pl2DownLeft = _pl2DownRight = 0;
    _pl1WasUp = _pl1WasDown = _pl1WasLeft = _pl1WasRight = 0;
    _pl2WasUp = _pl2WasDown = _pl2WasLeft = _pl2WasRight = 0;
    _pl1UpPress = _pl1DownPress = _pl1LeftPress = _pl1RightPress = 0;
    _pl2UpPress = _pl2DownPress = _pl2LeftPress = _pl2RightPress = 0;
    _pl1FireCounter[0] = _pl2FireCounter[0] = 0;
    _pl1LastFired = _pl2LastFired = _subsFire = 0;
}

/* p2p: a pass that plays no frame still shows one, and yields to the peer's process. */
static void waitForPeer(void)
{
    Platform_Present();
    SDL_Delay(1);
}

/* The frame the match ends at, once nothing can change it: the first frame at which the
 * match is over, or the end of the recording. Only frames that are final count -- played,
 * and with every input before them confirmed, so that no rollback saves them again. */
static void findEnd(int32_t frame, int32_t confirmed)
{
    int64_t last = (int64_t)confirmed + 1;
    uint32_t f;

    if (last > frame)
        last = frame;
    for (f = s_np.endScanFrom; (int64_t)f <= last && f < s_np.traceLen; f++) {
        if (s_np.over[f]) {
            s_np.endFrame = (int32_t)f;
            return;
        }
    }
    s_np.endScanFrom = f;
    /* A corpus match also ends when the RECORDING does. A live match has no
     * recording -- ReplayTest_FrameCount() is 0 -- so this test would declare
     * the match over at frame 0, which is exactly what it did: both peers
     * synchronized, played one frame, and stopped with "frame 0 is confirmed:
     * the recording ends there". A live match ends only when the MATCH does,
     * which is the s_np.over[] scan above. */
    if (s_np.live)
        return;
    if (last >= (int64_t)ReplayTest_FrameCount() && f > ReplayTest_FrameCount())
        s_np.endFrame = (int32_t)ReplayTest_FrameCount();
}

static void p2pFinish(void)
{
    ReplayDriverState st;

    fprintf(stderr, "[netplay] frame %d is confirmed: %s\n", s_np.endFrame,
            s_np.live ? "the session stops there"
                      : (s_np.endFrame < (int32_t)ReplayTest_FrameCount()
                             ? "the match is over there" : "the recording ends there"));
    /* A corpus match hands back to the replay rig, whose finish() reports the
     * comparison and then EXITS THE PROCESS. A live match has no rig and must
     * not exit here: the caller still has to leave the match loop, hash the
     * final state and report it. Returning GAME_FRAME_MATCH_OVER (p2pFrame)
     * ends the match instead, and the process ends on its own terms. */
    if (s_np.live) {
        if (s_liveFinished)
            s_liveFinished();
        return;
    }

    st.frame = (uint32_t)s_np.endFrame;
    ReplayTest_SetDriverState(&st);
    ReplayTest_Finish(0);
}

/* p2p: this peer's player's recorded input, GGPO's prediction of the other's, and a frame
 * whenever GGPO allows one. */
static int32_t p2pFrame(void)
{
    MatchPlayerInput in[MATCH_PLAYERS];
    uint8_t local[MATCH_INPUT_SIZE], synced[INPUTS_SIZE];
    int32_t frame = 0, confirmed = -1, rc, disconnect = 0;
    uint32_t now = SDL_GetTicks();
    GameFrameResult r;

    if (!s_np.prepared) {
        int32_t f;
        /* A LIVE match is already standing at kick-off: the menus set the teams
         * up and the match started normally, and BOTH peers did the same thing
         * because the host sent the setup. There is no recording to restore
         * from, and restoring one would throw that away. */
        if (s_np.live) {
            s_np.prepared = 1;
            s_np.lastProgressMs = now;
            resetControlHistory();      /* before GGPO saves frame 0 */
            /* And the clock, AGAIN, here. StartLive zeroed it, but the pre-match screens
             * run in between -- the stadium line-up and its waits -- and the SDL timer
             * thread keeps ticking through them by the wall clock. Two peers spend
             * different wall time there, so they reached this frame 0x1B against 0x1D
             * and desynced on frame 1 (reproduced with SWOS_PRESENT_CAPTURE slowing one
             * of them). The replay rig resets at exactly this point for the same reason
             * (applyFirstFrameSetup); these are its values, and its note says why
             * _lastGameTick starts one behind. */
            {
                extern volatile uint16_t _currentGameTick;
                extern volatile uint16_t _currentTick;
                extern uint16_t          _lastGameTick;
                extern uint16_t          _lastFrameTicks;
                _currentGameTick = 0;
                _currentTick     = 0;
                _lastGameTick    = 0xFFFF;
                /* What frame 0's ReadTimerDelta will compute from the two above. It
                 * is set now as well because GGPO saves frame 0 BEFORE that runs, and a
                 * leftover here is the one byte that differed in the kick-off dumps. */
                _lastFrameTicks  = 1;
            }
            /* SWOS_LIVE_DUMP=<dir>: the state this peer kicks off from, for
             * --netplay-state-diff. Taken HERE, at the first frame step, so it is the
             * same point however the match was reached -- the live test or the
             * lobby menu, whose pre-match screens run in between. */
            {
                const char *dir = getenv("SWOS_LIVE_DUMP");
                if (dir && dir[0]) {
                    char path[512];
                    uint32_t n = SimState_Size(SIM_TIER_ALL);
                    uint8_t *buf = (uint8_t *)malloc(n);
                    snprintf(path, sizeof path, "%s/kickoff-p%d.bin", dir, s_np.player);
                    if (buf) {
                        FILE *f;
                        SimState_Save(buf, SIM_TIER_ALL);
                        if ((f = fopen(path, "wb")) != NULL) {
                            fwrite(buf, 1, n, f);
                            fclose(f);
                            fprintf(stderr, "[netplay] kick-off state dumped to %s\n", path);
                        }
                        free(buf);
                    }
                }
            }
            goto prepared;
        }
        /* Before GGPO saves frame 0, which it does when the first input is added. */
        ReplayTest_PrepareFirstFrame();
        s_np.prepared = 1;
        s_np.lastProgressMs = now;
        /* An input delay of d plays the first d frames with no input from this player. */
        for (f = 0; f < s_np.delay; f++) {
            ReplayTest_RecordedInput((uint32_t)f, in);
            if (in[s_np.player - 1].controls || in[s_np.player - 1].key) {
                fprintf(stderr, "[netplay] frame %d of the recording carries player %d's input, "
                                "which an input delay of %d drops: this match cannot equal the "
                                "plain run\n", f, s_np.player, s_np.delay);
                break;
            }
        }
    }
prepared:

    Netplay_Idle(s_np.session);        /* the peer's inputs arrive; a misprediction rolls back */
    if (s_np.live) {
        /* Until the two sessions synchronize, no frame runs at all -- and from
         * the outside that is indistinguishable from a match that is simply
         * slow. Say which it is, once a second. */
        static uint32_t lastSay;
        if (!s_np.running && now - lastSay > 1000) {
            lastSay = now;
            printf("[live] waiting for the peer's session...\n");
            fflush(stdout);
        }
    }
    Netplay_CurrentFrame(s_np.session, &frame);
    Netplay_ConfirmedFrame(s_np.session, &confirmed);
    if (confirmed > s_np.lastConfirmed) {
        s_np.lastConfirmed = confirmed;
        s_np.lastProgressMs = now;
    }
    if (now - s_np.lastProgressMs > (uint32_t)s_np.timeoutMs)
        fail("nothing played or confirmed for %d s (%s, frame %d, confirmed %d)",
             s_np.timeoutMs / 1000, s_np.running ? "running" : "never synchronized with the peer",
             frame, confirmed);

    /* A live match normally ends when the match does. SWOS_LIVE_FRAMES=N stops
     * it at a set frame instead, so the pair test finishes in bounded time and
     * both peers report a final hash to compare -- a full match is thousands of
     * frames of wall clock. The limit is the same on both peers (same
     * environment, set by the script), so both stop at the same frame. */
    if (s_np.live && s_np.endFrame < 0 && s_np.running && s_np.liveFrameLimit > 0 &&
        (int32_t)s_np.frames >= s_np.liveFrameLimit) {
        s_np.endFrame = frame;
        printf("[live] stopping at frame %d (SWOS_LIVE_FRAMES)\n", frame);
        fflush(stdout);
    }
    if (s_np.endFrame < 0 && s_np.running)
        findEnd(frame, confirmed);
    if (s_np.endFrame >= 0) {
        /* Stay until the peer has every input of ours, which it needs to reach the same end. */
        int32_t unacked = 1;
        if (!s_np.lingerStartMs)
            s_np.lingerStartMs = now ? now : 1;
        if (Netplay_UnackedInputs(s_np.session, MATCH_PLAYERS + 1 - s_np.player, &unacked) != NETPLAY_OK)
            unacked = 1;
        if (unacked == 0 || s_np.peerGone || now - s_np.lingerStartMs > LINGER_MS) {
            p2pFinish();
            /* A live match has to END, not just stop stepping: the session is
             * over, every frame is confirmed, and the caller (InitAndPlayGame)
             * needs to return so the state can be hashed and compared. Without
             * this the game loop keeps going with no session behind it and the
             * final hash is never reached. */
            if (s_np.live)
                return GAME_FRAME_MATCH_OVER;
        }
        waitForPeer();
        return GAME_FRAME_DONE;
    }
    /* A corpus match ends when the recording runs out. A LIVE match has no
     * recording to run out of -- it ends when the match does. */
    if (!s_np.running || (!s_np.live && (uint32_t)frame >= ReplayTest_FrameCount())) {
        waitForPeer();
        return GAME_FRAME_DONE;
    }

    /* Input added now applies `delay` frames later.
     *
     * A LIVE match reads this machine's pad; a corpus match replays a recording.
     * Only the LOCAL player's half is packed either way -- the other player's
     * arrives from GGPO in Netplay_SyncInput below -- so live and recorded
     * differ in exactly one line, and everything after it is the same code the
     * corpus tests exercise. */
    if (s_np.live) {
        extern uint8_t InputSDL_LocalMatchControls(void);
        memset(in, 0, sizeof in);
        in[s_np.player - 1].controls =
            s_inputSource ? s_inputSource() : InputSDL_LocalMatchControls();
        in[s_np.player - 1].key = 0;       /* see InputSDL_LocalMatchControls */
    } else {
        ReplayTest_RecordedInput((uint32_t)(frame + s_np.delay), in);
    }
    MatchStep_PackInput(&in[s_np.player - 1], local);
    rc = Netplay_AddLocalInput(s_np.session, s_np.player, local);
    if (rc != NETPLAY_OK) {
        if (!Netplay_ShouldWait(rc))
            fail("frame %d: adding player %d's input: %s", frame, s_np.player, Netplay_ErrorName(rc));
        waitForPeer();
        return GAME_FRAME_DONE;
    }
    rc = Netplay_SyncInput(s_np.session, synced, &disconnect);
    if (rc != NETPLAY_OK)
        fail("frame %d: synchronize input: %s", frame, Netplay_ErrorName(rc));
    r = step(synced, frame, 0);
    if (r != GAME_FRAME_DONE && r != GAME_FRAME_MATCH_OVER)
        fail("frame %d left the match loop (GameFrameResult %d), which a netplay match cannot "
             "roll back", frame, (int)r);
    s_np.frames++;
    s_np.lastProgressMs = now;
    /* A live match has no recording to compare against as it goes, so it says
     * where it is often enough to tell "playing" from "stalled", and prints the
     * state hash so two peers can be compared at the SAME frame without waiting
     * for the end. */
    if (s_np.live && (s_np.frames == 1 || (s_np.frames % 100) == 0)) {
        printf("[live] frame %u  hash %016llx  (rollbacks %u)\n",
               s_np.frames, (unsigned long long)SimState_Hash(SIM_TIER_ALL),
               s_np.resimulated);
        fflush(stdout);
    }
    Netplay_AdvanceFrame(s_np.session, frameChecksum());
    Platform_Present();
    if (s_np.stallMs) {
        /* A busy machine, now and then: the other peer plays on, predicting this one's
         * input, and rolls back when the real input arrives. */
        uint32_t x = s_np.stallRng;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        s_np.stallRng = x;
        if ((x & 15) == 0)
            SDL_Delay((x >> 8) % (uint32_t)s_np.stallMs + 1);
    }
    return GAME_FRAME_DONE;
}

int32_t NetplayMatch_Frame(void)
{
    return s_np.mode == NETPLAY_MATCH_P2P ? p2pFrame() : singleProcessFrame();
}

int32_t NetplayMatch_Finish(void)
{
    int32_t code = 0, i;
    uint32_t frames = s_np.traceLen;

    switch (s_np.mode) {
    case NETPLAY_MATCH_OFF:
        return 0;
    case NETPLAY_MATCH_DIRECT:
        fprintf(stderr, "[netplay] direct: %u frames played\n", s_np.frames);
        break;
    case NETPLAY_MATCH_SYNCTEST:
        fprintf(stderr, "[netplay] synctest: %u frames played, %u re-simulated with matching "
                        "checksums (%u saves, %u loads)\n",
                s_np.frames, s_np.resimulated, s_np.saves, s_np.loads);
        /* A session that never rolled back proved nothing. */
        if (s_np.frames > (uint32_t)s_np.checkDistance && s_np.loads == 0) {
            fprintf(stderr, "[netplay] FAIL: GGPO never rolled the match back\n");
            code = 2;
        }
        break;
    case NETPLAY_MATCH_P2P:
        fprintf(stderr, "[netplay] p2p player %d: %u frames played, %u re-simulated after %u "
                        "rollbacks (%u saves)\n",
                s_np.player, s_np.frames, s_np.resimulated, s_np.loads, s_np.saves);
        frames = s_np.endFrame >= 0 ? (uint32_t)s_np.endFrame + 1 : 0;
        break;
    }
    /* What the emulated network actually did, not what it was asked for: a
     * condition configured at 2% may never have fired in a short match, and a
     * run that proved nothing should say so. */
    if (Netem_Enabled())
        fprintf(stderr, "[netplay] network emulation did: %s\n", Netem_Summary());
    if (frames)
        code |= writeTrace(frames);
    if (s_np.session) {
        Netplay_Close(s_np.session);
        s_np.session = NULL;
    }
    for (i = 0; i < FIRST_RUN_FRAMES; i++) {
        free(s_firstRun[i].state);
        s_firstRun[i].state = NULL;
    }
    if (s_np.unidentified) {
        fprintf(stderr, "[netplay] FAIL: %u checksums met a pointer with no identity\n",
                s_np.unidentified);
        code = 2;
    }
    return code;
}

#endif /* SWOS_SDL_BUILD */
