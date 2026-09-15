/*
 * p2p_link.c -- the connection two players make before a netplay match (SDL builds).
 * docs/NETPLAY.md, "The MULTIPLAYER menu". The interface, and what the link is for,
 * are in include/p2p_link.h.
 *
 * The protocol is six packets over UDP, all of them tiny and none of them retried in
 * order -- every one is either repeated until it is answered (HELLO), or harmless to
 * lose (PING, PONG, BYE):
 *
 *   HELLO   joiner -> host    "this is the game I would play"
 *   WELCOME host  -> joiner   "so would I"   (also resent when a HELLO repeats)
 *   REFUSE  host  -> joiner   "no, because"  (a reason byte)
 *   PING    either way        the sender's own clock, to be echoed
 *   PONG    the echo          so the sender alone measures the round trip
 *   BYE     either way        leaving the menu
 *
 * Every packet starts with the same 8 bytes -- "SWUN", the protocol version, the type
 * -- so a peer of a future version is recognised as one rather than ignored, and told
 * so. Multi-byte fields are big-endian through endian_helpers.h, the byte order the
 * rest of this port's wire formats use.
 *
 * The socket is non-blocking and polled from the menu's per-frame draw; nothing here
 * ever waits, and nothing here runs during a match.
 */
#include "p2p_link.h"

#ifdef SWOS_SDL_BUILD

#include <SDL.h>            /* SDL_GetTicks, SDL_Delay (the self-test) */
#include <stdio.h>
#include <string.h>

#ifdef SWOS_NETPLAY

#include "endian_helpers.h"
#include "sim_state.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Raise this when a packet's meaning changes; peers then refuse each other by version
 * instead of misreading a field. */
enum { kProtocol = 1 };

enum {
    PKT_HELLO = 1, PKT_WELCOME = 2, PKT_REFUSE = 3,
    PKT_PING  = 4, PKT_PONG    = 5, PKT_BYE    = 6,
};

enum {
    kHeaderLen  = 8,
    kSetupLen   = 20,   /* header + the setup fields */
    kRefuseLen  = 9,
    kPingLen    = 12,
    kPacketMax  = 64,
};

enum {
    kHelloIntervalMs = 500,     /* until the host answers */
    kPingIntervalMs  = 1000,
    kJoinTimeoutMs   = 15000,   /* no answer at all: wrong address, port or firewall */
    kQuietTimeoutMs  = 6000,    /* connected, then nothing heard */
    kMaxReadsPerPoll = 32,      /* a frame's worth; the rest waits for the next poll */
    kJoinPortTries   = 10,      /* two peers on one machine need different local ports */
};

/* What both sides must agree on, or the very first frame of a match diverges. The
 * state hash is SimState_LayoutHash (docs/ROLLBACK.md): the shape of the registry a
 * rollback saves, which differs whenever the build's match state does. */
typedef struct {
    uint32_t stateHi, stateLo;
    uint16_t screenWidth;
    uint8_t  gameplay;          /* bit 0 DOS gameplay, 1 arcade, 2 the original fire-hold lockout */
} Setup;

typedef struct {
    int32_t            sock;            /* -1 when closed */
    int32_t            isHost;
    P2PLinkState       state;
    P2PLinkFailure     failure;
    P2PLinkFailure     notice;
    Setup              local;
    uint16_t           localPort;
    struct sockaddr_in peer;
    int32_t            hasPeer;
    uint32_t           openedMs, lastHeardMs, lastSentMs;
    int32_t            pingMs;
    int32_t            joinTimeoutMs;
    char               peerText[24];    /* "255.255.255.255:65535" */
} P2PLink;

static P2PLink s_link = { -1, 0, P2P_IDLE, P2P_FAIL_NONE, P2P_FAIL_NONE,
                          { 0, 0, 0, 0 }, 0, { 0 }, 0, 0, 0, 0, -1, kJoinTimeoutMs, "" };

static uint32_t now_ms(void)
{
    return SDL_GetTicks();
}

/* The game this build would play. The three gameplay flags are set from the command
 * line and SWOS.ini in main.c; the simulation still reads the screen width in a couple
 * of places (docs/NETPLAY.md, "Still to do"), so it counts too. */
static void setup_local(Setup *s)
{
    extern int _screen_width;               /* display.h */
    extern int _gDosGameplay;               /* dos_mode.h */
    extern int _gArcadeMode;                /* arcade.c */
    extern int _gOriginalFireHoldLockout;   /* team_control.c */

    uint64_t hash = SimState_LayoutHash();
    s->stateHi     = (uint32_t)(hash >> 32);
    s->stateLo     = (uint32_t)hash;
    s->screenWidth = (uint16_t)_screen_width;
    s->gameplay    = (uint8_t)((_gDosGameplay ? 1 : 0) |
                               (_gArcadeMode ? 2 : 0) |
                               (_gOriginalFireHoldLockout ? 4 : 0));
}

static P2PLinkFailure setup_mismatch(const Setup *mine, const Setup *theirs)
{
    if (mine->stateHi != theirs->stateHi || mine->stateLo != theirs->stateLo)
        return P2P_FAIL_STATE;
    if (mine->gameplay != theirs->gameplay)
        return P2P_FAIL_GAMEPLAY;
    if (mine->screenWidth != theirs->screenWidth)
        return P2P_FAIL_WIDTH;
    return P2P_FAIL_NONE;
}

/* ---- packets ---- */

static int32_t pkt_header(uint8_t *p, uint8_t type)
{
    p[0] = 'S'; p[1] = 'W'; p[2] = 'U'; p[3] = 'N';
    p[4] = (uint8_t)kProtocol;
    p[5] = type;
    p[6] = 0; p[7] = 0;
    return kHeaderLen;
}

static void pkt_write_setup(uint8_t *p, const Setup *s)
{
    BE_WRITE_U32(p + 8,  s->stateHi);
    BE_WRITE_U32(p + 12, s->stateLo);
    BE_WRITE_U16(p + 16, s->screenWidth);
    p[18] = s->gameplay;
    p[19] = 0;
}

static void pkt_read_setup(const uint8_t *p, Setup *s)
{
    s->stateHi     = BE_READ_U32(p + 8);
    s->stateLo     = BE_READ_U32(p + 12);
    s->screenWidth = BE_READ_U16(p + 16);
    s->gameplay    = p[18];
}

static void link_send(P2PLink *l, const struct sockaddr_in *to, const uint8_t *buf, int32_t len)
{
    if (l->sock < 0)
        return;
    /* UDP and unreliable by design: every packet here is repeated or expendable. */
    (void)sendto(l->sock, (const char *)buf, (size_t)len, 0,
                 (const struct sockaddr *)to, (socklen_t)sizeof *to);
}

static void send_setup_packet(P2PLink *l, const struct sockaddr_in *to, uint8_t type)
{
    uint8_t buf[kSetupLen];
    pkt_header(buf, type);
    pkt_write_setup(buf, &l->local);
    link_send(l, to, buf, (int32_t)sizeof buf);
}

static void send_refuse(P2PLink *l, const struct sockaddr_in *to, P2PLinkFailure reason)
{
    uint8_t buf[kRefuseLen];
    pkt_header(buf, PKT_REFUSE);
    buf[8] = (uint8_t)reason;
    link_send(l, to, buf, (int32_t)sizeof buf);
}

static void send_stamped(P2PLink *l, const struct sockaddr_in *to, uint8_t type, uint32_t stamp)
{
    uint8_t buf[kPingLen];
    pkt_header(buf, type);
    BE_WRITE_U32(buf + 8, stamp);
    link_send(l, to, buf, (int32_t)sizeof buf);
}

static void send_bye(P2PLink *l)
{
    uint8_t buf[kHeaderLen];
    pkt_header(buf, PKT_BYE);
    link_send(l, &l->peer, buf, (int32_t)sizeof buf);
}

/* ---- the socket ---- */

static void drop_socket(P2PLink *l)
{
    if (l->sock >= 0)
        close(l->sock);
    l->sock = -1;
}

/* Bind `port`, or the next free one up to `tries` above it (a joiner sharing a machine
 * with the host). Port 0 asks the system for any free one -- the self-test does, so it
 * never collides with whatever else is running. */
static P2PLinkFailure open_socket(P2PLink *l, uint16_t port, int32_t tries)
{
    int32_t s = (int32_t)socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return P2P_FAIL_SOCKET;

    int32_t flags = fcntl(s, F_GETFL, 0);
    if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(s);
        return P2P_FAIL_SOCKET;
    }

    for (int32_t i = 0; i < tries; i++) {
        if (port != 0 && (int32_t)port + i > 65535)
            break;
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof sin);
        sin.sin_family      = AF_INET;
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
        sin.sin_port        = htons((uint16_t)(port == 0 ? 0 : port + i));
        if (bind(s, (struct sockaddr *)&sin, (socklen_t)sizeof sin) == 0) {
            socklen_t len = (socklen_t)sizeof sin;
            if (getsockname(s, (struct sockaddr *)&sin, &len) == 0)
                l->localPort = ntohs(sin.sin_port);
            else
                l->localPort = (uint16_t)(port + i);
            l->sock = s;
            return P2P_FAIL_NONE;
        }
        if (port == 0)
            break;
    }
    close(s);
    return P2P_FAIL_PORT_IN_USE;
}

static void set_peer(P2PLink *l, const struct sockaddr_in *from)
{
    char ip[INET_ADDRSTRLEN];
    l->peer    = *from;
    l->hasPeer = 1;
    if (!inet_ntop(AF_INET, &from->sin_addr, ip, (socklen_t)sizeof ip))
        snprintf(ip, sizeof ip, "?");
    snprintf(l->peerText, sizeof l->peerText, "%s:%u", ip, (unsigned)ntohs(from->sin_port));
}

static int32_t same_peer(const P2PLink *l, const struct sockaddr_in *from)
{
    return l->hasPeer &&
           l->peer.sin_addr.s_addr == from->sin_addr.s_addr &&
           l->peer.sin_port == from->sin_port;
}

static void link_reset(P2PLink *l)
{
    drop_socket(l);
    l->isHost      = 0;
    l->state       = P2P_IDLE;
    l->failure     = P2P_FAIL_NONE;
    l->notice      = P2P_FAIL_NONE;
    l->hasPeer     = 0;
    l->pingMs      = -1;
    l->localPort   = 0;
    l->peerText[0] = '\0';
    memset(&l->peer, 0, sizeof l->peer);
}

static void link_fail(P2PLink *l, P2PLinkFailure reason)
{
    drop_socket(l);
    l->hasPeer = 0;
    l->pingMs  = -1;
    l->state   = P2P_FAILED;
    l->failure = reason;
}

/* The other player is gone. A host keeps its socket and waits for the next one -- that
 * is what hosting is -- and says what happened to the last; a joiner has nothing left
 * to wait for. */
static void peer_gone(P2PLink *l, P2PLinkFailure reason)
{
    if (l->isHost) {
        l->state   = P2P_HOSTING;
        l->notice  = reason;
        l->hasPeer = 0;
        l->pingMs  = -1;
        l->peerText[0] = '\0';
        memset(&l->peer, 0, sizeof l->peer);
    } else {
        link_fail(l, reason);
    }
}

static void link_connected(P2PLink *l, const struct sockaddr_in *from, uint32_t now)
{
    set_peer(l, from);
    l->state       = P2P_CONNECTED;
    l->notice      = P2P_FAIL_NONE;
    l->lastHeardMs = now;
    l->pingMs      = -1;
    /* Measure the round trip straight away rather than a ping interval later: it is
     * the number a player picks an input delay from. */
    send_stamped(l, &l->peer, PKT_PING, now);
    l->lastSentMs = now;
}

/* ---- receiving ---- */

static P2PLinkFailure refuse_reason(uint8_t byte)
{
    switch (byte) {
    case P2P_FAIL_HOST_BUSY:
    case P2P_FAIL_PROTOCOL:
    case P2P_FAIL_STATE:
    case P2P_FAIL_GAMEPLAY:
    case P2P_FAIL_WIDTH:
        return (P2PLinkFailure)byte;
    default:
        return P2P_FAIL_PROTOCOL;   /* a reason this version does not know */
    }
}

static void handle_packet(P2PLink *l, const uint8_t *buf, int32_t len,
                          const struct sockaddr_in *from, uint32_t now)
{
    if (len < kHeaderLen || memcmp(buf, "SWUN", 4) != 0)
        return;                                  /* not ours: some other traffic */

    uint8_t type = buf[5];

    if (buf[4] != (uint8_t)kProtocol) {
        /* A OpenSWOS of another version. Tell a joiner so, and take the same from a
         * host rather than waiting out the timeout. */
        if (l->state == P2P_HOSTING && type == PKT_HELLO)
            send_refuse(l, from, P2P_FAIL_PROTOCOL);
        else if (l->state == P2P_JOINING && same_peer(l, from))
            link_fail(l, P2P_FAIL_PROTOCOL);
        return;
    }

    switch (l->state) {
    case P2P_HOSTING:
        if (type == PKT_HELLO && len >= kSetupLen) {
            Setup theirs;
            pkt_read_setup(buf, &theirs);
            P2PLinkFailure bad = setup_mismatch(&l->local, &theirs);
            if (bad != P2P_FAIL_NONE) {
                send_refuse(l, from, bad);
                l->notice = bad;        /* so the host sees why someone bounced off */
                return;
            }
            send_setup_packet(l, from, PKT_WELCOME);
            link_connected(l, from, now);
        }
        break;

    case P2P_JOINING:
        if (!same_peer(l, from))
            return;                     /* only the host we asked */
        if (type == PKT_WELCOME && len >= kSetupLen) {
            Setup theirs;
            pkt_read_setup(buf, &theirs);
            P2PLinkFailure bad = setup_mismatch(&l->local, &theirs);
            if (bad != P2P_FAIL_NONE) {
                link_fail(l, bad);
                return;
            }
            link_connected(l, from, now);
        } else if (type == PKT_REFUSE && len >= kRefuseLen) {
            link_fail(l, refuse_reason(buf[8]));
        }
        break;

    case P2P_CONNECTED:
        if (!same_peer(l, from)) {
            if (type == PKT_HELLO)
                send_refuse(l, from, P2P_FAIL_HOST_BUSY);
            return;
        }
        l->lastHeardMs = now;
        if (type == PKT_HELLO) {
            send_setup_packet(l, from, PKT_WELCOME);    /* our welcome was lost */
        } else if (type == PKT_PING && len >= kPingLen) {
            send_stamped(l, from, PKT_PONG, BE_READ_U32(buf + 8));
        } else if (type == PKT_PONG && len >= kPingLen) {
            int32_t rtt = (int32_t)(now - BE_READ_U32(buf + 8));
            if (rtt < 0)
                rtt = 0;
            l->pingMs = (l->pingMs < 0) ? rtt : (l->pingMs * 3 + rtt) / 4;
        } else if (type == PKT_BYE) {
            peer_gone(l, P2P_FAIL_PEER_LEFT);
        }
        break;

    default:
        break;
    }
}

static void link_poll(P2PLink *l, uint32_t now)
{
    if (l->sock < 0)
        return;

    for (int32_t i = 0; i < kMaxReadsPerPoll; i++) {
        uint8_t buf[kPacketMax];
        struct sockaddr_in from;
        socklen_t fromLen = (socklen_t)sizeof from;
        ssize_t n = recvfrom(l->sock, (char *)buf, sizeof buf, 0,
                             (struct sockaddr *)&from, &fromLen);
        if (n < 0) {
            /* Nothing left to read. Winsock (so Cygwin) also reports an ICMP "port
             * unreachable" from an earlier send here, on a socket that is not
             * connected -- a host that is not listening yet, which is not an error
             * for us: read on, and let the join time out if it never answers. */
            if (errno == ECONNRESET || errno == ECONNREFUSED)
                continue;
            break;
        }
        if (from.sin_family == AF_INET)
            handle_packet(l, buf, (int32_t)n, &from, now);
        if (l->sock < 0)
            break;                      /* the packet ended the link */
    }

    if (l->state == P2P_JOINING) {
        if (now - l->openedMs >= (uint32_t)l->joinTimeoutMs) {
            link_fail(l, P2P_FAIL_NO_ANSWER);
        } else if (now - l->lastSentMs >= (uint32_t)kHelloIntervalMs) {
            send_setup_packet(l, &l->peer, PKT_HELLO);
            l->lastSentMs = now;
        }
    } else if (l->state == P2P_CONNECTED) {
        if (now - l->lastHeardMs >= (uint32_t)kQuietTimeoutMs)
            peer_gone(l, P2P_FAIL_LOST);
        else if (now - l->lastSentMs >= (uint32_t)kPingIntervalMs) {
            send_stamped(l, &l->peer, PKT_PING, now);
            l->lastSentMs = now;
        }
    }
}

/* ---- opening and closing ---- */

static int32_t link_host(P2PLink *l, uint16_t port)
{
    link_reset(l);
    P2PLinkFailure bad = open_socket(l, port, 1);
    if (bad != P2P_FAIL_NONE) {
        l->state   = P2P_FAILED;
        l->failure = bad;
        return 1;
    }
    setup_local(&l->local);
    l->isHost   = 1;
    l->state    = P2P_HOSTING;
    l->openedMs = l->lastHeardMs = l->lastSentMs = now_ms();
    return 0;
}

/* `setupOverride` is the self-test's way to pose as a different build; NULL is this one. */
static int32_t link_join(P2PLink *l, const char *address, uint16_t port, const Setup *setupOverride)
{
    struct in_addr addr;

    link_reset(l);
    if (!address || inet_pton(AF_INET, address, &addr) != 1) {
        l->state   = P2P_FAILED;
        l->failure = P2P_FAIL_BAD_ADDRESS;
        return 1;
    }

    P2PLinkFailure bad = open_socket(l, port, kJoinPortTries);
    if (bad != P2P_FAIL_NONE) {
        l->state   = P2P_FAILED;
        l->failure = bad;
        return 1;
    }

    setup_local(&l->local);
    if (setupOverride)
        l->local = *setupOverride;

    struct sockaddr_in host;
    memset(&host, 0, sizeof host);
    host.sin_family = AF_INET;
    host.sin_addr   = addr;
    host.sin_port   = htons(port);
    set_peer(l, &host);

    l->isHost   = 0;
    l->state    = P2P_JOINING;
    l->openedMs = l->lastHeardMs = l->lastSentMs = now_ms();
    send_setup_packet(l, &l->peer, PKT_HELLO);
    return 0;
}

static void link_close(P2PLink *l)
{
    if (l->sock >= 0 && l->state == P2P_CONNECTED && l->hasPeer) {
        /* Twice: it is never retried, and it saves the other player a six-second wait. */
        send_bye(l);
        send_bye(l);
    }
    link_reset(l);
}

/* ---- the game's link ---- */

int32_t P2PLink_Host(uint16_t port)                    { return link_host(&s_link, port); }
int32_t P2PLink_Join(const char *address, uint16_t port) { return link_join(&s_link, address, port, NULL); }
void    P2PLink_Poll(void)                             { link_poll(&s_link, now_ms()); }
void    P2PLink_Close(void)                            { link_close(&s_link); }

P2PLinkState   P2PLink_State(void)     { return s_link.state; }
P2PLinkFailure P2PLink_Failure(void)   { return s_link.failure; }
P2PLinkFailure P2PLink_Notice(void)    { return s_link.notice; }
const char    *P2PLink_PeerText(void)  { return s_link.peerText; }
uint16_t       P2PLink_LocalPort(void) { return s_link.localPort; }
int32_t        P2PLink_PingMs(void)    { return s_link.pingMs; }

int32_t P2PLink_IsAddress(const char *text)
{
    struct in_addr addr;
    return text && inet_pton(AF_INET, text, &addr) == 1;
}

/* ---- self-test ---- */

static const char *state_name(P2PLinkState s)
{
    switch (s) {
    case P2P_IDLE:      return "idle";
    case P2P_HOSTING:   return "hosting";
    case P2P_JOINING:   return "joining";
    case P2P_CONNECTED: return "connected";
    case P2P_FAILED:    return "failed";
    }
    return "?";
}

/* Poll every link for up to `ms`, stopping as soon as `done` is true. */
typedef int32_t (*DoneFn)(void *ctx);
static void pump(P2PLink **links, int32_t count, int32_t ms, DoneFn done, void *ctx)
{
    uint32_t start = now_ms();
    for (;;) {
        uint32_t now = now_ms();
        for (int32_t i = 0; i < count; i++)
            link_poll(links[i], now);
        if (done && done(ctx))
            return;
        if (now - start >= (uint32_t)ms)
            return;
        SDL_Delay(1);
    }
}

static int32_t s_failures;

static void check(int32_t ok, const char *what)
{
    printf("  %-44s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok)
        s_failures++;
}

typedef struct { P2PLink *a, *b; } PairCtx;

static int32_t both_connected(void *ctx)
{
    PairCtx *p = (PairCtx *)ctx;
    return p->a->state == P2P_CONNECTED && p->b->state == P2P_CONNECTED && p->b->pingMs >= 0;
}

static int32_t link_settled(void *ctx)
{
    P2PLink *l = (P2PLink *)ctx;
    return l->state == P2P_FAILED;
}

static int32_t host_waiting_again(void *ctx)
{
    P2PLink *l = (P2PLink *)ctx;
    return l->state == P2P_HOSTING && l->notice != P2P_FAIL_NONE;
}

int32_t P2PLink_SelfTest(void)
{
    P2PLink host, joiner, second, mismatched, lonely;
    P2PLink *pair[2];

    printf("[p2p-link] self-test: two links over loopback\n");
    s_failures = 0;

    memset(&host, 0, sizeof host);       host.sock = -1;       host.pingMs = -1;       host.joinTimeoutMs = kJoinTimeoutMs;
    memset(&joiner, 0, sizeof joiner);   joiner.sock = -1;     joiner.pingMs = -1;     joiner.joinTimeoutMs = kJoinTimeoutMs;
    memset(&second, 0, sizeof second);   second.sock = -1;     second.pingMs = -1;     second.joinTimeoutMs = 1000;
    memset(&mismatched, 0, sizeof mismatched); mismatched.sock = -1; mismatched.pingMs = -1; mismatched.joinTimeoutMs = 1000;
    memset(&lonely, 0, sizeof lonely);   lonely.sock = -1;     lonely.pingMs = -1;     lonely.joinTimeoutMs = 400;

    /* 1. They connect, and each measures the round trip. */
    check(link_host(&host, 0) == 0, "host binds a port");
    if (host.state != P2P_HOSTING) {
        printf("  host could not start: %s\n", state_name(host.state));
        return 1;
    }
    check(link_join(&joiner, "127.0.0.1", host.localPort, NULL) == 0, "joiner opens and says hello");

    pair[0] = &host; pair[1] = &joiner;
    PairCtx both = { &host, &joiner };
    pump(pair, 2, 2000, both_connected, &both);
    check(host.state == P2P_CONNECTED && joiner.state == P2P_CONNECTED, "both sides connect");
    check(joiner.pingMs >= 0, "the joiner measures the round trip");
    check(host.peerText[0] != '\0' && joiner.peerText[0] != '\0', "each knows the other's address");

    /* 2. A second player is turned away while the host has one. */
    check(link_join(&second, "127.0.0.1", host.localPort, NULL) == 0, "a second player tries to join");
    {
        P2PLink *three[3] = { &host, &joiner, &second };
        pump(three, 3, 1500, link_settled, &second);
    }
    check(second.state == P2P_FAILED && second.failure == P2P_FAIL_HOST_BUSY, "the host is busy, and says so");
    link_close(&second);

    /* 3. The joiner leaves: the host hears it and waits for the next player. */
    link_close(&joiner);
    pump(pair, 1, 1500, host_waiting_again, &host);
    check(host.state == P2P_HOSTING && host.notice == P2P_FAIL_PEER_LEFT, "the host sees the player leave");

    /* 4. A build that would play a different game is refused, with the reason. */
    {
        Setup theirs;
        setup_local(&theirs);
        theirs.gameplay ^= 1;           /* the other gameplay style */
        check(link_join(&mismatched, "127.0.0.1", host.localPort, &theirs) == 0, "a mismatched build joins");
        P2PLink *two[2] = { &host, &mismatched };
        pump(two, 2, 1500, link_settled, &mismatched);
        check(mismatched.state == P2P_FAILED && mismatched.failure == P2P_FAIL_GAMEPLAY,
              "it is refused: a different gameplay style");
        check(host.state == P2P_HOSTING && host.notice == P2P_FAIL_GAMEPLAY, "the host says who bounced off");
        link_close(&mismatched);
    }

    /* 5. Nobody there: the join gives up rather than hanging. */
    {
        uint16_t empty = (uint16_t)(host.localPort ^ 0x0100);   /* a port with no link on it */
        P2PLink *one[1] = { &lonely };
        check(link_join(&lonely, "127.0.0.1", empty, NULL) == 0, "a join with nobody listening");
        pump(one, 1, 2000, link_settled, &lonely);
        check(lonely.state == P2P_FAILED && lonely.failure == P2P_FAIL_NO_ANSWER, "it gives up after the timeout");
        link_close(&lonely);
    }

    link_close(&host);
    printf("[p2p-link] self-test %s\n", s_failures ? "FAILED" : "passed");
    fflush(stdout);
    return s_failures != 0;
}

#else /* !SWOS_NETPLAY -- the builds with no GGPO to play through either */

int32_t P2PLink_Host(uint16_t port)                      { (void)port; return 1; }
int32_t P2PLink_Join(const char *address, uint16_t port) { (void)address; (void)port; return 1; }
void    P2PLink_Poll(void)                               { }
void    P2PLink_Close(void)                              { }

P2PLinkState   P2PLink_State(void)     { return P2P_FAILED; }
P2PLinkFailure P2PLink_Failure(void)   { return P2P_FAIL_UNAVAILABLE; }
P2PLinkFailure P2PLink_Notice(void)    { return P2P_FAIL_NONE; }
const char    *P2PLink_PeerText(void)  { return ""; }
uint16_t       P2PLink_LocalPort(void) { return 0; }
int32_t        P2PLink_PingMs(void)    { return -1; }
int32_t        P2PLink_IsAddress(const char *text) { (void)text; return 0; }

int32_t P2PLink_SelfTest(void)
{
    printf("[p2p-link] self-test: built without netplay (make NETPLAY=1), nothing to test\n");
    return 0;
}

#endif /* SWOS_NETPLAY */
#endif /* SWOS_SDL_BUILD */
