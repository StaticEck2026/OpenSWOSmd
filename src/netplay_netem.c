/*
 * netplay_netem.c -- a misbehaving network on the netplay socket. See
 * include/netplay_netem.h for the conditions it models and how to ask for them.
 *
 * Everything runs off ONE queue of whole packets, each with its destination and
 * a due time, and one rule in Netem_Pump: send what is due. Every condition is
 * then just a due time, a copy, or a refusal --
 *
 *   lag            due = now + delay
 *   jitter         due = now + delay +/- jitter, per packet
 *   out of order   due pushed past the packets sent after it
 *   throttle       due = the end of the open window, for every packet in it, so
 *                  they leave together in one burst
 *   drop / loss    no packet at all
 *   duplicate      the same packet queued more than once
 *   tamper         a copy with bits flipped in it
 *
 * The queue is in SEND order, not due order, and Netem_Pump scans it -- that is
 * what lets packets overtake each other. It is also bounded: a delay long enough
 * to overflow it would be modelling a link nobody could play on, and dropping
 * the oldest at that point is both honest (a real queue does the same) and
 * better than growing without limit in a test that is already misbehaving on
 * purpose.
 *
 * A packet that needs none of this is not touched at all: Netem_Send returns 1
 * and GGPO's own sendto runs, so "delay 0" really is no delay and not "one poll
 * later".
 */
#include "netplay_netem.h"

#ifdef SWOS_SDL_BUILD
#ifdef SWOS_NETPLAY

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <SDL.h>

#define NETEM_MAX_PACKET   1500
#define NETEM_QUEUE        256
#define NETEM_MAX_COPIES   8

typedef struct {
    uint32_t         dueMs;
    int              len;
    int              flags;
    int              dstLen;
    struct sockaddr_storage dst;
    char             data[NETEM_MAX_PACKET];
} NetemPacket;

/* A link in one word. Every field is also an env var, and the env wins. */
typedef struct {
    const char *name;
    int16_t delayMs, jitterMs, lossPct;
    int16_t reorderPct, reorderMs;
    int16_t dupPct, dupCount;
    int16_t throttlePct, throttleMs;
} NetemProfile;

static const NetemProfile k_profiles[] = {
    /*  name        delay jitter loss  reord  ms  dup cnt  thr  ms  */
    { "lan",           2,     1,    0,     0,  0,   0, 0,    0,   0 },
    { "adsl",         30,     6,    1,     1, 15,   0, 0,    0,   0 },
    { "wifi",         20,    15,    1,     2, 20,   0, 0,    3, 120 },
    { "mobile",       70,    40,    2,     3, 40,   1, 2,    5, 250 },
    { "satellite",   300,    25,    1,     0,  0,   0, 0,    0,   0 },
    { "awful",       150,    80,    8,     8, 60,   2, 2,    8, 300 },
};

static int32_t     s_ready;
static int32_t     s_enabled;
static int32_t     s_delayMs;
static int32_t     s_jitterMs;
static int32_t     s_lossPct;
static int32_t     s_reorderPct, s_reorderMs;
static int32_t     s_dupPct, s_dupCount;
static int32_t     s_throttlePct, s_throttleMs, s_throttleDrop;
static int32_t     s_tamperPct, s_tamperBits;
static uint32_t    s_rng;
static char        s_desc[320];
static char        s_summary[256];

static NetemPacket s_queue[NETEM_QUEUE];
static int32_t     s_count;
static int32_t     s_windowOpen;
static uint32_t    s_windowEnd;

/* What actually happened. Counters exist so a run can say which conditions
 * fired: "loss 8%" in a log line is what was asked for, not what occurred. */
static uint32_t    s_offered, s_untouched, s_dropped, s_held;
static uint32_t    s_reordered, s_duplicated, s_tampered;
static uint32_t    s_throttled, s_windows, s_overflowed;

/* xorshift32 -- the emulator's own, never the game's (see the header). */
static uint32_t netem_rand(void)
{
    uint32_t x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x ? x : 0x9E3779B9u;
    return s_rng;
}

/* True with probability pct/100. pct <= 0 never rolls the generator, so a
 * condition that is off cannot shift the sequence the others see. */
static int netem_chance(int32_t pct)
{
    if (pct <= 0)
        return 0;
    return (int32_t)(netem_rand() % 100u) < pct;
}

static const char *env_str(const char *name)
{
    const char *v = getenv(name);
    return (v && v[0]) ? v : NULL;
}

static int32_t env_int(const char *name, int32_t dflt)
{
    const char *v = env_str(name);
    return v ? (int32_t)atoi(v) : dflt;
}

static int32_t clamp_i(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Append to s_desc, so the line names only the conditions that are on. */
static void desc_add(const char *fmt, ...)
{
    va_list ap;
    size_t n = strlen(s_desc);
    if (n + 2 >= sizeof s_desc)
        return;
    if (n) {
        s_desc[n++] = ',';
        s_desc[n++] = ' ';
        s_desc[n] = '\0';
    }
    va_start(ap, fmt);
    vsnprintf(s_desc + n, sizeof s_desc - n, fmt, ap);
    va_end(ap);
}

void Netem_Init(void)
{
    const NetemProfile *p = NULL;
    const char *want;
    size_t i;

    if (s_ready)
        return;
    s_ready = 1;

    want = env_str("SWOS_NETEM_PROFILE");
    if (want) {
        for (i = 0; i < sizeof k_profiles / sizeof k_profiles[0]; i++)
            if (SDL_strcasecmp(k_profiles[i].name, want) == 0)
                p = &k_profiles[i];
        if (!p) {
            fprintf(stderr, "[netem] no such profile '%s' -- have:", want);
            for (i = 0; i < sizeof k_profiles / sizeof k_profiles[0]; i++)
                fprintf(stderr, " %s", k_profiles[i].name);
            fputc('\n', stderr);
        }
    }

    s_delayMs      = env_int("SWOS_NETEM_DELAY_MS",     p ? p->delayMs     : 0);
    s_jitterMs     = env_int("SWOS_NETEM_JITTER_MS",    p ? p->jitterMs    : 0);
    s_lossPct      = env_int("SWOS_NETEM_LOSS_PCT",     p ? p->lossPct     : 0);
    s_reorderPct   = env_int("SWOS_NETEM_REORDER_PCT",  p ? p->reorderPct  : 0);
    s_reorderMs    = env_int("SWOS_NETEM_REORDER_MS",   p && p->reorderMs ? p->reorderMs : 20);
    s_dupPct       = env_int("SWOS_NETEM_DUP_PCT",      p ? p->dupPct      : 0);
    s_dupCount     = env_int("SWOS_NETEM_DUP_COUNT",    p && p->dupCount ? p->dupCount : 2);
    s_throttlePct  = env_int("SWOS_NETEM_THROTTLE_PCT", p ? p->throttlePct : 0);
    s_throttleMs   = env_int("SWOS_NETEM_THROTTLE_MS",  p && p->throttleMs ? p->throttleMs : 200);
    s_throttleDrop = env_int("SWOS_NETEM_THROTTLE_DROP", 0);
    s_tamperPct    = env_int("SWOS_NETEM_TAMPER_PCT",   0);
    s_tamperBits   = env_int("SWOS_NETEM_TAMPER_BITS",  1);
    s_rng          = (uint32_t)env_int("SWOS_NETEM_SEED", 0);
    if (!s_rng)
        s_rng = (uint32_t)SDL_GetTicks() * 2654435761u + 1u;

    /* Asking for a window without saying how often one opens is the easy
     * mistake, and a window that never opens is not a test: give it a rate. */
    if (s_throttlePct <= 0 && env_str("SWOS_NETEM_THROTTLE_MS"))
        s_throttlePct = 5;

    s_delayMs      = clamp_i(s_delayMs, 0, 10000);
    s_jitterMs     = clamp_i(s_jitterMs, 0, 10000);
    s_lossPct      = clamp_i(s_lossPct, 0, 100);
    s_reorderPct   = clamp_i(s_reorderPct, 0, 100);
    s_reorderMs    = clamp_i(s_reorderMs, 0, 10000);
    s_dupPct       = clamp_i(s_dupPct, 0, 100);
    s_dupCount     = clamp_i(s_dupCount, 2, NETEM_MAX_COPIES);
    s_throttlePct  = clamp_i(s_throttlePct, 0, 100);
    s_throttleMs   = clamp_i(s_throttleMs, 0, 10000);
    s_tamperPct    = clamp_i(s_tamperPct, 0, 100);
    s_tamperBits   = clamp_i(s_tamperBits, 1, 64);
    /* Jitter bigger than the delay would ask for a negative one; clamp rather
     * than send into the past, which would just be "no delay" wearing a hat. */
    if (s_jitterMs > s_delayMs)
        s_jitterMs = s_delayMs;
    if (s_throttleMs <= 0)
        s_throttlePct = 0;

    s_enabled = s_delayMs > 0 || s_lossPct > 0 || s_reorderPct > 0 ||
                s_dupPct > 0 || s_throttlePct > 0 || s_tamperPct > 0;

    s_desc[0] = '\0';
    if (!s_enabled)
        return;
    if (p)
        desc_add("profile %s", p->name);
    if (s_delayMs > 0)
        desc_add("delay %dms +/-%d", s_delayMs, s_jitterMs);
    if (s_lossPct > 0)
        desc_add("loss %d%%", s_lossPct);
    if (s_reorderPct > 0)
        desc_add("reorder %d%% by %dms", s_reorderPct, s_reorderMs);
    if (s_dupPct > 0)
        desc_add("duplicate %d%% x%d", s_dupPct, s_dupCount);
    if (s_throttlePct > 0)
        desc_add("throttle %d%% for %dms%s", s_throttlePct, s_throttleMs,
                 s_throttleDrop ? " (dropped)" : "");
    if (s_tamperPct > 0)
        desc_add("tamper %d%% of %d bit%s", s_tamperPct, s_tamperBits,
                 s_tamperBits == 1 ? "" : "s");
}

int Netem_Enabled(void)
{
    Netem_Init();
    return s_enabled;
}

const char *Netem_Describe(void)
{
    Netem_Init();
    return s_desc;
}

const char *Netem_Summary(void)
{
    size_t n;

    Netem_Init();
    if (!s_enabled)
        return "";
    n = (size_t)snprintf(s_summary, sizeof s_summary,
                         "%u packets: %u untouched, %u delayed, %u dropped",
                         s_offered, s_untouched, s_held, s_dropped);
    if (s_reordered && n < sizeof s_summary)
        n += (size_t)snprintf(s_summary + n, sizeof s_summary - n, ", %u reordered", s_reordered);
    if (s_duplicated && n < sizeof s_summary)
        n += (size_t)snprintf(s_summary + n, sizeof s_summary - n, ", %u duplicates sent", s_duplicated);
    if (s_tampered && n < sizeof s_summary)
        n += (size_t)snprintf(s_summary + n, sizeof s_summary - n, ", %u tampered", s_tampered);
    if (s_windows && n < sizeof s_summary)
        n += (size_t)snprintf(s_summary + n, sizeof s_summary - n, ", %u %s in %u throttle window%s",
                              s_throttled, s_throttleDrop ? "lost" : "burst", s_windows,
                              s_windows == 1 ? "" : "s");
    if (s_overflowed && n < sizeof s_summary)
        snprintf(s_summary + n, sizeof s_summary - n, ", %u dropped by a full queue", s_overflowed);
    return s_summary;
}

/* Flip s_tamperBits bits at random. The whole packet is fair game, header
 * included: what the receiver does with a packet whose type or sequence number
 * is nonsense is exactly the question. */
static void netem_tamper(char *data, int len)
{
    int32_t i;
    for (i = 0; i < s_tamperBits; i++) {
        uint32_t r = netem_rand();
        data[r % (uint32_t)len] ^= (char)(1u << ((r >> 16) & 7u));
    }
}

/* A slot at the back of the queue, evicting the oldest if it is full. */
static NetemPacket *netem_push(uint32_t dueMs, const char *buf, int len, int flags,
                               const struct sockaddr *dst, int dstLen)
{
    NetemPacket *p;

    if (s_count >= NETEM_QUEUE) {
        /* Full: drop the OLDEST, which is what a real queue does under
         * pressure, and keep the newest -- the one still worth delivering. */
        memmove(&s_queue[0], &s_queue[1], sizeof s_queue[0] * (NETEM_QUEUE - 1));
        s_count--;
        s_overflowed++;
    }
    p = &s_queue[s_count++];
    p->dueMs  = dueMs;
    p->len    = len;
    p->flags  = flags;
    p->dstLen = dstLen;
    memcpy(&p->dst, dst, (size_t)dstLen);
    memcpy(p->data, buf, (size_t)len);
    return p;
}

int Netem_Send(int sock, const char *buf, int len, int flags,
               const struct sockaddr *dst, int dstLen)
{
    char tampered[NETEM_MAX_PACKET];
    const char *payload = buf;
    uint32_t now;
    int32_t delay, copies = 1, i;
    int throttled = 0, tamper;

    Netem_Init();
    if (!s_enabled)
        return 1;                       /* send it yourself */

    if (len <= 0 || len > NETEM_MAX_PACKET || (size_t)dstLen > sizeof(struct sockaddr_storage))
        return 1;                       /* not ours to model; let it through */

    now = SDL_GetTicks();
    s_offered++;

    /* Throttle first: a window is the link being unavailable, and what happens
     * to a packet while it is closed does not depend on the other knobs. The
     * window is closed here rather than in the pump because nothing else can
     * open one -- a quiet link keeps an expired window until the next packet,
     * which is the same thing. */
    if (s_throttlePct > 0) {
        if (s_windowOpen && (int32_t)(now - s_windowEnd) >= 0)
            s_windowOpen = 0;
        if (!s_windowOpen && netem_chance(s_throttlePct)) {
            s_windowOpen = 1;
            s_windowEnd  = now + (uint32_t)s_throttleMs;
            s_windows++;
        }
        throttled = s_windowOpen;
    }
    if (throttled && s_throttleDrop) {
        s_throttled++;
        s_dropped++;
        return 0;                       /* the link was simply gone */
    }

    if (netem_chance(s_lossPct)) {
        s_dropped++;
        return 0;                       /* gone, as if it never left */
    }

    /* When it goes out. A throttled packet leaves with the rest of its window,
     * which is what makes the burst; otherwise the delay is the configured one,
     * jittered, and pushed further back again if this is the packet chosen to
     * arrive out of order. */
    delay = s_delayMs;
    if (s_jitterMs > 0)
        delay += (int32_t)(netem_rand() % (uint32_t)(2 * s_jitterMs + 1)) - s_jitterMs;
    if (throttled) {
        delay = (int32_t)(s_windowEnd - now);
        s_throttled++;
    } else if (netem_chance(s_reorderPct)) {
        delay += s_reorderMs;
        s_reordered++;
    }
    if (delay < 0)
        delay = 0;

    tamper = netem_chance(s_tamperPct);
    if (netem_chance(s_dupPct))
        copies = s_dupCount;

    if (tamper) {
        memcpy(tampered, buf, (size_t)len);
        netem_tamper(tampered, len);
        payload = tampered;
        s_tampered++;
    }
    if (copies > 1)
        s_duplicated += (uint32_t)(copies - 1);

    if (delay <= 0) {
        if (!tamper && copies == 1) {
            s_untouched++;
            return 1;                   /* GGPO's own send, right now */
        }
        /* Out now, but not the packet GGPO handed us: send the copies
         * ourselves, in order, so a duplicate follows its original. */
        for (i = 0; i < copies; i++)
            sendto(sock, payload, (size_t)len, flags, dst, (socklen_t)dstLen);
        return 0;
    }

    for (i = 0; i < copies; i++)
        netem_push(now + (uint32_t)delay, payload, len, flags, dst, dstLen);
    s_held++;
    return 0;                           /* we have it now */
}

void Netem_Pump(int sock)
{
    uint32_t now;
    int32_t i;

    Netem_Init();
    if (!s_enabled || s_count == 0)
        return;

    now = SDL_GetTicks();
    for (i = 0; i < s_count; ) {
        NetemPacket *p = &s_queue[i];
        /* Signed compare: SDL_GetTicks wraps every ~49 days, and an unsigned
         * `now >= due` would hold every packet for the rest of the session the
         * one time it happened. */
        if ((int32_t)(now - p->dueMs) >= 0) {
            sendto(sock, p->data, (size_t)p->len, p->flags,
                   (const struct sockaddr *)&p->dst, (socklen_t)p->dstLen);
            /* Out of the middle: the queue is in SEND order, not due order,
             * because jitter and reordering mean an earlier packet can be due
             * later. Keeping send order and scanning is what lets them overtake
             * each other. */
            memmove(&s_queue[i], &s_queue[i + 1], sizeof s_queue[0] * (size_t)(s_count - i - 1));
            s_count--;
            continue;
        }
        i++;
    }
}

#else  /* !SWOS_NETPLAY */

void Netem_Init(void) {}
int  Netem_Enabled(void) { return 0; }
const char *Netem_Describe(void) { return ""; }
const char *Netem_Summary(void) { return ""; }
int  Netem_Send(int sock, const char *buf, int len, int flags,
                const struct sockaddr *dst, int dstLen)
{
    (void)sock; (void)buf; (void)len; (void)flags; (void)dst; (void)dstLen;
    return 1;
}
void Netem_Pump(int sock) { (void)sock; }

#endif /* SWOS_NETPLAY */
#endif /* SWOS_SDL_BUILD */
