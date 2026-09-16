/*
 * netplay_setup.h -- the match setup one peer sends the other before kick-off.
 *
 * The teams, the user tactics, the match options and the random seed: the thing
 * a kick-off state is DERIVED from, not the state itself. Both peers then run
 * the same deterministic init over it and agree on every frame -- the same way
 * the corpus two-peer test gets two identical machines from one recording.
 *
 * ~3.6 KB. p2p_link chunks it across (include/p2p_link.h, "the match setup").
 */
#ifndef NETPLAY_SETUP_H
#define NETPLAY_SETUP_H

#ifdef SWOS_SDL_BUILD

#include "swos_types.h"

/* Bytes a setup can take; the buffer to hand NetplaySetup_Encode. */
uint32_t NetplaySetup_MaxBytes(void);

/* Pack this machine's current match setup. Returns the size, or -1. */
int32_t  NetplaySetup_Encode(uint8_t *out, uint32_t cap);

/* Adopt a peer's. 0 on success; non-zero leaves this machine's setup alone. */
int32_t  NetplaySetup_Apply(const uint8_t *in, uint32_t len);

/* Round-trip check: encode, scribble over every global it covers, apply, and
 * require equality. Part of --netplay-selftest. 0 on success. */
int32_t  NetplaySetup_SelfTest(void);

#endif /* SWOS_SDL_BUILD */
#endif /* NETPLAY_SETUP_H */
