/*
 * netplay_netem.h -- lag and packet loss on the netplay socket, for testing.
 *
 * A rollback netcode is only exercised by a network that misbehaves: with a
 * clean loopback link every prediction is correct, nothing ever rolls back, and
 * a test that passes has proved almost nothing. This puts the misbehaviour in
 * the process so it can be tested without two machines and a real internet
 * connection between them.
 *
 *   SWOS_NETEM_DELAY_MS=N     hold every outgoing packet N ms (one way)
 *   SWOS_NETEM_JITTER_MS=N    vary that by +/- N ms, so packets REORDER
 *   SWOS_NETEM_LOSS_PCT=N     drop N% of outgoing packets outright
 *   SWOS_NETEM_SEED=N         the emulator's own generator (default: the pid)
 *
 * It sits on the SEND side. A one-way delay applied by both peers is the same
 * thing as delay on the path between them, and dropping on send is the same as
 * dropping in transit -- with the advantage that each peer can be given
 * different conditions, which is how you reproduce one player on a bad line.
 *
 * ITS RANDOMNESS IS ITS OWN, deliberately. It never touches the game's
 * generator: Rand() is match state, both peers must agree on it, and a test
 * harness that perturbed it would desync the match it is trying to test. It is
 * also NOT synchronised between peers -- real networks are not.
 *
 * Off unless an env var sets it, so a normal match pays a single predictable
 * branch and nothing else.
 */
#ifndef NETPLAY_NETEM_H
#define NETPLAY_NETEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct sockaddr;

/* Read the environment. Called once, lazily; safe to call repeatedly. */
void Netem_Init(void);

/* Non-zero when any emulation is configured. */
int  Netem_Enabled(void);

/* Offer a packet to the emulator.
 *   1  the caller should send it itself, now, as usual
 *   0  the emulator has taken it (queued for later, or dropped)
 * Never blocks. */
int  Netem_Send(int sock, const char *buf, int len, int flags,
                const struct sockaddr *dst, int dstlen);

/* Send whatever is now due. Call often -- once per network poll is right. */
void Netem_Pump(int sock);

/* "delay 80ms +/-20 loss 2%" for a log line, or "" when off. */
const char *Netem_Describe(void);

#ifdef __cplusplus
}
#endif

#endif /* NETPLAY_NETEM_H */
