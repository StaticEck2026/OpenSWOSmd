# Crowd animation

How the fans in the stands move: what makes them stand up, sit down, cheer or
sulk, and what changes the speed.

Everything is one routine — `DrawAnimatedPatterns()` in
`src/animated_patterns.c:379` (ported from `SWOS2.s:53273`), called once per
frame from the game loop (`src/game_loop.c:704`), after the sprites are
positioned and before `DrawSprites`. There is no crowd entity, no crowd
sprites and no crowd actor list: **the crowd is pitch tiles**.

The audio crowd (chants, the goal roar, the "oooh") is a separate system that
shares only two counters — see §8.

---

## 1. What a "fan animation" physically is

The pitch is a 42x55 grid of 16x16 tiles whose pointers live in
`pitchDatBuffer` (2310 slots x 4 bytes). Fan tiles come in **consecutive
pairs**:

| tile index | meaning |
|---|---|
| odd  | fans sitting (static) |
| even | fans standing / jumping (animated) |

Only indices 1..22 are fan tiles (`MAX_FAN_TILE_INDEX`, `include/display.h:583`);
anything else at the chosen cell is skipped.

Animating a cell is therefore just **adding or subtracting one `TILE_SIZE`**
from that cell's pointer slot (`animated_patterns.c:648-664`). That changes what
the ordinary pitch renderer draws from then on. To make the change visible in
the *current* frame the routine also spot-blits the new tile through
`drawPatternFromIndex()` (`:126`), which hijacks **sprite 137** as a shim: point
its `sprData` at the pitch tile, then draw it at
`(tileCol*16 - cameraX, tileRow*16 - cameraY)`.

It draws **twice** (`:199-208`): once into the frontbuffer, once into the
mask/background buffer, so the change survives the buffer swap and sprite
erasure.

> SDL note: the 4-byte `pitchDatBuffer` slots cannot hold a 64-bit address, so
> every toggle is mirrored into `s_pitchFullPtrs[]` (the pitch.c side table the
> renderer actually reads). Writer and reader must stay in step or the crowd
> looks frozen. A rollback restores only the slots, hence
> `AnimatedPatterns_SyncTileMirror()` (`:291`).

## 2. The per-frame pump: 8 slots, two knobs

`_animatedPatterns[8]` is a ring of 8 work slots (`0xFFFF` = free). Each frame
the loop walks all 8 (`:555`):

- **slot occupied** -> redraw that cell once more, then free the slot. (That
  second draw is the other of the two display buffers.)
- **slot free** -> if this frame's budget allows, pick a random fan cell and
  toggle it.

Every game-state input is expressed through exactly two values that the state
machine sets before that loop runs:

| knob | meaning |
|---|---|
| `d6` | **how many** of the 8 slots may be refilled this frame -> the animation *rate* |
| `d0` -> `_patternStatic` | **which way** to toggle: non-zero = make fans stand, 0 = make them sit -> the *phase* |

`d0` is nearly always a square wave off `currentGameTick` (incremented once per
VBlank, `src/interrupts.c:272`), so the crowd runs an up/down wave whose duty
cycle and period are chosen per state.

Note the name `_patternStatic` is the original asm symbol and reads backwards:
non-zero means *animate*.

## 3. The state machine

Selection order, live play (`:379-486`):

1. `trainingGame` -> return; nothing in the ground moves.
2. Tick the goal-celebration clock: `patternsGoalCounter` free-runs 1->299 and
   wraps to 0. It is armed to 1 when a goal fires (`GoalScored`,
   `src/game_timers.c:614`, alongside `goalCounter`).
3. Decrement `showFansCounter` if non-zero.
4. Clear `otherTeamScored`, then pick a state:

| # | state (label) | trigger | `d6` | `d0` (animate phase) | effect |
|---|---|---|---|---|---|
| 6 | `game_hasnt_started` `:527` | `showFansCounter != 0` | 8 | `tick&31 > 15` -> 50% | full-crowd 50/50 wave, 8 cells/frame — the loudest idle |
| 7 | `game_in_progress` `:535` | `gameStatePl == ST_GAME_IN_PROGRESS`, or `gameState == ST_KEEPER_HOLDS_BALL` | 1 | `tick&31 > 25` -> 6/32 | one cell per frame, mostly sitting: background fidgeting |
| 1 | `ball_out_of_play` `:490` | no celebration running | 5 | `tick&31 > 25` -> 6/32 | same phase, 5x the rate — crowd murmurs during stoppages |
| 2 | `greatest_cheers` `:498` | `patternsGoalCounter < 25` | 8 | always 1 | everything picked stands up and stays up |
| 3 | `middle_cheers` `:504` | `< 200` | 8 | `tick&15 > 4` -> 11/16, **16-tick period** | fastest cycle, mostly standing — sustained celebration |
| 5 | `cheers_fading` `:521` | `< 225` | 8 | always 0 | the ground sits back down, fast |
| 4 | `other_team_scored_be_still` `:512` | scoring team is not the stand on camera | 1 | `tick&31 > 15` -> 50% | 1 cell/frame, and sets `otherTeamScored = 1` |

Past 225 the celebration falls through into state 1 (`ball_out_of_play`) until
the counter wraps at 300.

So the two ways the game changes the crowd's **speed** are the budget (1 / 5 /
8 cells per frame) and the wave period (32 ticks everywhere except
`middle_cheers`, which halves it to 16).

### 3.1 Whose fans are cheering

Decided at `:430-448`:

- `cameraY > 440` -> the **bottom** stand is on screen, else the **top** one.
- `fansSidesSwapped` flips that mapping. It is set from the parity of the top
  team's name in `src/pitch.c:534-541`, and it is the same flag that decides
  which stand wears which kit colours.
- If `teamScoredGamePtr` matches the stand on camera you get the cheer phases
  (states 2/3/5); if not, state 4.

In state 4 the top stand's home block (random index 21..41) additionally has its
toggle forced to "sit" (`:598`) — the scoring team's own fans do not
celebrate in the away end.

### 3.2 `showFansCounter` — the pre-kick-off cutaway

Set to 100 at match start for non-training games (`src/match_setup.c:976`),
decremented once per frame here, cleared when `ST_STARTING_GAME` is dismissed
(`src/game_timers.c:170`). While it is non-zero it is also the **highest
priority camera state**: `MoveCamera` returns immediately (`src/move_camera.c:83`),
which is what holds the camera on the stands while the crowd does its 50/50
wave. `CrowdResetMatchState()` (`src/sfx_crowd.c:49`) zeroes it along with the
goal counters.

## 4. Which cells get picked

- `animPatternsIndex` walks 0..63 through `_animPatternsRandomTable`, a
  permutation of 0..62 that gets **two entries swapped on every pick**
  (`swapTwoRandomIndicesFromAnimPatternsTable`, `:104` — two `Rand()` calls). The
  63 visible fan cells are therefore visited roughly evenly, but never in a
  repeating order.

  > This table must be the real permutation from the Amiga port's `Data.c:2221`.
  > With the zero stub it once shipped as, every pick was index 0: one tile
  > "animated" forever and the rest never moved.

- **Camera Y chooses the band** (`:592-609`):

  | `cameraY` | band | sprite base |
  |---|---|---|
  | `< 64` | top stand | 21 |
  | `>= 544` | bottom stand | 1071 |
  | otherwise | no crowd on screen | *skip entirely* |

  Mid-pitch, nothing animates at all — which is most of a normal match.

- The picked index is decomposed into a 21-column window and offset by the
  camera in tiles: `pitchIdx = row*42 + col + (Sim_DrawCameraX() >> 4)` (`:619`).
  The window scrolls with the camera.
- If the tile at that cell is not in 1..22, or its parity already matches the
  wanted state, the slot is left free and retried next frame.

### 4.1 Widescreen

The base loop only ever touches a 21-column window (the 4:3 view width), so on a
wide view the right half would never animate. `drawFanMirror()` (`:328`) mirrors
each touched cell into the `+21` column of the **same row** and toggles it with
the same `d4`, so both halves move in lock-step without needing their own slot.
It is bounds- and parity-checked without `PanicHaltLoop` — the mirror can legally
land off the pitch or on a non-fan tile.

It keys off `_sim_draw_width`, not the display width: which tile toggles decides
whether a slot fills, and so decides every subsequent `Rand()` call of the match.

## 5. Reset

`ResetAnimatedPatternsForBothTeams()` (`:267`) sweeps the two 126-entry fan
strips (grid rows 1-3 at slot 42, rows 51-53 at slot 2142), forces every even
tile back to odd, and clears the 8 slots — i.e. sits the whole ground down at
once. Called from:

- `src/game_timers.c:395` — after a goal restart, when the goal camera was active
- `src/highlight.c:764,1002` — replay start and end

`InitAnimatedPatterns()` (`src/match_setup.c:326`) just fills the 8 slots with
`0xFFFF` at match setup.

## 6. Replays

During a replay (`replayState != 0`) the routine derives nothing — it reads
`animPatternsState` (1..7) out of the recording and jumps straight to that
label (`:466-476`). The state word is written per frame into `.HIL`
(`src/highlight.c:711`) and into the `.SRP` frame head
(`src/replay_v2.c:474`), which is one reason the frame head must be taken
*with* the sprites: `DrawAnimatedPatterns` sets the crowd state for the frame it
is about to draw (docs/REPLAY_FORMAT_V2.md).

An out-of-range state word panics on Amiga (`panicReasonCode = 0x5665`); on SDL
it is treated as "no pattern update", because at match start the replay buffer
has not been populated yet.

## 7. Rollback / netplay

`animPatternsState`, `animPatternsIndex`, `patternStatic`,
`patternsGoalCounter`, `showFansCounter` and `otherTeamScored` are all in the
**SIM** tier of the rollback registry (`src/sim_state.c:541-544`) — the cell
picks consume `Rand()`, so the crowd is part of the simulation, not decoration.
After a load, `AnimatedPatterns_SyncTileMirror()` brings `s_pitchFullPtrs[]`
back in line with the restored pitch slots.

## 8. The audio crowd is a separate system

`src/sfx_crowd.c` shares only the two counters:

- `PlayCrowdChants()` is an independent 4-state machine — idle -> fade-in (128
  frames) -> cooldown (`Rand()*2 + 500`) -> fade-out (128) — picking one of four
  chant samples on channel 2.
- `PlayEnqueuedSamples()` fires the post-goal roar when `goalCounter` reaches
  zero. That counter is set to 100 / 200 / 300 plus `Rand()` depending on how
  dramatic the goal was — equaliser, go-ahead, two-goal cushion
  (`src/game_timers.c:592-613`).

So a goal's **visual** celebration is a fixed 300 frames while its **audible**
length varies with the scoreline.

## 9. Other tiers

- **Arcade pitches** have no fan tiles. The state machine still runs (the
  counters must keep ticking), but the toggle/draw is skipped — it would corrupt
  the diced BMP tiles (`:552`).
- **Fan recolouring** to the playing kits happens once at pitch setup
  (`src/pitch.c` steps 3-4), not here; that sweep is per crowd row, deliberately
  not per half, or it repaints the advertising hoardings (docs/DOS_GRAPHICS.md).
