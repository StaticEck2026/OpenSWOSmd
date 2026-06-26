/*
 * arcade.c — SWOS arcade / coin-op mode (SDL-only). See docs/ARCADE.md.
 *
 * Phase 1: boot gate + minimal attract loop (black background, flashing
 * INSERT COIN for P1/P2, MAME coin keys 5/6, F2 service stub). Later phases add
 * the random arcade-pitch background, the SWOS logo + HIL/RPL attract reels, the
 * SVCFONT service menu, and the 45s golden-goal match.
 */
#ifdef SWOS_SDL_BUILD

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "arcade.h"
#include "text_render.h"   /* DrawMenuText */
#include "display.h"       /* _screen_width, SCREEN_HEIGHT, WIDE_MARGIN */

int _gArcadeMode = 0;

extern void WaitForVSync(void);
extern void ReadTimerDelta(void);
extern volatile uint16_t _quitToDos;
extern volatile uintptr_t _frontbufferBasePtr;
extern uint8_t _bigCharsTable[];

#define ARC_COLOUR   14          /* text colour (placeholder until the SVCFONT phase) */
#define ARC_CHARS    ((uint8_t *)_bigCharsTable)

/* DrawMenuText x is in classic 336-space; the blit adds WIDE_MARGIN to centre it
 * in a wide view, so pass plain 0..335 coords here. */
static void arc_clear(void)
{
    memset((void *)_frontbufferBasePtr, 0,
           (size_t)_screen_width * (size_t)SCREEN_HEIGHT);
}

void ArcadeMainLoop(void)
{
    int credits1 = 0, credits2 = 0;
    uint32_t frame = 0;

    fprintf(stderr, "[arcade] boot-to-arcade: entering attract loop\n");

    /* The boot loader leaves the copper palette faded to black; restore the menu
     * palette so the attract screen is visible. (The arcade loop draws its own
     * content over a cleared frontbuffer.) */
    { extern uint16_t _menuPalette[];
      WriteCopperPalette(_menuPalette); }

    ReadTimerDelta();

    while (_quitToDos == 0) {
        int coin1 = 0, coin2 = 0, service = 0;
        Platform_ArcadeInput(&coin1, &coin2, &service);
        if (coin1)  { credits1 += coin1; fprintf(stderr, "[arcade] coin P1 (credits=%d)\n", credits1); }
        if (coin2)  { credits2 += coin2; fprintf(stderr, "[arcade] coin P2 (credits=%d)\n", credits2); }
        if (service) fprintf(stderr, "[arcade] F2 service button — service menu TODO\n");

        arc_clear();

        int flash = (int)((frame >> 4) & 1);   /* ~1.5 Hz blink at 50 fps */
        uint16_t y = 248;                      /* near the bottom of the 4:3 336x272 region */
        /* P1 anchored bottom-LEFT, P2 bottom-RIGHT of the classic 336 view (the
         * blit centres these in a wide window via WIDE_MARGIN). */
        if (credits1 == 0) { if (flash) DrawMenuText(8, y, ARC_COLOUR, (uint8_t *)"INSERT COIN", ARC_CHARS); }
        else                            DrawMenuText(8, y, ARC_COLOUR, (uint8_t *)"PLAYER 1 READY", ARC_CHARS);
        if (credits2 == 0) { if (flash) DrawMenuTextRightAligned(328, y, ARC_COLOUR, (uint8_t *)"INSERT COIN", ARC_CHARS); }
        else                            DrawMenuTextRightAligned(328, y, ARC_COLOUR, (uint8_t *)"PLAYER 2 READY", ARC_CHARS);

        WaitForVSync();
        ReadTimerDelta();
        frame++;
    }
}

#endif /* SWOS_SDL_BUILD */
