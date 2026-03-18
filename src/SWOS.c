/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "swos_sections.h"
#include "swos_asm.h"
#include "frontend.h"
#include "game_utils.h"
#include "fade.h"
#include "display.h"
#include "asset_loader.h"
#include "loader_bootstrap.h"
#include "loader_intro.h"
#include "audio.h"
#include "menus.h"
#include "deterministic.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;

#define SWOS_CHIPMEMSIZE 0x00200000u
#define SWOS_FASTMEMSIZE 0x00200000u
#define SWOS_CHIPBITS (1<<(SWOS_CHIPMEMSIZE/0x80000))-1
#define SWOS_FASTBITS ((1<<(SWOS_FASTMEMSIZE/0x80000))-1)<<24
#define SWOS_MAGIC_VALUE 0x12345678u
/* Note: use FRONTBUFFER_SIZE_TOTAL_BYTES from display.h instead of hardcoded OCS value */

#define CIAA_PRA_ADDR 0x00BFE001u
#define CIAA_DDRA_ADDR 0x00BFE201u
#define CIAA_ICR_ADDR 0x00BFED01u

#define LEVEL_2_INTERRUPT_AUTOVECTOR 0x68u
#define LEVEL_3_INTERRUPT_AUTOVECTOR 0x6Cu
#define LEVEL_4_INTERRUPT_AUTOVECTOR 0x70u

extern void _SafeStart(void);
extern void _LoadDosLibrary(void);
extern void _DetachOSDisplay(void);
extern void _InitHardwareAtFrameStart(void);
extern void _SetupInterrupts(void);
extern void Int2KeyboardHandler(void);
extern void Int3TimerHandler(void);
extern void Int4Handler(void);

extern uint32_t S_VBR;
extern uint32_t loaderBankBitmap;
extern uint16_t chipRamBitmapEnd;
extern uint16_t pitchDatBuffer;
extern uint16_t currentMenu;
extern uint32_t magicValue;
extern uint32_t loaderMemTop;
extern uint32_t loaderParam;
extern uint16_t frontbufferBase;
extern uint16_t frontbufferBaseAlt;
extern uint16_t frontbufferMaskBuffer;

/* Pre-allocated cache memory (defined in SWOS2.s BSS, filled before hardware takeover) */
extern uint8_t *g_earlyAllocCacheMem;
extern uint32_t g_earlyAllocCacheSize;

extern uint8_t *_AllocMemAny(uint32_t size);

SWOS_CHIP_BSS volatile uint32_t g_swos_chip_bss_anchor;
SWOS_FAST_BSS volatile uint32_t g_swos_fast_bss_anchor;

static void set_vector(uint32_t vbr, uint32_t offset, void (*handler)(void))
{
    volatile uint32_t *vector = (volatile uint32_t *)(vbr + offset);
    *vector = (uint32_t)handler;
}

__attribute__((used, externally_visible)) void _start(void)
{
    _SafeStart();
    _LoadDosLibrary();
    _DetachOSDisplay();
    _InitHardwareAtFrameStart();

    uint32_t bank_bitmap_ptr = (uint32_t)&loaderBankBitmap;
    uint32_t bank_bitmap_end = (uint32_t)&chipRamBitmapEnd;
    do
    {
        *(volatile uint16_t *)bank_bitmap_ptr = 0x0000u;
        bank_bitmap_ptr += 2u;
    } while (bank_bitmap_ptr != bank_bitmap_end);

    uint32_t pitch_ptr = (uint32_t)&pitchDatBuffer;
    uint32_t menu_ptr = (uint32_t)&currentMenu;
    do
    {
        *(volatile uint16_t *)pitch_ptr = 0x0000u;
        pitch_ptr += 2u;
    } while (pitch_ptr != menu_ptr);

    magicValue = SWOS_MAGIC_VALUE;
    /* Bank scan disabled: assetLoaderBank0Base is in BSS (relocated),
     * so computed bank addresses (base + N*0x80000) point to invalid memory.
     * Set loaderMemTop = 0 to skip the bank scan in _AssetLoaderInitAltPathAndBanks.
     * The main 195KB cache region is sufficient. */
    loaderMemTop = 0;
    *(volatile uint32_t *)&loaderBankBitmap = 0;
    loaderParam = SWOS_FASTBITS;

    custom->color[0] = 0x0u;

    volatile uint16_t *front = &frontbufferBase;
    volatile uint16_t *alt = &frontbufferBaseAlt;
    volatile uint16_t *mask = &frontbufferMaskBuffer;

    for (uint32_t bytes = FRONTBUFFER_SIZE_TOTAL_BYTES; bytes != 0u; bytes -= 2u)
    {
        *front++ = 0xF0F0u;
        *alt++ = 0xF0F0u;
        *mask++ = 0xF0F0u;
    }

    custom->dmacon = 0x8340u;
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    set_vector(S_VBR, LEVEL_2_INTERRUPT_AUTOVECTOR, Int2KeyboardHandler);
    set_vector(S_VBR, LEVEL_3_INTERRUPT_AUTOVECTOR, Int3TimerHandler);

#ifdef DETERMINISTIC_MODE
    _InitDeterministicMode();
#endif
    set_vector(S_VBR, LEVEL_4_INTERRUPT_AUTOVECTOR, Int4Handler);

    _SetupInterrupts();

    *(volatile uint8_t *)CIAA_ICR_ADDR = 0x88u;
    custom->intena = 0xC008u;
    *(volatile uint8_t *)CIAA_ICR_ADDR = 0x82u;
    *(volatile uint8_t *)CIAA_DDRA_ADDR |= 0x02u;
    *(volatile uint8_t *)CIAA_PRA_ADDR |= 0x02u;
}

/* ---- Timing constant ---- */
#define FPS_50HZ 49

/* ---- External globals for mainC ---- */
extern uint16_t assetLoaderAltPathReady;
extern uint16_t paletteIndex;
extern uint16_t loaderModeFlag;
extern uint16_t hilNumGoals;
extern volatile uint16_t currentGameTick;

extern uint8_t kTitleLogoColors[];
extern uint8_t kTitleScreenColors[];
extern uint8_t kTitleTextColors[];

extern uint16_t titleLogoPalette[];
extern uint16_t titleScreenPalette[];
extern uint16_t titleTextPalette[];

extern uint16_t bootTemp0;
extern uint16_t bootTemp1;
extern uint16_t bootTemp2;
extern uint16_t bootTemp3;
extern uint16_t bootTemp4;
extern uint16_t bootTemp5;
extern uint16_t bootTemp6;

extern uint16_t introMenuImageScript[];
extern uint8_t imageCmdWorkBuf[];
extern uint32_t imageStreamCursor;
extern uint32_t menuImageStreamBaseEnd;

extern uint16_t g_quitToDos;

extern void _AssetSys_InitDiskImageSources(void);
extern int16_t _AssetSvcLoaderOp2(void);
extern int16_t _AssetSvcLoaderOp3(void);
extern void _ProcessImageCommandStream(const uint16_t *cmdBuf, uint8_t *workBuf);
extern void _MenuProc(void);
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);

/* delay_ticks: reset game tick counter, busy-wait until target reached */
static void delay_ticks(uint16_t ticks)
{
    currentGameTick = 0;
    while (currentGameTick < ticks)
        ;
}

/* ================================================================
 * mainC  (was mainAsm in SWOS2.s / _mainAsm in frontend.c)
 *
 * Main game boot sequence. Initializes display, loads assets,
 * shows loader intro, builds menu image stream, and enters
 * the front-end main loop.
 * ================================================================ */
__attribute__((used, externally_visible))
void _mainC(void)
{
    /* ---- Phase 1: Hardware init ---- */
    _AssetLoaderResetState();
    assetLoaderAltPathReady = 0;
    _InitDisplayAndCopper();
    _AssetSys_InitDiskImageSources();
    _InitSkinAndDefaultPointers();

    /* ---- Phase 2: Build palettes ---- */
#ifdef AGA
    {
        extern uint16_t menuPalette[];
        _WriteCopperPalette(menuPalette);
    }
#else
    _BuildPaletteFromColors(kTitleLogoColors, titleLogoPalette);
    _BuildPaletteFromColors(kTitleScreenColors, titleScreenPalette);
    _BuildPaletteFromColors(kTitleTextColors, titleTextPalette);
#endif

    /* ---- Phase 3: Boot temps ---- */
    bootTemp0 = 1;
    bootTemp1 = 0;
    bootTemp2 = 0;
    bootTemp3 = 0;
    bootTemp4 = 0;
    bootTemp5 = 0;
    bootTemp6 = 0;

    /* ---- Phase 4: Loader intro ---- */
    loaderModeFlag = 1;
    _ShowLoaderIntroSequence();
    _AssetLoaderInitAltPathAndBanks();
    _AssetPrecacheCore();
    _InstallAndInitLoaderBootstrap();
    _FadeOutToBlack();
    loaderModeFlag = 0;

    /* ---- Phase 5: Show title screens ---- */
    _DoubleBufferSwapSequence();
    paletteIndex = 0;
    _LoadOverlayToFrontbuffer((const uint8_t *)GRAFS "olddisk1.raw");
    _BlitterCopyPanelToScreen2();
    _WaitForVSync();

    _LoadOverlayToFrontbuffer((const uint8_t *)GRAFS "disk22.raw");
    paletteIndex = 0;
    _FadeIn();

    /* ---- Phase 6: Poll loader op3 with 50Hz delays ---- */
    do
    {
        delay_ticks(FPS_50HZ);
    } while (_AssetSvcLoaderOp3() != 0);

    _InstallAndRunLoaderBootstrap_Op3();
    _FadeOutToBlack();

    /* ---- Phase 7: Poll loader op2 ---- */
    if (_AssetSvcLoaderOp2() != 0)
    {
        _BlitterCopyPanelToScreen2();
        _WaitForVSync();
        paletteIndex = 0;
        _FadeIn();

        do
        {
            delay_ticks(FPS_50HZ);
        } while (_AssetSvcLoaderOp2() != 0);

        _FadeOutToBlack();
    }

    /* ---- Phase 8: Build intro menu assets ---- */
    _ProcessImageCommandStream(introMenuImageScript, imageCmdWorkBuf);
    menuImageStreamBaseEnd = imageStreamCursor;
    _AssetPrecacheMenuPack();
    assetLoaderAltPathReady = 1;
    hilNumGoals = 0;

    /* ---- Phase 9: Enter front-end loop ---- */
    _FrontEndMainLoop();
}
