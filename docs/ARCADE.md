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
  between them; empty dir = no reels, handle gracefully). Reuses the existing
  (now widescreen-correct) HIL/RPL playback path. **The pitch art is NOT the
  attract background — the reels are.**
- **Flashing "INSERT COIN"** for **player 1 and player 2** (both, bottom corners).
- **Coin insert (MAME convention): key `5` = credit P1, key `6` = credit P2.**
  Any credit interrupts attract → match.
- **Arcade pitches** (`bin/grafs_arcade/SWCPICH1.png … SWCPICH7.png`) are the
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
- **Attract music = `HERO.SNG`** (the title theme). On entering **country (flag)
  select**, switch to **`MENU.SNG`** (the menu music) via `InitMenuMusic` /
  `TryPlayMusic` / `_menuMusic`.
- **Attract RPL/HIL reels also render on the random arcade pitches** — the same
  `SWCPICH*` → 8bpp tile-bank pipeline used for the match, a random pitch per reel
  (the recorded pitch is replaced).

### Game modes

The arcade match runs in a selectable **game mode**, chosen on the F2 service menu's
**MODE** row and persisted as `[arcade] mode=<name>` in `SWOS.ini` (case-insensitive;
unknown/blank falls back to the default). Index 0 is the default.

| Mode | Rules |
|--|--|
| **GOLDEN GOAL** (default) | Short timed match (`game_seconds`, default 45 s). The first goal wins and ends the match immediately; if it's still level at full time the match goes to a penalty shootout (`penalties_on_draw`). |

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

### Build phasing (this implementation)
1. ✅ DONE (`f2f3ccc`): `[arcade] enabled` + `gArcadeMode` + boot gate (skip
   intro/menu) + attract skeleton (flashing INSERT COIN bottom corners, coin keys
   5/6, F2 stub).
2. **Attract reels**: scan `attract_mode/`, random HIL/RPL playback free-running
   (camera-follow as normal, fade in/out between reels) with the SWOS logo + the
   INSERT-COIN overlay; any coin interrupts → match. ← here
3. Service menu (F2): SVCFONT loader + basic settings list (mode + arcade rules).
4. Arcade match: random `SWCPICH*` pitch (→8bpp) + 45 s golden-goal,
   draw→penalties, launched on credit. Winner stays on.
5. Operator config persistence + bookkeeping; cabinet I/O (coin/service GPIO).
