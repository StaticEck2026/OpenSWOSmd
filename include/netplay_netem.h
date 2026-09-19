/*
 * netplay_netem.h -- a misbehaving network on the netplay socket, for testing.
 *
 * A rollback netcode is only exercised by a network that misbehaves: with a
 * clean loopback link every prediction is correct, nothing ever rolls back, and
 * a test that passes has proved almost nothing. This puts the misbehaviour in
 * the process so it can be tested without two machines and a real internet
 * connection between them.
 *
 * The set of conditions is the one clumsy (jagt.github.io/clumsy) offers, which
 * is the set a real link actually produces -- lag, drop, throttle, out of order,
 * duplicate, tamper. Its seventh, "set TCP RST", has no meaning here: GGPO's
 * transport is UDP, and the thing it models (the connection cut from under you)
 * is a throttle window with SWOS_NETEM_THROTTLE_DROP.
 *
 *   SWOS_NETEM_PROFILE=name    a whole link in one word (see the table below)
 *
 *   SWOS_NETEM_DELAY_MS=N      hold every packet N ms (one way)   -- clumsy: lag
 *   SWOS_NETEM_JITTER_MS=N     vary that by +/- N ms
 *   SWOS_NETEM_LOSS_PCT=N      drop N% of packets outright        -- clumsy: drop
 *   SWOS_NETEM_REORDER_PCT=N   N% of packets are held back behind the ones sent
 *                              after them                  -- clumsy: out of order
 *   SWOS_NETEM_REORDER_MS=N    how far back (default 20)
 *   SWOS_NETEM_DUP_PCT=N       N% of packets are also sent again  -- clumsy: duplicate
 *   SWOS_NETEM_DUP_COUNT=N     how many copies in total (default 2)
 *   SWOS_NETEM_THROTTLE_PCT=N  N% of packets open a window during which nothing
 *                              is sent; at its end the lot goes out in one
 *                              burst                              -- clumsy: throttle
 *   SWOS_NETEM_THROTTLE_MS=N   how long that window lasts (default 200)
 *   SWOS_NETEM_THROTTLE_DROP=1 the window drops what it holds instead of
 *                              bursting it: a link that was simply gone
 *   SWOS_NETEM_TAMPER_PCT=N    N% of packets have bits flipped in them -- clumsy: tamper
 *   SWOS_NETEM_TAMPER_BITS=N   how many bits (default 1)
 *   SWOS_NETEM_SEED=N          the emulator's own generator (default: the clock)
 *
 * Profiles are the shape of a link, not a measurement of one. Every knob above
 * still overrides what the profile set, so `SWOS_NETEM_PROFILE=wifi
 * SWOS_NETEM_LOSS_PCT=10` is wifi with the loss turned up.
 *
 *   lan        2ms          a switch between two machines
 *   adsl       30ms +/-6    1% loss, a little reordering
 *   wifi       20ms +/-15   1% loss, and a stall now and then -- the interesting one
 *   mobile     70ms +/-40   2% loss, longer stalls
 *   satellite  300ms +/-25  1% loss: latency alone, and a lot of it
 *   awful      150ms +/-80  8% loss, duplicates, reordering, long stalls
 *
 * TAMPER IS A FUZZ KNOB, NOT A LINK. UDP carries a checksum and the hardware
 * drops what fails it, so a flipped bit reaching the game is rare; what it tests
 * is whether a corrupt packet is refused rather than acted on. Expect a session
 * to die under it -- that is a result, not a failed test, and it is why no
 * profile turns it on.
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
 * Off unless the environment sets it, so a normal match pays a single
 * predictable branch and nothing else.
 *
 * The rules and the profile table are include/netem_core.h, shared with the
 * matchmaking server's relay (third_party/matchmaker, `--netem`), which applies
 * the same conditions in the middle of a relayed match. This one stays: most
 * matches are peer to peer, and only an emulator on the peer can disturb those.
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
 *   0  the emulator has taken it (sent it already, queued it, or dropped it)
 * Never blocks. */
int  Netem_Send(int sock, const char *buf, int len, int flags,
                const struct sockaddr *dst, int dstlen);

/* Send whatever is now due. Call often -- once per network poll is right. */
void Netem_Pump(int sock);

/* "delay 80ms +/-20, loss 2%" for a log line, or "" when off. */
const char *Netem_Describe(void);

/* What it actually did: "1204 packets: 23 dropped, ..." -- the counterpart to
 * Describe(), and the only proof a condition ever fired. "" when off. */
const char *Netem_Summary(void);

#ifdef __cplusplus
}
#endif

#endif /* NETPLAY_NETEM_H */
