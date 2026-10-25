#!/bin/sh
# The 68000 time a match frame costs: busy cycles (everything but the end-of-frame wait)
# per game frame, over frames FROM..TO of a CPU-vs-CPU match. The NTSC budget is one VBlank,
# 128K cycles, less the VBlank's DMA; PAL's is 152K.
#   scripts/md/bench.sh [from] [to] [--pal]
cd "$(dirname "$0")/../.."
FROM=${1:-1000}; TO=${2:-6000}
ELF=build/md/SWOS-MD.elf
G=$(m68k-linux-gnu-nm "$ELF" | awk '$3=="md_gameFrames"{print $1; exit}')
RUN="build/md/harness/mdrun --core build/md/harness/genesis_plus_gx_libretro.so --rom dist/SWOS-MD.bin $3"
F0=$($RUN --frames "$FROM" --peek 0x$G:1 | awk '/^peek/{print $3}')
F1=$($RUN --frames "$TO" --peek 0x$G:1 --profile "$FROM:build/md/bench.prof" | awk '/^peek/{print $3}')
python3 - "$F0" "$F1" build/md/bench.prof "$ELF" <<'PY'
import struct, subprocess, sys, bisect
f0, f1 = int(sys.argv[1], 16), int(sys.argv[2], 16)
data = open(sys.argv[3], "rb").read()
bins = struct.unpack("<%dI" % (len(data) // 4), data)
nm = subprocess.run(["m68k-linux-gnu-nm", "-n", sys.argv[4]], capture_output=True, text=True).stdout
idle = [int(l.split()[0], 16) for l in nm.splitlines() if l.endswith(" MD_FrameDone")][0]
funcs = sorted(int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3 and l.split()[1] in "tT")
nxt = funcs[bisect.bisect_right(funcs, idle)]
tot = sum(bins)
idl = sum(bins[idle >> 2: nxt >> 2])
frames = (f1 - f0) & 0xFFFF
print("%d frames, %.0f busy 68000 cycles per frame (%.0f%% of an NTSC VBlank)" % (
    frames, (tot - idl) / 7.0 / frames, 100.0 * (tot - idl) / 7.0 / frames / 127840))
PY
