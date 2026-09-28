# NETPLAY_GHOSTING.md — players running through each other online

"Ghosting" is what testers call players gliding through each other in an online match
with no tackle: "you can just walk through your opponents". SWOS 2020's netplay has had it
for years, and it appeared in OpenSWOS's first multiplayer weekend (2026-09-27, CROM v
MLADEN, "BALL CONTROL SECOND HALF"). This is what is known, what the game now records
about it, and how to reproduce it without testers.

## Two possible causes

| | what happens | what it looks like |
|---|---|---|
| **Rollback** | When the other player's input is late the game guesses it (GGPO repeats their last input), draws that frame, and corrects it when the input arrives. What was drawn and what happened differ, while both machines still end on the same state. | An opponent drawn where he was not: a contact shown that never happened, or one that happened and was never shown; possession flickering |
| **The game itself** | SWOS has no bodies, only the ball: two players can stand on the same spot, and a challenge wins nothing unless the ball is reached. An opponent can run straight through the man on the ball. | The same, but in the match as really played, on both screens |

Every determinism test we have passes under the first -- both peers agree on every frame
-- which is why none of them ever saw it.

## What is known (2026-09-28)

- **Not a port bug in the rules.** The form roll (`AdjustPlayerSkills`, `ScaleSkill`,
  `GetPlayerPrice`) and the ball contest (`CalculateIfPlayerWinsBall`) match `SWOS2.s`
  instruction for instruction; the contest reproduced all 186 in the corpus identically in
  Amiga, DOS and SWOS 2020 (2026-09-17).
- **Mladen and Crom's worst half had no rollback at all.** Their ping fell to 60 ms at
  about 3:30 and both logs show 0 rollbacks from 4:00 to the end, with the input delay at
  2 frames (40 ms) -- so what they saw was the match as played. No desync either.
- **Simulated, rollback distortion is small at real links** (below). At their link (about
  100 ms, delay 2) 2-6% of frames were drawn differently, by at most 8 px, and there were
  no rollback ghosts. On a mobile-like link 9-11% of frames, up to 28 px, and 0-2 ghosts a
  minute of 1-3 frames each -- short flickers, not a player walking through another.

So the leading suspect is the game itself -- possibly made more frequent online by the
2-3 frames of input delay making challenges harder to time -- and the evidence to settle it
is testers' marks (below).

| link (one way) | ping | delay | frames drawn wrong | worst | possession wrong | rollback ghosts | contacts never drawn |
|---|---|---|---|---|---|---|---|
| 48 ms +/-5 (Mladen-Crom) | 100 | 2 | 2-6% | 8 px | 0-4 | 0 | 0 |
| 48 ms +/-25 | 91-98 | 2 | 0-12% | 24 px | 0-7 | 0 | 0-1 |
| 80 ms +/-20 | 153-167 | 3 | 11% | 19 px | 6-7 | 0 | 0-1 |
| 100 ms +/-40, 3% loss | 223 | 3 | 11-17% | 22 px | 11-18 | 0-2 | 0-1 |
| `wifi` | 41-60 | 1 | 5.5% | 20 px | 3-5 | 0 | 0-1 |
| `mobile` | 124-141 | 2 | 9-10% | 28 px | 11-17 | 1-2 | 0-4 |

One minute each, two ball-chasing players, the delay agreed from each link's own ping;
each cell is the two players' range. A player is about 16 px tall.

## What every online match records

`src/netplay_ghost.c` records each frame twice -- as this player drew it, and as finally
played -- and at the end of the match (an early end too) writes `[ghost]` lines to SWOS.log:

```
[ghost] player 2: 3000 frames, 339 drawn differently from the final game (11.3%); players drawn up to 28 px ...
[ghost] player 2: rollback ghosts: 1 drawn touching near the ball and 12+ px apart in the final game ...
[ghost] player 2: 1 walk-throughs in the final game -- an opponent on top of the man on the ball for 4+ frames who did not win it
[ghost]   0:31 (frame 1599), 4 frame(s): team 1 #2 and team 2 #8 walked through the man on the ball, won nothing ...
[ghost] MARK 1 at 0:20 (frame 1001): in the 3 s before it, 16 frame(s) drawn differently ..., players up to 12 px off
[ghost]   nothing: no rollback ghost and no walk-through near it -- what was seen happened in the match as played
```

- **Rollback ghosts**: opponents drawn touching near the ball who were 12+ px apart in the
  real match ("drawn touching, never did"), or the reverse ("touched, never drawn").
- **Walk-throughs**: in the real match, an opponent on top of the man on the ball for 4+
  frames who did not win it. Plenty are ordinary SWOS; they matter next to a mark.
- **Jumps**: a player drawn 24+ px from where he really was -- he jumps when the
  correction arrives. And `the worst:` names the frame and player of the single largest
  drawing error of the match.
- **Marks**: see below.

**The windows.** Around every mark, the 3 longest detected events and the 2 biggest jumps,
3 s either side, the
game keeps each frame as played and, where it differed, as drawn. The match report
(docs/NETPLAY_REPORT.md) carries both players' to the other player and to the server:
`ghost_player1.csv` and `ghost_player2.csv` in the zip, a few tens of KB. They are what the
viewer draws.

The whole match as played is the `.SRP` (`LATEST.SRP`, and the server's recording with
RECORD MATCHES on). It holds only what was finally played -- never what a player was shown
-- which is why the windows exist.

## The G key -- for testers

**Press G (keyboard) or BACK (controller) the moment you see a ghost** in an online match.
It does nothing to the match -- the other player never knows -- and writes
`[ghost] MARKED by this player at 0:34`. At the end of the match each mark is broken down
(the 3 seconds before it), and its window goes into the match report. Times are time since
kick-off, not the match clock on the screen.

For a test session:

1. Both on the same build (bottom line of the main menu), installed with `SWOS-Setup.exe`
   or `SWOS-full.zip`.
2. Both with RECORD MATCHES on: MULTIPLAYER, log in, SETTINGS, MULTIPLAYER SETTINGS -- or
   `record=server` under `[netplay]` in SWOS.ini.
3. SEND MATCH REPORTS left on (the default in tester builds).
4. G / BACK whenever it happens.
5. After the match answer (or skip) HOW WAS THE MATCH? and stay in the lobby a few seconds:
   that is when the two halves of the report cross.

## Reading them -- for us

```bash
scripts/fetch_reports.py --grep MARK        # matches where someone pressed G
scripts/fetch_recordings.py                 # their recordings, paired with the reports
scripts/ghost_view.py build/reports/<match id>/player1.zip
scripts/fetch_reports.py --clear            # then empty the server of what we now hold
scripts/fetch_recordings.py --clear         #   (SWOS_ADMIN_USER / SWOS_ADMIN_PASSWORD:
                                            #   tools/matchmaker-admin/README.md)
```

**Did they arrange their team?** The lobby logs every use of the team screen
(`[lobby] team screen: formation 4-4-2 -> 3-5-2`, `slot 3: A -> B`, or `opened, nothing
changed`), and each side's form at kick-off is `[match] skill roll`. A misplaced player
plays at 75% of every skill -- tackling included, which is what a challenge on the man on
the ball rolls against -- so a walk-through by a floored player is the rules at work.
`SWOS_SKILL_DEBUG=1` prints each slot's file price, computed price and form.

`SWOS_SCREENSHOT_FROM=N` with `SWOS_SCREENSHOT_INTERVAL=1` screenshots a replay from its
N-th frame (the 200-shot cap otherwise ends in the first minutes of a match).

`ghost_view.py` draws each window around the ball: a filled dot where a player really was,
a hollow ring where this player's screen drew him (joined by a line) when that was
somewhere else, team 1 red, team 2 blue, the ball white -- as a contact sheet
(`window<N>.png`) and an animation (`window<N>.gif`) under `build/ghost/`. A window with
nothing drawn differently at the moment of a mark means the player saw the real match.

`bin/SWOS.x86_64 --srp-resim=<recording>` plays the match again frame by frame from its
inputs; `--view-replay=<recording>` shows it.

## Reproducing it without testers

```bash
SWOS_NETEM_LINK=1 SWOS_LIVE_PLAIN_LOBBY=1 DELAY_MS=80 JITTER_MS=20 \
  HOST_ENV=SWOS_LIVE_CHASE_INPUT=1 JOIN_ENV="SWOS_LIVE_CHASE_INPUT=2 SWOS_KEY_SCRIPT=2600:G" \
  scripts/netplay_live_test.sh           # then grep '^\[ghost\]' in the two peer logs
```

| knob | what it does |
|---|---|
| `SWOS_LIVE_CHASE_INPUT=<seed>` | a player who chases the ball, re-aims every 2-6 frames, slides in on the man on the ball and weaves when it has it -- reacting to what its own screen shows. A random pad never reaches the ball, and two of them never meet there. |
| `SWOS_NETEM_LINK=1` | the emulated network (`SWOS_NETEM_*`, include/netplay_netem.h) in front of the lobby's link too, so the input delay is agreed from the emulated ping as in a real match |
| `SWOS_LIVE_PLAIN_LOBBY=1` | `netplay_live_test.sh` skips its scripted lobby races, which a 100 ms round trip changes the timing of |
| `SWOS_KEY_SCRIPT=<frame>:G` | a G press at that presented frame |
| `SWOS_GHOST_TRACE=<file>` | every frame of the match, drawn and final, as CSV |

The same knobs work through the real menus and the match report:
`REPORTS=1 RELAY=0 HOST_ENV=... JOIN_ENV=... scripts/relay_test.sh` (FRAMES, MATCHES).

## Found along the way, not yet fixed

- **Fixed: a jittery line broke the matchmaker's punch-through** (the link "lost" after
  6 s, no match). Punching goes to every candidate address at once and each side took the
  first that answered -- on a jittery link, different ones -- then dropped the other's
  packets. Now a packet from another of the peer's addresses is the peer, replies go down
  the chosen path, and player 2 follows player 1's (src/p2p_link.c). The `mobile` profile
  and 150 ms +/-70 with 3% loss both connect and play through the lobby.
- **`netplay_live_test.sh` passes a match that never started**: under `PROFILE=awful` both
  peers ended at frame 0 (THE OTHER PLAYER NEVER ARRIVED) with equal hashes.
- **The live test's lobby race** (the joiner changes side as START lands) is refused twice
  at a 100 ms round trip and the test gives up.
