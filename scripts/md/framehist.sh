#!/bin/sh
# Histogram of match frame times in scanlines (md_isr.c md_frameHist): 262 lines is one
# NTSC VBlank, 313 one PAL. A frame past the line misses its tick.
#   scripts/md/framehist.sh [frames] [--pal]
cd "$(dirname "$0")/../.."
H=$(m68k-linux-gnu-nm build/md/SWOS-MD.elf | awk '$3=="md_frameHist"{print $1}')
build/md/harness/mdrun --core build/md/harness/genesis_plus_gx_libretro.so --rom dist/SWOS-MD.bin \
    --frames "${1:-6000}" $2 --peek 0x$H:41 | grep '^peek' | python3 -c '
import sys
v = [int(x, 16) for x in sys.stdin.read().split()[2:]]
h, mx = v[:40], v[40]
for i, c in enumerate(h):
    if c:
        print("%3d-%3d lines: %5d %s" % (i * 16, i * 16 + 15, c, "#" * min(60, (c + 9) // 10)))
print("worst %d lines" % mx)'
