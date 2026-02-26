#include "frontend.h"
#include "game_utils.h"
#include "fade.h"
#include "display.h"
#include "asset_loader.h"
#include "loader_bootstrap.h"
#include "loader_intro.h"
#include "audio.h"
#include "menus.h"

/* ---- Timing constant ---- */
#define FPS_50HZ 49

/* ---- External globals (all live in SWOS2.s / Resload.s) ---- */
extern uint16_t assetLoaderAltPathReady;
extern uint16_t paletteIndex;
extern uint16_t loaderModeFlag;
extern uint16_t hilNumGoals;
extern volatile uint16_t currentGameTick;
extern uint16_t menuMusic;
extern uint16_t menuFade;
extern uint8_t frontbufferMaskBuffer[];

/* Palette color source tables (RGB bytes, 16 entries x 3 bytes each) */
extern uint8_t kTitleLogoColors[];
extern uint8_t kTitleScreenColors[];
extern uint8_t kTitleTextColors[];

/* Palette destination tables (16 x uint16_t) */
extern uint16_t titleLogoPalette[];
extern uint16_t titleScreenPalette[];
extern uint16_t titleTextPalette[];

/* Boot temporaries (words) */
extern uint16_t bootTemp0;
extern uint16_t bootTemp1;
extern uint16_t bootTemp2;
extern uint16_t bootTemp3;
extern uint16_t bootTemp4;
extern uint16_t bootTemp5;
extern uint16_t bootTemp6;

/* Image command stream */
extern uint16_t introMenuImageScript[];
extern uint8_t imageCmdWorkBuf[];
extern uint32_t imageStreamCursor;
extern uint32_t menuImageStreamBaseEnd;

/* FrontEndMainLoop */
extern uint16_t g_quitToDos;

/* ---- External asm functions (C-callable bridges in swos2_unit.s) ---- */
extern void _AssetSys_InitDiskImageSources(void);
extern int16_t _AssetSvcLoaderOp2(void);
extern int16_t _AssetSvcLoaderOp3(void);
extern void _ProcessImageCommandStream(const uint16_t *cmdBuf, uint8_t *workBuf);
extern void _MenuProc(void);
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
/* _InitMenuMusic, _TryPlayMusic declared in audio.h */
/* _PrepareMenuBackground, _InitMainMenuStuff declared in menus.h */

/* ================================================================
 * Delay helper: wait for N+1 VBlank ticks.
 * Mirrors the original delay loop:
 *   move.w #FPS_50HZ,d1
 *   delay_outer: move.w (currentGameTick),d0
 *   wait_inner: cmp.w (currentGameTick),d0; beq wait_inner
 *   dbf d1,delay_outer
 * ================================================================ */
static void delay_ticks(int16_t count)
{
    int16_t i;
    uint16_t tick;

    for (i = count; i >= 0; i--)
    {
        tick = currentGameTick;
        while (currentGameTick == tick)
        {
            /* spin until next tick */
        }
    }
}

/* ================================================================
 * LoadOverlayToFrontbuffer  (SWOS2.s line 42675)
 *
 * Loads a raw image file into the frontbuffer mask buffer.
 * Original asm: lea (frontbufferMaskBuffer),a1; jmp LoadFile
 * ================================================================ */
__attribute__((used, externally_visible))
void _LoadOverlayToFrontbuffer(const uint8_t *path)
{
    _LoadFileEx((uint8_t *)path, frontbufferMaskBuffer, (uint32_t *)0);
}

/* ================================================================
 * FrontEndPreLoopSetup  (SWOS2.s line 42642)
 *
 * Initializes menu music, prepares menu background, sets up menu
 * data structures, optionally starts music playback, and enables
 * the menu fade-in flag.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FrontEndPreLoopSetup(void)
{
    _InitMenuMusic();
    _PrepareMenuBackground();
    _InitMainMenuStuff();

    if (menuMusic != 0)
    {
        _TryPlayMusic();
    }

    menuFade = 1;
}

/* ================================================================
 * mainAsm  (SWOS2.s line 19613)
 *
 * Main game boot sequence. Initializes display, loads assets,
 * shows loader intro, builds menu image stream, and enters
 * the front-end main loop.
 * ================================================================ */
__attribute__((used, externally_visible))
void _mainAsm(void)
{
    /* ---- Phase 1: Hardware init ---- */
    _AssetLoaderResetState();
    assetLoaderAltPathReady = 0;
    _InitDisplayAndCopper();
    _AssetSys_InitDiskImageSources();
    _InitSkinAndDefaultPointers();

    /* ---- Phase 2: Build palettes ---- */
    _BuildPaletteFromColors(kTitleLogoColors, titleLogoPalette);
    _BuildPaletteFromColors(kTitleScreenColors, titleScreenPalette);
    _BuildPaletteFromColors(kTitleTextColors, titleTextPalette);

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
    _LoadOverlayToFrontbuffer((const uint8_t *)"grafs/olddisk1.raw");
    _BlitterCopyPanelToScreen2();
    _WaitForVSync();

    _LoadOverlayToFrontbuffer((const uint8_t *)"grafs/disk22.raw");
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

/* ================================================================
 * FrontEndMainLoop  (SWOS2.s line 42901)
 *
 * Main front-end event loop. Initializes timer and menu system,
 * then repeatedly calls MenuProc until g_quitToDos is set.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FrontEndMainLoop(void)
{
    _ReadTimerDelta();
    _FrontEndPreLoopSetup();

    do
    {
        _MenuProc();
    } while (g_quitToDos == 0);
}
