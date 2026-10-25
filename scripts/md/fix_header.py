#!/usr/bin/env python3
"""Finish a Mega Drive ROM image: pad it, write the ROM end and the checksum.

    fix_header.py rom.bin

Pads to a multiple of 128 KB (what flash carts and most emulators expect), writes the
last ROM address at 0x1A4 and the header checksum (the 16-bit sum of every word from
0x200 to the end) at 0x18E.
"""
import sys


def main():
    path = sys.argv[1]
    data = bytearray(open(path, "rb").read())
    if len(data) < 0x200:
        sys.exit("fix_header: %s is too small to be a ROM" % path)
    size = (len(data) + 0x1FFFF) & ~0x1FFFF
    data += b"\xFF" * (size - len(data))
    if size > 0x400000:
        sys.exit("fix_header: ROM is %d bytes, over the 4 MB the Mega Drive maps" % size)
    data[0x1A4:0x1A8] = (size - 1).to_bytes(4, "big")
    s = 0
    for i in range(0x200, size, 2):
        s = (s + (data[i] << 8 | data[i + 1])) & 0xFFFF
    data[0x18E:0x190] = s.to_bytes(2, "big")
    open(path, "wb").write(data)
    print("%s: %d KB, checksum %04X" % (path, size // 1024, s))


if __name__ == "__main__":
    main()
