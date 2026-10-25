#!/usr/bin/env python3
"""Where the 68000's time went: mdrun --profile FRAME:prof.bin, symbolised by function.

    profile.py prof.bin build/md/SWOS-MD.elf [N]
"""
import bisect
import struct
import subprocess
import sys

data = open(sys.argv[1], "rb").read()
bins = struct.unpack("<%dI" % (len(data) // 4), data)
nm = subprocess.run(["m68k-linux-gnu-nm", "-n", sys.argv[2]], capture_output=True, text=True).stdout
funcs = []
for line in nm.splitlines():
    f = line.split()
    if len(f) == 3 and f[1] in "tTwW":
        funcs.append((int(f[0], 16), f[2]))
addrs = [a for a, _ in funcs]
tot = {}
for i, c in enumerate(bins):
    if not c:
        continue
    k = bisect.bisect_right(addrs, i * 4) - 1
    name = funcs[k][1] if k >= 0 else "?"
    tot[name] = tot.get(name, 0) + c
total = sum(tot.values())
n = int(sys.argv[3]) if len(sys.argv) > 3 else 40
print("total %d master cycles (%.1f s of 68000)" % (total, total / 7 / 7670454.0))
for name, c in sorted(tot.items(), key=lambda t: -t[1])[:n]:
    print("%6.2f%%  %s" % (100.0 * c / total, name))

# profile.py prof.bin elf N FUNC: the hottest instructions of one function
if len(sys.argv) > 4:
    want = sys.argv[4]
    k = [i for i, (a, n) in enumerate(funcs) if n == want][0]
    lo, hi = funcs[k][0], funcs[k + 1][0]
    dis = subprocess.run(["m68k-linux-gnu-objdump", "-d", "--start-address=%#x" % lo,
                          "--stop-address=%#x" % hi, sys.argv[2]], capture_output=True, text=True).stdout
    rows = []
    for line in dis.splitlines():
        m = line.split(":", 1)
        try:
            a = int(m[0].strip(), 16)
        except ValueError:
            continue
        rows.append((bins[a >> 2] if (a & 3) == 0 else 0, a, m[1].strip()))
    tot_f = tot.get(want, 1)
    for c, a, txt in rows:
        if c * 200 >= tot_f:
            print("%5.1f%%  %06x  %s" % (100.0 * c / tot_f, a, txt[:70]))
