# Gameplay regression testing — recorded-game replay

Deterministic gameplay testing for OpenSWOS: replay a recorded match
frame-by-frame and assert that our simulation reproduces the reference state
*exactly*, every frame.

The idea is borrowed wholesale from **swos-port**
(`C:\Projects\GitHub\swos-port`), whose `tests/src/tests/RecordedDataTest.cpp`
replays `.RGD` recordings against its DOS port. That corpus is our starting
point; an Amiga corpus follows once we can harvest it (see
[Harvesting Amiga data](#harvesting-amiga-data)).

Status: **live.** The corpus decodes, converts and replays; all 12 decodable
recordings run, and **`zkz-11021807` now replays end to end — all 4064 frames,
every bound field, every frame.** The rest reach 1108-3916 at level `info`
(section 5), except `zkz-10174427`, whose frame 0 is one byte of formation
coverage rather than a porting defect. Twenty-seven real defects found and fixed
on the way in, and
**401 of the 416 player values the recordings carry are reproduced exactly** —
the strongest single measurement the corpus supports (section 5, "The
player-price audit").

```bash
scripts/replay_test.sh                                  # convert + replay + report
scripts/replay_test.sh --all                            # every comparison level

scripts/rgd_convert.py --info    tests/dos/*.RGD.gz     # header + frame geometry
scripts/rgd_convert.py --verify  tests/dos/*.RGD.gz     # decode invariants
scripts/rgd_convert.py --dump-frames 0:4 tests/dos/zkz-12065730.RGD.gz
```

**12 of 13 recordings decode with zero trailing bytes and pass verification.**
The decode is corroborated semantically, not just arithmetically: full matches
end at exactly 90 min, one ends at 120 min drawn 3-3 (extra time), scorelines are
plausible, the game clock is monotonic and `gameTime[0]` is always 0. Frame 0 of
`zkz-12065730` shows player 1 pressing fire at kick-off with the camera then
panning smoothly — velocity ramping 0, 2, 4, 6 — and `cameraX` 11534336 is
exactly 176.0 in 16.16. Field offsets are therefore right, not merely the
stride.

| | frames | ends |
|---|---:|---|
| `zkz-12065730` (v1.5) | 1519 | 3 min, 1-0 |
| `zkz-11021807` (v1.3) | 4064 | 6 min, 0-0 |
| `zkz-11054632` (v1.1) | 7314 | 8 min, 0-0 |
| `playa-10090613` (v1.1) | 22497 | 90 min, 3-0 |
| `zkz-10140228` (v1.1) | 39066 | 120 min, 3-3 |
| `zkz-12080203` (v1.4) | 31635 | 90 min, 10-10 |

---

## 1. Layout

```
tests/dos/      13 x *.RGD.gz  (66 MB)  — imported from swos-port
                canada.diy, commentary.zip
tests/amiga/    (planned — needs a dumper, see section 6)
```

`tests/dos` is a verbatim copy of `swos-port/tests/data`. All 13 archives pass
`gzip -t`.

The corpus is **untracked** (`.gitignore`) until the rig has proven it decodes
and replays; only then do we decide between Git LFS and a fetch script. 66 MB of
binary is hard to remove from git history once it is in. Restore it with:

```bash
mkdir -p tests/dos && cp /mnt/c/Projects/GitHub/swos-port/tests/data/* tests/dos/
```

---

## 2. The `.RGD` format

A recording is `Header` followed by a flat array of fixed-size `Frame` records —
no index, no per-frame length, so the frame size must be derived from the
version in the header's first two bytes. Little-endian, `#pragma pack(1)`.

Sizes below are **measured from the corpus**, not read off the C++ — each was
confirmed by checking that `(filesize - header) % frame == 0` *and* that the
`frameNo` dword at the start of consecutive frames runs 0,1,2,3,…

| version | header | frame | struct |
|---|---:|---:|---|
| 1.0 / 1.1 | 3610 | 7404 | `Header` + `FrameV1p0` |
| 1.2 | ? | ? | **unresolved — see below** |
| 1.3 | 3617 | 7475 | `HeaderV1p2` + `FrameV1p3` (`+ BenchDataV1`, 71) |
| 1.4 | 3617 | 7481 | `+ userKey, fireBlocked, statsTimer, resultTimer` (+6) |
| 1.5 | 3617 | 7485 | `+ pl1/pl2TapTimeoutCounter` (+4) |

`BenchDataV1` is 71, not 72 — `shirtNumberTable` is `2 * kNumPlayersInTeam` with
`kNumPlayersInTeam == 16`. That makes the whole model self-consistent: the base
frame is 7404 for *every* version, and 7404 + 71 + 6 + 4 = 7485 reproduces the
measured v1.5 size exactly.

The component sizes are likewise forced rather than assumed. Their `TeamGame` is
1704 (`10 words + markedPlayer + teamName[17] + 3 bytes + 16 * PlayerGame(61) +
unknownTail[686]`) and `TeamStatsData` is 14, which leaves
`33 * Sprite == 7404 - 3436 - 338 == 3630`, i.e. **`Sprite` == 110 — exactly our
32-bit `Sprite`**. `scripts/rgd_convert.py::build_frame_layout()` asserts the
sum, so a wrong constant fails loudly rather than silently mis-parsing.

Note our `TeamGame` (1018) is their first 1018 bytes — everything before
`unknownTail[686]`.

### The corpus

| file | ver | frames | teams |
|---|---|---:|---|
| `playa-10090613` | 1.1 | — | BORUSSIA FULDA v ITALY 96/97 |
| `playa-10095214` | 1.1 | — | BRAZIL v URUGUAY |
| `playa-10144556` | 1.1 | — | FORT. DUSSELDORF v FREIBURG |
| `zkz-10140228` | 1.1 | — | FORT. DUSSELDORF v MSV DUISBURG |
| `zkz-10173518` | 1.1 | — | BRAZIL v ARGENTINA |
| `zkz-10174427` | 1.1 | — | AC MILAN v JUVENTUS |
| `zkz-10190651` | 1.1 | — | FORT. DUSSELDORF v MSV DUISBURG |
| `zkz-11021807` | 1.3 | 4064 | FORT. DUSSELDORF v MSV DUISBURG |
| `zkz-11054632` | 1.1 | 7314 | FAETANO v FOLGORE |
| `zkz-11122841` | 1.2 | — | GREAT OLYMPICS v GREAT OLYMPICS |
| `zkz-11215003` | 1.1 | — | JUVENTUS v MANCHESTER UTD |
| `zkz-12065730` | 1.5 | 1519 | ALIANZA ATLETICO v UNIVERSITARIO |
| `zkz-12080203` | 1.4 | 31635 | FORT. DUSSELDORF v MSV DUISBURG |

**Open:** the single v1.2 file resolves with no (header, frame) pair at all —
brute-forced over headers 3560–3700 against frame sizes 6000–9000 requiring ten
consecutive `frameNo`s, with no hit. It is also the largest file (112 MB). It
may be a format variant or simply a damaged capture. One file out of thirteen;
parked.

### Header fields

Offsets verified: the two `TeamFile` records land at +8 and +692, and their name
strings read correctly ("ALIANZA ATLETICO", "UNIVERSITARIO").

```
+0    byte   major, minor, fixedRandomValue
+3    int8   pitchTypeOrSeason, pitchType, gameLength, autoReplays, allPlayerTeamsEqual
+8    TeamFile topTeamFile     (684)
+692  TeamFile bottomTeamFile  (684)
+1376 byte   topTeamCoachNo, topTeamPlayerNo, bottomTeamCoachNo, bottomTeamPlayerNo
+1380 byte   extraTime, penaltyShootout
+1382 int8   topTeamMarkedPlayer, bottomTeamMarkedPlayer
+1384 TeamTactics userTactics[6]  (6 x 370 = 2220)
+3604 word   inputControls
+3606 dword  initialFrame            -> end of `Header` = 3610
+3610 dword  trainingGameCareerSize  } HeaderV1p2
+3614 int8   season, minSubstitutes, maxSubstitutes  -> 3617
```

### Frame fields

Offsets within a frame, as decoded and verified by `scripts/rgd_convert.py`:

```
   +0  4     frameNo
   +4  2+2   player1Controls, player2Controls
   +8  145   topTeam (TeamGeneralInfo)      +153  11   topTeamPlayers
 +164  145   bottomTeam                     +309  11   bottomTeamPlayers
 +320  1704  topTeamGame                   +2024  1704 bottomTeamGame
+3728  14    topTeamStats                  +3742  14   bottomTeamStats
+3756  4+4   cameraX, cameraY  (16.16 fixed point)
+3764  1+1   cameraXVelocity, cameraYVelocity
+3766  4     gameTime[4]   (minutes = d[3] + 10*d[2] + 100*d[1]; d[0] is always 0)
+3770  1+1   team1Goals, team2Goals
+3772  1+1   gameState, gameStatePl
+3774  3630  sprites[33]   (110 each)
 =7404       end of FrameV1p0
+7404  71    BenchDataV1                    (v1.3+)
+7475  1+1+2+2  userKey, fireBlocked, statsTimer, resultTimer   (v1.4+)
+7481  2+2   pl1TapTimeoutCounter, pl2TapTimeoutCounter         (v1.5+)
```

Original C++ declaration order, for reference:

```
dword frameNo
int16 player1Controls, player2Controls   (bitfield: up1 down2 left4 right8 fire32 bench64 esc128)
TeamGeneralInfo topTeam;    char topTeamPlayers[11]
TeamGeneralInfo bottomTeam; char bottomTeamPlayers[11]
TeamGame topTeamGame, bottomTeamGame
TeamStatsData topTeamStats, bottomTeamStats
FixedPoint cameraX, cameraY;  int8 cameraXVelocity, cameraYVelocity
byte gameTime[4], team1Goals, team2Goals, gameState, gameStatePl
Sprite sprites[33]
[v1.3+] BenchDataV1     [v1.4+] userKey, fireBlocked, statsTimer, resultTimer
[v1.5+] pl1TapTimeoutCounter, pl2TapTimeoutCounter
```

---

## 3. Running it

```bash
scripts/replay_test.sh                    # the shortest recording, level "info"
scripts/replay_test.sh --all              # every level, in order
scripts/replay_test.sh --level sprites tests/dos/zkz-11021807.RGD.gz
scripts/replay_test.sh --level info --frames 0:400 tests/dos/zkz-12080203.RGD.gz

bin/SWOS --replay-selftest                # CalculateDeltaXAndY vectors, no data needed
```

The script converts the recording (`scripts/rgd_convert.py --emit`) and hands
the `.swr` to `bin/SWOS --replay-test=`. Exit code **0** = every frame matched,
**2** = divergence, **1** = setup failure. Useful flags, all also available
directly on the binary:

| flag | what |
|---|---|
| `--replay-level=<scalars\|teams\|info\|sprites>` | how much of each frame to compare |
| `--replay-max-errors=N` | keep going and list up to N mismatching fields |
| `--replay-skip=a,b,…` | mute fields whose name contains any of these substrings |
| `--replay-window` | open a window; off by default, a replay is a batch run |

The four levels are cumulative, cheapest first:

| level | fields/frame | what it adds |
|---|---:|---|
| `scalars` | 19 | game clock, goals, `gameState`/`gameStatePl`, camera, the v1.4 timers |
| `teams` | 793 | both `TeamGame`s (16 `PlayerInfo` each) and both `TeamStatisticsData` |
| `info` | 915 | both `TeamGeneralInfo`s — the per-team control/AI block |
| `sprites` | 2499 | all 33 sprites |

Start low. A cheap level runs the same simulation but reports the *symptom*
rather than the cause; a high level reports the cause but has more known
non-comparable fields to work around. Divergences found at `scalars` are almost
always visible much earlier at `info`.

---

## 4. What it compares, and how

Once, before frame 0: `checkPlayerValues` recomputes the per-player match value
of all 32 players from the recording's own team records and compares it with
what the reference build stored there. See "The player-price audit" — it is
setup state, so it is reported rather than fatal, and it is the only check that
reaches `GetPlayerPrice` and the tactics coverage at all.

Per frame: `ReplayTest_FrameStart` (top of `GameLoop`'s body) writes the
recorded stick state into `_joy1Dat` / `_joy0Dat` / `_ciaa_pra` — the same
registers `input_sdl.c` writes, so `Player1StatusProc` decodes a recorded stick
exactly as it decodes a real one — and `ReplayTest_FrameEnd` (just before
`wait_for_vblank_sync`) compares every bound field and stops at the first
mismatch.

Comparison is by **name**, never by address or offset. `scripts/rgd_schema.py`
pins the DOS layout in one place and emits `(name, type, level, cmp)` rows;
`src/replay_test.c` builds a registry of the same names from our own
`offsetof`s and binds the two at load. A schema name with no matching global is
a hard load error, so the rig can never silently skip a field.

`cmp` classes:

| class | meaning |
|---|---|
| `eq` | exact, at the recorded field's width (so their `word` vs our `int16_t` is not a difference) |
| `bool` | truth test only — see "byte-set word flags" below |
| `view` | depends on the view geometry; compared only when ours matches the recording's |
| `spr` | a sprite-bank index; compared after subtracting the recording's `sprite_index_offset` |
| `ptr:<kind>` | a pointer, compared as a semantic **identity** — see below |
| `na` | the slot has no counterpart in this port at all — never compared |
| `in` | recorded input, injected rather than compared |
| `meta` | bookkeeping (`frameNo`) |

### Pointers are compared, as identities

The recording does not store addresses. swos-port's recorder writes each
pointer field as a small semantic value, and its own rig decodes it the same
way (`verifySpritePointer`, `verifyShotChanceTable`): `-1` for null, `0`/`1`
for "which team", and an index into the recording's own 33-sprite order for a
player. So the rig resolves the identity to the pointer *our* build should
hold, and reports a mismatch by name:

```
top_info.controlled_player   recorded=sprite[23]   actual=sprite[21]
```

This is the tier the first version of the rig deferred, and it turned out to be
the most valuable one. Every scalar can agree while the two ports are driving
**different men** — which is exactly what was happening (see below), and no
amount of scalar comparison would have said so.

### Determinism

A replay is only meaningful if two runs agree, and three inputs had to be taken
away from the SDL host before they did:

- **`Rand` / `Rand2`** return the recording's `fixedRandomValue` while
  `_replayFixedRandom >= 0` (random.c). The recordings pin the RNG to one
  value; without this nothing reproduces.
- **`_currentGameTick` and `_currentTick`.** On Amiga these are the VBlank
  interrupt, i.e. exactly one step per game-loop iteration. On SDL two threads
  bump them from a wall clock. Gameplay reads both — `_currentTick & 2` gates
  the ball-control speed bonus (`ball_possession.c`), `_currentGameTick & mask`
  gates most of the AI's phase decisions — so the rig owns them and steps them
  itself (`ReplayTest_OwnsGameTick`, gated in `interrupts.c`).
- **`_lastGameTick`.** This was the subtle one. `ReadTimerDelta` turns
  `_currentGameTick - _lastGameTick` into `_lastFrameTicks`, which every
  stoppage/referee/result timer is stepped by. Coming out of the menus it held
  whatever the wall clock had reached, so frame 0's delta was a random number
  and two runs of the same recording diverged at different frames. Reset with
  the other two at kick-off.

`--replay-test` also forces `SWOS_HARNESS_TURBO` (no 50 Hz wait — the clock is
driven per iteration, not by the wall), `--dos-gameplay=on`, the port's native
336x272 render rather than the tester's monitor aspect, and
`SDL_VIDEODRIVER=dummy` so nothing opens a window (a full matrix is 48 runs).
Every buffer and blit is still real — the game renders into the frontbuffer,
which is what the `onScreen` cull reads back — there is just nowhere to put it.
`--replay-window` opts back in to watching one.

### The simulation view

SWOS reads the view size from *inside* the simulation, so the rig sets it to
the geometry the recording was made in (`_sim_view_width` / `_sim_view_height`,
`include/display.h`) while the renderer keeps drawing at `_screen_width`. Every
camera constant in the engine turns out to be a function of it, which is what
makes this a faithful generalisation rather than a test hook:

| | formula | at 336x272 (ours) | at 320x200 (DOS) |
|---|---|---:|---:|
| kick-off centre X | `(672 - viewW) / 2` | 168 | 176 |
| bottom start Y | `664 - (viewH - 200)` | 592 | 664 |
| camera centre Y | `viewH / 2` | 136 | 100 |
| pan range | `672 - viewW` | 336 | 352 |
| target Y clamp | `664 - (viewH - 200)` | 592 | 664 |
| position Y clamp | `880 - viewH` | 608 | 680 |

The left column is what this port had hard-coded (`168`, `592`, `0x88`,
`0x250`, `0x260`); the right column is swos-port's own named constants
(`kCenterX`, `kBottomStartLocationY`, `kPitchMaxX`, `kCameraMaxY`). Same
formulas, both releases.

The default is unchanged behaviour — `main.c` follows `_screen_width` into it,
exactly what the culls and camera used before. Pinning it to 336x272 instead is
the widescreen fix noted below, and is deliberately *not* what the default
does.

---

## 5. Results

Every recording that decodes also replays. The measured numbers live in
**[TESTS_RESULTS.md](TESTS_RESULTS.md)** — every recording at every level, plus
the field each one first disagrees on — because they change with the engine and
this document does not. Regenerate them with:

```bash
scripts/replay_matrix.sh > docs/TESTS_RESULTS.md      # a few minutes
scripts/replay_matrix.sh --levels info --tsv          # quick, diffable
```

The shape of the results, as of the run in that file: **889-5276** frames at
`scalars` (`zkz-11021807` complete), **1108-3916** at `info`, **939-2694** at
`sprites`, and **401 of 416**
on the per-player match values (the `values` column — a setup-state check that
runs before frame 0; see "The player-price audit"). `zkz-10174427` is left out
of those ranges: it stops at frame 0 on a skill nibble, for a reason that is
understood and is not a porting defect (same section).

Four things to read into them:

- **The four levels now reach roughly the same depth.** That is the headline:
  the full sprite compare — 22 player sprites' positions, deltas, directions,
  speeds, plus the ball and both goals — survives as far as the cheap scalar
  compare does. It used to stop at frame 116-147.
- **Nothing stops on `on_screen` any more.** The `sprites` column stops on
  ordinary simulation values (`sprite[N].dest_y`, `.x`, `.z`,
  `.player_down_timer`), which is what you want a divergence to look like.
- **`scalars` and `info` are not comparable with runs before the simulation
  view existed.** The camera fields used to be skipped as non-comparable and
  are now checked, so several numbers went *down* while the test got
  *stronger*: `zkz-11021807` reads 222 rather than 775 because it now stops on
  `camera_x`, which nothing was looking at before.
- **`values` is the one column that does not depend on how far a replay
  runs.** It compares 32 whole `GetPlayerPrice` results per recording against
  the numbers the reference build left in the header, so it grades the deepest
  arithmetic in match setup without needing a single frame to line up.

Hundreds of frames in which 930+ named fields agree exactly is a real
regression net: a change that perturbs the simulation moves these numbers
immediately.

### Can we claim "100% accurate"?

Not against this corpus — and the reason is worth understanding, because it is
not a shortcoming of the port.

**The recordings are SWOS++, not the retail DOS game.** swos-port's own source
says so where it initialises a sprite:

```c
// this is for SWOS++/tests compatibility
playerDirection = playerOrdinal ? 0 : -1;
...
// SWOS initial value for frameSwitchCoutner is 0, but we will initialize
// with -1 to be compatible with SWOS++ and tests
frameSwitchCounter = -1;
```

It deliberately deviates from SWOS's own initial values to agree with the
recordings, and its rig compensates elsewhere for the recorder running "*after*
reset and SWOS++'s *before*". The files are `zkz-*` — Zlatko Karakaš, SWOS++'s
author. So **matching those fields 100% would mean reproducing SWOS++'s
departures from SWOS**, which is the opposite of what this port is for.

That reframes the target. There are four classes of field where exact agreement
is either impossible or undesirable:

| class | why | closable? |
|---|---|---|
| Recorder conventions (`frame_switch_counter`, `player_direction`, `starting_direction`, `save_sprite`, `field_3e`) | SWOS++ initial values that the reference's own source calls deviations from SWOS | **No — and we should not want to** |
| Two sprite slots (corner flag, player-name HUD) | swos-port keeps four corner flags and picks by camera; its rig skips the HUD slot outright | No — no counterpart exists here |
| Byte-set word flags (`ballOutOfPlayOrKeeper`, `goaliePlayingOrOut`, `isMoving`) | `SWOS2.s` writes a byte into a word, so "set" is `0x00FF` little-endian and `0xFF00` big-endian. Exactness is *target-dependent*; a truth test is the correct comparison, not a compromise | No — the truth test **is** the right test |
| ~~Per-image cull metrics~~ | **Retracted.** The banks are a 1:1 conversion and the metrics are identical; the residual was two bugs of ours (the cull keyed off the render width, and `onScreen` was overwritten after the enqueue). Both fixed | **Closed** |

So the honest strong claim available today is *bounded*, and still worth making:

> Every field two differently-derived builds can meaningfully be compared on
> agrees exactly, for the first N frames of each recorded match — with a
> four-item exclusion list, every item justified from the reference
> implementation's own source.

**The fourth class is closed** — it was never about the artwork. See "The
`onScreen` ceiling" above for the measurement that settled it and the two
defects of ours it exposed. Three classes remain, and none of them is a
statement about this port's accuracy: two are slots that do not exist here, and
one is a flag whose exact value is endian-dependent by design.

### The player-price audit — and the 416 checks hiding in the header

`zkz-10174427` was the corpus's one frame-0 divergence: `top_game.players[5]`
(AC Milan's Stefano Eranio) came out with `passing`/`tackling`/`ballControl` 4
where the recording has 5. The cleanest possible target — no camera, no cull,
pure match setup — so it got a full line-by-line read against `data/SWOS2.s`.
**Our code is right**, and the way that was finally settled turned
into a permanent part of the rig.

#### The recording carries the answer

A recorded team record is a whole 684-byte `TeamFile`, and byte 1 of each
`PlayerFile` is `cachedMatchValue` — what the *reference build's*
`UpdatePlayerPositionAndReturnRole` computed for that squad in that formation
before the match. For the ten outfield starters that is an entire
`GetPlayerPrice` run (the 35-quadrant walk over the team's tactics coverage, six
weight tables, the crowding count, the nearness ratio) reduced to one byte we
can check exactly. For the goalkeeper and the five substitutes it is the file
price, which checks the routine's dispatch rules instead.

That is **32 assertions per recording, 416 across the corpus**, on setup state
the frame loop never reaches — and `AdjustPlayerSkills` scales every skill
nibble by this value, so a wrong price is the difference between reproducing a
recorded match and not.

`checkPlayerValues()` (`src/replay_test.c`) runs it before frame 0: it installs
the recorded record, forces `playerValuesDirty` (with the flag CLEAN the routine
just hands back the byte we are trying to check), recomputes all 16, compares,
and restores. Every run prints one line:

```
[replay] player values: 32/32 match
[replay]   AC MILAN player 5 (STEFANO ERANIO): value recorded=28 actual=27
[replay] player values: 31/32 match — see above (formation coverage, ...)
```

It is **reported, not fatal**: the corpus was recorded from a DOS release whose
formation *coverage* differs from the Amiga's in a handful of bytes, so a
mismatch here can be data rather than code. The `values` column in
docs/TESTS_RESULTS.md is this number.

#### The result: 401/416

| | |
|---|---|
| all 13 recordings | **401 / 416** |
| the 12 decodable ones | **382 / 384** |
| `zkz-11122841` alone (v1.2) | 19 / 32 — see below |

`zkz-11122841` is the file whose `(header, frame)` sizes never resolved. Its
*first* GREAT OLYMPICS record is perfect (16/16, which is what says the record
is being parsed correctly); its *second* is a hand-built squad whose ten
starters are all trialists or reserves (`form` `0x9B`/`0x9A`) with a stored
value of **0 across the board** — including four **substitutes**, whose value is
just the file price, a number `GetPlayerPrice` is never even asked for. Byte 1
of that record does not describe the squad it holds, so the file is excluded
rather than explained.

That leaves **two** real mismatches in the whole corpus, and both are one byte
of formation coverage.

#### Eranio: one coverage byte, worth exactly the missing bucket

Narrowed backwards from the recorded value: `ScaleSkill` rounds stochastically
on `Rand2`, which the rig pins to the recording's `fixedRandomValue` (187), so
`r = 73` and solving `f(5)=5 && f(6)=5` forces `d2 = 95`; `appPercent` is 100
here; in a friendly `GetPlayerPricePercentageChange` adds only
`(Rand2()*24>>8)-12 = +5`. So `GetPlayerPrice` must return 28 and ours returns
27 — which `cachedMatchValue` then confirmed directly, no back-solving needed.

Inside `GetPlayerPrice` the gap is tiny and structural: our 35-quadrant total is
12531 and the next `÷350` bucket starts at 12600, so the price is 69 units of
total short of 28. It all comes from one quadrant. Eranio is player 5, so he
walks **row 4** of the 4-4-2, and at ball position 0 that row differs between the
two releases:

| | row 4, ball position 0 | quadrant | contribution |
|---|---|---:|---:|
| Amiga `teamTactics442` (what we ship) | `0x33` | 6 | 297 |
| DOS `tact_4_4_2` | `0x23` | 5 | 378 |

`+81`, total 12612, price 28, recorded skills reproduced. Every other one of
Eranio's 35 quadrants is identical between the two. Verified against the disassembly
directly: our `442_original` is byte-identical to `data/SWOS2.s`'s
`teamTactics442` (0 of 350 differ, and SWOS-RE's copy agrees), so `0x33` is
genuinely the Amiga's value and this is a data difference, not a transcription
slip.

**So the last frame-0 divergence in the corpus is the DOS-formation gap**
(`project_dos_tactics_gap`), not a porting defect: `ApplyGameStyle`
(`src/dos_gameplay.c`) swaps the physics and timer constants for
`--dos-gameplay=on` and leaves the formations alone, and `GetPlayerPrice` reads
them. `make clean && make SWOS_DOS_TACTICS=1` now bakes the DOS coverage
(docs/TACTICS_POSITION_VALUE.md §9.6) — and running the corpus against it is
what produced the table below, so read that before wiring it to the runtime
flag.

> **Retracted:** an earlier revision of this section concluded "the corpus uses
> the DOS 4-4-2", on the strength of swos-port's baked `tact_4_4_2` being
> byte-identical to `docs/tactics_amiga_vs_dos/tactics_dos.json`. Substituting
> that whole table makes things *worse*: it fixes Eranio and breaks the two
> forwards, in every 4-4-2 team in the corpus. See the next section — the
> "slot 10 anomaly" that revision left open was an artefact of exactly that
> substitution.

#### The recording grades our formation data

With `cachedMatchValue` as ground truth, the corpus can be replayed against
*any* candidate coverage table without running the game. Over the 12 decodable
recordings — 24 team instances, 240 outfield starters, 5 formations:

| coverage fed to `GetPlayerPrice` | matches |
|---|---:|
| what we ship (Amiga + the tester corrections) | 238 / 240 |
| pristine Amiga (`SWOS_ORIGINAL_TACTICS=1`) | 229 / 240 |
| the DOS snapshot | 225 / 240 |
| **DOS rows 0–7 + Amiga rows 8–9** | **240 / 240** |

The first three rows are the build itself, not a model: `make clean && make
SWOS_ORIGINAL_TACTICS=1` and `SWOS_DOS_TACTICS=1` (§9.6 of
docs/TACTICS_POSITION_VALUE.md) select the other two, and the rig then scores
**382 / 373 / 369** of 384 respectively over the twelve decodable recordings —
the same players, with the same values, that the table above predicts. Under
the DOS data Eranio and Ayala below both come out right and fifteen forwards do
not; the three sets of failures are disjoint, which is the whole point.

> **Retracted.** The last row was read as "the corpus's data is a hybrid — DOS
> for slots 0-7, Amiga for the two forward rows — so no table we hold is the one
> it ran". It is not a hybrid, and the table we hold *is* the one it ran. See
> "The rows that were never executed" below: those forward rows are dead in both
> binaries, and with the startup copy applied the DOS coverage scores
> **384/384** on the corpus's own player values — every outfielder in every
> decodable recording, Eranio and Ayala included.

This also grades the four formations this port deliberately edited (commit
`23f0c59`, tester-supplied `.TAC` fixes for central defenders / attackers
flipping sides — see docs/TACTICS_POSITION_VALUE.md §9):

| formation | what the fix does | DOS | corpus |
|---|---|---|---|
| 4-4-2 | un-swaps CD rows 1↔2 at ball positions 4 and 20 | same at 4; different values at 20 | **confirms** position 4 (5 players) |
| 4-3-3 | swaps CD rows 1↔2 at 16 and 20, forward rows 6↔8 at 16 and 20 | **byte-identical** to our corrected rows 6 and 8; agrees at CD 16 | **confirms** position 16 (5 players) |
| Sweep | swaps CD rows 1↔2 at 4 and 15, forward rows 8↔9 at 4 | agrees at 4; keeps the Amiga order at 15 | **contradicts** position 15 (1 player) |
| 3-4-3 | swaps forward rows 6↔8 at 14 ball positions | keeps the Amiga data | untested — no corpus team uses it |

Ten recorded players say the 4-4-2 and 4-3-3 fixes are right and the pristine
Amiga is wrong — the DOS release made the same edit. One says the Sweep fix went
a step too far: at **Sweep row 1/row 2, ball position 15** our data (`0x42` /
`0x73`) is the swap of a pair the Amiga *and* DOS both agree on (`0x73` / `0x42`),
and Argentina's Roberto Ayala comes out 29 where the recording has 28. Left as
shipped — it is a deliberate gameplay change and reverting one ball position of
it is the owner's call — but it is the one byte of our tactics data that a
reference implementation contradicts.

#### The read itself

For the record, since the audit was done: `GetPlayerPrice`,
`CountNearnessFactor`, `GetPlayerPricePercentageChange`, `ScaleSkill`,
`ApplyTeamTactics` **and its call site** all match the asm, including the parts
that are easy to get wrong — the second `mulu.w` truncating the first's 32-bit
result, the byte-wide `sub.b`/`bmi` tail, the divide by `numPlayersAtQuadrant-1`,
and `ext.l` before the unsigned `divu.w #350`. All six data tables
(`skillsPerQuadrant`, `quadrantDistanceFactor`,
`quadrantCoordinatesToBallQuadrant`, `priceSkillPonders`, `skillValuePonders`,
`plPricesIncreaseTable`) match byte-for-byte, and the
`loc_122D6A+1 == skillsPerQuadrant - 35` claim our source comment makes was
confirmed by counting instruction bytes back from the label. 401 recorded values
now say the same thing from the other end.

**Method worth reusing:** solve backwards from the recorded value to the exact
input it requires, then check that input — and before positing a difference,
look for a place the reference already wrote its own answer down. A long chain
of arithmetic on skill nibbles, and a wrong conclusion drawn from it, were both
settled by a byte that had been sitting in the recording's header all along.

### With an Amiga corpus, 100% becomes a real target

All four classes above exist only because the recording came from a different
release. Record from the Amiga original at 336x272 with the Amiga sprite bank
(section 8) and: the recorder conventions are the original's own, the corner
flag and HUD slots are the ones we have, the byte-set flags land in the byte
order we use, and the cull metrics are the artwork we ship. Nothing is left to
exclude, and "every recorded field, every frame" is a claim that can actually
be made — and would mean what it says.

### What the rig has already found

Twenty-five real defects so far, all fixed — plus the two in "The `onScreen`
ceiling" below, which that measurement turned up on its own:

1. **`GS_54_49` was 54; it is 70.** The match-clock accumulator reload.
   `zkz-12065730` reaches game-minute 1 at frame 702 and minute 2 at frame 842
   — 140 frames per game-minute. The accumulator subtracts
   `gameLenSecondsTable[0] = 30` per frame and reloads by `N`, so 60 seconds
   take `2N` frames: **N = 70**. 54 would give 108 frames/minute, swos-2020's
   69 would give 138. `docs/DOS_GAMEPLAY.md` had taken 54 from a swos-2020
   comment; swos-port's `kPcTicksPerGameSeconds = 70` agrees with the
   measurement.
2. **The 41/64 speed_factor used C division instead of shifts.** `41/64` is
   `x - x/4 - x/16 - x/32 - x/64` with *arithmetic* shifts, which floor;
   C division truncates toward zero, so the two disagree by one on negative
   deltas and the trajectory forks within a few frames. swos-port hit exactly
   this ("shifts have to be used explicitly, divisions end up with slightly
   different result"). Ours produced `delta_x = -93531` where the recording had
   `-93530`; with the shift chain, `bin/SWOS --replay-selftest` matches
   **10/10** of swos-port's `CalculateDeltaXAndY` reference vectors (it matched
   4/10 before). At `sprites` level this moved the first divergence from frame
   12 to frame 128.
3. **Two DOS result-banner timer constants were missing.** `ShowResult` maps
   its `30000` / `31000` markers to a display length; DOS uses 275 / 165 where
   the Amiga uses 250 / 150 — the same ×1.1 family as every other `GS_*`.
   Now `_gsResultHalftimeLength` / `_gsResultBreakLength` in `dos_gameplay.c`.
4. **A wall-clock value leaked into the match** through `_lastGameTick`, making
   even *our own* runs non-deterministic (see above). That one is a real
   gameplay bug outside the rig: the first frame of every match steps its
   timers by an arbitrary amount.
5. **Frame 0's timer delta was 0 where it should be 1.** Having fixed (4) by
   zeroing both `_currentGameTick` and `_lastGameTick` at kick-off, frame 0's
   `ReadTimerDelta` returned 0 — so `_lastFrameTicks` was 0 for one frame and
   every timer-driven transition in the whole match landed one frame late.
   `GameLoop` reads the delta once *before* the loop and then jumps to the
   vblank wait, so on Amiga one VBlank (one tick) falls between that read and
   the first pass through the body: frame 0's delta is 1 like every other
   frame's. The fix is to start `_lastGameTick` one *behind* the tick, which is
   what that pre-loop read latched. Three recordings had been stalling the
   kick-off at frame 99 (`game_state` still `ST_STARTING_GAME` where the
   recording had moved on) and now run to **912** and **1253**; a fourth went
   from 99 to **1300**.

   Two effects hide in this one line, and they are worth separating. Starting
   the *tick* at 1 gets the same delta, but it also shifts the phase of every
   `currentGameTick & mask` decision in the AI — and that costs two recordings
   several hundred frames (`zkz-10190651` 1796 → 1028, `zkz-10173518`'s
   `sprites` level 117 → 15). Keeping the tick at 0, so it equals the frame
   number as it does in the reference rig, and moving only `_lastGameTick`,
   keeps every gain and loses neither.
6. **The bench-slide window was hard-coded to the Amiga's view.** See below.
7. **A goalkeeper substitution could not move the selection cursor** — the
   guard around it was inverted. See "Two bugs in one substitution" below.
8. **The bench menu's held-direction repeat delay is 2 on Amiga and 4 on DOS**,
   and DOS mode was using 2. Same section.
9. **DOS mode latched the AMIGA pitch ball-speed influence for the whole
   match** — an ordering bug in our own `ApplyGameStyle`. See "The constant
   that was right and the value that wasn't" below.
10. **Every outfield player stood 30 pixels too far up the pitch whenever the
    ball was in the top Y band** — the ball-quadrant loop's leading bound was
    read from the wrong table. See "Two tables that are really one" below.
11. **Placing the ball zeroed all 32 bits of its Z**, where the asm clears only
    the integer half (`clr.w`, against `clr.l` for deltaZ one line later). Every
    set-piece placement started from a different sub-pixel height. Same
    half-write in `RemoveReferee`.
12. **`substituteInProgress` was treated as a boolean where the asm branches on
    its SIGN** (`tst.w` / `bmi`). A positive value falls through to the
    direction and fire handling; we returned, freezing the bench fire timer for
    the whole substitution animation.
13. **The bench's team-pick alternation is a different VARIABLE on DOS**, not a
    different constant — `benchPickTeamCounter`, which advances only when the
    bench check runs, against the Amiga's global `frameCount`. Both are in
    `BenchCheckControls`.
14. **The camera's return from the bench is 50 frames on Amiga, 55 on DOS** —
    one write site each (`SWOS2.s:18791`, `swos.asm:92305`), and the same ×1.1
    family as every other `GS_*`. DOS mode was using 50, so its bench check
    resumed exactly five frames early after every substitution.
15. **`IsThisPlayerOkToSelect` returns an OFFSET, not an error, for an inactive
    player** — and its callers test the sign. See "A routine that answers with
    the wrong number" below.
16. **The player-down timer after a tackle or a jumping header is 50 on Amiga,
    55 on DOS.** Two write sites (`PlayerBeginTackling`,
    `PlayerAttemptingJumpHeader`), checked one at a time rather than scaled:
    `PlayerTackled`'s own 50 stays 50 in the DOS build, and the keeper's 75/15
    and the 20/25s are unchanged everywhere. Two recordings stopped on
    `sprite[N].player_down_timer` counting down from 55 against our 50.
17. **Five "nearest player" scans walked the sprite table backwards** — the
    68000 counts its loop register down while the address register walks up, and
    translating the counter into the index reverses the visit order. Invisible
    until two candidates tie on distance, and then it silently picks the other
    man. See "The loop that counted down while the pointer went up" below.
18. **The goalkeeper's catch gate read the wrong two fields of the goalie-skill
    table** — offsets 0 and 2 where the asm reads 42 and 44. See "Sprite equates
    on a pointer that is not a sprite" below.
19. **`tacklingTimer == -2` was left alone where the asm folds it to -1.**
    `cmpi.w #$FFFE,…; blt` branches away only when the timer is *strictly* less
    than -2, so -2 itself falls through; our `> -2` kept it.
20. **`InitUserTactics` wrote its 5-3-2 forward-row copy to the bare
    `_teamTactics442` / `_teamTacticsSweep` symbols**, so neither variant build
    received it. See "The rows that were never executed" below — that copy also
    settles what looked like the largest DOS-vs-Amiga data difference in the
    project.
21. **The AI's "don't shoot sideways" gate was inverted on both halves.** The
    asm no-fires when the ball is deep in the *attacking* third; ours no-fired
    everywhere else, so a computer player hammered it across his own six-yard
    box and refused the same shot in midfield. See "Three inverted gates in one
    AI routine" below.
22. **A `cmpi.w #$FFFF` arm that `andi.w #7` had already made dead was ported as
    `diff == 7`**, giving the AI a counter-clockwise turn-and-shoot arm the
    original does not have.
23. **`random_rotate_when_no_target`'s two y tests were inverted**, swapping
    "leave the direction unforced" with "run the rotate gate". CPU-vs-CPU only,
    so no recording covers it.
24. **The AI fire-angle tolerance included its own negative bound.**
    `neg.b d1; cmp.b d1,d2; bgt` fires only on `d2 > -tolerance`.
25. **The mid-right after-touch spin band included x == 478.** `bcs` is an
    unsigned *lower* test, so 478 belongs to the midfield band.
### The bench-slide window — the last camera constant that was not `f(view)`

The corpus's two earliest camera stops, `zkz-11021807` at frame 222 and
`zkz-11054632` at 791, were the same bug, and it is worth writing down because
of how narrow the evidence was and how completely it decided the answer.

At frame 222 the recording's camera jumps from a 3.25 px/frame drift to the
5.0 px/frame cap and ours does not. `MoveCamera` moves by
`clamp((target - cameraX) >> 2 * 0.25, ±5)`, so a delta of exactly 3.25 px
means `target - cameraX ∈ [-52, -49]` and 5.0 px means `≤ -80`. With the camera
at 86 and then 83, that is a target of **37 then ≤ 3** — and 37 is
`boundsMargin`, the left bound the target is clamped to. So the reference's
`boundsMargin` went **37 → 0** on that frame, and the only branch in the whole
routine that sets 0 is the subs-menu one, gated on

```
cmpi.w  #303,(cameraY)   ; below the window -> margin 37
cmpi.w  #323,(cameraY)   ; above the window -> margin 37
```

Our camera Y was 360 then 359 — just above 323, so we kept 37 forever. The DOS
release compares against **339 and 359**: 359 is the last frame *inside* its
window, which is exactly where the recording switches. swos-port names the same
pair `kBenchSlideAreaStartY` / `kBenchSlideAreaEndY` with the comment "once the
camera reaches this area it's allowed to slide all the way left".

339 − 303 = 359 − 323 = **36 = 136 − 100**, the difference between the two
releases' half view heights. So this is not a platform difference at all: it is
one more `f(view)` constant of the kind "The simulation view" above already
collects, and it means "the camera has arrived at the bench view, within ±10 of
where the bench target 449 sits once centred":

```c
#define SIM_CAMERA_BENCH_Y_LO ((int16_t)(0x1C1 - 10 - SIM_CAMERA_CENTRE_Y))
#define SIM_CAMERA_BENCH_Y_HI ((int16_t)(0x1C1 + 10 - SIM_CAMERA_CENTRE_Y))
```

303/323 at 272 lines — the Amiga literals, so the m68k codegen is unchanged —
and 339/359 at 200. `zkz-11021807` went 222 → **775** and `zkz-11054632`
791 → **1140**, and with it **no recording's first divergence is a camera field
at a frame the `sprites` level had not already diverged by**. Every remaining
`camera_x_vel` / `camera_y` stop is a symptom of a sprite difference the cheaper
levels cannot see.

Everything else in swos-port's `src/game/camera.cpp` was already right:
`kCenterX` 176, `kPitchMaxX` 352, `kBottomStartLocationY` 664,
`kTrainingPitchMaxY` 616, `kCameraMaxY` 680 all fall out of the existing
`SIM_CAMERA_*` formulas at 320x200, and the three side limits (37 / 51 / 63)
are view-independent in both releases.

### The DOS build is a RECOMPILE of the Amiga one — treat a difference as a bug

Worth stating before any more "DOS says X, Amiga says Y" appears below, because
it sets how much weight such a claim can carry. `swos-port/swos/swos.asm` is a
disassembly of the DOS SWOS **96/97** executable, and our `data/SWOS2.s` is the
Amiga SWOS **96/97** (`SWOS VERSION 152`, 6/11/1996) — the same release. The DOS
one was not written independently: SWOS-RE's README documents it as a **static
recompilation** of the Amiga binary, with fifteen dwords at 0x3146d standing in
for the 68000's D0-D7/A0-A6 (A7 maps to `esp`), which is why its listing reads
`mov A6, offset topTeamData` and `mov word ptr D1, 37`. Non-portable routines
(file, input, video) and a few hot ones were rewritten; the rest is the Amiga
program in x86 clothing.

So the two agree by construction, and the corpus shows it: 401 of 416 recorded
player values reproduce, and thousands of frames of physics and AI line up
field-for-field. **A DOS-vs-Amiga difference is therefore a deliberate hand edit
or a misreading, and the null hypothesis is misreading.** Two of the four
"differences" this rig has turned up were the second kind and dissolved on
inspection:

- the bench-slide window, 303..323 against 339..359 — the same constant
  expressed at a different view height (339 - 303 = 36 = 136 - 100);
- the ball-quadrant leading bound, which the Amiga reaches by adjacency
  (`sub.w -4(a1)` lands on `$0081`) and the DOS spells out as the first entry of
  its own table. Same number, 129. The DOS layout is what *confirmed* the Amiga
  reading.

Two survived, and both were then confirmed by measurement against a recording,
not by reading alone — see the sections below. Both live behind
`_gDosGameplay`; **nothing in this port's Amiga behaviour has been changed on
the strength of the DOS listing.** Where the DOS asm appears as evidence for a
fix to the default path, it is as a second witness to what the Amiga asm already
said.

### Two bugs in one substitution

`zkz-11021807` stopped at frame 459 on `sprite[4].frame_delay` — the top team's
**goalkeeper**. Reading the rest of that frame's report was the whole
diagnosis: our keeper was walking to `(39, 449)`, and `(39, 449)` is where the
recording sends `sprite[5]`, the *outfield* player at lineup position 1. We were
substituting the wrong man.

The v1.3+ frame carries `BenchDataV1`, 30 named fields of substitutes-menu state
that the schema does not decode yet — but Python can. It said exactly what the
reference did: enter "select player to substitute" with the cursor on lineup
position 0, hold DOWN for four frames, step **once** to position 1, fire. Two of
our own bugs sat behind that one sentence.

**One: the goalkeeper guard was inverted.** In `UpdateSubstitutePlayerIndices`,
when the player coming on is index 11 — the reserve keeper, who can only replace
the keeper — the selection cursor is frozen. The asm freezes it only when *more
than five* substitutes are allowed:

```
cmpi.w  #5,(gameMaxSubstitutes)   ; SWOS2.s:19307
bhi     exit                      ; leave when maxSubstitutes > 5
```

We had `<= 5`. Five is the normal maximum, so the cursor was frozen for **every**
goalkeeper substitution in the shipping game: whoever it happened to start on was
the only player you could take off. That is a real gameplay bug, not a replay
artefact, and nothing short of a frame-exact reference was going to point at it.

**Two: the repeat delay is a DOS constant we did not have.** With the cursor
free, a held direction repeats after `subsMovementDelayTimer` reaches a
threshold — `2` in `data/SWOS2.s:18639`, `4` in the DOS asm
(`swos-port/swos/swos.asm:92055`), routines otherwise identical. One compare
site in each listing, so it is not a misread of a second path; and forcing it
back to 2 reproduces the divergence at 459, so the recording agrees. At 2 the four
held frames step the cursor *twice* and we substituted a different wrong player.
Now `_gsSubsMovementDelay` in `dos_gameplay.c`, beside the other GS_* pairs —
though note it is not one of the ×1.1 frame-rate conversions: 2 → 4 is a feel
change (×1.1 would be 3).

Both were needed; forcing either back reproduces a divergence at 459.
`zkz-11021807` went **459 → 1111 at every level**, and it is the first recording
in the corpus where all four levels stop on the same frame and the same field.

> Also found on the way: `src/bench_subs.c` carries **two copies** of this
> routine family — a `static` lowercase set that runs and an
> `externally_visible` uppercase set declared in `bench_subs.h` and called by
> nobody. The first fix went into the copy that isn't used. Both are corrected
> and flagged; merging them is a separate cleanup.

### The constant that was right and the value that wasn't

`zkz-11215003` stopped at frame 704 on `sprite[1].speed` — the ball, recorded
2191, ours 2193 — and the gap stayed at exactly 2 as it decelerated. Frame 704
is the **kick-off**: `game_state` flips 0 → 100 and the ball is played for the
first time.

A `gdb` watchpoint on `_ballSprite.speed`, armed at frame 703, gave the whole
answer in two stops. `PlayerKickingBall` sets 2208 in both. Then `UpdateBall`
takes the first friction step and we land on 2193 where the reference lands on
2191 — friction 15 against 17:

```c
int16_t friction = _kBallGroundConstant;                /* DOS 13, Amiga 16 */
if (nobody has the ball) friction += _pitchBallSpeedFactor;
```

`_kPitchBallSpeedInfluence` held the DOS values ({-3, 4, 1, …}, so
`ApplyGameStyle` had run) but `_pitchBallSpeedFactor` was **2** — a number that
appears nowhere in the DOS table and is exactly the AMIGA table's entry for the
pitch this match rolled ({-2, **2**, 3, …}).

It is an ordering bug, and ours alone. `InitGameRestoreTeams` *latches* three
pitch physics factors out of those tables at match setup; the only
`ApplyGameStyle` call site was `InitTeamsData`, which `StartingMatch` reaches
from **inside** `GameLoop` — after the latch. So DOS gameplay ran every match
with the Amiga's ball-speed influence: two units per frame less ground friction
on every loose ball, for ninety minutes. `ApplyGameStyle` is a pure write of
constants, so the fix is to call it once more at the top of
`InitGameRestoreTeams`.

`zkz-11215003` went **704 → 1592** at `sprites` (and 836 → 1694 at `scalars`),
which makes it the deepest recording in the corpus. Nothing else moved, and
that is the tell: the pitch is chosen randomly per match, and the Amiga and DOS
influence tables differ only in their first three entries — the other
recordings rolled a pitch where the two agree.

### Two tables that are really one

`zkz-12065730` stopped at frame 689 on `sprite[11].dest_y`, and the report made
the shape obvious immediately: **every** player of **both** teams had a `dest_y`
about 30 lower than the recording — 244/214, 203/174, 323/293, 283/253,
242/213. A systematic offset, not a wrong player.

`SetPlayerWithNoBallDestination` builds a target as
`_playerYQuadrantOffset + _playerYQuadrantCoordinates[yIdx]`, and the offset
comes from the ball's Y band:

```c
_playerYQuadrantOffset = (qy - yRefLimit - 45) * 5 / 15;
```

`yRefLimit` is the *previous* band's bound — and for band 0 there is no previous
entry, so the asm reads the word before the table:

```
lea     (ballYQuadrantLimits).l,a1
cmp.w   (a1)+,d2            ; band 0 taken -> a1 = base+2
...
sub.w   -4(a1),d2           ; base-2, i.e. the word BEFORE the table
```

The word before `ballYQuadrantLimits` is the last entry of
`ballXQuadrantLimits`, `$0081` — which is **129**, not an X limit at all but
this loop's missing leading bound. (The X loop's own leading bound sits in
`pad_111202` immediately before *its* table, the same trick.) The DOS build
spells the layout out with no adjacency at all —
`ballYQuadrantLimits dw 129, 220, 312, …` with the loop starting at `+2` — which
is how the reading was confirmed.

Our port had fallen back to `ballYQuadrantLimits[0]` = 220. With the ball at
y 219 that gives an offset of **-15** where the reference computes **+15**: the
whole formation, both teams, thirty pixels too far toward the top goal for as
long as the ball stayed in that band. `zkz-12065730` went 689 → 1073, and three
other recordings moved with it — the ones that had a ball in band 0 at all.

This is the "adjacent-globals" pattern CLAUDE.md warns about, seen from the
other side: not a read that runs off the end of a variable, but a read that runs
off the *front* of one.

### A routine that answers with the wrong number

`IsThisPlayerOkToSelect` decides whether the bench arrow may land on a slot. Its
documented contract is the **sign flag** — "set = not OK, clear = OK" — and the
callers honour that literally: `if (isThisPlayerOkToSelect() >= 0) return;`.

The three rejection paths do `move.w #$FFFF,d0`. The *fourth* does not:

```
loc_10D204:
    move.w  (benchPlayerIndex).l,d0
    addi.w  #$A,d0
    mulu.w  #PLAYERINFO_SIZE+PLAYERINFO_SKIP,d0   ; d0 = the byte offset
    ...
    tst.b   PLAYERINFOHEADER_POSITION(a0)
    bmi.s   locret_10D230                         ; straight to the rts —
    ...                                           ; d0 is STILL the offset
locret_10D230:
    rts
```

So a player with a negative `position` — the marker `SubstitutePlayer` writes on
the slot of the man who just came off — returns `(benchPlayerIndex + 10) * 61`,
a small positive number, and the caller reads that as **selectable**. It looks
like an original-game slip; it is nonetheless the behaviour, and the DOS
recompile inherits it verbatim (`js short @@out`, then
`mov ax, word ptr D0; or ax,ax; retn`).

Our port returned the negative position, so the arrow skipped that slot. The rig
caught it precisely: `bench.arrow_player_index` recorded 1, ours 2, on the exact
frame the player pressed DOWN — and the fire that followed then offered a
substitution for the wrong man. `zkz-11021807` went **1178 → 2262** (of 4064
frames), and with the bench-camera constant beside it, **1111 → 3390** at
`scalars`.

Worth keeping as a pattern: a routine whose contract is a *flag* can return a
*value* on some paths, and translating it as "return the thing that was wrong"
is a plausible, wrong guess. Read what the register actually holds at the `rts`.

### The loop that counted down while the pointer went up

`playa-10090613` stopped at frame 795 on `bottom_info.pass_to_player_ptr` —
`sprite[11]` recorded against our `sprite[14]`. Every input to that choice
matched: `on_screen`, `sent_away`, `player_ordinal`, `player_state`, and every
`ball_distance`. The scan picks the nearest eligible team-mate, so with identical
inputs it should pick the same man.

It does, unless there is a **tie**. Printing the eleven candidates showed one:

```
[ 7] onScr=1 away=0 ord=8  st=0 bd=1625
[ 9] onScr=1 away=0 ord=10 st=0 bd=34     <- the controlled player, excluded
[10] onScr=1 away=0 ord=11 st=0 bd=1625
```

Slots 7 and 10 at exactly 1625. The update is strictly-less-than
(`cmp.l d5,d1 / bcc` — skip when not lower), so the winner is simply **whoever
is visited first**, and the two ports visit in opposite orders:

```
    move.w  #10,d6                  ; counter runs DOWN
loop:
    movea.l (a1)+,a2                ; table pointer runs UP
    ...
    dbf     d6,loop
```

The 68000 idiom counts the loop register down while the address register walks
forward, so the players are visited **0..10**. Translating the counter into an
index — `for (i = 10; i >= 0; i--) sprTable[i]` — reverses that, and nothing
shows until two candidates tie.

Five scans had it backwards, all of them "nearest player" searches:
`UpdateControlledPlayer`, both paths of `UpdatePlayerBeingPassedTo`,
`GetClosestNonControlledPlayerInDirection`, and the nearest-team-mate search in
`tackle_foul.c`. The rest of the `for (i = 10; i >= 0; i--)` loops in the tree
are fine: some already walk a pointer forward inside the descending counter
(which is the faithful translation), and the others set a field on all eleven
players, where order cannot matter.

`playa-10090613` went 795 → 912 and `zkz-11021807` went from 3390 frames to
**all 4064 — the first recording in the corpus to replay end to end.**

> The matrix script then reported that as a regression to frame 500, because its
> "how far did it get" regex took the first `N/M frames matched` line in the log
> and a passing run's first such line is a progress tick. Fixed; worth
> remembering that the harness can lie in the direction of bad news too.

### Sprite equates on a pointer that is not a sprite

`playa-10090613` stopped at 912 with `game_state` 100 recorded against our 3 —
`ST_KEEPER_HOLDS_BALL`. Our goalkeeper caught the ball; the reference played on.

Both keepers reached the same intervention block (`team->field_46` is set there,
and it is non-zero in both). What differs is the gate just after it:

```
    move.w  currentGameTick(pc),d1
    andi.w  #$F0,d1
    lsr.w   #4,d1
    sub.w   SPRITE_DIRECTION(a0),d1     ; a0 = the GOALIE SKILL entry
    bmi.w   comment_timing_gate_fail_dir    ; -> claim the ball
    sub.w   SPRITE_SPEED(a0),d1
```

`a0` is `TEAMGENERALINFO_SHOTCHANCETABLE(a6)` — a `GoalieSkill *`, not a
`Sprite *`. The disassembly is applying **Sprite** equates to a foreign pointer,
and `data/SWOS2.s` says what they are: `SPRITE_DIRECTION EQU 42`,
`SPRITE_SPEED EQU 44`. So the gate reads `GoalieSkill` offsets **42 and 44**
(`field_2A` / `field_2C`), not 0 and 2. Our port read 0 and 2, so the keeper's
"do I claim this?" threshold was a different number entirely, and he claimed on
frames he should have played on.

The sibling gate forty lines earlier survives as the control: the disassembly
writes that one as a plain `sub.w 48(a0),d1`, our port reads `field_30`, and it
was right all along.

`playa-10090613` went **912 → 3917** at `scalars` and 912 → 1419 at `sprites`;
`playa-10095214` 3275 → 3505.

> A named equate is only a name. When a disassembly applies one struct's equates
> to another struct's pointer, the offset is the fact and the name is noise —
> and "direction/speed" reads so plausibly as "the first two fields" that it
> goes straight past review.

Two smaller things fell out of the same recording:

- `sprite[N].tackling_timer` at -1 recorded against our -2. `cmpi.w #$FFFE,…`
  followed by `blt` branches away only when the timer is *strictly* less than
  -2, so -2 itself falls through and is folded to -1; our `> -2` kept it.
  1419 → 2694 at `sprites`.
- `top_info.field_46` is a 16-bit flag the asm sets with a **byte** write
  (`move.b #1,$46(a6)`), so its numeric value is endian-dependent by design: the
  byte lands in the high half on the Amiga (0x0100, what this port stores) and
  the low half on the DOS build (0x0001, what the recordings hold). Every reader
  is a truth test and the clear is `&= 0xFF`, so the schema now compares it as a
  boolean — the same class as the `sne`-set flags in "Known non-comparable
  state".

### The rows that were never executed

For a while this document said the DOS release rewrote the 4-4-2's and the
Sweep's forward line — 121 bytes across four slots, dwarfing every other
difference — and that the corpus contradicted it, since fifteen recorded
forwards matched the Amiga rows and none matched the DOS ones. Both halves were
wrong in the same way.

`InitUserTactics` starts like this, in **both** disassemblies:

```
    lea  ((teamTactics532+…+$118)).l,a0     ; SWOS2.s:43757
    lea  ((teamTactics442+…+$118)).l,a1
    lea  ((teamTacticsSweep+…+$118)).l,a2
    move.w #69,d0
loc_120528:
    move.b (a0),(a1)+
    move.b (a0)+,(a2)+
    dbf    d0,loc_120528
```

`$118` is 280, which is `positions[8]`, and 70 bytes is exactly two player rows.
**At startup the 5-3-2's last two rows are copied over the 4-4-2's and the
Sweep's.** Whatever those two formations have baked in slots 8 and 9 is dead the
moment the game boots. The Amiga's baked rows already equal the 5-3-2's, so the
copy changes nothing there and the redundancy is invisible; the DOS binary's are
different, and never run.

So the corpus was right and the disassembly was being read too literally: the
recordings' forwards match the 5-3-2's rows because that is what every 4-4-2 and
Sweep uses at runtime, on both releases. The real Amiga-vs-DOS difference is
**21 coverage bytes** across three formations, not 142.

Two fixes came out of it, and neither is in the data:

- **`InitUserTactics` wrote to the bare `_teamTactics442` / `_teamTacticsSweep`
  symbols.** With `SWOS_ORIGINAL_TACTICS` or `SWOS_DOS_TACTICS` the table points
  at variant copies, and those never received the copy. It now writes through
  `_teamTacticsTable`, which is the set the game actually plays with — and which
  is what the asm's `lea teamTactics442` means.
- **The rig never called it.** `InitUserTactics` runs in the front end
  (`InitMainMenuStuff`), which the rig skips, so the replay was using whatever
  was baked. `applySetup` now calls it before installing the recording's own
  user tactics, which is the order the real game loads a `.TAC` in.

With both, `make clean && make SWOS_DOS_TACTICS=1` scores **384/384** on the
corpus's recorded player values — every outfielder in every decodable recording,
including Eranio and Ayala, the two the shipped Amiga data gets wrong. The
shipped default is unchanged at 382/384, which is what it should be: those two
are genuine Amiga-vs-DOS coverage bytes.

> The lesson is the one this document keeps relearning from the other side: a
> disassembly shows what is *stored*, not what is *executed*. 121 bytes of
> difference sat in the listing looking like the biggest finding in the file,
> and a nine-line loop at startup made all of it moot.

### Three inverted gates in one AI routine

`zkz-10190651` stopped at frame 1113 with the AI's bottom team still holding the
ball in the recording — `player_has_ball` 1, `controlled_player` `sprite[25]`,
`ball_can_be_controlled` 1 — and having just hit it in ours: `normal_fire` 1,
`shooting` 1, `pass_kick_timer` 25, `passing_kicking_player` `sprite[25]`,
`controlled_player` null. Everything else in the frame follows from that one
decision: the ball's `delta_x`, `dest_x` and `z`, both possession counters, the
camera velocities on the frame after.

Frame 1112 matched field for field, so the fork is inside one call to
`AI_SetControlsDirection` with identical inputs. Printing the gates it walks
gives the answer in one line:

```
[ai 1113 bot] tolgate tol=15 d2=6 d2b=-4 d6=21640 d5=196 aiRand=220 aiResume=0 pkt=0
```

Every gate passes, so we fire. The reference leaves for `no_fire_fallback_flow`
one gate earlier:

```
horizontal_dir_half_gate:                       ; SWOS2.s:32052
                cmpa.l  #topTeamData,a6
                beq.s   top_team_ball_y_gate
                cmpi.w  #158,(ballSprite+SPRITE_Y).l
                ble.s   no_fire_fallback_flow
                bra.s   distance_and_random_fire_gate
top_team_ball_y_gate:
                cmpi.w  #740,(ballSprite+SPRITE_Y).l
                bge.s   no_fire_fallback_flow
```

A player facing sideways — direction 2 or 6 — does **not** hit the ball while it
is deep in the attacking third. The bottom team attacks the y=129 line, so
`ballY <= 158` is its "in the box" case; the top team's is `ballY >= 740`
against y=769. There the fallback flow turns him toward goal instead, which is
the whole point of the gate. Both tests read the other way round in this port:
we declined the shot out in midfield and took it from six yards out, facing
across the goal. The ball was at y=147 and `sprite[25]` was facing 6.

Auditing the rest of the routine against the listing turned up two more
inversions of the same shape. Neither is visible in this corpus:

- **`decide_immediate_turn_or_gate`** (SWOS2.s:32240) tests the
  bucket-minus-facing delta against 0, 1 and `$FFFF` — but the compare sits
  after `andi.w #7,d0`, so the third arm can never be taken and only a bucket
  *at* or one step *clockwise* of the facing gets the shot. Reading that arm as
  `diff == 7` gave the AI a counter-clockwise arm the original does not have.
  The same trap as the tactics forward rows, in miniature: a disassembly shows
  what is stored, not what is executed.
- **`random_rotate_when_no_target`** (SWOS2.s:32458): the two x tests jump **to**
  the gate and the two y tests jump **past** it, so a player inside the central
  corridor and already deep in a box gets no forced direction at all, and
  everyone else reaches the gate. Ours had the y arms the other way round,
  swapping the two groups. It runs only when **both** teams are
  computer-controlled — which sounded like "no recording covers it" until the
  matrix said otherwise: `zkz-10174427` is a **coach-mode** match
  (`top_team_player_no` and `bottom_team_player_no` both 0, the human's
  `team_controls` 3), the game plays itself, and the fix took it 714 → 889.
  Worth remembering before writing off a CPU-only path as untestable.

And one off-by-one on each of two bounds:

- The fire-angle tolerance is `cmp.b d1,d2 / bgt no_fire` followed by
  `neg.b d1 / cmp.b d1,d2 / bgt fire`, so it fires on
  `-tolerance < d2 <= tolerance` — asymmetric by one, because the second test is
  strict. Ours used `d2 < -tolerance` and let the negative bound itself through.
- `cmpi.w #478,ballX / bcs mid_right_x_gate` is an unsigned **lower** test, so
  x == 478 belongs to the midfield spin band rather than the mid-right one.

`zkz-10190651` went **1113 → 2570** at every level, `playa-10095214`
3505 → 5276 at `scalars`, and `zkz-10174427` 714 → 889.

> Three inversions in one routine is not three mistakes, it is one habit: an
> `ble`/`bge` that jumps to the *reject* label reads naturally as the accept
> condition. When a gate's branch target is the failure path, the C `if` has to
> carry the same sense as the branch, not the opposite of it.

### The `onScreen` ceiling — how far a DOS corpus can go

`UpdateControlledPlayer` — auto player-selection — **skips players whose
`onScreen` is clear**:

```c
if (_gameStatePl == ST_GAME_IN_PROGRESS) {
    if (!pl->onScreen)
        continue;
}
```

`onScreen` is set by the sprite cull in `sprites.c`, against the camera, the
view size and the per-image `centerX`/`pixWidth`/`nlines` metrics. So the view
geometry does not merely change the camera — **it feeds back into the
simulation**, and GAMEPLAY.md §5, §25.2 and §25.5 all say so.

That was worth 200+ frames per recording, and simulating at the recording's own
geometry (above) recovers it. The worked example that found it,
`playa-10090613` frame 660, when the rig still simulated at 336x272:

| | |
|---|---|
| recorded `top_info.controlled_player` | `sprite[23]` (team2Player9), world y 449.1 |
| ours | `sprite[21]` (team2Player7), world y 373.7 |
| DOS camera / view | (156.5, 389.8), 320x200 → y spans **390..590** |
| ours | 72 higher, 272 tall → y spans ~318..590 |
| recorded `sprite[21].on_screen` | **0** — above the top of the DOS view |

Our taller view put `sprite[21]` on screen, it was nearer the ball, so we
selected it; DOS could not see it. Simulating at 320x200 removes that entirely.

**What was left turned out to be two bugs of our own, not the artwork.** An
earlier revision of this document claimed the residual `on_screen` differences
came from the two releases' sprites not being pixel-identical, and that the
per-image `centerX`/`pixWidth`/`nlines` therefore could not match. **That was
wrong.** The DOS and Amiga banks are a 1:1 conversion (see
`SwosGfx/DosToAmiga.cs`) and the metrics are identical — checked directly:

| sprite | our Amiga bank | DOS `TEAM1.DAT` / `BENCH.DAT` |
|---|---|---|
| amiga 227 / dos 341 | `w=12 h=15 cx=4 cy=12` | `pixWidth=12 nlines=15 centerX=4 centerY=12` |
| amiga 1091 / dos 1205 | `w=73 h=7 cx=0 cy=23` | `pixWidth=73 nlines=7 centerX=0 centerY=23` |
| amiga 1092 / dos 1206 | `w=73 h=28 cx=0 cy=27` | `pixWidth=73 nlines=28 centerX=0 centerY=27` |

(`SWOS_DEBUG_SPRITE_LOAD=1` prints the left column.) With that ruled out, the
recordings themselves give the cull window exactly. Taking the per-image
metrics from the DOS banks and conditioning each axis on the other being
comfortably inside, across all 1519 frames of `zkz-12065730`:

```
X (vertically inside):   onScreen=1 max d1 = 335    onScreen=0 min d1 = 336
Y (horizontally inside): onScreen=1 max d2 = 199    onScreen=0 min d2 = 200
```

A clean boundary with no overlap: the window is **336 x 200**. Note the width —
the DOS release's visible screen is 320 wide, but it culls to 336, the Amiga's
buffer stride, which the DOS port evidently inherited. Only the height follows
the display. So:

- **the cull width is a constant 336 in both releases**, not the view width;
- the cull height is the view height (200 DOS, 272 Amiga).

Two of our own defects fell out of fixing that:

1. **The cull keyed off the render width.** That is the widescreen bug below,
   and it is now fixed rather than filed.
2. **`onScreen` was overwritten after the enqueue.** `BuildSpriteBandAnd­
   GeneralRenderStacks` ended each iteration with an unconditional
   `a1->onScreen = 1`, which was harmless while "culled for gameplay" and
   "culled for drawing" were the same test and wrong the moment they are not —
   a sprite outside the simulation's window but inside a wider render view got
   drawn (right) and then read back as on-screen (wrong).

With both fixed, the `sprites` level goes from **116-147 frames to 689-840**,
and it no longer stops on `on_screen` at all.

**The widescreen bug is fixed.** The cull used to test the *live* render width,
so a 650-wide view admitted players the Amiga culls, one of them was nearer the
ball, and `UpdateControlledPlayer` picked a different man — **ultrawide changed
CPU behaviour.** `onScreen` is now decided by a fixed 336-wide window (and
`_sim_view_height`), while whether to *enqueue* a sprite still keys off the
render width, so a wide display draws everything it can fit. That is exactly
what GAMEPLAY.md §25.5 prescribes: cull to a fixed window, let the renderer
draw wider. At 4:3 and on Amiga the two windows coincide and nothing changes.

### Known non-comparable state

Not bugs; consequences of comparing two different builds.

- **View geometry.** SWOS's camera *is* the view's top-left in world space and
  every camera constant is derived from the view size: kick-off camera
  `((pitchWidth - viewWidth)/2, pitchBottom - viewHeight)`, `MoveCamera`
  centring on `target - viewWidth/2`, pan range `pitchWidth - viewWidth`. DOS
  renders 320x200, we render 336x272, so a DOS recording's kick-off camera is
  `(176, 664)` where ours is `(168, 592)` — not one offset, since X is centred
  (half the difference) and Y is bottom-anchored (all of it). Sprite
  coordinates are WORLD coordinates and are unaffected. The camera fields and
  `Sprite.onScreen` carry `cmp = view` and are skipped, with a line saying how
  many.
  **This also caps the `sprites` level**: `updateSpriteAnimation` only advances
  a sprite whose `onScreen` is set, so at a different view size animation state
  itself diverges (frame 128 of `zkz-12065730` is `sprite[14]`'s animation
  never starting). Sprite-exact replay needs a recording made at our geometry
  — see section 7.
- **Sprite-bank indices.** DOS sprite indices are ours + 114
  (`DOS_TO_AMIGA_OFFSET`, `include/dos_graphics.h`) — confirmed independently
  here as exactly the delta on all 33 sprites' `imageIndex`. Handled by
  `cmp = spr` plus `sprite_index_offset` in the `.swr` setup.
- **Byte-set word flags.** `SWOS2.s` writes several 16-bit fields with a byte
  `sne` — `ballOutOfPlayOrKeeper` and `goaliePlayingOrOut` (lines 29236-29243),
  `Sprite.isMoving` (29122). "Set" is `0x00FF` little-endian and `0xFF00`
  big-endian; ours says `-1`, which is the same flag without the byte trick,
  and every read is a truth test (`tst.w`, `cmp.b`). Marked `cmp = bool`.
- **Two sprite slots have no counterpart here at all** (`cmp = na`, never
  compared, whatever the geometry). Slot 31 is a corner flag and swos-port
  keeps *four*, picking one by camera position; our port has the original's
  single `_cornerFlagSprite`, so the recording is describing one of four
  objects we do not have. Slot 32 is the player-name HUD sprite, which
  swos-port's own rig `continue`s past rather than field-comparing. Both were
  first classified `view`, which was right in effect and wrong in reason — and
  the moment the simulation view matched, `view` stopped skipping them and they
  reappeared as false divergences.
- **swos-port's own sprite init.** Its `Sprite::init()` sets several fields to
  values its source explicitly calls "for SWOS++/tests compatibility", one of
  which (`frameSwitchCounter = -1`) its comment says SWOS itself starts at 0 —
  which is what we do. `scripts/replay_test.sh` mutes `starting_direction`,
  `frame_switch_counter`, `field_3e`, `player_direction` and `save_sprite` at
  the `sprites` level; `--strict` compares them.
- **v1.0/1.1 headers** stop before `season` / `min` / `maxSubstitutes`. The
  converter falls back to the DOS data segment's own defaults for the
  substitute counts (2 and 5); leaving them 0 benches players 11..15 before the
  first frame. `season` has no safe default, so the rig keeps this build's
  value and says so.

### Open leads

Ranked by how early they stop a recording. See
[TESTS_RESULTS.md](TESTS_RESULTS.md) for which file stops where.

1. ~~**The camera**~~ — **closed as a primary lead.** One view-dependent
   constant was missing (the bench-slide window, see "The bench-slide window"
   below); with it, `zkz-11021807` goes 222 -> 775 and `zkz-11054632` 791 ->
   1140, and **no recording's first divergence is a camera field at a frame the
   `sprites` level hadn't already diverged by.** Every remaining `camera_x_vel`
   / `camera_y` stop is downstream of a sprite difference that the cheaper
   levels are simply not looking at — chase the sprite, not the camera.
2. ~~**Inject the recorded BENCH bit**~~ — **done.** The rig injects the
   recorded per-team bench button on its rising edge (section 6b);
   `zkz-11215003` goes 117 -> 836/836/711/704, so a 27819-frame recording is
   back in play and the whole substitutes-menu sequence — bench camera pan,
   menu, return — replays field-for-field.
3. ~~**`zkz-10174427` frame 0** skill nibbles~~ — **closed.** One byte of
   4-4-2 coverage (row 4, ball position 0: Amiga `0x33`, DOS `0x23`), worth the
   exactly-one bucket the `÷350` needs; 401 of the corpus's own 416 recorded
   player values reproduce (see "The player-price audit"). Not a porting defect
   — but it is the visible end of a real gap: **DOS-gameplay mode does not
   switch the re-tuned formations** (`project_dos_tactics_gap`). The same
   measurement leaves one open item of its own: our **Sweep** row 1/row 2 at
   ball position 15 is contradicted by the corpus, and by the pristine Amiga
   and DOS data both.
4. **Player selection and possession** — still the largest cluster, and its
   clearest sample is now closed: `zkz-10190651` at 1113 was the AI firing a
   shot the reference did not take, and the gate that should have stopped it was
   inverted (see "Three inverted gates in one AI routine"). That took the
   recording to 2570, `playa-10095214` to 5276 and `zkz-10174427` to 889.
   What remains: `controlled_pl_direction` (1108 of `zkz-10140228`, 1976 of
   `playa-10095214`), `current_allowed_direction` (1169 of `zkz-11054632`), and
   `top_info.controlled_player` at `zkz-10190651`'s new stop. All four are on
   the **human**-controlled side of their recording — check the setup before
   assuming otherwise, as the AI fix above did not move any of them — so they
   point at `team_control.c` and the kick-direction writers in
   `ball_possession.c` / `headers.c` / `tackle_foul.c`, not at the AI. The
   method that closed 1113 transfers: print the gates on the diverging frame
   rather than reasoning about them.
5. **Sprite state**, at 939-2694 (setting aside `zkz-10174427`, whose frame-0
   stop has a known cause): `dest_y`, `x`, `z`, `player_down_timer`,
   `frame_delay`. These are downstream of (4) in every case so far, so start
   there — `sprites` is the earliest stop (or tied with it) on all twelve
   recordings, so it is where the corpus still has something to say. The
   earliest is now `playa-10090613` at 795 on `bottom_info.pass_to_player_ptr`,
   which is really (4).
6. ~~**Decode `BenchDataV1` in the schema**~~ — **done** (section 6b), and it
   paid three times over: the `substituteInProgress` sign bug, the bench-camera
   constant, and `IsThisPlayerOkToSelect`. `zkz-11021807` went 926 → 2262 on
   the back of it. `bench.pl2_prev_direction` at 1251 of `zkz-12065730` is the
   next thread from the same spool.
7. The single **v1.2** recording still resolves to no (header, frame) pair.

---

## 6. Design notes

Two pieces, deliberately split so the fragile part is offline and in Python.

### 6a. `scripts/rgd_convert.py` + `scripts/rgd_schema.py`

`rgd_schema.py` is the DOS layout, pinned in one readable place; `--emit`
walks a `.RGD` through it and writes a `.swr` of named fields. The C rig never
parses a DOS byte, so 32/64-bit and pointer growth stop mattering, and the same
container can carry Amiga dumps later.

`.swr` layout (little-endian):

```
0   char magic[8]  "SWOSREC\0"
8   u32  fileVersion (1)
12  u32  setupOff,  setupLen     JSON — the match setup
20  u32  schemaOff, schemaLen    TSV — name <TAB> type <TAB> level <TAB> cmp
28  u32  blobOff,   blobLen      the fixed-layout binary records
36  u32  framesOff, frameCount, frameSize
48  u32  reserved[4]
64  ... sections, in that order
```

Frames are the schema's fields packed back to back in schema order, which is
why the schema carries the types: the reader needs a name and a width, never an
offset. The blob section holds the two 684-byte team records and the six
370-byte user tactics — fixed-layout binary records of exactly the kind
`.EDT`/`.TAC` support is kept for, and setup state rather than per-frame state.
The rig copies each whole and then restores the one endian-sensitive field
(`TeamFile.globalTeamNumber`) from the named scalar beside it.

### 6b. `src/replay_test.c`

A mode of the SDL binary rather than a separate harness: it needs the real
globals. It also has to skip the front end, which means doing by hand the few
things the front end would have done — `ProcessImageCommandStream` for the menu
sprite bank (the stadium line-up SIGSEGVs without it), `InitTeams`, and the
control assignment the two `ShowPlayMatchMenu` prompts would have produced
(`ReplayTest_SetupPlayers`, called from `SetupPlayers` and mirroring the
arcade-mode branch beside it).

The **bench button** is injected per TEAM, on its rising edge. swos-port's
`TeamGeneralInfo` is our 144 bytes plus a trailing `secondaryFire` it sets from
its own bench event, so the recording says which team's bench a press belongs
to and the rig does not have to infer it from who drives whom — the schema
exposes that byte as `top_bench_button` / `bottom_bench_button`, injected and
never compared (we have no such field). The edge matters:
`MainKeysCheck` sets `bench1Called` / `bench2Called` from a key EVENT, and
inside the menu the same flag is what *leaves* it, so re-asserting a held
button would open and close the bench on consecutive frames.

The v1.4 **`userKey`** goes in as a real keypress: the rig reverses `keycodeLut`
to find the scancode that maps to the recorded ASCII and pushes THAT into
`keyBuffer`, so `GetKey` and `MainKeysCheck` run their own guards rather than
the rig faking their result. Only 'S' (the stats screen) appears in the corpus.

Still not injected: the recorded **ESC** bit. It blocks nothing — every ESC in
the corpus is on the recording's last frame, since it is how the match ends. The
rig warns once when a recording uses one.

**`BenchDataV1` is decoded** for v1.3+ recordings — 26 named fields of
substitutes-menu state at level `teams`, which is how the last two bench bugs
were found. Five are muted as recorder conventions rather than state we have:
the two team pointers and the two arrow/menu indices are swos-port's own
encodings, `shirtNumberTable` is a display cache we do not keep, and `controls`
/ `last_direction` are its live per-frame copies where SWOS keeps a variable
that is only written on the frames it samples. The other twenty-one are real,
and one of them — `alternate_teams_timer` — is what confirmed the DOS
team-pick counter is right for 926 frames before it drifts by one.

`checkPlayerValues` is the one comparison that is not per-frame. It runs
between `installTeam` and `InitTeams`, while the recorded team records are
still untouched, and it puts everything back afterwards — the 16
`cachedMatchValue` bytes, `playerValuesDirty`, and `_playerPositionsArray`, all
of which `UpdatePlayerPositionAndReturnRole` writes. That restore is what keeps
it from perturbing the replay; the frame depths in TESTS_RESULTS.md are
unchanged with the check in.

---

## 7. Order of work

1. ~~Decide how the 66 MB is stored.~~ Still untracked; `tests/replay/` (the
   converted `.swr` files) is ignored too — regenerate rather than track.
2. ~~`scripts/rgd_convert.py`~~ — done; 12 of 13 files decode and verify.
3. Resolve v1.2 (1 file, no (header, frame) pair found).
4. ~~The C rig~~ — done; see section 5 for where each level reaches.
5. Chase the open leads in section 5, cheapest level first.
6. Inject bench / ESC / `userKey` so recordings that use them stay comparable.
7. **The Amiga dumper.** Now the priority rather than a nice-to-have: the view
   geometry cap means a DOS recording can never validate sprite-level state
   against this port, and a divergence in DOS data always has to be triaged
   against "is this a DOS-vs-Amiga difference?" first. An Amiga corpus removes
   both problems — every divergence there is a porting bug.

---

## 8. Harvesting Amiga data

Target: `C:\Projects\Amiga\SWOS-RE\Amiga\WHDLoad\Static` — a buildable
disassembly (`SWOS/SWOS2.s`, 5.6 MB) plus a WHDLoad slave
(`SWOS.Slave/SWOSHD.s`). Built with the **Amiga Assembly VSCode plugin** (not
the amiga-debug fork our C port uses).

**Hard constraint: nothing may move.** `SWOS/SWOS2.ld` pins `text`/`data` at
`0x00100000` and `bss` at `0x0015736A`. Inserting so much as one instruction
into `SWOS2.s` shifts every later label, and the static build then no longer
behaves like the original — which is the whole reason it exists.

### Recommended: patch from the slave, don't touch SWOS2.s

`SWOSHD.s` already carries a WHDLoad patch list, so the mechanism is proven in
this exact build:

```
    PL_START
    PL_P    $670a,fix_memory_routine_9697_1   ; JSR to our routine
    PL_PS   $76a4,access
    PL_PSS  $23ac,fix_snoop_bug,4
    PL_W    $CC,$6002                          ; poke a word
    PL_R    $fd0                               ; remove
```

`PL_P` / `PL_PS` overwrite instructions **in place** at a fixed address with a
call into a routine that lives in the *slave's* own memory. The image keeps its
exact layout; only the few patched bytes change, and the routine re-executes the
displaced instructions before returning. That satisfies "don't move data
around" precisely, and it keeps the dumper out of the game's address space.

Steps:

1. **Pick the hook.** The per-frame point in the in-match loop, matching where
   the DOS recorder samples — after the tick's state update, before render.
   Find it in `SWOS2.s`; cross-check against the same routine in our
   `data/SWOS2.s`, which shares its provenance.
2. **Write the dumper in the slave**, next to the existing fix routines: save
   registers, walk the globals the schema names, append one frame record,
   restore, run the displaced instructions, return.
3. **Emit the `.swr` container directly** — same schema names as
   `scripts/rgd_schema.py`, with `view_width`/`view_height` set to the Amiga's
   336x272 and `sprite_index_offset` 0, so the rig compares the camera and
   `onScreen` too and the `spr` class becomes a no-op. Do not reinvent `.RGD`:
   the Amiga structs are not the DOS ones, and hand-decoding a second binary
   layout is exactly the work the converter exists to avoid.
4. **Write it out** with WHDLoad's `resload_SaveFile`. 31 k frames × ~4.4 KB is
   ~140 MB, so dump a bounded window (a few thousand frames) or only the fields
   one level needs — the schema's `level` column is there for this.
5. **Verify nothing moved**: diff the built image against an unpatched build —
   byte-identical except the patched instructions. Same discipline as checking
   `bin/SWOS2.map` for the C port.

### Why the Amiga corpus matters

The DOS corpus validates DOS gameplay tuning, and it has already paid for
itself (section 5). But its view geometry differs, its sprite bank differs, and
every divergence has to be triaged against "DOS-vs-Amiga difference?" first.
The Amiga recordings validate the path the port actually derives from
(`data/SWOS2.s`), where any divergence is a genuine porting bug.
