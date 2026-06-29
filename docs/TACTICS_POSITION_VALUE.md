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
