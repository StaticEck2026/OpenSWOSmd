/*
 * p2p_link.h -- the connection two players make before a netplay match (SDL builds).
 * docs/NETPLAY.md, "The MULTIPLAYER menu".
 *
 * A direct UDP link, no server: one player hosts on a port, the other joins by the
 * host's IPv4 address. The joiner says hello, the host answers, and each side checks
 * the other would play the same match -- the same protocol, the same state registry
 * (SimState_LayoutHash), the same gameplay style, the same screen width -- because a
 * session that disagrees on any of them desyncs on the first frame. While connected
 * both sides ping once a second, so a player sees the round trip before choosing an
 * input delay.
 *
 * This is only the handshake: GGPO opens its own socket for the match itself
 * (src/netplay.c). The link closes before it does, on the same local port, which is
 * also what keeps a NAT mapping open for the session that follows.
 *
 * Without SWOS_NETPLAY (make NETPLAY=1) there is no socket and every call fails with
 * P2P_FAIL_UNAVAILABLE -- the same builds that have no GGPO to play through.
 */
#ifndef P2P_LINK_H
#define P2P_LINK_H

#ifdef SWOS_SDL_BUILD

#include "swos_types.h"

typedef enum {
    P2P_IDLE,        /* no socket */
    P2P_HOSTING,     /* waiting for someone to join */
    P2P_JOINING,     /* saying hello to the host */
    P2P_CONNECTED,
    P2P_FAILED,      /* P2PLink_Failure says why; the socket is closed */
} P2PLinkState;

typedef enum {
    P2P_FAIL_NONE,
    P2P_FAIL_UNAVAILABLE,   /* built without SWOS_NETPLAY */
    P2P_FAIL_BAD_ADDRESS,   /* not a dotted IPv4 address */
    P2P_FAIL_PORT_IN_USE,
    P2P_FAIL_SOCKET,        /* the socket itself could not be opened */
    P2P_FAIL_NO_ANSWER,     /* the host never answered */
    P2P_FAIL_LOST,          /* nothing heard from the other player for too long */
    P2P_FAIL_PEER_LEFT,     /* the other player left the menu */
    P2P_FAIL_HOST_BUSY,     /* the host already has a player */
    P2P_FAIL_PROTOCOL,      /* a different netplay protocol version */
    P2P_FAIL_STATE,         /* a different match-state registry (a different build) */
    P2P_FAIL_GAMEPLAY,      /* a different gameplay style */
    P2P_FAIL_WIDTH,         /* a different screen width */
} P2PLinkFailure;

/* Open the link. Both return 0, or non-zero with the reason in P2PLink_Failure.
 * The joiner binds the same port number when it is free -- on one machine, or behind
 * one router, the next free port above it. */
int32_t        P2PLink_Host(uint16_t port);
int32_t        P2PLink_Join(const char *address, uint16_t port);

/* Once a frame: read what arrived, send what is due, time out. Cheap when idle. */
void           P2PLink_Poll(void);

/* Leave: tell a connected player, close the socket, back to P2P_IDLE. Idempotent. */
void           P2PLink_Close(void);

P2PLinkState   P2PLink_State(void);
P2PLinkFailure P2PLink_Failure(void);

/* What happened to the last player who tried, while hosting continues: one who was
 * turned away (a mismatch, or the host was busy), or the one who left or went quiet.
 * P2P_FAIL_NONE once hosting starts again. */
P2PLinkFailure P2PLink_Notice(void);

/* The other player as "a.b.c.d:port", or "" before there is one. */
const char    *P2PLink_PeerText(void);
/* The port actually bound, which is not the port asked for when that one was taken. */
uint16_t       P2PLink_LocalPort(void);
/* The round trip in milliseconds, or -1 until one has been measured. */
int32_t        P2PLink_PingMs(void);

/* Whether `text` is a dotted IPv4 address -- what P2PLink_Join takes. */
int32_t        P2PLink_IsAddress(const char *text);

/* Two links over loopback: they connect, ping, refuse a mismatched game, notice a
 * player leaving, turn away a second one, and give up on a host that is not there.
 * Prints a report; 0 on success. Part of `--netplay-selftest`. */
int32_t        P2PLink_SelfTest(void);

#endif /* SWOS_SDL_BUILD */
#endif /* P2P_LINK_H */
