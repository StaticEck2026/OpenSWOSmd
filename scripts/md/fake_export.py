#!/usr/bin/env python3
"""Write an --md-export directory (src/md_export.c's format) from the stand-in art.

    fake_export.py DIR

The real export needs the player's Amiga data, which no checkout has. This writes the
same files from make_md_data.py's stand-in drawings -- with the teams spread over three
countries, as a database is -- so `make -f Makefile.md MD_EXPORT=DIR` exercises the
converter's --export path, and the ROM's country-by-country team selection, without it.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_md_data as md  # noqa: E402


def write_images(path, images, indices):
    with open(path, "wb") as f:
        sel = [i for i in indices if i in images]
        f.write(b"MDSP" + struct.pack(">H", len(sel)))
        for i in sel:
            im = images[i]
            f.write(struct.pack(">Hhhhh", i, im.w, im.h, im.cx, im.cy))
            for row in im.pix:
                f.write(bytes(row))


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    images = md.standin_images()
    write_images(os.path.join(out, "sprites.bin"), images, range(md.NUM_IMAGES))
    kits = md.standin_kits(images)
    for sheet in (1, 2, 3):
        write_images(os.path.join(out, "kit%d.bin" % sheet), kits[sheet], range(*md.KIT_RANGE))
    tiles, pmap = md.standin_pitch()
    for n in range(1, 7):
        with open(os.path.join(out, "pitch%d.bin" % n), "wb") as f:
            f.write(b"MDPT" + struct.pack(">H", len(tiles)))
            for t in tiles:
                f.write(bytes(t))
            f.write(struct.pack(">%dH" % len(pmap), *pmap))
    teams = md.standin_teams()
    recs = []
    for k, rec in enumerate(teams * 3):         # 48 teams: England, Estonia, France
        country = (8, 10, 13)[k // len(teams)]
        rec = bytearray(rec)
        rec[0] = country
        rec[1] = k % len(teams)
        recs.append(bytes(rec))
    with open(os.path.join(out, "teams.bin"), "wb") as f:
        f.write(b"".join(recs))
    print("fake export: %d images, %d pitch tiles, %d teams -> %s" % (
        len([i for i in range(md.NUM_IMAGES) if i in images]), len(tiles), len(recs), out))


if __name__ == "__main__":
    main()
