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
#   scripts/netplay_test.sh --modes "p2p p2p:2" --stall 8 tests/dos/zkz-11215003.RGD.gz
#
# Modes, each compared with the plain run:
#   direct       the netplay step without GGPO: the input, GameLoop_Frame, the clock, the
#                buffer swap and the VBlank interrupt, all called from outside the loop
#   synctest:N   GGPO's synctest session: every N frames (1..8) it loads the state of N
#                frames back and plays them again. A re-simulated frame whose checksum
#                differs from the first run's fails the run, naming the registry entries
#                that came out different.
#   p2p[:D]      two processes over loopback UDP, each with one player's recorded input and
#                GGPO's prediction of the other's, input delay D (default 0). Each writes
#                every frame as finally played (--netplay-trace); both must equal the plain
#                run. A recording with input in its first D frames cannot -- the delay
#                drops it -- and is reported as skipped.
#
# Options: --modes "<m> <m> ...", --gameplay amiga|dos|both, --jobs N, --out DIR, --all,
#   --stall MS      p2p: each peer pauses now and then, up to MS, like a busy machine
#                   (--netplay-stall); the other peer plays ahead on predicted input and
#                   rolls back far more than on an idle loopback
# Exit code: 0 everything passed, 2 something failed, 1 setup failure.
set -uo pipefail
cd "$(dirname "$0")/.."

MODES="direct synctest:1 synctest:8 p2p"
GAMEPLAY=both
JOBS=$(nproc 2>/dev/null || echo 4)
OUT=tests/netplay
ALL=0
RECS=()
STALL=0
BIN=bin/SWOS.x86_64
DEFAULT_REC=tests/dos/zkz-12065730.RGD.gz    # v1.5, 1519 frames -- the shortest
BASE_PORT=47950
P2P_TIMEOUT=1800

while [ $# -gt 0 ]; do
    case "$1" in
        --modes)    MODES=$2; shift 2 ;;
        --gameplay) GAMEPLAY=$2; shift 2 ;;
        --jobs)     JOBS=$2; shift 2 ;;
        --out)      OUT=$2; shift 2 ;;
        --all)      ALL=1; shift ;;
        --stall)    STALL=$2; shift 2 ;;
        -h|--help)  sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*)         echo "unknown option $1" >&2; exit 1 ;;
        *)          RECS+=("$1"); shift ;;
    esac
done

for m in $MODES; do
    case "$m" in direct|synctest|synctest:[1-8]|p2p|p2p:[0-7]) ;;
        *) echo "bad mode $m (direct|synctest:N|p2p[:D])" >&2; exit 1 ;; esac
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

# run_p2p <tag> <swr> <gameplay> <delay> <port>: the two peers of one match, on ports
# <port> and <port>+1, logged and traced as <tag>-1 and <tag>-2.
run_p2p() {
    local tag=$1 swr=$2 gp=$3 delay=$4 port=$5 p
    for p in 1 2; do
        (
            cd bin && timeout "$P2P_TIMEOUT" \
                ./SWOS.x86_64 --replay-test="../$swr" --dos-gameplay="$gp" --rollback=trace \
                --netplay-match=p2p --netplay-delay="$delay" --netplay-stall="$STALL" --netplay-player=$p \
                --netplay-port=$((port + p - 1)) --netplay-peer=127.0.0.1:$((port + 2 - p)) \
                --netplay-trace="$OUT/$tag-$p.trace" > "$OUT/$tag-$p.log" 2>&1
            echo "[netplay-test] exit $?" >> "$OUT/$tag-$p.log"
        ) &
    done
    wait
}
export -f run run_p2p
export OUT STALL P2P_TIMEOUT

SWRS=()
for rec in "${RECS[@]}"; do
    [ -f "$rec" ] || { echo "no such recording: $rec" >&2; exit 1; }
    swr=$(swr_for "$rec") || { echo "conversion failed: $rec" >&2; exit 1; }
    SWRS+=("$swr")
done

mode_tag() { local m=${1/:/}; echo "${m:-$1}"; }   # synctest:8 -> synctest8, p2p:2 -> p2p2

jobs_file=$(mktemp)
trap 'rm -f "$jobs_file"' EXIT
port=$BASE_PORT
for swr in "${SWRS[@]}"; do
    name=$(basename "$swr" -scalars.swr)
    for gp in "${GPS[@]}"; do
        echo "run plain-$name-$gp $swr $gp --rollback=trace --rollback-trace=$OUT/plain-$name-$gp.trace"
        for m in $MODES; do
            mt=$(mode_tag "$m")
            case "$m" in
            p2p*)
                delay=${m#p2p}; delay=${delay#:}
                echo "run_p2p $mt-$name-$gp $swr $gp ${delay:-0} $port"
                port=$((port + 2)) ;;
            *)
                echo "run $mt-$name-$gp $swr $gp --rollback=trace --rollback-trace=$OUT/$mt-$name-$gp.trace --netplay-match=$m" ;;
            esac
        done
    done
done > "$jobs_file"
xargs -P "$JOBS" -I{} bash -c '{}' < "$jobs_file"

exited_ok() { grep -q '^\[netplay-test\] exit 0$' "$1"; }

fail=0
for swr in "${SWRS[@]}"; do
    name=$(basename "$swr" -scalars.swr)
    for gp in "${GPS[@]}"; do
        ref=$OUT/plain-$name-$gp.trace
        if [ ! -s "$ref" ] || ! exited_ok "$OUT/plain-$name-$gp.log"; then
            echo "  $name-$gp: the plain run failed (see $OUT/plain-$name-$gp.log)"
            fail=1
            continue
        fi
        frames=$(wc -l < "$ref")
        for m in $MODES; do
            mt=$(mode_tag "$m")
            case "$m" in
            p2p*)
                tag=$mt-$name-$gp
                if grep -qs "cannot equal the plain run" "$OUT/$tag-1.log" "$OUT/$tag-2.log"; then
                    echo "  $name-$gp $m: skipped -- the recording has input in the frames the delay drops"
                    continue
                fi
                verdict=identical
                for p in 1 2; do
                    if ! exited_ok "$OUT/$tag-$p.log" || [ ! -s "$OUT/$tag-$p.trace" ]; then
                        verdict=FAILED
                    elif ! cmp -s "$ref" "$OUT/$tag-$p.trace"; then
                        line=$(cmp "$ref" "$OUT/$tag-$p.trace" 2>&1 | sed -n 's/.*line \([0-9]*\).*/\1/p')
                        verdict="DIFFERS (player $p from frame $(( ${line:-1} - 1 )))"
                    fi
                done
                rolls=$(grep -hoE 'player [12]: [0-9]+ frames played, [0-9]+ re-simulated after [0-9]+ rollbacks' \
                            "$OUT/$tag-1.log" "$OUT/$tag-2.log" |
                        sed -E 's/player ([12]): [0-9]+ frames played, ([0-9]+) re-simulated after ([0-9]+) rollbacks/p\1 \3 (\2 frames)/' |
                        paste -sd' ' -)
                if [ "$verdict" = identical ]; then
                    echo "  $name-$gp $m: both peers identical to the plain run ($frames frames); rollbacks $rolls"
                else
                    echo "  $name-$gp $m: $verdict; rollbacks $rolls"
                    for p in 1 2; do
                        grep -hE '^\[netplay\] FAIL|^\[netplay\]   |terminate called|what\(\)' "$OUT/$tag-$p.log" | head -4 | sed "s/^/    player $p: /"
                    done
                    echo "    (see $OUT/$tag-1.log, $OUT/$tag-2.log)"
                    fail=1
                fi ;;
            *)
                tag=$mt-$name-$gp log=$OUT/$tag.log trace=$OUT/$tag.trace
                session=$(grep -hoE '^\[netplay\] (synctest|direct): .*' "$log" | tail -1)
                session=${session#\[netplay\] }
                if ! exited_ok "$log" || [ ! -s "$trace" ]; then
                    echo "  $name-$gp $m: FAILED -- $(grep -hE '^\[netplay\] FAIL|^\[replay\] FAIL' "$log" | head -1)"
                    grep -hE '^\[netplay\]   ' "$log" | head -12
                    echo "    (see $log)"
                    fail=1
                elif cmp -s "$ref" "$trace"; then
                    echo "  $name-$gp $m: $session; identical to the plain run ($frames frames)"
                else
                    line=$(cmp "$ref" "$trace" 2>&1 | sed -n 's/.*line \([0-9]*\).*/\1/p')
                    echo "  $name-$gp $m: $session; DIFFERS from the plain run from frame $(( ${line:-1} - 1 ))"
                    fail=1
                fi ;;
            esac
        done
    done
done

if [ "$fail" = 0 ]; then echo "PASS"; exit 0; else echo "FAIL"; exit 2; fi
