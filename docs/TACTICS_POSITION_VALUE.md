# Tactics Editor — Position-Value & the Tick / Cross Indicators

This document explains the green-tick / red-cross marks shown in the **tactics
editor** (and the squad / play-match screens) when you move a player, and the
exact formula behind them. It covers the on-screen indicators, the comparison
logic, the per-player *position-value* formula (`GetPlayerPrice`), the
team-average formula (`ComputeOldPlayerSkillForViewResult`), and the full set of
weight tables that drive it.

All line numbers refer to this repo (SDL port); the logic is a faithful port of
the original Amiga `SWOS2.s`.

---

## 1. What the marks mean

When you change a player's position, SWOS recomputes a **position-value** for
every player — a single number (0..49) that measures *how well that player's
skills suit the areas of the pitch their tactic sends them to*. It then compares
the new value against a snapshot taken when you entered the editor:

| Mark | When it shows |
|------|---------------|
| **green tick** (small, per player) | that player's value **rose** |
| **red cross** (small, per player) | that player's value **fell** |
| *(nothing)* | unchanged |
| **green tick** (big, whole team) | the team **average** value rose |
| **red cross** (big, whole team) | the team average value fell |
| *(nothing)* | team average unchanged |

So it is **not** a legality check — it's a "did this change make the player /
team better or worse?" verdict.

### 1.1 Sprite constants

Defined canonically in `include/play_match_menu.h`. Indices verified against
`Amiga_Images_16/MENUS.bmp` and `SWOS2.s` `UpdateBigCheckMark`:

| Constant | Index | Graphic |
|----------|-------|---------|
| `SPR_CHECK_BIG`   | 206 | big green tick ✓ |
| `SPR_CROSS_BIG`   | 207 | big red cross ✗ |
| `SPR_CHECK_SMALL` | 208 | small green tick ✓ |
| `SPR_CROSS_SMALL` | 209 | small red cross ✗ |

---

## 2. The comparison logic

### 2.1 Small, per-player — `src/edit_tactics.c` (`EditTacticsAfterDrawPlayerNames`)

```c
if (slot < 11) {                                  /* first 11 players only */
    uint8_t curPos = _playerPositionsArray[playerIndex];      /* value now      */
    uint8_t oldPos = _oldPlayerPositionsArray[playerIndex];   /* snapshot value */
    if      (curPos > oldPos) entry39->fgWord = SPR_CHECK_SMALL;  /* rose → tick  */
    else if (curPos < oldPos) entry39->fgWord = SPR_CROSS_SMALL;  /* fell → cross */
}
```

`_oldPlayerPositionsArray` is a snapshot of `_playerPositionsArray` taken on
entry (`EditTacticsMenuSavePlayerPositions`, `src/edit_tactics.c:493`). The same
per-player check is drawn on the squad screen in
`src/play_match_menu.c:1175`.

### 2.2 Big, whole-team — `src/edit_tactics.c` (`SetBigCheckSprite`) / `src/menus.c` (`UpdateBigCheckMark`)

```c
if (_oldPlayerSkill == _newPlayerSkill) return;             /* equal → hidden */
if (_oldPlayerSkill >  _newPlayerSkill)  fg = SPR_CHECK_BIG; /* stronger → tick  */
else                                     fg = SPR_CROSS_BIG; /* weaker  → cross  */
```

`_oldPlayerSkill` holds the team's **freshly recomputed** average (refreshed by
`ComputeOldPlayerSkillForViewResult`); `_newPlayerSkill` is the baseline from the
last commit. (The variable names read backwards — `_oldPlayerSkill` is the
*current* value — but the behaviour matches the per-player check above:
current > baseline ⇒ improvement ⇒ tick.)

---

## 3. The per-player position-value pipeline

```
UpdatePlayerPositionAndReturnRole(team, i)   src/match_sim.c:1993
   └─ for an outfield player in an outfield slot:
        GetPlayerPrice(team, playerBase, i)  src/match_sim.c:1832   ← the formula
   └─ writes the result to _playerPositionsArray[slot]
   └─ returns (role<<16 | value)

ComputeOldPlayerSkillForViewResult(team, n)  src/match_sim.c:744
   └─ sums every player's value into a team average → _oldPlayerSkill
```

`UpdatePlayerPositionAndReturnRole` has a fast path (when the team's values are
already "clean" it just returns the cached `cachedMatchValue`) and a full path
that calls `GetPlayerPrice`. Edge cases it applies **before** the formula:

| Situation | Value |
|-----------|-------|
| Goalkeeper in the GK slot | the player's stored `price` (skill) |
| Non-GK in the GK slot, or GK in an outfield slot | `0` |
| Substitute (index ≥ 11) | stored `price` |
| Sold / on-loan player (`PF_LOAN`) | `1` |
| Injured player | `0` |
| otherwise | `GetPlayerPrice(...)` (min 1) |

---

## 4. `GetPlayerPrice` — the formula (`src/match_sim.c:1832`)

This is the heart of it. A player's value is how well their 7 skills match the
areas of the pitch their tactic sends them to, summed over the whole pitch.

### 4.1 Skills

The 7 player skills are packed into 4 bytes and unpacked to a value of **1..8**
each:

```c
d5 = (passing<<24) | (velocityHeading<<16) | (tacklingControl<<8) | speedFinishing;
d5 &= 0x07777777;   /* keep 3 bits per nibble */
d5 += 0x01111111;   /* +1 → range 1..8        */
```

The 7 skills, in the order the inner loop reads them (low nibble first):

`Finishing, Speed, Control, Tackling, Heading, Velocity, Passing`

### 4.2 The pitch model: 35 ball-quadrants

The pitch is divided into a **7 × 5 grid of 35 "ball-quadrants"** — 7 bands along
the length (own goal → opponent goal) × 5 across the width. Quadrant number
`Q = yband + 5·xband`, where `xband` 0..6 is the length band (0 = defensive,
6 = attacking) and `yband` 0..4 is the width band.

A player's **tactic** is a 35-entry list: for each of the 35 possible
ball-positions, which pitch coordinate that player moves to. That coordinate is
mapped to a ball-quadrant via `_quadrantCoordinatesToBallQuadrant`.

### 4.3 The loop (per ball-quadrant, all 35)

For each ball-position `d3` (0..34):

1. `ballQ = quadrantCoordinatesToBallQuadrant[ playerTactics[d3] ]` — where the
   player is for this ball position.
2. **Skill match**: read the 7-skill *ponder* for `ballQ`
   (`_priceSkillPonders[ballQ]`, one nibble per skill) and accumulate
   `quadPrice += skill · skillValuePonders[ponder]` over the 7 skills. Skills
   with ponder 0 don't count.
3. `quadPrice >>= 1`.
4. **Position importance**: `× skillsPerQuadrant[positionType][ballQ]` — how much
   this quadrant matters for the player's position type (1..4).
5. **Distance falloff**: `× quadrantDistanceFactor[d3][ballQ]`, then `>> 3`.
6. **Crowding** (human teams): `÷ (players sharing that quadrant)`.
7. **Nearness**: `× CountNearnessFactor(...)`.
8. Accumulate into `_totalSkillsFactorSum`.

### 4.4 Final value

```c
priceIdx = _totalSkillsFactorSum / 350 - 8;   /* clamp to 0..49 */
if (priceIdx > storedPrice)                   /* a rise is softened via a table */
    priceIdx = plPricesIncreaseTable[priceIdx - storedPrice] + storedPrice;
```

That `priceIdx` (0..49) is the player's **position-value** — what lands in
`_playerPositionsArray` and feeds both tick/cross checks.

---

## 5. The weight tables

### 5.1 `_skillValuePonders[8]` — ponder → weight (`src/swos_data_bulk_imports.c:252`)

```
ponder:  0   1   2   3   4   5   6   7
weight:  0   1   3   6  10  15  21  28      (triangular numbers T(n) = n(n+1)/2)
```

A skill's contribution scales **quadratically** with its ponder, so the few
skills that matter most at a position dominate.

### 5.2 `_priceSkillPonders[35]` — which skills matter per ball-quadrant (`src/tactics_data.c:114`)

Decoded (one column per skill, value = ponder 0..7):

```
ballQ | Fin Spd Ctl Tkl Hea Vel Pas
------+----------------------------
    0 |   0   2   0   5   0   0   0     ← defensive band (xband 0): Tackling dominates
    1 |   0   1   0   4   2   0   0
    2 |   0   0   0   4   3   0   0
    3 |   0   1   0   4   2   0   0
    4 |   0   2   0   5   0   0   0
    5 |   0   3   0   3   0   0   1
    6 |   0   2   0   3   1   0   1
    7 |   0   1   0   4   2   0   0
    8 |   0   2   0   3   1   0   1
    9 |   0   3   0   3   0   0   1
   10 |   0   3   0   2   0   0   2
   11 |   0   1   0   3   1   0   2
   12 |   0   0   0   3   1   0   1
   13 |   0   1   0   3   1   0   2
   14 |   0   3   0   2   0   0   2
   15 |   0   3   1   0   1   0   2
   16 |   0   0   0   3   1   0   2
   17 |   0   0   0   1   1   0   3     ← midfield: Passing rising
   18 |   0   0   0   3   1   0   2
   19 |   0   3   1   0   1   0   2
   20 |   0   3   2   1   0   0   2
   21 |   0   2   2   0   0   0   3
   22 |   0   0   2   0   0   1   4
   23 |   0   2   2   0   0   0   3
   24 |   0   3   2   1   0   0   2
   25 |   0   4   2   0   0   0   3
   26 |   0   2   2   0   1   2   2
   27 |   2   1   3   0   3   2   3
   28 |   0   2   2   0   1   2   2
   29 |   0   4   2   0   0   0   3
   30 |   0   3   3   0   0   0   3     ← attacking band (xband 6): Fin/Ctl/Pas
   31 |   1   2   2   0   0   2   2
   32 |   4   2   1   0   2   2   1     ← in front of goal: Finishing dominates
   33 |   1   2   2   0   0   2   2
   34 |   0   3   3   0   0   0   3
```

The gradient is clear: **defensive quadrants reward Tackling/Heading, attacking
quadrants reward Finishing/Control/Passing.** This is why a striker dropped into
defence loses value and a tackler pushed forward does too.

### 5.3 `_skillsPerQuadrant[245]` — quadrant importance per position type (`src/tactics_data.c:110`)

7 rows (player position types 1..7, defensive → attacking) × 35 quadrants,
value 1..4 = how much that quadrant counts toward this position's value
(position type 0 = GK is handled separately):

```
pos\Q |  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34
  1   |  4  4  4  3  3  4  4  4  3  3  4  4  3  3  3  4  4  3  3  3  3  3  2  2  2  3  2  1  1  2  3  1  1  1  2
  2   |  3  3  4  4  4  3  3  4  4  4  3  3  3  4  4  3  3  3  4  4  2  2  2  3  3  2  1  1  2  3  2  1  1  1  3
  3   |  3  4  4  4  3  3  4  4  4  3  2  4  4  4  2  1  3  4  3  1  1  2  3  2  1  1  1  3  1  1  1  1  4  1  1
  4   |  3  3  1  2  2  3  3  2  2  2  4  4  3  3  3  4  4  3  3  3  4  4  3  3  3  4  4  3  3  3  4  4  3  3  3
  5   |  2  2  1  3  3  2  2  2  3  3  3  3  3  4  4  3  3  3  4  4  3  3  3  4  4  3  3  3  4  4  3  3  3  4  4
  6   |  1  1  2  1  1  2  3  4  3  2  3  4  4  4  3  3  4  4  4  3  3  4  4  4  3  1  2  3  2  1  1  1  1  1  1
  7   |  1  1  1  1  1  1  1  2  1  1  1  2  3  2  1  2  3  4  3  2  3  4  4  4  3  3  4  4  4  3  3  4  4  4  3
```

Row 1 weights the defensive quadrants; row 7 weights the attacking quadrants —
so the *same* skill-match counts for more or less depending on whether the
quadrant is "the player's job".

### 5.4 `_quadrantCoordinatesToBallQuadrant[240]` — fine grid → ball-quadrant (`src/tactics_data.c:115`)

15 rows × 16 columns of fine pitch coordinates, each mapped to a ball-quadrant
0..34 (note the 3-row × ~2-col banding that collapses the fine grid onto the
7×5 quadrant grid):

```
 0  0  0  5  5 10 10 15 15 20 20 25 25 30 30 30
 0  0  0  5  5 10 10 15 15 20 20 25 25 30 30 30
 0  0  0  5  5 10 10 15 15 20 20 25 25 30 30 30
 1  1  1  6  6 11 11 16 16 21 21 26 26 31 31 31
 1  1  1  6  6 11 11 16 16 21 21 26 26 31 31 31
 1  1  1  6  6 11 11 16 16 21 21 26 26 31 31 31
 2  2  2  7  7 12 12 17 17 22 22 27 27 32 32 32
 2  2  2  7  7 12 12 17 17 22 22 27 27 32 32 32
 2  2  2  7  7 12 12 17 17 22 22 27 27 32 32 32
 3  3  3  8  8 13 13 18 18 23 23 28 28 33 33 33
 3  3  3  8  8 13 13 18 18 23 23 28 28 33 33 33
 3  3  3  8  8 13 13 18 18 23 23 28 28 33 33 33
 4  4  4  9  9 14 14 19 19 24 24 29 29 34 34 34
 4  4  4  9  9 14 14 19 19 24 24 29 29 34 34 34
 4  4  4  9  9 14 14 19 19 24 24 29 29 34 34 34
```

### 5.5 `_quadrantDistanceFactor[35×35]` — distance falloff (`src/tactics_data.c:113`)

A 35×35 matrix; row *i* is the weight from the ball-position *i* to every
ball-quadrant, peaking at **0x20 (32)** on the diagonal and falling off with
distance (`0x18, 0x10, 0x08, 0x02, 0x01, 0x00`). Sample (row 0):

```
32 24 16  8  2 24 24 16  8  2 16 16 16  8  2  8  8  8  8  2  2  2  2  2  2  1  1  1  1  1  0  0  0  0  0
```

This makes a player's contribution strongest where the ball is *near* their
assigned position and negligible far away.

### 5.6 `_plPricesIncreaseTable[50]` — softens a computed rise (`src/swos_data_bulk_imports.c:196`)

```
idx:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 …
val:  0  1  2  3  4  4  5  5  6  6  7  7  7  8  8  8  9  9  9  9 10 10 … (saturates at 10)
```

When the computed value exceeds the player's stored price, the *increase* is
looked up here and added to the stored price — i.e. a player can't gain more
than +10 over their base regardless of tactic.

---

## 6. Team average — `ComputeOldPlayerSkillForViewResult` (`src/match_sim.c:744`)

Produces the `_oldPlayerSkill` the big tick/cross compares:

1. (Optional first pass over subs, scaling each sub's value ×4, GK ×1.)
2. Sum the 11 outfield players' position-values (`UpdatePlayerPositionAndReturnRole`).
   Count how many are "out of position" (value 0 with a non-GK role) into
   `oldPlayerSkillDivisor`.
3. `result = sum / 15`.
4. If a **non-keeper is in the goalkeeper slot**, `result >>= 1` (halved as a
   weakness penalty).
5. `result /= oldPlayerSkillDivisor`, store in `_oldPlayerSkill`.

So the big mark reflects the *whole eleven*, penalising out-of-position players
and an outfielder forced into goal.

---

## 7. Intuition / worked summary

- A player's value = **Σ over the pitch of (skill-fit × position-importance ×
  distance)**, normalised to 0..49.
- "Skill-fit" rewards the skills a quadrant demands (tackling at the back,
  finishing up front), weighted *quadratically* (`skillValuePonders`).
- Moving a player so their tactic sends them to areas matching their strong
  skills **raises** the value → green tick; sending them where their skills are
  wasted **lowers** it → red cross.
- The big tick/cross is the same judgement averaged over the team, with extra
  penalties for out-of-position players and a non-GK in goal.

---

## 8. Code reference

| Piece | File · symbol |
|-------|---------------|
| Sprite constants | `include/play_match_menu.h` |
| Small per-player check (editor) | `src/edit_tactics.c` · `EditTacticsAfterDrawPlayerNames` |
| Small per-player check (squad) | `src/play_match_menu.c:1175` |
| Big team check (editor) | `src/edit_tactics.c` · `SetBigCheckSprite` |
| Big team check (squad) | `src/menus.c` · `UpdateBigCheckMark` |
| Position-value dispatch | `src/match_sim.c` · `UpdatePlayerPositionAndReturnRole` |
| **Position-value formula** | `src/match_sim.c` · `GetPlayerPrice` |
| Team average | `src/match_sim.c` · `ComputeOldPlayerSkillForViewResult` |
| Weight tables | `src/tactics_data.c`, `src/swos_data_bulk_imports.c` |

*Tables decoded from the committed C data; behaviour cross-checked against the
original `SWOS2.s`.*

---

## 9. Amiga vs DOS — why the *same* swap can show different marks

> **Update (2026-06-29) — the "byte-identical to the Amiga" claims below are now
> historical.** Since this analysis, tester-supplied `.TAC` fixes were applied
> (commit `23f0c59`) to **4 Amiga formations — 4-4-2, Sweep, 4-3-3, 3-4-3** —
> which had buggy ball-position coverage (central defenders / attackers flipping
> sides instead of holding position). So this port's `442`/`Sweep`/`433`/`343`
> now **intentionally diverge** from the pristine Amiga data. The byte counts in
> §9.3 and the `docs/tactics_amiga_vs_dos/` snapshot describe the data **before**
> that fix; they remain accurate as a record of the original Amiga-vs-DOS delta,
> not of the current shipped formations. The formations now live in
> `bin/data_json/tactics/*.json` (see `docs/` data-pipeline notes).
>
> **The pristine data is still buildable.** `make SWOS_ORIGINAL_TACTICS=1`
> (after a `clean` — the flag changes baked data) selects it. The pre-fix
> formations live beside the shipped ones as
> `bin/data_json/tactics/<id>_original.json` and are baked into the same
> `src/tactics_data.c` as `_teamTactics<id>Original`, round-trip checked by
> `scripts/emit_tactics.py` like everything else. This exists because the fix
> is observable well beyond the slots it edits: `GetPlayerPrice` walks the
> tactics table for every player, and its per-quadrant "how many of our men
> cover this spot" count reads all ten outfield rows — so changing two
> central-defender rows perturbs *other* players' computed prices and, through
> `AdjustPlayerSkills`, their in-game skill nibbles. Anything measuring this
> port against original behaviour — the recorded-game replay rig especially
> (docs/TESTS.md) — has to be able to turn the fix off.

A tester noticed that on the **squad screen**, swapping two central defenders
(Brazil's **Aldair** ↔ **Ronaldão**) produces different indicators:

| Version | Aldair (moved to Ronaldão's slot) | Ronaldão (moved to Aldair's slot) |
|---------|-----------------------------------|-----------------------------------|
| **Amiga** (and this port) | ✗ red cross | *(no mark)* |
| **DOS** | ✗ red cross | ✓ green tick |

Both agree Aldair got *worse*; they disagree on whether Ronaldão got *better* or
*stayed the same*. The reason is **not** a difference in the indicator code.

### 9.1 The engine is byte-for-byte identical on both platforms

Verified against both the DOS disassembly (`swos-port/swos/swos.asm`) and the
Amiga disassembly (`SWOS-RE/Amiga/SWOS/SWOS2.s`):

- **Per-player (small) check** — DOS `SquadAfterDrawCommon` does exactly what our
  `play_match_menu.c` / `edit_tactics.c` does: `current > snapshot` → `0xD0`
  (**208** `SPR_CHECK_SMALL`, tick), `==` → no sprite, `<` → `0xD1`
  (**209** `SPR_CROSS_SMALL`, cross).
- **Team (big) check** — DOS `UpdateBigCheckMark` is identical: gated on
  `teamSkillDiffAfterChange`, then `oldPlayerSkill > newPlayerSkill` →
  `SPR_CHECK_BIG`, `<` → `SPR_CROSS_BIG`.
- **`GetPlayerPrice`** — line-for-line the same, including the **16-bit**
  intermediate arithmetic, the 16-bit running total with the `cwde`/`ext.l`
  sign-extend before `÷350`, the `−8`, the `0..49` clamp, and the
  `plPricesIncreaseTable` softening.
- **All six weight tables byte-identical** — `skillsPerQuadrant` (7×35),
  `priceSkillPonders`, `quadrantDistanceFactor`, `quadrantCoordinatesToBallQuadrant`,
  `skillValuePonders`, `plPricesIncreaseTable`.

The engine is platform-independent, so a different mark for the same swap can
*only* come from different **input data**.

### 9.2 The team files are identical — so it's the FORMATION coverage

The Brazil team file (`TEAM.082`) decompresses to byte-identical data on both
versions (the Amiga's `TEAM.*` are RNC-compressed and the DOS plain, but the
decoded bytes match), so the player **skills, price, and position bytes are
identical**. That leaves exactly one input that feeds the value *and changes when
you swap two players*: the **formation/tactic coverage** — the per-slot
35-quadrant pattern in the team's chosen formation. (A swap doesn't change the
formation's *shape*, so the crowding counts are unchanged; each swapped player's
value is just his skills evaluated against his **new slot's** coverage.)

The two platforms even store it differently:

- **DOS** embeds each formation in the executable — `tact_4_4_2 db 34h,34h,32h,…`
  (`g_tacticsTable` → `tact_4_4_2`).
- **Amiga** has it as `teamTactics442` (real `dc.b` data) in `SWOS2.s` — *not* the
  11-byte `tacticsPosition442` BSS stub that sits nearby and is easy to mistake
  for it.

### 9.3 The actual difference, proven asm-to-asm

At the time of this analysis the port reproduced the real Amiga formations
**byte-for-byte** (verified against `SWOS-RE/.../SWOS2.s` `teamTactics*` for all 12
formations — 0 differences). *(Since superseded for 4 formations — see the update
note at the top of §9.)*
Comparing those against the DOS `tact_*` tables, only **3 of the 12 formations
were re-tuned** for the DOS release; the other 9 are byte-identical:

| Formation | header diff | coverage diff |
|---|---:|---:|
| 4-4-2 | 2 | **68** |
| 4-3-3 | 1 | 10 |
| Sweep | 0 | 64 |
| *(other 9)* | 0 | 0 |

In the 4-4-2 the differences sit in the **outfield slots**, and the two
central-defender slots have their coverage **swapped** at several ball positions
(values are packed `x,y` pitch coordinates):

```
slot 1 (CD), ball-pos 4:   Amiga 0x90   DOS 0x70
slot 2 (CD), ball-pos 4:   Amiga 0x70   DOS 0x90   ← swapped vs slot 1
slot 1 (CD), ball-pos 20:  Amiga 0x73   DOS 0x33
slot 2 (CD), ball-pos 20:  Amiga 0x44   DOS 0x64
```

So the two central-defender slots send the defender to **different quadrants** in
the DOS vs the Amiga 4-4-2 — which is exactly the Aldair ↔ Ronaldão swap. (The
header bytes 4,5 — `$00,$41` Amiga vs `$20,$42` DOS — are in the `name[]` field,
skipped by `GetPlayerPrice`'s `+9`, so they don't affect the value.)

**Where the other 64 bytes are** (added 2026-09-03, from the replay-corpus
measurement in docs/TESTS.md). The 68-byte 4-4-2 delta is not mostly the CDs.
Per outfield slot:

| slot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | **8** | **9** |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| bytes differing | 0 | 2 | 2 | 1 | 1 | 0 | 1 | 1 | **31** | **29** |

Slots 8 and 9 — the two forwards — are not re-tuned but **rewritten**: the DOS
pair holds a narrow central channel (x quadrants 4-7 and 7-A) where the Amiga
pair spreads wide (2-8 and 6-C). Sweep is the same shape of
delta (64 bytes: 32 + 29 in slots 8 and 9, 3 elsewhere). Of the six single-byte
differences in slots 0-7, one matters to a real recording: **slot 4, ball
position 0 — Amiga `0x33`, DOS `0x23`** — worth +81 to a running total that was
69 short of the next `÷350` bucket, and so the whole of `zkz-10174427`'s frame-0
divergence.

Two things follow that §9's original conclusion could not see:

- The DOS release re-tuned the 4-4-2 and Sweep **forward line**, not just the
  defenders. Any "DOS formations in DOS mode" work has to carry those two rows.
- The corpus that recorded the DOS behaviour disagrees with those two rows in
  16 independent players while agreeing with DOS everywhere in slots 0-7, so
  the DOS snapshot in `docs/tactics_amiga_vs_dos/` is **not** the table that
  produced the recordings. See docs/TESTS.md, "The recording grades our
  formation data" — that section also grades the four formations this port
  edited in `23f0c59`: the 4-4-2 and 4-3-3 fixes are confirmed against the
  reference (the 4-3-3 forward fix is byte-identical to DOS), and one byte of
  the Sweep fix — rows 1/2 at ball position 15 — is contradicted by it.

The full data and a per-coordinate spreadsheet are in
`docs/tactics_amiga_vs_dos/` (`tactics_amiga.json`, `tactics_dos.json`,
`tactics_amiga_vs_dos.csv`).

**All twelve, measured** (2026-09-03, from the DOS disassembly directly —
`swos-port/swos/swos.asm` `tact_*`, whose `g_tacticsTable` order is ours;
370 bytes each, cross-checked against the snapshot above). Nine formations are
byte-identical, and **the live difference is 21 bytes**:

| formation | name bytes | live coverage bytes | where | dead |
|---|---:|---:|---|---:|
| 4-3-3 | 1 | **10** | slots 1,2,5,6,7,8 | — |
| 4-4-2 | 2 | **8** | slots 1,2,3,4,6,7 | 60 |
| Sweep | 0 | **3** | slots 1,2,4 | 61 |
| *the other nine* | 0 | 0 | — | — |

> **Retracted (2026-09-03).** An earlier revision of this table read 68 / 64 /
> 10 and concluded that "the DOS release rewrote the 4-4-2's and Sweep's forward
> line — 121 of the 142 bytes". **That was wrong.** Those 121 bytes are in slots
> 8 and 9, and **neither release ever executes them**: `InitUserTactics` copies
> the 5-3-2's last two player rows (70 bytes at `+$118`) over the 4-4-2's *and*
> the Sweep's at startup, on both platforms —
> `data/SWOS2.s:43757` and `swos-port/swos/swos.asm:22569`, the same routine,
> the same 70 bytes. The Amiga's baked rows already equal the 5-3-2's so the
> copy is a no-op there and the difference never showed; the DOS binary's are
> different, and dead. The corpus said so all along (fifteen recorded forwards
> matching the Amiga and none matching the DOS dseg) and it was read as evidence
> that no table we hold is the one the recordings ran. It is: with the copy
> applied and the DOS coverage selected, **all 384 recorded player values
> reproduce**. See docs/TESTS.md, "The rows that were never executed".

The eleven-byte tail (`pair_ids` + `ballOutOfPlayTactics`) is identical in all
twelve.

The two name-byte differences are cosmetic. The 9-byte `name[]` is a label, a
NUL, then a second short string the tactics menu shows as a grade: DOS grades
4-4-2 `" B"` where the Amiga says `"\0A"` (no space — an original-data slip),
and 4-3-3 `" B"` where the Amiga says `" C"`. `GetPlayerPrice` skips all nine
bytes (`+9`), so they change nothing but the display.

### 9.4 Why that flips the mark — and why it's asymmetric

Swapping Aldair ↔ Ronaldão lands each on a slot whose coverage *differs between
the two versions* → `GetPlayerPrice` yields a different value → the tick appears
on DOS but not Amiga.

Why asymmetric (Aldair ✗ on both, Ronaldão only ticks on DOS)? The end of
`GetPlayerPrice` is non-linear around the player's stored price:

- computed index **below** the stored price → returned raw, so a drop shows fully
  (**Aldair → ✗**);
- **at/above** the stored price → the gain is compressed via
  `plPricesIncreaseTable` (saturates at +10), and the index is coarsely quantised
  (`÷350`, `0..49`).

Ronaldão (price 36, right at the threshold) has his rise flattened to "no change"
under the Amiga coverage but it survives one bucket under the DOS coverage. The
mark is hair-trigger *because* the value is so coarsely bucketed — and the
coverage difference is what tips it.

### 9.5 Conclusion

The differing data is the **default formation/tactic coverage tables**
(`teamTactics442`, `teamTactics433`, `teamTacticsSweep`) — re-tuned for the DOS
release — **not** the engine and **not** the team files. It is **not a bug** in
this port: our formation data is **byte-identical to the real Amiga** (verified,
all 12 formations) and the engine is verified-identical, so we reproduce the
**Amiga** result (Ronaldão: no mark). DOS shows a tick because its 4-4-2 sends
those two defenders to different quadrants.

> Investigation note (and a few traps for next time): this took several wrong
> turns — first guessing the *ratings* differed, then the *price*, then
> mis-reading the Amiga formation as the 11-byte `tacticsPosition442` BSS stub
> (the real data is `teamTactics442`), then a regex that dropped the **signed**
> `dc.b` minus signs (a negative like `-112` is byte `0x90`), which briefly made
> the port look DOS-derived. Settled result: engine + team files identical; this
> port == the real Amiga byte-for-byte; DOS re-tuned 3 formations (4-4-2 =
> 2 header + 68 coverage bytes, 4-3-3 = 10, Sweep = 64).

### 9.6 Building the DOS formations — `SWOS_DOS_TACTICS`

```bash
make clean && make SWOS_DOS_TACTICS=1        # the DOS coverage
make clean && make SWOS_ORIGINAL_TACTICS=1   # the pristine Amiga coverage
make clean && make                           # what ships (Amiga + the fixes)
```

Baked from `bin/data_json/tactics/<id>_dos.json` by `scripts/emit_tactics.py`,
which round-trip checks them like everything else, and selected in
`_teamTacticsTable` — a rebuild, not a file swap, and the two flags are mutually
exclusive (`#error`). Only four variants exist because only four formations need
one: 4-4-2, 4-3-3 and Sweep because DOS changed them, and 3-4-3 because *this
port* did (the DOS 3-4-3 is the pristine Amiga one).

The `_dos.json` files carry what the DOS **binary** holds, dead rows included;
`InitUserTactics` overwrites slots 8 and 9 at startup exactly as the real game
does, so the baked data stays a faithful image and the runtime behaviour is
correct. (That routine writes through `_teamTacticsTable`, not the bare
`_teamTactics442` / `_teamTacticsSweep` symbols — otherwise the variant builds
never receive the copy, which is what made this flag look wrong for a while.)

**With it on, the corpus's own recorded player values score 384/384** — every
outfielder in every decodable recording, including the two the shipped Amiga
data gets wrong (Eranio, Ayala). That is as direct a confirmation as the corpus
can give that these are the formations the recordings were made with.

It is still **not** wired to `--dos-gameplay=on`: that is a gameplay decision
(it changes squad-screen marks and player values in menus, not just in the
match), not a data question any more.
