#!/usr/bin/env python3
"""Name the ROM tables the game wrote to, from mdrun's "rom-write ADDR from PC PC" lines.

    mdrun ... | rom_writes.py build/md/SWOS-MD.elf

A table in scripts/md/rom_tables.txt that the engine writes is a bug on hardware: the
write is lost. Each one printed here must come off that list (or the write be explained).
"""
import bisect
import re
import subprocess
import sys

elf = sys.argv[1]
nm = subprocess.run([sys.argv[2] if len(sys.argv) > 2 else "m68k-linux-gnu-nm", "-n", "-S", elf],
                    capture_output=True, text=True).stdout
syms = []
for line in nm.splitlines():
    f = line.split()
    if len(f) == 4:
        syms.append((int(f[0], 16), int(f[1], 16), f[3]))
addrs = [s[0] for s in syms]
def name(a):
    i = bisect.bisect_right(addrs, a) - 1
    if i >= 0 and syms[i][0] <= a < syms[i][0] + max(1, syms[i][1]):
        return syms[i][2]
    return "?%06X" % a
def func(pc):
    i = bisect.bisect_right(addrs, pc) - 1
    return syms[i][2] if i >= 0 else "?"
found = {}
for line in sys.stdin:
    m = re.match(r"rom-write ([0-9A-F]+) from PC ([0-9A-F]+)", line)
    if m:
        a, pc = int(m.group(1), 16), int(m.group(2), 16)
        found.setdefault(name(a), set()).add(func(pc))
for n, fs in sorted(found.items()):
    print("%s  (written by %s)" % (n, ", ".join(sorted(fs))))
sys.exit(1 if found else 0)
