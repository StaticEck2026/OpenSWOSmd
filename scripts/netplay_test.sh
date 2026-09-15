#!/usr/bin/env bash
# Netplay match test, headless (docs/NETPLAY.md, "Playing a match through GGPO").
#
# Plays recorded matches through the netplay frame step (src/netplay_match.c) and
# requires each to be the match the replay rig plays: every frame's state hashes equal
# to a plain run's (--rollback=trace). A recording supplies the setup and both players'
# inputs.
#
#   scripts/netplay_test.sh                          # the shortest recording, both gameplays
#   scripts/netplay_test.sh --all                    # every recording in tests/dos
#   scripts/netplay_test.sh --modes synctest:1 tests/dos/zkz-11215003.RGD.gz
#
# Modes, each compared with the plain run:
#   direct       the netplay step without GGPO: the input, GameLoop_Frame, the clock, the
#                buffer swap and the VBlank interrupt, all called from outside the loop
#   synctest:N   GGPO's synctest session: every N frames (1..8) it loads the state of N
#                frames back and plays them again. A re-simulated frame whose checksum
#                differs from the first run's fails the run, naming the registry entries
#                that came out different.
#
# Options: --modes "<m> <m> ...", --gameplay amiga|dos|both, --jobs N, --out DIR, --all.
# Exit code: 0 everything passed, 2 something failed, 1 setup failure.
set -uo pipefail
cd "$(dirname "$0")/.."

MODES="direct synctest:1 synctest:8"
GAMEPLAY=both
JOBS=$(nproc 2>/dev/null || echo 4)
OUT=tests/netplay
ALL=0
RECS=()
BIN=bin/SWOS.x86_64
DEFAULT_REC=tests/dos/zkz-12065730.RGD.gz    # v1.5, 1519 frames -- the shortest

while [ $# -gt 0 ]; do
    case "$1" in
        --modes)    MODES=$2; shift 2 ;;
        --gameplay) GAMEPLAY=$2; shift 2 ;;
        --jobs)     JOBS=$2; shift 2 ;;
        --out)      OUT=$2; shift 2 ;;
        --all)      ALL=1; shift ;;
        -h|--help)  sed -n '2,23p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*)         echo "unknown option $1" >&2; exit 1 ;;
        *)          RECS+=("$1"); shift ;;
    esac
done

for m in $MODES; do
    case "$m" in direct|synctest|synctest:[1-8]) ;; *) echo "bad mode $m (direct|synctest:N)" >&2; exit 1 ;; esac
done
case "$GAMEPLAY" in amiga) GPS=(off) ;; dos) GPS=(on) ;; both) GPS=(off on) ;;
    *) echo "bad --gameplay $GAMEPLAY (amiga|dos|both)" >&2; exit 1 ;; esac
[ -x "$BIN" ] || { echo "$BIN not built -- make X86_64=1" >&2; exit 1; }
if [ "$ALL" = 1 ]; then
    RECS=(tests/dos/*.RGD.gz)
elif [ ${#RECS[@]} -eq 0 ]; then
    RECS=("$DEFAULT_REC")
fi

mkdir -p "$OUT" tests/replay
OUT=$(cd "$OUT" && pwd)                     # the runs start in bin/: absolute paths only

# Inputs are all a run needs from a recording: the smallest .swr, "scalars", reused on
# the same rule as scripts/replay_test.sh and scripts/rollback_test.sh.
swr_for() {
    local rec=$1 name out
    name=$(basename "$rec" .RGD.gz)
    out=tests/replay/$name-scalars.swr
    if [ ! -f "$out" ] || [ "$rec" -nt "$out" ] || [ scripts/rgd_convert.py -nt "$out" ] ||
       [ scripts/rgd_schema.py -nt "$out" ]; then
        python3 scripts/rgd_convert.py --emit "$out" --level scalars "$rec" >&2 || return 1
    fi
    echo "$out"
}

# run <tag> <swr> <dos-gameplay on|off> <extra args...>: one headless match, logged to
# $OUT/<tag>.log with its exit code on the last line.
run() {
    local tag=$1 swr=$2 gp=$3; shift 3
    ( cd bin && ./SWOS.x86_64 --replay-test="../$swr" --dos-gameplay="$gp" "$@" ) \
        > "$OUT/$tag.log" 2>&1
    echo "[netplay-test] exit $?" >> "$OUT/$tag.log"
}
export -f run
export OUT

SWRS=()
for rec in "${RECS[@]}"; do
    [ -f "$rec" ] || { echo "no such recording: $rec" >&2; exit 1; }
    swr=$(swr_for "$rec") || { echo "conversion failed: $rec" >&2; exit 1; }
    SWRS+=("$swr")
done

jobs_file=$(mktemp)
trap 'rm -f "$jobs_file"' EXIT
for swr in "${SWRS[@]}"; do
    name=$(basename "$swr" -scalars.swr)
    for gp in "${GPS[@]}"; do
        echo "run plain-$name-$gp $swr $gp --rollback=trace --rollback-trace=$OUT/plain-$name-$gp.trace"
        for m in $MODES; do
            mt=${m/:/}
            echo "run $mt-$name-$gp $swr $gp --rollback=trace --rollback-trace=$OUT/$mt-$name-$gp.trace --netplay-match=$m"
        done
    done
done > "$jobs_file"
xargs -P "$JOBS" -I{} bash -c '{}' < "$jobs_file"

fail=0
for swr in "${SWRS[@]}"; do
    name=$(basename "$swr" -scalars.swr)
    for gp in "${GPS[@]}"; do
        ref=$OUT/plain-$name-$gp.trace
        if [ ! -s "$ref" ] || ! grep -q '^\[netplay-test\] exit 0$' "$OUT/plain-$name-$gp.log"; then
            echo "  $name-$gp: the plain run failed (see $OUT/plain-$name-$gp.log)"
            fail=1
            continue
        fi
        for m in $MODES; do
            mt=${m/:/}
            tag=$mt-$name-$gp log=$OUT/$tag.log trace=$OUT/$tag.trace
            session=$(grep -hoE '^\[netplay\] (synctest|direct): .*' "$log" | tail -1)
            session=${session#\[netplay\] }
            if ! grep -q '^\[netplay-test\] exit 0$' "$log" || [ ! -s "$trace" ]; then
                echo "  $name-$gp $m: FAILED -- $(grep -hE '^\[netplay\] FAIL|^\[replay\] FAIL' "$log" | head -1)"
                grep -hE '^\[netplay\]   ' "$log" | head -12
                echo "    (see $log)"
                fail=1
            elif cmp -s "$ref" "$trace"; then
                echo "  $name-$gp $m: $session; identical to the plain run ($(wc -l < "$ref") frames)"
            else
                line=$(cmp "$ref" "$trace" 2>&1 | sed -n 's/.*line \([0-9]*\).*/\1/p')
                echo "  $name-$gp $m: $session; DIFFERS from the plain run from frame $(( ${line:-1} - 1 ))"
                fail=1
            fi
        done
    done
done

if [ "$fail" = 0 ]; then echo "PASS"; exit 0; else echo "FAIL"; exit 2; fi
