# ---- Toolchain ----
# Cross-compile to ARM 32-bit (armv7-a / armhf) with `make ARM=1`. Picks
# the standard Debian/Ubuntu cross-toolchain prefix; override with
# `make ARM=1 CC=...` if your distro uses a different name. The build
# expects libsdl2-dev:armhf installed (or a sysroot containing SDL2),
# discoverable via `arm-linux-gnueabihf-pkg-config`. The Amiga struct
# size_check[] asserts assume 32-bit pointers, which armhf already
# provides — no -m32 needed.
ifdef ARM
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
CC := gcc
endif
SHELL := /bin/bash

# ---- Platform ----
UNAME_S := $(shell uname -s)

# ---- Output ----
ifeq ($(OS),Windows_NT)
TARGET_NAME := SWOS.exe
else ifeq ($(UNAME_S),CYGWIN_NT)
TARGET_NAME := SWOS.exe
else ifeq ($(UNAME_S),MINGW32_NT)
TARGET_NAME := SWOS.exe
else ifeq ($(UNAME_S),MINGW64_NT)
TARGET_NAME := SWOS.exe
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
# On MinGW we compile SWOS.rc → SWOS.res.o via windres and link it in,
# which gives the .exe the SWOS.ico for both the file icon (Explorer)
# and the SDL window/taskbar icon (SDL2 picks up resource ID 1 when
# no SDL_SetWindowIcon is called).
GCC_MACHINE := $(shell $(CC) -dumpmachine 2>/dev/null)
WINDRES ?= windres
ifneq (,$(findstring mingw,$(GCC_MACHINE)))
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
ifdef ARM
# armhf is already 32-bit (ILP32) — no -m32 needed; passing it would
# fail with "unrecognized command-line option '-m32'" on most ARM gcc
# builds. Force armv7-a to keep things uniform across Pi 2/3/4 etc.
ARCHFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=hard
else ifeq ($(GCC_MACHINE),x86_64-w64-mingw32)
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

CFLAGS  := $(ARCHFLAGS) -O2 -g -std=gnu11 -fno-strict-aliasing -fno-builtin \
           -D_FILE_OFFSET_BITS=64 $(DEBUG_FLAGS) \
           -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable \
           -Wno-attributes -Wno-multichar -Wno-builtin-declaration-mismatch \
           -Wno-pointer-sign -Wno-int-conversion -Wno-incompatible-pointer-types \
           $(DATA_FLAGS) \
           -include $(SRCDIR)/swos_platform.h \
           -I$(SRCDIR) $(SDL2_CFLAGS)
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