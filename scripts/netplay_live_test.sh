#!/usr/bin/env bash
#
# netplay_live_test.sh — two processes play a LIVE match against each other.
#
#   scripts/netplay_live_test.sh
#
# The end-to-end check for what the multiplayer menus drive: p2p_link connects,
# the input delay is AGREED, the host sends the match setup, the joiner adopts
# it, and both open a GGPO session and play. Neither touches a pad — the point
# is that two machines given no input still agree frame for frame.
#
# SWOS_NO_LOG=1 is not optional. Without it both processes redirect stderr into
# the SAME ~/.swos/SWOS.log and clobber each other, and the interesting output
# is the part that disappears.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

BIN="${BIN:-SWOS.x86_64}"
OUT="${TMPDIR:-/tmp}/swos-live.$$"
WAIT="${WAIT:-420}"

[ -x "bin/$BIN" ] || { echo "live-test: bin/$BIN not built" >&2; exit 1; }
# Keep the per-side logs on failure -- they are the only record of where a
# stalled run stopped, and deleting them was how the first stall got away.
cleanup() { kill -9 $(jobs -p) 2>/dev/null; }
trap cleanup EXIT INT TERM

# FRAMES bounds the run: a full match is thousands of frames of wall clock, and
# what this test needs is "did the two peers stay identical", which a few
# thousand frames answers. Both peers get the same limit, so both stop together.
FRAMES="${FRAMES:-3000}"

run() {   # role, netem-seed-salt, logfile
    ( cd bin && SWOS_NO_LOG=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        SWOS_LIVE_FRAMES="$FRAMES" \
        SWOS_NETEM_DELAY_MS="${DELAY_MS:-0}" SWOS_NETEM_JITTER_MS="${JITTER_MS:-0}" \
        SWOS_NETEM_LOSS_PCT="${LOSS_PCT:-0}" SWOS_NETEM_SEED="$2$$" \
        ./"$BIN" --netplay-live-pair="$1" ) > "$3" 2>&1
}

echo "-- host"; run host 11 "$OUT.host" & HOST=$!
sleep 2
echo "-- join"; run join 22 "$OUT.join" & JOIN=$!

# Bounded wait: a hung peer must not hang the suite.
( sleep "$WAIT"; kill "$HOST" "$JOIN" 2>/dev/null ) & WATCH=$!
wait "$HOST"; HRC=$?
wait "$JOIN"; JRC=$?
kill "$WATCH" 2>/dev/null
kill -9 "$HOST" "$JOIN" 2>/dev/null   # a peer that ignored SIGTERM must not keep the port

for side in host join; do
    echo "================ $side ================"
    grep -E "^\[live\] (host|join|stopping|final)|^  (ok|FAIL|\.\.)|^\[netplay\]" "$OUT.$side" \
        || tail -5 "$OUT.$side"
done

# The per-frame hashes are INFORMATIONAL ONLY and are expected to differ once
# rollbacks start.
#
# Each is sampled on a frame's FIRST play, which under rollback may be a
# predicted state that is re-simulated moments later -- so the two peers are
# sampled at different points in their own rollback cycles and have no reason to
# match. Treating a difference here as a desync would be crying wolf.
#
# The real checks are GGPO's confirmed-frame checksums (it aborts the match on a
# mismatch, which is how the frame-0 desync was caught) and the FINAL state
# below, taken when every frame is confirmed.
echo "---- per-frame hashes (informational: first-play, not confirmed) ----"
join -j1 \
    <(grep -oE "^\[live\] frame [0-9]+ +hash [0-9a-f]+" "$OUT.host" | awk '{print $3, $5}' | sort -k1,1) \
    <(grep -oE "^\[live\] frame [0-9]+ +hash [0-9a-f]+" "$OUT.join" | awk '{print $3, $5}' | sort -k1,1) \
    2>/dev/null | awk '{ if ($2 != $3) bad++; else same++ }
        END { printf "  %d sampled, %d matched, %d differed (differences here are expected under rollback)\n", same+bad, same, bad }'
echo "======================================"

hh=$(grep -oE "final state hash [0-9a-f]+" "$OUT.host" | awk '{print $4}')
jh=$(grep -oE "final state hash [0-9a-f]+" "$OUT.join" | awk '{print $4}')
echo "host hash: ${hh:-none}"
echo "join hash: ${jh:-none}"

if [ "$HRC" = 0 ] && [ "$JRC" = 0 ] && [ -n "$hh" ] && [ "$hh" = "$jh" ]; then
    echo "live-test: PASSED — both peers played the same match"
    exit 0
fi
echo "live-test: FAILED (host rc=$HRC join rc=$JRC)"
exit 1
