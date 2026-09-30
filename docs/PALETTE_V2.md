# Palette v2 — two banks, one shared block

> **Status (2026-09-30): plan, step 1 of §7 done.** The shading options exist
> (`[graphics] outfit_shading / shadow_shading / shade_step`, defaults true / true
> / 2; `--outfit-shading`, `--shadow-shading`, `--shade-step`). Steps 2-5 (the
> bank plane, two-palette upload, crowd LUT to the kit block, SwosGfx remapper)
> are not in the code yet. Meanwhile the Jaguar tier gets its entries by
> **donation** of the palette's duplicate entries (§1), not from the fixed
> `201..239` pool alone (`src/jag_graphics.c`, "THE DONATION MAP"). The Jaguar
> sprites are the default only on the web build (`[graphics] sprites = auto`).

The 256-colour palette is the last thing in this port that is still allocated by
convention rather than by rule, and it has now produced three bugs in a month:
the replay "R" came out in the wrong colours, the 2020 stadium roof turned
yellow, and a goalkeeper's jersey followed the pitch onto a muddy brown. All
three are the same bug — something wrote an entry that belonged to somebody else,
and nothing could have told it not to.

This is the rule that replaces the convention.

---

## 1. What is actually wrong

Measured from the shipped art, not assumed.

**There is no spare room, and the "spare" range was a guess.** The Jaguar sprite
tier installs its colours into entries 201–239. In the AGA tier that range is
empty and it works. In `grafs_2020` every one of those 39 entries is real pitch
art:

| tier | entries 16..239 that nothing draws |
|---|---|
| AGA / Amiga | **163** |
| `grafs_dos` | **0** |
| `grafs_2020` | **0** |

The 2020 pitch draws in 184 distinct indices, up to 255. There is no better range
to move to, because there is no free range at all.

**The palette is full of colours nobody needs.** The DOS palette (which is also
the 2020 palette — SWOS 2020's art was redrawn against the original VGA
palette, it does not ship its own) contains **62 exact duplicate entries**:
`109`, `110`, `147`, `163`, `177`, `178`, `237`, `238` are all `#000000`, the
same as entry 3. So the space exists; it is just spent badly.

**And an index does not say what a colour means.** This is the one that cost a
shipped bug. The sprite art's `220..223` is the keeper's green jersey on
`CJCTEAMG`, dark kit shades on `CJCBENCH`, and absent entirely from the outfield
sheets. Recolouring "the shadow" by index turned the keeper brown. A pixel-level
mapping table said those indices came from Amiga colour 8 — and they do, because
on the keeper sheet colour 8 draws *both* the ground shadow and part of his
jersey, and the Jaguar artists split them.

**There are also two different index spaces, and it is easy to confuse them:**

- the **sheet index** — what the art file stores (the referee is drawn in
  `112..119` on `CJCBENCH`)
- the **palette entry** — what the screen shows (`0x70..0x7F` is the replay-R
  bank)

The sprite tier's remap table sits between them, so those two never actually
collide. But every sentence that says "112 is reserved" is talking about one
space while quoting the other, and that is how the keeper bug got written.

## 2. The rule

**Two palette banks, selected per pixel.**

| bank | holds | entries |
|---|---|---|
| **0 — background** | pitch tiles, scenery, stadium, menu art | 0..239 free |
| **1 — sprites** | players, ball, referee, shadow, HUD glyphs, replay-R | 0..239 free |
| **both** | **the kit block, `240..255`** — identical in both banks | 16, aligned |

That is the whole specification. Each bank gets **240 entries of its own**, and
exactly **one 16-entry block is shared**.

The frame carries one extra byte per pixel saying which bank drew it. The
conversion at the top of the frame becomes `rgba = palette[bank[i]][index[i]]` —
one more load per pixel. The plane defaults to 0 when the pitch is drawn, so only
the sprite blitters set it.

> **`drawPatternFromIndex` is the trap.** The crowd spot-blit draws *pitch tiles*
> through the *sprite* blitter (`src/animated_patterns.c:126`). It must write
> bank 0 explicitly or the fans change palette mid-animation.

### 2.1 Why the kit block is the only shared thing

Because the crowd wears the teams' shirts. Fan tiles are pitch art, and
`pitchFansConvertTable` rewrites exactly two of their colours to the playing
kits (`src/pitch.c` step 4). Pointing those two at the kit block instead of at
base-16 colours is what makes the crowd work across the bank boundary — and it
is why `240..255` must mean the same thing in both banks.

Nothing else crosses. The shadow *reads* the pitch's grass colour at match setup
to tint itself, but that is a value read, not an index reference. Skin and hair
live in the sprite bank's low 16, where the game's own per-player recolour
already puts them. The pitch's low 16 is then just pitch art.

### 2.2 The kit block is 16 entries

`240..255`: four slots of four shades — stripes, shirt, shorts, socks. **Not 32.**
There are four more 4-shade ramps at `224..239` in the Jaguar palette, and they
look like kit slots, but the art does not use them and the same ramp shape
appears at `216..219` and `220..223` too — a 4-shade ramp is that palette's
general vocabulary, not a kit marker. `224..239` is ordinary art space.

One consequence: there is no spare slot, so **the keeper's jersey is not
recolourable**. It stays fixed art, or it uses the shorts and socks slots the way
the Amiga already does.

## 3. The Amiga keeps one palette

AGA gives one CLUT for the whole display; per-pixel bank selection does not
exist. So the m68k target keeps a single partitioned palette — which costs
nothing real, because its tiers are the easy case: the AGA pitch uses 60 indices
(max 108) and its sprite banks are 4bpp. 163 entries are free.

The crowd remap has a harder limit there, and it is physical:

| tier | fan tiles | can a fan tile name entry 240? |
|---|---|---|
| `grafs/` (OCS) | 4 bitplanes | **No** — a 4bpp tile holds 0..15, full stop |
| `grafs_aga/` | 8 bitplanes | Yes, and `240..255` is free in that palette |
| `grafs_dos`, `grafs_2020` | 8bpp chunky | Yes |

So the LUT branches once:

```c
if (pitchTilesAre8bpp)  table[10] = KIT_STRIPES; table[11] = KIT_SHIRT;
else                    table[10] = kitSecondary; table[11] = kitPrimary;
```

On AGA the crowd *could* take the kit block even though its players cannot (4bpp
sprites), giving shaded fans in front of flat-kit players. Allowed, slightly odd,
and optional.

## 4. What the sprite bank holds

Measured across the six Jaguar gameplay sheets — 98 indices in 12 ramps:

| range | px | n | |
|---|---|---|---|
| `1..6`, `9..15` | 14.6k | 13 | the SWOS 16 — skin, hair, greys, **recoloured per player** |
| `29..31` | 6794 | 3 | **the shadow** |
| `48..53`, `61..62` | 52 | 8 | reds |
| `96..103` | 244 | 8 | light greys |
| `106..119` | 493 | 14 | dark greys — **the referee**, black at 119 |
| `145..163` | 478 | 19 | yellows |
| `169..173` | 65 | 5 | browns |
| `181..193` | 455 | 13 | oranges |
| `220..223` | 1127 | 4 | greens — the keeper's jersey |
| `240..255` | 8918 | 16 | the kit block |

Outside the base 16 and the kit block that is **70 colours**, plus the replay-R
bank's 16 → about **85 entries** in a bank that has 240. The headroom is for
custom sprites, which is the point: more shades than the 16-colour game can
express, without ever being able to collide with a pitch.

## 5. Shading, and the options

The kit ramp is currently hard-coded to the Jaguar's own ratios —
`{16, 13, 9, 6}/16`, so the darkest shade is **37.5%**. Those ratios were chosen
for the Jaguar's own art, not for SWOS kit colours, and a near-white sock shaded
to 37.5% comes out dark grey. That is a real complaint, not a taste.

```ini
[graphics]               ; (planned here as [jaguar]; shipped under [graphics])
outfit_shading = true    ; false = flat kit, exactly the Amiga look
shadow_shading = true    ; false = one shadow colour
shade_step     = 2       ; sixteenths per shade: 16, 16-N, 16-2N, 16-3N
```

`shade_step = 2` gives `{16, 14, 12, 10}` — 100 / 87.5 / 75 / 62.5% — and white
socks stay white. `3` reproduces roughly today's look. Both shading flags off
gives the original Amiga contrast exactly, which is the point: **the Jaguar
sprites become the default renderer, and the 16-colour look becomes a setting**,
not a separate asset tier.

Command line without a tier prefix, since these stop being Jaguar-specific:
`--outfit-shading=off`, `--shadow-shading=off`, `--shade-step=N`.

## 6. What the exporter has to do

SwosGfx grows a **remapper**, not just a quantizer:

1. **Reduce** a tier's art to what its bank allows (240 entries, which every
   existing tier already fits — the 2020 pitch's 182 unique colours need no
   reduction at all).
2. **Remap** each pixel to its v2 index: exact match where the colour already
   exists, nearest otherwise.
3. **Refuse** anything that lands in `240..255` unless it is a kit pixel, and say
   which file and how many pixels. A tool that warns is better than one that
   fails, but this one is a hard error — it is the invariant the whole document
   exists to protect.
4. **Report** per file: colours in, colours out, mean and worst error.

Fan tiles keep their two kit tones and get remapped to `240`/`244` (shade 0 of
stripes and shirt). Re-authoring them with four tones each — so the crowd shades
like the players do — is a later, separate step, and needs new art rather than a
remap: the existing tiles only carry two tones.

## 7. Order of work

| | |
|---|---|
| 1 | `shade_step`, `outfit_shading`, `shadow_shading` — small, and people are asking |
| 2 | The bank plane and the two-palette upload; everything keeps working, nothing moves yet |
| 3 | The crowd LUT to `240`/`244`, with the OCS branch |
| 4 | SwosGfx remapper; re-export the tiers |
| 5 | Retire the sprite tier's pool allocator — with two banks it has nothing left to do |

Step 5 is the measure of whether this worked: the code that picks "free" entries,
the volatile-entry exclusions and the whole idea of a pool all delete, because a
sprite can no longer be in a pitch's way.

## 8. Where this ends: truecolour pitches — PARKED

> **Parked 2026-09-22** on the technical challenges below — chiefly weather,
> which has no handle without a palette, and fades, which stop being a palette
> operation at all. Kept because the analysis is the reason, and because alpha
> has to come from somewhere: for now that is `docs/SHADERS.md`.


Once the pitch has a bank of its own, the obvious next question is why it needs a
palette at all. It does not — and the answer is better than "more colours".

**Per layer, a v2 pitch declares its format: `indexed` or `rgba`.** The crowd
layer stays indexed, because its pixels are recoloured to the playing kits and
that is an index operation. Everything else — grass, stands, roofs, clouds — can
be truecolour, with no palette constraint whatsoever.

Three things fall out, and the third is the one worth having:

- **The pitch's palette problem disappears entirely.** The 2020 art's 182
  colours stop being a budget. New pitch art can be photographic.
- **Per-layer alpha becomes real.** `docs/PITCH_FORMAT_V2.md` §7.1 lists the
  `fade` track as having no renderer, because blending two palette indices means
  a 64 KB LUT or a GPU. An RGBA layer just blends.
- **True blended shadows, with no shader.** This thread started with "can we
  blend the shadow with the background like a real shadow?", and the answer was
  no — a shader cannot recover a background the sprite has already overwritten,
  and a CPU blend can only reach colours the palette happens to hold. **If the
  destination is RGBA, a shadow pixel is a multiply.** Not an approximation, not
  a nearest-entry search: the actual thing, on any pitch, in any condition.

Shape: the composite target becomes an RGBA surface. Indexed layers and sprites
still draw through the bank plane and resolve at the top of the frame; RGBA
layers write it directly. The Amiga keeps indexed everything, as it must.

**Three things that need answers before this is designed properly:**

1. **Weather.** The pitch nine is how SWOS makes a pitch muddy or frozen —
   nine palette entries rewritten per condition. Truecolour art has no such
   handle, so either it ships seven variants of every pitch (seven times the
   data) or a per-condition tint is applied at load, which is a multiply over the
   layer and is probably the better answer anyway.
2. **Fades.** `fade.c` ramps the palette, and an RGBA surface does not fade with
   it. Either the fade becomes a multiply over the composite, or it stops being a
   palette operation at all.
3. **The shadow's reference.** It currently tints itself from the grass *entry*.
   With no palette there is no such entry, so a v2 pitch declares its
   representative grass colour in metadata — which it should arguably do anyway,
   since that is a fact about the pitch rather than about its palette.

Cost is the honest part: the pitch fill goes from one byte a pixel to four —
91 KB a frame to 366 KB. Nothing on a desktop, measurable on WASM and Switch,
and worth measuring there before committing.

## 9. Related

- `docs/PITCH_FORMAT_V2.md` — the pitch format this pairs with. Its layers are
  where per-layer palettes would live, and its animation system is the reason the
  bank plane is worth having.
- `docs/JAGUAR.md` — the sprite tier as it stands today, including the pool this
  document retires.
- `docs/SHADERS.md` — the GPU path, where "two palettes and a mask" is native
  rather than an extra byte per pixel.
