/*
 * relay_proto.h -- the matchmaking server's relay, as both ends see it.
 *
 * A relayed match is two players who each send their game packets to the SERVER
 * instead of to each other; the server forwards every datagram, unchanged, to the
 * other player, from the same port it arrived on. Neither the pre-match link
 * (src/p2p_link.c) nor GGPO can tell: each sees one peer, at one address, and
 * that address happens to be the server's. So the relay needs nothing from the
 * packets it carries -- no header on them, no change to GGPO.
 *
 * What it does need is to know who is who, because a relay that forwarded for
 * anybody would be an open amplifier. At START the server makes a relay session
 * with a random token and tells each OpenSWOS player "RELAY:<token hex>,<n>"
 * over the matchmaking connection, just before START. Each player then REGISTERS
 * from the socket it will play on; the server binds that source address to the
 * session, and forwards only between the two registered addresses. Registering
 * is repeated with every punch round, which also keeps the NAT mapping open.
 *
 * Why a relay at all when matches are peer to peer: testing (the server can put
 * a misbehaving network in the middle of a real match between two real machines,
 * `--netem`), the browser build (no UDP, so no peer-to-peer -- phase 2 carries the
 * same sessions over WebSocket), and players whose NATs will not punch.
 *
 * It shares UDP 19602 with the matchmaker (ENet's intercept hook), so a server
 * needs no second port forwarded. A relay packet is recognised by the magic AND
 * its exact length AND a token the server issued -- a datagram that is anything
 * less is left to ENet.
 *
 * Control packet, 16 bytes, big-endian:
 *
 *   0   4  "SWRL"
 *   4   1  RELAY_PROTOCOL
 *   5   1  type        RELAY_REGISTER / RELAY_REGISTERED
 *   6   1  player      1 or 2
 *   7   1  status      RELAY_OK / RELAY_UNKNOWN / RELAY_BAD_PLAYER (REGISTERED only)
 *   8   8  token
 */
#ifndef RELAY_PROTO_H
#define RELAY_PROTO_H

#include <stdint.h>
#include <string.h>

#define RELAY_MAGIC        "SWRL"
#define RELAY_PROTOCOL     1
#define RELAY_CONTROL_LEN  16

#define RELAY_REGISTER     1       /* player -> server: this address is me */
#define RELAY_REGISTERED   2       /* server -> player: and here is what I made of it */

#define RELAY_OK           0
#define RELAY_UNKNOWN      1       /* no session with that token (expired, or never) */
#define RELAY_BAD_PLAYER   2       /* not 1 or 2 */

/* The matchmaking message that hands a player the token, sent just before START
 * and only to OpenSWOS clients (a stock SWOS 2020 client's parser throws on a
 * header it does not know). */
#define RELAY_MM_HEADER    "RELAY:"
/* The same for a session to FALL BACK to: the players punch direct and move to the
 * relay only if that gets no round trip (p2p_link.c, relay_fallback). A header of its
 * own, so a game that predates it skips it as unknown and plays as it always did. */
#define RELAY_FALLBACK_MM_HEADER "RELAYFB:"

typedef struct {
    uint8_t  type;
    uint8_t  player;
    uint8_t  status;
    uint64_t token;
} RelayControl;

static inline void Relay_WriteControl(uint8_t *p, const RelayControl *c)
{
    int i;
    memcpy(p, RELAY_MAGIC, 4);
    p[4] = RELAY_PROTOCOL;
    p[5] = c->type;
    p[6] = c->player;
    p[7] = c->status;
    for (i = 0; i < 8; i++)
        p[8 + i] = (uint8_t)(c->token >> (56 - 8 * i));
}

/* 0 when `p` is a control packet of this protocol, filled into `c`. */
static inline int Relay_ReadControl(const uint8_t *p, int32_t len, RelayControl *c)
{
    int i;
    if (len != RELAY_CONTROL_LEN || memcmp(p, RELAY_MAGIC, 4) != 0 || p[4] != RELAY_PROTOCOL)
        return 1;
    c->type   = p[5];
    c->player = p[6];
    c->status = p[7];
    c->token  = 0;
    for (i = 0; i < 8; i++)
        c->token = (c->token << 8) | p[8 + i];
    return 0;
}

#endif /* RELAY_PROTO_H */
