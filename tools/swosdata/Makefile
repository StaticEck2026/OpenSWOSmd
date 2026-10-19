# swosdata -- the data setup's conversion library and its command-line tool.
#
#   make -C tools/swosdata            # build/swosdata (the CLI) and the library
#   make -C tools/swosdata check      # convert, then compare with the reference trees
#
# The Slint setup app links the same sources through CMakeLists.txt.

ROOT    := ../..
OUT     := build
CC      ?= gcc
CFLAGS  ?= -O2 -g
# ARCHFLAGS: e.g. "-arch x86_64" for one slice of a macOS universal build.
ARCHFLAGS ?=
CFLAGS  += $(ARCHFLAGS) -std=c99 -Wall -Wextra -Wno-unused-parameter -Ilib -I$(ROOT)/include -I$(ROOT)/src

LIB_SRCS := $(wildcard lib/*.c) $(ROOT)/src/rnc.c
LIB_OBJS := $(patsubst %.c,$(OUT)/obj/%.o,$(notdir $(LIB_SRCS)))

vpath %.c lib $(ROOT)/src cli test

all: $(OUT)/swosdata

$(OUT)/obj/%.o: %.c | $(OUT)/obj
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(OUT)/obj:
	mkdir -p $@

$(OUT)/libswosdata.a: $(LIB_OBJS)
	ar rcs $@ $^

$(OUT)/swosdata: $(OUT)/obj/swosdata_cli.o $(OUT)/libswosdata.a
	$(CC) $(CFLAGS) $^ -o $@

$(OUT)/%_test: $(OUT)/obj/%_test.o $(OUT)/libswosdata.a
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(OUT)

-include $(wildcard $(OUT)/obj/*.d)

.PHONY: all clean
