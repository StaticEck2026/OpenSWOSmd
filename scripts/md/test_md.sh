#!/bin/sh
# The Mega Drive ROM's checks, headless (make -f Makefile.md test). Needs the harness
# (scripts/md/build_harness.sh, built on first use).
#
#   1. attract mode, NTSC and PAL: the title times out into a computer-v-computer match,
#      which then plays on (20,000 frames: several matches, half and full time, the
#      statistics boards). Fails on a panic or exception screen, any write into ROM (a
#      table placed in ROM that the engine writes: lost on a real cartridge), or more than
#      1% of the match's ticks with no new frame to show (the game below full speed).
#   2. the menus with a pad: START, FRIENDLY, PLAY MATCH with pad 1 on team 1, then
#      the kick-off taken and play going -- the match clock has to move.
set -e
cd "$(dirname "$0")/../.."
ELF=build/md/SWOS-MD.elf
ROM=dist/SWOS-MD.bin
H=build/md/harness
[ -x "$H/mdrun" ] || sh scripts/md/build_harness.sh
T=${TMPDIR:-/tmp}/swos-md-test.$$
mkdir -p "$T"
trap 'rm -rf "$T"' EXIT

sym() { m68k-linux-gnu-nm "$ELF" 2>/dev/null | awk -v s="$1" '$3==s{print $1; exit}'; }
LOG=$(sym md_logring); LAG=$(sym md_lagTicks); TICKS=$(sym md_matchTicks)
FRAMES=$(sym md_gameFrames); GT=$(sym _gameTime)
RUN="$H/mdrun --core $H/genesis_plus_gx_libretro.so --rom $ROM --log 0x$LOG"
fail=0

for region in ntsc pal; do
    flag=; [ $region = pal ] && flag=--pal
    $RUN $flag --frames 20000 --peek 0x$LAG:1 --peek 0x$TICKS:1 --peek 0x$FRAMES:1 > "$T/$region.txt" 2>&1 || true
    python3 - "$T/$region.txt" $region <<'PY' || fail=1
import sys
out = open(sys.argv[1]).read()
peeks = [int(l.split()[2], 16) for l in out.splitlines() if l.startswith("peek")]
writes = [l for l in out.splitlines() if l.startswith("rom-write")]
lag, ticks, frames = peeks if len(peeks) == 3 else (0, 0, 0)
bad = []
if "PANIC" in out: bad.append("panic: " + out[out.index("PANIC"):].splitlines()[0])
if not writes or writes[0] != "rom-writes: 0": bad.append("ROM written: " + " ".join(writes[:4]))
if ticks < 10000: bad.append("the attract-mode match did not run (%d match ticks)" % ticks)
elif lag * 100 > ticks: bad.append("%d of %d match ticks late (%.2f%%)" % (lag, ticks, 100.0 * lag / ticks))
print("%s attract mode: %d match ticks, %d late (%.2f%%), %s" % (
    sys.argv[2].upper(), ticks, lag, 100.0 * lag / max(ticks, 1), "FAIL: " + "; ".join(bad) if bad else "ok"))
sys.exit(1 if bad else 0)
PY
done

# the menus: title -> START, main menu FRIENDLY -> START (the friendly screen opens on
# PLAY MATCH, team 1 PLAYER 1), then B held at the kick-off and a run up the pitch
cat > "$T/pad.txt" <<'EOF'
150 0 S
153 0 -
260 0 S
263 0 -
380 0 S
383 0 -
1500 0 B
1530 0 -
1600 0 U
2400 0 -
EOF
$RUN --frames 3000 --input "$T/pad.txt" --peek 0x$GT:2 --peek 0x$TICKS:1 > "$T/menu.txt" 2>&1 || true
python3 - "$T/menu.txt" <<'PY' || fail=1
import sys
out = open(sys.argv[1]).read()
peeks = [l.split()[2:] for l in out.splitlines() if l.startswith("peek")]
bad = []
if "PANIC" in out: bad.append("panic")
if len(peeks) == 2:
    gt = int(peeks[0][0] + peeks[0][1], 16)
    ticks = int(peeks[1][0], 16)
    if ticks < 1000: bad.append("no match after the menus (%d match ticks)" % ticks)
    if gt == 0: bad.append("the match clock never moved")
else:
    bad.append("no peeks")
print("menus -> friendly with pad 1: %s" % ("FAIL: " + "; ".join(bad) if bad else "ok"))
sys.exit(1 if bad else 0)
PY

[ $fail = 0 ] && echo "md tests: all passed" || { echo "md tests: FAILED"; exit 1; }
