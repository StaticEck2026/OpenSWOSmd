# NETPLAY_BALANCE.md — sharing the guessing between the two players (parked)

> **Status (2026-09-30): investigated and prototyped, then rolled back — parked for later.**
> A prototype worked on average but swung between the players every 10–20 s; it was not
> worth delaying a tester drop for. This is everything needed to pick it up again. Nothing
> of it is in the code.

## The problem

Under rollback, a player whose opponent's input arrives late plays on a **guess** (GGPO-X
repeats the last input it has) and rolls back when the real input comes. The recordings
keep how late, per frame per player: PRED's **lag** (docs/REPLAY_FORMAT_V2.md §4 "What each
player saw"), drawn as the playback page's timeline.

On real matches the timeline is lopsided: one player guesses on **every** frame for minutes
while the other almost never does, then it flips.

### The evidence

`20260930-185127-e6865849.SRP` (peer to peer, input delay 3), average lag per player over
30-second slices:

| frames | P1 | P2 | sum |
|---|---|---|---|
| 0–1499 | 1.1 | 1.5 | 2.6 |
| 1500–2999 | 0.3 | 0.9 | 1.2 |
| 3000–5999 | 0.0 | 1.1 | 1.1 |
| 7500–12523 | 1.0 | (no data) | — |

- **The sum is steady** (≈1 frame after the first minute): it is set by the connection —
  one-way travel time less the agreed input delay, both ways added.
- **The split moves**: it depends on which machine is a fraction of a frame ahead. Here P2
  carried all of it through the middle, P1 later.
- **GGPO reported them level the whole time.** Both players' `SWOS.log` 30-second lines
  (from the match report zips) for a match like it: player 1 "−1.1 frames ahead", player 2
  "+0.1", steady; ping 205 ms.
- Across 23 real matches' reports: **player 2 (the joiner) hits the prediction limit**
  64–559 times a match, player 1 ~0; the time sync asked for only 2–15 waits a match
  (0.1–1.4 s in all), and the learned slowdown ended at 0.
- None of those matches was relayed, so the lopsided path is not our relay — either the
  route between the players, or a delay inside one machine before its input is sent (a
  frame loop that presents before it idles, say). The data here cannot tell them apart.

### Why GGPO's time sync cannot see it

`third_party/ggpo-x/src/lib/ggpo/network/udp_proto.cpp` estimates the other peer's frame as
its last input's frame + **half the round trip** (in frames) + 1, and the local advantage as
that minus our frame; `timesync.cpp` averages both peers' figures and recommends
`-(remote + local) / 2`. The half-round-trip assumption is the flaw: on a path where one
direction takes longer, both machines agree they are level while one player's inputs arrive
a frame or more later than the other's. The lead that flips the split is about one frame —
within that estimate's error, and our handler (`netplay_match.c`, `NETPLAY_EVENT_TIMESYNC`)
ignores anything under a quarter of a frame anyway. (Also noted: `TimeSync::_remoteFrameDelay`
is never set, and the quality report sends the advantage as `(uint8)(x * 10)`, which a
negative advantage wraps.)

**It is milder than it looks:** a lag of 1 is one frame of the opponent's input guessed —
usually invisible. The timeline looks lopsided because the whole shortfall lands on one
player at a time instead of being shared. Sharing it is fairness, not fewer rollbacks.

## The prototype (rolled back)

Each peer knows its own lag exactly (it is what PRED records). So:

1. **Measure**: sum the lag at every frame's first play (`frame - confirmed`, capped at 15)
   over a period of `BALANCE_PERIOD = 100` frames (2 s).
2. **Report**: send the average by GGPO-X's chat (`ggpo_client_chat`, wrapped as
   `Netplay_Chat`; it arrives as `NETPLAY_EVENT_CHAT`) as `"SWLAG <hundredths>"`. Chat is
   keyed like every GGPO packet and unreliable, which suits a figure resent every period.
3. **Act**: `lead = (mine - theirs) / 2` — how far past the point where both guess alike.
   If this peer is ahead by more than half a frame, wait half the lead, spread over the next
   period (`s_np.paceUsPerFrame`, `paceFramesLeft`), and nudge the learned standing slowdown
   (`s_np.paceBiasUs`) as the time sync does. The peer behind does nothing.
4. **Own the pacing** while the other's reports are fresh (< 6 s): GGPO's own time-sync
   corrections are only logged, since they pull the other way; without reports (an older
   peer, a dead link) the time sync takes over again.
5. Only wall time changes — the simulation never sees any of it, so rollback and
   determinism are untouched.

The code, as it was (in `src/netplay_match.c`; `Netplay_Chat` in `src/netplay.c` beside
`Netplay_NetStats`, a stub in the `!SWOS_NETPLAY` half):

```c
enum { BALANCE_PERIOD = 100, BALANCE_STALE_MS = 6000 };
/* s_np: lagSum, lagN, myLagX100 (smoothed), peerLagX100, peerLagMs, noBalance,
 *       balanceWaits, balancePeriods */

static int32_t balance_fresh(uint32_t now)
{
    return !s_np.noBalance && s_np.peerLagMs && now - s_np.peerLagMs < BALANCE_STALE_MS;
}

static void balance_period(uint32_t now)
{
    char msg[32];
    int32_t avg = s_np.lagN ? s_np.lagSum * 100 / s_np.lagN : 0;
    s_np.myLagX100 = s_np.balancePeriods++ ? (s_np.myLagX100 + avg) / 2 : avg;  /* smoothed */
    s_np.lagSum = s_np.lagN = 0;
    snprintf(msg, sizeof msg, "SWLAG %d", (int)s_np.myLagX100);
    Netplay_Chat(s_np.session, msg);
    if (!balance_fresh(now) || s_np.noPacing)
        return;
    int64_t frameUs = 1000000 / DosGameplay_TargetFps();
    int32_t leadX100 = (s_np.myLagX100 - s_np.peerLagX100) / 2;
    if (leadX100 < 50) {                        /* even enough, or behind */
        s_np.paceBiasUs -= s_np.paceBiasUs / 8; /* the learned slowdown eases off */
        return;
    }
    int64_t close = frameUs * leadX100 / 2 / 100 / BALANCE_PERIOD;   /* half the lead */
    s_np.paceBiasUs += close / 4;
    if (s_np.paceBiasUs > frameUs / 10)
        s_np.paceBiasUs = frameUs / 10;
    s_np.paceUsPerFrame = (int32_t)close;
    s_np.paceFramesLeft = BALANCE_PERIOD;
    s_np.balanceWaits++;
}
/* at each frame's first play (live, after Ghost_Lag):
 *     s_np.lagSum += lag < 15 ? lag : 15;
 *     if (++s_np.lagN >= BALANCE_PERIOD) balance_period(now);
 * NETPLAY_EVENT_CHAT "SWLAG n": s_np.peerLagX100 = n; s_np.peerLagMs = SDL_GetTicks();
 * NETPLAY_EVENT_TIMESYNC: if (balance_fresh(now)) break;  -- before it acts
 * SWOS_LIVE_NO_BALANCE=1 turned it off, for comparing; a "[balance]" line at the match end
 * and "lag x here, y there" on the 30-second stats line. */
```

## What the tests showed

A lopsided link in miniature: peer to peer, only the host's outgoing packets delayed
100 ms by the game's own emulator (`SWOS_NETEM_DELAY_MS` delays a process's *outgoing*
packets), `SWOS_NETEM_LINK=1` so the input delay is agreed from the emulated ping (it came
out 2), both players chasing the ball. Each player's lag read from the host's `LATEST.SRP`
(both halves, after the lobby swap), from 20 s on:

```bash
RECORD=1 RELAY=0 MATCHES=1 FRAMES=4000 \
  HOST_ENV="SWOS_NETEM_DELAY_MS=100 SWOS_NETEM_LINK=1 SWOS_LIVE_CHASE_INPUT=1" \
  JOIN_ENV="SWOS_LIVE_CHASE_INPUT=2" scripts/relay_test.sh
# add SWOS_LIVE_NO_BALANCE=1 to both for the "off" run
```

| | P1 lag | P2 lag | frames guessed P1 / P2 | per 10 s (P1/P2) |
|---|---|---|---|---|
| balance off | 0.57 | 2.33 | 31% / 81% | 1.7/2.1 0.9/2.4 1.0/1.7 0.0/3.6 1.2/1.1 0.0/2.8 0.4/2.4 |
| balance on | 1.30 | 1.28 | 57% / 57% | 3.5/0.0 0.5/1.9 0.7/1.8 2.7/0.1 0.1/2.5 1.5/1.0 2.3/0.4 |

(A first version — no smoothing, closing the whole lead each period — was worse: 3.66 /
3.42 overall, swinging 4.2/2.8 ↔ 2.9/4.0, and it raised the total by about a frame, since
both peers took turns believing they were ahead and slowing.)

**Even on average, but not steady**: every 10–20 s the whole lag still swings to one side.
The balance equalises the long-run split without holding it.

## What to try next

- **Compare like with like.** The two averages are from different windows: ours is fresh,
  theirs a period old when it arrives. Tag each report with its frame range and compare
  against our own average over the same frames (keep a short per-frame history).
- **A proper controller.** A small PI controller on `lead`, with a longer period (5–10 s)
  and a deadband, rather than bang-bang half-steps. Log `lead` per period and tune against
  the emulated lopsided link, then against a real one.
- **Measure the asymmetry directly.** Each peer can know how late the *other* receives its
  inputs if the other reports it — which is what `SWLAG` is — so the one-way difference is
  `(theirLag - myLag)/2 + the offset`. Estimating the offset from GGPO's advantage and the
  asymmetry from the lags separately may converge more cleanly than acting on the lag gap.
- **Or ask GGPO-X to do it.** The time sync is the right place: its estimate could weight
  the two directions by measured input lateness instead of assuming half the round trip.
  This doc and the recordings are the data for that conversation with the GGPO-X side.
- **Decide whether it matters.** Rollbacks and total guessing do not change — only who
  carries them. Worth asking testers whether the player who guesses more notices.

Before shipping any version: `scripts/netplay_test.sh --all`, `netplay_lobby_test.sh`,
`RECORD=1 scripts/relay_test.sh` (both peers' `.SRP` byte-identical, resims pass), and a
real two-machine match.
