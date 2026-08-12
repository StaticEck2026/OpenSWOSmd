#!/usr/bin/env python3
"""Quantize the arcade themed-pitch art into indexed BMPs for the DOS pitch pipeline.

The arcade match runs in DOS-graphics mode, where the in-game 256-colour palette is
SHARED: the SWOS DOS sprite/kit/UI colours live in the low 16 slots (0..15) and the
pitch fills the rest. The player kit colours (slots 10/11/14/15) are also recoloured
per-team at match time, so the pitch must NEVER use any of the low 16 slots.

This script therefore:
  * RESERVES slots 0..15 to the SWOS DOS in-game sprite sub-palette
    (SwosGfx DosPalette.Game[0..15]) so players/UI stay correctly coloured;
  * median-cut quantises each pitch image to 240 colours placed in slots 16..255;
  * remaps every pitch pixel into 16..255 only (it can't land on a sprite slot).

Input : full-colour RGB pitch art (672x880). Preferred source is the original
        non-indexed PNGs (pitch-<theme>.png) so the median-cut works on the real
        colours; falls back to bin/grafs_arcade/png/SWCPICH<n>.png.
Output: bin/grafs_arcade/SWCPICH<n>.bmp      (8bpp indexed, palette reserved as above)

Usage : scripts/quantize_arcade_pitch.py [SRC_DIR]
        SRC_DIR defaults to the original-RGB folder below; pass a path to override.

The BMPs are gitignored generated art; re-run this whenever the source art changes.
Reference: C:\\Projects\\GitHub\\SwosGfx\\SwosGfx\\DosPalette.cs (Game palette, pitch
layout) and docs/ARCADE.md (arcade pitch pipeline).
"""
import glob
import os
import sys

from PIL import Image

# SWCPICH index (1..7) -> original-RGB source filename. The pitch-select order in
# src/arcade.c is BEACH, PARK, YARD, SKATE PARK, PRISON, CITY, STADIUM.
SRC_NAMES = {
    1: "pitch-beach.png",   2: "pitch-park.png",    3: "pitch-yard.png",
    4: "pitch-skate.png",   5: "pitch-prison.png",  6: "pitch-city.png",
    7: "pitch-stadium.png",
}
# Default original-RGB source folder (user's art repo). Override via argv[1].
DEFAULT_SRC_DIR = "/mnt/d/Projects/SWOS/OpenSWOS/Arcade/Pitches/New"

# SWOS DOS in-game palette, slots 0..15 (8-bit RGB) — the sprite/kit/UI colours that
# MUST be preserved so players render correctly under the shared DOS palette.
# Verbatim from SwosGfx DosPalette.Game[0..15].
DOS_SPRITE_RGB = [
    (0x70, 0x50, 0x00), (0x98, 0x98, 0x98), (0xFC, 0xFC, 0xFC), (0x00, 0x00, 0x00),
    (0x64, 0x20, 0x00), (0xB8, 0x44, 0x00), (0xFC, 0x64, 0x00), (0x60, 0x50, 0x00),
    (0x00, 0x20, 0x00), (0x50, 0x50, 0x00), (0xFC, 0x00, 0x00), (0x00, 0x00, 0xFC),
    (0x64, 0x00, 0x20), (0x98, 0x98, 0xFC), (0x00, 0xDC, 0x00), (0xFC, 0xFC, 0x00),
]
RESERVED = len(DOS_SPRITE_RGB)        # 16 reserved low slots
PITCH_COLORS = 256 - RESERVED         # 240 pitch slots (16..255)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PNG_FALLBACK_DIR = os.path.join(ROOT, "bin", "grafs_arcade", "png")
OUT_DIR = os.path.join(ROOT, "bin", "grafs_arcade")


def quantize_one(src_path, out_path):
    rgb = Image.open(src_path).convert("RGB")

    # Median-cut the pitch art to 240 colours (PIL indices 0..239).
    pq = rgb.quantize(colors=PITCH_COLORS, method=Image.Quantize.MEDIANCUT)

    # Shift every pixel index up by RESERVED so the pitch occupies slots 16..255
    # and can never collide with a sprite/kit slot. 239 + 16 = 255 (no overflow).
    shift = bytes((i + RESERVED) & 0xFF for i in range(256))
    out_img = Image.frombytes("P", pq.size, pq.tobytes().translate(shift))

    # Full 256-entry palette: reserved sprite colours, then the 240 pitch colours.
    full = []
    for (r, g, b) in DOS_SPRITE_RGB:
        full += [r, g, b]
    full += pq.getpalette()[: PITCH_COLORS * 3]
    full += [0, 0, 0] * (256 - len(full) // 3)
    out_img.putpalette(full)

    out_img.save(out_path)


def resolve_source(idx, src_dir):
    """Prefer the named original-RGB source; fall back to the indexed png."""
    named = os.path.join(src_dir, SRC_NAMES[idx])
    if os.path.isfile(named):
        return named
    fb = os.path.join(PNG_FALLBACK_DIR, f"SWCPICH{idx}.png")
    return fb if os.path.isfile(fb) else None


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC_DIR
    done = 0
    for idx in range(1, 8):
        src = resolve_source(idx, src_dir)
        if not src:
            print(f"SWCPICH{idx}: no source (looked in {src_dir} and {PNG_FALLBACK_DIR})",
                  file=sys.stderr)
            continue
        quantize_one(src, os.path.join(OUT_DIR, f"SWCPICH{idx}.bmp"))
        print(f"wrote SWCPICH{idx}.bmp  <- {os.path.basename(src)}")
        done += 1
    # RANDOM is a selector-only preview (resolved to a real pitch at match time);
    # keep it from the indexed png if present.
    r_src = os.path.join(PNG_FALLBACK_DIR, "SWCPICHR.png")
    if os.path.isfile(r_src):
        quantize_one(r_src, os.path.join(OUT_DIR, "SWCPICHR.bmp"))
        print("wrote SWCPICHR.bmp  (selector preview)")
    return 0 if done else 1


if __name__ == "__main__":
    sys.exit(main())
