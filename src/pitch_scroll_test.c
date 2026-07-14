/*
 * pitch_scroll_test.c — standalone pitch scroll test harness
 *
 * A minimal display loop that shows JUST a pitch (no players/AI/ball/fades)
 * and lets the camera be scrolled up/down/left/right, with an on-screen
 * overlay of the live scroll parameters. Purpose: isolate + live-tune the
 * AGA FMODE=1 pitch scroll (fine-scroll smoothness, display/blit alignment,
 * and the copper block-B vertical wrap) without any match-loop noise.
 *
 * Entered instead of the menu when built with -DPITCH_SCROLL_TEST (see the
 * hook at the tail of mainC in SWOS.c). Amiga/AGA only — the body compiles to
 * an empty stub on every other target (this test drives the real Amiga copper
 * scroll path, which the SDL build renders chunky and does not exercise).
 *
 * Drive it with the joystick on port 1 (arrow keys / numpad under FS-UAE), or
 * poke g_pstCamX / g_pstCamY directly from gdb.
 */

#include "swos_types.h"
#include "game_types.h"
#include "display.h"
#include "text_render.h"

/* Amiga-both (AGA + OCS): the rig drives the general copper scroll path, which
 * exists in both depths. Relaxed from AGA-only so OCS can be captured for the
 * scroll-corruption comparison. */
#if !defined(SWOS_SDL_BUILD)

/* ---- pitch setup ---- */
extern uint16_t _pitchNumberAndType;
extern void LoadPitchMap(uint16_t index);
extern void FixupAndColorPitchFile(void);
extern void WriteCopperPitchPalette(void);
extern void ClearFrontbuffers(void);
extern uint16_t _paletteIndex;

/* ---- scroll / display ---- */
extern void ScrollToCurrent(void);
extern void ScrollYUpdateRowsAndBlit(void);
extern void ReplayUpdateCameraAndSwapBuffers(int16_t targetX, int16_t targetY);
extern void SetCameraCoordinates(int16_t targetX, int16_t targetY);
extern void UpdatePlanePtrsAndBlitParams(void);
extern void WaitForVSync(void);
extern int16_t _oldCameraY;
extern int16_t _oldCameraX;

/* height (screen rows) of the top-left debug overlay strip; repainted with
 * pitch each frame so the text doesn't smear as the buffer scrolls */
#define PST_OVERLAY_ROWS 40

/* ---- camera state ---- */
extern FixedPoint _cameraX;
extern FixedPoint _cameraY;
extern uint16_t _cameraXMod320;
extern uint16_t _cameraYMod320;

/* ---- input ---- */
extern void Player1StatusProc(void);
extern uint8_t _pl1Up, _pl1Down, _pl1Left, _pl1Right;

/* ---- text ---- */
extern uint8_t _bigCharsTable[];

/* ---- copper buffer (for reading back the live scroll registers) ---- */
extern uint8_t _copperListBuffer[];

/* gdb-pokeable camera target (plain globals so a watchpoint / set works) */
int16_t g_pstCamX = 0;
int16_t g_pstCamY = 0;
int16_t g_pstSpeed = 2;
/* gdb-pokeable continuous auto-scroll (px/frame, 0 = off); bounces at edges */
int16_t g_pstAutoScrollX = 0;

/* gdb-pokeable copper-register overrides (see UpdatePlanePtrsAndBlitParams,
 * gated PITCH_SCROLL_TEST). -1 = use the camera-computed value. Lets me sweep
 * BPLCON1 / DDFSTRT / DDFSTOP / modulo live to find the real bit->pixel map. */
int32_t g_pstB1Force = -1;
int32_t g_pstDsForce = -1;
int32_t g_pstDeForce = -1;
int32_t g_pstMoForce = -1;
/* coarse plane-pointer byte nudge (see UpdatePlanePtrsAndBlitParams,
 * gated PITCH_SCROLL_TEST). 0 = camera-computed. Lets me sweep the pointer
 * offset live to nail the FMODE=1 positive-shift "left strip" fix. */
int32_t g_pstPtrByteOff = 0;
/* 1 = draw the debug overlay (+ repaint the strip under it each frame);
 * 0 = pure pitch scroll, no overlay, no strip-repaint (gdb-pokeable). */
int32_t g_pstShowOverlay = 1;
/* 1 = full-buffer re-fill each frame (verified-clean redraw path, sidesteps the
 * incremental ScrollX leading-edge FMODE strip); 0 = real incremental scroll.
 * gdb-pokeable — toggle to A/B the incremental bug vs the display path. */
int32_t g_pstFullRedraw = 1;
/* 1 = do the scroll blit during VBlank (avoids the FMODE-fetch beam glitch);
 * 0 = old behaviour (blit mid-frame). gdb-pokeable A/B toggle. */
int32_t g_pstVBlankBlit = 1;

/* one nibble -> hex ASCII */
static uint8_t hexNib(uint16_t v)
{
    v &= 0xF;
    return (uint8_t)(v < 10 ? ('0' + v) : ('A' + (v - 10)));
}

/* write "XXXX" (4 hex digits) into buf, return buf+4 */
static uint8_t *putHex16(uint8_t *buf, uint16_t v)
{
    *buf++ = hexNib(v >> 12);
    *buf++ = hexNib(v >> 8);
    *buf++ = hexNib(v >> 4);
    *buf++ = hexNib(v);
    return buf;
}

/* copy a literal label, return end */
static uint8_t *putStr(uint8_t *buf, const char *s)
{
    while (*s)
        *buf++ = (uint8_t)*s++;
    return buf;
}

/* write a signed decimal (camera coords are easier to read as decimal) */
static uint8_t *putDec(uint8_t *buf, int16_t v)
{
    uint8_t tmp[6];
    int16_t n = 0;
    uint16_t u;
    if (v < 0) { *buf++ = '-'; u = (uint16_t)(-v); }
    else       { u = (uint16_t)v; }
    if (u == 0) tmp[n++] = '0';
    while (u) { tmp[n++] = (uint8_t)('0' + (u % 10u)); u /= 10u; }
    while (n) *buf++ = tmp[--n];
    return buf;
}

/* read a 16-bit value out of the copper list at byte offset */
static uint16_t copRead(uint16_t off)
{
    return *(volatile uint16_t *)(void *)(_copperListBuffer + off);
}

__attribute__((used, externally_visible))
void PitchScrollTest(void)
{
    int16_t maxX, maxY;

    /* --- set up a single pitch --- */
    _pitchNumberAndType = 0x0004;              /* pitch 0, normal type */
    LoadPitchMap((uint16_t)(_pitchNumberAndType >> 8));
    FixupAndColorPitchFile();
    ClearFrontbuffers();
#ifdef AGA
    WriteCopperPitchPalette();                   /* AGA-only, mirrors game_loop */
#else
    { extern uint16_t _gamePalette[]; extern void WriteCopperPalette(const uint16_t *);
      WriteCopperPalette(_gamePalette); }        /* OCS: push 16-colour pitch palette to copper */
#endif
    _paletteIndex = 2;                          /* pitch palette bank */

    /* pan ranges: X matches the in-game clamp (SetCameraCoordinates clamps to
     * [0, 0x2A0 - screen width]). Y covers the FULL pitch: the pitch is 55 tile
     * rows (PITCH_POINTER_COUNT 2310 / 42 tiles-per-row) x 16px = 880px tall, so
     * the camera-Y range is 880 - SCREEN_HEIGHT = 608 (view bottom at the last
     * tile row). This exercises cameraYMod320 through its full 0..271 wrap. */
    maxX = (int16_t)(0x2A0 - SCREEN_WIDTH);
    maxY = 608;

    g_pstCamX = 0;
    g_pstCamY = 0;

    /* paint BOTH double-buffers with the pitch at the start position */
    ReplayUpdateCameraAndSwapBuffers(g_pstCamX, g_pstCamY);

    for (;;)
    {
        /* --- input: joystick port 1 (arrow keys / numpad under FS-UAE) --- */
        Player1StatusProc();
        if (_pl1Left)  g_pstCamX -= g_pstSpeed;
        if (_pl1Right) g_pstCamX += g_pstSpeed;
        if (_pl1Up)    g_pstCamY -= g_pstSpeed;
        if (_pl1Down)  g_pstCamY += g_pstSpeed;
        /* gdb-pokeable auto-scroll (px/frame): lets me FREE-RUN a continuous
         * incremental scroll and screenshot live mid-motion (the horizontal
         * "empty column" is a dynamic transient that step-and-halt misses).
         * Bounces at the edges so it stays incremental (no big jump). */
        if (g_pstAutoScrollX)
        {
            g_pstCamX += g_pstAutoScrollX;
            if (g_pstCamX >= maxX)   { g_pstCamX = maxX; g_pstAutoScrollX = -g_pstAutoScrollX; }
            else if (g_pstCamX <= 0) { g_pstCamX = 0;    g_pstAutoScrollX = -g_pstAutoScrollX; }
        }
        if (g_pstCamX < 0)      g_pstCamX = 0;
        else if (g_pstCamX > maxX) g_pstCamX = maxX;
        if (g_pstCamY < 0)      g_pstCamY = 0;
        else if (g_pstCamY > maxY) g_pstCamY = maxY;

        /* Blit during VBLANK, not mid-frame: under FMODE=1 the 8-plane lores
         * fetch is so DMA-heavy that a scroll blit running during the VISIBLE
         * display steals fetch cycles and glitches the beam (the intermittent
         * left-edge strip — only ever on blitter-active frames). Waiting for
         * VBlank here (the ISR also does the buffer swap) means the column blit
         * below runs while the bitplanes are idle. gdb-toggle g_pstVBlankBlit=0
         * to A/B against the old mid-frame-blit behaviour. */
        if (g_pstVBlankBlit) WaitForVSync();

        /* --- move camera + blit the newly-exposed edge --- */
        SetCameraCoordinates(g_pstCamX, g_pstCamY);
        if (g_pstFullRedraw)
        {
            /* DIAGNOSTIC: force a FULL-buffer re-fill each frame (ScrollY over the
             * whole screen height) instead of the incremental edge blit. This is
             * the verified-clean redraw path — if the scroll is clean+smooth with
             * this on, the display/H7 path is correct and the incremental ScrollX
             * is the sole source of the leading-edge strip. */
            _oldCameraX = _cameraX.s.i;                              /* no X delta -> ScrollX no-op */
            _oldCameraY = (int16_t)(_cameraY.s.i - SCREEN_HEIGHT);   /* deltaY = full screen */
            ScrollYUpdateRowsAndBlit();
        }
        else
        {
            ScrollToCurrent();
        }

        /* --- repaint the pitch under the overlay strip so last frame's text
         *     doesn't smear: drive ScrollY with a synthetic "scrolled up by
         *     PST_OVERLAY_ROWS" delta, which redraws the top PST_OVERLAY_ROWS
         *     screen rows with pitch. Leaves _oldCameraY == cameraY (no effect
         *     on next frame's real scroll-delta detection). --- */
        if (g_pstShowOverlay)
        {
            _oldCameraY = (int16_t)(_cameraY.s.i + PST_OVERLAY_ROWS);
            ScrollYUpdateRowsAndBlit();
        }

        UpdatePlanePtrsAndBlitParams();

        /* --- debug overlay (screen coords; the glyph blitter adds the wrap) --- */
        if (g_pstShowOverlay)
        {
            uint8_t line[48];
            uint8_t *p;
            uint16_t coarse = (uint16_t)((_cameraXMod320 >> 3) & SCROLL_X_COARSE_ALIGN_MASK);
            uint16_t frac   = (uint16_t)(_cameraXMod320 & 0x1F);

            /* line 1: camera X + wrap + coarse byte offset + fine fraction */
            p = line;
            p = putStr(p, "X ");   p = putDec(p, _cameraX.s.i);
            p = putStr(p, " XM "); p = putDec(p, (int16_t)_cameraXMod320);
            p = putStr(p, " CO "); p = putHex16(p, coarse);
            p = putStr(p, " FR "); p = putHex16(p, frac);
            *p = 0;
            DrawMenuText(4, 4, 2, line, _bigCharsTable);

            /* line 2: copper scroll registers actually in effect */
            p = line;
            p = putStr(p, "B1 "); p = putHex16(p, copRead(COP_BPLCON1_VAL_OFF));
            p = putStr(p, " DS "); p = putHex16(p, copRead(COP_DDFSTRT_VAL_OFF));
            p = putStr(p, " DE "); p = putHex16(p, copRead(COP_DDFSTOP_VAL_OFF));
            p = putStr(p, " MO "); p = putHex16(p, copRead(COP_BPL1MOD_VAL_OFF));
            *p = 0;
            DrawMenuText(4, 16, 2, line, _bigCharsTable);

            /* line 3: camera Y + wrap (block-B size grows with YM) */
            p = line;
            p = putStr(p, "Y ");   p = putDec(p, _cameraY.s.i);
            p = putStr(p, " YM "); p = putDec(p, (int16_t)_cameraYMod320);
            *p = 0;
            DrawMenuText(4, 28, 2, line, _bigCharsTable);
        }
        if (!g_pstVBlankBlit) WaitForVSync();   /* old behaviour: blit ran mid-frame */
    }
}

#else  /* not Amiga-AGA: this test only drives the Amiga copper scroll path */

__attribute__((used))
void PitchScrollTest(void)
{
}

#endif
