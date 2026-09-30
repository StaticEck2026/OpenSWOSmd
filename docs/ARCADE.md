# Arcade mode (OpenSWOS Arcade)

A dedicated **arcade build/mode** of the SDL port: no front-end menus, no career,
no save/load UI. The cabinet boots straight into an **attract loop**, takes
**coins/credits**, plays short **first-goal-wins** matches, and exposes an
operator **service menu** (Midway/NBA-Hangtime style). The normal Amiga game is
the definitive product; this is a separate, stripped, coin-op presentation layer
on top of the same match engine.

Gated behind an arcade flag (proposed `gArcadeMode`, set like `gDosGraphics` from
`[arcade] enabled=true` / `--arcade`). When off, nothing here is reachable and the
normal build is unchanged.

> **Status (2026-09-30):** built — `src/arcade.c` (SDL-only), flag `_gArcadeMode`
> from `[arcade] enabled` in `SWOS.ini` (there is no `--arcade` command-line
> switch). §1–§6 are the original spec; "Current implementation decisions"
> below is what was built and supersedes it where they differ.

> Source spec: `D:\Projects\SWOS\OpenSWOS\Arcade\ARCADE_MODE.md`.
> Service-menu visual reference: NBA Hangtime
> (`D:\Projects\Arcade\nba-hangtime\MAME\snap\nbahangt\*.png`).

## 1. Game rules (arcade match)

- **45 seconds** play time. **First goal wins** (golden goal — match ends the
  instant anyone scores).
- **Draw → immediate penalty shootout** (sudden death), no extra time.
- **Winner stays on.** A waiting player 2 must **pay/insert a credit ("play a
  quarter")** to challenge; on a win they take the seat.
- Short, repeatable, coin-friendly sessions. No tactics screens, no subs UI, no
  half-time — straight kickoff → result → next.

## 2. Attract mode

When idle (no credits, no game in progress) the cabinet runs an **attract loop**:

- Plays back **special highlight files** — `.HIL` reels recorded for this purpose.
  Source sets live in `D:\Projects\SWOS\DOS\save\*.zip`:
  - `2020 widescreen HIL files in DOS mode 70FPS.zip` ← primary (matches the
    arcade's 70 FPS DOS gameplay, see `docs/DOS_GAMEPLAY.md`)
  - `2020 widescreen HIL files in AMIGA mode 50FPS.zip`
  - `original DOS HIL files.zip`
  - (matching `.RPL` replay sets exist alongside)
- The loop cycles: title/logo → attract highlight(s) → "INSERT COIN" / high-score
  or how-to-play card → repeat.
- **Any coin/credit interrupts** the attract loop and drops into team select /
  kickoff.
- Reuses the existing highlight playback path (`highlight.c` / `_PlayInstantReplayLoop`
  / `.HIL` loader) — the arcade just feeds it the curated reels and disables the
  normal "press fire to exit to menu" so it free-runs.
- Pick the `.HIL` set by gameplay mode: 70 FPS DOS reels when DOS gameplay is on,
  50 FPS Amiga reels otherwise (a reel recorded at one rate plays back wrong at
  the other — see DOS_GAMEPLAY.md timing notes).
  *(Superseded: each reel now self-describes its rate and plays at it, `c396c60`,
  so one flat reel folder serves both modes — see "Current implementation
  decisions".)*
- **`.SRP` reels are accepted too** (`docs/REPLAY_FORMAT_V2.md`): drop a recording
  from `[replays] auto_save` into `bin/grafs_arcade/attract_mode/` and it joins the
  rotation. One carries its own tick rate, camera and **sound**, so it needs no
  per-mode set and plays correctly whichever gameplay the cabinet is running.

## 3. Service menu (operator menu)

Triggered by a **service button** (F2 in MAME; a dedicated cabinet input on real
hardware). Pauses the game/attract loop and shows the operator UI. Style is
deliberately the classic **Midway service menu** (NBA Hangtime), not the SWOS
menu look.

### 3.1 Visual style (from the NBA Hangtime reference)

- **Black background.**
- **Bordered boxes** drawn with the SVCFONT border/box glyphs — a title box at
  top, a left-hand **menu list** box, and right-hand **description** + **values**
  boxes.
- **Multi-colour text**, colour-by-role:
  - green = screen title (e.g. "GAME ADJUSTMENT")
  - white/grey = subtitle / static labels
  - red = instructions ("SELECT WITH ANY STICK — PRESS A BUTTON TO MODIFY")
  - blue = description text (in its own bordered box)
  - orange/yellow = values
- A scrollable **menu list** with a **selection bar**: the current row is
  highlighted and its label **flashes/cycles through colours** (see 3.4).

### 3.2 Font — `bin/grafs_arcade/SVCFONT.bmp`

- 320×256, 8bpp paletted; **palette embedded in the BMP** and identical to the
  arcade **Service palette** (16 entries) now in `dos_graphics.c`
  (`DosGfx_BuildServicePalette`, == `DosPalette.Service`). First entry is the
  transparent/background black; entries 1..15 are the text/UI colours
  (grey, white, red, orange, yellow, green, blue, …).
- Two glyph sizes on the sheet (a **large** WIMP-style face and a **small**
  variant), full ASCII (upper/lower/digits/punct) plus special glyphs:
  - a **hollow box / border** glyph (panel frames)
  - a **logo** glyph (bottom-right roundel)
  - `Sp` (space) and `END`/`RUB` editing glyphs
- **Selection bar**: a **solid white 8×8** glyph drawn behind/around the selected
  row (distinct from the hollow border glyph).
- **Variable-width glyphs.** Use a glyph LUT in the format of
  `D:\Projects\Arcade\nba-hangtime\FONT_EXPORT\combined_320x256.h`:
  ```c
  typedef struct {
      unsigned char  ch;     /* ASCII code, 0 if none */
      unsigned short x, y;   /* top-left of glyph in the sheet */
      unsigned char  w, h;   /* glyph size in pixels */
      signed char    yoff;   /* glyph top offset from text baseline-top */
  } FontGlyph;
  ```
  Generate one LUT per face (e.g. `SVCFONT_LARGE_glyphs[]`, `SVCFONT_SMALL_glyphs[]`)
  from the SVCFONT sheet. Text layout advances by per-glyph `w` (+1px tracking),
  positions each glyph by `yoff` from the row's baseline-top.

### 3.3 Rendering pipeline

- The service menu is its own full-screen 8bpp chunky surface drawn to the
  frontbuffer, like the rest of the renderer.
- When the service menu is up, set the active 256-entry SDL palette's first 16
  entries from `DosGfx_BuildServicePalette()` (rest can be zero) — analogous to
  the DOS palette hook in `platform_sdl.c::refresh_palette_from_copper`, gated on
  "service menu active" rather than `playGame`.
- Blit glyphs by copying their `w×h` rect from the decoded SVCFONT chunky bitmap
  to the target X/Y, skipping palette-index-0 (transparent). A `_DrawSvcText(x,y,
  colour, str)` helper walks the string through the glyph LUT.
- Colour is applied by **remapping the glyph's ink index to the chosen Service
  palette index** at blit time (1-byte LUT, same idea as `_ConvertSpriteColors`)
  — the sheet stores glyphs in one ink colour; the draw recolours per call.

### 3.4 Selection: flashing multi-colour text + selection bar

- The selected menu row draws its **selection bar** first: tile the solid-white
  8×8 glyph across the row width (or draw a filled rect in palette white).
- The selected row's **text flashes through the Service palette colours** —
  cycle the ink colour each frame (or every N frames) through a small colour list
  (e.g. white→yellow→orange→red→… ) keyed off `currentGameTick`, so it visibly
  pulses. Non-selected rows draw static (white/grey).
- Up/down moves the selection; a button "modifies"/enters the highlighted item.

### 3.5 Service-menu contents (initial)

Model on NBA Hangtime's "GAME ADJUSTMENT" screen; start minimal and grow:

- **Diagnostics / boot screen**: build/version string, CMOS/settings status,
  serial number, "PRESS ANY BUTTON TO CONTINUE" (the 0000.png boot card).
- **Game adjustment**: free play on/off, credits-per-coin / coin chute
  multipliers, attract-mode sound on/off, difficulty, match-time (default 45s),
  win bonus.
- **Bookkeeping**: coins in, games played, play-time totals.
- **Input test, sound test, screen test** (standard operator diagnostics).
- **Reset to factory settings.**
- Settings persist to an operator config file (own file, not the player
  `SWOS.ini`).

## 4. Pitches (arcade-exclusive art)

Seven themed pitches (art already ripped to `bin/grafs_arcade/pitch-*.png`):
Beach, Park, Yard, Skate Park, Prison, City, Stadium. These slot into the DOS
pitch pipeline (`docs/DOS_GRAPHICS.md`) as additional pitch sets — selectable per
match (random or rotating in attract). (Conversion of the PNGs into the
`PITCH*.BLK`/`.DAT` tile-bank format is a separate art-pipeline task.)

## 5. Hardware target (real cabinet)

From the spec — for context, not needed for the SDL build:
- SBC: Luckfox CM5 IO board / Raspberry Pi CM4 / Pi 5 (2GB). The ARM Linux build
  (`bin/SWOS.aarch64`) is the deployment target.
- Coin acceptor: SparkFun programmable 6-coin acceptor (GPIO → credit input).
- Card reader: Stripe Terminal M2 (cashless credit).
- Inputs map to the existing joystick/fire handling; coin/service lines are extra
  GPIO inputs.

## 6. Implementation phasing (proposed)

1. **`gArcadeMode` flag** + `[arcade]`/`--arcade` wiring (mirror the `[dos]`
   plumbing in `main.c`/`swos_settings.c`/`dos_mode.h`).
2. **SVCFONT loader**: decode `SVCFONT.bmp` → chunky bitmap + generate/embed the
   glyph LUT(s); `_DrawSvcText` + service palette hook.
3. **Service menu shell**: bordered-box layout, list + selection bar + flashing
   selection, navigation, the boot/diagnostic card. Hook the service button.
4. **Attract loop**: free-running `.HIL` playback (curated reels) + INSERT-COIN
   card; coin interrupts → match.
5. **Arcade match rules**: 45s golden-goal, draw→penalties, winner-stays-on,
   credit-to-join. (Trim the match flow: no tactics/subs/half-time.)
6. **Operator config persistence** + bookkeeping counters.
7. **Arcade pitches** art pipeline + per-match selection.
8. **Cabinet I/O** (coin/service GPIO) on the ARM target.

## Open questions
- Exact service-menu item set / defaults (start from NBA Hangtime, prune to SWOS).
- Whether the selection "flash" is a fixed colour cycle or palette-rotate.
- `.HIL` attract reels: which curated set ships, and the rotation order.
- Two-player join/seat-swap flow specifics on the cabinet.

---

## Current implementation decisions (live build, supersedes spec where noted)

User-confirmed concrete requirements as we build it:

- **`[arcade] enabled=false` (default)** in `SWOS.ini`. When `true`, SWOS boots
  **straight to attract mode and never shows the main menu**.
- **Attract mode = the HIL/RPL reels PLAY** (camera follows normally, exactly like
  the replay viewer) showing gameplay footage, with the **SWOS logo overlaid** and
  **flashing INSERT COIN at the bottom** — like a real arcade cabinet's attract
  loop. Reels live in `bin/grafs_arcade/attract_mode/` (random pick, fade in/out
  between them; empty dir = no reels, handle gracefully; `.HIL`, `.RPL` and `.SRP`
  are all accepted; each reel plays at its own recorded rate, so the flat folder is
  the recommended layout and the legacy `dos/`/`amiga/` subfolders are simply
  scanned as well — `arc_scan_reels`). Reuses the existing
  (now widescreen-correct) HIL/RPL playback path. **The pitch art is NOT the
  attract background — the reels are.**
- **Flashing "INSERT COIN"** for **player 1 and player 2** (both, bottom corners).
- **Coin insert (MAME convention): key `5` = credit P1, key `6` = credit P2.**
  Any credit interrupts attract → match. (Rebindable: `[arcade] key_p1_credit`,
  `key_p2_credit`, `key_p1_start` (1), `key_p2_start` (2), `key_service` (F2), as
  SDL key names.)
- **Arcade pitches** (`bin/grafs_arcade/SWCPICH1.bmp … SWCPICH7.bmp`, plus
  `SWCPICHR.bmp` for RANDOM in the pitch selector) are the
  **match playing field**, chosen at random **when a game starts** (not in
  attract). Full 672×880 RGB → quantised to a per-pitch 256-colour palette, loaded
  into the 8bpp pitch pipeline. (SDL-only direct load, not DOS `PITCH*.BLK`/`.DAT`.)
- **Service menu:** **F2** opens it (pauses attract/game). Start with a **basic**
  menu showing the **current mode** + the obvious arcade settings. Font =
  `bin/grafs_arcade/SVCFONT.bmp` (§3.2). Visual ref: NBA Hangtime
  (`D:\Projects\Arcade\nba-hangtime\MAME\snap\nbahangt`).
- **First arcade game mode** = a **FRIENDLY**-style match with arcade rules:
  **45 s** play, **first goal wins**, **draw → direct penalty shootout**. All of
  these (time, golden-goal, etc.) **adjustable in the service menu**.
- **Match teams = national teams picked by flag** (`bin/grafs_arcade/FLAGS.bmp`,
  48 flags, 6×8, row-major). Each flag maps directly to a SWOS national-team
  record `{team-file, ordinal}`, loaded via the normal `LoadTeamFile` +
  find-by-ordinal path into `_selectedTeams[0/1]` (no team-select UI). SWOS ships
  a complete national-team roster across six files: `team.080` Europe, `team.081`
  Africa, `team.082` S.America, `team.083` N&C.America, `team.084` Asia,
  `team.085` Oceania. Full cell→team table below.
- **Attract audio** is selectable (service-menu **ATTRACT AUDIO**, `[arcade]
  attract_audio=music|sfx|music+sfx`, default `music+sfx`): **MUSIC** plays
  `HERO.SNG` (the title theme) over the reels; **SFX** lets the reels' own
  recorded crowd/SFX play with no music; **MUSIC+SFX** mixes both. On
  entering **country (flag) select** it switches to **`MENU.SNG`** (the menu music).
  Note: music and SFX share one RJP1 player, so in MUSIC mode the reels' SFX
  triggers are suppressed (`_gArcadeSfxSuppressed`) — otherwise a reel sound event
  retriggers one of HERO's own samples (its vocal track). The actual match always
  plays real crowd SFX.
- **Attract RPL/HIL reels also render on the random arcade pitches** — the same
  `SWCPICH*` → 8bpp tile-bank pipeline used for the match, a random pitch per reel
  (the recorded pitch is replaced).

### Game modes

The arcade match runs in a selectable **game mode**, chosen on the F2 service menu's
**MODE** row and persisted as `[arcade] mode=<name>` in `SWOS.ini` (case-insensitive;
unknown/blank falls back to the default). Index 0 is the default.

| Mode | Rules |
|--|--|
| **GOLDEN GOAL** (default) | Short timed match (`game_seconds`, default 45 s). The first goal wins and ends the match (after a brief celebration); if it's still level at the time limit the match goes to a penalty shootout (`penalties_on_draw`). |

Per-match rule toggles (F2 service menu, saved to `SWOS.ini`): `game_seconds`,
`golden_goal`, `penalties_on_draw`, and `goal_replay` (replay each goal — drives the
engine's auto-replay, the same thing the normal OPTIONS menu calls **AUTO REPLAYS** /
`_autoReplays`). The match length is counted in **game frames** (deterministic, not
wall-clock), so it stays lockstep-safe for future multiplayer.

**Goal replay (golden goal):** a golden goal no longer ends the match on a fixed
2-second timer. That old timer raced the goal celebration + auto-replay and ended the
match before the replay could play ("a goal immediately exits"). Instead
`Arcade_GoalScored` flags the match to end, and `Arcade_MatchTick` waits for the whole
goal sequence to resolve — the engine runs the celebration (`_goalCameraMode != 0`)
and, when AUTO REPLAYS is on, the instant replay (`_replayState != 0`) — then ends with
the scorer ahead. With `goal_replay` off, the celebration just plays through and the
match ends; with it on, the replay plays first.

More modes will be added later. Adding one is: add its display name to
`s_arcadeModeNames[]` in `src/arcade.c` (the `Arcade_Mode*` helpers expose
name↔index for the service menu + `SWOS.ini`), then wire its rules into the match
setup. The mode shows on the pre-match line-up title in place of "FRIENDLY"
(`_aArcadeMatchName`).

### National-team flag mapping (FLAGS.bmp cell → SWOS national team)

48 cells, row-major (6 across). `file.ord` = national-team file byte + team
ordinal (hex). Five cells have **no SWOS national team** and load a **generic
placeholder** squad; `COD` is mapped to SWOS's "CONGO" (≈ DR Congo).

| # | code | team | # | code | team | # | code | team |
|--:|--|--|--:|--|--|--:|--|--|
| 0 | MEX | 83.08 Mexico         | 16 | CIV | 81.0D Ivory Coast    | 32 | SEN | 81.17 Senegal        |
| 1 | CAN | 83.01 Canada         | 17 | TUN | 81.1E Tunisia        | 33 | AUT | 80.02 Austria        |
| 2 | BRA | 82.02 Brazil         | 18 | CZE | 80.09 Czech Republic | 34 | COL | 82.04 Colombia       |
| 3 | USA | 83.0E United States  | 19 | BIH | — generic (Bosnia)   | 35 | CRO | 80.07 Croatia        |
| 4 | GER | 80.11 Germany        | 20 | HAI | — generic (Haiti)    | 36 | EGY | 81.08 Egypt          |
| 5 | NED | 80.13 Holland        | 21 | TUR | 80.2D Turkey         | 37 | KSA | 84.16 Saudi Arabia   |
| 6 | KOR | 84.18 South Korea    | 22 | CUW | — generic (Curaçao)  | 38 | NOR | 80.20 Norway         |
| 7 | SUI | 80.2C Switzerland    | 23 | SWE | 80.2B Sweden         | 39 | ALG | 81.00 Algeria        |
| 8 | MAR | 81.13 Morocco        | 24 | BEL | 80.05 Belgium        | 40 | UZB | — generic (Uzbekistan)|
| 9 | AUS | 85.00 Australia      | 25 | ESP | 80.2A Spain          | 41 | PAN | 83.0A Panama         |
| 10 | ECU | 82.05 Ecuador       | 26 | FRA | 80.0F France         | 42 | NZL | 85.02 New Zealand    |
| 11 | JPN | 84.09 Japan         | 27 | ARG | 82.00 Argentina      | 43 | CPV | — generic (Cape Verde)|
| 12 | RSA | 81.19 South Africa  | 28 | POR | 80.22 Portugal       | 44 | IRQ | 84.08 Iraq           |
| 13 | QAT | 84.15 Qatar         | 29 | ENG | 80.0B England        | 45 | JOR | 84.0A Jordan         |
| 14 | SCO | 80.27 Scotland      | 30 | IRN | 84.07 Iran           | 46 | COD | 81.07 Congo (≈DR Congo)|
| 15 | PAR | 82.06 Paraguay      | 31 | URU | 82.08 Uruguay        | 47 | GHA | 81.0B Ghana          |

**Generic placeholder (no SWOS national team):** BIH Bosnia (19), HAI Haiti (20),
CUW Curaçao (22), UZB Uzbekistan (40), CPV Cape Verde (43). These load a neutral
default squad so the cell is still playable.

### High scores, name entry, and the attract display

A coin-op high-score system, **one table per game mode**, all in `src/arcade.c`
(it reuses the menu-font / menu-palette compositing the flag & pitch selectors use —
**not** the service font; SVCFONT stays service-menu-only).

- **A play session is a winner-stays-on run.** SCORE = **consecutive wins**, TIME =
  **total in-game time over the run**.
  - **1P**: champion (top/team-1 slot, joystick 1) vs a CPU opponent. Beat the CPU and
    you keep playing fresh random opponents on a fresh random pitch — a free
    continuation (no extra credit). The run ends on the champion's first non-win
    (loss, or losing the penalty shootout). `arc_start_game` loops the match.
  - **2P**: a single head-to-head match (champion = joy1, opponent = joy2); the winner
    posts a score (`wins = 1`). The full "P2 inserts a coin to challenge the 1P
    champion mid-streak and take the seat" cabinet flow is a larger TODO — kept out so
    each player stays on their own stick.
- **Table columns**: RANK · NAME · WINS · TIME. Ranked by wins desc, then by a faster
  total time on a tie. Ten rows per mode.
- **Name entry** (`arc_name_entry`): a 4×10 grid of `A-Z 0-9 . -` + space, plus a
  DEL/END action row, drawn in the in-game menu font with a flashing selector. Either
  player's stick navigates (the champion may be P1 or P2); fire picks a cell; END or
  the 3rd initial finishes. Shown only when the run's score makes that mode's table.
- **Run end** (`arc_run_over`): GAME OVER beat (wins/time) → if it qualifies, name
  entry → insert + save → flash the updated table; otherwise just show the table. The
  whole run + run-over is **modal** (`s_arcModalRunOver`): coins still accumulate for
  the next game, but start/service can't interrupt a run in progress.
- **Attract display** (`arc_fg_advance`, driven once per attract frame from
  `Arcade_DrawAttractOverlay`): over the attract reels (and in the no-reels idle),
  the SWOS logo holds/**fades out**, then each game mode's high-score table
  cross-fades in and out in turn (with flashing INSERT COIN / CREDIT–PUSH START). A
  coin/start/service request returns to the attract loop to act on it.
- **Persistence**: `ARCADE.HI` (a small human-readable text file next to `SWOS.ini`),
  keyed by mode **name** so reordering modes doesn't scramble tables. Seeded with
  placeholder rows when absent, so a fresh cabinet still has something to show.

### CONTINUE? countdown (coin-op continue gate)

`arc_continue_countdown()` — the classic coin-op "**CONTINUE?**" screen, shown the
moment a run ends (the 1P champion loses/draws, or a 2P match finishes), **before**
the GAME OVER / high-score sequence.

- **Look** (matches the reference art): `CONTINUE?` pinned near the top, a flashing
  `INSERT COIN` at the bottom, and a **large countdown digit filling the centre** of
  the 4:3 safe area. The digit is just the *menu font*, nearest-neighbour **scaled**
  so it fills ~60% of the screen height at **any** resolution — no per-size font
  asset. The scale is derived from the glyph's own height, so it tracks the font. It's
  drawn exactly as `DrawMenuText` would (body → white, the font's own drop **shadow**
  → shadow colour, both via the menu palette), only bigger — so it reads as the same
  font as the surrounding text. Rendering reuses the selectors' MENUBG + menu-palette
  compositing; the digit is blitted straight into the RGBA present buffer over the top
  (`arc_big_glyph` / `arc_big_digit_centered`).
- **Timing**: counts **9 → 0, one second per digit** (`ARC_FRAMES_PER_SEC` frames
  each), after a short fade-in. `0` is held for its second, then the screen times out.
- **Input**: the run's modal flag (`s_arcModalRunOver`) is still set, so only **coins**
  register here (start/service are ignored). A coin is detected as an increase over the
  credit count on entry.
- **Outcomes** (both return control to the attract loop; the credit count decides what
  attract shows next):
  - **Coin inserted** → returns `1`; the caller **skips** GAME OVER / high-score entry
    and drops back to attract, which now shows **CREDIT / PUSH START**. (This banks a
    credit for a fresh game; true mid-run *resume* — keep the win streak — is a later
    enhancement. Skipping the initials screen on a continue is deliberate: in coin-op
    convention you only enter initials at a genuine game over.)
  - **Timed out at 0** → returns `0`; the caller runs the normal `arc_run_over` GAME
    OVER + high-score sequence, then returns to attract "from the start".
- **Debug**: the F10 force-name-entry path (`s_arcDebugForceNameEntry`) **skips** the
  countdown so it still lands straight on the name-entry / high-score test flow.
- SDL-only, `#ifdef SWOS_SDL_BUILD` like the rest of `arcade.c`; no shared files
  touched, so the Amiga target is unaffected.

### Debug: F10 screen cycler

`F10` is the arcade debug key (`Platform_ArcadeDebugWin`, edge-triggered), with two
contexts:

- **During a match** — ends the run with a random score and jumps straight to the
  GAME OVER / name-entry flow (unchanged; forces name entry even if it wouldn't
  qualify).
- **From the attract loop** — steps a **screen cycler** (`arc_debug_next_screen`) so
  every result/interstitial screen can be eyeballed **without playing a match**. Each
  F10 press aborts the reel (like a coin does) and shows the **next** screen with
  sample data, wrapping around. The current step is logged to `SWOS.log`
  (`[arcade] DEBUG screen N/7: …`). Dismiss a screen the normal way (fire / timeout /
  END) to return to attract, then press F10 again for the next. The sequence is:

  | # | Screen | Function |
  |---|--------|----------|
  | 0 | CONTINUE? countdown | `arc_continue_countdown` |
  | 1 | WINNER! / WIN STREAK | `arc_continue_screen(5)` |
  | 2 | GAME OVER | `arc_message_screen(…)` |
  | 3 | NAME ENTRY | `arc_name_entry(0,…)` |
  | 4 | HIGH SCORE table | `arc_present_table(mode, …, 0)` |
  | 5 | FLAG SELECT (1P) | `arc_flag_select(1,…)` |
  | 6 | PITCH SELECT | `arc_pitch_select()` |

  The cycler forces the run-over modal flag on for the duration, so a stray coin
  during the demo can't start a real game. SDL-only, no shared files touched.

### Build phasing (this implementation)
1. ✅ DONE (`f2f3ccc`): `[arcade] enabled` + `gArcadeMode` + boot gate (skip
   intro/menu) + attract skeleton (flashing INSERT COIN bottom corners, coin keys
   5/6, F2 stub).
2. ✅ DONE: **Attract reels** — scan `attract_mode/`, random HIL/RPL playback
   free-running (camera-follow as normal, fade in/out between reels) with the SWOS logo
   + the INSERT-COIN overlay; any coin interrupts → match.
3. ✅ DONE: Service menu (F2): SVCFONT loader + basic settings list (mode + arcade rules).
4. ✅ DONE: Arcade match — random `SWCPICH*` pitch (→8bpp) + golden-goal, draw→penalties,
   launched on credit; winner-stays-on run (1P).
5. ✅ DONE: High-score tables + name entry + attract high-score display; goal-replay fix
   (golden goal waits for the celebration + AUTO REPLAYS to finish).
6. ✅ DONE: **CONTINUE? countdown** — 9→0 coin gate on a lost run; coin → attract
   (CREDIT/PUSH START), timeout → GAME OVER / high scores. (`arc_continue_countdown`.)
7. Operator config persistence + bookkeeping; cabinet I/O (coin/service GPIO);
   2P-challenge-mid-streak seat-swap flow; true mid-run *resume* on continue.
