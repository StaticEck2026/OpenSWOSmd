/*
 * state_wire.h -- the match state as a network packet: what a peer receives to resync
 * or to join a match in progress (docs/ROLLBACK.md, "The wire format").
 *
 * Binary, not JSON. A packet is read only by the other peer of the same session, never
 * stored, and being small is its whole purpose.
 *
 *   payload  SimState_SavePortable: the registry regions of the chosen tiers, in table
 *            order, in the portable encoding -- typed fields big-endian, every pointer
 *            as the identity of what it points at -- so any build reads it: 32- or
 *            64-bit, Linux, Windows or macOS
 *   delta    optionally XORed with a BASE state the receiver holds too -- the kick-off
 *            state both peers compute, or the last frame whose checksums they agreed
 *            on -- which turns everything that has not changed into zeros
 *   packing  raw deflate (miniz)
 *
 * The header says enough for a packet to be refused rather than misread: the registry
 * layout it was made against (SimState_LayoutHash), the hash of the base it was made
 * from, and the hash of the state it rebuilds. Nothing is loaded unless all three check
 * out.
 *
 * Everything travels big-endian (network byte order).
 */
#ifndef STATE_WIRE_H
#define STATE_WIRE_H

#include "swos_types.h"

#define STATE_WIRE_VERSION       1
#define STATE_WIRE_HEADER_BYTES  52
#define STATE_WIRE_DELTA         0x0001u    /* the payload was XORed with a base state */

typedef struct {
    uint16_t version;
    uint16_t flags;         /* STATE_WIRE_DELTA */
    uint32_t tiers;         /* SIM_TIER_* */
    uint64_t layout;        /* SimState_LayoutHash() of the sender */
    uint32_t frame;         /* the frame the state belongs to */
    uint32_t baseFrame;     /* the base's frame; delta only */
    uint64_t baseHash;      /* FNV-1a of the base (portable bytes); delta only */
    uint64_t stateHash;     /* FNV-1a of the rebuilt state (portable bytes) */
    uint32_t rawBytes;      /* SimState_PortableSize(tiers) */
    uint32_t packedBytes;   /* deflated payload following the header */
} StateWireInfo;

enum {
    STATE_WIRE_OK           =  0,
    STATE_WIRE_BAD_PACKET   = -1,   /* truncated, not a state packet, or an unknown version */
    STATE_WIRE_WRONG_LAYOUT = -2,   /* made by a build with another registry */
    STATE_WIRE_WRONG_BASE   = -3,   /* a delta against a base this receiver does not hold */
    STATE_WIRE_CORRUPT      = -4,   /* does not inflate, or rebuilds a state that hashes wrong */
    STATE_WIRE_NO_MEMORY    = -5,
    STATE_WIRE_NO_ROOM      = -6,   /* the output buffer is too small */
    STATE_WIRE_BAD_STATE    = -7    /* holds a pointer with no identity, or names one this build lacks */
};

/* FNV-1a over a portable buffer: equals SimState_Hash(tiers) of the state it holds. */
uint64_t    StateWire_BufferHash(const uint8_t *buf, uint32_t len);

/* Upper bound on the size of a packet of these tiers. */
uint32_t    StateWire_MaxBytes(uint32_t tiers);

/* Pack the live state of `tiers` for `frame`. With `base` (SimState_PortableSize(tiers) bytes of
 * portable state, from SimState_SavePortable at `baseFrame`) the packet is a delta
 * against it; with NULL it stands alone. Returns the packet size (> 0), or a negative
 * STATE_WIRE_* error. */
int32_t     StateWire_Encode(uint8_t *out, uint32_t cap, uint32_t tiers, uint32_t frame,
                             const uint8_t *base, uint32_t baseFrame);

/* Read and validate a packet's header without applying it. */
int         StateWire_Peek(const uint8_t *packet, uint32_t len, StateWireInfo *info);

/* Apply a packet to the live state. `base` is the receiver's portable state of the
 * packet's baseFrame, for the tiers the packet carries (ignored for a packet that stands
 * alone). On any error the live state is untouched. `info` may be NULL. */
int         StateWire_Decode(const uint8_t *packet, uint32_t len, const uint8_t *base,
                             StateWireInfo *info);

const char *StateWire_ErrorName(int err);

#endif /* STATE_WIRE_H */
