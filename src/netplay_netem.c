/*
 * netplay_netem.c -- lag and packet loss on the netplay socket. See
 * include/netplay_netem.h for what it is for and how it is configured.
 *
 * A held packet is kept whole, with its destination, until its due time and
 * then sent. Jitter is applied per packet, which is what makes packets arrive
 * OUT OF ORDER -- the case that actually finds bugs, and the one a fixed delay
 * never produces.
 *
 * The queue is bounded. A delay long enough to overflow it would be modelling a
 * link nobody could play on, and dropping the oldest at that point is both
 * honest (a real queue does the same) and better than growing without limit in
 * a test that is already misbehaving on purpose.
 */
#include "netplay_netem.h"

#ifdef SWOS_SDL_BUILD
#ifdef SWOS_NETPLAY

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <SDL.h>

#define NETEM_MAX_PACKET   1500
#define NETEM_QUEUE        256

typedef struct {
    uint32_t         dueMs;
    int              len;
    int              flags;
    int              dstLen;
    struct sockaddr_storage dst;
    char             data[NETEM_MAX_PACKET];
} NetemPacket;

static int32_t     s_ready;
static int32_t     s_enabled;
static int32_t     s_delayMs;
static int32_t     s_jitterMs;
static int32_t     s_lossPct;
static uint32_t    s_rng;
static char        s_desc[64];

static NetemPacket s_queue[NETEM_QUEUE];
static int32_t     s_count;
static uint32_t    s_dropped, s_held, s_overflowed;

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

static int32_t env_int(const char *name, int32_t dflt)
{
    const char *v = getenv(name);
    if (!v || !v[0])
        return dflt;
    return atoi(v);
}

void Netem_Init(void)
{
    if (s_ready)
        return;
    s_ready = 1;

    s_delayMs  = env_int("SWOS_NETEM_DELAY_MS", 0);
    s_jitterMs = env_int("SWOS_NETEM_JITTER_MS", 0);
    s_lossPct  = env_int("SWOS_NETEM_LOSS_PCT", 0);
    s_rng      = (uint32_t)env_int("SWOS_NETEM_SEED", 0);
    if (!s_rng)
        s_rng = (uint32_t)SDL_GetTicks() * 2654435761u + 1u;

    if (s_delayMs < 0)  s_delayMs = 0;
    if (s_jitterMs < 0) s_jitterMs = 0;
    if (s_lossPct < 0)  s_lossPct = 0;
    if (s_lossPct > 100) s_lossPct = 100;
    /* Jitter bigger than the delay would ask for a negative one; clamp rather
     * than send into the past, which would just be "no delay" wearing a hat. */
    if (s_jitterMs > s_delayMs)
        s_jitterMs = s_delayMs;

    s_enabled = (s_delayMs > 0 || s_lossPct > 0);
    if (s_enabled)
        snprintf(s_desc, sizeof s_desc, "delay %dms +/-%d, loss %d%%",
                 s_delayMs, s_jitterMs, s_lossPct);
    else
        s_desc[0] = '\0';
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

int Netem_Send(int sock, const char *buf, int len, int flags,
               const struct sockaddr *dst, int dstLen)
{
    uint32_t now;
    int32_t delay;

    (void)sock;
    Netem_Init();
    if (!s_enabled)
        return 1;                       /* send it yourself */

    if (len <= 0 || len > NETEM_MAX_PACKET || (size_t)dstLen > sizeof(struct sockaddr_storage))
        return 1;                       /* not ours to model; let it through */

    if (s_lossPct > 0 && (int32_t)(netem_rand() % 100u) < s_lossPct) {
        s_dropped++;
        return 0;                       /* gone, as if it never left */
    }

    if (s_delayMs <= 0)
        return 1;

    if (s_count >= NETEM_QUEUE) {
        /* Full: drop the OLDEST, which is what a real queue does under
         * pressure, and keep the newest -- the one still worth delivering. */
        memmove(&s_queue[0], &s_queue[1], sizeof s_queue[0] * (NETEM_QUEUE - 1));
        s_count--;
        s_overflowed++;
    }

    now = SDL_GetTicks();
    delay = s_delayMs;
    if (s_jitterMs > 0)
        delay += (int32_t)(netem_rand() % (uint32_t)(2 * s_jitterMs + 1)) - s_jitterMs;
    if (delay < 0)
        delay = 0;

    {
        NetemPacket *p = &s_queue[s_count++];
        p->dueMs  = now + (uint32_t)delay;
        p->len    = len;
        p->flags  = flags;
        p->dstLen = dstLen;
        memcpy(&p->dst, dst, (size_t)dstLen);
        memcpy(p->data, buf, (size_t)len);
        s_held++;
    }
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
             * because jitter means an earlier packet can be due later. Keeping
             * send order and scanning is what lets them overtake each other. */
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
int  Netem_Send(int sock, const char *buf, int len, int flags,
                const struct sockaddr *dst, int dstLen)
{
    (void)sock; (void)buf; (void)len; (void)flags; (void)dst; (void)dstLen;
    return 1;
}
void Netem_Pump(int sock) { (void)sock; }

#endif /* SWOS_NETPLAY */
#endif /* SWOS_SDL_BUILD */
