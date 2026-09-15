/*
 * state_wire.c -- the match state as a network packet. See include/state_wire.h.
 */
#include "state_wire.h"
#include "sim_state.h"
#include "endian_helpers.h"
#include "miniz_swos.h"

#include <stdlib.h>
#include <string.h>

static const uint8_t k_magic[4] = { 'S', 'W', 'S', 'T' };

uint64_t StateWire_BufferHash(const uint8_t *buf, uint32_t len)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < len; i++)
        h = (h ^ buf[i]) * 0x100000001b3ull;
    return h;
}

static void write_u64(uint8_t *p, uint64_t v)
{
    BE_WRITE_U32(p, (uint32_t)(v >> 32));
    BE_WRITE_U32(p + 4, (uint32_t)v);
}

static uint64_t read_u64(const uint8_t *p)
{
    return ((uint64_t)BE_READ_U32(p) << 32) | (uint64_t)BE_READ_U32(p + 4);
}

uint32_t StateWire_MaxBytes(uint32_t tiers)
{
    return STATE_WIRE_HEADER_BYTES + (uint32_t)mz_compressBound(SimState_PortableSize(tiers));
}

int32_t StateWire_Encode(uint8_t *out, uint32_t cap, uint32_t tiers, uint32_t frame,
                         const uint8_t *base, uint32_t baseFrame)
{
    uint32_t raw = SimState_PortableSize(tiers);
    uint64_t stateHash, baseHash = 0;
    uint8_t *buf;
    size_t packed;

    if (cap <= STATE_WIRE_HEADER_BYTES)
        return STATE_WIRE_NO_ROOM;
    buf = (uint8_t *)malloc(raw ? raw : 1);
    if (!buf)
        return STATE_WIRE_NO_MEMORY;

    if (SimState_SavePortable(buf, tiers) != 0) {
        free(buf);
        return STATE_WIRE_BAD_STATE;
    }
    stateHash = StateWire_BufferHash(buf, raw);
    if (base) {
        baseHash = StateWire_BufferHash(base, raw);
        for (uint32_t i = 0; i < raw; i++)
            buf[i] ^= base[i];
    }

    /* Raw deflate (negative window bits: no zlib header or checksum -- the header's
     * stateHash is the check). */
    packed = tdefl_compress_mem_to_mem(out + STATE_WIRE_HEADER_BYTES,
                                       cap - STATE_WIRE_HEADER_BYTES, buf, raw,
                                       tdefl_create_comp_flags_from_zip_params(
                                           MZ_BEST_COMPRESSION, -MZ_DEFAULT_WINDOW_BITS,
                                           MZ_DEFAULT_STRATEGY));
    free(buf);
    if (packed == 0)
        return STATE_WIRE_NO_ROOM;

    memcpy(out, k_magic, 4);
    BE_WRITE_U16(out + 4, STATE_WIRE_VERSION);
    BE_WRITE_U16(out + 6, base ? STATE_WIRE_DELTA : 0);
    BE_WRITE_U32(out + 8, tiers);
    write_u64(out + 12, SimState_LayoutHash());
    BE_WRITE_U32(out + 20, frame);
    BE_WRITE_U32(out + 24, base ? baseFrame : frame);
    write_u64(out + 28, baseHash);
    write_u64(out + 36, stateHash);
    BE_WRITE_U32(out + 44, raw);
    BE_WRITE_U32(out + 48, (uint32_t)packed);
    return (int32_t)(STATE_WIRE_HEADER_BYTES + packed);
}

int StateWire_Peek(const uint8_t *packet, uint32_t len, StateWireInfo *info)
{
    StateWireInfo h;

    if (!packet || len < STATE_WIRE_HEADER_BYTES || memcmp(packet, k_magic, 4) != 0)
        return STATE_WIRE_BAD_PACKET;
    h.version     = BE_READ_U16(packet + 4);
    h.flags       = BE_READ_U16(packet + 6);
    h.tiers       = BE_READ_U32(packet + 8);
    h.layout      = read_u64(packet + 12);
    h.frame       = BE_READ_U32(packet + 20);
    h.baseFrame   = BE_READ_U32(packet + 24);
    h.baseHash    = read_u64(packet + 28);
    h.stateHash   = read_u64(packet + 36);
    h.rawBytes    = BE_READ_U32(packet + 44);
    h.packedBytes = BE_READ_U32(packet + 48);
    if (h.version != STATE_WIRE_VERSION || h.tiers == 0 || (h.tiers & ~SIM_TIER_ALL) != 0 ||
        (h.flags & ~STATE_WIRE_DELTA) != 0 || h.packedBytes != len - STATE_WIRE_HEADER_BYTES)
        return STATE_WIRE_BAD_PACKET;
    if (info)
        *info = h;
    return STATE_WIRE_OK;
}

int StateWire_Decode(const uint8_t *packet, uint32_t len, const uint8_t *base,
                     StateWireInfo *info)
{
    StateWireInfo h;
    uint8_t *buf;
    size_t inflated;
    int err = StateWire_Peek(packet, len, &h);

    if (err != STATE_WIRE_OK)
        return err;
    if (info)
        *info = h;
    if (h.layout != SimState_LayoutHash() || h.rawBytes != SimState_PortableSize(h.tiers))
        return STATE_WIRE_WRONG_LAYOUT;
    if ((h.flags & STATE_WIRE_DELTA) &&
        (!base || StateWire_BufferHash(base, h.rawBytes) != h.baseHash))
        return STATE_WIRE_WRONG_BASE;

    buf = (uint8_t *)malloc(h.rawBytes ? h.rawBytes : 1);
    if (!buf)
        return STATE_WIRE_NO_MEMORY;
    inflated = tinfl_decompress_mem_to_mem(buf, h.rawBytes, packet + STATE_WIRE_HEADER_BYTES,
                                           h.packedBytes, 0);
    if (inflated != h.rawBytes) {
        free(buf);
        return STATE_WIRE_CORRUPT;
    }
    if (h.flags & STATE_WIRE_DELTA)
        for (uint32_t i = 0; i < h.rawBytes; i++)
            buf[i] ^= base[i];
    if (StateWire_BufferHash(buf, h.rawBytes) != h.stateHash) {
        free(buf);
        return STATE_WIRE_CORRUPT;
    }
    err = SimState_LoadPortable(buf, h.tiers);
    free(buf);
    return err == 0 ? STATE_WIRE_OK : STATE_WIRE_BAD_STATE;
}

const char *StateWire_ErrorName(int err)
{
    switch (err) {
    case STATE_WIRE_OK:           return "ok";
    case STATE_WIRE_BAD_PACKET:   return "not a state packet";
    case STATE_WIRE_WRONG_LAYOUT: return "made by a different build";
    case STATE_WIRE_WRONG_BASE:   return "delta against a base this peer does not hold";
    case STATE_WIRE_CORRUPT:      return "corrupt";
    case STATE_WIRE_NO_MEMORY:    return "out of memory";
    case STATE_WIRE_NO_ROOM:      return "buffer too small";
    case STATE_WIRE_BAD_STATE:    return "holds a pointer with no identity in this build";
    default:                      return "unknown error";
    }
}
