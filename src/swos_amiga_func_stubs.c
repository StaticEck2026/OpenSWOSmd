/*
 * swos_amiga_func_stubs.c -- C stubs for Amiga assembly functions that
 * are still referenced by the ported C code but have no C definition.
 *
 * Each one is a no-op. They will be replaced with real SDL-aware
 * implementations as we wire up subsystems.
 *
 * (The interrupt handlers Int2/3/4/5 used to be entries in the m68k
 * vector table; on SDL the host pumps events directly so these never
 * fire.)
 */

#include <stdint.h>

#include "swos_platform.h"

void _SafeStart(void)        {}
void _LoadDosLibrary(void)   {}
void _DetachOSDisplay(void)  {}

void Int2KeyboardHandler(void) {}
void Int3Handler(void)         {}
void Int3TimerHandler(void)    {}
void Int4Handler(void)         {}
void Int5Handler(void)         {}

/* The CallMenu* helpers are tiny m68k trampolines that swap argument
 * order before calling a menu callback. On SDL we just call the
 * function pointer directly with host calling convention. This is
 * what enables onInit / onSelect / beforeDraw callbacks -- without
 * these the main menu never runs _SWOSMainMenuInit so things like
 * the START and SAVE buttons never get hidden. */
void _CallMenuCallback(void *funcPtr)
{
    if (funcPtr) ((void (*)(void))funcPtr)();
}

void _CallMenuFuncWithXY(void *funcPtr, uint16_t x, uint16_t y)
{
    if (funcPtr) ((void (*)(uint16_t, uint16_t))funcPtr)(x, y);
}

void _CallMenuOnSelect(void *funcPtr, void *selectedEntry)
{
    if (funcPtr) ((void (*)(void *))funcPtr)(selectedEntry);
}
