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
#include "swos_asm.h"
#include "platform_sdl.h"
#include "display.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Global panic code referenced from the C source ---------------- */
volatile uint16_t panicReasonCode = 0;

/* The C source declares `_panicReasonCode = &panicReasonCode` in main.c
 * (the SWOS_C version). We do not need it in the SDL build but keep
 * the symbol around for any reference. */
volatile uint16_t *_panicReasonCode = &panicReasonCode;

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
static struct Custom s_custom_storage;
struct Custom *Platform_GetCustomStorage(void) { return &s_custom_storage; }

/* Called from Platform_Init BEFORE the tick timer starts so that the very
 * first _Int3TimerHandler invocation lands in valid memory. */
void Platform_RewireCustom(void)
{
    extern volatile struct Custom *custom;
    custom = (volatile struct Custom *)&s_custom_storage;
}

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
    extern uint32_t paletteTable[];
    extern uint8_t  menuPalette[];
    extern uint8_t  blackPalette[];
    extern uint8_t  gamePalette[];
    extern uint8_t  titleScreenPalette[];
    extern uint8_t  panicPalette[];
    extern uint8_t  titleTextPalette[];
    extern uint8_t  titleLogoPalette[];
    paletteTable[0] = (uint32_t)(uintptr_t)menuPalette;
    paletteTable[1] = (uint32_t)(uintptr_t)blackPalette;
    paletteTable[2] = (uint32_t)(uintptr_t)gamePalette;
    paletteTable[3] = (uint32_t)(uintptr_t)titleScreenPalette;
    paletteTable[4] = (uint32_t)(uintptr_t)panicPalette;
    paletteTable[5] = (uint32_t)(uintptr_t)titleTextPalette;
    paletteTable[6] = (uint32_t)(uintptr_t)titleLogoPalette;
}

/* ---- Forward declarations from the ported Amiga C ------------------- */
/* SWOS.c defines `_start` (the original m68k entry). The hosted libc
 * also defines `_start` as the CRT entry, so we cannot reference the
 * SWOS one by that name from main.c. We declare a renamed extern via
 * the linker option `-Wl,--defsym=_swos_c_start=_start` (set in the
 * Makefile) and call that instead. */
extern void _swos_c_start(void);
extern void _mainC(void);

/* ---- Quit flag ------------------------------------------------------ */
/* Defined in this file (was defined in SWOS2.s) — referenced by frontend.c. */
uint16_t g_quitToDos = 0;

void Platform_RequestQuit(void)
{
#ifdef SWOS_DEBUG_AUTOSKIP
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
#ifdef SWOS_DEBUG_AUTOSKIP
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
/* Called from main.c. The Amiga version did stack-pointer dancing; we
 * just call straight through. We deliberately SKIP _start() (the Amiga
 * hardware-takeover routine) on SDL -- it pokes raw chip-register
 * addresses that segfault on a hosted target. The work it did
 * (clearing scratch buffers, picking initial palettes) is harmless to
 * skip because all those buffers live in BSS-zero memory. */
void _startShim(void)
{
    /* Re-point `custom` (defined in display.c with a 0xDFF000 initializer)
     * at our heap-backed stub buffer so any subsequent register pokes
     * land in valid memory. */
    extern volatile struct Custom *custom;
    custom = (volatile struct Custom *)&s_custom_storage;

    _mainC();
}

/* ===================================================================
 * Stubs for the small remaining set of Amiga symbols that have no
 * C definition anywhere in the ported source.
 *
 * The bigger functions (_SafeStart, _LoadDosLibrary, _DetachOSDisplay,
 * _InitHardwareAtFrameStart, _SetupInterrupts, Int*Handler, _AllocMemAny)
 * already live in the ported C files (resload.c, interrupts.c, ...) so
 * we must NOT redefine them here.
 * =================================================================== */

/* ---- VBR (vector base register) shadow ------------------------------ */
uint32_t S_VBR = 0;

/* ---- Pre-allocated cache (was set up by the loader pre-takeover) ----
 * The core+menu precache lists total ~281 KB of raw file data across
 * ~21 entries (each +48 B of node header); in-game loads pile on more.
 * 1 MiB gives comfortable headroom. */
#define EARLY_CACHE_BYTES (1024u * 1024u)
static uint8_t s_early_cache[EARLY_CACHE_BYTES];
uint8_t *g_earlyAllocCacheMem = s_early_cache;
uint32_t g_earlyAllocCacheSize = EARLY_CACHE_BYTES;
