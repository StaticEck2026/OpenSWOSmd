# Amiga in-match sprites: hardware sprites + bobs

Status (2026-07-11): **investigation + plan. Implementation (P3) pending.**
Prereq: AGA mixed-depth banks are decoded 4bpp — see [AGA.md](AGA.md).

Reference: SWOS's hardware-sprite/bob switching (and the ENGINE9000 debugger capture
of the DMA budget quoted in §1) is analysed in this video —
<https://www.youtube.com/watch?v=GeVrtYfFa_Y>.

SWOS renders the on-pitch objects (players, ball, keepers) with an **elaborate
switch between Amiga hardware sprites and blitter bobs**, chosen per sprite per
frame. The C port (`src/sprites.c`) already ports this system; it was fed
**8-plane (256-colour) sprite data**, which is the wrong shape for hardware sprites.
Now that sprites are confirmed **16-colour (4bpp)** they fit the hardware-sprite
path natively — that is the intended speedup.

---

## 1. Why switch between hardware sprites and bobs?

The blitter is saturated scrolling the 256-colour pitch (a live capture shows
**Blitter ~10312 DMA slots/frame vs Sprite ~16 slots/frame**). Amiga hardware
sprites are almost free — DMA'd and composited by the chip, and they **move
without a redraw** (no per-frame blit, no background save/restore). So SWOS puts as
many players as it can onto hardware sprites and only blits the rest.

Hardware-sprite constraints force the switching:
- Only **8** sprites; SWOS uses them as **attached pairs** → **16 colours**, and
  in this usage each band sprite is **≤16px wide**.
- A sprite channel shows **one object per scanline**, so a channel is **reused down
  the screen** (re-pointed via the copper to the next player below) — which only
  works if the players in that channel **don't overlap vertically**.
- A hardware sprite gets transparency free (colour 0 = transparent) and composites
  over the 256-colour playfield regardless of what's beneath — **no plane-clearing**.

So: narrow, non-overlapping players → **hardware-sprite bands** (free); wide players
or ones that would overlap a band sprite → **bobs** (blitter). This keeps the
blitter budget for the pitch.

### Why a bob is *not* cheaper at 4bpp (the fallback path)
A bob is cookie-cut into the **shared 8-plane playfield**. A 16-colour bob pixel
still needs planes 4-7 **cleared** where it's opaque (else colour =
`sprite_low4 | pitch_high4`). So a bob blit touches all 8 dest planes regardless of
sprite depth — 4bpp only trims source reads + the upper-plane clear (~10-20%). The
big win is hardware sprites, not cheaper bobs. (This is why the fallback stays; the
speedup comes from moving most players *off* the blitter.)

## 2. The render pipeline (`src/sprites.c`)

```
DrawSprites (1623)
 ├─ SortSprites (376)                          sort by Y
 ├─ BuildSpriteBandAndGeneralRenderStacks (1416)   route each sprite → band or bob
 ├─ RenderSpriteBandsToCopperLists (1297)      band sprites → copper sprite list
 └─ DrawQueuedSprites                          bob sprites → blitter cookie-cut
```

### Routing — `BuildSpriteBandAndGeneralRenderStacks` (SWOS2.s:7130)
Walks sorted sprites, culls off-screen, then per sprite:
- **Amiga**: `if (_overlayActiveFlag || pixWidth > 16)` → general (bob) stack, and
  it updates the **occlusion buckets** (16px columns tracking each column's min Y).
  Otherwise it tries to place the sprite in a band: it fits if its top is on-screen
  and its bottom `y+nlines` is above the band's current top (`plane23BandTopY` then
  `plane01BandTopY`) **and** clears the occlusion buckets under it (no overlap). On
  success `sprite->field_56` = 1 (plane01) or 2 (plane23); overflow → general stack.
- **SDL**: `if (1)` — **forces every sprite to the general (bob) stack**, because the
  copper/hardware-sprite path is a no-op on the chunky SDL renderer.

`field_56` (0 = bob, 1 = plane01 band, 2 = plane23 band) records the decision.

### Band → copper — `renderSpriteBandPlanePair` (SWOS2.s:7069) / `RenderSpriteBandsToCopperLists` (7007)
Builds an **attached sprite pair** (16 colours): planes 0/1 → sprite channel A,
planes 2/3 → channel B (`srcPlane2 = sprData + 4`), sets the **ATTACH bit**
(`ctl | 0x80`, `sprites.c:1265`), and programs VPOS/HPOS/VSTOP for the sprite's
screen position (display offsets `x+0x70`, `y+0x24`). No mask — hardware
transparency. The plane-pair copy stride is:

```c
sprites.c:1317:  BLT_W(_custom->bltamod, (NUM_PLANES - 2) * 2);   // 12 for 8-plane; must be 4 for 4-plane
```

### Bob — `DrawSpriteClipped` / the cookie-cut path (`sprites.c:515+`)
Cookie-cut blit into the playfield: `BLTCON0_BASE_LINKED` (`D = mask ? sprite : dest`),
`SPRITE_BLTSIZE = 15 × NUM_PLANES` lines (all 8 planes), mask from `sprData`'s
composed mask (`maskPtr`). Background is saved/restored via the sprite blit queue
(`ProcessSpriteBlitQueue`, `display.c:901`).

## 3. Where the 256-colour assumption bit — and the P3 fix

Both the storage and the band stride assume `NUM_PLANES` (=8) planes per sprite row:
- `blitDispatch` Amiga branch (`image_cmd.c:227,240`) stores `height × NUM_PLANES`
  plane-rows and composes an 8-plane mask.
- `RenderSpriteBandsToCopperLists` uses `bltamod = (NUM_PLANES-2)*2 = 12`.

With real 256-colour sprites the bands could only ever grab planes 0-3 (wrong
colours); with the new 4bpp banks the `×8` stride reads past each 8-byte row →
garbage. **The fix restores the original design:**

1. **Ungate `DetectBankPlanes` for Amiga** and capture the load size in the Amiga
   `0x7000`/`0x8000` branches so `s_bankSrcPlanes` is right on the Amiga build too.
2. **`blitDispatch` Amiga branch: store `height × srcPlanes` plane-rows** (4 for
   players). The **mask is only needed for the bob path**; band sprites use hardware
   transparency.
3. **Band stride = the sprite's plane count**: `bltamod = (srcPlanes-2)*2` = 4 for
   the 16-colour band sprites (a whole band is uniform 4-plane, so this can be a
   band-level constant).
4. **`soccer_s` (ball, 8bpp)** can't be a 16-colour hardware sprite → stays a bob.

Result: narrow players become **native 16-colour attached hardware sprites** —
free, no plane-clear, composited over the 256-colour pitch — offloading the blitter.

### The dual-format wrinkle
A given sprite can be a **band one frame and a bob the next** (its overlap/position
changes). So the stored sprite data + mask must serve both:
- Band use consumes 4 interleaved planes (no mask).
- Bob use needs the cookie-cut mask and, into the 8-plane playfield, planes 4-7
  cleared. Decide whether the stored data is 4-plane (band-native; the bob path pads
  / clears on blit) — this is the main design call to settle during P3.

## 4. Verifying on Amiga (now possible)

Build the Amiga AGA build, then FS-UAE + gdb:
```
scripts/amiga/amiga_gdb.sh up
python3 scripts/amiga/gdb_proxy.py cmd "print field_56 of a player"   # band vs bob
python3 scripts/swos_harness.py where                                  # drive into a match
```
Watch `Sprite` vs `Blitter` DMA (fewer bobs = more players on sprite DMA), and
inspect `field_56` / band top-Y state to confirm players are landing in bands.

## 5. Status / open questions

- The band system exists and is correct in shape (attached pairs, occlusion
  multiplexing, band/bob routing). Only the **plane count** is wrong (8 vs 4).
- Confirm intended layout with the owner: 4-plane band sprites + `soccer_s`/ball as
  an 8bpp bob; and the dual-format storage decision (§3 wrinkle).
- All changes are in the Amiga (`#else`) branches; SDL keeps its all-bobs chunky
  path. Test on FS-UAE.
