/*
 * platform_amiga_stubs.c -- C stubs for the Amiga assembly bootstrap.
 *
 * The original Amiga build linked against SWOS2.s which provided low-level
 * routines (interrupt setup, hardware takeover, DOS library handling, ...)
 * plus a giant data section. On SDL we replace those assembly entry points
 * with no-op C stubs and back the data symbols with regular BSS.
 *
 * The interesting bridge is `_startShim`: the original assembly version
 * just stacked-up a fresh stack and called `_start` then `_mainC`. We do
 * the same minus the stack switch.
 */

#include "swos_platform.h"
#include "swos_types.h"
#ifdef SWOS_SDL_BUILD
#include "swos_asm.h"
#include "platform_sdl.h"
#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

/* ---- Global panic code referenced from the C source ---------------- */
volatile uint16_t panicReasonCode = 0;

/* The C source declares `_panicReasonCode = &panicReasonCode` in
 * main.c on the Amiga port. SDL has its own main.c which doesn't, so
 * we keep a definition here for SDL — gated to prevent a multi-def
 * link error on Amiga where main.c is authoritative. */
#ifdef SWOS_SDL_BUILD
volatile uint16_t *_panicReasonCode = &panicReasonCode;
#endif

/* ---- The fake "Custom chip" surface --------------------------------- */
/* display.c does:  volatile struct Custom *custom = (...)0xDFF000;
 * We can't dereference 0xDFF000 on a hosted platform, so we let display.c
 * keep that initializer but allocate a real backing buffer at runtime.
 *
 * Strategy: define a static struct here, and have Platform_Init re-point
 * `custom` at it. We declare `custom` as a weak alias to avoid colliding
 * with display.c's definition.
 *
 * Actually -- the simplest thing is to overwrite the pointer in
 * Platform_Init via a getter. We expose the storage here. */
#ifdef SWOS_SDL_BUILD
static struct Custom s_custom_storage;
struct Custom *Platform_GetCustomStorage(void) { return &s_custom_storage; }

/* Called from Platform_Init BEFORE the tick timer starts so that the very
 * first _Int3TimerHandler invocation lands in valid memory. */
void Platform_RewireCustom(void)
{
    extern volatile struct Custom *custom;
    custom = (volatile struct Custom *)&s_custom_storage;
}
#endif

#ifdef SWOS_SDL_BUILD
/* The Amiga ISR (`_Int3TimerHandler`) calls into `_FadeOutTick` /
 * `_FadeInTick` which use the sentinel value FADE_SENTINEL (0x3E7) to
 * mean "no fade in progress". On Amiga the data section initialised
 * those globals to the sentinel; in our zero-filled BSS stubs they
 * default to 0, which the fade ticks misinterpret as "fade level 0
 * with a freshly-installed palette" and then crash dereferencing
 * paletteTable[0]. Pre-arm the sentinels here. */
void Platform_PreArmIsrGlobals(void)
{
    extern volatile uint16_t fadeCommand;
    extern volatile uint16_t fadeInitFlag;
    extern volatile uint16_t fadeState;
    fadeCommand  = 0x03E7u; /* FADE_SENTINEL */
    fadeInitFlag = 0x03E7u; /* FADE_SENTINEL */
    fadeState    = 0;       /* idle */

    /* The Amiga `_start()` set these, then we skip `_start()` on SDL
     * because it pokes raw chip registers. Set the survivors here:
     *   magicValue      -- menu-proc integrity check (0x12345678)
     *   irqCookieObfuscated -- xor'd against 0xCFD1F8B3 by the menu,
     *                       the Amiga bootstrap writes 0xCFD1EFE7 so
     *                       the check resolves to 0x1754.
     */
    extern uint32_t magicValue;
    extern uint32_t irqCookieObfuscated;
    magicValue = 0x12345678u;
    irqCookieObfuscated = 0xCFD1EFE7u;

    /* paletteTable: array of 7 pointers to uint16_t[16] palettes.
     * Originally initialised in SWOS2.s; on SDL we need to set them
     * up at runtime so fade ticks have somewhere to write. Order
     * matches the Amiga layout exactly. */
    extern void *paletteTable[];
    extern uint8_t  menuPalette[];
    extern uint8_t  blackPalette[];
    extern uint8_t  gamePalette[];
    extern uint8_t  titleScreenPalette[];
    extern uint8_t  panicPalette[];
    extern uint8_t  titleTextPalette[];
    extern uint8_t  titleLogoPalette[];
    paletteTable[0] = menuPalette;
    paletteTable[1] = blackPalette;
    paletteTable[2] = gamePalette;
    paletteTable[3] = titleScreenPalette;
    paletteTable[4] = panicPalette;
    paletteTable[5] = titleTextPalette;
    paletteTable[6] = titleLogoPalette;
}
#endif  /* SWOS_SDL_BUILD */

/* ---- Forward declarations from the ported Amiga C ------------------- */
/* SWOS.c defines `_start` (the original m68k entry). The hosted libc
 * also defines `_start` as the CRT entry, so we cannot reference the
 * SWOS one by that name from main.c. We declare a renamed extern via
 * the linker option `-Wl,--defsym=_swos_c_start=_start` (set in the
 * Makefile) and call that instead. */
extern void _swos_c_start(void);
extern void _mainC(void);

/* ---- Quit flag ------------------------------------------------------ */
/* SDL: defined here (was in SWOS2.s; SDL doesn't link SWOS2.s).
 * Amiga: defined by Resload.s, included via resload_unit.s. */
#ifdef SWOS_SDL_BUILD
uint16_t g_quitToDos = 0;
#else
extern uint16_t g_quitToDos;
#endif

void Platform_RequestQuit(void)
{
#if defined(SWOS_DEBUG_AUTOSKIP) && defined(SWOS_SDL_BUILD)
    if (!g_quitToDos)
        fprintf(stderr, "[autoskip] Platform_RequestQuit: g_quitToDos was 0, now setting 1\n");
#endif
    g_quitToDos = 1;
}

/* Called from _MenuProc each tick on the main thread. When a quit has
 * been requested (by SDL_QUIT / window-X / ESC), nudge the menu state
 * machines out: set `exitMenu` to break the inner _ShowMenu loop AND
 * set `abortSelectTeams` so the caller's state machine (e.g.
 * `_SelectTeamsFinalMenu`) sees "user backed out" and short-circuits
 * instead of re-entering another submenu — without this, pressing X
 * while in the team picker flickered between nested menus as each
 * level exited only to be reactivated by its parent on the way out.
 *
 * Originally used setjmp/longjmp from _FrontEndMainLoop for this but
 * Cygwin's 32-bit setjmp.h pulls in signal context headers that the
 * target doesn't implement, breaking the Windows/Cygwin build.
 * Cooperative unwinding via the game's existing abort flag is
 * portable and doesn't require setjmp. */
extern uint16_t exitMenu;
extern uint16_t abortSelectTeams;
extern uint16_t chooseTeamsResult;

void Platform_CheckQuitRequest(void)
{
    if (!g_quitToDos) return;
#if defined(SWOS_DEBUG_AUTOSKIP) && defined(SWOS_SDL_BUILD)
    static int once;
    if (!once) {
        once = 1;
        fprintf(stderr, "[autoskip] Platform_CheckQuitRequest: FIRST observation that g_quitToDos=1\n");
    }
#endif
    exitMenu = 1;
    abortSelectTeams = 1;
    chooseTeamsResult = 0;
}

/* ---- _startShim ----------------------------------------------------- */
/* Called from main.c. On Amiga the original asm version was in
 * swos2_unit.s; here we provide it as C now that SWOS2.s is retired.
 * The full Amiga sequence is _swos_c_start() -> _mainC(); SDL skips
 * _swos_c_start() because it pokes raw chip-register addresses that
 * segfault on a hosted target. */
#ifdef SWOS_SDL_BUILD
void _startShim(void)
{
    /* Re-point `custom` (defined in display.c with a 0xDFF000 initializer)
     * at our heap-backed stub buffer so any subsequent register pokes
     * land in valid memory. */
    extern volatile struct Custom *custom;
    custom = (volatile struct Custom *)&s_custom_storage;

    _mainC();
}
#else
/* Amiga: switch to our own stack before running _swos_c_start /
 * _mainC. Mirrors the original SWOS2.s _startShim which reserved
 * `stackSpace: ds.b 8192` then `movea.l #stackTop, sp`. Without this,
 * we run on DOS's default task stack (typically 4 KB) and the moment
 * a Level-3 VBlank interrupt nests into the deep `_Int3TimerHandler`
 * C call we blow the stack — that's the `Software Failure 8000 000B`
 * (AN_StackProbe with the leading `01` masked off when ORed against
 * AT_DeadEnd). 16 KB is comfortable headroom for the worst-case
 * nested-ISR + drawing call depth. */
extern uint32_t S_SavedSP;   /* Resload.s — saves caller's SP for return */

#define SWOS_STACK_BYTES 16384u
/* File-scope (not static) so the symbol name lands in the .o symbol
 * table for the naked-function inline asm below to reference. */
uint8_t s_swos_stack[SWOS_STACK_BYTES] __attribute__((aligned(4)));

__attribute__((naked, used))
void _startShim(void)
{
    __asm__ volatile (
        "move.l  %%sp,S_SavedSP            \n"
        "lea     s_swos_stack+%c0,%%sp     \n"
        "jsr     _swos_c_start             \n"
        "jsr     _mainC                    \n"
        "jsr     _ExitToDos                \n"
        "movea.l S_SavedSP,%%sp            \n"
        "moveq   #0,%%d0                   \n"
        "rts                               \n"
        : : "i"(SWOS_STACK_BYTES)
    );
}
#endif

/* ===================================================================
 * Stubs for the small remaining set of Amiga symbols that have no
 * C definition anywhere in the ported source.
 *
 * The bigger functions (_SafeStart, _LoadDosLibrary, _DetachOSDisplay,
 * _InitHardwareAtFrameStart, _SetupInterrupts, Int*Handler, _AllocMemAny)
 * already live in the ported C files (resload.c, interrupts.c, ...) so
 * we must NOT redefine them here.
 * =================================================================== */

/* ---- VBR (vector base register) shadow ------------------------------
 * On Amiga S_VBR is defined by Resload.s (included via resload_unit.s),
 * so the SDL-side stub must NOT collide with it. */
#ifdef SWOS_SDL_BUILD
uint32_t S_VBR = 0;
#endif


/* ---- Pre-allocated cache (was set up by the loader pre-takeover) ----
 * The core+menu precache lists total ~281 KB of raw file data across
 * ~21 entries (each +48 B of node header); in-game loads pile on more.
 * 1 MiB gives comfortable headroom. */
/* Asset cache pre-allocation. SDL has unlimited RAM; Amiga has at
 * most ~8 MB fast and we're already burning a chunk on BSS. 256 KB
 * is enough for the core+menu precache (~281 KB raw with ~21 node
 * headers — close fit but the asset_loader.c manages incremental
 * eviction). */
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
#define EARLY_CACHE_BYTES (256u * 1024u)
#else
#define EARLY_CACHE_BYTES (1024u * 1024u)
#endif
static uint8_t s_early_cache[EARLY_CACHE_BYTES];
uint8_t *g_earlyAllocCacheMem = s_early_cache;
uint32_t g_earlyAllocCacheSize = EARLY_CACHE_BYTES;
