#!/usr/bin/env bash
# Drive the Nintendo Switch build from WSL.
#
#   scripts/build_switch.sh            build   -> bin/SWOS.nss
#   scripts/build_switch.sh package    build, then dist/SWOS.nsp
#   scripts/build_switch.sh clean
#
# The compile itself has to happen on the WINDOWS side: NintendoClang is a
# Windows executable and would be handed Linux paths from here. This wraps
# scripts/build_switch.bat through Windows interop, the same approach
# scripts/build_dist.sh already uses to reach Cygwin for the Windows target.
#
# Overridable, and both default to where the reference port keeps them:
#   NINTENDO_SDK_ROOT   the NintendoSDK        (built against 22.2.10)
#   SDL_SWITCH_ROOT     libsdl-org/SDL-switch  (licensee-only)
#
# Neither tree is in this repository and neither may be committed to it — both
# are under NDA. See docs/SWITCH.md.
set -euo pipefail
cd "$(dirname "$0")/.."

ARG="${1:-}"

NINTENDO_SDK_ROOT="${NINTENDO_SDK_ROOT:-D:\\Nintendo\\NativeSDK22.2.10\\NintendoSDK}"
SDL_SWITCH_ROOT="${SDL_SWITCH_ROOT:-D:\\Projects\\GitHub\\SDL-switch}"

# Windows path of this checkout, for the .bat's `cd /d`.
if command -v wslpath >/dev/null 2>&1; then
    WINROOT="$(wslpath -w "$PWD")"
else
    echo "error: wslpath not found — run scripts/build_switch.bat from Windows instead" >&2
    exit 1
fi

CMD="${COMSPEC:-/mnt/c/Windows/System32/cmd.exe}"
[ -x "$CMD" ] || CMD=/mnt/c/Windows/System32/cmd.exe
if [ ! -x "$CMD" ]; then
    echo "error: cmd.exe not reachable — is Windows interop enabled?" >&2
    exit 1
fi

echo "== Switch build (via Windows interop)"
echo "   checkout : $WINROOT"
echo "   SDK      : $NINTENDO_SDK_ROOT"
echo "   SDL      : $SDL_SWITCH_ROOT"
[ -n "$EXTRA_CFLAGS" ] && echo "   cflags   : $EXTRA_CFLAGS"
echo

# The .bat is passed as ONE argv element and the roots go through the
# environment: building the whole thing into a single `cmd /c "..."` string
# means quoting it twice, and cmd then treats the escaped quotes as literal
# ("... is not recognized as an internal or external command"). WSLENV is how
# a WSL variable reaches a Windows process at all — without naming them there,
# cmd would fall back to the .bat's own defaults and silently ignore an
# override set here.
# EXTRA_CFLAGS reaches Makefile.switch the same way: named in WSLENV or cmd
# never sees it. scripts/build_dist.sh sets it to keep the console build's
# SWOS_TEST_RULES in step with every other target in a drop.
EXTRA_CFLAGS="${EXTRA_CFLAGS:-}"
export NINTENDO_SDK_ROOT SDL_SWITCH_ROOT EXTRA_CFLAGS
export WSLENV="${WSLENV:+$WSLENV:}NINTENDO_SDK_ROOT:SDL_SWITCH_ROOT:EXTRA_CFLAGS"

set +e
if [ -n "$ARG" ]; then
    "$CMD" /c "$WINROOT\\scripts\\build_switch.bat" "$ARG"
else
    "$CMD" /c "$WINROOT\\scripts\\build_switch.bat"
fi
rc=$?
set -e

if [ $rc -ne 0 ]; then
    echo "-- FAIL (exit $rc)" >&2
    exit $rc
fi

[ -f bin/SWOS.nss ] && ls -l --time-style=+"%H:%M" bin/SWOS.nss | awk '{printf "   bin/SWOS.nss  %s bytes  %s\n", $5, $6}'
[ -f dist/SWOS.nsp ] && ls -l --time-style=+"%H:%M" dist/SWOS.nsp | awk '{printf "   dist/SWOS.nsp %s bytes  %s\n", $5, $6}'
echo "== OK"
