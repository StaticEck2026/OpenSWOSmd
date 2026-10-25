#!/bin/sh
# How close a CPU-vs-CPU match runs to full speed (md_isr.c's counters): of the 50 Hz game
# ticks while match frames ran, how many found the frame still being computed ("late").
# 0 late = full speed: every tick got its frame, as on the Amiga.
#   scripts/md/perf.sh [frames] [--pal]
cd "$(dirname "$0")/../.."
ELF=build/md/SWOS-MD.elf
sym() { m68k-linux-gnu-nm "$ELF" | awk -v s="$1" '$3==s{print $1; exit}'; }
build/md/harness/mdrun --core build/md/harness/genesis_plus_gx_libretro.so --rom dist/SWOS-MD.bin \
    --frames "${1:-6000}" $2 --peek 0x$(sym md_lagTicks):1 --peek 0x$(sym md_matchTicks):1 \
    --peek 0x$(sym md_gameFrames):1 | python3 -c '
import sys
v = [int(l.split()[2], 16) for l in sys.stdin if l.startswith("peek")]
late, ticks, frames = v
print("match ticks %d, frames %d, late ticks %d (%.1f%%): %s" % (ticks, frames, late,
      100.0 * late / max(1, ticks), "full speed" if late == 0 else "slow frames"))
sys.exit(1 if late else 0)'
