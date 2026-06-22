# Widescreen / Adaptive Aspect Ratio

The SDL port renders at an aspect ratio that **matches the display**. On a 4:3
monitor it shows 4:3, on 16:9 it shows 16:9, on a 21:9 ultrawide it fills the
whole screen — no pillarbox/letterbox bars and nothing to configure. There is no
fixed "widescreen resolution"; the render width is derived from the monitor at
startup.

The classic Amiga build is unaffected: every widescreen quantity folds to zero /
to the original 336-wide layout when `g_screen_width == SCREEN_WIDTH`.

---

## Configuration

`SWOS.ini`:

```ini
[video]
; widescreen: true | false — true matches the render to your monitor's aspect
;   ratio so it fills the screen (16:9 / 21:9 / ultrawide); false = classic 4:3
widescreen=true
```

- **`true`** (default) — the render width tracks the monitor's aspect ratio.
- **`false`** — forces the classic `SCREEN_WIDTH` (336) view.

There is no separate "ultrawide" mode. `true` already fills whatever display you
have, up to the limit of the pitch (see below).

---

## How it works

### The render width: `g_screen_width`

`g_screen_width` (an `extern int`, `src/display.h`) is the **live frontbuffer
stride** — the width, in pixels, of the picture being drawn. It is *runtime*, not
compile-time.

It is computed in `src/main.c`, **after `SDL_Init`** (so the desktop resolution
is known) and before the window/texture are created:

```c
if (gWidescreen) {
    SDL_DisplayMode dm;                 /* SDL_GetDesktopDisplayMode */
    int w = SCREEN_HEIGHT * dm.w / dm.h;   /* 272 * monitor aspect */
    w = round-up-to-even(w);
    w = clamp(w, SCREEN_WIDTH, PITCH_VIEW_MAX_WIDTH);   /* [336 .. 672] */
    g_screen_width = w;
} else {
    g_screen_width = SCREEN_WIDTH;      /* 336 */
}
```

Key constants (`src/display.h`):

| Symbol | Value | Meaning |
| --- | --- | --- |
| `SCREEN_WIDTH` | 336 | Logical UI width — the menu/HUD layout width that gets **centered** in the wider frame. Never changes. |
| `SCREEN_HEIGHT` | 272 | Frame height. Never changes (only the width is adaptive). |
| `PITCH_VIEW_MAX_WIDTH` | 672 (`0x2A0`) | The pitch world width = the **maximum** useful render width. The camera clamp keys off the same `0x2A0`. |
| `FRONTBUFFER_MAX_STRIDE_BYTES` | 672 | Frontbuffer arrays are **allocated** at this max, so any runtime `g_screen_width` fits. |
| `g_screen_width` | 336 … 672 | The live render width (= frontbuffer stride). |

Example widths the auto-fit produces:

| Display | Aspect | `g_screen_width` |
| --- | --- | --- |
| 1280×1024 | 5:4 | ~340 |
| 1920×1080 | 16:9 | ~484 |
| 2560×1080 | 21:9 | ~648 |
| 3440×1440 | 21:9 | ~650 |
| anything ≥ 2.47:1 | — | capped at 672 (pitch edge) |

Because the cap is the pitch width (≈2.47:1), a display wider than that will
still get thin side bars — there is simply no more pitch to show.

### Centering: `WIDE_MARGIN` and `g_menu_draw_x_offset`

The UI (menus, HUD, stats) is authored for the 336-wide frame. To place it in the
centre of a wider frame we shift it right by:

```c
#define WIDE_MARGIN ((g_screen_width - SCREEN_WIDTH) / 2)   /* 0 when not wide */
```

`g_menu_draw_x_offset` (`extern int`, `src/display.h`) is a **per-draw X shift**
added to the frontbuffer *write base* inside the low-level primitives:

- `src/blitter.c` — `_FillRectangle`
- `src/text_render.c` — glyph/text blit
- `src/sprites.c` — sprite blit bases

It is set to `WIDE_MARGIN` in `_DrawMenu` (and persists across the menu's draws),
and **reset to 0** in `src/game_loop.c` before the in-match *world* draw, so the
pitch and players are never shifted.

> Anything drawn through those primitives while `g_menu_draw_x_offset ==
> WIDE_MARGIN` lands in the centred 336 region; while it's 0 it lands at raw
> coordinates.

### Camera

The pitch world is 672 px wide. The view is `g_screen_width` px wide, so the
camera can pan `672 - g_screen_width` px and still keep the view on the pitch.

- `src/move_camera.c` — the X clamp uses `pitchWidth(0x2A0) - g_screen_width`. As
  the view approaches the full pitch width (ultrawide), that pan range collapses
  (and would invert), so the camera **centres** the view on the pitch
  (`panRange / 2`) instead of clamping. At ~650 wide it pans only ~22 px — a
  near-static, full-pitch "tactical" view.
- `src/display.c` — `_SetCameraCoordinates` is the chokepoint every camera setter
  funnels through (gameplay, **and** the kickoff/intro pan and panel restore,
  which bypass `_MoveCamera`). It clamps `targetX` to `[0, 672 - g_screen_width]`
  so even those direct setters stay on the pitch. Without this, the first frames
  at kickoff showed the stands / off-pitch surround on a wide view.

A wider view therefore scrolls **less** far (the right edge still stops at the
pitch edge); everything collapses to the original behaviour at 336.

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
value columns). It is wrapped in `g_menu_draw_x_offset = WIDE_MARGIN`
(save/restore) so the grid, labels, and numbers all centre together — the same
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

## Amiga back-port

Everything folds to a no-op on the Amiga build, where `g_screen_width ==
SCREEN_WIDTH` (336), so `WIDE_MARGIN == 0` and `g_menu_draw_x_offset == 0`.

- **`src/display.h` is NOT in the file-sync list** (it carries the `CUSTOM_BASE`
  Amiga hardware-address divergence). When back-porting, **manually** patch the
  Amiga tree's `include/display.h` with: the `g_screen_width` fold to
  `SCREEN_WIDTH`, `WIDE_MARGIN`, `PITCH_VIEW_MAX_WIDTH`,
  `FRONTBUFFER_MAX_STRIDE_BYTES`, and `extern int g_menu_draw_x_offset;`.
- All other touched files **are** synced and compile/no-op on m68k:
  `main.c`, `menu_system.c`, `sprites.c`, `blitter.c`, `text_render.c`,
  `move_camera.c`, `display.c`, `game_state.c`, `game_hud.c`, `statistics.c`,
  `frontend.c`, `loader_intro.c`, `swos_globals.c`, `platform_sdl.c` (SDL-only),
  `swos_settings.c` (SDL-only).

---

## Gotchas / lessons learned

- **The masked (transparent) menu-item background was the last un-centered draw.**
  Both `_BlitMaskedRectAltToFront` *and* `_DrawMenuItemBackgroundMasked` must add
  `g_menu_draw_x_offset`. Missing it on the latter produced a corrupt EXIT/ABORT
  button that "fixed itself" on redraw — because the masked bg is only used in
  some item states, the corruption was state-dependent.
- **Direct camera setters bypass `_MoveCamera`.** Clamping only in `_MoveCamera`
  left the kickoff/intro frames off-pitch; the clamp belongs at the
  `_SetCameraCoordinates` chokepoint.
- **The camera clamp inverts on ultrawide.** When `pitchWidth - g_screen_width`
  is small, `xMax - margin` goes negative; centre the view instead of clamping.
- **Don't shift world sprites.** Only screen-fixed HUD sprites get `WIDE_MARGIN`;
  the players/ball/pitch must stay at raw camera-relative coordinates or the
  whole pitch would slide.
- **Window size was never the issue.** SDL `SDL_RenderSetLogicalSize` letterboxes
  the logical `g_screen_width × 272` inside the window/fullscreen, so getting rid
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
