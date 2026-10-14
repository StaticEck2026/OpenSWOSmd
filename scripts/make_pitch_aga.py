#!/usr/bin/env python3
"""Convert a PC pitch (PITCHn.DAT + PITCHn.BLK) to the Amiga AGA pitch map (SWCPICHn.MAP).

The PC pitch is a tile map -- 2310 little-endian u32 byte offsets into the tile bank,
one per 16x16 cell of the 42 x 55 grid -- and a bank of 256-byte chunky tiles. The AGA
map is the same two parts in the Amiga's form: the 2310 offsets big-endian, then each
tile as 8 interleaved bitplanes, a row at a time (plane 0's word, plane 1's ... plane
7's: 16 bytes a row, 256 a tile). src/pitch.c (FixupAndColorPitchFile) reads it; the
colours are the PC palette's indices, which is the AGA build's palette too.

    python3 scripts/make_pitch_aga.py --check        # the DOS pitches must reproduce
                                                     # bin/grafs_aga/SWCPICH1-6.MAP exactly
    python3 scripts/make_pitch_aga.py grafs_2020 P2020_  # -> bin/grafs_aga/P2020_1.MAP ... (unpacked)

Output is NOT packed. A packed map is loaded through LoadFileEx, which stages the packed
file in the frontbuffer, so it must fit 104,832 bytes; the 2020 pitches pack to 80-137
KB and do not all fit. The game reads them straight into the pitch buffer instead
(src/pitch.c, loadPitchMapRaw), and the buffer is sized to the largest
(PITCHFILE_MAX_SIZE, include/display.h) -- checked here.
"""
import os
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "scripts"))
from swos_team_file import rnc_unpack  # noqa: E402  (to read the shipped maps for --check)

CELLS = 42 * 55
TILE = 256
BUFFER = 544 * 1024             # PITCHFILE_MAX_SIZE on the AGA Amiga (include/display.h)


def pc_files(tier, n):
    d = os.path.join(REPO, "bin", tier)
    names = {f.lower(): f for f in os.listdir(d)}
    blk = open(os.path.join(d, names["pitch%d.blk" % n]), "rb").read()
    dat = open(os.path.join(d, names["pitch%d.dat" % n]), "rb").read()
    return dat, blk


def convert(dat, blk):
    if len(dat) != CELLS * 4 or len(blk) % TILE:
        raise ValueError("not a PC pitch: DAT %d bytes, BLK %d bytes" % (len(dat), len(blk)))
    offsets = struct.unpack("<%dI" % CELLS, dat)
    if max(offsets) + TILE > len(blk):
        raise ValueError("the map points past the tile bank")
    out = bytearray()
    for o in offsets:
        out += struct.pack(">I", o)
    for t in range(len(blk) // TILE):
        tile = blk[t * TILE:(t + 1) * TILE]
        for row in range(16):
            px = tile[row * 16:(row + 1) * 16]
            for plane in range(8):
                w = 0
                for x in range(16):
                    w |= ((px[x] >> plane) & 1) << (15 - x)
                out += struct.pack(">H", w)
    return bytes(out)


def main(argv):
    if argv[1:] == ["--check"]:
        bad = 0
        for n in range(1, 7):
            shipped = rnc_unpack(open(os.path.join(REPO, "bin", "grafs_aga", "SWCPICH%d.MAP" % n), "rb").read())
            same = convert(*pc_files("grafs_dos", n)) == shipped
            bad += not same
            print("pitch %d: %s" % (n, "identical" if same else "DIFFERS"))
        return 1 if bad else 0
    if len(argv) != 3:
        print(__doc__)
        return 2
    tier, prefix = argv[1], argv[2]
    over = 0
    for n in range(1, 7):
        raw = convert(*pc_files(tier, n))
        path = os.path.join(REPO, "bin", "grafs_aga", "%s%d.MAP" % (prefix, n))
        open(path, "wb").write(raw)
        fits = len(raw) <= BUFFER
        over += not fits
        print("%s: %d tiles, %d bytes%s" % (os.path.basename(path), (len(raw) - CELLS * 4) // TILE,
              len(raw), "" if fits else "  -- OVER the %d-byte pitch buffer" % BUFFER))
    return 1 if over else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
