# OpenSWOS -- the Sega Mega Drive / Genesis target.
#
#   make -f Makefile.md              # dist/SWOS-MD.bin
#   make -f Makefile.md test         # boot it headless and run the checks (scripts/md/test_md.sh)
#   make -f Makefile.md clean
#
# A 68000 (no 68020 instructions), 64 KB of work RAM, the VDP for all drawing: the
# shared match engine in src/ is compiled for it unchanged where it can be, with
# SWOS_MD_BUILD set, and src/md/ is the platform layer (docs/MEGADRIVE.md).
#
# Toolchain: any m68k gcc. A distro's m68k-linux-gnu-gcc (Debian/Ubuntu package
# gcc-m68k-linux-gnu) works -- we link no libc and no libgcc (src/md/md_libgcc.s), so
# its Linux userland is never used. An m68k-elf-gcc (SGDK, marsdev) works too:
#   make -f Makefile.md MD_PREFIX=m68k-elf-
#
# Game data: the ROM is built from the player's own SWOS data (the assets folder the
# data setup fills, $(SWOS_ASSETS)), converted by scripts/md/make_md_data.py. Without
# it the ROM still builds and plays, with the converter's stand-in art and teams.

MD_PREFIX ?= $(shell command -v m68k-elf-gcc >/dev/null 2>&1 && echo m68k-elf- || echo m68k-linux-gnu-)
CC      = $(MD_PREFIX)gcc
AS      = $(MD_PREFIX)as
OBJCOPY = $(MD_PREFIX)objcopy
SIZE    = $(MD_PREFIX)size
NM      = $(MD_PREFIX)nm
PYTHON ?= python3

OBJDIR  = build/md/obj
GENDIR  = build/md
ROM     = dist/SWOS-MD.bin
ELF     = build/md/SWOS-MD.elf

# The player's data (same default as the SDL build: the per-user assets folder).
SWOS_ASSETS ?= $(HOME)/.local/share/OpenSWOS/assets

# The match engine: the shared sources the Mega Drive links. Everything else in src/
# (menus, career, saves, SDL, netplay...) is not part of this target -- the cartridge's
# front end is src/md/md_frontend.c.
ENGINE_SRCS := \
    adverts.c ai.c animated_patterns.c ball_physics.c ball_possession.c bench_subs.c \
    camera.c dos_gameplay.c game_hud.c game_init.c game_loop.c game_state.c game_timers.c \
    game_utils.c goal_scored.c goalkeeper.c input.c match_setup.c math68k.c move_camera.c \
    panic.c player_anim.c player_positioning.c player_sprites.c random.c referee.c \
    sfx_crowd.c string_utils.c tackle_foul.c team_colors.c team_control.c teams.c \
    test_rules.c update_players.c headers.c statistics.c highlight.c color_sprites.c \
    training_match.c career_status.c career_settings.c career_trophy_flags.c \
    career_match_state.c career_setup.c career_team_scalars.c career_unavailable_players.c \
    match_sim.c edit_tactics.c diy_competition.c career_params.c menus.c \
    sprite_data.c tactics_data.c simulation_data.c player_data.c render_data.c \
    palette_data.c general_data.c strings_data.c swos_globals.c swos_amiga_data_stubs.c \
    swos_data_typed_stubs.c swos_data_bulk_imports.c swos_amiga_data_imports.c \
    swos_amiga_ptr_imports.c swos_amiga_strings.c

MD_C_SRCS := $(wildcard src/md/*.c)
MD_S_SRCS := $(wildcard src/md/*.s)
GEN_C_SRCS := $(GENDIR)/md_font.c $(GENDIR)/md_data.c

OBJS := $(addprefix $(OBJDIR)/md/,$(notdir $(MD_S_SRCS:.s=.o))) \
        $(addprefix $(OBJDIR)/md/,$(notdir $(MD_C_SRCS:.c=.o))) \
        $(addprefix $(OBJDIR)/gen/,$(notdir $(GEN_C_SRCS:.c=.o))) \
        $(addprefix $(OBJDIR)/engine/,$(ENGINE_SRCS:.c=.o))

# size_t/ptrdiff_t: the shared headers (swos_types.h) type them as on the Amiga
# toolchain, `long`; a Linux-targeted m68k gcc says `int`. Same width, but C wants one.
TYPEFIX = -U__SIZE_TYPE__ '-D__SIZE_TYPE__=long unsigned int' -U__PTRDIFF_TYPE__ '-D__PTRDIFF_TYPE__=long int'
# -mstrict-align -fno-store-merging -fdisable-tree-bswap: a 68000 faults on a word or long
# access at an odd address, and a gcc built for Linux (68020+, which allows it) still merges
# adjacent byte loads and stores into word ones at odd offsets even with -mstrict-align --
# BE_READ_U16(p + 1) became `move.w 1(a0)`. The A1200 never noticed; the Mega Drive does.
CFLAGS  = -m68000 -mstrict-align -fno-store-merging -fdisable-tree-bswap -O3 -fomit-frame-pointer -ffreestanding -fno-builtin-printf -fno-strict-aliasing \
          -fno-tree-loop-distribution -ffunction-sections -fdata-sections -fno-common \
          -fno-pic -fno-pie \
          -Wall -Wno-unused-function -MMD -MP \
          -DSWOS_MD_BUILD=1 -Iinclude -Isrc/md -I$(GENDIR) $(TYPEFIX) $(EXTRA_CFLAGS)
ENGINE_CFLAGS = $(CFLAGS) -w -DCOMPETITION_DATA_C=1 -DTACTICS_DATA_C=1 -DFIXTURE_DATA_C=1 \
          -DSIMULATION_DATA_C=1 -DPLAYER_DATA_C=1 -DRENDER_DATA_C=1 -DPALETTE_DATA_C=1 \
          -DSTRINGS_DATA_C=1 -DGENERAL_DATA_C=1
ASFLAGS = -m68000 --register-prefix-optional --noexecstack
LDFLAGS = -m68000 -nostdlib -nostartfiles -no-pie -Wl,--gc-sections -Wl,-T,src/md/md.ld \
          -Wl,-Map=build/md/SWOS-MD.map -Wl,--no-warn-rwx-segments -Wl,--cref

all: $(ROM)

$(ROM): $(ELF) scripts/md/fix_header.py
	@mkdir -p dist
	$(OBJCOPY) -O binary $(ELF) $@
	@$(PYTHON) scripts/md/fix_header.py $@
	@$(SIZE) -A $(ELF) | awk '/^\.(text|rodata|data|bss|debuglog)/{printf "  %-10s %7d\n", $$1, $$2}'

$(ELF): $(OBJS) src/md/md.ld $(GENDIR)/md_romdata.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(GENDIR)/md_romdata.ld: scripts/md/rom_tables.txt scripts/md/gen_romdata_ld.py
	@mkdir -p $(GENDIR)
	$(PYTHON) scripts/md/gen_romdata_ld.py scripts/md/rom_tables.txt $@

# The game data (scripts/md/make_md_data.py): from MD_EXPORT, the directory the SDL
# build's `--md-export` writes from the player's SWOS data; stand-in art without one.
MD_EXPORT ?= build/md/export
$(GENDIR)/md_data.c: scripts/md/make_md_data.py src/sprite_data.c $(wildcard $(MD_EXPORT)/*.bin)
	@mkdir -p $(GENDIR)
	$(PYTHON) scripts/md/make_md_data.py --out $@ $(if $(wildcard $(MD_EXPORT)/sprites.bin),--export $(MD_EXPORT))

$(GENDIR)/md_font.c: scripts/md/make_font.py
	@mkdir -p $(GENDIR)
	$(PYTHON) scripts/md/make_font.py $@

$(OBJDIR)/md/%.o: src/md/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/md/%.o: src/md/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(OBJDIR)/gen/%.o: $(GENDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -w -c -o $@ $<

$(OBJDIR)/engine/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ENGINE_CFLAGS) -c -o $@ $<

# A changed flag rebuilds every object, as in the other two makefiles (CLAUDE.md "A -D flag
# change rebuilds every object"): make compares timestamps, not flags.
BUILD_FLAGS_STAMP := $(OBJDIR)/.build-flags
BUILD_FLAGS_NOW   := $(strip $(CC) $(CFLAGS) :: $(ENGINE_CFLAGS) :: $(ASFLAGS))
ifneq ($(BUILD_FLAGS_NOW),$(strip $(shell cat $(BUILD_FLAGS_STAMP) 2>/dev/null)))
$(shell mkdir -p $(OBJDIR) && printf '%s\n' '$(BUILD_FLAGS_NOW)' > $(BUILD_FLAGS_STAMP))
endif
$(BUILD_FLAGS_STAMP):
	@mkdir -p $(OBJDIR)
	@printf '%s\n' '$(BUILD_FLAGS_NOW)' > $@
$(OBJS): $(BUILD_FLAGS_STAMP)

harness:
	sh scripts/md/build_harness.sh

test: $(ROM)
	sh scripts/md/test_md.sh

clean:
	rm -rf build/md/obj build/md/SWOS-MD.elf build/md/SWOS-MD.map build/md/*.c build/md/*.ld $(ROM)

.PHONY: all clean test harness
-include $(OBJS:.o=.d)
