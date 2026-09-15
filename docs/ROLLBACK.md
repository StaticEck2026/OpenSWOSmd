# ROLLBACK.md — the match state registry, and the rig that proves it

Rollback netplay (docs/NETPLAY.md) saves the game every frame and, when a late input
arrives, loads an earlier frame and plays forward again. That only works if the save
holds **every** variable the match depends on, and it is cheap and safe to send only if
it holds **nothing else**.

`src/sim_state.c` is that list — a registry of named variables. This document is how it
was built and how it is proved. Nothing goes in or out of the registry on a hunch: every
entry and every exclusion is backed by the rollback rig's evidence or by the executable
itself, and every change is checked by rolling real matches back.

| Path | Role |
|---|---|
| `include/sim_state.h`, `src/sim_state.c` | the registry: `SimState_Save/Load/SavePortable/LoadPortable/Size/Hash` |
| `include/state_wire.h`, `src/state_wire.c` | the state as a network packet: delta, deflate, validation (miniz in `third_party/miniz`) |
| `src/rollback_rig.c`, `include/rollback_rig.h` | the rig, inside the replay rig's headless match |
| `scripts/rollback_test.sh` | runs it over the corpus: conversion, both gameplays, parallel jobs, verdicts |
| `scripts/rollback_report.py` | merges the JSON reports of many runs into one table |

## 1. The registry

### Two tiers, and a third category that is in neither

| | what | size | restored on a local rollback | in a resync | in the checksum |
|---|---|---|---|---|---|
| `SIM_TIER_SIM` | everything the simulation reads, and everything that decides anything it reads | 15.0 KB portable | yes | yes | yes |
| `SIM_TIER_VIEW` | the highlight ring (`_goalBase`), its four pointers and the scene counters | 19.0 KB portable | yes | optional* | no |
| neither | render state describing what is **already drawn**: scroll offsets and their per-buffer copies, frontbuffers, render stacks, palette, copper list, HUD layer; the audio mixer | — | **no** | no | no |

The third row is excluded on purpose. After a rollback the screen still shows the
mispredicted frames; the renderer's own record of what it drew is what lets it draw the
next frame correctly from there. Rolling that record back would make it believe the
screen shows an older frame than it does.

\* The match reads the ring only while an instant replay plays; a session that plays
in-match replays has to send VIEW in a resync as well.

**The SIM tier is closed: no SIM byte may depend on a VIEW byte.** Otherwise a peer that
received only SIM would compute a different checksum from one that has both, and a
resync would raise a false desync. `scripts/rollback_test.sh --restore sim` is the test —
it restores SIM alone and requires the match, and the SIM hash, to come out identical.

The first split failed that test in 12 of 26 runs, and it moved most of what looked like
presentation into SIM:

- The **draw list** (`_sortedSprites`, `_numSpritesToRender`). The render-stack builder
  sets `onScreen` for every sprite in it, and the match reads `onScreen` (player
  auto-select, the referee, the camera, the animation gate). The list is not rebuilt
  every frame, and its order is history: `SortSprites` is a bubble sort that starts from
  last frame's order, so sprites at equal depth keep the order they had.
- **Every sprite in `_allSpritesArray`** — the clock, result, score digit, scorer and
  grid sprites included. Their `visible` and `y` decide the draw list's contents and
  order.
- **The scorer data** (`_team1Scorers`, totals, widths), which decides which scorer
  sprites are visible.
- `g_spriteAnimVisible` (sprites.c), the render-built list of widescreen-margin sprites
  the animation gate reads a frame later. The rig pins animation to the simulation view,
  so it is always empty there; it is registered on the code.

### Two encodings, and pointers

- **`SimState_Save/Load`** copy this build's memory byte for byte: a rollback inside this
  process, where it is a plain `memcpy`.
- **`SimState_SavePortable/LoadPortable`** write the **portable encoding**, the same bytes
  in every build — 32- or 64-bit, Linux, Windows or macOS — the way saves work. Every
  region has a `SimLayout`: a list of typed fields that tiles its element exactly. Byte
  runs are copied, 16- and 32-bit values are written big-endian, and every pointer is
  written as a 32-bit **identity** of what it points at, never as an address.
  `SimState_Hash` covers the portable encoding, so every platform computes the same
  checksum for the same match.

Only `Sprite` (`animationTable`, `frameIndicesTable`) and `TeamGeneralInfo` (9 fields) have
pointer fields; `TeamGame` and `ScorerInfo` hold only 8- and 16-bit ones. Every pointer the
match holds points into one of these, and its identity is the target's position in that
kind's list plus the element it points at:

| kind | points into | identity from |
|---|---|---|
| sprite | a `Sprite` | its slot in `_teamSpritesTable` |
| team info / team game / stats | `_topTeamData`…, `_topTeamInGame`…, `_team1StatsData`… | top or bottom |
| sprites table | an element of `_teamSpritesTable` | the element index |
| shot chance | `_teamPlOfs24Table`, `_kGoalieSkillTables[0..7]` | which table |
| animation table | one of 33 `AnimTable`s | its place in a name-sorted list |
| frame indices | one of 268 frame arrays, or a word inside one | the same, plus the word |
| goal ring | a byte of `_goalBase`, or its end | the offset |
| tactics | a byte of a `_teamTacticsTable` record (`_plPositions` points 9 bytes in) | record and offset |

Plus the 252 fan tile slots of `_pitchDatBuffer`, which hold `pitchChunkyBuffer + tile * 256`
(its low 32 bits on a 64-bit build) and travel as the tile's index. `0` stays the null
pointer and all-ones stays the `(AnimTable *)-1` every sprite that is not a player holds.

The animation-table and frame-array lists are generated from the source
(`scripts/gen_sim_state_ids.py` → `src/sim_state_ids.h`), sorted by name, so every build
numbers them identically; `scripts/rollback_test.sh` refuses to run on a stale list. A
pointer that fits no identity is never guessed at: `SimState_SavePortable` reports it,
`SimState_HashUnidentified` flags a checksum that contains one, and the rig fails the run.
That is how the first cut was caught registering `_advert1/2/3Pointers`: from frame 2672
of one recording they held no identity — they are copied from the sprite graphics once at
load and only read afterwards, so they are not match state and left the registry.

The portable SIM tier is 14,988 bytes, the VIEW tier 19,020. On the 32-bit build the SIM
tier's memory is exactly those 14,988 bytes: the portable layout is the 32-bit layout with
pointers made identities.

### The wire format

During a match only the **inputs** cross the network — one control word per player per
frame — plus a checksum of the SIM tier. GGPO's saves and loads never leave the machine.
State travels only in a **resync**: a peer recovering from a desync, or joining a match in
progress. That packet (`include/state_wire.h`) is binary:

| part | |
|---|---|
| header, 52 bytes, big-endian | magic `SWST`, version, flags, tiers, layout hash, frame, base frame, base hash, state hash, raw and packed sizes |
| payload | `SimState_SavePortable` of the tiers, XORed with a **base** state the receiver also holds, then raw deflate (miniz 3.0.2) |

Not JSON: a packet is never stored, is read only by the other peer of the session, and
being small is its whole purpose. The save format is JSON because a save has to outlive
rebuilds; a packet does not.

**Bases.** A delta turns everything that has not changed since the base into zeros, so it
is only as good as a base both peers are sure to hold:

- the **kick-off state** — every peer computes it identically from the match setup, so it
  serves a peer joining mid-match and any recovery;
- the **last frame both peers' checksums agreed on** — far smaller, for a desync caught
  within a few frames.

**Refused, never misread.** Decode loads nothing unless the magic, version and sizes are
right; the layout hash equals the receiver's (`SimState_LayoutHash`: every region's name,
portable size, tier, field layout and whether it is disabled, and the shape of the identity
lists — nothing about the build's memory); the base hashes to what the sender used; and the
inflated state hashes to what the sender packed; and every identity resolves. Tried with deliberately broken files:

| file | result |
|---|---|
| a loader with one registry entry disabled | refused: made by a different build |
| one payload byte flipped | refused: corrupt |
| another recording's kick-off as the base | refused: delta against a base this peer does not hold |
| truncated | refused |
| the unbroken file | loaded, and the match plays on identically |

**Sizes** (portable: SIM 14,988 B, SIM+VIEW 34,008 B):

| packet | bytes |
|---|---|
| SIM, standing alone | 1.6–1.9 KB at kick-off, 2.6–2.8 KB mid-match |
| SIM, delta against kick-off | 1.2–1.9 KB, 1.6 KB on average (75 resync saves over the corpus) |
| SIM, delta against 9 frames earlier | 0.12–1.3 KB, 0.5 KB on average (54,654 packets) — one datagram |
| SIM+VIEW, delta against kick-off | 1.3–14.8 KB, 7.3 KB on average — the highlight ring dominates |
| SIM+VIEW, delta against 9 frames earlier | 0.17–13.8 KB, 1.0 KB on average |

**Limits.**

- Both peers must run the same simulation — the same source and the same build flags. The
  portable encoding makes the *state* platform-independent; it cannot make two different
  games agree. A session handshake has to check that before a packet is ever needed.
- Both peers must play at the same simulation width until the width-invariance leak (§5)
  is removed.

### The crowd animation lives in the pitch tile map

`DrawAnimatedPatterns` toggles fan tiles between their static and animated variants by
rewriting slots of `_pitchDatBuffer`, then decides what to do next by reading them back.
It only ever picks from pitch rows 1–3 and 51–53 (sprite indices 21..83 and 1071..1133,
42 columns a row; the widescreen mirror stays within its row), so the registry holds
exactly those 252 slots (`SIM_PART`), not the 183 KB buffer. Each toggle also writes the
renderer's full-width copy (`_s_pitchFullPtrs`); `SimState_Load` rebuilds that copy for
the slots it changed (`AnimatedPatterns_SyncTileMirror`).

### What is deliberately not registered, and the proof

| excluded | why it is safe |
|---|---|
| per-match constants: the `gs*` timings, ball physics constants, pitch and card factors (`ApplyGameStyle`, `InitGameRestoreTeams`) | set before kick-off, equal on both peers by the match setup; `InitGameRestoreTeams` is only re-run to start a new match |
| `_ballSpeed`, `_ballSpeedAfterSave`, `_cameraSnapToPlayerCount`, `_disallowedTurnFlagsCounter`, `_turnDirCopiedFromCameraCount`, `_halfNumber`, `_timeVar`, `_strikeDestY/Z`, `_lastCameraDirection`, `_lastPlayerTurnFlags`, `_goalkeeperDiveAttemptCount`, `_nearestTeammatePtr`, `_findClosestBallDist`, `_teamNumThatScored`, `_substitutedPlX/Y`, `_pendingSampleId3_4`, the `dead*`/`*WriteOnly*` variables | no instruction in the executable reads them and no data relocation points at them (disassembly + `readelf -r`) |
| `prevLastPlayer`, `prevLastTeamPlayed`, `freeKickDestX_local` (update_players.c) | set and read within the same call |
| render state and audio (the table above) | describes what is drawn or played, not the match |
| `_joy0Dat`, `_joy1Dat`, the keyboard buffer | the input itself: netplay supplies it every frame |

## 2. How the registry was built

Three independent sources, because each alone misses something.

1. **What a match writes** (`--rollback=scan`, every frame's writable data diffed): 379
   of ~3800 symbols over the corpus.
2. **What the match needs** (`--rollback=discover`, leave-one-out per window; §4): over
   13 recordings × both gameplays, 1211 windows and 76,556 tests, 0 invalid windows —
   101 symbols change the game when left out, 47 of which the first registry lacked.
3. **What the match can write but the corpus never reaches** — penalties, cards and the
   referee, injuries, substitutions, instant replays. Found from the executable, not the
   recordings: a call graph from `GameLoop` over the disassembly (414 functions reach),
   the data symbols they write (634), minus those the corpus wrote, leaves 304, of which
   the game-state ones were added by reading the code that writes them.

On top of that, **state the random generator's call pattern depends on** is simulation
state even when it looks like presentation. In the original game (both releases) the
crowd chants (`sfx_crowd.c`), the advertising boards (`adverts.c`), the crowd animation
(`animated_patterns.c`) and the goal celebration all call `Rand()`, so their state
decides what the next gameplay `Rand()` returns. Decoupling them onto a separate
generator would shrink the registry but change the random sequence the original plays;
that is a separate decision.

## 3. How the rig plays a match

The rig runs inside the recorded-game replay rig (docs/TESTS.md): headless, unpaced,
game ticks owned by the loop. A recording supplies **only the match setup and each
frame's joystick input**. Nothing is compared with the recording; the game is always
compared with itself, so the whole corpus is usable in **both** gameplays
(`--dos-gameplay=on|off`).

| | replay comparison | rollback rig |
|---|---|---|
| `Rand`/`Rand2` | pinned to the recording's constant | **real generators**, seeded (`--rollback-seed`) — a constant would hide every RNG variable the registry lacks |
| gameplay | forced DOS | as given on the command line |
| instant replays | as recorded | off: they leave the loop body the rig hooks |
| audio | dummy driver | **no device, no pump thread** (`SWOS_AUDIO_OFF`) |
| timer thread | runs the ISR at 1 kHz | **quiesced** at kick-off (`Platform_QuiesceTimerThread`) |
| match ends before the inputs | a failure | the end of the run |

The last three guarantee that **every write to game memory comes from the game loop,
once per frame**. Without them the ISR and the audio mixer write game globals from other
threads a varying number of times per frame, and no two runs agree.

A rollback happens at the top of `GameLoop`'s body (`ReplayTest_FrameStart`): the rig
restores state and rewinds the replay's input cursor (`ReplayTest_SetDriverState`), and
the loop plays the same frames again. The replay driver's one-time match setup (extra
time and penalty flags, marked players, the tick counters) is applied **before** that
first frame boundary, so a window starting at frame 0 saves it and a rewind keeps it.
(It used to run after, which made every rollback to frame 0 re-simulate with a tick delta
of 0 — a rig bug the whole-match trace caught at frames 1–2.)

## 4. Modes and options

`bin/SWOS.x86_64 --replay-test=<file.swr> --rollback=<mode> [options]`

| mode | what it does | passes when |
|---|---|---|
| `oracle` | at frame F copy **all** writable data (`.data`+`.bss`, 17.5 MB); play K frames; copy it back, rewind, play them again | every byte comes out the same (4 wall-clock stamps excepted) |
| `check` | the same with only the **registry** restored | the restored tiers come out byte-identical; the report names what differs and the unregistered variables that changed in the window |
| `discover` | per window, for every symbol the window wrote: restore **everything except that symbol** in a forked child and play the window again | — evidence: which symbols change the game when left out |
| `scan` | diff the writable data every frame | — every symbol a match writes |
| `trace` | per frame: `<frame> <SIM hash> <VIEW hash> <observable hash>` | two traces compared by the script |

| option | |
|---|---|
| `--rollback-window=K`, `--rollback-every=N`, `--rollback-first=F` | window length, spacing, first frame |
| `--rollback-restore=all\|sim` | restore both tiers (default) or only SIM — proves the resync minimum alone keeps the match identical |
| `--rollback-disable=a,b` | leave registry entries out without rebuilding: a trial removal |
| `--rollback-portable` | restore through the resync encoding |
| `--rollback-wire` | every `check` rollback restores from a state packet — a delta against the previous window start, deflated, validated (script: `--wire`) |
| `--rollback-dump=<file>`, `--rollback-dump-frame=<n>` | every region's portable bytes at one frame, one line each — diff two runs to find a region that differs |
| `--rollback-save-state=<path>`, `--rollback-save-frames=a,b` | write a resync file (portable SIM, portable VIEW, the input cursor) to `<path>.<frame>` at each frame |
| `--rollback-load-state=<file>` | at the first frame, load a resync file (VIEW too unless `--rollback-restore=sim`) and continue from its frame |
| `--rollback-report=<json>`, `--rollback-trace=<file>`, `--rollback-seed=<n>` | |

`scripts/rollback_test.sh` (mode `check`) runs three proofs:

1. **Determinism** — two plain runs, in separate processes at different load addresses,
   trace identically. If not, nothing else means anything.
2. **In-window** — every K-frame re-simulation reproduces the restored tiers exactly.
3. **Whole match** — the rolled-back run's trace equals a plain run's trace **at every
   frame to the end**. A variable whose effect shows up hundreds of frames after the
   window (a timer, a counter, the draw order) passes (2) and fails (3).

With `--restore sim` the VIEW column is not compared. `--disable` is applied to the
reference runs as well, so both hash the same regions.

`--mode resync` is the harshest test: a plain run saves resync files at 25, 50 and 75% of
the match; each is loaded into a **fresh process at frame 0**, which must then trace
identically to the plain run from that frame to the end. A rollback window leaves
unregistered state at most K frames stale; here it has the whole match so far to have
drifted, so a variable that changes only at kick-off, half time or a goal cannot hide.
With `--restore sim` only the SIM tier is loaded — a peer resynced with the minimum. The
files hold state packets, deltas against the kick-off state the loading process computes
for itself, so this is also the end-to-end test of the wire format.

### What "changes the game" means in `discover`

After playing the window with symbol S left out, the child compares the writable data
with the real window's end state, **excluding S itself**. A difference anywhere in the
*game state* — the SIM tier plus the replay rig's observable fields (all 33 sprites, both
`TeamGame`s and `TeamGeneralInfo`s, the stats, clock and camera) — makes S **GAME**. A
difference only elsewhere makes it **OTHER**. No difference: **INERT**. A child that
crashes or hangs counts as GAME: leaving `_gameState` out sends `UpdatePlayers` out of
bounds. Every window also runs one whole-data pass in a child; a window where that does
not reproduce is reported **invalid** and its evidence discarded.

Leave-one-out evidence is one-sided:

- **GAME is proof** that the symbol is needed.
- **INERT is only absence of evidence** in the windows sampled. `_currentTick` gates the
  ball-control speed bonus and still came out INERT. Nothing leaves the registry on
  INERT alone; the whole-match trace is what licenses a removal.
- Two symbols holding the same information each come out INERT. `check` catches that.

## 5. Findings

- **The method is sound.** `oracle` re-simulates identically except for four millisecond
  wall-clock stamps, all presentation/pacing (`s_scroll_frac_ms`, `s_hud_stamp`,
  `s_last_isr_ms`, `s_last_main_render_ms` — `k_volatile` in the rig). No match state
  lives on the heap, in another thread, or in the wall clock.
- **Presentation shares the gameplay RNG** (§2).
- **The crowd animation keeps its state in the pitch tile map** (§1).
- **Presentation feeds the simulation through the draw list.** A render pass writes
  `onScreen`, and the list it walks holds the HUD sprites too; so the clock, score and
  scorer sprites are simulation state (§1). Found only by restoring SIM alone.
- **The SDL buffer swap is render-only.** `SwapBuffersAndCameraViewState` copies each
  buffer's saved camera back into `_cameraX`/`_cameraY` (on the Amiga that is the
  double-buffered view); SDL's `waitVBlankSync` saves the camera around the swap and
  puts it back with `SetCameraCoordinates`, so the per-buffer copies stay render state.
- **The simulation still reads the render width.** `DrawAnimatedPatterns` picks the fan
  tile column with `Render_CameraX()`, and the camera clamp in `SetCameraCoordinates`
  uses the view width; a 484-wide run and a 336-wide run of the same recording differ
  from frame 0. Netplay peers must run the same simulation width until that is removed.

## 6. Across builds

The portable encoding is only worth anything if every build produces the same bytes.
`scripts/rollback_xbuild.sh` plays the same recordings on every build it finds —
`bin/SWOS.x86_64` (64-bit Linux), `bin/SWOS` (32-bit Linux) and `bin/SWOS.exe` (Cygwin,
Windows, run through `cmd.exe` from WSL) — and requires two things:

- **traces:** a plain run's per-frame SIM, VIEW and observable hashes are identical on
  every build — the same match, and the same checksum for it;
- **resync:** state saved by one build at 25, 50 and 75% of the match, loaded by *each
  other* build into a fresh process at frame 0, plays on identically to the end.

The observable hash (the replay rig's ~2500 fields) is portable too: values big-endian at
their recorded width, pointers as identities.

Run over four recordings (the shortest in both gameplays, and 24,601- and 31,636-frame
matches), 64-bit Linux, 32-bit Linux and Windows:

| | result |
|---|---|
| traces | 8 of 8 identical to the 64-bit build at every frame — SIM, VIEW and observable |
| resync | 66 of 66 loads identical to the end: every build loaded every other build's state (one save point of one recording falls after its match ends, so 66 of 72) |
| packets | SIM delta against kick-off, 1,282–1,657 bytes, 1,456 on average |
| unidentified pointers | none, in any run |

Before this, the same recordings on the same three builds already passed the replay rig's
comparison against the recording (951 fields a frame, 1519 of 1519 frames): the game
itself computes the same match on each. The portable encoding is what lets a *state* and
a *checksum* cross between them.

## 7. Results

The registry: 354 regions — **SIM 14,988 + VIEW 19,020 bytes portable** (16,612 + 19,036
in a 64-bit build's memory, the portable sizes exactly in a 32-bit one). The corpus: 13
recordings × both gameplays = 26 matches, 491,993 frames.

On the build with the portable encoding:

| test | configuration | result |
|---|---|---|
| determinism | two plain runs, separate processes, different load addresses | identical per-frame traces (checked at the start of every script run) |
| check, both tiers | window 8, every 64 | 7,696 rollbacks, 0 failed; 26 of 26 matches identical to the plain run at every frame |
| check, **SIM only** | window 8, every 9 | 54,656 rollbacks, 0 failed; 26 of 26 identical — the SIM tier is closed |
| **state packets**, both tiers | `--wire`, window 8, every 9: every restore decoded from a delta packet against the previous window start | 54,656 rollbacks, 0 failed, 0 packets refused; 26 of 26 identical |
| **state packets**, SIM only | the same with `--restore sim` | 54,656 rollbacks, 0 failed, 0 refused; 26 of 26 identical |
| resync, both tiers | state packets saved at 25/50/75% of a plain run, loaded into a fresh process at frame 0 | 75 of 75 loads identical to the end (691,773 frames); 3 save points never reached because the match ended first |
| resync, **SIM only** | the same, loading only SIM | 75 of 75 identical |
| portable encoding, in process | `--rollback-portable`, window 8 every 9, shortest recording, both gameplays | 336 rollbacks, 0 failed; identical |
| across builds | 64-bit Linux, 32-bit Linux, Windows (§6) | 8 of 8 traces and 66 of 66 cross-build resync loads identical |

No run held a pointer with no identity, and no packet was refused.

On the build before it (357 regions, pointers hashed as offsets from the executable). Its
rollbacks go through the raw in-memory save and load, which the portable encoding did not
change; the registry has lost three load-time tables since:

| test | configuration | result |
|---|---|---|
| oracle | whole writable data restored, window 8, every 64 | 7,696 rollbacks, 0 failed — the method itself: no match state outside `.data`+`.bss` |
| check, both tiers | window 8, every 9 | 54,656 rollbacks, 0 failed; 26 of 26 identical |
| check, both tiers | window 3, every 5 | 98,393 rollbacks, 0 failed; 26 of 26 identical |
| check, both tiers | window 1, every 2 | 245,992 rollbacks, 0 failed; 26 of 26 identical |

### Trial removals

On the build before the portable encoding, each group was left out with `--disable` and
checked over the corpus with a SIM-only restore every 9 frames:

| group | bytes | result |
|---|---|---|
| current-frame input flags (`_pl1Up` … `_pl2DownRight`, directions, fire) | 25 | pass |
| previous-frame input flags and edge presses (`_pl1WasUp` …, `_pl1UpPress` …) | 16 | pass |
| AI helpers (`_aiRand`, `_aiAttackHalf`, `_AI_maxStoppageTime`, `_totalSkillsFactorSum`) | 8 | pass |
| bench menu layout (`_benchColors`, `_benchY`, `_subsMenuX` …) | 34 | pass |
| stats copies (`_statsTeam1GoalsCopy` …, `_statsNumber`) | 14 | pass |
| `_currentTick` | 2 | pass |
| ball helpers (`_ballDefensive*`, `_ballNext*`, `_ballNotHigh*`, `_ballIn*Area`, `_playerHadBall` …) | 31 | **fail** — 20 of 26 matches diverge, first at frames 343–8470 |
| HUD sprites (`_ballShadowSprite`, `_playerMarkSprite`, `_currentPlayerNameSprite` …) | 944 | **fail** — 25 of 26 diverge from frame 0 |

Nothing was removed. The six passing groups total 99 bytes — 0.6% of the SIM tier raw,
and next to nothing compressed — and a pass says only that the corpus never needed them,
not that no match will: `_currentTick` gates the ball-control speed bonus in the code.

The failures are the more useful result. **Every ball helper came out INERT in
`discover`**, and the HUD sprites were among the "registered, no window needed" symbols —
yet leaving them out changes most matches. Leave-one-out evidence says what one window
needed; only a whole-match trace licenses a removal.

## 8. Not covered by recordings

What the corpus reaches, from the variables the scan saw written: open play, goals,
throw-ins, corners and free kicks, substitutions (team 1's only — `_team2NumSubs` is never
written), half time (`_halfNumber`, the half-time result sprite) and full time
(`_endGameCounter`).

What it never reaches: **cards and the referee** (`_refState`, `_whichCard`,
`_bookedPlayer` and `_refereeSprite` are never written), **penalty shoot-outs**
(`_playingPenalties`, `_penaltiesTimer`, the penalty goals), and **instant replays**
(off in the rig). Injuries live in a sprite field, which a per-symbol scan cannot
distinguish from other sprite writes, so the corpus says nothing either way about them.

Those variables are in the registry by code evidence (§2.3); the rig can only confirm them
once a recording reaches them.

## 9. Running it

```bash
make X86_64=1                                   # Linux: the whole-memory modes need the symbol table
scripts/rollback_test.sh                        # determinism + check, shortest recording, both gameplays
scripts/rollback_test.sh --all                  # the corpus
scripts/rollback_test.sh --all --window 1 --every 2
scripts/rollback_test.sh --all --restore sim    # the resync minimum alone
scripts/rollback_test.sh --mode resync --all    # load mid-match state into a fresh process
scripts/rollback_test.sh --mode resync --all --restore sim
scripts/rollback_test.sh --all --disable _currentTick   # trial removal
scripts/rollback_test.sh --mode oracle --all
scripts/rollback_xbuild.sh                      # 32/64-bit and Windows: traces + resync between builds
scripts/gen_sim_state_ids.py                    # after adding or renaming an animation table / frame array
scripts/rollback_test.sh --mode discover --all  # -> tests/rollback/discover-summary.txt
scripts/rollback_report.py tests/rollback/discover-*.json
```

Recordings come from `tests/dos` (docs/TESTS.md, "The corpus"); the script converts them
at the smallest level, since only the inputs are used. Output lands in `tests/rollback/`.

`check` and `trace` work on every SDL target. `oracle`, `discover`, `scan` and the
symbol names in a `check` report read the executable's own ELF symbol table and need
64-bit Linux with an unstripped binary (the normal `make X86_64=1` build).
