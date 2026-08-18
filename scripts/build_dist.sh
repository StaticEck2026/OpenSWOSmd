#!/usr/bin/env bash
#
# build_dist.sh — build every OpenSWOS target this machine can produce and
#                 package the binaries into bin/SWOS.zip.
#
#   scripts/build_dist.sh                 # build everything + write bin/SWOS.zip
#   scripts/build_dist.sh --only windows  # just the .exe
#   scripts/build_dist.sh --skip wasm     # everything but the browser build
#   scripts/build_dist.sh --no-zip        # build only, don't package
#
# Targets:
#
#   linux-x86      make                 -> bin/SWOS          (32-bit, -m32)
#   linux-x86_64   make X86_64=1        -> bin/SWOS.x86_64
#   linux-arm64    make ARM64=1         -> bin/SWOS.aarch64
#   linux-armhf    make ARM=1           -> bin/SWOS.armhf
#   wasm           make WASM=1          -> bin/SWOS.html/.js/.wasm/.data
#   amiga          make -f Makefile.amiga -> bin/SWOS2
#   windows        make (in Cygwin)     -> bin/SWOS.exe
#
# macOS is deliberately NOT here: `make macos-app` hard-fails unless
# `uname -s` is Darwin, and there is no cross-compile path. Run this on a Mac:
#
#     make macos-universal      # bin/SWOS.macos (x86_64 + arm64)
#     make macos-app            # bin/SWOS.app
#
# TESTER BUILD BY DEFAULT
#
# SWOS_TEST_NO_INJURIES_CARDS=1 is ON unless you pass --release. It suppresses
# in-match yellow/red cards and post-match random injuries so a tester can hit
# VIEW RESULT through a whole season without substitution menus or booking
# prompts — without it, testing takes far longer. Pass --release for a public
# build (stock injuries + cards). Keep it consistent across ALL platforms in a
# given drop: a macOS build with the flag next to a Windows build without it
# produces bug reports for differences that aren't bugs (e.g. subbing an
# injured player scores as a DOWNGRADE under the flag — see the Makefile's
# "KNOWN DISPLAY SIDE-EFFECTS" note).
#
# THREE THINGS THIS SCRIPT EXISTS TO GET RIGHT
#
# 1. SWOS_SAVE_FORMAT_JSON_VERIFY defaults to ON for every target except WASM.
#    The `?= 0` line in the Makefile sits INSIDE the `ifdef WASM` branch, so a
#    plain `make` builds the save-format round-trip verifier in. That pass makes
#    every SAVE zero the saved fields, reload the file, and NOT restore the
#    snapshot — a bring-up diagnostic, not release behaviour. We pass
#    SWOS_SAVE_FORMAT_JSON_VERIFY=0 unless you ask for --verify.
#
# 2. Makefile.amiga has no SWOS_* knobs, so the m68k target would silently miss
#    SWOS_TEST_NO_INJURIES_CARDS. We pass it through its EXTRA_CFLAGS escape
#    hatch, and its objects live in obj/ (not build/) so `rm -rf build` alone
#    leaves it stale and reports "Nothing to be done for 'all'".
#
# 3. Toggling any -D flag needs a clean first: make compares timestamps, not
#    CFLAGS, so a flag change silently relinks stale objects. We wipe the object
#    dirs by default (--no-clean to skip when you know the flags are unchanged).
#
# The Windows binary is a CYGWIN binary (it imports cygwin1.dll) — testers need
# cygwin1.dll + SDL2.dll beside it. The Makefile picks the Windows target from
# the HOST SHELL ($(OS)/uname -s), never from a flag, so it has to be built
# inside Cygwin; from WSL we reach that shell through Windows interop.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

# ---- machine-specific paths (override via the environment) ------------------
EMSDK_DIR="${EMSDK_DIR:-/mnt/c/Projects/Linux/emsdk}"
AMIGA_TOOLCHAIN="${AMIGA_TOOLCHAIN:-/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux}"
CYGWIN_BASH="${CYGWIN_BASH:-/mnt/c/cygwin64/bin/bash.exe}"

ALL_TARGETS="linux-x86 linux-x86_64 linux-arm64 linux-armhf wasm amiga windows"

# Binaries that go into the zip. The WASM outputs are deliberately excluded:
# SWOS.html/.js/.wasm/.data are a web deployment, not something a tester runs
# from a zip, and .data alone is ~21 MB.
ZIP_MEMBERS="SWOS SWOS.x86_64 SWOS.aarch64 SWOS.armhf SWOS.exe SWOS2"

JOBS="$(nproc 2>/dev/null || echo 4)"
DO_CLEAN=1
DO_ZIP=1
VERIFY=0
NOINJ=1          # tester builds: suppress injuries/cards. --release turns it off.
ZIP_PATH="bin/SWOS.zip"
ONLY=""
SKIP=""

usage() {
    # Print the header comment block (from line 3) up to the first code line.
    awk 'NR >= 3 { if ($0 !~ /^#/) exit; sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
    cat <<EOF

Options:
  -j, --jobs N        parallel make jobs (default: $(nproc 2>/dev/null || echo 4))
      --only  LIST    comma-separated targets to build (default: all)
      --skip  LIST    comma-separated targets to skip
      --verify        build WITH SWOS_SAVE_FORMAT_JSON_VERIFY=1 (debug only)
      --release       PUBLIC build: stock injuries + cards (default is a tester
                      build with SWOS_TEST_NO_INJURIES_CARDS=1)
      --no-clean      don't wipe object dirs first (only safe if flags unchanged)
      --no-zip        build but don't package
      --zip PATH      zip output path (default: $ZIP_PATH)
  -h, --help          this text

Targets: $ALL_TARGETS
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        -j|--jobs)   JOBS="$2"; shift 2 ;;
        --only)      ONLY="$2"; shift 2 ;;
        --skip)      SKIP="$2"; shift 2 ;;
        --verify)    VERIFY=1; shift ;;
        --release)   NOINJ=0; shift ;;
        --no-clean)  DO_CLEAN=0; shift ;;
        --no-zip)    DO_ZIP=0; shift ;;
        --zip)       ZIP_PATH="$2"; shift 2 ;;
        -h|--help)   usage; exit 0 ;;
        *) echo "build_dist: unknown option '$1' (try --help)" >&2; exit 2 ;;
    esac
done

MAKE_FLAGS="SWOS_SAVE_FORMAT_JSON_VERIFY=$VERIFY"
AMIGA_EXTRA=""
if [ "$NOINJ" = 1 ]; then
    MAKE_FLAGS="$MAKE_FLAGS SWOS_TEST_NO_INJURIES_CARDS=1"
    # Makefile.amiga has no SWOS_* knobs of its own; EXTRA_CFLAGS is its
    # documented escape hatch, so the m68k binary matches the SDL ones.
    AMIGA_EXTRA="-DSWOS_TEST_NO_INJURIES_CARDS=1"
fi

wanted() {   # wanted <target> -> 0 if it should be built
    local t="$1"
    [ -n "$ONLY" ] && { case ",$ONLY," in *",$t,"*) : ;; *) return 1 ;; esac; }
    [ -n "$SKIP" ] && { case ",$SKIP," in *",$t,"*) return 1 ;; esac; }
    return 0
}

declare -A STATUS   # target -> OK / FAIL / SKIP(reason) / not-requested
ORDER=""

record() { STATUS["$1"]="$2"; ORDER="$ORDER $1"; }

banner() {
    echo ""
    echo "======================================================================"
    echo "== $*"
    echo "======================================================================"
}

# run <target> <label> <cmd...>
run() {
    local t="$1" label="$2"; shift 2
    banner "$label"
    if "$@"; then record "$t" "OK"; echo "-- OK: $label"
    else       record "$t" "FAIL"; echo "-- FAIL: $label"; fi
}

# ---- clean ------------------------------------------------------------------
if [ "$DO_CLEAN" = 1 ]; then
    banner "clean (object dirs — make compares timestamps, not CFLAGS)"
    # SDL targets keep objects under build/<os>/<arch>; the Amiga makefile uses
    # its own obj/ tree, so `rm -rf build` alone would leave it stale.
    rm -rf build
    make -f Makefile.amiga clean >/dev/null 2>&1
    echo "-- cleaned build/ and the Amiga obj/ tree"
fi

# ---- SDL targets ------------------------------------------------------------
wanted linux-x86    && run linux-x86    "linux x86 (32-bit) -> bin/SWOS" \
                           make $MAKE_FLAGS -j"$JOBS"
wanted linux-x86_64 && run linux-x86_64 "linux x86_64       -> bin/SWOS.x86_64" \
                           make X86_64=1 $MAKE_FLAGS -j"$JOBS"

if wanted linux-arm64; then
    if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 || \
       [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
        run linux-arm64 "linux arm64        -> bin/SWOS.aarch64" \
            make ARM64=1 $MAKE_FLAGS -j"$JOBS"
    else
        record linux-arm64 "SKIP (no aarch64-linux-gnu-gcc)"
    fi
fi

if wanted linux-armhf; then
    if command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1 || \
       [ "$(uname -m)" = "armv7l" ]; then
        run linux-armhf "linux armhf        -> bin/SWOS.armhf" \
            make ARM=1 $MAKE_FLAGS -j"$JOBS"
    else
        record linux-armhf "SKIP (no arm-linux-gnueabihf-gcc)"
    fi
fi

# ---- wasm -------------------------------------------------------------------
if wanted wasm; then
    if command -v emcc >/dev/null 2>&1 || [ -f "$EMSDK_DIR/emsdk_env.sh" ]; then
        banner "wasm -> bin/SWOS.html/.js/.wasm/.data"
        # emsdk_env.sh is not -u clean, hence the subshell + set +u.
        if ( set +u
             [ -f "$EMSDK_DIR/emsdk_env.sh" ] && . "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
             cd "$ROOT" && make WASM=1 $MAKE_FLAGS -j"$JOBS" ); then
            record wasm "OK"; echo "-- OK: wasm"
        else
            record wasm "FAIL"; echo "-- FAIL: wasm"
        fi
    else
        record wasm "SKIP (no emcc; set EMSDK_DIR)"
    fi
fi

# ---- amiga ------------------------------------------------------------------
if wanted amiga; then
    if [ -x "$AMIGA_TOOLCHAIN/opt/bin/m68k-amiga-elf-gcc" ] || \
       command -v m68k-amiga-elf-gcc >/dev/null 2>&1; then
        banner "amiga m68k -> bin/SWOS2"
        # The toolchain ships inside the amiga-debug VSCode extension and MUST be
        # the benbaker76 fork (see .claude/CLAUDE.md). A wrong path fails as
        # 'Error 127' and an up-to-date tree hides it as "Nothing to be done".
        if PATH="$AMIGA_TOOLCHAIN/opt/bin:$AMIGA_TOOLCHAIN:$PATH" \
               make -f Makefile.amiga EXTRA_CFLAGS="$AMIGA_EXTRA" -j"$JOBS"; then
            record amiga "OK"; echo "-- OK: amiga"
        else
            record amiga "FAIL"; echo "-- FAIL: amiga"
        fi
    else
        record amiga "SKIP (no m68k-amiga-elf-gcc; set AMIGA_TOOLCHAIN)"
    fi
fi

# ---- windows (Cygwin) -------------------------------------------------------
if wanted windows; then
    case "$(uname -s)" in
        CYGWIN*|MINGW*|MSYS*)
            # Already inside a Windows shell — plain make picks SWOS.exe.
            run windows "windows -> bin/SWOS.exe (native)" \
                make $MAKE_FLAGS -j"$JOBS"
            ;;
        *)
            if [ -x "$CYGWIN_BASH" ]; then
                banner "windows (Cygwin via interop) -> bin/SWOS.exe"
                # Translate this WSL path into a Cygwin one. -lc is a LOGIN
                # shell: the Cygwin profile is sourced, so a stray
                # SWOS_* export there would override our flags.
                WINPATH="$(wslpath -w "$ROOT" 2>/dev/null || echo "")"
                if [ -z "$WINPATH" ]; then
                    record windows "SKIP (can't map $ROOT to a Windows path)"
                elif "$CYGWIN_BASH" -lc \
                        "cd \"\$(cygpath -u '$WINPATH')\" && make $MAKE_FLAGS -j$JOBS"; then
                    record windows "OK"; echo "-- OK: windows"
                else
                    record windows "FAIL"; echo "-- FAIL: windows"
                fi
            else
                record windows "SKIP (no Cygwin at $CYGWIN_BASH)"
            fi
            ;;
    esac
fi

# ---- package ----------------------------------------------------------------
ZIPPED=""
MISSING=""
if [ "$DO_ZIP" = 1 ]; then
    banner "package -> $ZIP_PATH"
    if ! command -v zip >/dev/null 2>&1; then
        echo "-- SKIP: 'zip' not installed (apt install zip)"
    else
        for m in $ZIP_MEMBERS; do
            if [ -f "bin/$m" ]; then ZIPPED="$ZIPPED $m"; else MISSING="$MISSING $m"; fi
        done
        if [ -z "$ZIPPED" ]; then
            echo "-- SKIP: nothing to package"
        else
            rm -f "$ZIP_PATH"
            # -j junks paths so members sit at the zip root, not under bin/.
            ( cd bin && zip -j -q "$ROOT/$ZIP_PATH" $ZIPPED ) && \
                echo "-- OK: $ZIP_PATH <-$ZIPPED"
            [ -n "$MISSING" ] && echo "-- NOTE: not in the zip (not built):$MISSING"
        fi
    fi
fi

# ---- summary ----------------------------------------------------------------
banner "SUMMARY"
printf '  save-format verify : %s\n' \
    "$( [ "$VERIFY" = 1 ] && echo 'ON  (debug — every save round-trips)' || echo 'off' )"
printf '  injuries + cards   : %s\n' \
    "$( [ "$NOINJ" = 1 ] && echo 'OFF (tester build — SWOS_TEST_NO_INJURIES_CARDS=1)' || echo 'on  (release build)' )"
echo ""
RC=0
for t in $ALL_TARGETS; do
    s="${STATUS[$t]:-}"
    [ -z "$s" ] && s="not requested"
    printf '  %-14s %s\n' "$t" "$s"
    [ "$s" = "FAIL" ] && RC=1
done
echo ""
echo "  macos          not buildable here — run 'make macos-app' on a Mac"
echo ""
[ "$DO_ZIP" = 1 ] && [ -f "$ZIP_PATH" ] && ls -l "$ZIP_PATH"
exit $RC
