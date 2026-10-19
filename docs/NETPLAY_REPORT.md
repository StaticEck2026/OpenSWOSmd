# NETPLAY_REPORT.md — getting a match's data back from both players

When two testers play an online match and one of them says "it lagged" or "it froze", we
need BOTH players' logs for THAT match, together, without asking players who are not
power users to find, rename and send files. This is how that works. Running the server
side is docs/NETPLAY_SETUP.md §4 and §12.

## Why

Each game writes `SWOS.log` in its save folder (`Documents\SWOS` on Windows). For an
online match it holds the connection figures (docs/NETPLAY_SETUP.md §12 reads them):

```
[netplay] p2p match: …; player 1 on UDP port 60627, peer 69.156.139.25:51157, input delay 2, …
[match] skill roll, team 1 ENGLAND: form 97% on average (107 90 …), appearance 100%, skills 294 in play
[netplay] kick-off: ping 118 ms, jitter 9 ms (measured by the link), input delay 2
[netplay] 0:30, frame 1500: ping 121 ms (110..140), jitter 4 ms; 43 rollbacks, 0 holds since the last line; 0.3 frames ahead
[netplay] whole match: ping 124 ms (108..190), jitter 5 ms
[netplay] live match finished at frame 13742: … rollbacks … held at the prediction limit … time sync …
[netplay] rating: 4 of 5 stars, AGAINST ELSHEIMY (FULL TIME  2-1): "froze twice"
```

What went wrong before this existed (the first multiplayer weekend, 2026-09-26):

1. **The log is overwritten at every start.** A player whose game crashed, or who
   restarted it, lost the match we wanted.
2. **We need both players', for the same match.** A lag report from one side is half the
   story — the two logs disagree in exactly the ways that locate the problem (who rolls
   back, who waits, who runs ahead).
3. **Testers are not power users.** Anything that starts "open your Documents folder,
   find SWOS.log…" loses most reports.

## What happens after a match

1. **HOW WAS THE MATCH?** (`p2p/rateMatchMenu`, src/online_menus.c): five stars and an
   optional line about any problems, or SKIP.
2. Once answered, each game takes its `SWOS.log` — the last 2 MB at most, with the game
   folder's path (which names the player's account) replaced by `<SWOS folder>` —
   deflates it (a whole log is ~70 KB, ~5 KB deflated) and sends it with the rating and
   comment to the other player over the lobby's link: P2PLink channel 3, next to the
   lobby's own three (0-2) and before PRED's (4, what each player saw)
   (src/match_report.c, include/report_proto.h `REPORT_HALF`).
3. When both halves are in — or the other player has left, or two minutes pass, or the
   next match starts — each game writes **one zip**:

   ```
   save/netplay/2026-09-27_211405_PREDICADOR_vs_ELSHEIMY.zip
       SWOS_player1.log     the host's SWOS.log
       SWOS_player2.log     the joiner's
       ghost_player<N>.csv  each player's ghost windows (docs/NETPLAY_GHOSTING.md)
       rating.txt           match id, teams, input delay, relayed or not, and for each
                            player: build, platform, the result they saw, rating, comment
   ```

   The last 20 are kept. Both players end up with the same two logs.
4. Each game then sends its zip to the matchmaking server (a thread of its own: a slow
   or absent server costs nobody a frame) on the server's TCP port, beginning `SWMR`
   (`REPORT_UPLOAD`). A server run with `--reports DIR` files it as
   `DIR/<match id>/player<N>.zip` and writes a `"kind":"report"` ledger line:

   ```json
   {"kind":"report","match":"4d33222c0192d2f4","at":"2026-09-27T21:31:07","player":2,
    "name":"ELSHEIMY","peer":"PREDICADOR","build":"7e9b925","rating":2,
    "comment":"froze twice","bytes":4898,"from":"81.2.3.4:51240",
    "file":"reports/4d33222c0192d2f4/player2.zip"}
   ```

   Both players send one, so the match is covered if either gets through. A server
   without `--reports` turns it away; the game logs `[report] not sent to …` and carries
   on.

Every match is reported, rated or not: the good matches are what the bad ones are
compared against, and a report is a few KB.

The match id is the one the recorder uses (`Record_MatchId` of the GGPO key the lobby's
START carries), so both players arrive with the same one and a report lines up with a
recording of the same match.

## Consent

**MULTIPLAYER SETTINGS → SEND MATCH REPORTS** (`[netplay] reports` in `SWOS.ini`). ON by
default in tester builds (`SWOS_TEST_RULES`), OFF in a public release.

OFF means nothing about that player leaves their machine: their half tells the other
player only that they have it off (`REPORT_HALF_OFF`), they upload nothing, and their own
zip still holds their own log. The logs carry both players' IP addresses and names, which
is why it asks.

The web build exchanges halves and keeps its zip in the browser's save folder, but never
uploads — a browser has no TCP to give it.

## Testing

```bash
REPORTS=1 RELAY=0 scripts/relay_test.sh    # two players through the real menus, peer to peer
```

The matchmaker runs with `--reports`, the players with `SWOS_REPORTS=server`: every copy —
each player's two, and the server's two for each match — must hold both logs and
`rating.txt`, and the ledger a report from each side. `SWOS_REPORTS=off|server|host[:port]`
also switches reports on (or off) for any run; without it the automated tests (which set
`SWOS_LOBBY_READY` and friends) never upload. HOW WAS THE MATCH? itself is skipped in them
unless `SWOS_RATE_MATCH=1`, so their reports carry no rating.

## Ghosting

Every report also carries each player's ghost windows (`ghost_player1.csv`,
`ghost_player2.csv`): what that player's screen drew against the match as played, around
the worst detected ghosts. docs/NETPLAY_GHOSTING.md has what they are, and
`scripts/ghost_view.py`, which draws them.

## Still to do

- **A report tool** (`scripts/match_report.py <zip>`): the two logs as one timeline —
  both players' ping, rollbacks, holds and frames-ahead every 30 s side by side — and the
  usual suspects flagged: a relay, a 60 Hz display against a 50 Hz game, a player far
  ahead, a silence.
- **Retention on the server.** Nothing prunes `DIR/` by itself; a report is ~5 KB, so it
  is years of testing before it matters. By hand: `scripts/fetch_reports.py` mirrors the
  reports and `--clear` then deletes from the server what was fetched (through the admin
  gateway, `tools/matchmaker-admin`; docs/NETPLAY_GHOSTING.md, "Reading them").
- **Low-rated matches to ReportBot**, as reports of their own.
