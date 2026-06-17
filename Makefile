# to generate assembler listing with LTO, add to LDFLAGS: -Wa,-adhln=$@.listing,--listing-rhs-width=200
# for better annotations add -dA -dP
# to generate assembler source with LTO, add to LDFLAGS: -save-temps=cwd

ifdef OS
	WINDOWS = 1
	SHELL = cmd.exe
endif

# Data module flags: set to 1 to use C data, 0 to use ASM data
COMPETITION_DATA_C ?= 1   # worldCup, copaAmerica, cupPick*, careerContests, etc.
TACTICS_DATA_C     ?= 1   # positions*, sidePitchPositions*, tacticPosMap*, teamTactics*
FIXTURE_DATA_C     ?= 1   # fixtureSchedule2..24Teams
SIMULATION_DATA_C  ?= 1   # sim*Thresh, goalScorerDist*, quadrant*, skill*
PLAYER_DATA_C      ?= 1   # playerCardChances*, playerPositions*
RENDER_DATA_C      ?= 1   # opponentCoachFrames, ourCoachFrames
PALETTE_DATA_C     ?= 1   # frozenPalette/muddyPalette/etc.
STRINGS_DATA_C     ?= 1   # 244 aXxxx strings
GENERAL_DATA_C     ?= 1   # countriesTable, kTeamColorsTable, etc.

DATA_FLAGS = -DCOMPETITION_DATA_C=$(COMPETITION_DATA_C) -DTACTICS_DATA_C=$(TACTICS_DATA_C) -DFIXTURE_DATA_C=$(FIXTURE_DATA_C) -DSIMULATION_DATA_C=$(SIMULATION_DATA_C) -DPLAYER_DATA_C=$(PLAYER_DATA_C) -DRENDER_DATA_C=$(RENDER_DATA_C) -DPALETTE_DATA_C=$(PALETTE_DATA_C) -DSTRINGS_DATA_C=$(STRINGS_DATA_C) -DGENERAL_DATA_C=$(GENERAL_DATA_C)

subdirs := $(filter-out data/,$(wildcard */))
VPATH = $(subdirs)
cpp_sources := $(wildcard *.cpp) $(wildcard $(addsuffix *.cpp,$(subdirs)))
cpp_objects := $(addprefix obj/,$(patsubst %.cpp,%.o,$(notdir $(cpp_sources))))
c_sources := $(filter-out support/gcc8_c_support.c main.c,$(wildcard *.c) $(wildcard $(addsuffix *.c,$(subdirs))))
c_objects := $(addprefix obj/,$(patsubst %.c,%.o,$(notdir $(c_sources))))
s_sources := support/gcc8_a_support.s
s_objects := $(addprefix obj/,$(patsubst %.s,%.o,$(notdir $(s_sources))))
swos_asm_sources := asm/resload_unit.s
swos_asm_objects := $(addprefix obj/,$(patsubst %.s,%.o,$(notdir $(swos_asm_sources))))
vasm_sources := $(wildcard *.asm) $(wildcard $(addsuffix *.asm, $(subdirs)))
vasm_objects := $(addprefix obj/, $(patsubst %.asm,%.o,$(notdir $(vasm_sources))))
objects := $(cpp_objects) $(c_objects) $(s_objects) $(swos_asm_objects) $(vasm_objects)

# https://stackoverflow.com/questions/4036191/sources-from-subdirectories-in-makefile/4038459
# http://www.microhowto.info/howto/automatically_generate_makefile_dependencies.html

program ?= uae/dh0/data/SWOS2
OUT = $(program)
LDSCRIPT = SWOS2.ld
CC = m68k-amiga-elf-gcc
AS = m68k-amiga-elf-as
VASM = vasmm68k_mot

ifdef WINDOWS
	SDKDIR = $(abspath $(dir $(shell where $(CC)))..\m68k-amiga-elf\sys-include)
else
	SDKDIR = $(abspath $(dir $(shell which $(CC)))../m68k-amiga-elf/sys-include)
endif

# Optimization is parameterized so a gdb-friendly build can override it.
# DBG=1 -> -O0: precise line breakpoints (no reordering) and every local/static
# lives in memory (not elided into registers), which the gdb-driven test harness
# needs to drive the Amiga UI via input injection reliably (-Ofast reordering
# defeats it). Same struct/ABI layout as the release build, so save-format
# cross-target tests stay valid. NOTE: keep -fomit-frame-pointer even under DBG
# — -fno-omit-frame-pointer ICEs GCC 15.1.0 (dwarf2out_frame_debug_adjust_cfa);
# -g already emits CFI so gdb can still unwind without a frame pointer.
OPT = -Ofast
FP  = -fomit-frame-pointer
ifdef DBG
OPT = -O0
endif
CCFLAGS   = -g -MP -MMD -m68020 $(OPT) -nostdlib -Wextra -Wno-unused-function -Wno-volatile-register-var -Wno-stringop-overflow $(FP) -fno-tree-loop-distribution -fno-strict-aliasing -fno-exceptions -fno-optimize-sibling-calls -ffunction-sections -fdata-sections -Iinclude $(DATA_FLAGS)

# DUMP_ASM=1 → save preprocessed .i and assembly .s next to each .o.
# Slight build slowdown; output lands in obj/ alongside .o files.
DUMP_ASM ?= 1
ifeq ($(DUMP_ASM),1)
CCFLAGS   += -save-temps=obj -fverbose-asm
endif
CPPFLAGS  = $(CCFLAGS) -fno-rtti -fcoroutines -fno-use-cxa-atexit
ASFLAGS   = -mcpu=68020 -g --register-prefix-optional -I$(SDKDIR)
LDFLAGS   = -Wl,--emit-relocs,--no-check-sections,-e,main,-Map=$(OUT).map,-T,$(LDSCRIPT)
VASMFLAGS = -m68020 -Felf -opt-fconst -nowarn=62 -dwarf=3 -quiet -x -I. -I$(SDKDIR) $(DATA_FLAGS)

all: $(OUT)

$(OUT): $(OUT).elf
	$(info Elf2Hunk $(program))
ifdef WINDOWS
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
else
	@mkdir -p "$(dir $@)"
endif
	@elf2hunk $(OUT).elf $(OUT)
#ifdef WINDOWS
#	@python $(CURDIR)/fix_hunk_relocs.py $(OUT).elf $(OUT)
#else
#	@python3 $(CURDIR)/fix_hunk_relocs.py $(OUT).elf $(OUT)
#endif

$(OUT).elf: $(objects)
	$(info Linking $(program).elf)
ifdef WINDOWS
	@if not exist out mkdir out
else
	@mkdir -p out
endif
	@$(CC) $(CCFLAGS) $(LDFLAGS) $(objects) -o $@

clean:
	$(info Cleaning...)
ifdef WINDOWS
	-@if exist obj\* del /q obj\* 2>nul
	-@if exist out\* del /q out\* 2>nul
	-@if exist "$(subst /,\,$(OUT))" del /q "$(subst /,\,$(OUT))" 2>nul
	-@if exist "$(subst /,\,$(OUT)).elf" del /q "$(subst /,\,$(OUT)).elf" 2>nul
	-@if exist "$(subst /,\,$(OUT)).map" del /q "$(subst /,\,$(OUT)).map" 2>nul
else
	@$(RM) obj/* out/* $(OUT) $(OUT).elf $(OUT).map
endif

dump-swos-c-asm:
ifdef WINDOWS
	@if not exist out mkdir out
else
	@mkdir -p out
endif
	@$(CC) $(CCFLAGS) -fverbose-asm -S -o out/SWOS.c.s $(CURDIR)/src/SWOS.c

# Emit .s assembly for every C source into obj/, using whatever flags
# the regular build would use for that translation unit. Pure inspection
# target; doesn't touch .o files or relink the binary.
c_asm_outputs := $(addprefix obj/,$(patsubst %.c,%.s,$(notdir $(c_sources))))
dump-asm: $(c_asm_outputs)
	$(info Done dumping assembly to obj/*.s)

$(c_asm_outputs) : obj/%.s : %.c
	$(info Asm-only $<)
	@$(CC) $(if $(filter obj/diy_competition.s,$@),$(CCFLAGS_O2),$(if $(filter obj/game_timers.s obj/update_players.s obj/sprite_data.s obj/menu_data.s obj/competition_data.s obj/tactics_data.s obj/fixture_data.s obj/simulation_data.s obj/player_data.s obj/render_data.s obj/palette_data.s obj/strings_data.s obj/general_data.s,$@),$(CCFLAGS_NOLTO),$(CCFLAGS))) -fverbose-asm -S -o $@ $(CURDIR)/$<

-include $(objects:.o=.d)

$(cpp_objects) : obj/%.o : %.cpp
	$(info Compiling $<)
	@$(CC) $(CPPFLAGS) -c -o $@ $(CURDIR)/$<

# game_timers.c triggers GCC ICE in LTO CCP pass — compile without LTO
CCFLAGS_NOLTO = $(subst -flto,,$(subst -fwhole-program,,$(CCFLAGS)))

# diy_competition.c has complex nested loops that -Ofast miscompiles — use -O2
CCFLAGS_O2 = $(subst -Ofast,-O2,$(CCFLAGS))

$(c_objects) : obj/%.o : %.c
	$(info Compiling $<)
	@$(CC) $(if $(filter obj/diy_competition.o,$@),$(CCFLAGS_O2),$(if $(filter obj/game_timers.o obj/update_players.o obj/sprite_data.o obj/menu_data.o obj/competition_data.o obj/tactics_data.o obj/fixture_data.o obj/simulation_data.o obj/player_data.o obj/render_data.o obj/palette_data.o obj/strings_data.o obj/general_data.o,$@),$(CCFLAGS_NOLTO),$(CCFLAGS))) -c -o $@ $(CURDIR)/$<

$(s_objects): obj/%.o : %.s
	$(info Assembling $<)
	@$(AS) $(ASFLAGS) --MD $(@D)/$*.d -o $@ $(CURDIR)/$<

$(swos_asm_objects): obj/%.o : %.s
	$(info Assembling $<)
	@$(VASM) $(VASMFLAGS) -dependall=make -depfile $(@D)/$*.d -o $@ $(CURDIR)/$<

$(vasm_objects): obj/%.o : %.asm
	$(info Assembling $<)
	@$(VASM) $(VASMFLAGS) -dependall=make -depfile $(@D)/$*.d -o $@ $(CURDIR)/$<
