# Gameplay regression testing — recorded-game replay

Deterministic gameplay testing for OpenSWOS: replay a recorded match
frame-by-frame and assert that our simulation reproduces the reference state
*exactly*, every frame.

The idea is borrowed wholesale from **swos-port**
(`C:\Projects\GitHub\swos-port`), whose `tests/src/tests/RecordedDataTest.cpp`
replays `.RGD` recordings against its DOS port. That corpus is our starting
point; an Amiga corpus follows once we can harvest it (see
[Harvesting Amiga data](#harvesting-amiga-data)).

Status: **data imported; format decoded and verified; decoder written. Replay
rig not yet implemented.**

```bash
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

## 3. What the reference rig asserts

Per frame, after injecting the recorded input and running one tick
(`RecordedDataTest::verifyFrame`):

- `topTeamInGame` / `bottomTeamInGame` (`TeamGame`) — full compare
- `team1StatsData` / `team2StatsData` — full compare
- `topTeamData` / `bottomTeamData` (`TeamGeneralInfo`), plus player sprite order
- all 33 sprites
- `cameraX/Y` and both camera velocities
- game time in minutes, both goal counts
- `gameState`, `gameStatePl`
- bench state (v1.3+), `fireBlocked` / `statsTimer` / `resultTimer` (v1.4+)

Every one of those globals exists in our tree (`_topTeamInGame`,
`_bottomTeamData`, `_cameraX`, `_gameStatePl`, `_currentGameTick`, …). Setup is
also within reach: the rig pushes the header's two `TeamFile`s into
`selectedTeams` and calls `InitAndPlayGame()`, which we have in
`src/game_init.c`.

---

## 4. Why we cannot replay the bytes directly

The recording is a packed dump of the **DOS port's** structs. Ours have diverged
— deliberately, per the untangle roadmap. Measured:

| struct | ours 64-bit | ours 32-bit | swos-port |
|---|---:|---:|---:|
| `TeamTactics` | 370 | 370 | **370** ✓ |
| `PlayerFile` | 38 | 38 | **38** ✓ |
| `TeamFileEx` | 684 | 684 | **684** ✓ (their `TeamFile`) |
| `SpriteGraphics` | 32 | 24 | **24** |
| `TeamGeneralInfo` | 180 | 144 | **145** |
| `Sprite` | 118 | 110 | (frame-derived) |
| `TeamFile` | 76 | 76 | — (ours is the 76-byte header only) |

Three distinct problems:

1. **Pointer growth.** `Sprite`, `SpriteGraphics` and `TeamGeneralInfo` hold
   pointers. swos-port sidesteps this with `SwosDataPointer<T>`, a 32-bit
   VM-relative offset; ours are real pointers, 8 bytes on x86_64.
2. **`TeamFile` means different things.** Theirs is header + 16 players (684);
   ours is the 76-byte header, and `TeamFileEx` is the full 684-byte record.
   **Map their `TeamFile` to our `TeamFileEx`.**
3. **No contiguous image.** swos-port keeps one flat memory image and can
   `memcmp` a struct against a recorded region. Our globals are separate objects
   at linker-chosen addresses (the whole point of the untangle), so region
   compares are impossible *and* forbidden — the same rule the save format
   follows (see CLAUDE.md, "no address ranges").

Note `TeamGeneralInfo` is 144 for us on 32-bit against their 145 — an actual
layout difference to chase, not just pointer width.

**Consequence: the rig must compare NAMED FIELDS, never memory regions.**

---

## 5. Design

Two pieces, deliberately split so the fragile part is offline and in Python.

### 5a. `scripts/rgd_convert.py` — decode to a neutral format

Parses `.RGD.gz` using the version table above and emits a self-describing
per-frame record with **named fields**, independent of any struct layout or word
size. Rationale:

- the DOS layout is pinned in one readable place instead of smeared through C
  structs that must not change;
- the C rig never parses DOS bytes, so 32/64-bit and pointer growth stop
  mattering;
- the **same** output format can carry the Amiga dumps later, so the rig has one
  input format and the platform differences live in the converters.

Emit a compact binary keyed by field name (JSON per frame would be far too large
at 31 k frames), or JSON for a `--frames 0..N` slice when debugging.

### 5b. The C rig

A `--selftest` mode in the SDL binary rather than a separate harness: it needs
the real globals, and the gdb harness already proves we can drive and inspect a
live match. Per frame: set input → run one tick → compare each named field
against its global → report the first divergence with frame number and field.

The first divergence is the whole value here: gameplay is chaotic, so one wrong
bit diverges everything within a few frames. The report should name the frame
and field and stop.

### 5c. DOS mode

The corpus is DOS gameplay, so the rig must set DOS tuning before
`InitAndPlayGame()` — `_gDosGameplay = 1` (`--dos-gameplay=on`), which selects
the 70 Hz tick and the DOS movement/goalkeeper/ball constants
(`src/dos_gameplay.c`). Also set from the header: `pitchType`, `gameLength`,
`autoReplays`, `allPlayerTeamsEqual`, `season`, `min/maxSubstitutes`, the six
user tactics, and `fixedRandomValue` — the recordings pin the RNG, without which
nothing is reproducible.

Getting any of these wrong looks exactly like a gameplay bug, so assert them
rather than assuming.

---

## 6. Harvesting Amiga data

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
   registers, walk the same globals section 3 lists, append one frame record,
   restore, run the displaced instructions, return.
3. **Emit the neutral format directly** — same schema as
   `scripts/rgd_convert.py` output. Do not reinvent `.RGD`: the Amiga structs
   are not the DOS ones, and hand-decoding a second binary layout is exactly the
   work the converter exists to avoid.
4. **Write it out** with WHDLoad's `resload_SaveFile`. 31 k frames × ~7 KB is
   ~200 MB uncompressed, so dump a bounded window (a few thousand frames) or
   only the fields the rig asserts.
5. **Verify nothing moved**: diff the built image against an unpatched build —
   byte-identical except the patched instructions. Same discipline as checking
   `bin/SWOS2.map` for the C port.

### Why the Amiga corpus matters

The DOS corpus validates DOS gameplay tuning. The Amiga recordings validate the
path the port actually derives from (`data/SWOS2.s`), where any divergence is a
genuine porting bug rather than a DOS-vs-Amiga behavioural difference.

---

## 7. Order of work

1. Decide how the 66 MB is stored (LFS / untracked + fetch script).
2. ~~`scripts/rgd_convert.py`~~ — done; 12 of 13 files decode and verify.
3. Resolve v1.2 (1 file, no (header, frame) pair found).
4. The C rig, first asserting only the cheap scalars (game time, goals,
   `gameState`, camera), then teams, then all 33 sprites.
5. The Amiga dumper.

Start the rig against `zkz-12065730` — v1.5, fully decoded, and the shortest at
1519 frames.
