#!/bin/sh
# Builds the headless Mega Drive test runner: the Genesis Plus GX libretro core (pinned
# commit) and scripts/md/mdrun.c, into build/md/harness/. Needs git, make, a C compiler
# and zlib. See docs/MEGADRIVE.md "Testing".
set -e
cd "$(dirname "$0")/../.."
H=build/md/harness
GPGX_URL=${GPGX_URL:-https://github.com/libretro/Genesis-Plus-GX.git}
GPGX_REV=${GPGX_REV:-60bee4c724859710ce05672239e5b601ad01382e}
mkdir -p "$H"
if [ ! -d "$H/gpgx/.git" ]; then
    git clone --filter=blob:none "$GPGX_URL" "$H/gpgx"
fi
( cd "$H/gpgx" && git fetch -q origin "$GPGX_REV" 2>/dev/null || true; git checkout -q "$GPGX_REV" )
# Our changes to the core (scripts/md/gpgx_harness.patch): count the game's writes into
# ROM, and a cycle-weighted profile of the 68000's PC (mdrun --profile).
if ! grep -q mdrun_prof_on "$H/gpgx/core/m68k/m68kcpu.c"; then
    ( cd "$H/gpgx" && git checkout -q -- . )
    ( cd "$H/gpgx" && git apply "$OLDPWD/scripts/md/gpgx_harness.patch" )
    rm -f "$H/genesis_plus_gx_libretro.so"
fi
if [ ! -f "$H/genesis_plus_gx_libretro.so" ]; then
    make -C "$H/gpgx" -f Makefile.libretro -j"$(nproc 2>/dev/null || echo 4)"
    cp "$H/gpgx/genesis_plus_gx_libretro.so" "$H/"
fi
cc -O2 -Wall -o "$H/mdrun" scripts/md/mdrun.c -I"$H/gpgx/libretro/libretro-common/include" -ldl -lz
echo "built $H/mdrun and $H/genesis_plus_gx_libretro.so"
