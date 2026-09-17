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
#include <stdlib.h>
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
    PKT_DELAY = 7,              /* my proposed input delay, in frames */
    PKT_BLOB  = 8,              /* one chunk of the match setup */
    PKT_BLOB_ACK = 9,           /* which chunks the receiver has */
};

enum {
    kHeaderLen  = 8,
    kSetupLen   = 20,   /* header + the setup fields */
    kRefuseLen  = 9,
    kPingLen    = 12,
    kDelayLen   = 9,
    /* The READ buffer, so it has to fit the LARGEST packet, not the typical
     * one. It was 64 -- ample for the control packets, and 16 bytes of header
     * plus 1024 of payload short of a setup chunk. recvfrom truncates silently,
     * so every chunk lost all but its first 48 bytes while the transfer still
     * completed and acked: right length, full bitmap, wrong contents. The setup
     * blob defines this now (kBlobChunk is declared below and used here). */
    kPacketMax  = 8 + 8 + 1024,
};

enum {
    kHelloIntervalMs = 500,     /* until the host answers */
    kPingIntervalMs  = 1000,
    kJoinTimeoutMs   = 15000,   /* no answer at all: wrong address, port or firewall */
    kQuietTimeoutMs  = 6000,    /* connected, then nothing heard */
    kMaxReadsPerPoll = 32,      /* a frame's worth; the rest waits for the next poll */
    kJoinPortTries   = 10,      /* two peers on one machine need different local ports */
    kMaxCandidates   = 8,       /* what the matchmaker hands over (MM_MAX_PEER_ADDRS) */
    kDelayIntervalMs = 250,     /* re-send the proposal until the peer's is known (UDP) */
    kDelayMax        = 8,       /* more input lag than this is worse than the rollback */

    /* The match setup blob. It is the whole kick-off state (~35 KB raw, less
     * once deflated), which is far too big for one datagram, so it goes as
     * numbered chunks with the receiver naming what it still lacks.
     *
     * 1024 bytes of payload keeps a chunk under the smallest MTU anyone
     * realistically has once IP and UDP headers are on it; going to the
     * "normal" 1472 saves a third of the packets and loses the match on any
     * path that fragments. */
    kBlobChunk       = 1024,
    kBlobMaxChunks   = 128,                        /* 128 KB, well over what we send */
    kBlobMax         = kBlobChunk * kBlobMaxChunks,
    kBlobBitmapBytes = kBlobMaxChunks / 8,
    kBlobResendMs    = 120,     /* re-send whatever is still unacked */
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
    int32_t            delayLocal;      /* what we propose, -1 until we have a ping */
    int32_t            delayPeer;       /* what they proposed, -1 until it arrives */
    uint32_t           lastDelaySentMs;
    int32_t            joinTimeoutMs;
    char               peerText[24];    /* "255.255.255.255:65535" */

    /* Sending a setup blob (the host). */
    uint8_t           *txBlob;
    uint32_t           txLen;
    int32_t            txChunks;
    uint8_t            txAcked[kBlobBitmapBytes];
    uint32_t           txLastSendMs;

    /* NAT traversal: every address the peer might be reachable at. Both sides
     * send to all of them at once -- see punch_send. */
    struct sockaddr_in candidates[kMaxCandidates];
    int32_t            numCandidates;

    /* Receiving one (the joiner). */
    uint8_t           *rxBlob;
    uint32_t           rxLen;
    int32_t            rxChunks;
    uint8_t            rxHave[kBlobBitmapBytes];
    int32_t            rxComplete;
} P2PLink;

static P2PLink s_link = { -1, 0, P2P_IDLE, P2P_FAIL_NONE, P2P_FAIL_NONE,
                          { 0, 0, 0, 0 }, 0, { 0 }, 0, 0, 0, 0, -1,
                          -1, -1, 0, kJoinTimeoutMs, "" };

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

static void blob_free(P2PLink *l);      /* defined with the rest of the blob code */

static void link_reset(P2PLink *l)
{
    drop_socket(l);
    blob_free(l);                       /* a half-sent setup does not outlive its link */
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

/* ---- the input delay, agreed rather than each side choosing its own ----------
 *
 * THE BUG THIS EXISTS TO NOT HAVE. SWOS 2020 computes a delay on each machine
 * from that machine's own measurements and applies it to its own player only
 * (`ggpo_set_frame_delay(ggpo, local_player_handle, frameDelay)`, mpgame.cpp) --
 * the inputs are ping, jitter, and `1000/getMPFps()`. Two machines on the same
 * link do not measure the same jitter, and if one runs slower they do not even
 * agree on the frame time, so the two sides can settle on different numbers: a
 * match that started "AUTO 3" can end up 1 against 5. That is the reported
 * "chasing" -- the player carrying 5 frames of lag is reacting a quarter of a
 * second after the other one, and cannot win a race or a duel. It is very
 * probably most of the reported "ghosting" too: in a shoulder-to-shoulder
 * contest, four extra frames decides it before the skill roll is ever reached.
 *
 * So the delay is NEGOTIATED, and the rule is chosen so that both sides compute
 * the SAME answer from the SAME two numbers: each proposes, both take the
 * maximum. Max, not min or mean: the worse connection is the one that needs the
 * buffer, and a mean would round differently on each side the moment the two
 * proposals differ by an odd number.
 *
 * It is also agreed ONCE and then left alone. Re-deriving it mid-match is the
 * other way the two sides drift apart, and a delay that changes under you is
 * itself indistinguishable from lag.
 *
 * Both peers still apply it to their own local player -- that is all GGPO
 * offers -- but because the number is identical, the two are symmetric. */
static int32_t propose_delay(int32_t rttMs)
{
    extern int _gDosGameplay;               /* dos_mode.h */
    /* The frame time comes from the GAMEPLAY MODE, not from anything this
     * machine measures. That matters: setup_mismatch already refuses a peer
     * whose gameplay flags differ, so both sides are certain to use the same
     * number here. Taking it from the actual pacing (Platform_ActivePacingFps,
     * which is display-derived and in any case static to platform_sdl.c) would
     * let a machine that is running slow propose a different delay from its
     * opponent -- which is precisely the asymmetry this routine exists to
     * avoid. */
    int32_t fps = _gDosGameplay ? 70 : 50;
    int32_t frameMs, d;

    frameMs = 1000 / fps;
    if (frameMs <= 0)
        frameMs = 20;
    if (rttMs < 0)
        rttMs = 0;

    /* ceil(rtt / (3 * frameTime)). The 3 is the ratio GGPO-based fighting games
     * converge on (SWOS 2020 cites the same one): a third of the round trip is
     * what frame delay has to cover for rollback to absorb the rest. */
    d = (rttMs + (3 * frameMs - 1)) / (3 * frameMs);
    if (d < 0)
        d = 0;
    if (d > kDelayMax)
        d = kDelayMax;
    return d;
}

static void send_delay(P2PLink *l, uint32_t now)
{
    uint8_t pkt[kDelayLen];
    if (!l->hasPeer || l->delayLocal < 0)
        return;
    memcpy(pkt, "SWUN", 4);
    pkt[4] = (uint8_t)kProtocol;
    pkt[5] = PKT_DELAY;
    pkt[6] = 0;
    pkt[7] = 0;
    pkt[8] = (uint8_t)l->delayLocal;
    sendto(l->sock, (const char *)pkt, sizeof pkt, 0,
           (const struct sockaddr *)&l->peer, sizeof l->peer);
    l->lastDelaySentMs = now;
}

/* ---- the match setup blob --------------------------------------------------
 *
 * The host's kick-off state, so the joiner starts the match from exactly the
 * same place. It is the state_wire packet (docs/ROLLBACK.md, "The wire format"),
 * which already encodes portably -- typed fields big-endian, pointers as
 * identities -- so a 32-bit peer and a 64-bit one read the same bytes. What is
 * missing from UDP is only getting ~35 KB there intact, which is all this is.
 *
 * Numbered chunks, receiver-driven: the receiver says which chunks it HAS (a
 * bitmap), the sender re-sends what is missing until the bitmap is full. No
 * ordering assumptions and no per-chunk ack storm -- one small ack packet
 * describes the whole transfer, so a lost ack costs one retransmit round rather
 * than a stall. */
static void bitmap_set(uint8_t *bm, int32_t i)   { bm[i >> 3] |= (uint8_t)(1u << (i & 7)); }
static int32_t bitmap_get(const uint8_t *bm, int32_t i) { return (bm[i >> 3] >> (i & 7)) & 1; }

static int32_t bitmap_full(const uint8_t *bm, int32_t n)
{
    int32_t i;
    for (i = 0; i < n; i++)
        if (!bitmap_get(bm, i))
            return 0;
    return 1;
}

static void blob_free(P2PLink *l)
{
    free(l->txBlob); l->txBlob = NULL; l->txLen = 0; l->txChunks = 0;
    free(l->rxBlob); l->rxBlob = NULL; l->rxLen = 0; l->rxChunks = 0;
    l->rxComplete = 0;
    memset(l->txAcked, 0, sizeof l->txAcked);
    memset(l->rxHave,  0, sizeof l->rxHave);
}

/* One chunk: header, total length (u32), chunk index (u16), chunk count (u16),
 * then the bytes. */
static void send_blob_chunk(P2PLink *l, int32_t idx)
{
    uint8_t pkt[kHeaderLen + 8 + kBlobChunk];
    uint32_t off = (uint32_t)idx * (uint32_t)kBlobChunk;
    uint32_t n   = l->txLen - off;

    if (n > (uint32_t)kBlobChunk)
        n = (uint32_t)kBlobChunk;
    pkt_header(pkt, PKT_BLOB);
    BE_WRITE_U32(pkt + 8, l->txLen);
    BE_WRITE_U16(pkt + 12, (uint16_t)idx);
    BE_WRITE_U16(pkt + 14, (uint16_t)l->txChunks);
    memcpy(pkt + 16, l->txBlob + off, n);
    sendto(l->sock, (const char *)pkt, kHeaderLen + 8 + n, 0,
           (const struct sockaddr *)&l->peer, sizeof l->peer);
}

static void send_blob_ack(P2PLink *l)
{
    uint8_t pkt[kHeaderLen + kBlobBitmapBytes];
    pkt_header(pkt, PKT_BLOB_ACK);
    memcpy(pkt + kHeaderLen, l->rxHave, kBlobBitmapBytes);
    sendto(l->sock, (const char *)pkt, sizeof pkt, 0,
           (const struct sockaddr *)&l->peer, sizeof l->peer);
}

/* Is this packet from one of the addresses the peer might be at?
 *
 * HOST ONLY, deliberately: a NAT rewrites the SOURCE PORT, so the datagram that
 * finally arrives comes from a port nobody advertised. Comparing the port would
 * reject exactly the packets that prove a path works. The reference
 * implementation matches the same way (P2PConnection.cpp, matchAddressOnly). */
static int32_t from_candidate(const P2PLink *l, const struct sockaddr_in *from)
{
    int32_t i;
    for (i = 0; i < l->numCandidates; i++)
        if (l->candidates[i].sin_addr.s_addr == from->sin_addr.s_addr)
            return 1;
    return 0;
}

/* Say hello to every candidate at once.
 *
 * THIS IS THE HOLE PUNCH. Each datagram we send opens a mapping in our own NAT
 * for the address it went to, so the peer's reply has somewhere to arrive. Both
 * peers do it simultaneously -- neither is "the host" here -- which is what
 * gets two players behind different routers talking without either forwarding a
 * port. Most of these go nowhere (a LAN address on the other side of the
 * internet); that is the point, they cost a packet each and one of them works. */
static void punch_send(P2PLink *l)
{
    int32_t i;
    for (i = 0; i < l->numCandidates; i++)
        send_setup_packet(l, &l->candidates[i], PKT_HELLO);
    l->lastSentMs = now_ms();
}

static void link_connected(P2PLink *l, const struct sockaddr_in *from, uint32_t now)
{
    blob_free(l);   /* nothing carries over from a previous peer */
    l->delayLocal = l->delayPeer = -1;
    l->lastDelaySentMs = 0;
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
        /* With a candidate list this is the PUNCHING state and any of them may
         * answer -- and from a port a NAT chose, so the match is on host only.
         * With a single peer (the direct-IP menu) it is the old behaviour. */
        if (l->numCandidates > 0 ? !from_candidate(l, from) : !same_peer(l, from))
            return;
        if (type == PKT_HELLO && len >= kSetupLen && l->numCandidates > 0) {
            /* The peer is punching too and its packet arrived first. Answer it
             * and take THIS address: it is the one demonstrably working, which
             * the advertised one may not be. */
            Setup theirs;
            pkt_read_setup(buf, &theirs);
            {
                P2PLinkFailure bad = setup_mismatch(&l->local, &theirs);
                if (bad != P2P_FAIL_NONE) {
                    send_refuse(l, from, bad);
                    link_fail(l, bad);
                    return;
                }
            }
            send_setup_packet(l, from, PKT_WELCOME);
            link_connected(l, from, now);
            return;
        }
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
        } else if (type == PKT_DELAY && len >= kDelayLen) {
            int32_t theirs = buf[8];
            if (theirs > kDelayMax)
                theirs = kDelayMax;      /* never take a number we would not send */
            l->delayPeer = theirs;
            /* ANSWER IT. Without this the exchange deadlocks: each side stopped
             * proposing once it had the other's, so whoever heard first went
             * quiet before the other had heard anything at all, and that side
             * waited for a proposal that would never be sent again. Measured --
             * the joiner had both numbers while the host timed out holding one.
             * A reply is 9 bytes and makes the exchange self-completing. */
            if (l->delayLocal >= 0)
                send_delay(l, now);
        } else if (type == PKT_BLOB && len >= kHeaderLen + 8) {
            uint32_t total = BE_READ_U32(buf + 8);
            int32_t  idx   = (int32_t)BE_READ_U16(buf + 12);
            int32_t  count = (int32_t)BE_READ_U16(buf + 14);
            uint32_t n     = (uint32_t)len - (kHeaderLen + 8);

            /* Refuse anything that does not describe a transfer we could hold,
             * rather than trusting a length off the wire. */
            if (total == 0 || total > (uint32_t)kBlobMax ||
                count <= 0 || count > kBlobMaxChunks ||
                idx < 0 || idx >= count || n > (uint32_t)kBlobChunk)
                break;
            if (l->rxBlob && (l->rxLen != total || l->rxChunks != count))
                break;                        /* a different transfer mid-flight */
            if (!l->rxBlob) {
                l->rxBlob = (uint8_t *)calloc(1, total);
                if (!l->rxBlob)
                    break;
                l->rxLen = total;
                l->rxChunks = count;
                memset(l->rxHave, 0, sizeof l->rxHave);
                l->rxComplete = 0;
            }
            {
                uint32_t off = (uint32_t)idx * (uint32_t)kBlobChunk;
                if (off + n > l->rxLen)
                    break;
                memcpy(l->rxBlob + off, buf + kHeaderLen + 8, n);
                bitmap_set(l->rxHave, idx);
                if (bitmap_full(l->rxHave, l->rxChunks))
                    l->rxComplete = 1;
            }
            send_blob_ack(l);                 /* every chunk: cheap, and self-clocking */
        } else if (type == PKT_BLOB_ACK && len >= kHeaderLen + kBlobBitmapBytes) {
            memcpy(l->txAcked, buf + kHeaderLen, kBlobBitmapBytes);
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
            if (l->numCandidates > 0) {
                punch_send(l);      /* keeps every mapping alive, not just one */
            } else {
                send_setup_packet(l, &l->peer, PKT_HELLO);
                l->lastSentMs = now;
            }
        }
    } else if (l->state == P2P_CONNECTED) {
        /* Keep pushing whatever the receiver has not acked yet. */
        if (l->txBlob && !bitmap_full(l->txAcked, l->txChunks) &&
            now - l->txLastSendMs >= (uint32_t)kBlobResendMs) {
            int32_t i, sent = 0;
            for (i = 0; i < l->txChunks && sent < 16; i++) {
                if (!bitmap_get(l->txAcked, i)) {
                    send_blob_chunk(l, i);
                    sent++;
                }
            }
            l->txLastSendMs = now;
        }

        /* Propose as soon as there is a round trip to base it on, and keep
         * re-sending until the peer's proposal arrives: this is UDP and a lost
         * proposal would otherwise leave the match with no agreed delay. */
        if (l->delayLocal < 0 && l->pingMs >= 0)
            l->delayLocal = propose_delay(l->pingMs);
        /* Keep proposing until BOTH halves are known here. Losing the reply
         * above is otherwise unrecoverable, and this is UDP. */
        if (l->delayLocal >= 0 && l->delayPeer < 0 &&
            now - l->lastDelaySentMs >= (uint32_t)kDelayIntervalMs)
            send_delay(l, now);

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

/* NAT traversal: both peers call this, with the addresses the matchmaker gave
 * them for the other one, and both send to all of them at once.
 *
 * There is no host and no joiner here. A NAT lets a datagram IN only after one
 * has gone OUT to that address, so a peer that merely listens is unreachable;
 * both have to be punching at the same time. The player number still decides
 * who is player 1, but it plays no part in making the connection.
 *
 * The local port matters: it is the one the matchmaker saw us on, so the
 * mapping the server's traffic already opened is the one the peer's packets
 * arrive through. Binding a different port throws that away. */
static int32_t link_punch(P2PLink *l, const char *const *addresses, int32_t count,
                          uint16_t localPort)
{
    int32_t i, n = 0;

    link_reset(l);
    if (!addresses || count <= 0) {
        l->state = P2P_FAILED;
        l->failure = P2P_FAIL_BAD_ADDRESS;
        return 1;
    }

    for (i = 0; i < count && n < kMaxCandidates; i++) {
        char text[24], *colon;
        struct in_addr host;
        uint16_t port = 0;

        if (!addresses[i] || !addresses[i][0])
            continue;
        snprintf(text, sizeof text, "%s", addresses[i]);
        colon = strchr(text, ':');
        if (colon) {
            *colon = '\0';
            port = (uint16_t)atoi(colon + 1);
        }
        if (!port || inet_pton(AF_INET, text, &host) != 1)
            continue;               /* not an address we can punch to; skip it */
        memset(&l->candidates[n], 0, sizeof l->candidates[n]);
        l->candidates[n].sin_family = AF_INET;
        l->candidates[n].sin_addr   = host;
        l->candidates[n].sin_port   = htons(port);
        n++;
    }
    if (n == 0) {
        l->state = P2P_FAILED;
        l->failure = P2P_FAIL_BAD_ADDRESS;
        return 1;
    }
    l->numCandidates = n;

    {
        P2PLinkFailure bad = open_socket(l, localPort, kJoinPortTries);
        if (bad != P2P_FAIL_NONE) {
            l->state = P2P_FAILED;
            l->failure = bad;
            return 1;
        }
    }
    setup_local(&l->local);
    l->isHost = 0;
    l->state  = P2P_JOINING;        /* "connecting", as the menus already read it */
    l->openedMs = l->lastHeardMs = l->lastSentMs = now_ms();
    punch_send(l);                  /* open our side's mappings immediately */
    return 0;
}

int32_t P2PLink_Punch(const char *const *addresses, int32_t count, uint16_t localPort)
{
    return link_punch(&s_link, addresses, count, localPort);
}

int32_t P2PLink_Host(uint16_t port)                    { return link_host(&s_link, port); }
int32_t P2PLink_Join(const char *address, uint16_t port) { return link_join(&s_link, address, port, NULL); }
void    P2PLink_Poll(void)                             { link_poll(&s_link, now_ms()); }
void    P2PLink_Close(void)                            { link_close(&s_link); }

P2PLinkState   P2PLink_State(void)     { return s_link.state; }
P2PLinkFailure P2PLink_Failure(void)   { return s_link.failure; }
P2PLinkFailure P2PLink_Notice(void)    { return s_link.notice; }
const char    *P2PLink_PeerText(void)  { return s_link.peerText; }
uint16_t       P2PLink_LocalPort(void) { return s_link.localPort; }

int32_t P2PLink_AgreedDelay(void)
{
    const P2PLink *l = &s_link;
    if (l->state != P2P_CONNECTED || l->delayLocal < 0 || l->delayPeer < 0)
        return -1;
    /* Max, computed from the same two numbers on both peers, so both get the
     * same answer. See the note above propose_delay. */
    return l->delayLocal > l->delayPeer ? l->delayLocal : l->delayPeer;
}

/* ---- the setup blob, from the outside ---- */

int32_t P2PLink_SendSetup(const uint8_t *data, uint32_t len)
{
    P2PLink *l = &s_link;
    if (l->state != P2P_CONNECTED || !data || len == 0 || len > (uint32_t)kBlobMax)
        return 1;
    free(l->txBlob);
    l->txBlob = (uint8_t *)malloc(len);
    if (!l->txBlob)
        return 1;
    memcpy(l->txBlob, data, len);
    l->txLen    = len;
    l->txChunks = (int32_t)((len + kBlobChunk - 1) / kBlobChunk);
    memset(l->txAcked, 0, sizeof l->txAcked);
    l->txLastSendMs = 0;        /* the next poll starts sending */
    return 0;
}

int32_t P2PLink_SetupSent(void)
{
    const P2PLink *l = &s_link;
    if (!l->txBlob)
        return 0;
    return bitmap_full(l->txAcked, l->txChunks);
}

const uint8_t *P2PLink_ReceivedSetup(uint32_t *len)
{
    const P2PLink *l = &s_link;
    if (!l->rxComplete)
        return NULL;
    if (len)
        *len = l->rxLen;
    return l->rxBlob;
}

int32_t P2PLink_SetupProgress(void)
{
    const P2PLink *l = &s_link;
    int32_t i, have = 0, n = 0;
    if (l->txBlob) { n = l->txChunks; for (i = 0; i < n; i++) have += bitmap_get(l->txAcked, i); }
    else if (l->rxBlob) { n = l->rxChunks; for (i = 0; i < n; i++) have += bitmap_get(l->rxHave, i); }
    if (n <= 0)
        return -1;
    return have * 100 / n;
}

int32_t P2PLink_ProposedDelay(void) { return s_link.delayLocal; }
int32_t P2PLink_PeerProposedDelay(void) { return s_link.delayPeer; }
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

static int32_t both_have_delay(void *ctx)
{
    PairCtx *p = (PairCtx *)ctx;
    return p->a->delayLocal >= 0 && p->a->delayPeer >= 0 &&
           p->b->delayLocal >= 0 && p->b->delayPeer >= 0;
}

static int32_t joiner_has_setup(void *ctx)
{
    PairCtx *p = (PairCtx *)ctx;
    return p->b->rxComplete && bitmap_full(p->a->txAcked, p->a->txChunks);
}

/* P2PLink_SendSetup works on the game's one link; the self-test drives two of
 * its own, so it needs the same thing pointed at a given one. */
static int32_t P2PLink_SendSetupOn(P2PLink *l, const uint8_t *data, uint32_t len)
{
    if (l->state != P2P_CONNECTED || !data || len == 0 || len > (uint32_t)kBlobMax)
        return 1;
    free(l->txBlob);
    l->txBlob = (uint8_t *)malloc(len);
    if (!l->txBlob)
        return 1;
    memcpy(l->txBlob, data, len);
    l->txLen    = len;
    l->txChunks = (int32_t)((len + kBlobChunk - 1) / kBlobChunk);
    memset(l->txAcked, 0, sizeof l->txAcked);
    l->txLastSendMs = 0;
    return 0;
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

    /* 1b. The delay is AGREED, not chosen twice. Both sides must end up with the
     * same number -- an asymmetric delay is what makes one player feel they are
     * chasing the other (see propose_delay). */
    {
        PairCtx pairBoth = { &host, &joiner };
        pump(pair, 2, 3000, both_have_delay, &pairBoth);
        check(host.delayLocal >= 0 && joiner.delayLocal >= 0, "each side proposes a delay");
        check(host.delayPeer >= 0 && joiner.delayPeer >= 0, "each side hears the other's");
        {
            int32_t a = host.delayLocal > host.delayPeer ? host.delayLocal : host.delayPeer;
            int32_t b = joiner.delayLocal > joiner.delayPeer ? joiner.delayLocal : joiner.delayPeer;
            check(a == b, "both agree the SAME delay");
            if (a != b)
                printf("    host agreed %d, joiner agreed %d\n", a, b);
        }
    }

    /* 1c. The match setup crosses intact. ~40 KB, which is more than the real
     * kick-off state and well past one datagram, so it exercises the chunking,
     * the bitmap ack and the retransmit rather than a happy single packet. */
    {
        enum { kTestBlob = 40000 };
        uint8_t *sent = (uint8_t *)malloc(kTestBlob);
        const uint8_t *got;
        uint32_t gotLen = 0, i;
        PairCtx pairBoth = { &host, &joiner };

        if (sent) {
            for (i = 0; i < kTestBlob; i++)
                sent[i] = (uint8_t)(i * 31u + (i >> 8));      /* not compressible to nothing */
            check(P2PLink_SendSetupOn(&host, sent, kTestBlob) == 0, "the host sends a setup");
            pump(pair, 2, 8000, joiner_has_setup, &pairBoth);
            got = joiner.rxComplete ? joiner.rxBlob : NULL;
            gotLen = joiner.rxLen;
            check(got != NULL, "the joiner receives all of it");
            check(got && gotLen == kTestBlob, "it is the right length");
            check(got && gotLen == kTestBlob && memcmp(got, sent, kTestBlob) == 0,
                  "every byte matches");
            check(bitmap_full(host.txAcked, host.txChunks), "the host sees it acknowledged");
            free(sent);
        }
    }

    /* 1d. NAT traversal: two peers punch to each other through a list that is
     * mostly WRONG. Only one address in each list can work; the rest are
     * unroutable, which is exactly the shape of a real candidate list (a LAN
     * address on the other side of the internet, an old NAT mapping). Both
     * punch at once, as they must -- neither is listening. */
    {
        P2PLink pa, pb;
        P2PLink *both2[2];
        char aAddr[32], bAddr[32];
        const char *toB[3], *toA[3];
        PairCtx pairPunch;

        memset(&pa, 0, sizeof pa); pa.sock = -1; pa.pingMs = -1; pa.joinTimeoutMs = 6000;
        memset(&pb, 0, sizeof pb); pb.sock = -1; pb.pingMs = -1; pb.joinTimeoutMs = 6000;

        /* Bind both first so each knows the port to advertise. */
        check(link_punch(&pa, (const char *const[]){ "192.0.2.1:9" }, 1, 0) == 0,
              "peer A opens a socket and starts punching");
        check(link_punch(&pb, (const char *const[]){ "192.0.2.2:9" }, 1, 0) == 0,
              "peer B opens a socket and starts punching");
        snprintf(aAddr, sizeof aAddr, "127.0.0.1:%u", (unsigned)pa.localPort);
        snprintf(bAddr, sizeof bAddr, "127.0.0.1:%u", (unsigned)pb.localPort);

        /* Now the real lists: two dead ends (TEST-NET-1, never routed) and the
         * one address that works -- deliberately last, so finding it means
         * every candidate was tried rather than the first one happening to be
         * right. */
        toB[0] = "192.0.2.10:9999"; toB[1] = "198.51.100.7:9999"; toB[2] = bAddr;
        toA[0] = "192.0.2.11:9999"; toA[1] = "198.51.100.8:9999"; toA[2] = aAddr;
        check(link_punch(&pa, toB, 3, pa.localPort) == 0, "A punches to B's three candidates");
        check(link_punch(&pb, toA, 3, pb.localPort) == 0, "B punches to A's three candidates");

        both2[0] = &pa; both2[1] = &pb;
        pairPunch.a = &pa; pairPunch.b = &pb;
        pump(both2, 2, 6000, both_connected, &pairPunch);
        check(pa.state == P2P_CONNECTED && pb.state == P2P_CONNECTED,
              "they find each other through the dead ends");
        check(pa.peerText[0] && pb.peerText[0], "each ends up on the address that worked");
        if (pa.state != P2P_CONNECTED || pb.state != P2P_CONNECTED)
            printf("    A %s, B %s\n", state_name(pa.state), state_name(pb.state));
        link_close(&pa);
        link_close(&pb);
    }

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

    /* 5. Nobody there: the join gives up rather than hanging.
     *
     * The empty port comes from the system, released again at once. It used to be the
     * host's port with a bit flipped, but a joiner binds the port it dials (or one of
     * the next kJoinPortTries), and on Windows a derived number can land in a block
     * reserved by Hyper-V or WSL2, where every one of those binds fails and the join
     * never starts. Which block depended on the host's ephemeral port, so the check
     * passed or failed from one run to the next. */
    {
        P2PLink probe;
        memset(&probe, 0, sizeof probe);
        probe.sock = -1;
        open_socket(&probe, 0, 1);
        uint16_t empty = probe.localPort;
        link_reset(&probe);
        P2PLink *one[1] = { &lonely };
        check(empty != 0, "the system has a free port to dial");
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

/* The matchmaking handoff and the agreed input delay. These exist here for the
 * same reason as everything above: online_menus.c and netplay_match.c call them
 * unconditionally, and a build without SWOS_NETPLAY has to link. Missing them is
 * what broke the macOS build (NETPLAY defaults OFF there) while Linux and Cygwin
 * stayed green — so add the stub WITH the function, not after a release. */
int32_t P2PLink_Punch(const char *const *addresses, int32_t count, uint16_t localPort)
{
    (void)addresses; (void)count; (void)localPort;
    return 1;
}
int32_t        P2PLink_AgreedDelay(void)       { return -1; }
int32_t        P2PLink_ProposedDelay(void)     { return -1; }
int32_t        P2PLink_PeerProposedDelay(void) { return -1; }
int32_t        P2PLink_SendSetup(const uint8_t *data, uint32_t len)
{
    (void)data; (void)len;
    return 1;
}
int32_t        P2PLink_SetupSent(void)     { return 0; }
const uint8_t *P2PLink_ReceivedSetup(uint32_t *len) { if (len) *len = 0; return NULL; }
int32_t        P2PLink_SetupProgress(void) { return 0; }

int32_t P2PLink_SelfTest(void)
{
    printf("[p2p-link] self-test: built without netplay (make NETPLAY=1), nothing to test\n");
    return 0;
}

#endif /* SWOS_NETPLAY */
#endif /* SWOS_SDL_BUILD */
