# CONTEST_LOG.md — what the match decided when two players met at the ball

Testers report "ghosting", dead balls and "magnet" tackles online, and the question is
always the same: did the game do that, or did the network? A recording replays exactly
from its inputs (`--srp-resim`, docs/REPLAY_FORMAT_V2.md §4), so the game can be asked
afterwards. The contest log is that answer, one line per decision.

```bash
# one recording -> a CSV of every decision
bin/SWOS.x86_64 --srp-resim=<file>.SRP --contest-log=<out>.csv

# the report: per match and over all of them
scripts/contest_report.py build/recordings/*.SRP
scripts/contest_report.py <file>.SRP --rolls                 # every roll, with its numbers
scripts/contest_report.py <file>.SRP --ranges ranges.txt     # what is in reported frame ranges
scripts/contest_report.py <file>.SRP --views                 # where a player's own game showed something else
```

The binary must be the build the recording was made on, or one with the same rules and
state layout: a tester recording needs `make X86_64=1 SWOS_TEST_NO_INJURIES_CARDS=1`. A
recording the game refuses or that stops matching is listed as skipped, with the reason.
The CSVs are kept in `build/contest/` and reused until the recording or the binary changes.

## What is logged

`src/contest_log.c`; the hooks are `CONTEST_LOG(...)` in `src/ball_possession.c`,
`src/update_players.c` and the resim loop in `src/netplay_match.c`. They read state and
draw no random number, and are compiled out of the Amiga build, so a match with the log on
is the match without it — `--srp-resim` still matches every frame's hash with it on.

| event | where | what it is |
|---|---|---|
| `roll` | `CalculateIfPlayerWinsBall` | The dice for a contested ball: both men, the two ratings used, the odds (16 + their difference, of 32), the number drawn, the side locked out |
| `no-roll` | the gates before it | Two men at the ball (the other within the engine's "close", squared distance 72) and no dice, with the rule that decided it — below |
| `tackle` | the slide tackle's ball contact | A slide reached the ball: the other side is locked out for 12 team steps, with no roll |
| `turn` | the dribble's turn timer | The man on the ball turned for longer than `ballControlTurnThresholds[ball control]` and is locked out for 8 |
| `trap` | `autoswitch_commit` | A player took over a moving ball: its speed, whose it was, and how far it was moved to his feet |
| `down` | `SetPlayerDowntimeAfterTackle` | A slide came to rest: how many team steps the player lies there, in the `drawn` column, and which table gave it in `reason` (below) |

`no-roll` reasons, as the code reaches them:

| reason | meaning |
|---|---|
| `self-locked` | his side is locked out (it lost a roll, was tackled, or over-turned) |
| `not-on-it` | he is near but not within reach (squared distance 32) — not yet a contact |
| `ball-high` | the ball is in the air |
| `opp-locked` | the other side is locked out: he plays the ball freely |
| `opp-not-on-ball` | the other man is not "on the ball": fire held, shooting |
| `opp-still` | the other man holds no direction: the ball is simply taken |
| `opp-nobody` | the other side has no controlled player |

`who` and `opp` are `P1`/`P2`, the netplay player numbers, read from the team at the
moment of the event — the teams change ends at half-time, so "the top team" is not one
player for a whole match.

### How long a slide leaves a player down (`down`)

Two tables, both the original's (`SWOS2.s` 23128, 23621-23639, 30704-30714; the same in the
PC release):

| `reason` | when | team steps down |
|---|---|---|
| `by-tackling-skill` | fire was still held past the slide's second step | 30, 27, 24, 21, 18, 15, 12, 9 for tackling 0-7 |
| `computer` | **fire was let go within the slide's first two steps** (`tacklingTimer` ends as -1) | 3, whatever the skill |

The second table is named for the computer's players in the disassembly, but a human gets it
by TAPPING fire for the slide instead of holding it: the slide's timer counts while fire is
held, is negated the first step fire is found released, and a value of -1 or -2 becomes -1,
which selects that table. So a tapped slide has a player on his feet in 3 steps where a held
one leaves the same player down for 9 to 30. Two players with the same tackling who slide a
frame apart can get up five steps apart, and that is the rule, not the network.

First seen answering ReportBot 0021 (match `ee7d141f`, 2026-10-04): at frame 10862 F.
RAVANELLI (tackling 4) went down for 18; at 10863 H. CALHANOGLU (tackling 4) for 3. Over the
match one player tapped 5 of 40 slides, the other 2 of 50.

## Views: what each player's own game showed

The match as played is what both machines ended on. What a player WATCHED is his own game
the first time it showed each frame, which under rollback may have been played on a guess
of the other player's input and corrected a frame or two later. When the recording has
`PRED`, the re-simulation replays that too (`resimView` in `src/netplay_match.c`): for each
player and each frame his game first played on a guess, it goes back to the last frame he
had real input for (the frame less the recorded lag), plays forward on his own inputs and
the other player's input of that frame repeated — GGPO-X's guess — and logs the last of
those frames with `view` = 1 or 2 (0 is the match).

It checks itself against `SEEN`: after each replayed frame every man and the ball must be
where the recording says that player's screen drew them, and where the match had them
when it says nothing. `--srp-resim` prints the count, and the report totals it. On the
first run: 604,143 guessed frames replayed, 604,143 exact.

The report then compares, for every roll, slide tackle and over-turn, what each player's
game showed on that frame with what the match did: the same; a frame or so early or late;
the OTHER side locked out; not shown on that frame (it appears with the correction); or
shown and never in the match (a phantom).

## What the report adds

`scripts/contest_report.py` checks every roll against the rule (the side locked out must
be the one the number drawn gives), groups the lines into contacts, tabulates observed
against expected by rating difference, and reads from the recording's `PRED`/`SEEN` what
each player's screen was drawn from at each event: frames guessed, whether the guess was
wrong, and how far anything was drawn out of place in the 12 frames after.

## First result (2026-10-02)

The full write-up, with the rules and the proof that they are the original's, is
[BALL_CONTEST.md](BALL_CONTEST.md).

37 recordings (28 September – 2 October), 540,648 frames:

- 1,169 rolls, **0** not what the rule gives; weaker side locked out 616 times against
  608.8 expected (± 17.1).
- 1,037 contacts with both men in reach, 974 rolled for; the other 63: 32 the other man
  not on the ball, 15 ball in the air, 12 inside a lock-out, 4 the other man standing
  still. None without a rule's reason.
- Without a roll: 303 slide tackles reached the ball, 148 over-turns, 221 traps of a fast
  ball off the other side that moved it 4 px or more.
- The lock-out is the TEAM's: 74 times a different team-mate reached the ball inside it.
- In 673 rolls one side's rating was a team-mate's and differed from the man's own — the
  originals' squad-slot lookup (`GetPlayerPointerFromShirtNumber` with the other team's
  sprite).
- All 34 moments of ReportBot 0015–0017: 28 rolls, 1 slide tackle, 1 lock-out in force,
  4 traps.
- Views, over the 31 matches with both players' data — 2,785 events (each roll, slide
  tackle and over-turn, once a player): 940 played on real input; 1,756 on a guess and
  shown the same; 3 a frame or so apart; 85 not shown on that frame (24 rolls, 11 tackles,
  50 over-turns), appearing with the correction 1 to 3 frames later; 16 phantoms (12 rolls,
  3 tackles, 1 over-turn); **1** roll shown with the other side locked out. None of these
  is in any of the 34 reported moments.

## Limits

It is the match as finally played: a frame a peer guessed and took back is not in it, by
design. It says what the match did and what each screen was drawn from, not what a player
perceived. And it only reads recordings this build can replay.
