#!/usr/bin/env python3
"""
whdload_sizes.py -- the memory OpenSWOS's WHDLoad slave asks for, from SWOS2 itself.

    whdload_sizes.py bin/SWOS2 obj/whdload_sizes.i

Reads the executable's hunk header and writes, for asm/whdload/OpenSWOS.slave.s:

    SWOS2_CHIP  the MEMF_CHIP hunks, each rounded to 8 as resload_Relocate's
                WHDLTAG_ALIGN will round them: what goes in BaseMem
    SWOS2_FAST  every other hunk, rounded the same: what stays in ExpMem, where
                the file is loaded and relocated over itself
    SWOS2_FILE  the file's size, which must fit where it is loaded

The slave's ws_BaseMemSize and ws_ExpMem are fixed numbers in its header, so they are
worked out from the binary the slave ships with; the slave checks them again at run time.
"""
import struct
import sys


def hunk_sizes(path):
    data = open(path, "rb").read()
    pos = 0

    def word():
        nonlocal pos
        v = struct.unpack(">I", data[pos:pos + 4])[0]
        pos += 4
        return v

    if word() != 0x3F3:
        sys.exit(f"{path}: not an AmigaDOS executable (no HUNK_HEADER)")
    while True:                     # resident library names: none in an executable
        n = word()
        if n == 0:
            break
        pos += n * 4
    count, first, last = word(), word(), word()
    sizes = []
    for _ in range(last - first + 1):
        v = word()
        mem = v >> 30
        if mem == 3:                # extended memory flags follow
            word()
        sizes.append(((v & 0x3FFFFFFF) * 4, mem))
    return sizes, len(data)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sizes, file_size = hunk_sizes(sys.argv[1])
    align = lambda n: (n + 7) & ~7
    chip = sum(align(s) for s, mem in sizes if mem == 1)
    fast = sum(align(s) for s, mem in sizes if mem != 1)
    with open(sys.argv[2], "w") as f:
        f.write(f"; from {sys.argv[1]} by scripts/amiga/whdload_sizes.py -- do not edit\n")
        f.write(f"SWOS2_CHIP = {chip}\n")
        f.write(f"SWOS2_FAST = {fast}\n")
        f.write(f"SWOS2_FILE = {file_size}\n")


if __name__ == "__main__":
    main()
