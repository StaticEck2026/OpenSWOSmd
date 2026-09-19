/*
 * netem_core.h -- the network emulator's decisions, with no socket and no clock.
 *
 * Two emulators share this: the game's own (src/netplay_netem.c, on GGPO's
 * socket, for a match played peer to peer) and the matchmaking server's relay
 * (third_party/matchmaker/Server/Relay.cpp, for a match played through it). One
 * profile table and one set of rules means "wifi" is the same link in both
 * places -- a result measured through the relay can be compared with one measured
 * on the client without asking which emulator produced it.
 *
 * Plain C, no SDL, no platform headers: the server is C++ and builds without the
 * game. The caller owns time (every entry point takes `nowMs`) and the socket
 * (a NetemEmit callback sends what is due). A destination is an opaque blob the
 * caller hands back unchanged -- a sockaddr_storage for the game, an ENetAddress
 * for the server.
 *
 * The conditions, and what each one is in terms of the ONE queue:
 *
 *   lag            due = now + delay
 *   jitter         due = now + delay +/- jitter, per packet
 *   out of order   due pushed past the packets sent after it
 *   throttle       due = the end of the open window, for every packet in it, so
 *                  they leave together in one burst (or, with throttleDrop, never)
 *   drop / loss    no packet at all
 *   duplicate      the same packet queued more than once
 *   tamper         a copy with bits flipped in it
 */
#ifndef NETEM_CORE_H
#define NETEM_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NETEM_MAX_PACKET   1500
#define NETEM_MAX_DST      128     /* sizeof(struct sockaddr_storage) */
#define NETEM_QUEUE        256
#define NETEM_MAX_COPIES   8

/* A link in one word. */
typedef struct {
    const char *name;
    int16_t delayMs, jitterMs, lossPct;
    int16_t reorderPct, reorderMs;
    int16_t dupPct, dupCount;
    int16_t throttlePct, throttleMs;
} NetemProfile;

/* The profiles, in table order. `count` gets how many. */
const NetemProfile *NetemCore_Profiles(int32_t *count);

/* By name, case-insensitive; NULL if there is none. */
const NetemProfile *NetemCore_FindProfile(const char *name);

/* What a link does. Fill it from a profile (or zero it), override what you like,
 * then NetemCore_Clamp. */
typedef struct {
    int32_t delayMs, jitterMs, lossPct;
    int32_t reorderPct, reorderMs;
    int32_t dupPct, dupCount;
    int32_t throttlePct, throttleMs, throttleDrop;
    int32_t tamperPct, tamperBits;
} NetemConfig;

/* The profile's values plus the defaults a profile leaves unset (reorder by
 * 20 ms, duplicate x2, a 200 ms window). `p` may be NULL: a clean link. */
void    NetemCore_ConfigFromProfile(NetemConfig *c, const NetemProfile *p);

/* Into range, and the combinations that make no sense made into ones that do. */
void    NetemCore_Clamp(NetemConfig *c);

/* Non-zero if the config changes anything at all. */
int32_t NetemCore_ConfigEnabled(const NetemConfig *c);

/* "profile wifi, delay 20ms +/-15, loss 1%, ..." -- only the conditions that are
 * on. `profileName` may be NULL. */
void    NetemCore_Describe(const NetemConfig *c, const char *profileName,
                           char *buf, size_t cap);

typedef struct {
    uint32_t dueMs;
    int32_t  len;
    int32_t  flags;
    int32_t  dstLen;
    uint8_t  dst[NETEM_MAX_DST];
    uint8_t  data[NETEM_MAX_PACKET];
} NetemPacket;

/* One direction of one link: its rules, its own generator, and its queue. The
 * queue is heap-allocated on first use, so a link that never holds a packet
 * costs nothing but this struct. */
typedef struct {
    NetemConfig  cfg;
    int32_t      enabled;
    uint32_t     rng;
    NetemPacket *queue;
    int32_t      count;
    int32_t      windowOpen;
    uint32_t     windowEnd;
    /* What actually happened. "loss 8%" in a log line is what was asked for;
     * these are what occurred. */
    uint32_t     offered, untouched, dropped, held;
    uint32_t     reordered, duplicated, tampered;
    uint32_t     throttled, windows, overflowed;
} NetemLink;

/* `seed` 0 is replaced by a fixed non-zero one: pass something that varies. */
void    NetemCore_Init(NetemLink *l, const NetemConfig *cfg, uint32_t seed);
void    NetemCore_Free(NetemLink *l);

typedef void (*NetemEmit)(void *ctx, const uint8_t *data, int32_t len,
                          const void *dst, int32_t dstLen, int32_t flags);

/* Offer one packet. Returns 1 when the packet is untouched and the CALLER should
 * send it now itself (so "delay 0" really is no delay, not "one pump later");
 * 0 when the emulator has it -- dropped, queued, or already emitted. */
int32_t NetemCore_Send(NetemLink *l, uint32_t nowMs,
                       const uint8_t *buf, int32_t len,
                       const void *dst, int32_t dstLen, int32_t flags,
                       NetemEmit emit, void *ctx);

/* Emit everything that is due. Call it often -- every poll. */
void    NetemCore_Pump(NetemLink *l, uint32_t nowMs, NetemEmit emit, void *ctx);

/* "N packets: N untouched, N delayed, N dropped, ..." */
void    NetemCore_Summary(const NetemLink *l, char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* NETEM_CORE_H */
