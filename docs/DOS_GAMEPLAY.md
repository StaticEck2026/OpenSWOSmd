# DOS gameplay (70 FPS) support

The Amiga SWOS runs its match simulation at **50 FPS** (PAL VBlank); the DOS
version runs at **70 FPS**. SWOS is **frame-locked** — exactly one logic step per
rendered frame, no `deltaTime` multiplier — so to make the two versions *feel* the
same despite different frame rates, a set of physics/timer constants are
**pre-scaled per frame rate**. This doc captures which constants change and how,
so the SDL port can offer a DOS-gameplay mode (the reserved `gDosGameplay` flag /
`[dos] gameplay=true`).

Source of truth: **swos-2020** (`/mnt/c/Projects/GitHub/swos-2020`), the most
up-to-date reference, which patches every FPS-dependent constant at startup in
`src/options.cpp::setGameStyleVariables()`. (An older spreadsheet exists at
`D:\Projects\SWOS\DOS\2_code comparison table of 70-50-60FPS
version-r1.6.xlsx`, superseded by the source.)

## 1. Game-style / frame-rate selection

swos-2020 selects everything from one integer `m_gameStyle` (`options.cpp:89`,
default 0), set via `--game-style=N` / ini `gameStyle`:

| style | name | FPS | notes |
|---|---|---|---|
| 0 | **DOS** | **70** | native DOS tuning |
| 1 | **Amiga** | **50** | native Amiga tuning |
| 2 | Moon Soccer | 50 | Amiga physics, low gravity novelty |
| 3 | Amiga | 60 | experimental |
| 4 | XBOX | 60 | XBLA tuning (DOS keeper dist + strong keeper AI) |

Only **0 (DOS/70)** and **1 (Amiga/50)** matter for this port; 2/3/4 are bonus
reference points. The style→FPS map (`render.cpp::getTargerFPS()` 4620-4640,
`game.cpp:8927`) is `0→70, 1→50, 2→50, 3→60, 4→60`.

**For our port:** `gDosGameplay` (already in `dos_mode.h`, currently reserved)
selects DOS(70) vs Amiga(50). Note it is **independent of `gDosGraphics`** — you
can run DOS gameplay with Amiga graphics or vice-versa. Recommend a single
internal "game style" enum so 60 FPS modes can be added later.

## 2. Frame pacing (the game loop)

- DOS native tick was a PIT countdown of **17045** = "approximately 70 times a
  second" (`swos.asm:11705, 15522`). Amiga = 50 Hz VBlank.
- swos-2020 paces with a high-res busy-wait/`SDL_Delay`: delay =
  `perfFreq * factor / kTargetFps`, `kTargetFps` from the style
  (`render.cpp::frameDelay()` 4719-4806); menus fall back to 30 FPS.
- **Our port** drives the tick from `tick_timer_cb` / `_RunInt3HandlerThrottled`
  in `platform_sdl.c`, currently hard-coded to ~50 Hz (20 ms; throttle `< 18`).
  DOS gameplay needs this retuned to **70 Hz (~14.29 ms; throttle ~12 ms)** when
  the DOS style is active. The multiplayer frame budget per mode (from
  swos-2020 `issue-list.txt`): `1000/50 = 20 ms` (Amiga), `1000/70 ≈ 14.29 ms`
  (DOS), `1000/60 ≈ 16.67 ms` (XBOX).

## 3. The per-FPS constant table (load-bearing)

All set in `src/options.cpp::setGameStyleVariables()` (lines ~1563-1843). Apply
these to the matching SWOS globals in our tree (they exist as data from
`SWOS2.s`/`Data.c`) in an equivalent `ApplyGameStyle()` called at match init.
Columns: **DOS 70 / Amiga 50** (60-FPS shown where relevant).

### Ball physics
| Constant | DOS 70 | Amiga 50 | 60 FPS | Meaning / consumer site |
|---|---|---|---|---|
| `ballGroundConstant` | **13** | **16** | 16 | Per-frame speed lost to ground friction; `UpdateBall: speed -= ballGroundConstant` (`swos.asm:110451`). Higher at 50 FPS to lose the same speed over fewer frames. |
| `ballAirConstant` | **4** | **10** | 10 | Per-frame speed lost while airborne (z>0) (`swos.asm:110467`). |
| `gravityConstant` (int32) | **3291** | **4608** | 3840 | Per-frame downward Z-velocity decrement; `UpdateBall: D3 -= gravityConstant` (`swos.asm:110579`). (Moon=2560.) |
| `pitchBallSpeedInfluence[7]` | **-3, 4, 1**, 0,0,-1,-1 | **-2, 2, 3**, 0,0,-1,-1 | (=Amiga) | Per-pitch-type ball-friction modifier added to `ballGroundConstant` (`swos.asm:110463`). Only the first 3 entries differ. |

### Goalkeeper
| Constant | DOS 70 | Amiga 50 | Meaning |
|---|---|---|---|
| `keeperSaveDistance` | **16** | **24** | Dive/save distance threshold (`ShouldGoalkeeperDive`, `swos.asm:106705`). |
| `dseg_11061B[8]` | `0x28000…0x60000` | `0x30000…0x68000` (or `0x28000…0x60000` "stronger AI") | Keeper reach/dive distance table, difficulty-interpolated (`game.cpp::KeeperAI` 484-519; consumer `swos.asm:106773`). DOS reacts at shorter range. |

### Timers / match-clock thresholds (all int16 `word` compares)
| Constant | DOS 70 | Amiga 50 | 60 FPS | Gates (asm site) |
|---|---|---|---|---|
| `GS_54_49` | **54**¹ | **49** | 59 | **Frames per in-game second** — clock accumulator reload (`swos.asm:97826`). The central clock-rate constant (see §4). |
| `GS_55_50` | 55 | 50 | 60 | `endGameCounter` reload, last-minute switch (`swos.asm:98021`). |
| `GS_27_22` | 27 | 22 | 27 | Ball defensive-Z threshold for goalie catch (`swos.asm:114226`). |
| `GS_110_100` / `GS_PT_110_100` | 110 | 100 | 120 | Penalty timer → `NextPenalty` (`swos.asm:101431`). |
| `GS_385_350` | 385 | 350 | 420 | Halftime-result stoppage timer (`swos.asm:118468`). |
| `GS_550_500` | 550 | 500 | 600 | `ballOutOfGameTimer` (`swos.asm:101968`). |
| `GS_660_600` | 660 | 600 | 720 | Auto-advance final/halftime result (`swos.asm:118450`). |
| `GS_825_750` | 825 | 750 | 900 | Kickoff stoppage → `PrepareForInitialKick` (`swos.asm:101465`). |
| `GS_GK_55_50` | 55 | 50 | 60 | `stoppageEventTimer` reload, ball still (`swos.asm:101710`). |
| `GS_PT_55_50` | 50² | 50 | 60 | Penalty keeper-still-down compare (`swos.asm:113363`). |
| `GS_SF_128__128` / `GS_SF_8000h__8000h` | 128 / 0x8000 | -128 / -0x8000 | — | Sign-fix constants (negative for styles 1-3). |

¹ **Decision: default to the authentic original DOS value `54`, but make it a
tweakable** so it can be flipped to `69` at runtime. swos-2020 ships
`GS_54_49 = 69; // original = 54` (`options.cpp:1581`) — a deliberate retune of the
DOS clock cadence; we keep `54` as the default for authenticity and expose `69` as
an option. When implemented, the code comment must record both values (original
DOS = 54, swos-2020 = 69). The Amiga value is the clean **49**.
² `GS_PT_55_50` DOS value happens to equal the Amiga value (50).

### NOT differentiated (important)
- **Kick / pass / shot power** are the **same** for DOS and Amiga in shipping
  swos-2020 (`setDeltaZVariables`, `options.cpp:1490-1524`): `ballKickingSpeed=2208,
  highKickBallSpeed=2688, normalKickBallSpeed=2560`, banded `passingSpeed*`
  1536→2218. Only styles ≥3 (60 FPS) apply a ×1.2 boost. Do **not** rescale
  kick/pass power between 50 and 70.

## 4. Match clock

Frame-driven accumulator, **same algorithm** both modes, differing only by
`GS_54_49`:

- Per in-progress frame: `secondsSwitchAccumulator -= timeDelta`; on underflow →
  `secondsSwitchAccumulator += GS_54_49; gameSeconds++` (`swos.asm:97793-97830`).
  So `GS_54_49` ≈ frames-per-game-second: 49 @ 50 FPS, **54 @ 70 FPS** (our
  default = authentic original DOS; tweakable to swos-2020's `69`, see §3 note ¹).
- `timeDelta` comes from `gameLenSecondsTable dw 30,18,12,9` indexed by the chosen
  match length (`swos.asm:96458`) — **FPS-independent**; the FPS dependence is
  entirely in the `GS_54_49` reload.
- On-screen / record time reconstructs real seconds as `frames / fps`
  (`getFrameTimerString`, `game.cpp:2146`; `fps_rate = 1/{70,50,60}`,
  `game.cpp:8927`), so elapsed time is correct per mode.

Net: match length in minutes is identical; the 70 FPS mode just spends
proportionally more real frames per game-minute.

## 5. Scaling helpers

- **No `SCALE_FOR_FPS()` macro.** The only FPS-aware scaler is
  `setFixedPoint()` (`util/util.cpp:485-495`): identity for DOS/Amiga, ×(50/60)
  for the 60-FPS styles. It feeds ball-Z deltas — but that path
  (`setDeltaZVariables`) is **disabled** (`return;` at `options.cpp:1469`), so
  those deltas stay at asm defaults. Treat as "intended but inert."
- The 60-FPS `GS_*` values are hand-computed (×1.2), not macro-derived.
- `KeeperAI::set()` (`game.cpp:503-519`) linearly interpolates `dseg_11061B[]`
  between DOS and Amiga base arrays by difficulty — the clearest DOS-vs-Amiga
  table in the tree.

## 6. Explicitly NOT FPS-scaled (gaps / gotchas)

swos-2020 did **not** compensate these for frame rate — they run one step per
frame, so they are effectively faster in wall-clock terms at 70 FPS *by design*:

- **Player movement speed & acceleration** — `playerSpeedsGameInProgress`,
  `kPlayerSpeed=14`, `playerGroundConstant` (debug-tunable only). Not touched by
  `setGameStyleVariables`.
- **Animation step rates** — `Sprite.frameDelay` counts down once per frame; no
  `GS_*` modifies it, so anims cycle faster at higher FPS.
- **Camera speed/acceleration** — no FPS-scaled camera-speed constant exists in
  swos-2020. (Our `move_camera.c` `MAX_CAMERA_MOVEMENT`/velocity caps are
  per-frame; if DOS-70 camera feels too fast, this is unreferenced territory —
  decide empirically.)
- **Injury/foul timers** — no distinct FPS-scaled constants; they ride the
  `GS_PT_*` / stoppage `GS_*` timers above.

## 7. Porting plan (for this SDL tree)

1. **Game-style state**: drive off `gDosGameplay` (→ DOS 70 vs Amiga 50). Keep an
   internal enum so 60 FPS can be added.
2. **Pacing**: make the `platform_sdl.c` tick rate (currently 50 Hz / 20 ms /
   throttle 18) a function of the active style — 70 Hz / ~14.29 ms / throttle ~12
   for DOS.
3. **`ApplyGameStyle()`**: at match init, write the §3 constants into the
   corresponding SWOS globals in our tree (locate `ballGroundConstant`,
   `ballAirConstant`, `gravityConstant`, `pitchBallSpeedInfluence`,
   `keeperSaveDistance`, the `dseg_11061B`/keeper table, and the `GS_*` timer
   globals — they come from `Data.c`/`SWOS2.s`). Our data defaults are the
   **Amiga** values (the original SWOS2.s the tree was reversed from); apply the
   DOS column when DOS gameplay is selected, restore Amiga otherwise.
4. **Clock**: confirm our match-clock accumulator uses the `GS_54_49` reload (it
   should — same code path); just make sure the constant is set per style.
5. **Leave** player speed, animation, camera, and kick/pass power **unscaled** —
   match swos-2020.
6. **Attract/replay note**: `.HIL`/`.RPL` reels are frame-rate-specific — a 70 FPS
   reel plays back wrong at 50 FPS and vice-versa (see `docs/ARCADE.md`). Tag/keep
   them by mode.

## Authoritative swos-2020 files
- `src/options.cpp` — `setGameStyleVariables()` (1563-1843, THE table),
  `setDeltaZVariables()` (1467-1525), `--game-style` parse (619-634).
- `src/util/util.cpp:485-495` — `setFixedPoint()`.
- `src/game.cpp` — clock (`UpdateTime` ~15101), `getFrameTimerString` (2122-2195),
  `KeeperAI` (482-522), style enum (474-481), `fps_rate` (8927).
- `src/video/render.cpp` — pacing (`getTargerFPS` 4620, `frameDelay` 4719).
- `swos/swos.asm` — DOS defaults + consumer sites; `GS_*` block ~250950, physics
  ~203742. `swos/symbols.txt` — per-symbol C types.
- `issue-list.txt` — multiplayer frame-time + per-mode notes.

## Open questions
- **`GS_54_49` — DECIDED:** default `54` (authentic original DOS), exposed as a
  runtime tweak so it can be flipped to swos-2020's `69`. Keep both values in the
  code comment. (See §3 note ¹.)
- Do we want the 60-FPS styles at all, or just DOS-70/Amiga-50?
- Does our match-clock path already key off `GS_54_49`, or is the cadence
  hard-coded somewhere that needs the same treatment?
