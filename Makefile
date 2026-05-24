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
ifdef ARM64
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
# Homebrew installs SDL2 under /opt/homebrew (arm64) or /usr/local
# (intel). brew --prefix returns the right one. If pkg-config exists
# in PATH it will already pick the right .pc up; fall back to
# sdl2-config below if not.
PKGCONF := PKG_CONFIG_PATH=$(shell brew --prefix sdl2 2>/dev/null)/lib/pkgconfig pkg-config
else
CC := gcc
endif
endif
SHELL := /bin/bash

# ---- Platform ----
UNAME_S := $(shell uname -s)

# ---- Output ----
ifeq ($(OS),Windows_NT)
TARGET_NAME := SWOS.exe
else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
# uname -s on Cygwin outputs `CYGWIN_NT-10.0-22631` (or similar with
# the Windows build number suffix), not bare `CYGWIN_NT`. Use
# `findstring` so the version suffix doesn't break detection.
TARGET_NAME := SWOS.exe
else ifneq (,$(findstring MINGW,$(UNAME_S)))
# Same suffix issue for MSYS2 MINGW32_NT-X.Y / MINGW64_NT-X.Y.
TARGET_NAME := SWOS.exe
else ifdef ARM64
TARGET_NAME := SWOS.aarch64
else ifdef ARM
# Distinguish the cross-built ARM binary so it doesn't overwrite a
# native x86 bin/SWOS in the same checkout. Native ARM builds (on a
# Pi) also get the .armhf suffix — `file bin/SWOS.armhf` confirms the
# ELF arch either way.
TARGET_NAME := SWOS.armhf
else ifeq ($(UNAME_S),Darwin)
TARGET_NAME := SWOS.macos
else
TARGET_NAME := SWOS
endif

BINDIR := bin
TARGET := $(BINDIR)/$(TARGET_NAME)

# ---- Sources ----
SRCDIR  := src
SOURCES := $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/generated/*.c)
OBJECTS := $(SOURCES:.c=.o)
DEPS    := $(SOURCES:.c=.d)

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
WIN_RES_OBJ := SWOS.res.o
OBJECTS     += $(WIN_RES_OBJ)
else ifneq (,$(findstring cygwin,$(GCC_MACHINE)))
WIN_RES_OBJ := SWOS.res.o
OBJECTS     += $(WIN_RES_OBJ)
endif

# ---- Helpers ----
PKGCONF ?= pkg-config

SDL2_CFLAGS := $(shell $(PKGCONF) --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
SDL2_LIBS   := $(shell $(PKGCONF) --libs sdl2 2>/dev/null || sdl2-config --libs 2>/dev/null)

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
ifdef ARM64
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
# size_check[] asserts will fail until Phase 5 / 6 (same as ARM64).
ARCHFLAGS :=
else ifeq ($(GCC_MACHINE),x86_64-w64-mingw32)
ARCHFLAGS :=
else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
# Cygwin64's gcc is 64-bit-only; -m32 fails with "unimplemented for
# this target" in <signal.h>. Drop -m32 and let it produce a native
# 64-bit Windows binary (struct sizes go through the Phase 5
# bitness-conditional asserts the same way aarch64 does). Testers
# wanting a 32-bit Windows binary should use MSYS2 MINGW32 instead.
ARCHFLAGS :=
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

# Save-file format toggle. Default OFF — saves are written in the
# legacy raw-memory-dump format the original Amiga SWOS used. Build
# with `make SWOS_SAVE_FORMAT_JSON=1` to write new saves as JSON via
# mjson (see src/save_format_json.c). Load auto-detects format from
# the file header so existing saves keep working in either mode.
# Cleanup: once JSON is proven for all save types we delete the
# legacy paths and the toggle.
ifdef SWOS_SAVE_FORMAT_JSON
DEBUG_FLAGS += -DSWOS_SAVE_FORMAT_JSON=1
endif

# Coverage verification for the JSON save format. Set
# `make SWOS_SAVE_FORMAT_JSON=1 SWOS_SAVE_FORMAT_JSON_VERIFY=1` to
# enable a load-time diff that compares the byte image produced by
# the named-blob + semantic field loaders against the bytes from
# opaque_data. A zero-mismatch result means every meaningful byte
# is covered by typed fields/blobs and opaque_data can be removed.
ifdef SWOS_SAVE_FORMAT_JSON_VERIFY
DEBUG_FLAGS += -DSWOS_SAVE_FORMAT_JSON_VERIFY=1
endif

# Profile flag: prints per-phase wall-clock timings (`[json-prof-*]`
# lines) so you can see where load/save time is going. Useful when
# the format feels slow.
ifdef SWOS_SAVE_FORMAT_JSON_PROFILE
DEBUG_FLAGS += -DSWOS_SAVE_FORMAT_JSON_PROFILE=1
endif

CFLAGS  := $(ARCHFLAGS) -O2 -g -std=gnu11 -fno-strict-aliasing -fno-builtin \
           -D_FILE_OFFSET_BITS=64 $(DEBUG_FLAGS) \
           -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable \
           -Wno-attributes -Wno-multichar -Wno-builtin-declaration-mismatch \
           -Wno-pointer-sign -Wno-int-conversion -Wno-incompatible-pointer-types \
           -Wno-braces -Wno-address-of-packed-member \
           -Wno-strict-aliasing -Wno-array-bounds -Wno-stringop-overflow \
           $(DATA_FLAGS) \
           -include $(SRCDIR)/swos_platform.h \
           -I$(SRCDIR) $(SDL2_CFLAGS)
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
LDLIBS  := $(SDL2_LIBS) -lm

# ---- Targets ----
.PHONY: all clean debug
all: $(TARGET)

debug:
	$(MAKE) DEBUG=1 all

# Ensure bin/ exists, then link
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

# Create output directory if needed
$(BINDIR):
	mkdir -p $(BINDIR)

# Compile each .c into a .o (supports subdirectories)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile the Windows resource file (icon) on MinGW builds. The output
# is COFF-format and gets linked into the final exe.
SWOS.res.o: SWOS.rc SWOS.ico
	$(WINDRES) -O coff $< $@

clean:
	rm -f $(TARGET) $(OBJECTS) SWOS.res.o