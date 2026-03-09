#include "frontend.h"
#include "game_utils.h"
#include "audio.h"
#include "menus.h"

/* FrontEndMainLoop */
extern uint16_t g_quitToDos;

/* ---- External asm functions (C-callable bridges in swos2_unit.s) ---- */
extern void _MenuProc(void);

extern uint16_t menuMusic;
extern uint16_t menuFade;
extern uint8_t frontbufferMaskBuffer[];
extern volatile uint16_t currentGameTick;
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);

/* ================================================================
 * LoadOverlayToFrontbuffer  (SWOS2.s line 42675)
 *
 * Loads a raw image file into the frontbuffer mask buffer.
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
__attribute__((used, externally_visible, noinline))
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
 * FrontEndMainLoop  (SWOS2.s line 42901)
 *
 * Main front-end event loop. Initializes timer and menu system,
 * then repeatedly calls MenuProc until g_quitToDos is set.
 * ================================================================ */
__attribute__((used, externally_visible, noinline))
void _FrontEndMainLoop(void)
{
    _ReadTimerDelta();
    _FrontEndPreLoopSetup();

    do
    {
        _MenuProc();
    } while (g_quitToDos == 0);
}
