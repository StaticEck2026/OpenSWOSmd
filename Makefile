# ---- Toolchain ----
# Cross-compile to ARM 32-bit (armv7-a / armhf) with `make ARM=1`. Picks
# the standard Debian/Ubuntu cross-toolchain prefix; override with
# `make ARM=1 CC=...` if your distro uses a different name. The build
# expects libsdl2-dev:armhf installed (or a sysroot containing SDL2),
# discoverable via `arm-linux-gnueabihf-pkg-config`. The Amiga struct
# size_check[] asserts assume 32-bit pointers, which armhf already
# provides — no -m32 needed.
#
# Cross-compile to ARM 64-bit (aarch64) with `make ARM64=1`. Picks
# `aarch64-linux-gnu-gcc`; needs libsdl2-dev:arm64 (Debian/Ubuntu
# foreign-arch) or a sysroot. 64-bit pointers mean the static
# size_check[] asserts that hard-code Amiga (32-bit) struct sizes
# will fail to compile until Phase 5 / 6 of the 64-bit transition.
# Useful right now as a "what still hard-codes 32-bit" checklist.
#
# macOS: auto-detected via `uname -s == Darwin`. Native clang; expects
# SDL2 from Homebrew (`brew install sdl2`). Like ARM64, modern macOS
# is 64-bit, so the same Phase 5 / 6 constraints apply.
ifdef WASM
# WebAssembly via Emscripten (`make WASM=1`). `source <emsdk>/emsdk_env.sh`
# first so emcc is on PATH. SDL2 comes from emcc's bundled port (-sUSE_SDL=2),
# NOT pkg-config. Single-threaded + ASYNCIFY — see docs/WASM.md.
CC := emcc
PKGCONF := true
# Verify-on reloads the career into live globals on every save (corrupts live
# state) — pointless in the browser; default off (command line still overrides).
SWOS_SAVE_FORMAT_JSON_VERIFY ?= 0
else ifdef ARM64
ARM64_HOST_MACHINE := $(shell uname -m)
ifneq (,$(filter aarch64 arm64,$(ARM64_HOST_MACHINE)))
CC := gcc
PKGCONF := pkg-config
else
CC := aarch64-linux-gnu-gcc
PKGCONF := PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig pkg-config
endif
else ifdef ARM
# Native ARM builds (e.g. on a Raspberry Pi) use the system gcc.
# Cross-builds from x86_64 → armhf use the standard Debian/Ubuntu
# arm-linux-gnueabihf- toolchain. Detect by host architecture; user
# can override with `make ARM=1 CC=...` if the distro's prefix is
# different.
ARM_HOST_MACHINE := $(shell uname -m)
ifneq (,$(filter arm% aarch64,$(ARM_HOST_MACHINE)))
CC := gcc
PKGCONF := pkg-config
else
CC := arm-linux-gnueabihf-gcc
# Ubuntu/Debian don't ship an arm-linux-gnueabihf-pkg-config wrapper —
# use the regular pkg-config but force it to look at the armhf
# pkgconfig directory only (PKG_CONFIG_LIBDIR overrides the default
# /usr/lib/pkgconfig:/usr/share/pkgconfig search path that would
# otherwise return amd64 SDL2 paths). libsdl2-dev:armhf installs its
# .pc into /usr/lib/arm-linux-gnueabihf/pkgconfig.
PKGCONF := PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig pkg-config
endif
else
# Default: native compiler (gcc on Linux/MinGW/Cygwin, clang on macOS).
ifeq ($(shell uname -s),Darwin)
CC := cc
# SDL2 search order on macOS:
#   1. SDL2.framework in /Library/Frameworks, ~/Library/Frameworks,
#      /opt/homebrew/Frameworks, or /usr/local/Frameworks. Preferred
#      because the official libsdl.org build ships as a universal
#      binary — needed for `make macos-universal`.
#   2. Homebrew's SDL2 via pkg-config (single-arch only).
SDL2_MACOS_FW :=
ifneq (,$(wildcard /Library/Frameworks/SDL2.framework))
SDL2_MACOS_FW := /Library/Frameworks
else ifneq (,$(wildcard $(HOME)/Library/Frameworks/SDL2.framework))
SDL2_MACOS_FW := $(HOME)/Library/Frameworks
else ifneq (,$(wildcard /opt/homebrew/Frameworks/SDL2.framework))
SDL2_MACOS_FW := /opt/homebrew/Frameworks
else ifneq (,$(wildcard /usr/local/Frameworks/SDL2.framework))
SDL2_MACOS_FW := /usr/local/Frameworks
endif
ifndef SDL2_MACOS_FW
# Homebrew SDL2 lives under /opt/homebrew (arm64) or /usr/local (intel).
PKGCONF := PKG_CONFIG_PATH=$(shell brew --prefix sdl2 2>/dev/null)/lib/pkgconfig pkg-config
endif
else
CC := gcc
endif
endif
SHELL := /bin/bash

# ---- Platform ----
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
MACOS_ARCH ?= $(shell uname -m)

# Normalize an arch string for the build/<os>/<arch> object dir so the same
# 64-bit ARM target is "arm64" on every OS (Linux uname says aarch64, macOS/
# Windows say arm64) and 32-bit x86 is "x86": $(call norm_arch,aarch64) -> arm64.
norm_arch = $(patsubst aarch64,arm64,$(patsubst amd64,x86_64,$(patsubst i686,x86,$(patsubst i386,x86,$(1)))))

# ---- Output ----
# Each branch sets TARGET_NAME (the bin/ binary name) AND the BUILD_OS /
# BUILD_ARCH that select the build/<os>/<arch> object dir.
ifdef WASM
# emcc emits SWOS.html + SWOS.js + SWOS.wasm + SWOS.data (the preloaded data
# package). Objects live under build/emscripten/wasm (wasm32 = ILP32).
TARGET_NAME := SWOS.html
BUILD_OS    := emscripten
BUILD_ARCH  := wasm
else ifeq ($(OS),Windows_NT)
TARGET_NAME := SWOS.exe
BUILD_OS    := windows
BUILD_ARCH  := $(call norm_arch,$(UNAME_M))
else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
# uname -s on Cygwin outputs `CYGWIN_NT-10.0-22631` (or similar with
# the Windows build number suffix), not bare `CYGWIN_NT`. Use
# `findstring` so the version suffix doesn't break detection.
TARGET_NAME := SWOS.exe
BUILD_OS    := windows
BUILD_ARCH  := $(call norm_arch,$(UNAME_M))
else ifneq (,$(findstring MINGW,$(UNAME_S)))
# Same suffix issue for MSYS2 MINGW32_NT-X.Y / MINGW64_NT-X.Y.
TARGET_NAME := SWOS.exe
BUILD_OS    := windows
BUILD_ARCH  := $(call norm_arch,$(UNAME_M))
else ifdef ARM64
TARGET_NAME := SWOS.aarch64
BUILD_OS    := linux
BUILD_ARCH  := arm64
else ifdef ARM
# Distinguish the cross-built ARM binary so it doesn't overwrite a
# native x86 bin/SWOS in the same checkout. Native ARM builds (on a
# Pi) also get the .armhf suffix — `file bin/SWOS.armhf` confirms the
# ELF arch either way.
TARGET_NAME := SWOS.armhf
BUILD_OS    := linux
BUILD_ARCH  := armhf
else ifdef X86_64
# Linux x86_64 explicit build (drops the default -m32 so the binary
# is 64-bit LP64 — matches what the macOS / aarch64 / cygwin64
# targets already produce). The size_check[] asserts are gated on
# UINTPTR_MAX so they pick up the 64-bit struct sizes correctly.
TARGET_NAME := SWOS.x86_64
BUILD_OS    := linux
BUILD_ARCH  := x86_64
else ifeq ($(UNAME_S),Darwin)
TARGET_NAME := SWOS.macos
BUILD_OS    := macos
BUILD_ARCH  := $(call norm_arch,$(MACOS_ARCH))
else
# Default native build: 32-bit x86 (-m32).
TARGET_NAME := SWOS
BUILD_OS    := linux
BUILD_ARCH  := x86
endif

BINDIR := bin
TARGET := $(BINDIR)/$(TARGET_NAME)

# ---- Object output dir (build/<os>/<arch>) ----
# Objects build under build/<os>/<arch> instead of cluttering src/, split by OS
# AND arch so every target in the same checkout keeps its own .o files:
# build/linux/x86_64, build/windows/x86_64, build/windows/arm64, build/linux/
# arm64, build/macos/arm64, … never collide. (The old flat in-tree layout forced
# a `make clean` whenever you switched target; Windows-on-ARM now slots in too.)
OBJDIR := build/$(BUILD_OS)/$(BUILD_ARCH)

# ---- Sources ----
SRCDIR  := src
INCDIR  := include
SOURCES := $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/generated/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
DEPS    := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.d,$(SOURCES))

# The generated-C rules below define `src/*.c` TARGETS, which would otherwise become the
# Makefile's first (default) goal. Pin the default goal so a bare `make` still builds the
# binary (the `all` target), not competition_data.c.
.DEFAULT_GOAL := all

# ---- Data-driven C (JSON -> C bake) ----
# competition_data.c and menu_data.c are GENERATED from the editable JSON in
# bin/data_json/ by the Python emitters (the Amiga build bakes JSON->C the same way).
# Editing the JSON regenerates the C at build; `make regen-data` forces it. To verify the
# regen is faithful, run the bare emitter (no --out): python3 scripts/emit_competitions.py
COMP_JSON := $(wildcard bin/data_json/competitions/*.json)
MENU_JSON := $(wildcard bin/data_json/menus/*.json)
TACTICS_JSON := $(wildcard bin/data_json/tactics/*.json)

# python3 for the JSON->C regen. If it's not on PATH (e.g. a minimal Cygwin), the committed
# .c is compiled as-is and a JSON edit needs python3 to take effect (the touch suppresses a
# pointless rebuild loop). The committed .c always exists, so the build never hard-fails.
PYTHON := $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null)

$(SRCDIR)/competition_data.c: $(COMP_JSON) scripts/emit_competitions.py scripts/export_competitions.py scripts/swos_team_file.py
ifeq ($(strip $(PYTHON)),)
	@echo "  WARN  python3 not found; compiling the committed $@ (JSON edits won't apply)"; touch $@
else
	@echo "  REGEN $@"; $(PYTHON) scripts/emit_competitions.py --out $@ >/dev/null
endif

$(SRCDIR)/menu_data.c: $(MENU_JSON) scripts/emit_menus.py scripts/export_menus.py
ifeq ($(strip $(PYTHON)),)
	@echo "  WARN  python3 not found; compiling the committed $@ (JSON edits won't apply)"; touch $@
else
	@echo "  REGEN $@"; $(PYTHON) scripts/emit_menus.py --out $@ >/dev/null
endif

$(SRCDIR)/tactics_data.c: $(TACTICS_JSON) scripts/emit_tactics.py scripts/export_tactics.py
ifeq ($(strip $(PYTHON)),)
	@echo "  WARN  python3 not found; compiling the committed $@ (JSON edits won't apply)"; touch $@
else
	@echo "  REGEN $@"; $(PYTHON) scripts/emit_tactics.py --out $@ >/dev/null
endif

.PHONY: regen-data
regen-data:
	@test -n "$(strip $(PYTHON))" || { echo "regen-data needs python3 on PATH"; exit 1; }
	$(PYTHON) scripts/emit_competitions.py --out $(SRCDIR)/competition_data.c
	$(PYTHON) scripts/emit_menus.py --out $(SRCDIR)/menu_data.c
	$(PYTHON) scripts/emit_tactics.py --out $(SRCDIR)/tactics_data.c

# ---- Windows resource (icon) ----
# On MinGW and Cygwin we compile SWOS.rc → SWOS.res.o via windres and
# link it in, which gives the .exe the SWOS.ico for both the file icon
# (Explorer) and the SDL window/taskbar icon (SDL2 picks up resource ID
# 1 when no SDL_SetWindowIcon is called).
#
# `gcc -dumpmachine` returns e.g. `x86_64-w64-mingw32` on MinGW and
# `x86_64-pc-cygwin` on Cygwin64 — match either via findstring.
GCC_MACHINE := $(shell $(CC) -dumpmachine 2>/dev/null)
WINDRES ?= windres
ifneq (,$(findstring mingw,$(GCC_MACHINE)))
WIN_RES_OBJ := $(OBJDIR)/SWOS.res.o
OBJECTS     += $(WIN_RES_OBJ)
else ifneq (,$(findstring cygwin,$(GCC_MACHINE)))
WIN_RES_OBJ := $(OBJDIR)/SWOS.res.o
OBJECTS     += $(WIN_RES_OBJ)
endif

# ---- Helpers ----
PKGCONF ?= pkg-config

ifdef WASM
# Emscripten ships SDL2 as a port — downloaded + cached on first use. Same flag
# for compile and link; no pkg-config.
SDL2_CFLAGS := -sUSE_SDL=2
SDL2_LIBS   := -sUSE_SDL=2
else ifdef SDL2_MACOS_FW
# macOS framework: header search via -F + Headers/, link via -framework SDL2.
# The -rpath entries let the binary find SDL2.framework at runtime when
# either bundled inside the .app (Contents/Frameworks/) or installed
# system-wide under /Library/Frameworks/ or ~/Library/Frameworks/.
SDL2_CFLAGS := -F $(SDL2_MACOS_FW) -I $(SDL2_MACOS_FW)/SDL2.framework/Headers
SDL2_LIBS   := -F $(SDL2_MACOS_FW) -framework SDL2 \
               -Wl,-rpath,@executable_path/../Frameworks \
               -Wl,-rpath,/Library/Frameworks \
               -Wl,-rpath,$(HOME)/Library/Frameworks
else
SDL2_CFLAGS := $(shell $(PKGCONF) --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
SDL2_LIBS   := $(shell $(PKGCONF) --libs sdl2 2>/dev/null || sdl2-config --libs 2>/dev/null)
endif

# ---- Build flags ----
# -m32 keeps pointers and `long` at 32 bits so the Amiga struct layouts
#   (and the static size_check[] asserts) survive on a 64-bit host.
# -include forces swos_platform.h into every TU before any other header.
# -fno-strict-aliasing matches the original Amiga build (heavy type punning).
# Warnings dialed back: the imported Amiga code triggers many style warnings
# that are not bugs on x86/x64 but would otherwise drown the output.
#
# MSYS2/MINGW64 ships 64-bit-only libs and has no multilib, so -m32 fails
# at link time ("skipping incompatible libmingw32.a"). Detect the
# 64-bit-only mingw target via `gcc -dumpmachine` and drop -m32 there.
# Testers wanting an actual 32-bit Windows binary should use MSYS2
# MINGW32 (gcc already 32-bit; -m32 is a no-op) or Cygwin's 32-bit
# toolchain.
ifdef WASM
# wasm32 is ILP32 — no -m flags; emcc targets wasm by default. (Struct
# size_check[] asserts pass on ILP32, same as armhf.)
ARCHFLAGS :=
else ifdef ARM64
# aarch64 is 64-bit (LP64) — no -m32 / -m64 needed. The static
# size_check[] asserts that hard-code Amiga 32-bit struct sizes
# will fail until Phase 5 / 6 lands.
ARCHFLAGS :=
else ifdef ARM
# armhf is already 32-bit (ILP32) — no -m32 needed; passing it would
# fail with "unrecognized command-line option '-m32'" on most ARM gcc
# builds. Force armv7-a to keep things uniform across Pi 2/3/4 etc.
ARCHFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=hard
else ifeq ($(UNAME_S),Darwin)
# Modern macOS is 64-bit (arm64 or x86_64). No -m32. The static
# size_check[] asserts are gated on UINTPTR_MAX (see game_types.h) and
# already pick the 64-bit struct sizes.
#
# MACOS_ARCH: target architecture. Defaults to the host arch (so a plain
# `make` on an M-series Mac produces arm64, on Intel produces x86_64).
# Set explicitly to build a single non-host slice (used by macos-universal
# below). Accepted values match clang's -arch flag: arm64 or x86_64.
#
# MACOS_MIN: minimum deployment target. 11.0 = Big Sur — the oldest macOS
# that supports Apple Silicon natively and that libsdl.org's prebuilt
# SDL2.framework still loads on. Override with MACOS_MIN=12.0 etc. if
# you only need to target newer systems.
MACOS_ARCH ?= $(shell uname -m)
MACOS_MIN  ?= 11.0
ARCHFLAGS := -arch $(MACOS_ARCH) -mmacosx-version-min=$(MACOS_MIN)
else ifeq ($(GCC_MACHINE),x86_64-w64-mingw32)
ARCHFLAGS :=
else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
# Cygwin64's gcc is 64-bit-only; -m32 fails with "unimplemented for
# this target" in <signal.h>. Drop -m32 and let it produce a native
# 64-bit Windows binary (struct sizes go through the Phase 5
# bitness-conditional asserts the same way aarch64 does). Testers
# wanting a 32-bit Windows binary should use MSYS2 MINGW32 instead.
ARCHFLAGS :=
else ifdef X86_64
# Explicit 64-bit Linux x86 build. Drop -m32 and pass -m64 so the
# binary is LP64. Same code path as cygwin64 / aarch64 / macOS for
# the bitness-conditional size_check asserts.
ARCHFLAGS := -m64
else
ARCHFLAGS := -m32
endif
# All Amiga "data module" toggles set to 1 so the C versions of
# the leagues, palettes, sprites, fixtures, etc. are compiled in
# (replacing the SWOS2.s data section). This eliminates a large
# fraction of the zero-filled stubs.
DATA_FLAGS := -DCOMPETITION_DATA_C=1 -DTACTICS_DATA_C=1 -DFIXTURE_DATA_C=1 \
              -DSIMULATION_DATA_C=1 -DPLAYER_DATA_C=1 -DRENDER_DATA_C=1 \
              -DPALETTE_DATA_C=1 -DSTRINGS_DATA_C=1 -DGENERAL_DATA_C=1

# Diagnostic logging defaults to OFF. The bring-up loggers (autoskip
# menu trace, SFX trigger trace) are still in the source — gated on
# SWOS_DEBUG_AUTOSKIP / SWOS_DEBUG_SFX — so a tester chasing a
# regression can re-enable with `make SWOS_DEBUG=1`.
ifdef SWOS_DEBUG
DEBUG_FLAGS := -DSWOS_DEBUG_AUTOSKIP=1 -DSWOS_DEBUG_SFX=1
endif

# Beta-testing convenience: build with `make SWOS_TEST_NO_INJURIES_CARDS=1`
# to suppress in-match yellow/red cards and post-match random injuries.
# Lets the tester hit VIEW RESULT uninterruptedly through a season — no
# substitution menus, no booking prompts. Intentionally compile-time only
# so production builds never accidentally ship with it on.
ifdef SWOS_TEST_NO_INJURIES_CARDS
DEBUG_FLAGS += -DSWOS_TEST_NO_INJURIES_CARDS=1
endif

# Debug: force a World Cup for a national-team career, bypassing the natural
# 4-year competition cycle, so the national-coach WC path can be reproduced on
# demand (e.g. the season-transition 0x8888 backup-clobber in
# _VerifyTeamLoaded). Off by default; enable with `make SWOS_INJECT_WC=1`.
# Only affects national-team career selection (country code 80..85), and is
# compile-time only so production builds never ship with it on. See the inject
# site in src/career_init.c (_InitializeNewSeason).
ifdef SWOS_INJECT_WC
DEBUG_FLAGS += -DSWOS_INJECT_WC=1
endif

# Save-file format toggle. Default ON — new saves are written as JSON
# via mjson (see src/save_format_json.c) and load auto-detects format
# from the file header.
#
# The big overlay-dump saves (.CAR/.SEA/.PRE/.DIY) are now JSON-only on
# BOTH read and write regardless of this flag — their legacy raw-memory-
# dump readers AND writers have been deleted (roadmap step 4 complete).
# The flag now only governs the WRITE format of the small fixed-layout
# records that KEEP binary support (.HIL/.TAC): ON (default) writes them
# JSON-first; =0 writes them in the original binary layout. Their binary
# READ path is compiled either way (load auto-detects), so .HIL/.TAC/.EDT/
# .CFG interop is preserved. See docs/SAVE_DATA.md "Format migration /
# read-support policy".
ifneq ($(SWOS_SAVE_FORMAT_JSON),0)
DEBUG_FLAGS += -DSWOS_SAVE_FORMAT_JSON=1
endif

# Coverage verification for the JSON save format. Default ON during
# bring-up — load-time diff compares the byte image produced by the
# named-blob + semantic field loaders against the bytes from opaque_data.
# A zero-mismatch result means every meaningful byte is covered by typed
# fields/blobs and opaque_data can be removed. Opt out with
# `make SWOS_SAVE_FORMAT_JSON_VERIFY=0` to skip the diff (small load-time
# perf win once we trust the format).
ifneq ($(SWOS_SAVE_FORMAT_JSON_VERIFY),0)
DEBUG_FLAGS += -DSWOS_SAVE_FORMAT_JSON_VERIFY=1
endif

# Profile flag: prints per-phase wall-clock timings (`[json-prof-*]`
# lines) so you can see where load/save time is going. Default OFF —
# profiling is largely done; opt in with `make SWOS_SAVE_FORMAT_JSON_PROFILE=1`
# when investigating save/load perf regressions.
ifdef SWOS_SAVE_FORMAT_JSON_PROFILE
DEBUG_FLAGS += -DSWOS_SAVE_FORMAT_JSON_PROFILE=1
endif

# Legacy-dispatch toggle: build with `make SWOS_SAVE_LEGACY_DISPATCH=1` to
# select the per-field path-lookup loop (slow but the established
# baseline). Default OFF — the new walk-once dispatch is faster.
# Keep around for comparison testing when investigating verify deltas.
ifdef SWOS_SAVE_LEGACY_DISPATCH
DEBUG_FLAGS += -DSWOS_SAVE_LEGACY_DISPATCH=1
endif

CFLAGS  := $(ARCHFLAGS) -O2 -g -std=gnu11 -fno-strict-aliasing -fno-builtin \
           -MMD -MP \
           -D_FILE_OFFSET_BITS=64 $(DEBUG_FLAGS) \
           -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable \
           -Wno-attributes -Wno-multichar -Wno-builtin-declaration-mismatch \
           -Wno-pointer-sign -Wno-int-conversion -Wno-incompatible-pointer-types \
           -Wno-braces -Wno-address-of-packed-member \
           -Wno-strict-aliasing -Wno-array-bounds -Wno-stringop-overflow \
           $(DATA_FLAGS) \
           -include $(INCDIR)/swos_platform.h \
           -I$(SRCDIR) -I$(INCDIR) $(SDL2_CFLAGS)
# -Wno-braces:                 tactics_data.c auto-extractor wraps scalar
#                              initialisers in `{ }`; intentional, ~4200 hits.
# -Wno-address-of-packed-member: packed structs mirror Amiga byte layout
#                              everywhere; we take &field deliberately.
# -Wno-strict-aliasing:        we already pass -fno-strict-aliasing for the
#                              same reason; this just silences the diag.
# -Wno-array-bounds:           Amiga BSS adjacency reads (e.g. extern
#                              uint8_t selTeamsPtr; then read [4]) intentionally
#                              walk past the declared end of 1-byte stubs.
#                              Real adjacency bugs surface as runtime panics —
#                              this warning is too noisy to be useful.
# -Wno-stringop-overflow:      same root cause as -Wno-array-bounds but for
#                              the WRITE side (`((uint8_t *)&selTeamsPtr)[4] = x`).
#                              The compiler sees a 1-byte stub and flags any
#                              wider write; we know the real backing is much
#                              larger.
LDFLAGS := $(ARCHFLAGS)

# macOS: switch to Apple's classic linker (-ld_classic). Modern ld64
# (Xcode 15+) is strict about pointer-relocation alignment and rejects
# packed-struct atoms whose internal pointer fields aren't 8-byte
# aligned — emits errors like "pointer not aligned in _euroCup+0x2E".
# The classic linker behaves like every other linker and accepts the
# misaligned relocations, which is fine at runtime: ARM64 (and x86_64)
# scalar loads cope with misaligned pointers correctly. This keeps the
# struct layout identical across all 64-bit targets (Linux aarch64
# already builds + runs with the same packed structs).
ifeq ($(UNAME_S),Darwin)
LDFLAGS += -Wl,-ld_classic
endif

# Emscripten link flags. ASYNCIFY lets the blocking game loop yield to the
# browser at the one Platform_VBlank choke-point (emscripten_sleep — docs/WASM.md
# §0/§2a). Growable heap; preload the read-only game data into MEMFS; IDBFS for
# persistent saves at /save (mounted in wasm/pre.js). Single-threaded — the
# 20 ms SDL timer + 5 ms audio-pump thread are compiled out (#ifdef SWOS_WASM_BUILD).
ifdef WASM
# EMULATE_FUNCTION_POINTER_CASTS: SWOS's menu system stores void(void) callbacks
# (e.g. FriendlyMenu) in void(MenuEntry*) fields and invokes them through void*
# casts — fine on every native ABI, but wasm hard-traps an indirect call whose
# table signature differs ("indirect call signature mismatch"). This generates
# adapter thunks so the mismatched calls work, like native. wasm-only flag.
LDFLAGS += -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 \
           -sEMULATE_FUNCTION_POINTER_CASTS=1 \
           -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 \
           -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS -lidbfs.js \
           --pre-js wasm/pre.js --shell-file wasm/shell.html \
           --preload-file bin/data@/data \
           --preload-file bin/grafs@/grafs \
           --preload-file bin/sound@/sound \
           --preload-file bin/data_json@/data_json
# Re-link when the pre-js or shell HTML changes (both embedded at link time).
WASM_LINK_DEPS := wasm/pre.js wasm/shell.html
endif

LDLIBS  := $(SDL2_LIBS) -lm

# ---- Targets ----
.PHONY: all clean distclean debug macos-universal macos-app
all: $(TARGET)

debug:
	$(MAKE) DEBUG=1 all

# Ensure bin/ exists, then link. $(WASM_LINK_DEPS) re-links on a pre-js change
# (empty for non-WASM targets).
$(TARGET): $(OBJECTS) $(WASM_LINK_DEPS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

# Create output directory if needed
$(BINDIR):
	mkdir -p $(BINDIR)

# Compile each src/*.c into build/<os>/<arch>/*.o (mirrors src/ subdirs, e.g.
# src/generated/x.c -> $(OBJDIR)/generated/x.o). The mkdir creates the
# per-target object tree on first build.
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile the Windows resource file (icon) on MinGW/Cygwin builds. The output
# is COFF-format and gets linked into the final exe.
$(OBJDIR)/SWOS.res.o: SWOS.rc SWOS.ico
	@mkdir -p $(dir $@)
	$(WINDRES) -O coff $< $@

# Pull in the auto-generated header dependencies (-MMD) so editing a header
# rebuilds just the affected objects — no more blanket `make clean`.
-include $(DEPS)

# clean removes THIS target's objects (build/<os>/<arch>/) + its binary. It also
# sweeps any legacy in-tree *.o from before the build/ move. `distclean` nukes
# every target's objects.
clean:
	rm -rf $(OBJDIR)
	rm -f $(TARGET)
	rm -f $(SRCDIR)/*.o $(SRCDIR)/generated/*.o SWOS.res.o

distclean: clean
	rm -rf build

# ---- macOS universal binary (x86_64 + arm64) ---------------------- *
# Builds two slices via recursive make, lipos them into a single
# bin/SWOS.macos that runs on both Apple Silicon and Intel Macs.
#
# Requires SDL2.framework (universal) under one of:
#   /Library/Frameworks/  ~/Library/Frameworks/
#   /opt/homebrew/Frameworks/  /usr/local/Frameworks/
# (the official libsdl.org dmg installs into /Library/Frameworks/).
#
# Minimum deployment target defaults to macOS 12 (Monterey); override
# with `make macos-universal MACOS_MIN=11.0` etc. if needed.
#
# The slices now build into per-arch dirs (build/macos/x86_64 and
# build/macos/arm64) so they no longer share objects; the `make clean` between
# them is just a precaution (each slice's tree is already independent).
.PHONY: macos-universal
macos-universal:
	@test "$$(uname -s)" = Darwin || { \
	    echo "macos-universal: must run on macOS (got $$(uname -s))" >&2; \
	    exit 1; }
	@test -n "$(SDL2_MACOS_FW)" || { \
	    echo "SDL2.framework not found in any standard location." >&2; \
	    echo "Install from https://github.com/libsdl-org/SDL/releases" >&2; \
	    echo "(SDL2-2.x.x.dmg) and drop SDL2.framework into" >&2; \
	    echo "/Library/Frameworks/. See docs/MACOS_BUILD.md." >&2; \
	    exit 1; }
	@echo "==> [macos-universal] Building x86_64 slice"
	$(MAKE) clean
	$(MAKE) MACOS_ARCH=x86_64
	mv $(BINDIR)/SWOS.macos $(BINDIR)/SWOS.macos.x86_64
	@echo "==> [macos-universal] Building arm64 slice"
	$(MAKE) clean
	$(MAKE) MACOS_ARCH=arm64
	mv $(BINDIR)/SWOS.macos $(BINDIR)/SWOS.macos.arm64
	@echo "==> [macos-universal] Combining slices via lipo"
	lipo -create -output $(BINDIR)/SWOS.macos \
	     $(BINDIR)/SWOS.macos.x86_64 $(BINDIR)/SWOS.macos.arm64
	@lipo -info $(BINDIR)/SWOS.macos
	@echo "==> Universal binary: $(BINDIR)/SWOS.macos"

# ---- macOS .app bundle ------------------------------------------- *
# Builds (or reuses) the universal binary, then assembles a complete
# bin/SWOS.app drag-installable bundle that includes:
#   - the universal SWOS binary at Contents/MacOS/SWOS
#   - the game data dirs (data/ grafs/ grafs_aga/ grafs_dos/ grafs_2020/
#     grafs_arcade/ sound/ save/ data_json/) beside the binary, where the
#     SWOS chdir-to-own-dir startup finds them
#   - SDL2.framework bundled in Contents/Frameworks/ so end users
#     don't need to install SDL2 themselves (the binary's -rpath
#     already points at @executable_path/../Frameworks)
#   - a minimal Info.plist + PkgInfo
#   - an optional SWOS.icns icon if one is present in the checkout
#
# The bundle is unsigned by default — local double-click works, but
# Gatekeeper will warn on first launch when copied to another Mac.
# `codesign --force --deep --sign - bin/SWOS.app` adds an ad-hoc
# signature that lets right-click → Open work without warnings.

# Info.plist content as a multi-line variable (exported so the recipe
# can echo it via $$INFO_PLIST_CONTENT without escaping quotes).
define INFO_PLIST_CONTENT
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>               <string>SWOS</string>
    <key>CFBundleDisplayName</key>        <string>SWOS</string>
    <key>CFBundleIdentifier</key>         <string>com.baker76.swos</string>
    <key>CFBundleVersion</key>            <string>1.0</string>
    <key>CFBundleShortVersionString</key> <string>1.0</string>
    <key>CFBundlePackageType</key>        <string>APPL</string>
    <key>CFBundleSignature</key>          <string>????</string>
    <key>CFBundleExecutable</key>         <string>SWOS</string>
    <key>CFBundleIconFile</key>           <string>SWOS</string>
    <key>LSMinimumSystemVersion</key>     <string>$(MACOS_MIN)</string>
    <key>LSApplicationCategoryType</key>  <string>public.app-category.sports-games</string>
    <key>NSHighResolutionCapable</key>    <true/>
</dict>
</plist>
endef
export INFO_PLIST_CONTENT

macos-app: macos-universal
	@test "$$(uname -s)" = Darwin || { \
	    echo "macos-app: must run on macOS (got $$(uname -s))" >&2; \
	    exit 1; }
	@echo "==> [macos-app] Assembling $(BINDIR)/SWOS.app"
	@APP=$(BINDIR)/SWOS.app; \
	  rm -rf $$APP; \
	  mkdir -p $$APP/Contents/MacOS \
	           $$APP/Contents/Frameworks \
	           $$APP/Contents/Resources; \
	  echo "    binary: Contents/MacOS/SWOS"; \
	  cp $(BINDIR)/SWOS.macos $$APP/Contents/MacOS/SWOS; \
	  chmod +x $$APP/Contents/MacOS/SWOS; \
	  if [ -d "$(SDL2_MACOS_FW)/SDL2.framework" ]; then \
	    echo "    bundling SDL2.framework from $(SDL2_MACOS_FW)"; \
	    cp -R "$(SDL2_MACOS_FW)/SDL2.framework" $$APP/Contents/Frameworks/; \
	  else \
	    echo "    WARNING: SDL2.framework not found — user will need to install it system-wide" >&2; \
	  fi; \
	  for d in data grafs grafs_aga grafs_dos grafs_2020 grafs_arcade sound save data_json; do \
	    if [ -d "$(BINDIR)/$$d" ]; then \
	      echo "    copying $$d/"; \
	      cp -R "$(BINDIR)/$$d" $$APP/Contents/MacOS/; \
	    fi; \
	  done; \
	  if   [ -f SWOS.icns ];           then cp SWOS.icns           $$APP/Contents/Resources/; \
	  elif [ -f $(BINDIR)/SWOS.icns ]; then cp $(BINDIR)/SWOS.icns $$APP/Contents/Resources/; \
	  else echo "    (no SWOS.icns — Finder will use the default app icon)"; fi; \
	  printf 'APPL????' > $$APP/Contents/PkgInfo; \
	  echo "$$INFO_PLIST_CONTENT" > $$APP/Contents/Info.plist; \
	  xattr -cr $$APP 2>/dev/null || true; \
	  echo ""; \
	  echo "==> bin/SWOS.app ready"; \
	  echo "    archs: $$(lipo -info $$APP/Contents/MacOS/SWOS 2>/dev/null | sed 's/^.*: //')"; \
	  echo "    size:  $$(du -sh $$APP | cut -f1)"; \
	  echo ""; \
	  echo "    Run:        open $$APP"; \
	  echo "    Ad-hoc sign: codesign --force --deep --sign - $$APP"; \
	  echo "    Distribute: hdiutil create -fs HFS+ -srcfolder $$APP -volname SWOS $(BINDIR)/SWOS.dmg"