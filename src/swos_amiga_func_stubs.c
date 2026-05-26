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

#include "swos_types.h"
#include "swos_platform.h"
#include "display.h"   /* extern volatile struct Custom *custom */

#ifdef SWOS_SDL_BUILD
/* SDL doesn't link the Amiga bootstrap; provide empty stubs so the
 * SWOS.c hardware-takeover path links cleanly. On Amiga these live
 * in resload.c (with real Exec/DOS calls) and we must NOT redefine. */
void _SafeStart(void)        {}
void _LoadDosLibrary(void)   {}
void _DetachOSDisplay(void)  {}
#endif

/* The Int*Handler entries are direct m68k vector-table entries: the
 * CPU jumps here on a hardware interrupt and the function must return
 * via `rte`, not `rts`. Use GCC's `__attribute__((interrupt))` —
 * matches the demoscene/Cracktro convention (cleaner than naked-asm
 * wrappers, lets the compiler manage register save/restore).
 *
 * On m68k-amiga-elf 15.1.0 the `interrupt` attribute auto-saves the
 * d0-d1/a0-a1 caller-clobbered scratch regs the body actually uses and
 * emits `rte`. The body has full C semantics — it can call into other
 * C functions (which save d2-d7/a2-a6 themselves per the ABI), read
 * volatile globals, and write to memory-mapped registers. */
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
extern void _Int2KeyboardHandler(void);  /* worker in interrupts.c */
extern void _Int3TimerHandler(void);     /* worker in interrupts.c */

/* INTREQ acks. The double write is the demoscene/Cracktro workaround
 * for the A4000-CPU prefetch-vs-Paula race that can otherwise leave the
 * interrupt bit set and cause the same ISR to re-fire immediately. */

__attribute__((interrupt, used))
void Int2KeyboardHandler(void)
{
    _Int2KeyboardHandler();
    /* The body already acks via `custom->intreq = 8;` for the I/O Ports
     * bit (CIA-A) — no extra ack needed here. */
}

__attribute__((interrupt, used))
void Int3TimerHandler(void)
{
    _Int3TimerHandler();
}

__attribute__((interrupt, used))
void Int3Handler(void)
{
    _Int3TimerHandler();
    custom->intreq = 0x0070;   /* ack COPER + VERTB + BLIT */
    custom->intreq = 0x0070;
}

__attribute__((interrupt, used))
void Int4Handler(void)
{
    custom->intreq = 0x0780;   /* ack AUD0..AUD3 */
    custom->intreq = 0x0780;
}

__attribute__((interrupt, used))
void Int5Handler(void)
{
    custom->intreq = 0x1800;   /* ack RBF + DSKSYN */
    custom->intreq = 0x1800;
}

#else  /* SDL — never installed as a vector */
void Int2KeyboardHandler(void) {}
void Int3Handler(void)         {}
void Int3TimerHandler(void)    {}
void Int4Handler(void)         {}
void Int5Handler(void)         {}
#endif

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
