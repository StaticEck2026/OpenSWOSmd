# Widescreen / Adaptive Aspect Ratio

> **Status (2026-09-30):** current, with three later changes folded in below:
> the globals lost their `g_` prefix (`_screen_width`, `_menu_draw_x_offset`,
> `include/display.h`); the **simulation** view is pinned to 336x272 (320x200 for
> the DOS view) whatever the render width, and the camera is split into a game
> camera and a scaled render camera (see "Simulation vs render width" and
> "Camera"); and `[video] widescreen` gained `stretch` (a4f2689).

The SDL port renders at an aspect ratio that **matches the display**. On a 4:3
monitor it shows 4:3, on 16:9 it shows 16:9, on a 21:9 ultrawide it fills the
whole screen — no pillarbox/letterbox bars and nothing to configure. There is no
fixed "widescreen resolution"; the render width is derived from the monitor at
startup.

The classic Amiga build is unaffected: every widescreen quantity folds to zero /
to the original 336-wide layout when `_screen_width == SCREEN_WIDTH`.

---

## Configuration

`SWOS.ini`:

```ini
[video]
; widescreen: true | false | stretch. true fills the display's shape, showing
; more of the pitch at the sides; false draws the view's own width -- 336 the
; Amiga way, 320 the DOS way -- with bars; stretch draws that same picture
; pulled out to fill the display. ...
widescreen=true
; resolution: WxH drives the render ASPECT, or 'auto' for the desktop
resolution=auto
```

- **`true`** (default) — the render width tracks the display's aspect ratio
  (`resolution=WxH` if set, else the desktop of `[video] screen`).
- **`false`** — the view's own width: 336 (Amiga view) or 320 (DOS view),
  pillarboxed.
- **`stretch`** — that same own-width picture scaled per axis to fill the window
  (`apply_picture_scaling` in `src/platform_sdl.c`: `SDL_RenderSetScale` instead of
  the aspect-keeping `SDL_RenderSetLogicalSize`, recomputed every frame).

Command line: `--widescreen=on|off|stretch`. The field is
`PlatformInitOptions.widescreen` (`include/platform_sdl.h`: 1 / 0 / 2).

There is no separate "ultrawide" mode. `true` already fills whatever display you
have, up to the limit of the pitch (see below).

---

## How it works

### The render width: `_screen_width`

`_screen_width` (an `extern int`, `include/display.h`; formerly `g_screen_width`)
is the **live frontbuffer stride** — the width, in pixels, of the picture being drawn. It is *runtime*, not
compile-time.

It is computed in `src/main.c`, **after `SDL_Init`** (so the desktop resolution
is known) and before the window/texture are created:

```c
/* dw x dh = [video] resolution, or the desktop mode of [video] screen */
int w = SCREEN_HEIGHT * dw / dh;       /* 272 * display aspect */
w = round-up-to-even(w);
if (w > dw) w = dw & ~1;               /* never wider than the resolution asked for */
w = clamp(w, SCREEN_WIDTH, PITCH_VIEW_MAX_WIDTH);   /* [336 .. 672] */
if (opts.widescreen != 1) w = SCREEN_WIDTH;         /* false / stretch: 336 */
_screen_width = w;
```

The DOS view (`[gameplay] style = dos`, or `--view=dos`) does the same at 200
lines: `_dos_view_width = 200 * dw / dh`, clamped to `[320 .. 672]`, or 320 when
`widescreen` is not `true`. It is applied at kick-off and undone after the match
(`game_loop.c`), because the menus stay 336x272.

Key constants (`src/display.h`):

| Symbol | Value | Meaning |
| --- | --- | --- |
| `SCREEN_WIDTH` | 336 | Logical UI width — the menu/HUD layout width that gets **centered** in the wider frame. Never changes. |
| `SCREEN_HEIGHT` | 272 | Frame height. Never changes (only the width is adaptive). |
| `PITCH_VIEW_MAX_WIDTH` | 672 (`0x2A0`) | The pitch world width = the **maximum** useful render width. The camera clamp keys off the same `0x2A0`. |
| `FRONTBUFFER_MAX_STRIDE_BYTES` | 672 | Frontbuffer arrays are **allocated** at this max, so any runtime `_screen_width` fits. |
| `_screen_width` | 336 … 672 | The live render width (= frontbuffer stride). |
| `WIDESCREEN_WIDTH` | 480 | 16:9 reference; `HUD_WIDTH` caps the in-match HUD box at it on ultrawide. |

Example widths the auto-fit produces:

| Display | Aspect | `_screen_width` |
| --- | --- | --- |
| 1280×1024 | 5:4 | ~340 |
| 1920×1080 | 16:9 | ~484 |
| 2560×1080 | 21:9 | ~648 |
| 3440×1440 | 21:9 | ~650 |
| anything ≥ 2.47:1 | — | capped at 672 (pitch edge) |

Because the cap is the pitch width (≈2.47:1), a display wider than that will
still get thin side bars — there is simply no more pitch to show.

### Centering: `WIDE_MARGIN` and `_menu_draw_x_offset`

The UI (menus, HUD, stats) is authored for the 336-wide frame. To place it in the
centre of a wider frame we shift it right by:

```c
#define WIDE_MARGIN ((_screen_width - SCREEN_WIDTH) / 2)   /* 0 when not wide */
```

`_menu_draw_x_offset` (`extern int`, `include/display.h`; formerly
`g_menu_draw_x_offset`) is a **per-draw X shift**
added to the frontbuffer *write base* inside the low-level primitives:

- `src/blitter.c` — `_FillRectangle`
- `src/text_render.c` — glyph/text blit
- `src/sprites.c` — sprite blit bases

It is set to `WIDE_MARGIN` in `_DrawMenu` (and persists across the menu's draws),
and **reset to 0** in `src/game_loop.c` before the in-match *world* draw, so the
pitch and players are never shifted.

> Anything drawn through those primitives while `_menu_draw_x_offset ==
> WIDE_MARGIN` lands in the centred 336 region; while it's 0 it lands at raw
> coordinates.

### Simulation vs render width

The render width is NOT what the game plays at. SWOS's gameplay reads the view
size — the `onScreen` cull decides which players auto-selection may pick — so a
wider view used to change CPU behaviour. Now (`include/display.h`, "The
SIMULATION view"):

- `_sim_view_width/_height` — the view the simulation believes in: pinned to
  336x272 in `main.c` (320x200 in the DOS view, `game_loop.c`) whatever the
  display; the replay rig sets it from the recording.
- `_sim_draw_width` — the width for the few decisions that depend on what is
  DRAWN (margin sprites animating, crowd tiles, corner flags, bench): the render
  width locally, 336 in the replay rig, and one agreed width in a netplay match
  (`P2PLink_AgreedSimWidth`, docs/NETPLAY.md).

So the extra columns are margin the simulation cannot use; every display plays
the same game (docs/GAMEPLAY.md §25.4/§25.5).

### Camera

> **Changed since this section was written.** There are now two cameras
> (`include/game_types.h`, "The two camera origins"): `_cameraX` is the GAME's,
> clamped to the simulation range `0 .. 0x2A0-336` (both in `MoveCamera` via
> `SIM_CAMERA_PAN_RANGE` and in `SetCameraCoordinates`, which now clamps by the
> constant 336 — see the comment there for the tester bug the render-width clamp
> caused); `Render_CameraX()` (`src/camera.c`) is where the picture is DRAWN
> from, the game camera's range scaled onto the wide view's pan range
> `0 .. 0x2A0-W`. The text below describes the original single-camera design.

The pitch world is 672 px wide. The view is `_screen_width` px wide, so the
camera can pan `672 - _screen_width` px and still keep the view on the pitch.

- `src/move_camera.c` — the X clamp used `pitchWidth(0x2A0) - g_screen_width`. As
  the view approaches the full pitch width (ultrawide), that pan range collapses
  (and would invert), so the camera **centres** the view on the pitch
  (`panRange / 2`) instead of clamping. At ~650 wide it pans only ~22 px — a
  near-static, full-pitch "tactical" view.
- `src/display.c` — `_SetCameraCoordinates` is the chokepoint every camera setter
  funnels through (gameplay, **and** the kickoff/intro pan and panel restore,
  which bypass `_MoveCamera`). It clamped `targetX` to `[0, 672 - g_screen_width]`
  so even those direct setters stay on the pitch. Without this, the first frames
  at kickoff showed the stands / off-pitch surround on a wide view.

A wider view therefore scrolls **less** far (the right edge still stops at the
pitch edge); everything collapses to the original behaviour at 336.

### The DOS view

`[gameplay] style = dos` simulates the PC release's **320x200** view (`MatchViewEnter` pins
`_sim_view_width/_height`), which decides where the camera aims, its limits and which
players count as on screen. The PICTURE is separate:

| `[video] widescreen` | DOS picture |
|---|---|
| `false`, `stretch` | 320x200, the PC release's own (stretched to fill with `stretch`) |
| `true` | the same `w x 272` as the Amiga view's widescreen picture (484x272 on 16:9) |

Until 2026-09-30 `true` widened only: 356x200 on 16:9, which looked zoomed in and "threw you
back to DOS resolution" at kick-off (ReportBot 0013). Now the picture is as tall as the
Amiga's and shows more pitch in both directions, and the match underneath is still exactly
DOS -- the replay rig matches the DOS recordings frame for frame with it on.

A picture taller than the simulated view needs a **vertical render origin**, as a wider one
needs `Render_CameraX`: `Render_CameraY()` (camera.c) scales the simulation camera's range
`[0, 880 - simH]` onto the picture's `[0, 880 - H]`, so the picture reaches the top and
bottom of the pitch exactly when the camera does and never shows past the stadium; it is the
identity whenever the picture is no taller than the view (the Amiga view, DOS at 320x200).
Everything that DRAWS uses it -- the pitch (`DrawChunkyPitch`), the sprites, the crowd,
highlight playback, the `.SRP` player -- and nothing the game DECIDES from: the draw list's
`onScreen` and `animVis` culls keep measuring from `_cameraY` (`d2sim` in sprites.c), as they
keep `_cameraX` for X. The in-match instant replay records the drawn origin and hands it
back through the inverse, `Render_SimCameraYFromRender`.

The HUD: top-anchored items (the clock, the replay R) sit at `Render_CameraY() + K`;
bottom-anchored ones (the result panel, the scorer list, the name bar's bottom corners) add
`RENDER_EXTRA_Y` (display.h: picture height minus simulated height, 72 here, 0 otherwise) so
they follow the bottom edge down.

A `.SRP` records the height its match was simulated in (META, docs/REPLAY_FORMAT_V2.md), and
playback maps its camera from that (`ReplayV2_ApplyRecordedSimView`), so a replay draws what
the live match drew at any picture size.

### In-match HUD (screen-fixed sprites)

The in-match HUD — team names, score digits, the dash, the scorer list, the
halftime/result banner, the clock, and the controlled-player name — are drawn as
sprites that stay fixed on screen by setting

```c
sprite.x = cameraX + screen_pos;   /* so _DrawSpriteInCameraView's x -= cameraX
                                      leaves them at the fixed screen_pos */
```

To centre them, `WIDE_MARGIN` is added to each base:

- `src/game_state.c` — 17 positions across the score-panel layout
  (`position_sprites`) and the clock (`set_time_sprite_x`).
- `src/game_hud.c` — the controlled-player-name base (the per-corner offsets
  `+10`/`+20` left, `+326 - width` right key off it).

The **world** sprites (players, ball, pitch tiles) go through the same camera
subtraction but pass *true world coordinates*, so they are untouched and the
pitch fills the view.

### Stats overlay

`src/statistics.c` `_DrawStatistics` lays the match-stats screen out on 336-frame
coordinates (`_DrawGrid(16,…,304,…)`, `_DrawMenuTextCentered(168,…)`, the 80/256
value columns). It is wrapped in `_menu_draw_x_offset = WIDE_MARGIN`
(save/restore; now `(_screen_width - viewWidth) / 2`, the view being 336 or the
DOS 320) so the grid, labels, and numbers all centre together — the same
mechanism as the menus. This covers both callers: the **S-key** in-match stats
and the **full-time** summary.

---

## What's centered vs what fills

| Element | Behaviour on a wide view |
| --- | --- |
| Pitch, players, ball | **Fill** — rendered at the monitor's true aspect |
| Camera | Pans within the pitch; centres when the view ≈ pitch width |
| Menus (all front-end) | **Centred** 336 region (incl. masked/transparent item backgrounds) |
| Startup / intro / menu background art | Decoded centred (`+ WIDE_MARGIN`) |
| In-match score panel, clock | **Centred** |
| Controlled-player name | **Centred** |
| Match stats (S-key + full-time) | **Centred** |

---

## Amiga target

Everything folds to a no-op on the Amiga build, where `_screen_width` is
`#define`d to `SCREEN_WIDTH` (336) in `include/display.h` (as are
`_sim_view_width` and `_sim_draw_width`, and `Render_CameraX()` is `_cameraX`), so
`WIDE_MARGIN == 0` and `_menu_draw_x_offset == 0`.

> This section used to describe back-porting to a separate Amiga tree with a
> file-sync list. Since the 2026-08-12 merge there is one `src/`/`include/`
> tree; the `#ifdef SWOS_SDL_BUILD` folds in `include/display.h` are the whole
> mechanism. `platform_sdl.c` and `swos_settings.c` are SDL-only sources.

---

## Gotchas / lessons learned

- **The masked (transparent) menu-item background was the last un-centered draw.**
  Both `_BlitMaskedRectAltToFront` *and* `_DrawMenuItemBackgroundMasked` must add
  `_menu_draw_x_offset`. Missing it on the latter produced a corrupt EXIT/ABORT
  button that "fixed itself" on redraw — because the masked bg is only used in
  some item states, the corruption was state-dependent.
- **Direct camera setters bypass `_MoveCamera`.** Clamping only in `_MoveCamera`
  left the kickoff/intro frames off-pitch; the clamp belongs at the
  `_SetCameraCoordinates` chokepoint.
- **The camera clamp inverts on ultrawide.** When `pitchWidth - _screen_width`
  is small, `xMax - margin` goes negative; centre the view instead of clamping.
- **Don't shift world sprites.** Only screen-fixed HUD sprites get `WIDE_MARGIN`;
  the players/ball/pitch must stay at raw camera-relative coordinates or the
  whole pitch would slide.
- **Window size was never the issue.** SDL `SDL_RenderSetLogicalSize` letterboxes
  the logical `_screen_width × 272` inside the window/fullscreen, so getting rid
  of bars is about the *logical aspect*, not the window resolution.

---

## Changelog

| Commit | Change |
| --- | --- |
| `dca718f` | Center the masked menu-item background (fixes corrupt EXIT/ABORT button) |
| `b2f0fea` | Match render aspect to the monitor (adaptive `g_screen_width`, fills any display) |
| `4ff23d7` | Clamp the kickoff/intro camera to the pitch at `_SetCameraCoordinates` |
| `58264a2` | Center the in-match score panel + clock |
| `b3d6162` | Center the controlled-player name + end-of-game stats |
| `22f370a` | Pin the simulation view to 336 (widescreen no longer changes CPU player selection) |
| `a4f2689` | `widescreen = stretch`; the view follows `[gameplay] style` |
