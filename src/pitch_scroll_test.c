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
extern uint16_t GetKey(void);   /* pops one keyboard key (ASCII), 0 if none */
extern uint8_t _pl1Up, _pl1Down, _pl1Left, _pl1Right;

/* ---- text ---- */
extern uint8_t _bigCharsTable[];

/* ---- copper buffer (for reading back the live scroll registers) ---- */
extern uint8_t _copperListBuffer[];

/* ---- full-load test: player/ball sprite rendering (AGA_TEST.md §5b) ---- */
extern Sprite  *_sortedSprites[];
extern uint16_t _numSpritesToRender;
extern void     DrawSprites(void);
extern void     ProcessSpriteBlitQueue(void);
extern void     SaveCoordinatesForHighlights(int16_t marker, uint16_t camX, uint16_t camY);
extern void     ConvertColorsAndTextSprites(TeamGame *t1, TeamGame *t2);
extern void     InitCurrentGoalHighlight(void);
extern uint16_t _overlayActiveFlag;   /* !=0 -> DrawSprites routes players to bobs, not HW bands */
extern TeamGame _topTeamInGame, _bottomTeamInGame;
extern TeamGame *_topTeamPtr, *_bottomTeamPtr;

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
/* 1 = draw the debug overlay (+ repaint the strip under it each frame, a per-frame
 * ScrollY blit); 0 = pure pitch scroll, no overlay. Default OFF: the overlay's
 * ScrollY repaint adds blitter load that muddies scroll/perf observations. Toggle
 * with the 'O' key or gdb. */
int32_t g_pstShowOverlay = 0;
/* 1 = full-buffer re-fill each frame (verified-clean redraw path, sidesteps the
 * incremental ScrollX leading-edge FMODE strip); 0 = real incremental scroll.
 * Default OFF (0): incremental is the path under test — full-redraw every frame
 * disables it and is very blitter-heavy. gdb-pokeable to A/B. */
int32_t g_pstFullRedraw = 0;
/* 1 = do the scroll blit during VBlank (avoids the FMODE-fetch beam glitch);
 * 0 = old behaviour (blit mid-frame). gdb-pokeable A/B toggle. */
int32_t g_pstVBlankBlit = 1;

/* --- full-load test controls (AGA_TEST.md §5b), gdb-pokeable --- */
int32_t g_pstDrawSprites = 1;   /* 1 = render players+ball via DrawSprites each frame */
int32_t g_pstNumSprites  = 22;  /* player count (ball added on top); clamped */
int32_t g_pstMoveSprites = 0;   /* 1 = add in-place world-X jitter (player moving on the pitch); the pitch scroll already moves them across the screen. Toggle with 'M'. */
/* 1 = force every player onto the bob (general) render stack instead of the
 * Amiga hardware-sprite bands. In AGA 8-plane mode the player hardware-sprite
 * path ("P3 players -> HW sprites") is incomplete: band-routed players draw from
 * the sprite palette (_gamePalette[16..31]) rather than the recolored kit
 * bitplanes, so they render as black / wrong-shade / white silhouettes.
 * _overlayActiveFlag is the game's own "render sprites as bobs" switch (set when
 * the bench overlay is up); forcing it gives a correct render AND a heavier
 * bob-only worst case for the frame budget. Poke g_pstForceBobs=0 to A/B the
 * (currently broken in AGA) hardware-sprite band path. */
int32_t g_pstForceBobs   = 1;
/* Auto-scroll mode, keyboard-selectable (see the loop):
 *   0 = circular circuit (right -> down -> left -> up), the default
 *   1 = up     (W: jump to bottom, scroll up)
 *   2 = left   (A: jump to right,  scroll left)
 *   3 = down   (S: jump to top,    scroll down)
 *   4 = right  (D: jump to left,   scroll right)
 * SPACE returns to circular. Single-axis modes wrap back to the start edge at
 * the far edge so the chosen direction scrolls continuously for observation.
 * Step size = |g_pstAutoScrollX| (default 2). */
int32_t g_pstScrollMode = 0;

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

/* ================================================================
 * Full-load test: render a player + ball complement through the real
 * DrawSprites path (hardware-sprite bands + bobs) so the rig exercises
 * the game's full per-frame rendering budget. See AGA_TEST.md §5b.
 * ================================================================ */
#define PST_MAX_SPRITES 24
static Sprite   s_pstSprites[PST_MAX_SPRITES];
static uint16_t s_pstFrame;
static int16_t  s_pstScrollPhase;   /* circular scroll: 0=right 1=down 2=left 3=up */
static int16_t  s_pstRedrawFrames;  /* >0 = force a full-screen redraw (both buffers) after a camera JUMP */

static void pstSetupSprites(void)
{
    int16_t i;
    /* The rig bypasses match entry, so the player/goalie/ball sprite headers
     * (indices 227..1069) aren't in _spriteGraphics yet. ConvertColorsAndTextSprites
     * runs the image script that unpacks them and recolours the players from the
     * two teams' kit fields.
     *
     * Give each team a distinct, fully-populated kit so we can VISUALLY confirm
     * the 8-plane sprite path renders the full kit-colour complement (shirt,
     * stripes, shorts, socks all different, and team 1 clearly != team 2).
     * prShirtCol/Stripes/Shorts/Socks hold the *converted* in-game palette index
     * directly (the value ConvertTeamColor would produce); valid entries are the
     * shirtColorTable set {1,2,3,6,10,11,12,13,14,15}. Type 0 = solid shirt. */
    /* Palette (RGB4, read from _gamePalette): 10 = red (0x0f00), 2 = white
     * (0x0fff), 11 = blue (0x000f), 15 = yellow (0x0ff0). Max-contrast kits. */
    _topTeamInGame.prShirtType   = 0;      /* team 1 (top): RED shirt / WHITE shorts */
    _topTeamInGame.prShirtCol    = 10;
    _topTeamInGame.prStripesCol  = 10;
    _topTeamInGame.prShortsCol   = 2;
    _topTeamInGame.prSocksCol    = 2;
    _bottomTeamInGame.prShirtType   = 0;   /* team 2 (bottom): BLUE shirt / YELLOW shorts */
    _bottomTeamInGame.prShirtCol    = 11;
    _bottomTeamInGame.prStripesCol  = 11;
    _bottomTeamInGame.prShortsCol   = 15;
    _bottomTeamInGame.prSocksCol    = 15;
    _topTeamPtr    = &_topTeamInGame;
    _bottomTeamPtr = &_bottomTeamInGame;
    ConvertColorsAndTextSprites(&_topTeamInGame, &_bottomTeamInGame);
    /* The rig bypasses match entry, which is where InitCurrentGoalHighlight()
     * normally runs. Without it _currentHilPtr stays 0, so the per-sprite
     * SaveCoordinatesForHighlights() call in DrawQueuedSprites writes from
     * address 0 upward every frame — stomping the m68k exception vectors and
     * low memory until a stray interrupt jumps to a wild PC. Reset it here so
     * highlight recording behaves exactly as it does in a real match (and so
     * that per-frame cost is included in the full-load budget). */
    InitCurrentGoalHighlight();
    /* Point the render list at our fixed array (SortSprites re-sorts it each frame). */
    for (i = 0; i < PST_MAX_SPRITES; i++)
        _sortedSprites[i] = &s_pstSprites[i];
}

static void pstMoveSprites(void)
{
    int16_t n    = (int16_t)g_pstNumSprites;
    int16_t camX = _cameraX.s.i;
    int16_t camY = _cameraY.s.i;
    int16_t i;
    if (n < 0) n = 0;
    if (n > PST_MAX_SPRITES - 1) n = PST_MAX_SPRITES - 1;   /* keep 1 slot for the ball */

    for (i = 0; i < n; i++) {
        Sprite *s   = &s_pstSprites[i];
        int16_t col = (int16_t)(i % 6);
        int16_t row = (int16_t)(i / 6);
        /* Optional in-place jitter (M key, OFF by default). Smooth TRIANGLE
         * (-32..+32..-32), NOT a sawtooth — the old `& 0x3F - 32` snapped 63px at
         * every wrap and double-buffering showed both ends at once (the "grouping
         * into two"). A triangle changes 1px/frame with no jump. */
        int16_t osc = 0;
        if (g_pstMoveSprites) {
            uint16_t ph = (uint16_t)((s_pstFrame + (uint16_t)i * 7u) & 0x7Fu);  /* 0..127 */
            int16_t  tri = (ph < 64) ? (int16_t)ph : (int16_t)(128 - ph);       /* 0..64..0 */
            osc = (int16_t)(tri - 32);                                          /* -32..+32..-31 */
        }
        s->teamNumber = (int16_t)((i < n / 2) ? 1 : 2);
        s->imageIndex = (int16_t)((i < n / 2) ? 227 : 530);  /* standing player, up */
        s->direction  = 0;
        s->saveSprite = 0;
        s->visible    = 1;
        /* Camera-relative = FIXED SCREEN position: the players hold their spot on
         * screen while the pitch scrolls under them (owner prefers this for the
         * test — keeps the whole complement in view). Base Y +90 keeps the grid on
         * the green, below the ads banner. With jitter off they don't slide. */
        s->x.s.i = (int16_t)(camX + 40 + col * 48 + osc);
        s->y.s.i = (int16_t)(camY + 90 + row * 44);
        s->z.s.i = 0;
    }
    /* the ball (soccer_s, index 1068, 8bpp bob), also screen-fixed */
    {
        Sprite *b = &s_pstSprites[n];
        b->teamNumber = 3;
        b->imageIndex = 1068;
        b->direction  = 0;
        b->saveSprite = 0;
        b->visible    = 1;
        b->x.s.i = (int16_t)(camX + 168);
        b->y.s.i = (int16_t)(camY + 130);
        b->z.s.i = 0;
    }
    _numSpritesToRender = (uint16_t)(n + 1);
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

    /* full-load test: load player/ball sprite headers + build the render list once */
    pstSetupSprites();

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
        /* keyboard: W/A/S/D jump to an edge and scroll one axis; SPACE = circular.
         * Codes are ASCII (GetKey runs the rawkey->ASCII LUT). */
        {
            uint16_t k = GetKey();
            /* W/A/S/D JUMP the camera to an edge, so the whole screen must be
             * re-filled — the incremental scroll only redraws edge columns and
             * would leave the interior as garbage. Force a 2-frame full redraw
             * (one per double-buffer) after each jump, then resume incremental. */
            if      (k == 0x57) { g_pstScrollMode = 1; g_pstCamY = maxY; s_pstRedrawFrames = 2; }  /* W: bottom -> up    */
            else if (k == 0x41) { g_pstScrollMode = 2; g_pstCamX = maxX; s_pstRedrawFrames = 2; }  /* A: right  -> left  */
            else if (k == 0x53) { g_pstScrollMode = 3; g_pstCamY = 0;    s_pstRedrawFrames = 2; }  /* S: top    -> down  */
            else if (k == 0x44) { g_pstScrollMode = 4; g_pstCamX = 0;    s_pstRedrawFrames = 2; }  /* D: left   -> right */
            else if (k == 0x20) { g_pstScrollMode = 0;                   }  /* SPACE: circular (no jump) */
            else if (k == 0x42) { g_pstDrawSprites = !g_pstDrawSprites;  }  /* B: sprites on/off  */
            else if (k == 0x48) { g_pstForceBobs   = !g_pstForceBobs;    }  /* H: bob <-> HW path */
            else if (k == 0x4F) { g_pstShowOverlay = !g_pstShowOverlay;  }  /* O: overlay on/off  */
            else if (k == 0x4D) { g_pstMoveSprites = !g_pstMoveSprites;  }  /* M: player jitter   */
        }
        {
            int16_t spd = (int16_t)(g_pstAutoScrollX ? g_pstAutoScrollX : 2);
            if (spd < 0) spd = (int16_t)-spd;
            switch (g_pstScrollMode)
            {
                /* single-axis: scroll one way, wrap back to the start edge at the far
                 * edge so the chosen direction runs continuously. The wrap is a camera
                 * JUMP, so it needs a full redraw too (s_pstRedrawFrames) — otherwise the
                 * incremental scroll only repaints edge columns and the interior stays as
                 * pre-jump garbage tiles (the pitch corruption). */
                case 1:  g_pstCamY -= spd; if (g_pstCamY <= 0)    { g_pstCamY = maxY; s_pstRedrawFrames = 2; } break;  /* up    */
                case 2:  g_pstCamX -= spd; if (g_pstCamX <= 0)    { g_pstCamX = maxX; s_pstRedrawFrames = 2; } break;  /* left  */
                case 3:  g_pstCamY += spd; if (g_pstCamY >= maxY) { g_pstCamY = 0;    s_pstRedrawFrames = 2; } break;  /* down  */
                case 4:  g_pstCamX += spd; if (g_pstCamX >= maxX) { g_pstCamX = 0;    s_pstRedrawFrames = 2; } break;  /* right */
                default: /* 0 = circular circuit: right -> down -> left -> up */
                    switch (s_pstScrollPhase)
                    {
                        case 0:  g_pstCamX += spd; if (g_pstCamX >= maxX) { g_pstCamX = maxX; s_pstScrollPhase = 1; } break;
                        case 1:  g_pstCamY += spd; if (g_pstCamY >= maxY) { g_pstCamY = maxY; s_pstScrollPhase = 2; } break;
                        case 2:  g_pstCamX -= spd; if (g_pstCamX <= 0)    { g_pstCamX = 0;    s_pstScrollPhase = 3; } break;
                        default: g_pstCamY -= spd; if (g_pstCamY <= 0)    { g_pstCamY = 0;    s_pstScrollPhase = 0; } break;
                    }
                    break;
            }
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

        /* Undraw last frame's bobs BEFORE scrolling — game_loop.c runs
         * ProcessSpriteBlitQueue (647) ahead of ScrollToCurrent (682). Undrawing
         * AFTER the scroll (as this rig used to) restores each bob's stale
         * pre-scroll background onto the freshly-scrolled pitch, and the scroll
         * blit drags the still-baked-in old bobs to new positions the undraw then
         * misses — the smears / half-drawn / flickering sprites. Undraw here, on
         * the just-swapped back buffer at its pre-scroll position, so the restore
         * aligns and the scroll then shifts a clean pitch. */
        /* Always undraw last frame's bobs — even the frame right after toggling
         * sprites off (B), so the final bobs get erased instead of staying baked
         * into the scrolling pitch. An empty queue (sprites already off) is a
         * cheap no-op. */
        ProcessSpriteBlitQueue();

        /* --- move camera + blit the newly-exposed edge --- */
        SetCameraCoordinates(g_pstCamX, g_pstCamY);
        if (g_pstFullRedraw || s_pstRedrawFrames > 0)
        {
            /* Full-buffer re-fill (ScrollY over the whole screen height) instead of
             * the incremental edge blit. Used both as a diagnostic (g_pstFullRedraw)
             * and for the 2 frames after a camera JUMP (s_pstRedrawFrames) so the
             * whole screen is repainted at the new position rather than left as the
             * pre-jump garbage the incremental edge-blit can't reach. */
            _oldCameraX = _cameraX.s.i;                              /* no X delta -> ScrollX no-op */
            _oldCameraY = (int16_t)(_cameraY.s.i - SCREEN_HEIGHT);   /* deltaY = full screen */
            ScrollYUpdateRowsAndBlit();
            if (s_pstRedrawFrames > 0)
                s_pstRedrawFrames--;
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

        /* UpdatePlanePtrsAndBlitParams() is deliberately NOT called here (before the
         * bob draw). Calling it mid-render pointed the copper's block-A plane pointers
         * at the buffer being drawn; a VBLANK landing in the window before DrawSprites
         * finished then switched the live display to the half-drawn buffer, producing
         * the intermittent progressive top-to-bottom sprite "flicker" during vertical
         * scroll (the ex-§5d tear). It is called AFTER DrawSprites below, so the copper
         * only ever adopts a fully-drawn buffer. The real game_loop.c never calls it
         * between scroll and draw either — per frame the VBlank ISR does it at the swap. */

        /* full-load test: render the player/ball complement (bands + bobs) after the
         * pitch, mirroring game_loop.c. Exercises the full per-frame render budget. */
        if (g_pstDrawSprites) {
            /* Draw block, run AFTER the scroll (game_loop.c 683/711/720). The
             * bob undraw (ProcessSpriteBlitQueue) already ran before the scroll,
             * above. WaitForVSync ran the Level-3 ISR's SwapBuffersAndCameraViewState,
             * which alternates the sprite-blit-queue sentinel (A/B) — we must NOT
             * reset the cursor. The per-sprite SaveCoordinatesForHighlights in
             * DrawQueuedSprites needs _currentHilPtr initialised (pstSetupSprites
             * calls InitCurrentGoalHighlight for that). */
            pstMoveSprites();
            _overlayActiveFlag = (uint16_t)(g_pstForceBobs ? 1 : 0);
            SaveCoordinatesForHighlights((int16_t)0xFFFF, (uint16_t)_cameraX.s.i, (uint16_t)_cameraY.s.i);
            DrawSprites();
            SaveCoordinatesForHighlights((int16_t)0xFFFE, 0, 0);
        }
        /* Point the copper at the just-drawn buffer ONLY now that it is fully
         * written (see the note above the draw block). */
        UpdatePlanePtrsAndBlitParams();
        s_pstFrame++;

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
