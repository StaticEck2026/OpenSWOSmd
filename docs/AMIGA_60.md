# Amiga 60 Hz gameplay mode — design (implement later)

**Status: DESIGN ONLY — not implemented** (re-checked 2026-09-30: no `amiga60` style, no
`GameStyle_LogicHz`, no interpolation in the tree; only §9a's sub-pixel scroll shipped).
This captures the agreed approach so it can be built later. It is the sibling of `docs/DOS_GAMEPLAY.md` (read §8 there first —
especially §8.3/§8.4, which this doc extends).

## 1. Goal

Let the Amiga game run smoothly on a **60 Hz monitor** *without changing how it plays*.
The match must stay **bit-identical to the authentic 50 Hz Amiga** gameplay — same
physics, same tackles/bounces/keeper timings, same RNG outcomes — while the picture
updates at 60 Hz so motion looks smooth on modern displays.

This is a **rendering/refresh** feature, NOT a gameplay-tuning feature. Contrast with
the DOS mode, which deliberately changes gameplay (70 Hz + DOS physics + the 41/64
speed_factor).

## 2. Why the `speed_factor` approach is NOT good enough here

swos-2020's high-FPS approach (and the DOS mode we shipped) runs the **logic at the
higher rate** and scales the per-frame movement deltas by `50/fps` (`× 50/60 = 0.833`
for 60 fps) so distance-per-second stays roughly constant (`docs/DOS_GAMEPLAY.md`
§8.1/§8.3, the `CalculateDeltaXAndY_125` hook).

That is fine for a *different-feel* mode (DOS), but it is **NOT bit-exact** and so
fails the "authentic Amiga" bar:

- Each frame's delta is fixed-point and gets **truncated** (`* 50 / 60` integer
  division). Over many frames the discrete positions **drift off** the true 50 Hz
  path. Players arrive at slightly different pixels; a tackle/interception/keeper
  reach that connected at 50 Hz can miss (or vice-versa). RNG-independent, but the
  *positions* diverge.
- 60 logic steps/sec also means physics integrations, animation counters, timers and
  AI decisions run at a different cadence than the 50 Hz reference — subtle behaviour
  changes even where positions match.

The tester put it exactly right: *"For Amiga gameplay in 60 FPS this approach does not
work. Needs decoupling, otherwise the gameplay will never be the exact same."*

## 3. The approach: temporal decoupling (fixed 50 Hz logic + interpolated render)

Standard "fix your timestep" pattern (Gaffer on Games, *Fix Your Timestep!*):

- Run the **game logic at a fixed 50 Hz timestep** (20 ms) — byte-identical to the
  Amiga. Nothing about the sim changes.
- **Render at the monitor's refresh** (60 Hz, or any rate) by drawing every moving
  thing at an **interpolated** position between the two most recent logic frames.

```
DT = 1/50 s                     // fixed logic step (20 ms)
accumulator += realFrameDelta   // wall-clock since last render
while (accumulator >= DT) {
    snapshot_prev()             // prev := curr for every interpolated entity
    GameLoop_Logic()            // advance the sim ONE 50 Hz step (updates curr)
    accumulator -= DT
}
alpha = accumulator / DT        // 0..1, how far between prev and curr we are now
Render(alpha)                   // draw at prev + (curr-prev)*alpha, then present (vsync)
```

Result: **exact 50 Hz gameplay + smooth 60 Hz (or 120/144/…) visuals.** No physics
retuning, no speed_factor, no drift. This is our own approach — swos-2020 did not
cleanly solve it (its `g_SpeedFactor` path is abandoned/commented; §8.3).

Note: at a `realFrameDelta` slightly under `DT` some renders run the loop 0 times
(spare) and some run it 2× (catch-up); the accumulator absorbs this. `alpha` makes
motion smooth regardless.

## 4. What our match loop looks like today, and the split required

`GameLoop()` (`src/game_loop.c`) currently does, per frame: advance logic (physics/
AI/state) → `MoveCamera()` → `ScrollToCurrent()` → `DrawSprites()` → `waitVBlankSync()`
→ `Platform_VBlank()` (present + pace to `DosGameplay_TargetFps()`, `platform_sdl.c`).
It is **frame-locked**: one logic step *is* one drawn+presented frame.

> **Update (2026-09-30):** netplay has since split the loop body out, though not along
> this doc's line: `GameLoop_Frame()` is one whole frame (logic AND drawing — the render
> calls write match state: `DrawSprites` sets `onScreen`, `DrawAnimatedPatterns` calls
> `Rand`), and `GameLoop_EndFrame(present)` is the vblank wait / swap / present, with
> `present = 0` for a frame GGPO re-simulates (`src/netplay_match.c`). So a
> `GameLoop_Logic()` with "no drawing" is harder than §4 assumes: those render-side state
> writes would have to move into the logic half, or the draw run once per logic step, and
> both the replay rig and `scripts/netplay_test.sh --all` must stay bit-identical.

Decoupling requires splitting that into two callable halves:

- **`GameLoop_Logic()`** — everything that *mutates sim state*: physics, AI, ball,
  player movement (`MovePlayer`), `MoveCamera()` (updates `_cameraX/_cameraY`), game
  state, timers, RNG. **No drawing, no present.**
- **`Render(alpha)`** — `ScrollToCurrent()` + `DrawSprites()` + HUD, but reading
  **interpolated** positions, then present. Runs at monitor rate.

## 5. What to interpolate (and what NOT to)

| Entity | Field(s) | Interpolate? |
|---|---|---|
| Players / keepers (`Sprite`) | `x`, `y` | **Yes** (position) |
| Ball | `x`, `y`, `z` | **Yes** (z too → smooth height + shadow) |
| Camera / pitch scroll | `_cameraX`, `_cameraY` | **Yes** (else the pitch judders even if sprites don't) |
| Sprite **animation frame** | `frameIndex` / `frameDelay` | **NO** — discrete; use `curr` (interpolating frames = ghosting) |
| Direction / facing | `direction` | **NO** — use `curr` |
| HUD / score / clock | — | **NO** — static within a logic step |

Only **continuous positions** interpolate. Everything discrete uses the latest (`curr`)
logic value.

Mechanism: each interpolated entity needs a **`prev` snapshot** taken *before* each
logic step. Options — decide at build time:
1. Add `prevX/prevY` (and ball `prevZ`, `prev_cameraX/Y`) fields, snapshot in
   `snapshot_prev()`.
2. Keep a parallel prev-array keyed by sprite index.

At render, either (a) write the interpolated value into a dedicated **render-position**
field that `DrawSprites`/`ScrollToCurrent` read, or (b) temporarily overwrite live
`x/y` with the interpolated value for the draw and restore `curr` afterward. **(a) is
cleaner** (no save/restore risk); it means the sprite blit + the camera scroll consult
a render position, not the sim position. This is the main implementation cost.

## 6. Game-style integration

Amiga-60 is a `_gGameStyle` value (see the enum plan below). Its tuning:

- **Physics constants = Amiga** (identical to Amiga-50; `ApplyGameStyle()` writes the
  Amiga column). No DOS ball/keeper values.
- **Logic rate = 50 Hz** (NOT 60). The 60 is the *present/refresh* rate only.
- **speed_factor = 1/1** — **no delta scaling.** (This is the whole point: the logic is
  true 50 Hz, so `CalculateDeltaXAndY` must NOT be scaled; interpolation, not scaling,
  provides the smoothness. `_gMoveDeltaScaleNum/Den` stay 1/1 for Amiga-60.)
- Pacing model changes: the existing `DosGameplay_TargetFps()` conflates "logic rate"
  and "present rate" (fine when they're equal). Amiga-60 needs them **separate**: a
  logic rate (50) driving the accumulator, and a present rate (monitor / vsync). Add a
  `GameStyle_LogicHz()` (50 for Amiga-50/60, 70 for DOS) distinct from the present rate.

## 7. `_gGameStyle` enum + ini (shared with the DOS mode)

Today it's just `_gDosGameplay` (bool: DOS-70 vs Amiga-50), set by
`[gameplay] style = amiga | dos` (`src/swos_settings.c`; since 27c56b2 the style sets the
view too, and online a match plays the HOST's style). Three modes → a small enum:

```c
typedef enum { GS_AMIGA_50 = 0, GS_DOS_70, GS_AMIGA_60 } GameStyle;  /* our numbering */
```

`DosGameplay_TargetFps()` / `GameStyle_LogicHz()` / `ApplyGameStyle()` switch on it;
keep `_gDosGameplay` as a derived `(_gGameStyle == GS_DOS_70)` for back-compat with
existing call sites.

**ini (recommended shape — a `[gameplay]` section, one bundled selector).** Written
before the setting existed; the selector that shipped is `[gameplay] style = amiga | dos`,
so `amiga60` would become a third `style` value rather than a new `mode` key, and the
legacy `[dos] gameplay` fallback below no longer applies — there is no `[dos]` section
any more; the art side is `[graphics] style = amiga | dos | 2020`:

```ini
[gameplay]
; mode: amiga | dos | amiga60
;   amiga    50 Hz, authentic (default)
;   dos      70 Hz DOS physics/keeper/timers (players speed_factor 41/64)
;   amiga60  60 Hz refresh, authentic Amiga gameplay (decoupled 50 Hz logic)
mode = amiga

[dos]
graphics = true    ; graphics is ORTHOGONAL to gameplay — unchanged
```

Back-compat: if `[gameplay] mode` is absent, fall back to legacy `[dos] gameplay`
(`true → dos`, `false → amiga`). `[dos] graphics` is unrelated (DOS graphics can pair
with any gameplay).

## 8. Implementation phases (each shippable)

0. **Plumbing** — the `_gGameStyle` enum + `[gameplay] mode` parse. `amiga60` initially
   behaves like `amiga50` (no decoupling yet), just selectable. Low-risk.
1. **Split** `GameLoop()` → `GameLoop_Logic()` + `Render(alpha)` (alpha=1 → identical to
   today; pure refactor, verify byte-identical output first).
2. **Accumulator loop** — drive logic at fixed 50 Hz, present at monitor rate (alpha
   still 1, i.e. no interpolation yet → 50 Hz logic, 60 Hz duplicate-frame present;
   confirm gameplay unchanged).
3. **Interpolate sprites** — `prev/curr` snapshots + interpolated player/ball draw.
4. **Interpolate camera + ball-Z/shadow.**
5. **Edge cases** — state transitions (kickoff/goal/half-time freeze frames), pause,
   menus (decoupling is match-loop-only), variable/uneven refresh, vsync off.

## 9. Gotchas / constraints

- **Replays are logic-rate.** `.HIL`/`.RPL` and arcade attract reels are frame-rate-
  tagged (`docs/DOS_GAMEPLAY.md` §7.6, `docs/ARCADE.md`). Record/replay at the **50 Hz
  logic rate**; interpolation is render-only and is NOT recorded — so an Amiga-60 replay
  is a normal 50 Hz Amiga replay. Good (no new replay format).
- **Input is polled per logic step (50 Hz), not per render.** Sampling input per render
  frame would desync the sim and change feel — keep input in `GameLoop_Logic()`.
- **Determinism**: interpolation must be a pure function of `prev/curr/alpha` and must
  NEVER feed back into sim state (RNG, positions). Logic stays deterministic.
- **Animation frames / facing use `curr`** (no interpolation) — see §5.
- **The camera Y-follow + the widescreen render already work** (see
  `project_camera_freeze_reports`); interpolation sits on top of `_cameraX/_cameraY`,
  which are logic-updated by `MoveCamera()`.
- **Present should vsync** to the monitor for tear-free smoothness; the accumulator
  tolerates any present cadence.
- **This is Amiga-authentic and PC-only in spirit**, but the split loop is shared C —
  the real Amiga runs 50 Hz native (no accumulator needed; `GameStyle_LogicHz()==present
  rate==50`, alpha always 1, so the decoupled loop degenerates to the classic loop on
  the Amiga — no behaviour change, back-portable).

## 9a. Shipped partial mitigation: sub-pixel scroll presentation

`[video] subpixel_scroll` (default on, `--subpixel=off` to compare) addresses the
**spatial** half of the problem without any of the work above. The camera is 16.16
but everything is drawn at `_cameraX.s.i`, so a true velocity of 1.5 px/frame
renders as an alternating 1,2,1,2 px step. Measured on the corpus
(`scripts/rgd_convert.py --dump-frames`): **38% of consecutive moving frames change
step size**, and a long ball peaks at **5 px in one 20 ms frame**.

`camera.c` publishes the residual (`Render_PublishScrollSubpixel`, called from
`ScrollToCurrent`); `platform_sdl.c` `present_frame_copy` shifts the finished frame
by it. Render-only — the replay rig passes 1519/1519 unchanged. The shift is
quantised to whole *output* pixels so nearest-neighbour stays crisp, which also
means it does nothing at 1x window scale.

It does **not** fix the temporal half: 50 discrete positions per second is still
50, and on a 300 Hz sample-and-hold panel each is held for 6 refreshes. That is
what §3 is for, and it remains the real fix.

## 10. Fallback (if full decoupling is deferred)

The `speed_factor 50/60` path (identical mechanism to the DOS 41/64, via
`CalculateDeltaXAndY`) gives a quick, smooth-*ish* 60 Hz Amiga — but it is **not
bit-exact** (§2) and therefore not "authentic." Only ship it as an explicitly-labelled
approximate mode if the decoupling work is postponed; the owner's stated goal is the
authentic decoupled version.

## References

- `docs/DOS_GAMEPLAY.md` §8 — the speed_factor, the `CalculateDeltaXAndY_125` hook, the
  test-team preset table, and §8.4 (this decoupling insight in brief).
- `src/game_loop.c` `GameLoop`, `GameLoop_Frame`, `GameLoop_EndFrame`, `waitVBlankSync`; `src/platform_sdl.c`
  `Platform_VBlank` / pacing; `src/move_camera.c` `MoveCamera`; `src/camera.c`
  `ScrollToCurrent`; `DrawSprites`.
- swos-2020 — did not solve authentic high-FPS gameplay (abandoned `g_SpeedFactor`).
- Glenn Fiedler, *Fix Your Timestep!* — the accumulator + interpolation pattern.
