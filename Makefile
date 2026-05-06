# ---- Toolchain ----
CC := gcc
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

# ---- Helpers ----
PKGCONF := pkg-config

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
GCC_MACHINE := $(shell $(CC) -dumpmachine 2>/dev/null)
ifeq ($(GCC_MACHINE),x86_64-w64-mingw32)
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

CFLAGS  := $(ARCHFLAGS) -O2 -g -std=gnu11 -fno-strict-aliasing -fno-builtin \
           -D_FILE_OFFSET_BITS=64 -DSWOS_DEBUG_AUTOSKIP=1 -DSWOS_DEBUG_SFX=1 \
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

clean:
	rm -f $(TARGET) $(OBJECTS)