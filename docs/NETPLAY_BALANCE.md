# NETPLAY_BALANCE.md — sharing the guessing between the two players

> **Status (2026-10-05): built, on by default.** `[netplay] balance = false` (or
> `SWOS_LIVE_NO_BALANCE=1`) turns it off. It is `balance_*` in `src/netplay_match.c`.
> The first prototype (2026-09-30, below) balanced on average but swung between the players
> every 10-20 s and was rolled back; "What was built" says what is different. Not yet
> played between two real machines.

## What was built

Each peer knows its own lag exactly (it is what PRED records), so the two tell each other
and the one guessing more waits a little. Only wall time changes; the simulation, the
rollback state and the recordings are untouched.

- **Measure.** At every frame's first play this peer's lag (capped at 15) goes into a
  512-frame history.
- **Report.** Every 50 frames (a second) it sends the average over the last 90 frames by
  GGPO-X's chat (`Netplay_Chat`): `SWLAG <last frame of the window> <hundredths>`. Chat is
  keyed like every packet and not resent, which suits a figure sent again every second.
- **Compare like with like.** On the other's report a peer compares it with its OWN average
  over the SAME 90 frames, waiting until it has played them if it has not yet. The
  prototype compared windows a period apart, which is what made the two peers believe in
  turn that they were ahead.
- **Act.** When this peer guesses more by 0.6 of a frame, it waits a quarter of the
  difference, spread over the next 50 frames (waiting a quarter of a frame takes a quarter
  off its lag and adds a quarter to the other's: the difference halves), and learns an
  eighth of that as a standing slowdown. The other peer does nothing; when it is the one
  guessing more, any standing slowdown here eases off.
- **Then wait for a clean window.** No new correction until a report whose 90 frames were
  all played after the last correction was complete. Lag is whole frames -- 2/0, 1/1 or
  0/2 on a link a frame apart -- and a correction shows in the average only once the
  window has moved past it. Without this rule the second version walked from 2/0 through
  1/1 to 0/2 and back every eight seconds.
- **GGPO's time sync** is not acted on while the other's reports are fresh (under 6 s old)
  and its figure is under a frame and a half: it assumes the two directions take the same
  time, so on a lopsided route it pulls the other way. Past a frame and a half it is one
  machine running away from the other, and the time sync closes that as before. With no
  reports -- an older build, or balance off at the other end -- it is the time sync alone.

### Measured (2026-10-05)

The lopsided link of "What the tests showed" (the host's outgoing packets 100 ms late, 4,000
frames, both chasing the ball), each player's lag from the host's `.SRP`, from frame 1000:

| | P1 lag | P2 lag | per 2 s |
|---|---|---|---|
| balance off | 1.58 | 1.01 | 2.0/0.1 for 32 s, then 0.6/4.4, 0.0/6.1, ... 3.6/0.0, 4.0/0.0, ... 0.0/3.0 |
| balance on | 1.00 | 1.07 | 1.0/1.1 or 1.0/1.0 in 28 of 30 slices (0.6/1.6 and 1.5/0.6 the other two) |

Three or four corrections a peer in 80 seconds, 0.1 to 0.2 s of waiting in all.

Other cases, `scripts/netplay_lobby_test.sh`, 3,000 frames, re-simulated frames host / joiner:

| | off | on |
|---|---|---|
| an even local link | 277 / 196 | 241 / 72 |
| the joiner's clock 2% slow | 162 / 311 | 124 / 349 |

Still to do: a match between two real machines, and the testers' word on whether the
player who used to guess more notices.

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

## The first prototype (2026-09-30, rolled back)

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

## Prior art: Fightcade (`fightcade-snes9x`)

Fightcade's Snes9x fork (`D:\Projects\GitHub\fightcade-snes9x`, commit `d56c95d`) runs on the
original, closed GGPO (`ggponet.dll`), and does its own pacing — "rift balancing" — in about
25 lines of `src/win32/ggpo/ggpo.cpp` (`QuarkIncrementFrame`) and `src/win32/wsnes9x.cpp`
(`ProcessFrameDelay`, and the frame loop that calls it). What it does:

1. **It ignores GGPO's time sync entirely.** `case GGPO_EVENTCODE_TIMESYNC: // GGPO timesync is
   bad` — nothing else. Every correction comes from its own loop (our prototype did the same,
   gating GGPO's corrections while its own reports were fresh).
2. **It measures after every frame, from GGPO's stats.** `ggpo_get_stats` gives
   `timesync.local_frames_behind` and `remote_frames_behind` (integers: how many frames this
   peer, and the other, believes it is behind). A rolling average over the last **90 frames**
   of `local - remote`, halved, is the "rift" — in thousandths of a frame
   (`FramesBehind.average * 1000 / 2`). Using the DIFFERENCE of the two peers' own views, halved,
   cancels any bias the two share — the same formula as GGPO-X's
   `recommend_frame_wait_duration`.
3. **It acts at most once a second, and only past a frame.** `if (abs(rift) > 1000)` and one
   second since the last correction; the rift is clamped to ±5 frames.
4. **Both peers act — in opposite directions.** "one to go slightly faster, the other slightly
   slower! win win": `ProcessFrameDelay(-rift)`:
   - the peer **ahead** delays its next frame once, by the rift in milliseconds
     (`nFrameLast += ms`, `ms = thousandths / 60`: one frame of rift = ~16.7 ms at 60 fps);
   - the peer **behind** runs extra frames at once, unshown, to catch up (`nSkipFrames =
     2 * rift / 1000`, played back to back before the next shown frame).
5. **Its frame loop** is a plain 60 fps accumulator: `ggpo_idle(ggpo, 1)` on every pass while
   waiting for the next frame's time, one `ProcessFrame` (input, `S9xMainLoop`,
   `ggpo_advance_frame`, then the rift check) per 16.7 ms, catching up to 10 frames if late.

What that means for us:

- **Its signal is GGPO's own estimate**, so it corrects a CLOCK drift or a rift between the
  machines, but would not see the asymmetric-path split this doc is about: original GGPO
  estimates the remote frame the same way (last input + half the round trip). Fightcade
  players on a lopsided route would get the same one-sided guessing. For the split, our
  measured lag (PRED's figure, exchanged) is still the signal to use.
- **Its structure is what ours lacked, and is the dial-in:**
  - a **rolling** average over 90 frames, updated every frame — not a 100-frame block
    compared against the other peer's previous block, which is what made our two peers
    believe in turn that they were ahead;
  - a **deadband of a whole frame** (ours acted from half a frame) and **at most one
    correction a second**, which is what keeps it from hunting;
  - **both sides move**, so each needs only half the correction. Note that its "behind" side
    runs `2 * rift` frames and its "ahead" side waits `rift`, so together they correct 1.5x
    the rift each time — an overshoot its rate limit and long average absorb. Ours should
    split it evenly: the peer guessing more waits a quarter of the lag difference, the other
    catches up a quarter.
  - Catching up by **running frames unshown** is cheap in an emulator. Ours would do it by
    shortening the frame limiter's next waits (`Platform_DelayNextFrame` with a credit), not by
    skipping presents, so the picture stays smooth.
- **So, for the next attempt (which is "What was built", above):** our lag signal, Fightcade's controller. Per frame, each peer
  keeps a 90-frame rolling average of its own lag; it sends the average every second (chat
  `SWLAG`, tagged with the frame it ends at); on receiving the other's, it compares it with
  its OWN average ending at the same frame; if `|mine - theirs| / 2` is over one frame, the
  one guessing more waits a quarter of the difference, the other speeds up a quarter; at most
  once a second. Then test against the emulated lopsided link (above) and look for the swing.

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
