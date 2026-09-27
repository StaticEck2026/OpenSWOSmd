# NETPLAY_REPORT.md — getting a match's data back from the players (PROPOSAL)

**Status: for discussion, nothing here is built yet** except what "Today" lists. The
question: when two testers play an online match and one of them says "it lagged" or
"it froze", how do we get BOTH players' data for THAT match, together, without asking
non-technical players to find, rename and send files?

## Today

Each player's game writes `SWOS.log` in the save folder (`Documents\SWOS` on Windows).
For an online match it now holds (2026-09-27):

```
[netplay] p2p match: …; player 1 on UDP port 60627, peer 69.156.139.25:51157, input delay 2, …
[netplay] kick-off: ping 118 ms, jitter 9 ms (measured by the link), input delay 2
[netplay] 0:30, frame 1500: ping 121 ms (110..140), jitter 4 ms; 43 rollbacks, 0 holds since the last line; 0.3 frames ahead
[netplay] 1:00, frame 3000: …
[netplay] whole match: ping 124 ms (108..190), jitter 5 ms
[netplay] live match finished at frame 13742: … rollbacks … held at the prediction limit … time sync …
```

What that answers and how to read it: docs/NETPLAY_SETUP.md section 12. The recorder
(`--record`, RECORD MATCHES) is a separate thing — it catches desyncs, not lag.

**What goes wrong in practice** (the first multiplayer weekend, 2026-09-26):

1. **The log is overwritten at every start.** A player whose game crashed or who
   restarted it has lost the match we wanted: Elsheimy's log opened with the rematch,
   so the match that dropped had only one side.
2. **It is one file for everything.** Menus, lobby and several matches in one log;
   whoever reads it has to find the right match.
3. **We need both players', for the same match.** A lag report from one side is half
   the story — the two logs disagree in exactly the ways that locate the problem
   (who rolls back, who waits, who runs ahead).
4. **Testers are not power users.** Anything that starts "open your Documents folder,
   find SWOS.log, rename it…" loses most reports.

## Options

### Where the data lives on the player's machine

| | |
|---|---|
| **A. A log per online session** | `save/netplay/2026-09-26_2114_PREDICADOR_vs_ELSHEIMY.log`: only that match's netplay lines (plus the build, display and pacing lines from the top of SWOS.log), named by the date and both players. Never overwritten; the last N kept. Solves 1 and 2 |

This one is cheap and worth doing whatever is chosen below.

### How it reaches us

| option | player effort | both sides together? | notes |
|---|---|---|---|
| **B. Email / Discord by hand** | high: find the file, attach it | only if both remember | what happens today; works for the keen few |
| **C. ReportBot `/report` with the file attached** | medium | no — two separate reports | the pipeline exists (docs/TRIAGE.md) and takes attachments, but it cannot know two reports are one match |
| **D. The game uploads it to the server at the end of the match** | **none** | **yes, automatically** | see below |
| **E. Send the log to the other player** | none | yes, but on a player's disk | then one of them still has to send it to us — B again |
| **F. A separate reporting app** | install another program | no | more to build and maintain than D, for less |

### D in more detail — the recommendation

The pieces already exist:

- **Both games already know the same match id.** The recorder derives it from the GGPO
  key the lobby's START carries (include/record_proto.h), identical on both sides,
  without the server handing one out.
- **The matchmaker already accepts a TCP stream from a player at the end of a match**
  (the recorder, `--record`), on the same port, and already writes per-match records
  (`--ledger`, `recordings/`).

So: when an online match ends — finished, dropped or desynced — each game sends its
session log (A) to the server as one more message on that connection, keyed by the
match id. The server files both halves under the id: `reports/<match id>/host.log`,
`join.log`, next to the ledger line. Nobody finds, renames or sends anything, and the two
halves arrive paired.

Then, optionally:

- **G. A "HOW WAS THE MATCH?" screen** after the final whistle: 1–5, and a line of text
  for the keen. Its answer rides along with the log. It turns "somebody said it lagged
  on Saturday" into "match 4f98…, rated 2, 'kept freezing', here are both logs".
- **H. A report tool on our side**: `scripts/match_report.py <match id>` merges the two
  logs into one timeline (both players' ping, rollbacks, holds and frames-ahead every
  30 s, side by side) and flags the usual suspects: a relay, a 60 Hz display against a
  50 Hz game, a player far ahead, a silence. Low-rated matches could go to ReportBot as
  reports of their own.

## What has to be decided

1. **Consent and privacy.** The logs carry both players' IP addresses and names. A
   setting — MULTIPLAYER SETTINGS, SEND MATCH REPORTS: ON/OFF — and a line saying what
   is sent. Default ON in tester builds, OFF (or asked once) in a public release?
2. **Every match, or only when asked?** Every match (small: a session log is a few KB)
   gives us the good matches to compare against; only-low-rated keeps the server quiet.
3. **Which server keeps them.** The matchmaker on us.swos.eu is the natural place (it
   has the port, the ledger and the disk); its operator has to agree to the storage.
4. **Retention.** How long reports are kept, and who can read them.

## Proposed order

1. **A** — the per-session log file. Game only, no server, no decisions needed.
2. **D** — the upload, behind the setting from decision 1, with the server storing
   `reports/<match id>/`.
3. **H** — the merge tool, as soon as there are paired reports to merge.
4. **G** — the rating screen, once D is carrying data.
