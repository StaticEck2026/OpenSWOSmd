#!/usr/bin/env python3
"""Generate the Nintendo Switch application icon.

    python3 scripts/make_switch_icon.py

AuthoringTool wants a 1024x1024 24-bit UNCOMPRESSED BMP and rejects anything
else with a confusing message about baseline JPEG; it converts this into the
JPEGs the console actually shows. Kept as a script rather than a committed
binary blob so the icon can be regenerated when the source art changes.

The source is the game's own title logo (bin/grafs_arcade/LOGO.bmp, 320x200,
palettised), scaled with NEAREST so the pixel art stays pixel art, centred on
the black it was drawn against.
"""
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("needs Pillow:  pip install Pillow")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "bin", "grafs_arcade", "LOGO.bmp")
DST = os.path.join(ROOT, "packaging", "switch", "swos_icon.bmp")
SIZE = 1024

def main():
    if not os.path.exists(SRC):
        sys.exit("missing source art: %s" % SRC)

    logo = Image.open(SRC).convert("RGB")
    # Scale to the icon width by a whole number of pixels where possible; the
    # logo is 320 wide and the icon 1024, so 3x (960) leaves a 32px margin
    # either side rather than resampling to a non-integer factor.
    scale = SIZE // logo.width
    scaled = logo.resize((logo.width * scale, logo.height * scale), Image.NEAREST)

    icon = Image.new("RGB", (SIZE, SIZE), (0, 0, 0))
    icon.paste(scaled, ((SIZE - scaled.width) // 2, (SIZE - scaled.height) // 2))

    os.makedirs(os.path.dirname(DST), exist_ok=True)
    icon.save(DST, "BMP")          # Pillow writes 24-bit uncompressed for RGB

    n = os.path.getsize(DST)
    expect = SIZE * SIZE * 3
    print("wrote %s  %dx%d  %d bytes (%d of pixel data)" % (DST, SIZE, SIZE, n, expect))
    if n < expect:
        sys.exit("BMP is smaller than 24-bit uncompressed would be — check the mode")

if __name__ == "__main__":
    main()
