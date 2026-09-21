# The Atari Jaguar sprites (`--jag-graphics`)

Sensible Soccer's 1995 Atari Jaguar release redrew the gameplay art at 256
colours: the same players, the same animations, the same bank layout — every flat
colour turned into a small ramp of shades. SwosGfx converts it back into the
Amiga banks (`-jag2amiga`), which means the game's own image scripts cut the same
sprites out of it, and the whole port of it comes down to one question: **colour**.

```bash
scripts/make_jag_graphics.py            # SwosGfx's BMPs -> bin/grafs_jag
bin/SWOS.x86_64 --jag-graphics=on       # or [jaguar] graphics = true in SWOS.ini
```

It is the **players and the ball**. The pitch, the HUD, the menus and every other
screen are exactly what they were — this is a sprite tier, not a palette swap.
SDL only: an 8bpp bank is twice the chip RAM and twice the blitter bandwidth of
the 4bpp one the A1200 draws today, so the Amiga build does not take it.

## What the art does with colour

Comparing the Jaguar sheets against the 16-colour originals pixel for pixel —
they are the same drawings, so the comparison is exact — every Amiga colour
became either itself or a ramp:

| Amiga colour | | Jaguar |
|---|---|---|
| 1, 2, 3 | greys, black | the same, 1:1 |
| 4, 5, 6 | skin | the same, 1:1 |
| 9, 12, 13 | hair | the same, 1:1 |
| 8 | the drop shadow | 29–31, 220–223 (its own grass-tinted shades) |
| 10 | shirt stripes | **240–243** |
| 11 | shirt | **244–247** |
| 14 | shorts | **248–251** |
| 15 | socks | **252–255** |

So a Jaguar pixel is "*the colour SWOS means*, at *a shade*", and **only the kit
has to follow the team**: four slots of four shades at the top of the palette,
brightest first. Skin and hair need nothing new — the game already varies exactly
those indices per player (`ConvertColorTableToSkinColor` rewrites 4,5,6 and
9,12,13, and no others), and the art kept them. The keeper needs nothing either:
its jersey is drawn in the shorts and socks slots, as on the Amiga.

### Hair, where SWOS throws the shading away

The art draws hair in three shades (13 light, 9 mid, 12 dark) and the game's own
table decides what they become per face type. For a ginger head it keeps three
(`F60 B40 620`), so that hair shades by itself. For a **black** head — face types
0 and 2 — it sends all three to colour 3, and the Jaguar's shading is lost: a flat
block, exactly what the 16-colour game draws.

So when the three collapse onto one colour, this tier shades that colour instead.
Black has nothing darker than it, so the highlight lifts rather than the shadow
deepening: `000000` stays the shadow and the lit side becomes `111111` and
`333333`. The lift is the `k == 0 ? 3 : k == 1 ? 1 : 0` in `JagGfx_TeamTable` —
one constant, if it wants to be stronger or subtler. Hair the game already shades
is left alone.

Skin has a milder version of the same thing: face type 2 sends 6,5,4 to 4,4,3, so
its darkest tone is drawn with two shades rather than three. Left as the game has
it — changing it would move a skin tone, not just its shading.

## It needs almost no palette of its own

The game's 256 colours were converted from the DOS palette, which carries dimmed
copies of the 16 SWOS colours — which is exactly what a kit ramp is. **Every kit
colour's four shades are already in there**: the furthest is 3 units of 4096 from
the shade we would have computed. So are 64 of the art's own 76 colours. The tier
therefore **re-uses** entries instead of installing them:

- a kit shade is the nearest existing entry to colour × {16/16, 13/16, 9/16,
  6/16} — the ratios the Jaguar's own artwork uses (FF, D6, A5, 7B) — and
  **shade 0 is the kit colour itself**, so a Jaguar kit is the colour the Amiga
  draws with shading under it;
- an art colour takes the nearest entry when one is close enough and not already
  spoken for, and only otherwise costs an entry from a small pool (201–239).

In practice: 59 of the art's colours found in the palette, 22 given spare
entries. A very dark kit shades very little and a black one not at all — which is
what the 16-colour game does too.

### The palette is not a scratchpad

This is the part that bit. These entries belong to someone, and the tier must
neither write them nor re-use them:

| Entries | Whose |
|---|---|
| the nine in `_paletteIndices` | the **pitch** — rewritten per pitch condition |
| 0x70–0x7F | the **replay "R"** bank (`soccer_s`, sprites 0x447–0x486) |
| 240–255 | the kit block: what a Jaguar sprite's pixels *mean*, never a destination |

The first version of this tier installed the art's colours from entry 82 upward,
straight through 0x70–0x7F, and the spinning R came out in the wrong colours.
The stadium and crowd backgrounds are 8bpp art that reaches across the palette
too, which is why the pool sits high and small. To check: `SWOS_JAG_DUMP=1`
prints the R bank's entries at kick-off — they must be what the game put there.

## With the DOS graphics

`[dos] graphics` and `[jaguar] graphics` can both be on — DOS pitch and
backgrounds (`grafs_2020`, say) with Jaguar players. Two things make that work,
and neither is automatic:

- **The DOS overlay leaves the players alone.** DOS mode rebuilds the player,
  keeper and bench sprites from its own `.dat` files *after* the banks have
  loaded, which overwrote the Jaguar ones. With the Jaguar tier on it skips them
  and keeps the rest of its look.
- **A DOS match is drawn in the DOS palette**, not the Amiga one
  (`platform_sdl.c`), so this tier matches its colours against whichever palette
  is live — otherwise it picks entries by colours nobody displays. The DOS
  palette covers the art just as well (worst kit shade 3 of 4096, 64 of 76 art
  colours within 2), which is no coincidence: the Amiga palette was converted
  from it. The few colours that need an entry of their own are placed in both
  (`DosGfx_SetExtraColour`).

The mode is read at startup, so the colours are placed for the mode the game
started in.

## How it is wired

- **`scripts/make_jag_graphics.py`** turns SwosGfx's `Amiga_Images_256_Jag` BMPs
  into `bin/grafs_jag/*.RAW`: 320×256 sheets of 8 bitplanes, 8 slabs of 40 bytes
  a scanline. The loader recognises 8bpp from the size alone
  (`DetectBankPlanes`), exactly as it already does for the AGA `soccer_s` bank —
  nothing in the sprite pipeline needed changing. Beside them it writes
  `JAG.PAL`: the sheets' shared palette *and which of it the art draws in*, so
  the game places exactly the colours the art uses and looks at no others.
- **`src/resload.c`** puts `grafs_jag/` in front of `grafs_aga/` when the tier is
  on. It holds the gameplay banks only (`CJCTEAM1/2/3`, `CJCTEAMG`, `CJCBITS`,
  `CJCBENCH`), so everything else falls through to the existing tiers.
- **`src/image_cmd.c`** maps a Jaguar bank's colours into the palette as its
  pixels are decoded; the low 16 and the kit block pass through untouched.
- **`src/color_sprites.c`** turns the 16-entry table the game builds for a team
  into the 256-entry one a Jaguar sprite needs (`JagGfx_TeamTable`), per range,
  because the skin and hair entries change between ranges.
- **`src/jag_graphics.c`** is the colour: which entry each of the art's colours
  lands on, and which four a kit's shades come from.

## Keeping it honest

The tier is off by default and every part of it is behind that flag: with it off,
`JagGfx_Enabled()` is false, the loader searches where it always did, the decode
remap is a null pointer and the recolour is the 16-entry table the game built.

To see it: `--jag-graphics=on --view-replay=<file>.SRP` with
`SWOS_SCREENSHOT_INTERVAL=N` renders a recorded match to PNGs. A player sprite
comes out with about 19 colours in its box where the Amiga has 10, and the kit's
brightest shade equals the Amiga's flat kit colour.

## What is not done

- **The Jaguar's own pitch, menus and HUD.** The Jaguar CLUTs include a pitch
  palette (`clut_6FE48_pitch`), so the art exists on the cartridge; SwosGfx does
  not extract it yet, and the pitch is a different problem from the sprites (its
  colours are patched per pitch condition).
- **The celebration cells.** The Jaguar game has no goal celebration; SwosGfx
  stamps hand-drawn art over those cells, so they are not Jaguar art.
- **`CJCGRAFS`** (the scoreboard font and clock captions) is left to the Amiga
  set, so the HUD keeps its own palette entries.
- **The Amiga target**, for the memory and bandwidth reason above.
