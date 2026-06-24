/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "frontend.h"
#include "game_utils.h"
#include "audio.h"
#include "menus.h"
#include "display.h"
#ifdef SWOS_SDL_BUILD
#include <string.h>
#endif

/* FrontEndMainLoop */
extern uint16_t g_quitToDos;

/* ---- External asm functions (C-callable bridges in swos2_unit.s) ---- */
extern void MenuProc(void);

extern uint16_t menuMusic;
extern uint16_t menuFade;
extern uint8_t frontbufferMaskBuffer[];
extern volatile uint16_t currentGameTick;
extern int16_t LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
#ifdef SWOS_SDL_BUILD
extern uint8_t rawAssetLoadBuffer[];
#endif

/* ================================================================
 * LoadOverlayToFrontbuffer  (SWOS2.s line 42675)
 *
 * Loads a raw image file into the frontbuffer mask buffer.
 *
 * Amiga: loads the bitplane-format file straight into
 *   `frontbufferMaskBuffer`, which stays in bitplane form.
 * SDL:   stages the bitplane file into `rawAssetLoadBuffer`, then
 *   decodes the first SCREEN_HEIGHT scanlines into the chunky 8bpp
 *   `frontbufferMaskBuffer`. Subsequent draw/blit code reads the
 *   mask buffer expecting chunky palette indices.
 * ================================================================ */
__attribute__((used, externally_visible))
void LoadOverlayToFrontbuffer(const uint8_t *path)
{
#ifdef SWOS_SDL_BUILD
    LoadFileEx((uint8_t *)path, rawAssetLoadBuffer, (uint32_t *)0);

    /* Decode NUM_PLANES-plane interleaved bitplane data → 8bpp chunky.
     * Full-screen overlay images (MENUBG.RAW = 47872 bytes) are stored
     * at the *frontbuffer* stride: PLANE_STRIDE bytes per plane row,
     * NUM_PLANES planes per scanline = ROW_STRIDE_BYTES total. We
     * decode SCREEN_WIDTH pixels (the visible portion of PLANE_STRIDE*8
     * = 352 source bits). Sprite-source images use a smaller stride and
     * go through `blitDispatch`, not this path. */
    memset(frontbufferMaskBuffer, 0, FRONTBUFFER_SIZE_TOTAL_BYTES);
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        const uint8_t *rowSrc = rawAssetLoadBuffer + (size_t)y * ROW_STRIDE_BYTES;
        uint8_t       *rowDst = frontbufferMaskBuffer + (size_t)y * FRONTBUFFER_ROW_STRIDE_BYTES + WIDE_MARGIN;
        for (int px = 0; px < SCREEN_WIDTH; px++) {
            int bit = 7 - (px & 7);
            int byte = px >> 3;
            uint8_t idx = 0;
            for (int p = 0; p < NUM_PLANES; p++) {
                uint8_t planeByte = rowSrc[p * PLANE_STRIDE + byte];
                idx |= (uint8_t)(((planeByte >> bit) & 1u) << p);
            }
            rowDst[px] = idx;
        }
    }
#else
    LoadFileEx((uint8_t *)path, frontbufferMaskBuffer, (uint32_t *)0);
#endif
}

/* ================================================================
 * FrontEndPreLoopSetup  (SWOS2.s line 42642)
 *
 * Initializes menu music, prepares menu background, sets up menu
 * data structures, optionally starts music playback, and enables
 * the menu fade-in flag.
 * ================================================================ */
__attribute__((used, externally_visible, noinline))
void FrontEndPreLoopSetup(void)
{
#ifdef SWOS_SDL_BUILD
    /* SWOS_DATA_JSON: rebuild normal-country competition tables from data_json/
       before any menu reads competitionsTable[]. No-op unless the env var is set. */
    extern void DataJson_InstallCompetitionGraph(void);
    DataJson_InstallCompetitionGraph();
    /* career_params.json: override the compiled career-mode balance knobs (starting
       balance, gate receipts, wages, board expectations) so testers can tune them
       without a rebuild. Absent file -> compiled defaults. */
    extern void DataJson_LoadCareerParams(void);
    DataJson_LoadCareerParams();
    /* SWOS_MENU_JSON: load every menu from bin/data_json/menus/*.json and overwrite
       the compiled static menu bytes IN PLACE (docs/MENUS.md §2d), so editing the
       JSON changes the running game. Bounded by each menu's compiled size; a menu
       grown beyond it stays compiled and is flagged. Default-ON; SWOS_MENU_JSON=0
       disables. */
    { const char *v = getenv("SWOS_MENU_JSON");
      if (!(v && v[0] == '0' && v[1] == '\0')) {
        extern int MenuJson_InstallAll(void);
        MenuJson_InstallAll();
      } }
#endif
    InitMenuMusic();
    PrepareMenuBackground();
    InitMainMenuStuff();

    if (menuMusic != 0)
    {
        TryPlayMusic();
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
void FrontEndMainLoop(void)
{
    ReadTimerDelta();
    FrontEndPreLoopSetup();

    do
    {
        MenuProc();
    } while (g_quitToDos == 0);
}
