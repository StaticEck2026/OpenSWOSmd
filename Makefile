# to generate assembler listing with LTO, add to LDFLAGS: -Wa,-adhln=$@.listing,--listing-rhs-width=200
# for better annotations add -dA -dP
# to generate assembler source with LTO, add to LDFLAGS: -save-temps=cwd

ifdef OS
	WINDOWS = 1
	SHELL = cmd.exe
endif

subdirs := $(filter-out data/,$(wildcard */))
VPATH = $(subdirs)
cpp_sources := $(wildcard *.cpp) $(wildcard $(addsuffix *.cpp,$(subdirs)))
cpp_objects := $(addprefix obj/,$(patsubst %.cpp,%.o,$(notdir $(cpp_sources))))
c_sources := $(filter-out support/gcc8_c_support.c main.c,$(wildcard *.c) $(wildcard $(addsuffix *.c,$(subdirs))))
c_objects := $(addprefix obj/,$(patsubst %.c,%.o,$(notdir $(c_sources))))
s_sources := support/gcc8_a_support.s support/depacker_doynax.s
s_objects := $(addprefix obj/,$(patsubst %.s,%.o,$(notdir $(s_sources))))
swos_asm_sources := asm/swos2_unit.s asm/resload_unit.s
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

CCFLAGS   = -g -MP -MMD -m68020 -Ofast -nostdlib -Wextra -Wno-unused-function -Wno-volatile-register-var -fomit-frame-pointer -fno-tree-loop-distribution -flto -fwhole-program -fno-exceptions -ffunction-sections -fdata-sections -Iinclude
CPPFLAGS  = $(CCFLAGS) -fno-rtti -fcoroutines -fno-use-cxa-atexit
ASFLAGS   = -mcpu=68020 -g --register-prefix-optional -I$(SDKDIR)
LDFLAGS   = -Wl,--emit-relocs,--no-check-sections,-e,main,-Map=$(OUT).map,-T,$(LDSCRIPT)
VASMFLAGS = -m68020 -Felf -opt-fconst -nowarn=62 -dwarf=3 -quiet -x -I. -I$(SDKDIR)

all: $(OUT)

$(OUT): $(OUT).elf
	$(info Elf2Hunk $(program))
ifdef WINDOWS
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
else
	@mkdir -p "$(dir $@)"
endif
	@elf2hunk $(OUT).elf $(OUT)

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

-include $(objects:.o=.d)

$(cpp_objects) : obj/%.o : %.cpp
	$(info Compiling $<)
	@$(CC) $(CPPFLAGS) -c -o $@ $(CURDIR)/$<

# game_timers.c triggers GCC ICE in LTO CCP pass — compile without LTO
CCFLAGS_NOLTO = $(subst -flto,,$(subst -fwhole-program,,$(CCFLAGS)))

$(c_objects) : obj/%.o : %.c
	$(info Compiling $<)
	@$(CC) $(if $(filter obj/game_timers.o obj/update_players.o,$@),$(CCFLAGS_NOLTO),$(CCFLAGS)) -c -o $@ $(CURDIR)/$<

$(s_objects): obj/%.o : %.s
	$(info Assembling $<)
	@$(AS) $(ASFLAGS) --MD $(@D)/$*.d -o $@ $(CURDIR)/$<

$(swos_asm_objects): obj/%.o : %.s
	$(info Assembling $<)
	@$(VASM) $(VASMFLAGS) -dependall=make -depfile $(@D)/$*.d -o $@ $(CURDIR)/$<

$(vasm_objects): obj/%.o : %.asm
	$(info Assembling $<)
	@$(VASM) $(VASMFLAGS) -dependall=make -depfile $(@D)/$*.d -o $@ $(CURDIR)/$<
