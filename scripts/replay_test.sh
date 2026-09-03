#!/usr/bin/env bash
# Convert a recorded match and replay it against this build (docs/TESTS.md).
#
#   scripts/replay_test.sh                       # the default recording, level info
#   scripts/replay_test.sh --level sprites
#   scripts/replay_test.sh --level teams tests/dos/zkz-11021807.RGD.gz
#   scripts/replay_test.sh --all                 # every level, shortest first
#
# Exit code is the rig's: 0 every frame matched, 2 a divergence, 1 setup failed.
set -uo pipefail
cd "$(dirname "$0")/.."

REC=tests/dos/zkz-12065730.RGD.gz     # v1.5, 1519 frames — the shortest
LEVEL=info
FRAMES=
ALL=0
EXTRA=()

# Sprite fields the DOS corpus cannot agree on at kick-off: swos-port's
# Sprite::init() sets them to values its own source calls "for SWOS++/tests
# compatibility" (and frame_switch_counter's comment says SWOS itself starts at
# 0, which is what we do). save_sprite is 1 in our goal sprites' static data and
# 0 after their init. Muted by default at the sprites level so the run reaches
# real gameplay state; pass --strict to compare them anyway.
INIT_FIELDS='.starting_direction,.frame_switch_counter,.field_3e,.player_direction,.save_sprite'
STRICT=0

while [ $# -gt 0 ]; do
    case "$1" in
        --level)  LEVEL=$2; shift 2 ;;
        --frames) FRAMES=$2; shift 2 ;;
        --all)    ALL=1; shift ;;
        --strict) STRICT=1; shift ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*)       EXTRA+=("$1"); shift ;;
        *)        REC=$1; shift ;;
    esac
done

[ -f "$REC" ] || { echo "no such recording: $REC (see docs/TESTS.md to restore tests/dos)" >&2; exit 1; }
[ -x bin/SWOS ] || { echo "bin/SWOS not built — run make" >&2; exit 1; }

mkdir -p tests/replay
name=$(basename "$REC" .RGD.gz)

run_level() {
    local lvl=$1 out="tests/replay/$name-$1.swr"
    python3 scripts/rgd_convert.py --emit "$out" --level "$lvl" \
        ${FRAMES:+--frames "$FRAMES"} "$REC" || return 1
    local skip=()
    [ "$lvl" = sprites ] && [ "$STRICT" = 0 ] && skip=(--replay-skip="$INIT_FIELDS")
    ( cd bin && ./SWOS --replay-test="../$out" "${skip[@]}" "${EXTRA[@]+"${EXTRA[@]}"}" )
}

if [ "$ALL" = 1 ]; then
    rc=0
    for lvl in scalars teams info sprites; do
        echo "=== level $lvl"
        run_level "$lvl" || rc=$?
    done
    exit $rc
fi

run_level "$LEVEL"
