#!/usr/bin/env bash
#
# build_macos.sh — build the macOS .app on a remote Mac over ssh and bring the
#                  zip back into this checkout's bin/.
#
#   scripts/build_macos.sh                # tester build -> bin/SWOS-macOS-app.zip
#   scripts/build_macos.sh --release      # public build (stock injuries + cards)
#   scripts/build_macos.sh --no-pull      # build the Mac's tree as-is
#
# macOS is the one target scripts/build_dist.sh cannot produce: `make macos-app`
# hard-fails unless `uname -s` is Darwin and there is no cross-compile path. So
# this script drives a real Mac instead, and defaults to the SAME flags
# build_dist.sh uses so a drop is uniform across platforms:
#
#   SWOS_TEST_NO_INJURIES_CARDS=1   (tester build; --release turns it off)
#   SWOS_SAVE_FORMAT_JSON_VERIFY=0  (the Makefile's `?= 0` is inside the WASM
#                                    branch, so this defaults ON otherwise)
#
# It `git pull --ff-only`s first and PRINTS THE COMMIT IT BUILT. On 2026-08-17 a
# tester reported a bug fixed on the HTML build and still present on macOS; the
# cause was that the Mac was two commits behind, not anything Darwin-specific.
# Use --no-pull only when you deliberately want the Mac's working tree.
#
# The bundle is packed with `ditto -c -k --sequesterRsrc --keepParent`, NOT
# `zip -r`. SWOS.app/Contents/Frameworks/SDL2.framework is built out of symlinks
# (Versions/Current, and the top-level SDL2 -> Versions/Current/SDL2); plain zip
# follows them, which both bloats the archive and produces a bundle that fails
# to load. ditto preserves symlinks and resource forks.
#
# Overrides: MAC_HOST, MAC_PATH, MAC_ZIP.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

MAC_HOST="${MAC_HOST:-bbaker@192.168.1.37}"
MAC_PATH="${MAC_PATH:-~/Programming/OpenSWOS}"
MAC_ZIP="${MAC_ZIP:-SWOS-macOS-app.zip}"

JOBS=""
NOINJ=1
DO_PULL=1

usage() {
    awk 'NR >= 3 { if ($0 !~ /^#/) exit; sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
    cat <<EOF

Options:
  -j, --jobs N     parallel make jobs (default: the Mac's core count)
      --release    PUBLIC build: stock injuries + cards (default is a tester
                   build with SWOS_TEST_NO_INJURIES_CARDS=1)
      --no-pull    don't git pull on the Mac first
  -h, --help       this text

Environment:
  MAC_HOST   ssh target      (default: $MAC_HOST)
  MAC_PATH   remote checkout (default: $MAC_PATH)
  MAC_ZIP    archive name    (default: $MAC_ZIP)
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        -j|--jobs)  JOBS="$2"; shift 2 ;;
        --release)  NOINJ=0; shift ;;
        --no-pull)  DO_PULL=0; shift ;;
        -h|--help)  usage; exit 0 ;;
        *) echo "build_macos: unknown option '$1' (try --help)" >&2; exit 2 ;;
    esac
done

MAKE_FLAGS="SWOS_SAVE_FORMAT_JSON_VERIFY=0"
[ "$NOINJ" = 1 ] && MAKE_FLAGS="$MAKE_FLAGS SWOS_TEST_NO_INJURIES_CARDS=1"

banner() { echo ""; echo "======================================================================"; echo "== $*"; echo "======================================================================"; }

LOG="$(mktemp -t swos-macos-build.XXXXXX)"
trap 'rm -f "$LOG"' EXIT

banner "preflight: $MAC_HOST"
if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "$MAC_HOST" true 2>/dev/null; then
    echo "build_macos: cannot ssh to $MAC_HOST with key auth." >&2
    echo "  Check the Mac is awake and reachable, and that your key is in its" >&2
    echo "  ~/.ssh/authorized_keys (ssh-copy-id $MAC_HOST)." >&2
    exit 1
fi
echo "-- ssh OK"

banner "build on $MAC_HOST:$MAC_PATH"
# ssh does NOT preserve argv boundaries: it concatenates its arguments into a
# single command string that the REMOTE shell re-splits. So "$MAKE_FLAGS" (two
# words) would arrive as two arguments and shift everything after it — which is
# how `-j` once received "SWOS_TEST_NO_INJURIES_CARDS=1". printf %q quotes each
# argument for re-input to the remote bash. The tilde in MAC_PATH survives as a
# literal (%q escapes it, the remote parser strips the backslash) and is expanded
# by the `eval cd` below.
REMOTE_ARGV="$(printf ' %q' "$MAC_PATH" "$MAKE_FLAGS" "$JOBS" "$MAC_ZIP" "$DO_PULL")"
ssh "$MAC_HOST" "bash -s --$REMOTE_ARGV" 2>&1 <<'REMOTE' | tee "$LOG"
set -uo pipefail
REPO="$1"; FLAGS="$2"; JOBS="$3"; ZIP="$4"; DO_PULL="$5"

eval cd "$REPO" || { echo "!! no checkout at $REPO"; exit 1; }
[ -n "$JOBS" ] || JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

if [ "$DO_PULL" = 1 ]; then
    echo "== git pull --ff-only"
    git pull --ff-only || { echo "!! pull failed (diverged? use --no-pull)"; exit 1; }
fi
echo "== HEAD: $(git log -1 --format='%h %ad %s' --date=short)"
DIRTY="$(git status --porcelain --untracked-files=no)"
[ -n "$DIRTY" ] && { echo "== WARNING: tracked files modified — this build is NOT $(git rev-parse --short HEAD) exactly:"; echo "$DIRTY"; }

echo "== make macos-app $FLAGS -j$JOBS"
# macos-app depends on macos-universal, which runs `make clean` between the
# x86_64 and arm64 slices itself — no manual clean needed. Command-line
# variables reach those recursive sub-makes via MAKEFLAGS.
make macos-app $FLAGS -j"$JOBS" || { echo "!! make macos-app failed"; exit 1; }

[ -d bin/SWOS.app ] || { echo "!! bin/SWOS.app was not produced"; exit 1; }
echo "== lipo: $(lipo -info bin/SWOS.app/Contents/MacOS/SWOS 2>&1)"

echo "== packing bin/$ZIP (ditto — preserves SDL2.framework symlinks)"
rm -f "bin/$ZIP"
( cd bin && ditto -c -k --sequesterRsrc --keepParent SWOS.app "$ZIP" ) \
    || { echo "!! ditto failed"; exit 1; }
# zipinfo shows a leading 'l' in the mode column for a stored symlink. A plain
# `zip -r` would have followed them and this count would be 0.
echo "== SYMLINKS_IN_ZIP=$(zipinfo "bin/$ZIP" 2>/dev/null | grep -c '^l' || true)"
echo "== BUILT_COMMIT=$(git rev-parse --short HEAD)"
echo "== ZIP_BYTES=$(stat -f%z "bin/$ZIP")"
echo "== REMOTE_OK"
REMOTE

grep -q "== REMOTE_OK" "$LOG" || { echo ""; echo "-- FAIL: remote build did not complete"; exit 1; }

banner "fetch -> bin/$MAC_ZIP"
scp -q "$MAC_HOST:$MAC_PATH/bin/$MAC_ZIP" "bin/$MAC_ZIP" || { echo "-- FAIL: scp"; exit 1; }

banner "SUMMARY"
printf '  built from commit  : %s\n' "$(sed -n 's/.*BUILT_COMMIT=//p' "$LOG" | head -1)"
printf '  injuries + cards   : %s\n' \
    "$( [ "$NOINJ" = 1 ] && echo 'OFF (tester build — SWOS_TEST_NO_INJURIES_CARDS=1)' || echo 'on  (release build)' )"
printf '  save-format verify : off\n'
printf '  flag in build      : %s compile lines with -DSWOS_TEST_NO_INJURIES_CARDS=1\n' \
    "$(grep -c -- '-DSWOS_TEST_NO_INJURIES_CARDS=1' "$LOG")"
printf '  slices             : %s\n' "$(grep -m1 '^== lipo:' "$LOG" | sed 's/^== lipo: //')"
printf '  symlinks preserved : %s (0 would mean SDL2.framework is broken)\n' \
    "$(sed -n 's/.*SYMLINKS_IN_ZIP=//p' "$LOG" | head -1)"
echo ""
ls -l "bin/$MAC_ZIP"
