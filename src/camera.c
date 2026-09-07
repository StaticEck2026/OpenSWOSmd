/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "camera.h"
#include "game_types.h"
#include "scroll_blit.h"
#include "display.h"      /* _screen_width — widescreen pan-range clamp in MoveView */

/* ---- External globals ---- */
extern FixedPoint _cameraX;
extern FixedPoint _cameraY;
extern uint16_t _cameraXMod320;
extern uint16_t _cameraYMod320;
extern uint16_t _cameraCoordinatesValid;

/* ---- External C functions ---- */
extern void SetCameraCoordinates(int16_t targetX, int16_t targetY);

#ifdef SWOS_SDL_BUILD
/* The render origin — see display.h. `_cameraX` is the simulation camera and
 * may sit anywhere in [0, 0x2A0-336]; a render wider than 336 is centred on it
 * and then clamped so it never leaves the 0x2A0-wide pitch. Folds to
 * `_cameraX.s.i` whenever the render is 336 wide. */
int16_t Render_CameraX(void)
{
    int16_t off = (int16_t)((_screen_width - SCREEN_WIDTH) / 2);
    int16_t max = (int16_t)(0x2A0 - _screen_width);
    int16_t x   = (int16_t)(_cameraX.s.i - off);

    if (max < 0) max = 0;
    if (x < 0)   x = 0;
    else if (x > max) x = max;
    return x;
}
#endif

/* ---- Sub-pixel scroll residual -------------------------------------------
 * `_cameraX/_cameraY` are 16.16, but everything is DRAWN at the integer part,
 * so a true camera velocity of e.g. 1.5 px/frame renders as an alternating
 * 1,2,1,2 px step. Measured on the corpus: 38% of consecutive moving frames
 * change step size, and a long ball peaks at 5 px in one 20 ms frame. That
 * quantisation is what testers describe as "not 100% fluid" — it is NOT a
 * frame-pacing problem and no vsync setting touches it.
 *
 * We publish the residual (true position - drawn position) so the presenter
 * can shift the finished frame by that fraction. Everything on screen is drawn
 * relative to the camera, so one uniform shift is correct for the whole scene.
 *
 * Published as the raw 16.16 fraction, not a float: this file is shared with
 * the m68k build and the fixed-width-int rule applies. platform_sdl.c does the
 * float maths, and only it knows the output scale.
 *
 * A TIMESTAMP, not a consume-and-clear flag: several presents can happen per
 * ScrollToCurrent (waitVBlankSync spins, the timer-thread fallback), and
 * zeroing on the first one would reintroduce exactly the 1 px jitter this
 * removes. Menus never publish, so they decay to zero and present unshifted. */
#ifdef SWOS_SDL_BUILD
static uint16_t s_scroll_frac_x, s_scroll_frac_y;
static uint64_t s_scroll_frac_ms;

void Render_PublishScrollSubpixel(void)
{
    extern uint64_t Platform_NowMs(void);
    int16_t drawn = Render_CameraX();
    int16_t off   = (int16_t)((_screen_width - SCREEN_WIDTH) / 2);
    int16_t max   = (int16_t)(0x2A0 - _screen_width);
    if (max < 0) max = 0;

    /* X goes through the same clamp Render_CameraX applies, so at the pitch
     * edges — where the view is pinned and NOT scrolling — the residual comes
     * out zero instead of smearing the frame by a stale fraction. */
    int32_t truex = (int32_t)_cameraX.s.i - off;
    if (truex < 0 || truex > max || truex != (int32_t)drawn)
        s_scroll_frac_x = 0;          /* clamped or off-view: no sub-pixel term */
    else
        s_scroll_frac_x = _cameraX.s.frac;

    /* Y has no render-side clamp helper; pitch.c pins `_cameraY.s.i` to 0 at
     * the top without clearing the fraction, so a pinned camera can carry a
     * stale <1 px residual. Harmless (it is a constant sub-pixel offset, not
     * motion), and it self-corrects as soon as the camera moves again. */
    s_scroll_frac_y = _cameraY.s.frac;

    s_scroll_frac_ms = Platform_NowMs();
}

/* Returns 0 (and leaves *fx/*fy at 0) unless a match frame published recently. */
int Render_ScrollSubpixel(uint16_t *fx, uint16_t *fy)
{
    extern uint64_t Platform_NowMs(void);
    *fx = 0; *fy = 0;
    if (!s_scroll_frac_ms) return 0;
    /* 100 ms ~= five 50 Hz frames. Long enough to survive a stall, short
     * enough that leaving the match drops the shift within a few menu frames. */
    if (Platform_NowMs() - s_scroll_frac_ms > 100) return 0;
    *fx = s_scroll_frac_x; *fy = s_scroll_frac_y;
    return 1;
}
#endif

/* ================================================================
 * ScrollToCurrent  (SWOS2.s line 5625)
 *
 * Sets cameraCoordinatesValid = 1, then calls ScrollX and ScrollY
 * to blit pitch columns/rows matching the current camera position.
 * ================================================================ */
/* Set while MoveView/InitCameraMoveView is panning the camera to a target (match-setup
 * pitch pan, replay seek). During such a pan ScrollToCurrent is driven pixel-by-pixel and
 * expects plain incremental ScrollX+ScrollY — the left-edge full-refill below must NOT fire
 * (it rewrites _oldCameraX/_oldCameraY, derailing the pan and blacking the match on entry).
 * The seam fix is only needed for LIVE gameplay scrolling (game_loop's ScrollToCurrent). */
static uint8_t s_inCameraPan = 0;

__attribute__((used, externally_visible))
void ScrollToCurrent(void)
{
    _cameraCoordinatesValid = 1;
#ifdef SWOS_SDL_BUILD
    /* SDL: incremental scroll blits are no-ops (see scroll_blit.c).
     * Instead, redraw the whole visible pitch from the chunky tile
     * bank each call — matches swos-port-c's whole-screen chunky
     * pitch redraw shape. */
    extern void DrawChunkyPitch(void);
    DrawChunkyPitch();
    /* One publish site for the whole game: this is THE per-frame pitch scroll
     * (game_loop.c:693). Menus don't come through here, so they never get a
     * sub-pixel shift. */
    Render_PublishScrollSubpixel();
#else
    /* The FMODE left-verge "column strip" fix used to force a full-screen ScrollY refill here
     * (expensive: ~284k cycles/frame at the left edge, and it clobbered _oldCameraX/_oldCameraY).
     * It now lives in ScrollXUpdateColumnsAndBlit as a cheap margin-only ScrollYVergeRefresh()
     * — see scroll_blit.c. So ScrollToCurrent is just the plain incremental scroll again, and it
     * works in the rig too (the old fix was _playGame-gated and never ran there). */
    ScrollXUpdateColumnsAndBlit();
    ScrollYUpdateRowsAndBlit();
#endif
}

/* ================================================================
 * MoveView  (SWOS2.s line 5576)
 *
 * Iteratively moves cameraX.s.i toward newScreenX (one pixel per step,
 * with wrap), calling ScrollToCurrent each step. Once X matches,
 * does the same for cameraY.s.i toward newScreenY.
 *
 * 68k semantics: each increment/decrement adjusts both cameraX.s.i and
 * cameraXMod320 (wrapping at SCROLL_X_WRAP), and cameraY.s.i/cameraYMod320
 * (wrapping at 272).
 * ================================================================ */
__attribute__((used, externally_visible))
void MoveView(int16_t newScreenX, int16_t newScreenY)
{
    uint8_t savedPan = s_inCameraPan;
    s_inCameraPan = 1;   /* disable the live-gameplay seam refill during this pan */
    /* These clamps map a DOS-336 recorded replay camera into our wider view and
     * must run ONLY for loaded HIL/RPL playback (_playHighlightFlag). highlight.c
     * calls MoveView directly with the recorded X; the live in-game camera — and
     * the in-game instant replay, recorded in our own space — does its own
     * clamping (SetCameraCoordinates), so applying these here clamped the up-pan
     * to the subs bench, which legitimately leaves the playing-field Y range. */
    extern uint16_t _playHighlightFlag;
    if (_playHighlightFlag != 0) {
        /* X: 0x2A0 (pitch world width) - _screen_width is the max camera X — the
         * same clamp SetCameraCoordinates applies. No-op at 4:3 / on Amiga. */
        int16_t panRange = SIM_CAMERA_PAN_RANGE;
        if (panRange < 0) panRange = 0;
        if (newScreenX < 0) newScreenX = 0;
        else if (newScreenX > panRange) newScreenX = panRange;

        /* Y: the live camera's vertical range [0x10, 0x260]. A DOS HIL captured
         * in a 200px-tall view reaches ~0x295, which on our taller (272px)
         * playback view would pan past the bottom into the green surround.
         * This one stays keyed to the PLAYBACK view (0x260), not the
         * simulation view: it exists precisely to re-fit a foreign recording
         * to the window we are drawing in. */
        if (newScreenY < 0x10) newScreenY = 0x10;
        else if (newScreenY > 0x260) newScreenY = 0x260;
    }

    /* ---- Phase 1: move X ---- */
    /* Original asm uses do-while: ScrollToCurrent fires before the exit
     * check, so it always runs at least once and also runs at the final
     * target position.  This ensures the last tile boundary crossing is
     * blitted — critical for replay camera panning. */
    for (;;) {
        ScrollToCurrent();
        if (_cameraX.s.i == newScreenX)
            break;
        if (_cameraX.s.i < newScreenX) {
            /* Move right */
            _cameraX.s.i += 1;
            _cameraXMod320 += 1;
            if (_cameraXMod320 == SCROLL_X_WRAP)
                _cameraXMod320 = 0;
        } else {
            /* Move left */
            _cameraX.s.i -= 1;
            _cameraXMod320 -= 1;
            if ((int16_t)_cameraXMod320 < 0)
                _cameraXMod320 = SCROLL_X_WRAP - 1;
        }
    }

    /* ---- Phase 2: move Y ---- */
    for (;;) {
        ScrollToCurrent();
        if (_cameraY.s.i == newScreenY)
            break;
        if (_cameraY.s.i < newScreenY) {
            /* Move down */
            _cameraY.s.i += 1;
            _cameraYMod320 += 1;
            if (_cameraYMod320 == 272)
                _cameraYMod320 = 0;
        } else {
            /* Move up */
            _cameraY.s.i -= 1;
            _cameraYMod320 -= 1;
            if ((int16_t)_cameraYMod320 < 0)
                _cameraYMod320 = 271;
        }
    }
    s_inCameraPan = savedPan;
}

/* ================================================================
 * InitCameraMoveView  (SWOS2.s line 5557)
 *
 * Initializes camera by scrolling to two known positions (targetX,272)
 * and (cameraX.s.i,0), then calls MoveView to reach the actual target.
 *
 * Register semantics from asm:
 *   D1 = target X (passed by caller)
 *   D2 = target Y (saved, then restored for MoveView)
 * ================================================================ */
__attribute__((used, externally_visible))
void InitCameraMoveView(int16_t targetX, int16_t targetY)
{
    uint8_t savedPan = s_inCameraPan;
    s_inCameraPan = 1;   /* these setup ScrollToCurrent calls are a pan, not live scroll */

    /* First pass: set camera to (targetX, 272), scroll */
    SetCameraCoordinates(targetX, 272);
    ScrollToCurrent();

    /* Second pass: set camera to (cameraX.s.i, 0), scroll */
    SetCameraCoordinates(_cameraX.s.i, 0);
    ScrollToCurrent();

    /* Final: iteratively move to (cameraX.s.i, targetY) */
    MoveView(_cameraX.s.i, targetY);

    s_inCameraPan = savedPan;
}
