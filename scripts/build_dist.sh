#!/usr/bin/env bash
#
# build_dist.sh — build every OpenSWOS target this machine can produce and
#                 package the binaries into bin/SWOS.zip.
#
#   scripts/build_dist.sh                 # build everything + write bin/SWOS.zip
#   scripts/build_dist.sh --only windows  # just the .exe
#   scripts/build_dist.sh --skip wasm     # everything but the browser build
#   scripts/build_dist.sh --no-zip        # build only, don't package
#   scripts/build_dist.sh --no-full       # skip the big SWOS-full.zip
#
# Two archives are written:
#   bin/SWOS.zip       binaries only (~10 MB) — for someone who already has bin/
#   bin/SWOS-full.zip  binaries + game data + the Cygwin runtime for SWOS.exe:
#                      unzip and play. Excludes save/ (per-user dir, and 24 MB of
#                      test careers here).
#   bin/SWOS-Amiga.zip the m68k binary + ONLY the trees it actually opens
#                      (data grafs grafs_aga sound) + the AmigaDOS boot files
#                      (c s) + an empty save/. Unpack onto the volume
#                      FS-UAE/WinUAE boots from and s/startup-sequence runs it.
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

# SWOS-full.zip — a RUNNABLE drop: the same binaries plus the game data, so a
# tester can unzip and play instead of supplying their own bin/. Everything sits
# at the zip root because the SDL binary chdir's to its own directory.
#
# The data list mirrors what `make macos-app` bundles (Makefile, macos-app rule)
# — that is this project's tested definition of "what the game needs at runtime".
# save/ is deliberately EXCLUDED: it is 24 MB of test careers here, and saves go
# to a per-user directory anyway (swos_io.c), so shipping it would both bloat the
# zip and drop stale saves on testers.
FULL_DATA_DIRS="data grafs grafs_aga grafs_dos grafs_2020 grafs_arcade sound data_json"
# Cygwin runtime for SWOS.exe. `cygcheck bin/SWOS.exe` reports exactly these two
# as its non-system dependencies; everything else it prints is a Windows system
# DLL. (Don't derive this list with `strings` — that picks up every DLL name
# mentioned anywhere, including SDL backends it only ever dlopens, and lands you
# at ~14 MB of X11/pulse/codec libraries that are never loaded.)
FULL_DLLS="cygwin1.dll cygSDL2-2-0-0.dll"

# SWOS-Amiga.zip — the runnable Amiga drop: the m68k binary plus only the trees
# the m68k binary actually opens. Verified against the built binary itself
# (`strings bin/SWOS2 | grep -oE '^(data|grafs|...)/'`): it references data/,
# grafs/, grafs_aga/, sound/ and save/ — and NOTHING from grafs_dos, grafs_2020,
# grafs_arcade or data_json, which are SDL-only asset tiers. Shipping those would
# add ~50 MB the Amiga can never load. An EMPTY save/ is created because the
# binary writes there and will not create the directory itself.
# Copy the contents onto a volume FS-UAE/WinUAE mounts (the harness mounts bin/
# as DH1: and cd's in, so the same relative paths resolve).
#
# c/ and s/ are the AmigaDOS BOOT infrastructure and are NOT discoverable by the
# strings rule above — the binary never opens them, AmigaDOS does. s/startup-
# sequence is the whole point: mount the drop as the boot volume and it runs
# SWOS2 and then quits the emulator via c/UAEquit. Without them the drop is a
# pile of files someone has to launch by hand.
AMIGA_DATA_DIRS="data grafs grafs_aga sound"
AMIGA_BOOT_DIRS="c s"
CYGWIN_BIN="${CYGWIN_BIN:-/mnt/c/cygwin64/bin}"

JOBS="$(nproc 2>/dev/null || echo 4)"
DO_CLEAN=1
DO_ZIP=1
VERIFY=0
NOINJ=1          # tester builds: suppress injuries/cards. --release turns it off.
ZIP_PATH="bin/SWOS.zip"
FULL_ZIP_PATH="bin/SWOS-full.zip"
AMIGA_ZIP_PATH="bin/SWOS-Amiga.zip"
DO_FULL=1
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
      --no-zip        build but don't package (neither zip)
      --no-full       skip SWOS-full.zip (binaries-only SWOS.zip still written)
      --full-zip PATH full-dist output path (default: bin/SWOS-full.zip)
      --amiga-zip PATH Amiga-dist output path (default: bin/SWOS-Amiga.zip)
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
        --no-zip)    DO_ZIP=0; DO_FULL=0; shift ;;
        --no-full)   DO_FULL=0; shift ;;
        --full-zip)  FULL_ZIP_PATH="$2"; shift 2 ;;
        --amiga-zip) AMIGA_ZIP_PATH="$2"; shift 2 ;;
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

# ---- package: full dist (binaries + data + Windows runtime) ------------------
if [ "$DO_FULL" = 1 ] && [ -n "$ZIPPED" ] && command -v zip >/dev/null 2>&1; then
    banner "package -> $FULL_ZIP_PATH  (full game: binaries + data)"
    FULL_DIRS=""
    for d in $FULL_DATA_DIRS; do
        [ -d "bin/$d" ] && FULL_DIRS="$FULL_DIRS $d"
    done
    if [ -z "$FULL_DIRS" ]; then
        echo "-- SKIP: no data directories under bin/"
    else
        rm -f "$FULL_ZIP_PATH"
        # Binaries junked to the root, then the data trees with their paths kept.
        ( cd bin && zip -j -q "$ROOT/$FULL_ZIP_PATH" $ZIPPED ) &&
        ( cd bin && zip -r -q "$ROOT/$FULL_ZIP_PATH" $FULL_DIRS -x '*/.DS_Store' ) &&
            echo "-- data:     $FULL_DIRS"

        # Windows runtime beside SWOS.exe. Skipped cleanly off-Cygwin hosts.
        DLL_ADDED=""; DLL_MISS=""
        for d in $FULL_DLLS; do
            if [ -f "$CYGWIN_BIN/$d" ]; then DLL_ADDED="$DLL_ADDED $CYGWIN_BIN/$d"
            else DLL_MISS="$DLL_MISS $d"; fi
        done
        if [ -n "$DLL_ADDED" ]; then
            zip -j -q "$FULL_ZIP_PATH" $DLL_ADDED && \
                echo "-- windows runtime:$(echo $DLL_ADDED | tr ' ' '\n' | sed 's|.*/||' | tr '\n' ' ')"
        fi
        [ -n "$DLL_MISS" ] && \
            echo "-- NOTE: Cygwin DLLs not found in $CYGWIN_BIN (SWOS.exe will not run standalone):$DLL_MISS"
        echo "-- OK: $FULL_ZIP_PATH"
    fi
fi

# ---- package: Amiga dist (m68k binary + the trees it actually reads) ---------
if [ "$DO_FULL" = 1 ] && [ -f bin/SWOS2 ] && command -v zip >/dev/null 2>&1; then
    banner "package -> $AMIGA_ZIP_PATH  (Amiga: SWOS2 + Amiga data)"
    AMIGA_DIRS=""
    for d in $AMIGA_DATA_DIRS $AMIGA_BOOT_DIRS; do
        [ -d "bin/$d" ] && AMIGA_DIRS="$AMIGA_DIRS $d"
    done
    for d in $AMIGA_BOOT_DIRS; do
        [ -d "bin/$d" ] || echo "!! bin/$d missing — the drop will not self-boot" >&2
    done
    [ -f "bin/s/startup-sequence" ] ||
        echo "!! bin/s/startup-sequence missing — the drop will not self-boot" >&2
    rm -f "$AMIGA_ZIP_PATH"
    ( cd bin && zip -j -q "$ROOT/$AMIGA_ZIP_PATH" SWOS2 ) &&
    ( cd bin && zip -r -q "$ROOT/$AMIGA_ZIP_PATH" $AMIGA_DIRS -x '*/.DS_Store' ) && {
        # Empty save/ — the binary writes there but won't mkdir it. Staged from a
        # temp dir so bin/save's 24 MB of test careers cannot leak in.
        AMIGA_TMP="$(mktemp -d)"
        mkdir -p "$AMIGA_TMP/save"
        ( cd "$AMIGA_TMP" && zip -r -q "$ROOT/$AMIGA_ZIP_PATH" save )
        rm -rf "$AMIGA_TMP"
        echo "-- data:    $AMIGA_DIRS + empty save/"
        echo "-- OK: $AMIGA_ZIP_PATH"
    }
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
[ -f "$FULL_ZIP_PATH" ] && ls -l "$FULL_ZIP_PATH"
[ -f "$AMIGA_ZIP_PATH" ] && ls -l "$AMIGA_ZIP_PATH"
exit $RC
