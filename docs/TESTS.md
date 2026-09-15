# Gameplay regression testing — recorded-game replay

Deterministic gameplay testing for OpenSWOS: replay a recorded match
frame-by-frame and assert that our simulation reproduces the reference state
*exactly*, every frame.

The idea is borrowed wholesale from **swos-port**
(`C:\Projects\GitHub\swos-port`), whose `tests/src/tests/RecordedDataTest.cpp`
replays `.RGD` recordings against its DOS port. That corpus is our starting
point; an Amiga corpus follows once we can harvest it (see
[Harvesting Amiga data](#harvesting-amiga-data)).

The corpus is the **original DOS game**: swos-port is a static recompilation, so
the recordings capture SWOS's own code executing. An earlier version of this
document called them "SWOS++, not retail DOS" and treated a five-field
exclusion list as permanent — that was wrong, and "Can we claim 100% accurate?"
now has the correction.

Status: **live.** The corpus decodes, converts and replays; all 12 decodable
recordings run, and **two of them replay end to end at every level** —
`zkz-11054632`'s 7314 frames and `zkz-11021807`'s 4064, every bound field on
every frame. The rest reach 1251-14519 at level `info` (section 5).
`zkz-10174427` had stopped at **frame 0** since the rig was built and now runs
**14519 frames, 54.5% of itself** — the coach-mode recording turned out to be
the one holding three separate defects nothing else could see. Thirty-two real
defects found and fixed on the way in, and
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

A converted `.swr` is **reused** when it is newer than the recording, the
converter and the schema — a full matrix regenerates ~600 MB of them otherwise,
which on a `/mnt/c` working copy costs more than the twelve replays it feeds.
`--frames` never reuses (a windowed emit is not the whole file), and deleting
`tests/replay/` forces a rebuild.

A replay is headless **and silent** by default: `--replay-test` sets
`SDL_VIDEODRIVER=dummy` (unless `--replay-window`) and `SDL_AUDIODRIVER=dummy`,
so a matrix run opens no windows and makes no noise. Both respect the variable
if the caller has already set it. The dummy drivers keep every buffer, blit and
audio-queue drain real — the simulation is bit-identical, it just goes nowhere.

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

Each cell carries the frame reached **and that frame as a share of the
recording's own length**, and the header carries the corpus-wide figure — the
sum of the frames matched over the sum of the frames recorded, per level. That
last number is the honest headline: 100% of it is not reachable against a DOS
corpus (see "Can we claim 100% accurate?"), but it moves for exactly one reason,
which is a defect being fixed.

The shape of the results, as of the run in that file: **1252-14519** frames at
`scalars`, **1251-21927** at `info` and at `sprites` — **40.9%** of every
recorded frame in the corpus at `scalars` and **40.8%** at `sprites`, which are
now within a tenth of a point of each other — and **401 of 416**
on the per-player match values (the `values` column — a setup-state check that
runs before frame 0; see "The player-price audit"). Every recording is in those
ranges now: `zkz-10174427`'s frame-0 stop was the coverage byte that
`--dos-gameplay` was not switching, and with the DOS formations plus the two
AI defects it uncovered it now runs to 14519.

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

Closer than this document used to say, and the correction is worth writing down
because the wrong version was load-bearing for months.

**It said: "the recordings are SWOS++, not the retail DOS game." That is wrong.**
swos-port is a *static recompilation* — `swos/swos.asm` is the DOS binary's own
code with a register file standing in for D0-D7/A0-A6, and the C++ around it is
a platform layer. The simulation the recordings capture is the original DOS
program executing, instruction for instruction. **The recordings are the
original game.**

What misled the earlier reading is two comments, and they are both in one place:

```c
    // src/sprites/Sprite.h — swos-port's own C++ Sprite class
    void init() {
        int offset = offsetof(Sprite, frameOffset);
        memset(reinterpret_cast<char *>(this) + offset, 0, sizeof(*this) - offset);
        // this is for SWOS++/tests compatibility
        playerDirection = playerOrdinal ? 0 : -1;
        ...
        // SWOS initial value for frameSwitchCoutner is 0, but we will initialize
        // with -1 to be compatible with SWOS++ and tests
        frameSwitchCounter = -1;
```

That is a **constructor**, not the game. swos-port builds its sprites in C++
instead of using SWOS's static sprite table, and the `memset` from
`offsetof(frameOffset)` throws that table's initial values away before the
recompiled code ever runs. Everything the constructor touches therefore carries
swos-port's values at frame 0 — two of them labelled SWOS++, the rest simply
zero because they were memset.

The five fields the `sprites` level mutes are exactly that constructor's
footprint, and checking each against `data/SWOS2.s` settles who is right:

| field | recorded | ours | SWOS says |
|---|---:|---:|---|
| `starting_direction` | 0 | -1 | **-1** — `ballSprite`/`team1Player2Sprite` static data |
| `frame_switch_counter` | -1 | 0 | **0** — and swos-port's own comment says so |
| `field_3e[4]` / `[5]` | 1 / 0 | 0 / 1 | **[5] = 1** — every sprite's static data |
| `player_direction` | 0 | -1 | **-1** — static data, outfield players included |
| `save_sprite` (goals) | 0 | 1 | **1** — `SWOS2.s:16857` writes it |

**On all five this port holds the SWOS value and the harness does not.** They
are muted because the reference cannot produce them, not because we cannot; and
they are frame-0 construction, so they say nothing about any frame after it.

That leaves a much shorter honest list:

| class | why | closable? |
|---|---|---|
| Sprite construction (the five above) | swos-port builds sprites in C++ and memsets the game's static table away before frame 0 | No — but they are *its* deviation, not ours, and cannot recur once the game code runs |
| Two sprite slots (corner flag, player-name HUD) | swos-port keeps four corner flags and picks by camera; its rig skips the HUD slot outright | No — no counterpart exists here |
| Byte-set word flags (`ballOutOfPlayOrKeeper`, `goaliePlayingOrOut`, `isMoving`) | `SWOS2.s` writes a byte into a word, so "set" is `0x00FF` little-endian and `0xFF00` big-endian. Exactness is *target-dependent*; a truth test is the correct comparison | No — the truth test **is** the right test |
| ~~Control-layer state (`quick_fire`, `normal_fire`)~~ | **Retracted.** The recordings are the DOS binary, not swos-port's C++. The signature was ours: an AI tackle clears the human's `pl1FireCounter` through a tail the port had split in two | **Closed** |
| ~~Per-image cull metrics~~ | **Retracted.** The banks are a 1:1 conversion and the metrics identical; the residual was two bugs of ours | **Closed** |
| ~~"The corpus is SWOS++"~~ | **Retracted.** A static recompilation runs the original code; see above | **Closed** |

So the claim available is stronger than the old bounded one:

> The recordings are the original DOS game running under a static
> recompilation. Every field that both builds compute — which is every field
> except five that the reference *constructs* before frame 0, two slots that do
> not exist here, and three flags whose exact value is endian-dependent by
> design — agrees exactly, for as far as each recording currently reaches.

100% of *frames* is still the open question, and the honest remaining obstacle
is the view geometry, not the corpus's provenance: DOS renders 320x200 and this
port 336x272, `onScreen` gates player selection, and the rig narrows that by
setting `_sim_view_width`/`_sim_view_height` to the recording's geometry rather
than by pretending it does not matter. An Amiga corpus removes the last of it —
see section 8 — but it is no longer true that the corpus itself is the ceiling.

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

The classes above exist because the recording came through a different
*harness*, not from a different game — swos-port's C++ scaffolding around the
recompiled DOS code, and the 320x200 view that scaffolding renders at. Record
from the Amiga original at 336x272 with the Amiga sprite bank (section 8) and
all of it goes: sprites are constructed from the game's own static table, the
corner flag and HUD slots are the ones we have, the byte-set flags land in the
byte order we use, the cull metrics are the artwork we ship, and `onScreen`
stops depending on a view size we do not render. Nothing is left to exclude,
and "every recorded field, every frame" is a claim that can actually be made —
and would mean what it says.

### What the rig has already found

Thirty-six real defects so far, all fixed — plus the two in "The `onScreen`
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
26. **The goalkeeper's reach entry is picked differently on the two releases.**
    The Amiga indexes the skill table with `(tick >> 2) & 15`; the DOS uses
    `tick & 15` and then steps one entry *down* the table with a floor at 0. One
    unambiguous site in each listing plus a measurement, so it is switched on
    `_gDosGameplay` rather than chosen. See "The keeper's reach entry" below.
27. **The throw-in "hide the result banner" gate is 50 frames on Amiga and 55
    on DOS** (`SWOS2.s:30563` `cmpi.w #50` against `swos.asm:115401`
    `cmp stoppageTimerActive, 55`). It reaches much further than the banner:
    hiding it *negates* `resultTimer`, a negative `resultTimer` is what makes
    `ShowResult` zero it, and a zero `resultTimer` is what lets
    `AI_SetControlsDirection`'s own `tst.w resultTimer` gate fall through and
    start rotating the thrower. Five frames early there turned an AI throw-in
    taker the reference left standing. `zkz-11054632` **1169 -> 7314, the whole
    recording, at every level**. See "One constant, three globals away" below.
28. **A 32-bit move of "the x" carries x's fraction on m68k and *y's* fraction
    on x86.** `DoGoalkeeperSprites` parks the ball on the diving keeper with one
    long move at the x field's integer word — which is the start of the field on
    the Amiga and its second word on the DOS build, so the same instruction
    copies a different pair of fields. Gated on `_gDosGameplay`; the Amiga side
    is the one that reads as intended. See "Two ways a 32-bit move can mean
    different fields" below.
29. **`SetStaticHeaderDirection`'s tail is missing from the DOS build.** The
    Amiga funnels all three outcomes into
    `SetPlayerAnimationTableAndPictureIndex(&staticHeaderAimAnimTable)`; the DOS
    build's two `beq`s become `jz @@out` and the turn falls through to the same
    bare `retn`, so a DOS static header never leaves the *attempt* animation.
    Also gated. See "The tail that the recompilation dropped" below.
30. **`--dos-gameplay` was not switching the formations.** The DOS release
    re-tuned 4-4-2, 4-3-3 and Sweep, `ApplyGameStyle` swapped the physics and
    the timers but not those tables, and the gap had been diagnosed and written
    down for weeks behind a compile-time flag. `TacticsData_SelectRelease` makes
    it runtime. Three recordings roughly quadruple their `sprites` depth. See
    "The formations are part of what `--dos-gameplay` means" below.
31. **The AI's random-rotate position gate does not exist in the DOS build.**
    Four comparisons the Amiga opens `random_rotate_when_no_target` with are
    simply absent from `swos.asm` — 273 and 398 appear nowhere in that listing
    outside the goalkeeper-area check — so on DOS every player reaches the gate.
    Gated. `zkz-10174427` 1259 → 13248.
32. **`a2` is a parameter, not an uninitialised local — it holds the ball.**
    `AI_SetControlsDirection`'s `ST_FOUL` spin test reads `SPRITE_Y(a2)`, and
    the caller passes `ballSprite` (SWOS2.s:30123). This port declared it NULL
    and guarded the read, so the gate never fired. `zkz-10174427` 13248 → 14519.
    See "The register that arrived holding something" below.
33. **`tst.l` on a two-byte field spans the next one.**
    `tst.l TEAMGENERALINFO_LONGPASS(a6)` covers `longPass` *and* `longSpinPass`,
    at four sites; this port tested `longPass` alone at three of them, so a
    spin-pass picked the wrong lead angle and threw the pass landing point
    across the pitch. `zkz-12080203` 2449 → 10551. See "A long test on a word
    field" below.
34. **A shift count taken from a comment instead of the instructions.** The
    tackle's near-goalmouth slowdown is four shifts (/16) under a comment that
    says "/= 32", in *both* listings — one wrong annotation inherited from the
    shared DOS IDA database. We had `>>= 5`. `playa-10095214` 4419 → 10129. See
    "The comment is not the code" below.
35. **The referee's entry position was a literal, not `f(view)`.** He enters
    from below at `cameraY + 287` on the Amiga and `+ 215` on DOS — a
    difference of exactly one view height. Now `SIM_REFEREE_ENTRY_BELOW`.
    `zkz-10140228` 7053 → 11833.
36. **Seven of the twelve `stoppageEventTimer` reloads scale ×1.1 and five do
    not** — including two 100s that stay where three others become 110. We had
    the Amiga values throughout, so DOS mode left every end-of-period pause
    early. `zkz-10140228` 11833 → 14282, `zkz-10174427` 14519 → 21927. See
    "Twelve reloads, seven of which scale" below.
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

### The keeper's reach entry — where the two listings really do disagree

`zkz-10140228` stopped at 1108 on `top_info.controlled_pl_direction`, 2 recorded
against our 6. The rest of that frame says what it actually is:
`sprite[15].player_state` 6 (`PL_GOALIE_DIVING_HIGH`) against our 0,
`player_down_timer` 75 against 0, `speed` 2048 against 256. **The reference's
goalkeeper dived and ours stood still**, and `controlledPlDirection` is only the
dive direction `GoalkeeperJumping` writes on the way past.

Two things narrow it to one line before any instrumentation:

- `speed` 2048 is `kGoalkeeperFarJumpSpeed`, which `GoalkeeperJumping` only
  reaches with `speedVariant == 0`. The `goalie_react_goal_scored` path passes
  2, which would have given `(tick & 0xFF) + 1024`. So the dive came from
  `goalie_save_attempt_entry`, i.e. `ShouldGoalkeeperDive` returned 1.
- `goalkeeper_saved_comment_timer` never moves off 0, so the finisher-vs-keeper
  roll above it did not run either — `ballDistance` 466 takes the
  `> 128` exit straight to the save attempt.

Printing the gate values on the frame:

```
[gk 1108 top] dive: dy=16 ballY=158 gkY=142 defX=349 gkX=330
[gk 1108 top]   saveDist=16 dy=16
[gk 1108 top]   framesY=8 absBallDy=16
[gk 1108 top]   framesX=64 dx=19
[gk 1108 top]   framesDive=7 skillIdx=1 diveDelta=196608 -> NO
```

The last gate is `framesDive >= framesY`, and 7 < 8 by one frame. Every input to
it is a compared field that matched at 1107 — ball position and deltas, keeper
position and deltas — except the reach entry, which comes from the tick:

```
    Amiga  SWOS2.s:26266    andi.w  #$3C,d0        ; (tick >> 2) & 15
                            lsr.w   #1,d0
    DOS    swos.asm:106639  and  word ptr D0, 0Fh  ;  tick & 15
                            shl  word ptr D0, 1
    DOS    swos.asm:106644  dec  ax                ; entry - 1, floored at 0
                            jns  short @@calc_frames
                            mov  ax, 0
```

The Amiga's index advances every fourth tick; the DOS's every tick. The DOS then
steps one entry **down** its reach table — which is already the lower of the two
(`0x28000…0x60000` against `0x30000…0x68000`, and both binaries ship their own,
so `ApplyGameStyle` was already swapping the data). At tick 1108 the Amiga
expression gives entry 5 → skill 1 → 3.0 px/frame → 7 frames; the DOS one gives
entry 4 → skill 1 → minus one → 0 → 2.5 px/frame → **8** frames, and 8 >= 8
dives.

This is the third retraction's rule applied in the other direction. "The DOS is
a recompile, so a difference is a misreading" is the null hypothesis, not a
prohibition — and it is discharged the way the rule says: one unambiguous site
in each listing (an `andi.w`/`lsr.w` pair against an `and`/`shl` pair, plus a
`dec` with no Amiga counterpart) and a measurement that only one of them
reproduces. So it is **switched on `_gDosGameplay`**, like the DOS bench
team-pick counter before it; the Amiga path is byte-for-byte what it was.

`zkz-10140228` went **1108 → 1286 / 1286 / 1281 / 1112**, and `playa-10095214`
— whose `controlled_pl_direction` stop at 1976 was the same keeper, on the same
gate — **1976 → 4186** at `info` and 1976 → 3356 at `sprites`. Nothing regressed.

> A gate that misses by exactly one frame is worth more attention than one that
> misses by a mile: it means every input is right except a single quantised one.

### One constant, three globals away from the thing it broke

`zkz-11054632` stopped at 1169 on `bottom_info.current_allowed_direction`, 2
recorded against our 3, with `sprite[12].direction` 2 against 3 and its
`cycle_frames_timer` 33 against **90**. The timer is the tell: 90 is a fresh
animation, so our thrower had just been re-pointed and re-animated during a
throw-in the reference left alone.

A gdb watchpoint on the sprite's direction named the writer immediately
(`update_players.c`, the `d0 != player->direction` arm of the throw-in
handler), and a second one on `currentAllowedDirection` named who fed it:
`AI_SetControlsDirection`, in `try_apply_turn_step` — the AI rotating the
throw-in taker one step clockwise.

That routine can only get there past `set_direction_core`, which opens with

```
    tst.w   (resultTimer).l
    bne.w   locret_118178
```

so it only rotates anyone while `resultTimer` is **zero**. Watching that global
gives the whole chain in three writes:

```
ShowResult          113 -> 112     (counts down each frame)
UpdatePlayers       112 -> -112    (the throw-in UI "hide the banner" negate)
ShowResult         -112 -> 0       (negative means hide; hide means zero)
```

And the negate is behind one gate:

```
    Amiga  SWOS2.s:30563    cmpi.w  #50,(stoppageTimerActive).l
                            bhi.s   toggle_stats_timers
    DOS    swos.asm:115401  cmp     stoppageTimerActive, 55
                            ja      short @@throw_in_hide_result
```

50 against 55 — the `GS_55_50` family again, and the fourth member of it the
corpus has caught (the bench camera, the player-down timer, the bench repeat
delay). We were hiding the banner five frames early, which zeroed `resultTimer`
five frames early, which opened the AI's turn gate on the one frame in sixteen
it samples (`tick & 0xE == 0`, and 1169 mod 16 is 1). Now `_gsThrowInHideResult`.

`zkz-11054632` went **1169 → 7314**: the whole recording, at every level, 2104
fields a frame. It is the first recording to complete at `sprites`.

There are **two** of these gates, not one — `SWOS2.s:30162` for a controlled
player at a break and `:30563` for a throw-in, against `swos.asm:114574` and
`:115401`. Only the second had a recording stopping on it, so the first went
unfixed for a day until a grep of the constant across both listings turned it
up; it was worth as much as its twin — `zkz-10140228` **2273 → 7411** and
`zkz-10173518` 1663 → 2049. Grep the constant across both listings *before*
calling a site unique: it is a two-second habit and it found half of this one.

> Three globals separate the constant from the symptom, and none of the three is
> a field the rig compares. What made it findable was that the *symptom* was
> compared: `cycle_frames_timer` jumping to 90 says "this animation was just
> restarted", which is a much stronger clue than the direction byte beside it.
> Watchpoints, then, not reasoning — each one names its writer in a second.

### Two ways a 32-bit move can mean different fields

`playa-10144556` stopped at 1042 with only the **ball's** x and y differing, and
both by less than a pixel: recorded (358.85, 145.22) against our (358.30,
145.87). The integers agree; the *fractions* are swapped-looking. Frame 1042 is
`ST_KEEPER_HOLDS_BALL` — the keeper has just claimed it — and a watchpoint on
`_ballSprite.x.raw` names `DoGoalkeeperSprites`, which parks the ball on the
diving keeper's sprite.

Splitting the two values into integer and fraction says the rest:

| | integer | fraction | fraction is… |
|---|---:|---:|---|
| recorded x | 358 | 55850 | the **ball's own**, from frame 1041 |
| recorded y | 145 | 14344 | the **keeper's** |
| our x | 358 | 20182 | the **keeper's** |
| our y | 145 | 57161 | the **ball's own** |

Each build copies one fraction from the keeper and keeps the other. And both are
doing the same instruction:

```
    Amiga  SWOS2.s:  move.l  SPRITE_X(a5),(ballSprite+SPRITE_X).l
    DOS    swos.asm:110889:  mov  eax, [esi+(Sprite.x+2)]
                             mov  ballSprite.x+2, eax
```

One 32-bit move of "the x, at its integer word". On the m68k the integer word is
the **start** of the 16.16 field, so the long carries x's integer and x's
fraction. On x86 it is the **second** word, so the same long carries x's integer
and then runs straight on into **y's fraction**. Same source, same instruction,
two different pairs of fields — the conversion could not have preserved this
without knowing it mattered.

So the Amiga is the one that reads as intended and the DOS is the artefact;
`placeBallOnDivingKeeper` keeps the plain `.raw` copy for Amiga and spells the
straddle out under `_gDosGameplay`. **1042 → 1308.**

### The tail that the recompilation dropped

The next stop, at 1308, was `sprite[22].frame_delay` 10 recorded against our 5 —
a static header whose animation we restarted and the reference did not. A
watchpoint on the sprite's `frameDelay` named
`SetPlayerAnimationTableAndPictureIndex(&_staticHeaderAimAnimTable, …)`, called
from `SetStaticHeaderDirection`, which is the last thing that routine does:

```
    _11288E_apply_dir_wrap_mod8:
            andi.w  #7,SPRITE_DIRECTION(a1)
    apply_static_header_aim_anim:               ; SWOS2.s:25882
            lea     (staticHeaderAimAnimTable).l,a0
            jsr     SetPlayerAnimationTableAndPictureIndex
```

All three outcomes reach it — already facing the input (`beq`), a half-turn away
(`beq`), or one step turned (fall-through). **The DOS build has no such tail.**
Both `beq`s become `jz @@out` and the turn falls through to the same bare `retn`
(swos.asm:112213, 112216, 112231); the caller is identical on both sides
(swos.asm:115894). So a DOS static header stays on the *attempt* animation for
its whole 20 frames, and the recording says exactly that: `frame_delay` 10 — the
attempt list's own `-10` command — held from 1304 to 1312, where the aim table's
5 would have replaced it.

A missing tail is a strange thing for a recompilation to produce, and it is a
bigger difference than any constant this document has recorded. It is taken on
the same terms as the rest: one unambiguous site in each listing, the caller
checked on both sides, and a measurement only one of them reproduces. Gated on
`_gDosGameplay`; the Amiga path is untouched. **1308 → 1696.**

> Both of these are the same lesson from opposite ends. A static recompilation
> preserves *instructions*, not *meaning* — and where the meaning of an
> instruction depends on the machine (which half of a 16.16 field its address
> names) or where an instruction simply went missing, the two binaries diverge
> without anybody deciding anything. Neither is a tuning choice, so neither
> changes what this port does on the Amiga.

### The formations are part of what `--dos-gameplay` means

Three of the corpus's `sprites` stops were the same shape: a player's `dest_x`
or `dest_y` off by one grid step and everything downstream of it following —
`sprite[20].dest_x` 145 recorded against our 179, `sprite[10].dest_y` 394
against 434. Destinations come from the tactics grid, and this document already
knew the answer: the DOS release re-tuned three formations (21 coverage bytes
across 4-4-2, 4-3-3 and Sweep; its 3-4-3 is the pristine Amiga one), and
`ApplyGameStyle` was swapping the physics and the timers for `--dos-gameplay`
but **not** the formations. It was written down as an open gap and left there,
because the machinery to fix it — the baked DOS tables — existed only behind a
compile-time flag.

Testing the hypothesis costs one build:

```
make clean && make SWOS_DOS_TACTICS=1
    playa-10090613   2694 -> 7103      (sprites)
    zkz-11215003     1592 -> 11267
    zkz-10173518      939 -> 5051
```

So the tables come out from behind the flag. `TacticsData_SelectRelease(dos)`
points slots 0/5/7/8 of `_teamTacticsTable` at one release's set or the other,
and `ApplyGameStyle` calls it beside the ball friction and the `GS_*` timers.
Two details make it safe to call from there:

- It **re-applies `InitUserTactics`' 5-3-2 forward-row copy** to whichever set
  is now in the table. That copy is what makes slots 8 and 9 of the 4-4-2 and
  the Sweep dead data on both releases (see "The rows that were never
  executed"), and `InitUserTactics` runs in the front end, long before
  `ApplyGameStyle`. Redoing it here means call order does not matter.
- It does **not** touch the six USER slots. Those are the player's own tactics,
  loaded from a `.TAC` or — in the rig — from the recording's own blob.

The four DOS tables are now always compiled: ~1.5 KB, and the Amiga link shows
exactly that (`.data` 0x1dca4 → 0x1e26e, `.text` +260 bytes for the function,
`.bss` unchanged). `-DSWOS_DOS_TACTICS` still exists and still pins a build to
the DOS set; the Amiga default is byte-for-byte what it was, because
`TacticsData_SelectRelease(0)` writes back the pointers the static initialiser
already held and redoes a copy that was already done.

The full corpus said more than the three probes did. `zkz-11021807` completes at
**every** level now rather than only at `scalars`; `zkz-10174427`, which had
stopped at frame 0 since the rig was built, runs to **1345** — its frame-0
divergence was the Eranio coverage byte, which is to say this exact gap seen at
setup time instead of in play; and `playa-10095214` goes 5276 → 10130 at
`scalars`. Corpus-wide, `sprites` coverage went **13.2% → 22.1%**.

One loose end: `checkPlayerValues` still reports 31/32 for the two recordings
whose values depend on those bytes. It runs between `installTeam` and
`InitTeams`, which is *before* `ApplyGameStyle` — so the audit still grades the
Amiga tables while the match that follows plays the DOS ones. The 384/384 that
`-DSWOS_DOS_TACTICS` scores is the right number for DOS mode; making the
setup-time check agree means moving the selection earlier, which is a question
about where `--dos-gameplay` should take effect rather than about the data.

> The gap had been diagnosed, measured and written down for weeks; what it was
> missing was somebody running `make SWOS_DOS_TACTICS=1` against the corpus and
> reading the three numbers. A hypothesis that costs one build is not worth
> reasoning about.

### The register that arrived holding something

`zkz-10174427` — the coach-mode recording, both teams computer-driven — gave up
two defects in a row once the formation gap let it off frame 0, and both are
about `AI_SetControlsDirection` reading state this port had not given it.

**The position gate the DOS build does not have.** At frame 1259 the recording
forces direction 6 and we force nothing (`current_allowed_direction` -1). The
controlled player is at (361, 757), and the Amiga's `random_rotate_when_no_target`
opens with four comparisons:

```
    random_rotate_when_no_target:               ; SWOS2.s:32460
            move.w  #$FFFF,d0
            cmpi.w  #273,SPRITE_X(a5)
            blt.s   random_rotate_or_base_dir_gate
            cmpi.w  #398,SPRITE_X(a5)
            bgt.s   random_rotate_or_base_dir_gate
            cmpi.w  #158,SPRITE_Y(a5)
            ble.s   loc_118040
            cmpi.w  #740,SPRITE_Y(a5)
            bge.s   loc_118040
```

x is inside the corridor and y is past 740, so the Amiga stores the -1 it
started with. **The DOS build has no such test.** Its
`@@randomly_flip_or_continue_direction` (swos.asm:118956) opens straight on
`mov ax, currentGameTick / and 18h`, and 273 and 398 appear nowhere else in
that listing except the goalkeeper-area check. Every player reaches the gate
there, and `tick & 0x18` at 1259 is non-zero, which stores the player's own
direction — 6. Gated on `_gDosGameplay`; **1259 → 13248**.

That is the third block the recompilation simply does not have, after
`SetStaticHeaderDirection`'s tail. Worth noticing as a pattern rather than
three coincidences: when a divergence survives a careful reading of the Amiga
listing, grep the *constants* in the DOS one — an absent block leaves no
instruction to misread, only numbers that are not there.

**`a2` is the ball.** The next stop, 13248, was `ai_ball_spin_direction` 0
recorded against our 1, on the frame a free kick is taken. The spin comes from:

```
    clr.w   d0
    cmpi.w  #ST_FOUL,(gameState).l
    bne.s   compute_turn_from_angle_sign
    cmpa.l  #topTeamData,a6
    bne.s   foul_y_gate_bottom_team
    cmpi.w  #682,SPRITE_Y(a2)
    bge.w   apply_turn_and_return           ; -> spin stays 0
```

`a2` is never assigned anywhere on the path that reaches this, so it is an
*input*: `UpdatePlayers` calls the routine with a2 = `ballSprite`, and wraps
the call in `movem.l a1-a3,-(sp)` precisely because the routine clobbers those
(SWOS2.s:30123). This port had declared it `Sprite *a2 = NULL` with a
`a2 != NULL` guard on the test — so the gate never fired and the spin always
came from the angle sign instead. The ball was at y=693 against a `>= 682`
bound. **13248 → 14519.**

> An uninitialised-looking register in a disassembly is a parameter you have not
> found yet. The tell is the caller: a `movem.l a1-a3,-(sp)` around the `bsr`
> says those registers mean something on both sides of it. Guarding the read
> with `!= NULL` turns a missing parameter into a branch that never runs, which
> is quieter and worse than a crash — the CLAUDE.md rule about NULL guards,
> arriving from a direction it does not name.

### RETRACTED: "the control layer is swos-port's, not the game's"

This section used to file **four** stops — `zkz-12065730` at 1251,
`playa-10144556` at 1696, `zkz-10190651` at 2570 and `zkz-10173518` at 5051 —
as a fourth class of non-comparable state, on the grounds that swos-port
replaces the input path with C++ (`src/controls/gameControls.cpp`: private
`m_pl1FireCounter`, `m_teamSwitchCounter`, an `updateFireBlocked()` that
clears no counters) and that the recordings were therefore describing that
C++ rather than the game.

**The premise is wrong. The recordings are made by SWOS++ driving the
original DOS binary in a VM — SWOS++ instruments the game, it does not
reimplement it.** swos-port is a *listing* in this project, a second witness
to what the original does; it is not the thing that produced the corpus. Every
value in a `.RGD` is the real DOS game's.

So there is no fourth class, and "when a divergence is input-derived and the
disassembly checks out, read swos-port's C++" was exactly the wrong rule: it
sent the next look away from the code that was actually wrong. The correct
rule is the boring one this document already had — *the disassembly checking
out means you have not read enough of it yet.*

That is not hindsight. The audit this section rested on said "all four
`headerOrTackle` sites are faithful", and they were; what it never asked was
who else jumps into the tail those sites feed. The answer was the AI team, and
it is the finding above — an AI tackle clears the human's `pl1FireCounter`,
which is precisely how both fire flags come to read 0 on a hold our classifier
arms. Three of the four stops carry that exact signature, to the frame.

Two things worth keeping out of the wreckage:

- **`quick_fire` is genuinely observable** — non-zero on 36 to 108 frames of
  every recording — so these stops were always real behavioural differences
  rather than a recorder that never captures the flag. That part stands, and
  it is what made them worth chasing.
- **The team-alternation difference is real, and it is in the original
  binaries, not in swos-port.** The Amiga picks the team to update with
  `btst #0,frameCount+1` (SWOS2.s:25593) — a global that advances every frame,
  including the frames the fire-block skips. The DOS build keeps a dedicated
  counter and advances it **inside** the `fireBlocked == 0` branch:

  ```
      @@no_stats_exit:                            ; swos.asm:100580
      add     teamSwitchCounter, 1
      mov     A6, offset topTeamData
      test    byte ptr teamSwitchCounter, 1
      jnz     short @@update_controlled_player
      mov     A6, offset bottomTeamData
  ```

  So a fire-block period shifts the DOS phase and leaves the Amiga's alone,
  and the two are on opposite teams afterwards. **Now measured and fixed**
  (defect 42): player 2 holds fire through a stoppage in `zkz-12065730` to
  f1249 and releases at f1250, and the reference resumes writing the bottom
  team's `direction` at f1252 where `frameCount` says f1251 — which the
  bench's double-tap detector then latches a full alternation late, recorded
  `subsPrevPl2Direction` -1 at f1251 and 7 at f1253 against our 7 at f1251.
  That recording now replays **complete**, and with it the last of the four
  stops this section claimed could never move.

### A long test on a word field

`zkz-12080203` stopped at 2449 on `bottom_info.ball_x` — 803 recorded against
our -64, with `sprite[7].dest_x` carrying the same two values. Neither is a ball
coordinate: the pitch is 672 wide. `TeamGeneralInfo.ballX`/`ballY` are reused as
the **pass landing point**, the receiver walks to it, and both builds had agreed
on it for 2448 frames.

The landing point comes from a lead offset:

```
pass_assist_compute_lead_offset:                ; SWOS2.s:31038
                move.b  #192,d1                 ; -64
                tst.l   TEAMGENERALINFO_LONGPASS(a6)
                beq.s   pass_assist_apply_lead_sign
                move.b  #224,d1                 ; -32
pass_assist_apply_lead_sign:
                tst.b   d0
                blt.s   pass_assist_build_friction_index
                neg.b   d1
```

`TEAMGENERALINFO_LONGPASS` is at 124 and its size is 2 — but the test is
**`tst.l`**. It spans `longSpinPass` at 126 as well, and the pair is non-zero if
*either* word is. This port tested `team->longPass` alone, so a spin-pass took
the -64 arm where the reference takes -32, which indexes a different entry of
the direction-offset table entirely and throws the landing point across the
pitch.

The asm does that at **four** sites — `SWOS2.s:23703`, `:27564`, `:31006` and
`:31040` — and this port had it right at exactly one of them
(`ball_physics.c:1599`, `longPass != 0 || longSpinPass != 0`, which is the
idiom the other three now use). One correct instance of a pattern in the tree is
not evidence the rest are; grep the *equate*, not the field name.

`zkz-12080203` went **2449 → 11495 / 11495 / 11494 / 10551**, and
`playa-10090613` — which had been stopping on a sprite position — **7103 →
10426 at every level**, its `sprites` and `scalars` depths now identical.
Corpus-wide, `sprites` coverage went **27.4% → 32.0%**.

> This is the `PACK_BE_U16` rule from CLAUDE.md arriving as a *read* rather than
> a write: `tst.l` on a two-byte field is the same "two adjacent fields accessed
> as one wider unit" that the endian helpers exist for. Testing both fields by
> name is the portable form — it says what it means, it greps, and it is
> endian-independent, where `*(int32_t *)&team->longPass` would be none of those.

### The comment is not the code — and the comments came from the DOS database

`playa-10095214` stopped at 4419 on `sprite[13].speed`: 47 recorded against our
23. The player is mid-tackle (`PL_TACKLING`), his speed decaying 1024 → 928 →
832 by `kPlayerGroundConstant`, and then in one step to 47 where ours goes to
23. A watchpoint put the write at the "out of pitch by Y, near the goalmouth"
branch, which is a shift and an `or`:

```
                lsr     SPRITE_SPEED(a1) ; player tackling out of pitch horizontally, but near goal
                lsr     SPRITE_SPEED(a1)
                lsr     SPRITE_SPEED(a1)
                lsr     SPRITE_SPEED(a1)
                ori.w   #1,SPRITE_SPEED(a1) ; sprite speed /= 32
```

**Four** `lsr`s — /16 — under a comment that says /32. This port had `>>= 5`:
it implemented the comment. 736 >> 4 is 46, `| 1` is **47**, exactly what the
recording holds; 736 >> 5 is 23, exactly what we had. **4419 → 10129.**

The comment is wrong on the DOS side too, and identically:

```
    shr     [esi+Sprite.speed], 1        ; swos.asm:115736-115743, four of them
    ...
    or      [esi+Sprite.speed], 1        ; sprite speed /= 32
```

So this is not an Amiga-vs-DOS difference at all — it is **one annotation from
the shared IDA database**, wrong in both listings because both inherit it from
Zlatko Karakaš's DOS database (see CLAUDE.md, "Sources"). That is worth holding
onto as a rule in its own right:

> The comments in `data/SWOS2.s` are DOS-derived. They are a good guide to
> *intent* and no evidence at all about the instructions beside them — they can
> be wrong for both releases, as here, or right for DOS and wrong for the Amiga.
> When a comment and an instruction sequence disagree, the instructions win; and
> `swos-port/swos/swos.asm` is a second witness that costs one grep.

This is the same discipline as "a disassembly shows what is stored, not what is
executed", one level further out: a disassembly also shows what somebody
*thought* it did.

### Twelve reloads, seven of which scale

`zkz-10140228` stopped at 7053 on `sprite[30]` — the referee — with his Y 72
pixels out and his `delta_x` exactly half ours. 72 is the view-height
difference, 272 − 200, which points at a constant that should be `f(view)` and
is not. A watchpoint put the write in `ActivateReferee`:

```
    Amiga  SWOS2.s:15172    addi.w  #287,d2
    DOS    swos.asm:102740  add     word ptr D2, 215
```

287 − 215 = 72. The referee entering from *below* is parked one view height plus
15 under the camera, and the literal was the Amiga's 272-high view — the same
shape as the bench-slide window, and now `SIM_REFEREE_ENTRY_BELOW`. Its sibling
`- 20`, for entering from *above*, is 20 in both listings: that one is measured
from the top edge and genuinely is not view-dependent. **7053 → 11833.**

The new stop was `game_state` 23 recorded against our 25 — we leave
`ST_GOING_TO_HALFTIME` early — and `zkz-10174427` had been sitting on the same
transition at 14519. `stoppageEventTimer` gates it, and grepping the literal in
both listings gives the whole family at once:

| routine | `SWOS2.s` | `swos.asm` | scaled |
|---|---:|---:|:--:|
| `UpdateCameraBreakMode` | 50 | 55 | ✓ |
| `UpdateCameraBreakMode` | 75 | 75 | |
| `StartingMatch` | 100 | 100 | |
| `SetCameraMovingToShowerState` | 100 | 110 | ✓ |
| `EndFirstHalf` | 100 | 100 | |
| `FirstHalfJustEnded` | 250 | 275 | ✓ |
| `EndOfGame` | 150 | 150 | |
| `PlayersLeavingPitch` | 250 | 275 | ✓ |
| `GoToHalftime` | 700 | 770 | ✓ |
| `GameOver` | 1500 | 1650 | ✓ |
| `StartFirstExtraTime` | 100 | 110 | ✓ |
| `EndFirstExtraTime` | 100 | 110 | ✓ |

**Seven scale and five do not**, and two of the unscaled ones are 100 — the same
literal as three that do. There is no rule to apply here, only a table; scaling
"the 100s" would have been wrong three times out of five. This is the discipline
the player-down timer established, and the first time the corpus has offered a
whole family at once to apply it to.

Four constants cover the seven: `_gsStoppageShowerPan` (100/110),
`_gsStoppageHalfEnded` (250/275), `_gsStoppageHalftime` (700/770) and
`_gsStoppageGameOver` (1500/1650).

`zkz-10140228` went **7053 → 14282**, `zkz-10174427` **14519 → 21927** — 82.4%
of itself — and `playa-10090613`, which stops elsewhere, 10426 → 11222. Corpus
coverage went **34.6% → 40.8%** at `sprites`.

> Grepping the *literal* across both listings — `move.w #…,(stoppageEventTimer)`
> against `mov stoppageEventTimer,` — took one command and produced a table that
> would have taken a dozen watchpoints to assemble one stop at a time. When a
> constant turns out to be release-specific, ask immediately how many siblings
> it has.

### The tail the AI team shares with player one

`playa-10095214` stopped at 10129 with the human's controlled player taking a
shot the reference never took: `normal_fire` -1 against a recorded 0, and with
it `shooting`, `pass_kick_timer` 25, a cleared `controlled_player` and a
`passing_kicking_player` — the whole signature of `execute_shot_and_update_context`.

`normalFire` is armed by a counter, and the counter is easy to follow.
`Player1StatusProc` writes `pl1FireCounter = -1` on the frame the button goes
down and decrements it on every later poll while it stays down; the classifier
in `UpdateAndApplyTeamControls` arms `normalFire` once the value drops below
-4 and zeroes the counter, and because the decrement is guarded on the counter
being non-zero, a zeroed counter stays zero for the rest of the hold. One team
is polled per frame, so "poll" means every other frame, and the team's own
`fireCounter` — which the recording carries — counts them.

The recording says the mechanism is right and our arithmetic is right:

```
   f9931  fireCounter 1                     press
   f9939  fireCounter 5   normal_fire -1    armed, exactly where the classifier says
   f10005 fireCounter 1                     press
   f10013 fireCounter 5   normal_fire  0    ... and never armed, all the way to 15
   f10121 fireCounter 1                     press
   f10129 fireCounter 5   normal_fire  0    ... nor here
```

Three holds by the same player; the first arms and the other two do not. So
something cleared `pl1FireCounter` during the second and third. Only three
things clear it, and two were ruled out on the recording's own evidence: the
`fireBlocked` release needs both buttons up, and the team's own header/tackle
sites all clear `ballCanBeControlled` as well, which stayed 1 throughout.

That leaves the fourth clear — the one in the tail:

```
    post_input_common_p1:                       ; SWOS2.s:25634
            ...
    apply_input_and_update_players_p1:
            bsr.w   UpdatePlayers
            tst.w   TEAMGENERALINFO_HEADERORTACKLE(a6)
            beq.w   _1126D6_exit_rts
            clr.w   TEAMGENERALINFO_HEADERORTACKLE(a6)
            clr.w   (pl1FireCounter).l
            rts
```

and the thing to notice is who reaches it. A team with `playerNumber == 0` —
the computer — reads no joystick, and the port had given it a small tail of its
own: run `UpdatePlayers`, clear `headerOrTackle`, return, with a comment saying
there was no fire counter to clear for an AI team. The asm gives it no tail of
its own at all. `move.w 4(a6),d0 / beq.w post_input_common_p1` drops it into
**player one's**, and the `clr.w (pl1FireCounter)` is shared. The DOS build
does the same (swos.asm:100703).

So an AI tackle resets the human's fire hold. Read as an accident of a shared
tail it looks like a bug in the original; read as behaviour it is exactly what
should happen — a button held down through an opponent's challenge does not
come out the other side still charging a shot. Either way it is what the game
does, and the port has to do it.

**10129 → 21613.** One line.

> The general shape: *a branch that looked like it needed its own arm.* The
> port asked "what does the AI path do here?" and answered from what the AI
> path plausibly needs, when the listing had already answered by jumping into
> somebody else's code. Two of this document's earlier findings are the same
> mistake with the polarity flipped — rows that were never executed, a tail
> that the recompilation dropped. A shared tail is the third face of it: not
> code that was never reached, but code reached from somewhere the port did
> not look.

### The default arm of a state chain is not an exit

21613 was `game_state` 24 against our 30 — the reference leaving `ST_GAME_ENDED`
for `ST_PLAYERS_GOING_TO_SHOWER` while we stayed put. `EndOfGame` arms 150
ticks and the recording's own timings say that is right: state 30 at f21464 to
24 at f21613 is 149 frames for a timer of 150, matching halftime's 29 at f11021
to 23 at f11120, 99 frames for 100.

A trace on the timer showed it sitting at 150, not counting, for 25 frames.
`UpdateGameTimersAndCameraBreakMode` asks whether a fire button is down during
states 21..30 and runs the `end_game_transmission` chain if it is; the port
called that chain and returned. The chain is four `cmpi/bne` arms, and the last
one is

```
    game_not_starting:                              ; SWOS2.s:25267
            cmpi.w  #ST_CAMERA_GOING_TO_SHOWERS,(gameState).l
            bne.s   game_interrupted
```

— `game_interrupted` being the stoppage countdown itself. Only states 25, 26,
21 and 22 leave the routine there. Every other state in the window **falls
through into the countdown**, and state 30 is one of them. The final whistle
usually catches a player mid-press, so the timer stalled for as long as the
button was held: down from f21460 to f21487, `PlayersLeavingPitch` 25 frames
late.

`handleEndGameTransmission` now returns whether it handled the state, and the
caller returns only when it did. **21613 → 21888.**

> This is the C-versus-asm hazard that has no analogue in the asm at all. A
> chain of `cmpi/bne` has a default arm whether or not anybody wrote one, and
> it is *whatever the last `bne` targets*. Translated into a chain of `if`s
> returning `void`, the default arm silently becomes "return" — and a `return`
> that should have been a fall-through is invisible in review, because the C
> reads exactly like what it was meant to be. Worth checking every helper this
> port factored out of a state chain: the question is not "are the arms right"
> but "where does the last `bne` go".

### Three more camera literals, and one dropped branch

21888 was `camera_x` 176 recorded against our 168, at the jump into
`ST_RESULT_AFTER_THE_GAME`. 168 is `GameOver`'s literal, and 176 is what
`SIM_CAMERA_CENTRE_X` — `(0x2A0 - view_width) / 2`, already used by `GameLoop` —
evaluates to at 320 wide. Grepping the literal across both listings found the
whole set:

| site | `SWOS2.s` | `swos.asm` | is |
|---|---|---|---|
| `GameOver` x | 168 | 176 | `SIM_CAMERA_CENTRE_X` |
| `ViewHighLightsMain` x | 168 | 176 | `SIM_CAMERA_CENTRE_X` |
| `ViewHighLightsMain` y | 313 | 349 | `449 - view_height/2` |
| `GameLoop` x, match arm | 168 | 176 | already correct |
| `GameLoop` x/y, TRAINING arm | 168 / 313 | 168 / 313 | **neither** |

313 and 349 are the camera Y that centres the bench (449), so it gets a name —
`SIM_CAMERA_BENCH_Y` — and `MoveCamera`'s "has the camera arrived at the bench"
window becomes ±10 around it instead of repeating the expression.

The training arm is the interesting row. It is 168/313 in the DOS build too, so
in a training game the camera really does start 8 pixels off centre and 36 lines
high there. Same literal, same routine, one arm f(view) and the other not —
which is the reason this table was built by grepping the value across both
listings rather than by reasoning about which constants "look like" geometry.

Then `camera_y`, 80.75 recorded against our 16.75. The Amiga picks 80 for a
training game and 16 for a match:

```
            move.w  #80,d2                          ; SWOS2.s:28050
            tst.w   (trainingGame).l
            bne.s   set_for_training_or_regular_game
            move.w  #16,d2
```

The DOS build has `mov word ptr D2, 80` and goes straight on (swos.asm:103860).
This is the same class as the dropped animation tail above — a recompiler has
no reason to delete a `tst/bne` pair — and `playa-10095214` is a plain league
match landing on 80, which is where the Amiga's test would have said 16. Gated
on `_gDosGameplay`. **21888 → 22400 of 22401.**

### One tick, eight thousand frames later

`zkz-12080203` stopped at 11492 on `sprite[1].dest_x`, -796 recorded against
our -812. sprite[1] is the ball, and it had just glanced a goalpost:

```
            move.w  (currentGameTick).l,d4          ; SWOS2.s:14830
            andi.w  #$1F,d4
            lsl.w   #4,d4
            subi.w  #$100,d4
            ...
            add.w   d4,SPRITE_DESTX(a0)
```

so the deflection is `((tick & 0x1F) << 4) - 0x100`, a value on a 16-unit grid.
-812 and -796 are adjacent points on that grid: **the two runs read a
`currentGameTick` one apart.** The expression is identical in the DOS listing
(swos.asm:110378), so the difference was the tick itself.

It was not a phase offset. A phase offset would be wrong from frame 0, and the
rig starts the tick at 0 deliberately. Printing the tick against the frame
number found it in phase at f3038 and one behind from f3042 — it had *lost* a
count in between, and the rig's own gate says where:

```
    if (_showingUserStats == 0 && _paused == 0)
        _currentGameTick++;
```

f3039 is the one frame of this recording where the stats overlay is up
(`stats_timer` 32000, and 0 again by f3040). The gate mirrors the Amiga ISR
faithfully. The DOS `TimerProc` does not have it:

```
    mov     ax, paused                          ; swos.asm:12171
    or      ax, ax
    jnz     short @@bump_intro_timers
    inc     currentGameTick
```

Only `paused`. The `paused` half survived the conversion, which is what makes
the missing `showingUserStats` half a hand edit rather than something the
recompiler did. So a DOS stats overlay costs the game clock nothing, and an
Amiga one costs it a tick per frame it is up.

The consequence is out of all proportion to the cause. `currentGameTick` is not
really a clock — it is the game's cheap source of pseudo-randomness, and dozens
of AI and physics decisions are `tick & mask` against it. One lost count
re-phases every one of them for the rest of the match. Here it stayed invisible
for 8000 frames and then surfaced as a ball bouncing 16 units wrong off a post.

Two wrong answers were ruled out on the way, and both are worth keeping:

- **The phase is not globally off by one.** Forcing the tick to start at 1
  costs `zkz-11021807` 4064 → 2863 and `zkz-11054632` 7314 → 671. An earlier
  attempt at that same experiment had reported the same verdict for the wrong
  reason: `_lastGameTick` is set to `0xFFFF` so that frame 0's timer delta
  comes out as 1, and moving the tick without moving it too makes frame 0's
  delta 2, which breaks the kick-off on its own. The experiment only means
  something with both moved together — and it still says 0.
- **The DOS goalkeeper reach entry's `dec ax` is not a phase correction.**
  That was the obvious suspicion once a tick discrepancy turned up, since a
  `-1` on a tick-indexed lookup would fit one exactly. It decrements the table
  VALUE, not the index — `mov ax, [esi+ebx+10] / dec ax / jns` (swos.asm:106642)
  — so defect 26 stands as measured.

> The lesson is about *invisible* state. Every field the rig compares agreed
> for 8000 frames while the two runs were already out of step, because the
> thing that was wrong is not in the recording and is not in any struct — it is
> a counter that only ever appears masked. When a divergence is a small
> quantised step in a value that comes off a `tick & mask` grid, suspect the
> tick and not the expression, and go looking for where the count was lost
> rather than for where the phase was set.

### The bench call the rig cannot place

Two recordings stop where a player calls the substitutes bench, and the rig
cannot tell which frame to make the call on.

The recorded signal is `TeamGeneralInfo.secondaryFire` (offset 144, which the
DOS `UpdateAndApplyTeamControls` copies out of the joystick port,
swos.asm:100612) with `p1_controls` bit 6 rising on the same frame. The game's
own trigger is `bench1Called`, which `MainKeysCheck` sets from a key event. The
rig turns the recorded rising edge into `bench1Called` at frame start — and
that is right in one of the corpus's three presses and one frame early in the
other two:

| recording | level rises | reference acts |
|---|---:|---:|
| `zkz-11215003` | 117 | 117 |
| `zkz-11215003` | 11479 | 11480 |
| `playa-10090613` | 11222 | 11223 |

Both readings are confirmed by 11000 frames of agreement on the other side of
them: injecting on the edge runs `zkz-11215003` from 117 to 11479, and
injecting one frame later runs `playa-10090613` from 11223 to 22496.

There is nothing to choose between them from the recording. The whole gate was
traced on both sides — `waitForPlayerToGoInTimer`, `cameraLeavingSubsTimer`,
`substituteInProgress`, `refState`, `injuriesForever`, `statsTimer`,
`gameStatePl`, `whichCard`, `playingPenalties`, `gameState` — and every input
is identical at both frames, so the game is not deferring the call; it simply
had not seen the key yet. The loop order is the same in both listings
(`GetKey`/`MainKeysCheck` at SWOS2.s:20671-20673 ahead of `BenchCheckControls`
at 20723; swos.asm:7629-7630 ahead of 7686), so a press lands in the same frame
it is polled — *if* it lands before the poll. Whether it did is sub-frame
timing, and the recording samples once a frame.

Same class as the terminal-frame `fire_this_frame` above: an input edge inside
a frame, invisible to a rig that injects a frame's input atomically before the
frame runs.

**f117 is definitely a same-frame call, so the ambiguity is real.** Under a
pure lag-1 build that frame diverges on far more than the camera: the
reference has `game_state_pl` 101 and every player's destination on the
bench-walk line (`sprite[4].dest_x` 772 against our 336, and the same for
5, 6, 7, 8 — a diagonal toward the bench corner). The bench opens there, on
the frame the button rises.

**And it is the whole remaining gap — measured, not estimated.** A throwaway
build that defers the call by one frame whenever `gameStatePl != 0` — the one
rule that fits all three presses — runs

    zkz-11215003   11479 -> 27487 of 27819
    playa-10090613 11222 -> 22496 of 22497   (the terminal frame)

so nothing else in either recording disagrees with this port. That is the
corpus at **98.0%** instead of 87.1%.

**The mechanism is now fully known, and it is why this cannot be recovered.**
One physical key reaches the game down TWO independent paths:

- the **polled bitmask**. `Player1StatusProc` reads one input word and splits
  it — `test ax, 32` is fire, `test ax, 64` is `pl1SecondaryFire`
  (swos.asm:13033-13037). That is the same bit 6 the recording carries as
  `p1_controls & RC_BENCH`, and `UpdateAndApplyTeamControls` copies it into
  `TeamGeneralInfo.secondaryFire` (swos.asm:100610), which is the other half
  of what the recording carries.
- the **buffered scancode**. `bench1Called` is set nowhere else in either
  listing than `MainKeysCheck`, off the key itself (swos.asm:7984,
  SWOS2.s:21900) — and `MainKeysCheck` runs at GameLoop line 666 where
  `UpdateAndApplyTeamControls` runs at 703.

So the recording records the *bitmask* latch and the game opens the bench off
the *scancode* latch, and a scancode can arrive a frame after the bitmask sees
the key held. Nothing in the record times the queue. `p1_controls` is sampled
at the frame's end and rises with the level, and a press after 703 would
separate the two recorded signals by a frame — the schema note records that no
press in the corpus does, which pins the press inside the frame but not inside
the frame's 666.

**The rule IS shipped, as an estimate and labelled as one.** The argument
against it was that it is a guess, fitted to three presses. What settles it
the other way is that the same-frame placement it replaced is a guess too —
the assumption that the press landed before line 666 — and a worse one, right
on one press of three where this is right on three. There is no neutral option
here: the rig must place this input on some frame.

So it is placed, every deferral prints ESTIMATED in the log, and
`src/replay_test.c` carries the whole of this argument at the call site.
**A bench-frame match is evidence of nothing** — treat any result that turns
on a substitution as unverified. Every gate input in `BenchCheckControls` was
traced identical at both frames, so if a mechanism exists it is not in there.
What would make this a recovered fact is a recording that carries
`bench1Called` itself — which the Amiga dumper (section 8) would, since it is
our own build being dumped. It was worth 11% of this corpus, which is the
strongest argument for building it.

### The goalpost has three arms and the port had two

`zkz-12080203` stopped at 11493 with the ball a pixel further down the pitch
than the reference and its `destY` unreversed. The ball had wedged against the
side of a post at 11492 — the recording holds its x and y perfectly still while
`destY` alternates 478, -390, 654, -534 and the speed drops by a quarter every
frame.

`UpdateBall`'s goalpost block is entered two ways and leaves three:

```
    loc_1097E6:                 ; z > 15 — level with the crossbar
    loc_109810:                 ; z <= 15 and outside the goal mouth — a post
            move.w  (currentGameTick).l,d4      ; both compute the same step
            andi.w  #$1F,d4
            lsl.w   #4,d4
            subi.w  #$100,d4
            cmpi.l  #$FFFFB000,SPRITE_DELTAY(a0)
            blt     loc_10984C                  ; strong Y
            cmpi.l  #$5000,SPRITE_DELTAY(a0)
            bgt     loc_10984C
```

and where that step is *added* is the whole difference (SWOS2.s:14829-14869,
swos.asm:110374-110438 identical):

| entry | Y velocity | arm | what it does |
|---|---|---|---|
| crossbar height | weak | `loc_109878` | negate deltaZ, restore Z, push the ball one pixel down the pitch |
| post height, outside the mouth | weak | `loc_109810` falls through | reverse the Y destination, **`destY += d4`** |
| either | strong | `loc_10984C` | reverse the Y destination, **`destX += d4`** |

The port had the first and the third, and routed the second into the first. So
a ball resting against the side of a post with little Y velocity bounced off a
crossbar that is not there — a pixel down the pitch every frame instead of
rattling along the goal line.

The arithmetic confirms itself once the middle arm exists. f11493 is the weak
arm: `reverseDestYDirection` on a ball at y 132 with `destY` 478 gives -214,
and the tick step at that frame is -176, so `destY` lands on the recorded
**-390**. f11494 is the strong arm: `destY` reverses to the recorded **654**
and is left there while `destX` takes the step instead, -796 + -160 = the
recorded **-956**.

**11493 → 31634 of 31635 frames.** One missing arm was 20141 frames of a
recording, and it also carried `zkz-10140228` from 29984 to 32545.

> Worth noticing how it hid. Both entries compute `d4` identically and both
> test the same threshold, so the two blocks read as duplicates — and a
> duplicate is exactly the kind of thing a port collapses into one. They are
> not duplicates: they differ only in what happens when the test FAILS, which
> is the arm with no code of its own in the listing, just a fall-through.

### The tick is a real-time counter, and the recording does not carry it

Two of the last stops are the same thing, and it is not a defect.

`zkz-10173518` stops at 25490 on `sprite[28]` — the player-mark, the little
blob over the marked player. `MarkPlayer` blinks it between the two teams on
one bit of the game tick:

```
            lea     (topTeamData).l,a1              ; SWOS2.s:16699
            btst    #4,(currentGameTick+1).l
            beq.s   loc_10B130      ; don't blink for both teams at the same time
            lea     (bottomTeamData).l,a1
```

and the DOS build is identical (swos.asm:99390). Only one team here has a
marked player, so the recording's `sprite[28].image_index` is a **direct
readout of `currentGameTick & 0x10`, once a frame, for the whole match**.
Reading it back is unambiguous:

```
    frames 25440..25503, shown = X
    XXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX..XXXXXXXXXXXXXX
                                                    ^^
```

Sixteen on, sixteen off, sixteen on — and then a gap of **two** where there
should be sixteen. The reference's tick gained 14 counts in a single frame.
Frame 25489 is where `game_state` goes 24 → 26, which is `GameOver`, and the
first thing `GameOver` does is `FadeOutToBlack`. That is a wall-clock wait,
the DOS `TimerProc` keeps counting through it, and the game loop does not.

`zkz-10174427` at 21927 is the same shape on `sprite[29]`, the booked-player
card: a 16-frame blink window that starts 8 frames off where the frame count
puts it.

**The rig advances `currentGameTick` exactly once per frame, by construction**
— that is one of the four determinism pins (section 4), and without it a
replay does not reproduce at all. So a tick that drifts against the frame
count in the reference cannot be reproduced, and the recording carries no way
to recover it: the `.RGD` record is a fixed set of structs (the schema maps
every byte of it) and neither `currentGameTick` nor `lastFrameTicks` is among
them.

Two things worth being precise about:

- **This is not the stats-overlay finding above.** That one was a real code
  difference — the Amiga ISR gates the tick on `showingUserStats` and the DOS
  one does not — and it cost exactly one count, at the frame the overlay went
  up. This costs fourteen, at a fade, and no code change can produce it.
- **It is why the drift stayed invisible so long.** The game timers step by
  `lastFrameTicks`, and at both of these stops the clock is stopped — after
  the final whistle in one, at a free kick in the other. What the drift
  reaches is the `tick & mask` decisions, and the two it reached here are the
  two blinking HUD sprites.

### Two defects in one penalty shoot-out

`zkz-10140228` is the longest recording in the corpus and the only one that
goes to penalties. Both of its remaining stops were in the shoot-out.

**The keeper's free frames.** At 32545 both goalkeepers took the stopped-play
speed table — 1136 — where the reference held the goalkeeper's own 1024 for
five more frames. The route in is `UpdatePlayers`' goalkeeper block:

```
    player_goalkeeper:
            if (_playingPenalties) {
                if (_penaltiesTimer > 50)      <-- straight to the speed tail
                    goto finalize;
            }
```

`cmpi.w #50,(penaltiesTimer)` (SWOS2.s:29226) against `cmp penaltiesTimer, 55`
(swos.asm:112784) — and its sibling twelve instructions earlier in
`UpdateCameraBreakMode` is already paired as 100/110, so the two scale
together. The recording gives the flip to the frame, and in the order the team
alternation predicts: goalkeeper 1 at f32550, goalkeeper 2 at f32551, one team
per frame. **32545 → 38395.**

**The end of a shoot-out.** Then 38395, `game_state` 24 recorded against our
30. The Amiga picks a winner from the penalty score and calls `EndOfGame`:

```
    end_penalty_shootout:                       ; SWOS2.s:27760
            clr.w   (playingPenalties).l
            ...
            lea     (topTeamInGame).l,a0
            move.w  (team1PenaltyGoals).l,d0
            cmp.w   (team2PenaltyGoals).l,d0
            bhi.s   set_winning_team_pointer
            lea     (bottomTeamInGame).l,a0
    set_winning_team_pointer:
            move.l  a0,(winningTeamPtr).l
            bsr.w   EndOfGame
```

The DOS build sets no winner and calls `PlayersLeavingPitch` instead
(swos.asm:104434), skipping `ST_GAME_ENDED` altogether. It has both routines
and calls `EndOfGame` from `UpdateTime`, so nothing about the recompilation
forced this.

The recording settles it as firmly as this corpus can. **`ST_GAME_ENDED`
appears on none of its 39066 frames.** `ST_PLAYERS_GOING_TO_SHOWER` starts at
f38395 — the exact frame our `EndOfGame` fired — and runs 275 frames, which is
`PlayersLeavingPitch`'s own DOS reload. **38395 → 39065 of 39066**, the last
frame being the terminal input edge.

`winningTeamPtr` is not cosmetic on the Amiga side, which is worth saying
because a "no winner is recorded" difference sounds like one: it is what
`UpdatePlayers`' post-match gate reads to choose between the winning and
losing reaction animations (SWOS2.s:31291).

### A table that runs off its end — in the DOS binary

`zkz-10174427` stopped at 21927 on `sprite[29].image_index`, the booked
player's card-or-number sprite: the reference shows an image and we show
nothing, and we keep showing nothing for as long as the rig looks.

`UpdateBookedPlayerNumberSprite` indexes a card table by `refTimer >> 3`, and
the tables are three of the shortest data in the game. Two of them are
identical in both releases — 30 bytes with a -1 at index 29. The third is not:

```
    secondYellowTable:                              ; SWOS2.s:16637 — 46 bytes
            dc.b 0,9,0,9,0,9,0,9,0,9,0,9,0,0,0,0
            dc.b 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
            dc.b 0,0,0,0,0,0,0,0,0,0,0,0,-1,0        ; terminator at index 44

    secondYellowTable db 0,9,0,9,0,9,0,9,0,9,0,9,0,0,0,0,0,0,0,0,0,0,0
    db 0, 0, 0, 0                                   ; swos.asm:245189 — 27 bytes
    unusedPalTable01 db 96, 3, 153, 9, 255, 15, ...  ; and this is what follows
```

**The DOS table is 27 bytes with no terminator at all**, so index 27 reads
into the palette table the assembler placed behind it. The recording dates it
to the frame. The booking commits at f21711, so `refTimer` is `frame - 21711`:

| index | refTimer | value | what the sprite does |
|---|---|---|---|
| 0..11 | 0..95 | 0/9 | the number blinks, 8 frames on, 8 off |
| 12..26 | 96..215 | zeros | hidden |
| **27** | 216..223 | **96** | SHOWN — past the end of the table |
| **28** | 224..231 | **3** | SHOWN |
| **29** | 232 | **153** | as a SIGNED byte, -103 — the negative sentinel |

and the recording turns that sprite on at f21927 and off at f21943, which is
refTimer 216 and 232 exactly. The last row is the part that matters beyond the
sprite: a negative entry is what makes the referee leave and the player be
sent off, so **a DOS second yellow ends 120 frames sooner than an Amiga one.**

Reproduced by giving the DOS path its own array — the 27 real bytes followed
by the palette bytes that actually sit behind them, read as signed. **21927 →
26448 of 26624.**

> This document has a whole section on reading past the end of a label into
> the next global, because it is how the Amiga original packs its data and how
> this port keeps getting caught. It is worth noting that the same bug exists
> in the DOS build, that nobody noticed for thirty years because the byte it
> lands on happens to be a palette entry, and that a rig comparing two
> implementations finds it in an afternoon while reading either listing alone
> would not — the Amiga table is correct, and the DOS table looks fine until
> you count it.

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

**And a third defect fell out of it, months later, that no test could see.**
`onScreen` was never only a cull — `player_anim.c` gates *animation* on it too,
while `movePlayer` moves the sprite regardless. So the moment "drawn" and
"on screen" stopped meaning the same thing, every sprite in the margin between
x=336 and `_screen_width` was drawn, moved, and never advanced a frame. **It
slid.** Not subtly: replaying `zkz-11021807` at 484x272 and counting drawn
sprites whose animation is skipped gives **16,411 of 66,491 — 24.7%**. A
tester reported it as "sprites sliding backwards" the day the first drop went
out.

The rig could not have caught it. `main.c` pins a `--replay-test` run to
`SCREEN_WIDTH x SCREEN_HEIGHT`, so the corpus runs at the one width where the
two culls coincide exactly — 250,865 frames at 100%, all of them blind to it.
Nor is that the only pin: the rig also forces `_gDosGameplay = 1`, in `main.c`
*and* again in `replay_test.c`, so **nothing in this document is evidence about
Amiga mode**, which is what a tester actually runs. Both pins are worth an
option; until then, read every result in this file as "DOS gameplay, 336x272".

The fix (`310c1a6`) gives animation its own per-frame visibility set in
`sprites.c` — `Sprite` has a hard size assert (110 Amiga / 118 SDL) so the flag
cannot live on the sprite — and only ever *adds* to what `onScreen` allows,
guarded on `!onScreen && _screen_width > SCREEN_WIDTH`. At the rig's geometry
the set stays empty and the gate is bit-for-bit what it was. That guard is not
belt-and-braces: a first attempt computed the test unconditionally, on the
argument that it must equal `onScreen` under replay. It disagreed on **1 call in
56,672** and desynced two recordings. When a fix is supposed to be inert in a
configuration, make it inert by construction rather than by argument.

The general lesson is the one the split itself teaches. `onScreen` carried two
meanings that were identical in the original — SWOS has one view — and
separating them fixed the two consumers that were looked at and broke the third
that was not. **Splitting an overloaded flag is not done when the callers you
were thinking about are right; it is done when every reader has been
enumerated.** `grep -n 'onScreen' src/` was four minutes of work and would have
named `player_anim.c` both times.

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
- **swos-port's own sprite construction.** Its `Sprite::init()` memsets from
  `offsetof(frameOffset)` and then applies its own initial values, so the game's
  static sprite table never reaches frame 0 there. Five fields differ as a
  result, and on **every one of them this port holds the value `data/SWOS2.s`
  gives** — see the table in "Can we claim 100% accurate?".
  `scripts/replay_test.sh` mutes `starting_direction`, `frame_switch_counter`,
  `field_3e`, `player_direction` and `save_sprite` at the `sprites` level;
  `--strict` compares them. Muting them is a statement about the harness, not
  about this port, and nothing after frame 0 depends on it.
- **An input edge inside one frame.** Both listings poll `Player1StatusProc`
  twice per frame when the game is stopped — once from `UpdateCameraBreakMode`'s
  fire check, once from the team dispatch — so the second call always clears
  the short-fire edge and `fireThisFrame` is 0. `playa-10095214`'s terminal
  frame has the reference seeing the edge on the SECOND call, which is only
  possible if the button went down between the two: real asynchronous input
  landing inside a frame. The rig injects a frame's input atomically before the
  frame runs and cannot reproduce it. Costs the last frame of that recording
  and nothing else, since it can only happen on a press the reference then acts
  on by leaving the match.
- **A tick that drifts in real time.** `currentGameTick` is advanced by the
  DOS timer interrupt, not by the game loop, so any wall-clock wait — a fade,
  a load — moves it without moving the frame count. The rig pins it to one per
  frame because determinism requires that, and the recording carries neither
  the tick nor `lastFrameTicks`. Reaches the `tick & mask` decisions, and at
  both stops it reached only a blinking HUD sprite. See "The tick is a
  real-time counter". Costs `zkz-10173518` at 25490 and `zkz-10174427` at
  21927.
- **The bench call's frame.** Two of the corpus's three bench presses are
  acted on one frame after the recorded button rises and one is acted on the
  same frame, with every gate input identical at both — sub-frame key timing
  the recording samples too coarsely to carry. See "The bench call the rig
  cannot place". Costs `playa-10090613` at 11222 and `zkz-11215003` at 11479.
- **v1.0/1.1 headers** stop before `season` / `min` / `maxSubstitutes`. The
  converter falls back to the DOS data segment's own defaults for the
  substitute counts (2 and 5); leaving them 0 benches players 11..15 before the
  first frame. `season` has no safe default, so the rig keeps this build's
  value and says so.

### The audit against SWOS2.s

When the sliding-sprite report came in, the reasonable question was whether the
47 defects had actually been checked against the listing or whether the corpus
had been chased into agreeing with itself. So every gameplay change of the run
was re-read against `data/SWOS2.s` line by line. **All of them hold**, and the
bug was not among them — it was a rendering change from `22f370a`, months
earlier, that no gameplay test touches.

Confirmed against the listing:

| change | listing | result |
|---|---|---|
| ball-Y fire gates, both halves | `32053-32060` | correct |
| fire-angle tolerance asymmetry | `32079-32083` | correct — `neg.b`/`bgt` is strict |
| `random_rotate_when_no_target`, incl. `d0 = -1` and the byte-vs-word add | `32460-32485` | correct |
| goalie reach, Amiga arm | `26266` | byte-identical to pre-session |
| `StopPlayers` | `28260-28278` | correct |
| `PlayersLeavingPitch` | `28007-28021` | correct |
| leaving-pitch position tables | `42511`, `42558` | correct |
| second-yellow / card tables | `16629-16640` | correct |
| goalpost three-arm restructure | `14829-14869` | correct |
| the dropped `cmpi.w #$FFFF` arm | `32233-32239` | dead after `andi.w #7` — dropping it is right |
| the `193 <= x < 478` band | `32270-32293` | `bcs` is unsigned lower; 478 falls through to the wings gate, and the C reproduces the fallthrough into `set_zero_turn` |
| `a2` is the ball, not a local | `29067`, `32103/32111`, `32392` | confirmed twice over |

The last row is worth spelling out, because it is the one that looked weakest.
`AI_SetControlsDirection` reads `SPRITE_Y(a2)` at `32392` without ever loading
a2 on that path; the port had declared it `NULL` with a `!= NULL` guard, so the
gate never fired. The claim that a2 arrives holding the ball rests on three
independent facts: `UpdatePlayers` does `lea (ballSprite).l,a2` at **29067**
with the comment *"A2 -> ball sprite, from now on"*, and nothing writes a2
between there and the `bsr` at 30124; the caller wraps that `bsr` in
`movem.l a1-a3,-(sp)` precisely because a2 is live and meaningful across it;
and the only two writes to a2 *inside* the routine (`32103`, `32111`) are each
immediately followed by `cmpa.l #0,a2 / beq` — the routine NULL-checks a2 where
it can be NULL and does not at `32392`, which is only consistent with a2 there
being a guaranteed-non-NULL input.

Two habits came out of the audit rather than out of any single defect. First,
**an uninitialised-looking register in a disassembly is a parameter you have
not found yet, and the caller is the tell** — guarding the read with `!= NULL`
turns a missing parameter into a branch that never runs, which is quieter and
worse than a crash. Second, **a green corpus is a statement about the
configuration the corpus runs in**, and this one runs in DOS mode at 336x272.
It was never evidence about the build a tester installs. The thing that would
have caught the sliding sprites was not another test — it was launching the
game once.

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
4. ~~**Player selection and possession**~~ — **the cluster is dissolved, and
   neither half of it was about possession.** `zkz-10190651` at 1113 was the AI
   taking a shot the reference did not, on an inverted gate ("Three inverted
   gates in one AI routine"); both `controlled_pl_direction` stops —
   `zkz-10140228` at 1108 and `playa-10095214` at 1976 — were the **goalkeeper**
   failing to dive, on a reach index that differs between the two releases ("The
   keeper's reach entry"). `controlledPlDirection` is written by whoever kicks,
   dives or heads, so it names the *symptom's owner*, never the cause: read the
   rest of the frame before believing the field name.
   The last field of it — `bottom_info.current_allowed_direction` at 1169 of
   `zkz-11054632` — is closed too, and it was a third thing again: a throw-in
   restart constant three globals away from the AI that acted on it ("One
   constant, three globals away"). That recording now replays **complete**.
   ! And check the setup before assuming which side is which: the `.swr`
   header's `top_team_player_no` / `bottom_team_player_no` do not read the way
   their names suggest — `top_info.player_number` / `bottom_info.player_number`
   are the fields that say which side is AI.
5. **The earliest stops.** Superseded — the three lowest were filed here as
   "the control-layer group and will not move", which is retracted; see item 6.
   [TESTS_RESULTS.md](TESTS_RESULTS.md) carries the live numbers.
6. ~~**The fire-counter family**~~ — **closed, and it WAS a defect.** This
   entry previously said the opposite, on the retracted premise that the
   recordings came from swos-port's C++ control layer. They come from the DOS
   binary. The cause is one line: `UpdateAndApplyTeamControls` gives a
   computer-controlled team no tail of its own, so an AI tackle clears
   **player one's** `pl1FireCounter`, and a fire hold that crosses an opponent's
   challenge never arms `normalFire`. `playa-10095214` went 10129 → 21613 on
   it; the other three stops with that signature — `playa-10144556` at 1696,
   `zkz-10190651` at 2570, `zkz-10173518` at 5051 — are re-opened by it.
   `zkz-12065730` at 1251 was the fourth and a different question — the DOS
   team-alternation counter, defect 42 — and it is closed too. All four moved.
7. ~~**Sprite state**~~ — **closed.** `sprites` is no longer the earliest stop
   on any recording — every level now stops on the same frame, or within one
   of it, and none of the remaining stops is a defect here. See
   [TESTS_RESULTS.md](TESTS_RESULTS.md) for what is left and why.
8. ~~**Decode `BenchDataV1` in the schema**~~ — **done** (section 6b), and it
   paid three times over: the `substituteInProgress` sign bug, the bench-camera
   constant, and `IsThisPlayerOkToSelect`. `zkz-11021807` went 926 → 2262 on
   the back of it. `bench.pl2_prev_direction` at 1251 of `zkz-12065730` is the
   next thread from the same spool.
9. The single **v1.2** recording still resolves to no (header, frame) pair.

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

The **bench button** is recorded per TEAM and injected on its rising edge.
swos-port's `TeamGeneralInfo` is our 144 bytes plus a trailing `secondaryFire`
it sets from its own bench event, so the recording says which team's bench a
press belongs to — the schema exposes that byte as `top_bench_button` /
`bottom_bench_button`, injected and never compared (we have no such field).
It reaches the game the way a player's press does, through the input code the
rig shares with netplay (`src/match_step.c`): as the bench button of the player
who controls that team, which `MatchStep_ApplyInput` routes back to that team.
A press for a team no human controls is not injected — `MainKeysCheck`'s guard
would refuse it. Against the recording that does not happen, but the rollback
rig plays its own match, where it can: in `zkz-11215003` at frame 11479 the
rig's bottom team has no human, and the rig used to open that computer team's
bench. The edge matters: `MainKeysCheck` sets `bench1Called` / `bench2Called`
from a key EVENT, and inside the menu the same flag is what *leaves* it, so
re-asserting a held button would open and close the bench on consecutive
frames. The edge latches are match state (`g_matchInputLatch`, in the
rollback registry), so a rollback restores them with everything else.

The v1.4 **`userKey`** goes in as a real keypress: the rig reverses `keycodeLut`
to find the scancode that maps to the recorded ASCII and pushes THAT into
`keyBuffer`, so `GetKey` and `MainKeysCheck` run their own guards rather than
the rig faking their result. Only 'S' (the stats screen) appears in the corpus.
The screen it opens waited for fire inside a single frame (`KeepStatisticsOnScreen`); with
input a frame at a time (`_gFrameStepInput`) it makes one pass per frame instead and stays
up into the next frame. Both recordings that open it (zkz-12065730, zkz-12080203) close it
within that pass, so their runs are the same -- and a netplay peer holding only a prediction
of the fire that closes it no longer spins for ever (docs/NETPLAY.md §5).

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
