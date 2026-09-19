/*
 * netem_core.c -- the network emulator's rules. See include/netem_core.h.
 *
 * Everything runs off ONE queue of whole packets, each with its destination and
 * a due time, and one rule in NetemCore_Pump: send what is due. Every condition
 * is then just a due time, a copy, or a refusal.
 *
 * The queue is in SEND order, not due order, and the pump scans it -- that is
 * what lets packets overtake each other. It is also bounded: a delay long enough
 * to overflow it would be modelling a link nobody could play on, and dropping
 * the oldest at that point is both honest (a real queue does the same) and
 * better than growing without limit in a test that is already misbehaving on
 * purpose.
 *
 * Moved here from src/netplay_netem.c when the matchmaking server's relay got an
 * emulator of its own, so the two cannot drift: same table, same order of
 * decisions, same generator.
 */
#include "netem_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const NetemProfile k_profiles[] = {
    /*  name        delay jitter loss  reord  ms  dup cnt  thr  ms  */
    { "lan",           2,     1,    0,     0,  0,   0, 0,    0,   0 },
    { "adsl",         30,     6,    1,     1, 15,   0, 0,    0,   0 },
    { "wifi",         20,    15,    1,     2, 20,   0, 0,    3, 120 },
    { "mobile",       70,    40,    2,     3, 40,   1, 2,    5, 250 },
    { "satellite",   300,    25,    1,     0,  0,   0, 0,    0,   0 },
    { "awful",       150,    80,    8,     8, 60,   2, 2,    8, 300 },
};

const NetemProfile *NetemCore_Profiles(int32_t *count)
{
    if (count)
        *count = (int32_t)(sizeof k_profiles / sizeof k_profiles[0]);
    return k_profiles;
}

static int32_t same_name(const char *a, const char *b)
{
    for (;; a++, b++) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return 0;
        if (!ca)
            return 1;
    }
}

const NetemProfile *NetemCore_FindProfile(const char *name)
{
    size_t i;
    if (!name || !name[0])
        return NULL;
    for (i = 0; i < sizeof k_profiles / sizeof k_profiles[0]; i++)
        if (same_name(k_profiles[i].name, name))
            return &k_profiles[i];
    return NULL;
}

void NetemCore_ConfigFromProfile(NetemConfig *c, const NetemProfile *p)
{
    memset(c, 0, sizeof *c);
    c->delayMs     = p ? p->delayMs     : 0;
    c->jitterMs    = p ? p->jitterMs    : 0;
    c->lossPct     = p ? p->lossPct     : 0;
    c->reorderPct  = p ? p->reorderPct  : 0;
    c->reorderMs   = p && p->reorderMs ? p->reorderMs : 20;
    c->dupPct      = p ? p->dupPct      : 0;
    c->dupCount    = p && p->dupCount ? p->dupCount : 2;
    c->throttlePct = p ? p->throttlePct : 0;
    c->throttleMs  = p && p->throttleMs ? p->throttleMs : 200;
    c->tamperBits  = 1;
}

static int32_t clamp_i(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void NetemCore_Clamp(NetemConfig *c)
{
    c->delayMs     = clamp_i(c->delayMs, 0, 10000);
    c->jitterMs    = clamp_i(c->jitterMs, 0, 10000);
    c->lossPct     = clamp_i(c->lossPct, 0, 100);
    c->reorderPct  = clamp_i(c->reorderPct, 0, 100);
    c->reorderMs   = clamp_i(c->reorderMs, 0, 10000);
    c->dupPct      = clamp_i(c->dupPct, 0, 100);
    c->dupCount    = clamp_i(c->dupCount, 2, NETEM_MAX_COPIES);
    c->throttlePct = clamp_i(c->throttlePct, 0, 100);
    c->throttleMs  = clamp_i(c->throttleMs, 0, 10000);
    c->tamperPct   = clamp_i(c->tamperPct, 0, 100);
    c->tamperBits  = clamp_i(c->tamperBits, 1, 64);
    /* Jitter bigger than the delay would ask for a negative one; clamp rather
     * than send into the past, which would just be "no delay" wearing a hat. */
    if (c->jitterMs > c->delayMs)
        c->jitterMs = c->delayMs;
    if (c->throttleMs <= 0)
        c->throttlePct = 0;
}

int32_t NetemCore_ConfigEnabled(const NetemConfig *c)
{
    return c->delayMs > 0 || c->lossPct > 0 || c->reorderPct > 0 ||
           c->dupPct > 0 || c->throttlePct > 0 || c->tamperPct > 0;
}

/* Append to buf, so the line names only the conditions that are on. */
static void desc_add(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    size_t n = strlen(buf);
    if (n + 2 >= cap)
        return;
    if (n) {
        buf[n++] = ',';
        buf[n++] = ' ';
        buf[n] = '\0';
    }
    va_start(ap, fmt);
    vsnprintf(buf + n, cap - n, fmt, ap);
    va_end(ap);
}

void NetemCore_Describe(const NetemConfig *c, const char *profileName, char *buf, size_t cap)
{
    if (!cap)
        return;
    buf[0] = '\0';
    if (!NetemCore_ConfigEnabled(c))
        return;
    if (profileName)
        desc_add(buf, cap, "profile %s", profileName);
    if (c->delayMs > 0)
        desc_add(buf, cap, "delay %dms +/-%d", (int)c->delayMs, (int)c->jitterMs);
    if (c->lossPct > 0)
        desc_add(buf, cap, "loss %d%%", (int)c->lossPct);
    if (c->reorderPct > 0)
        desc_add(buf, cap, "reorder %d%% by %dms", (int)c->reorderPct, (int)c->reorderMs);
    if (c->dupPct > 0)
        desc_add(buf, cap, "duplicate %d%% x%d", (int)c->dupPct, (int)c->dupCount);
    if (c->throttlePct > 0)
        desc_add(buf, cap, "throttle %d%% for %dms%s", (int)c->throttlePct, (int)c->throttleMs,
                 c->throttleDrop ? " (dropped)" : "");
    if (c->tamperPct > 0)
        desc_add(buf, cap, "tamper %d%% of %d bit%s", (int)c->tamperPct, (int)c->tamperBits,
                 c->tamperBits == 1 ? "" : "s");
}

void NetemCore_Init(NetemLink *l, const NetemConfig *cfg, uint32_t seed)
{
    memset(l, 0, sizeof *l);
    l->cfg = *cfg;
    NetemCore_Clamp(&l->cfg);
    l->enabled = NetemCore_ConfigEnabled(&l->cfg);
    l->rng = seed ? seed : 0x9E3779B9u;
}

void NetemCore_Free(NetemLink *l)
{
    free(l->queue);
    l->queue = NULL;
    l->count = 0;
}

/* xorshift32 -- the emulator's own, never the game's: the match must not be able
 * to tell whether it is being emulated. */
static uint32_t netem_rand(NetemLink *l)
{
    uint32_t x = l->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    l->rng = x ? x : 0x9E3779B9u;
    return l->rng;
}

/* True with probability pct/100. pct <= 0 never rolls the generator, so a
 * condition that is off cannot shift the sequence the others see. */
static int32_t netem_chance(NetemLink *l, int32_t pct)
{
    if (pct <= 0)
        return 0;
    return (int32_t)(netem_rand(l) % 100u) < pct;
}

/* Flip tamperBits bits at random. The whole packet is fair game, header
 * included: what the receiver does with a packet whose type or sequence number
 * is nonsense is exactly the question. */
static void netem_tamper(NetemLink *l, uint8_t *data, int32_t len)
{
    int32_t i;
    for (i = 0; i < l->cfg.tamperBits; i++) {
        uint32_t r = netem_rand(l);
        data[r % (uint32_t)len] ^= (uint8_t)(1u << ((r >> 16) & 7u));
    }
}

/* A slot at the back of the queue, evicting the oldest if it is full. */
static int32_t netem_push(NetemLink *l, uint32_t dueMs, const uint8_t *buf, int32_t len,
                          int32_t flags, const void *dst, int32_t dstLen)
{
    NetemPacket *p;

    if (!l->queue) {
        l->queue = (NetemPacket *)malloc(sizeof *l->queue * NETEM_QUEUE);
        if (!l->queue)
            return 1;
    }
    if (l->count >= NETEM_QUEUE) {
        /* Full: drop the OLDEST, which is what a real queue does under
         * pressure, and keep the newest -- the one still worth delivering. */
        memmove(&l->queue[0], &l->queue[1], sizeof l->queue[0] * (NETEM_QUEUE - 1));
        l->count--;
        l->overflowed++;
    }
    p = &l->queue[l->count++];
    p->dueMs  = dueMs;
    p->len    = len;
    p->flags  = flags;
    p->dstLen = dstLen;
    memcpy(p->dst, dst, (size_t)dstLen);
    memcpy(p->data, buf, (size_t)len);
    return 0;
}

int32_t NetemCore_Send(NetemLink *l, uint32_t nowMs,
                       const uint8_t *buf, int32_t len,
                       const void *dst, int32_t dstLen, int32_t flags,
                       NetemEmit emit, void *ctx)
{
    const NetemConfig *c = &l->cfg;
    uint8_t tampered[NETEM_MAX_PACKET];
    const uint8_t *payload = buf;
    int32_t delay, copies = 1, i;
    int32_t throttled = 0, tamper;

    if (!l->enabled)
        return 1;                       /* send it yourself */
    if (len <= 0 || len > NETEM_MAX_PACKET || dstLen <= 0 || dstLen > NETEM_MAX_DST)
        return 1;                       /* not ours to model; let it through */

    l->offered++;

    /* Throttle first: a window is the link being unavailable, and what happens
     * to a packet while it is closed does not depend on the other knobs. The
     * window is closed here rather than in the pump because nothing else can
     * open one -- a quiet link keeps an expired window until the next packet,
     * which is the same thing. */
    if (c->throttlePct > 0) {
        if (l->windowOpen && (int32_t)(nowMs - l->windowEnd) >= 0)
            l->windowOpen = 0;
        if (!l->windowOpen && netem_chance(l, c->throttlePct)) {
            l->windowOpen = 1;
            l->windowEnd  = nowMs + (uint32_t)c->throttleMs;
            l->windows++;
        }
        throttled = l->windowOpen;
    }
    if (throttled && c->throttleDrop) {
        l->throttled++;
        l->dropped++;
        return 0;                       /* the link was simply gone */
    }

    if (netem_chance(l, c->lossPct)) {
        l->dropped++;
        return 0;                       /* gone, as if it never left */
    }

    /* When it goes out. A throttled packet leaves with the rest of its window,
     * which is what makes the burst; otherwise the delay is the configured one,
     * jittered, and pushed further back again if this is the packet chosen to
     * arrive out of order. */
    delay = c->delayMs;
    if (c->jitterMs > 0)
        delay += (int32_t)(netem_rand(l) % (uint32_t)(2 * c->jitterMs + 1)) - c->jitterMs;
    if (throttled) {
        delay = (int32_t)(l->windowEnd - nowMs);
        l->throttled++;
    } else if (netem_chance(l, c->reorderPct)) {
        delay += c->reorderMs;
        l->reordered++;
    }
    if (delay < 0)
        delay = 0;

    tamper = netem_chance(l, c->tamperPct);
    if (netem_chance(l, c->dupPct))
        copies = c->dupCount;

    if (tamper) {
        memcpy(tampered, buf, (size_t)len);
        netem_tamper(l, tampered, len);
        payload = tampered;
        l->tampered++;
    }
    if (copies > 1)
        l->duplicated += (uint32_t)(copies - 1);

    if (delay <= 0) {
        if (!tamper && copies == 1) {
            l->untouched++;
            return 1;                   /* the caller's own send, right now */
        }
        /* Out now, but not the packet we were handed: send the copies
         * ourselves, in order, so a duplicate follows its original. */
        for (i = 0; i < copies; i++)
            emit(ctx, payload, len, dst, dstLen, flags);
        return 0;
    }

    for (i = 0; i < copies; i++)
        if (netem_push(l, nowMs + (uint32_t)delay, payload, len, flags, dst, dstLen) != 0)
            return 1;                   /* out of memory: better late than never */
    l->held++;
    return 0;                           /* we have it now */
}

void NetemCore_Pump(NetemLink *l, uint32_t nowMs, NetemEmit emit, void *ctx)
{
    int32_t i;

    if (!l->enabled || l->count == 0)
        return;

    for (i = 0; i < l->count; ) {
        NetemPacket *p = &l->queue[i];
        /* Signed compare: a millisecond clock wraps every ~49 days, and an
         * unsigned `now >= due` would hold every packet for the rest of the
         * session the one time it happened. */
        if ((int32_t)(nowMs - p->dueMs) >= 0) {
            emit(ctx, p->data, p->len, p->dst, p->dstLen, p->flags);
            /* Out of the middle: the queue is in SEND order, not due order,
             * because jitter and reordering mean an earlier packet can be due
             * later. Keeping send order and scanning is what lets them overtake
             * each other. */
            memmove(&l->queue[i], &l->queue[i + 1], sizeof l->queue[0] * (size_t)(l->count - i - 1));
            l->count--;
            continue;
        }
        i++;
    }
}

void NetemCore_Summary(const NetemLink *l, char *buf, size_t cap)
{
    size_t n;

    if (!cap)
        return;
    buf[0] = '\0';
    if (!l->enabled)
        return;
    n = (size_t)snprintf(buf, cap, "%u packets: %u untouched, %u delayed, %u dropped",
                         (unsigned)l->offered, (unsigned)l->untouched, (unsigned)l->held,
                         (unsigned)l->dropped);
    if (l->reordered && n < cap)
        n += (size_t)snprintf(buf + n, cap - n, ", %u reordered", (unsigned)l->reordered);
    if (l->duplicated && n < cap)
        n += (size_t)snprintf(buf + n, cap - n, ", %u duplicates sent", (unsigned)l->duplicated);
    if (l->tampered && n < cap)
        n += (size_t)snprintf(buf + n, cap - n, ", %u tampered", (unsigned)l->tampered);
    if (l->windows && n < cap)
        n += (size_t)snprintf(buf + n, cap - n, ", %u %s in %u throttle window%s",
                              (unsigned)l->throttled, l->cfg.throttleDrop ? "lost" : "burst",
                              (unsigned)l->windows, l->windows == 1 ? "" : "s");
    if (l->overflowed && n < cap)
        snprintf(buf + n, cap - n, ", %u dropped by a full queue", (unsigned)l->overflowed);
}
