#!/usr/bin/env python3
"""make_jag_graphics.py -- build bin/grafs_jag from SwosGfx's Jaguar conversion.

The Atari Jaguar release of Sensible Soccer redrew the Amiga gameplay art at 256
colours, and SwosGfx's -jag2amiga mode lays it back into the ORIGINAL AMIGA BANK
LAYOUT: same sheets, same cell coordinates, so the game's own image-command
scripts cut the same sprites out of it. What it writes is 8bpp BMPs; what the
game loads is the Amiga's planar bank format. This does that conversion, and
nothing else:

    scripts/make_jag_graphics.py [SRC] [-o bin/grafs_jag]

SRC defaults to SwosGfx's output directory (Amiga_Images_256_Jag). Each bank
becomes a 320x256 sheet of 8 bitplanes -- 8 slabs of 40 bytes per scanline,
81920 bytes -- which the loader recognises as 8bpp from its size alone
(DetectBankPlanes), exactly as it already does for the AGA soccer_s bank.
The sheets' shared 256-colour palette is written beside them as JAG.PAL, 256
RGB triplets, which the game installs into free palette entries at run time
(src/jag_graphics.c, docs/JAGUAR.md).

Only the in-match banks are converted. Everything else -- menus, charset, the
loader pictures -- keeps falling back to grafs_aga/grafs through resload's
existing tier search, so Jaguar mode changes the players and nothing else.
"""
import argparse
import os
import sys

BANKS = ["CJCTEAM1", "CJCTEAM2", "CJCTEAM3", "CJCTEAMG", "CJCBITS", "CJCBENCH"]
WIDTH, HEIGHT, PLANES = 320, 256, 8
ROW_BYTES = WIDTH // 8                      # 40, one bitplane row
DEFAULT_SRC = ("/mnt/c/Projects/GitHub/SwosGfx/SwosGfx/bin/Debug/net8.0/"
               "Amiga_Images_256_Jag")


def to_planar(pixels):
    """One 320x256 8bpp sheet -> Amiga interleaved planar bytes."""
    out = bytearray(HEIGHT * PLANES * ROW_BYTES)
    for y in range(HEIGHT):
        row = pixels[y * WIDTH:(y + 1) * WIDTH]
        base = y * PLANES * ROW_BYTES
        for x, value in enumerate(row):
            if not value:
                continue
            byte, bit = x >> 3, 7 - (x & 7)
            for p in range(PLANES):
                if value & (1 << p):
                    out[base + p * ROW_BYTES + byte] |= 1 << bit
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src", nargs="?", default=DEFAULT_SRC,
                    help="SwosGfx's Jaguar output directory (default: %(default)s)")
    ap.add_argument("-o", "--out", default="bin/grafs_jag", help="where the banks go")
    args = ap.parse_args()

    try:
        from PIL import Image
    except ImportError:
        sys.exit("make_jag_graphics: needs Pillow (pip install pillow)")

    if not os.path.isdir(args.src):
        sys.exit("make_jag_graphics: no such directory: %s\n"
                 "  Run SwosGfx's !Convert_Jag_to_Amiga.bat first." % args.src)
    os.makedirs(args.out, exist_ok=True)

    palette = None
    used = bytearray(256)       # the colours the art actually draws in
    written = 0
    for bank in BANKS:
        path = os.path.join(args.src, bank + ".bmp")
        if not os.path.exists(path):
            print("  skip %-9s (not in %s)" % (bank, args.src))
            continue
        im = Image.open(path)
        if im.mode != "P" or im.size != (WIDTH, HEIGHT):
            sys.exit("make_jag_graphics: %s is %s %dx%d, expected an 8bpp %dx%d sheet"
                     % (bank, im.mode, im.width, im.height, WIDTH, HEIGHT))
        # All the sheets share one palette -- that is what makes a single remap
        # table at run time possible, so a stray sheet must not pass quietly.
        this = im.getpalette()[:768]
        if palette is None:
            palette = this
        elif this != palette:
            sys.exit("make_jag_graphics: %s has a different palette from the other "
                     "banks; the game installs ONE Jaguar palette" % bank)
        pixels = bytes(im.tobytes())
        for value in set(pixels):
            used[value] = 1
        data = to_planar(pixels)
        with open(os.path.join(args.out, bank + ".RAW"), "wb") as f:
            f.write(data)
        print("  %-9s -> %s.RAW  (%d bytes, %d planes)" % (bank, bank, len(data), PLANES))
        written += 1

    if not written:
        sys.exit("make_jag_graphics: no banks found in %s" % args.src)
    # JAG.PAL: the palette AND which of it the art uses, so the game installs
    # exactly those colours in its free entries instead of all 256.
    with open(os.path.join(args.out, "JAG.PAL"), "wb") as f:
        f.write(b"JGP1")
        f.write(bytes(palette))
        f.write(bytes(used))
    print("  palette   -> JAG.PAL   (256 RGB triplets, %d of them used)" % sum(used))
    print("%d bank(s) written to %s" % (written, args.out))


if __name__ == "__main__":
    main()
