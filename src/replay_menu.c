#include "replay_menu.h"
#include "menu_system.h"
#include "text_render.h"
#include "blitter.h"
#include "display.h"
#include "fade.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern volatile int16_t waitingInterrupt;
extern volatile uint16_t currentGameTick;
extern uint16_t paletteIndex;
extern uint16_t replaySelected;
extern uint16_t menuControlsDirection;
extern uint16_t fire;

extern uint8_t frontbufferMaskBuffer[];
extern uint8_t bigCharsTable[];
extern uint16_t kColorTableShine[];

/* String constants in SWOS2.s */
extern uint8_t aGrafsMenubg2Ra[];   /* "grafs/menubg2.raw" */
extern uint8_t aReplay[];           /* "REPLAY" */
extern uint8_t aExit[];             /* "EXIT" */

/* ---- External functions ---- */
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);

/* ================================================================
 * DrawMenuBack150x20  (SWOS2.s line 18456)
 *
 * Draws a 150x20 menu background box with outer frame.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuBack150x20(uint16_t color, uint16_t x, uint16_t y)
{
    uint16_t w = 150;
    uint16_t h = 20;
    _DrawMenuItemBackground(color, x, y, w, h);
    _DrawMenuItemOuterFrame(color, x, y, w, h);
}

/* ================================================================
 * DrawReplayContinueShiningFrame  (SWOS2.s line 18513)
 *
 * Draws an animated shining frame around a 150x20 box.
 * Uses currentGameTick to cycle through kColorTableShine colors.
 *   D0 = x position, D1 = y position
 *
 * Frame consists of 4 FillRectangle calls:
 *   - Top horizontal line:    (x, y, 0x96, 1)
 *   - Bottom horizontal line: (x, y+0x13, 0x96, 1)
 *   - Left vertical line:     (x, y, 1, 0x14)
 *   - Right vertical line:    (x+0x95, y, 1, 0x14)
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawReplayContinueShiningFrame(int16_t x, int16_t y)
{
    uint16_t idx;
    uint16_t color;

    /* Compute animated color from tick */
    idx = currentGameTick >> 1;
    idx &= 7;
    color = kColorTableShine[idx];

    /* Top horizontal line */
    _FillRectangle((uint16_t)x, (uint16_t)y, 0x96, 1, color);
    /* Bottom horizontal line */
    _FillRectangle((uint16_t)x, (uint16_t)(y + 0x13), 0x96, 1, color);
    /* Left vertical line */
    _FillRectangle((uint16_t)x, (uint16_t)y, 1, 0x14, color);
    /* Right vertical line */
    _FillRectangle((uint16_t)(x + 0x95), (uint16_t)y, 1, 0x14, color);
}

/* ================================================================
 * DrawReplayOrExitMenu  (SWOS2.s line 18412)
 *
 * Draws the "REPLAY" and "EXIT" menu boxes twice (double-buffered).
 * Each pass: draw two 150x20 backgrounds, then two centered text
 * labels, then waits for VBlank.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawReplayOrExitMenu(void)
{
    int16_t d7;

    for (d7 = 1; d7 >= 0; d7--)
    {
        /* REPLAY background (color 14, x=101, y=106) */
        _DrawMenuBack150x20(14, 101, 106);
        /* EXIT background (color 12, x=101, y=136) */
        _DrawMenuBack150x20(12, 101, 136);

        /* "REPLAY" text centered (x center=176, y=112, color index=2) */
        _DrawMenuTextCentered(176, 112, 2, aReplay, bigCharsTable);
        /* "EXIT" text centered (x center=176, y=142, color index=2) */
        _DrawMenuTextCentered(176, 142, 2, aExit, bigCharsTable);

        /* Wait for VBlank */
        waitingInterrupt = 1;
        while (waitingInterrupt != 0)
            ;
    }
}

/* ================================================================
 * ReplayExitMenuAfterFriendly  (SWOS2.s line 18349)
 *
 * Shows the post-friendly-match "REPLAY / EXIT" menu.
 * Loads background image, sets up display, then runs an event loop
 * handling up/down navigation and fire to select.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ReplayExitMenuAfterFriendly(void)
{
    /* Load menu background image */
    _LoadFileEx(aGrafsMenubg2Ra, frontbufferMaskBuffer, (uint32_t *)0);

    _ProcessSpriteBlitQueue();
    _BlitterCopyPanelToScreen();

    /* Reset camera and double-buffer swap */
    _SetCameraCoordinates(0, 0);
    _SwapBuffersAndCameraViewState();
    _UpdatePlanePtrsAndBlitParams();
    _ProcessSpriteBlitQueue();
    _BlitterCopyPanelToScreen();
    _SwapBuffersAndCameraViewState();
    _UpdatePlanePtrsAndBlitParams();

    /* Initialize state */
    replaySelected = 1;
    paletteIndex = 0;

    /* Draw initial menu and fade in */
    _DrawReplayOrExitMenu();
    _FadeIn();

    /* Main event loop.
     * Assembly has two entry points:
     *   replay_exit_loop_draw_menu     → full redraw then wait
     *   replay_exit_loop_wait_interrupt → skip redraw, just wait
     * We use needsRedraw flag to replicate this. */
    {
        int16_t needsRedraw = 1;

        for (;;)
        {
            if (needsRedraw)
            {
                _DrawReplayOrExitMenu();
                needsRedraw = 0;
            }

            /* Wait for VBlank */
            waitingInterrupt = 1;
            while (waitingInterrupt != 0)
                ;

            /* Draw shining frame on selected item */
            if (replaySelected != 0)
                _DrawReplayContinueShiningFrame(0x65, 0x6A);
            else
                _DrawReplayContinueShiningFrame(0x65, 0x88);

            /* Process input */
            _MenuCheckControls();

            if (menuControlsDirection == 0)
            {
                /* No direction pressed */
                if (replaySelected != 0)
                    continue;           /* on REPLAY, just wait */
                replaySelected = 1;     /* snap back to REPLAY */
                needsRedraw = 1;
                continue;
            }

            if (menuControlsDirection == 4)
            {
                /* Down pressed */
                if (replaySelected == 0)
                    continue;           /* already on EXIT, just wait */
                replaySelected = 0;     /* switch to EXIT */
                needsRedraw = 1;
                continue;
            }

            /* Other direction (up/left/right) — check fire */
            if (fire != 0)
            {
                _FadeOutToBlack();
                return;
            }
            /* No fire, just wait */
        }
    }
}
