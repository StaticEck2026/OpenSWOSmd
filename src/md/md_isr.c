/* md_isr.c -- the game's clock, its VBlank work, and the controls.
 *
 * The Amiga match runs on its VBlank interrupt (src/interrupts.c _Int3TimerHandler):
 * once the game loop has finished a frame and is waiting, the interrupt swaps the
 * buffers and re-sets the camera; every VBlank it steps the fades and the music and
 * counts the two ticks the simulation reads (currentTick, and currentGameTick unless
 * the game is paused). Everything in the match is "per 50 Hz tick".
 *
 * Outside a match the cartridge does just that: the VBlank is the interrupt, a game tick
 * every 50 Hz. In a match the frames own the clock instead, as the SDL build's netplay
 * step does (NetplayMatch_OwnsClock): each frame of the game loop ends by doing the
 * interrupt's frame work itself -- the swap, the camera, one tick of each counter -- and
 * hands its picture to a two-frame queue; the 50 Hz tick shows the next picture from it.
 * A match therefore plays exactly as an Amiga running at full speed plays it (every frame
 * one tick, as the original always gets when it keeps up), whatever the 68000 takes over
 * any one frame: a slow frame borrows from the faster ones around it, and only a run of
 * slow frames shows (as a picture held a tick longer). It also makes the match the same
 * every time from the same input, which the tests rely on.
 *
 * 50 Hz on any console: a PAL Mega Drive's VBlank is the tick. An NTSC console interrupts
 * at 60 Hz, so one VBlank in six shows no new frame -- a match lasts as long, and a ball
 * travels as fast, as on the Amiga. `md_clock60` (the options' 60 HZ) ticks on every
 * VBlank instead, the game 20% faster, as an NTSC Amiga played it.
 *
 * Controls: the engine reads the Amiga joystick registers (src/input.c); md_input_poll
 * writes them from the pads, the same encoding src/input_sdl.c uses.
 */
#include "md_hw.h"
#include "md_game.h"
#include "game_types.h"
#include "md_render.h"

extern volatile int16_t _waitingInterrupt;
extern volatile uint16_t _currentGameTick;
extern volatile uint16_t _currentTick;
extern volatile uint16_t _didSwapThisFrame;
extern volatile uint16_t _showingUserStats;
extern volatile uint16_t _paused;
extern volatile uint16_t _currentKey;
extern FixedPoint _cameraX, _cameraY;
extern void SwapBuffersAndCameraViewState(void);
extern void SetCameraCoordinates(int16_t targetX, int16_t targetY);
extern void FadeOutTick(void);
extern void FadeInTick(void);

/* The Amiga joystick registers, as src/input.c reads them on SDL and here (defined in
 * src/swos_globals.c). */
extern uint16_t _joy0Dat, _joy1Dat;
extern uint8_t  _ciaa_pra;

uint8_t  md_clock60;               /* 1: one game tick per VBlank on NTSC too */
static uint8_t s_ntscPhase;
volatile uint16_t md_ticks;        /* 50 Hz ticks since power-on */
volatile uint16_t md_lagTicks;      /* match ticks with no new frame to show: the game behind */
volatile uint16_t md_gameFrames;    /* frames the game loop completed */
volatile uint16_t md_matchTicks;    /* ticks while a match was showing frames */
volatile uint8_t  md_inMatch;       /* the game loop is running frames: they own the clock */
static volatile uint8_t s_pending;  /* frames staged and not yet shown (0..2) */
static volatile uint16_t s_emptyRun; /* match ticks since one last had a frame to show */

/* How long each match frame took, in scanlines of CPU: md_frameHist[n] counts frames of
 * 16n..16n+15 lines, md_frameMax the worst. One NTSC VBlank is 262 lines, one PAL 313;
 * at 50 Hz on NTSC a frame has 1.2 VBlanks on average. */
uint16_t md_frameHist[40];
uint16_t md_frameMax;
static uint16_t s_frameStartV, s_frameStartLine;

static uint16_t now_lines(uint16_t *vb)
{
    uint16_t v = md_vblanks, hv = MD_VDP_HV;
    if (md_vblanks != v) { v = md_vblanks; hv = MD_VDP_HV; }
    *vb = v;
    uint16_t lines = (uint16_t)(hv >> 8), top = md_is_pal() ? 240 : 224;
    /* lines since the VBlank (the V counter's jump in the blank is ignored: a few lines) */
    return (uint16_t)(lines >= top ? lines - top : lines + (md_is_pal() ? 313 : 262) - top);
}

static void frame_timing(uint16_t waited)
{
    uint16_t vb, l = now_lines(&vb);
    uint16_t per = md_is_pal() ? 313 : 262;
    uint16_t took = (uint16_t)((uint16_t)(vb - s_frameStartV) * per + l - s_frameStartLine - waited);
    if (md_inMatch) {
        if (took > md_frameMax) md_frameMax = took;
        md_frameHist[took / 16 < 40 ? took / 16 : 39]++;
#ifdef MD_LOG_SLOW_FRAMES
        {
            extern uint16_t _gameState, _gameStatePl, _frameCount;
            if (took > per)
                md_log("slow frame %u: %u lines, state %u/%u, sprites %u", _frameCount, took, _gameState, _gameStatePl, md_sprCount);
        }
#endif
    }
}

/* ---- the frame boundary (game_loop.c waitVBlankSyncPresent) ---- */
void MD_FrameDone(void)
{
    int16_t camX = _cameraX.s.i, camY = _cameraY.s.i;
    /* Ticks that went by with nothing new to show and ended with this frame: the game
     * was behind. A long gap is not that -- a pause, the statistics, a fade between
     * scenes -- and is not counted. */
    md_irq_off();
    if (s_emptyRun && s_emptyRun <= 8) md_lagTicks += s_emptyRun;
    s_emptyRun = 0;
    md_irq_on();
    /* the picture: wait for a free slot in the queue, then stage it; the frame's time
     * counts its logic and its staging, not the wait */
    uint16_t vb, l0 = now_lines(&vb), vb0 = vb;
    while (s_pending >= 2) {}
    uint16_t l1 = now_lines(&vb);
    uint16_t per = md_is_pal() ? 313 : 262;
    uint16_t waited = (uint16_t)((uint16_t)(vb - vb0) * per + l1 - l0);
    md_render_frame();
    frame_timing(waited);
    md_irq_off();
    s_pending++;
    md_irq_on();
    md_gameFrames++;
    /* what the Amiga's interrupt does at the end of a frame (_Int3TimerHandler) */
    SwapBuffersAndCameraViewState();
    SetCameraCoordinates(camX, camY);
    _didSwapThisFrame = 1;
    _currentTick++;
    if (_paused == 0 && _showingUserStats == 0)
        _currentGameTick++;
    md_inMatch = 1;
    {
        uint16_t vb;
        s_frameStartLine = now_lines(&vb);
        s_frameStartV = vb;
    }
}

/* The pause and the statistics overlay (game_loop.c pauseSpinWaitOneTick): no frame runs,
 * the Amiga's interrupt still counts currentTick (and not currentGameTick). */
void MD_WaitTick(void)
{
    uint16_t t = md_ticks;
    while (md_ticks == t) {}
    if (md_inMatch) _currentTick++;
}

void md_match_clock_stop(void)
{
    md_irq_off();
    md_inMatch = 0;
    s_pending = 0;
    md_irq_on();
}

/* _Int3TimerHandler (src/interrupts.c, SWOS2.s:5305-5364), one 50 Hz tick. */
static void game_tick(void)
{
    md_ticks++;
    if (md_inMatch) {
        md_matchTicks++;
        if (s_pending) {
            md_render_commit();     /* the oldest staged frame goes to the VDP now */
            s_pending--;
        } else {
            s_emptyRun++;           /* counted as late if a frame turns up soon (MD_FrameDone) */
        }
    } else {
        /* outside the game loop the interrupt keeps the clock, as on the Amiga */
        _currentTick++;
        if (_paused == 0 && _showingUserStats == 0)
            _currentGameTick++;
    }
    _waitingInterrupt = 0;          /* frees a WaitForVSync (game_utils.c), match or not */
    FadeOutTick();
    FadeInTick();
    md_sound_tick();
}

void md_vblank_irq(void)
{
    int tick = 1;
    md_vblanks++;
    if (!md_is_pal() && !md_clock60) {
        if (++s_ntscPhase >= 6) { s_ntscPhase = 0; tick = 0; }
    }
    if (tick) game_tick();
    md_dma_flush();
    md_sound_frame();
}

/* ---- controls ---- */

/* encode_joydat (src/input_sdl.c): the Amiga JOYxDAT quadrature bits for a direction. */
static uint16_t encode_joydat(int up, int down, int left, int right)
{
    uint16_t v = 0;
    if (right)                  v |= (1u << 1);
    if (right ^ (down ? 1 : 0)) v |= (1u << 0);
    if (left)                   v |= (1u << 9);
    if (left  ^ (up   ? 1 : 0)) v |= (1u << 8);
    return v;
}

#define FIRE_BUTTONS (PAD_A | PAD_B | PAD_C)

/* Pads -> the Amiga's ports. Pad 1 is the Amiga's joystick port (JOY1DAT, fire on CIA-A
 * bit 7), which SWOS calls player 1; pad 2 the mouse port (JOY0DAT, bit 6). */
void md_input_poll(void)
{
    uint16_t p1, p2;
    md_pad_poll();
    p1 = md_pad[0];
    p2 = md_pad[1];
    _joy1Dat = encode_joydat(p1 & PAD_UP, p1 & PAD_DOWN, p1 & PAD_LEFT, p1 & PAD_RIGHT);
    _joy0Dat = encode_joydat(p2 & PAD_UP, p2 & PAD_DOWN, p2 & PAD_LEFT, p2 & PAD_RIGHT);
    _ciaa_pra = 0xFF;
    if (p1 & FIRE_BUTTONS) _ciaa_pra &= (uint8_t)~0x80;
    if (p2 & FIRE_BUTTONS) _ciaa_pra &= (uint8_t)~0x40;
}

/* The keys a match listens for (game_loop.c MainKeysCheck), from the pads:
 *   START        P: pause / resume
 *   X            R: the instant replay
 *   Y            S: the statistics (and S again, or fire, closes them)
 *   MODE, or START+A   ESC: leave the match
 * In the attract mode (md_demo) START or any fire button leaves. */
extern volatile uint16_t _playGame;
extern uint16_t _gameState, _gameStatePl, _replayState;
uint8_t md_abandoned;           /* the match was left before its end (the result screen says so) */

static uint16_t quit_key(void)
{
    /* game_loop.c MainKeysCheck's ESC: once the result is up, leaving is the way out */
    if (_playGame && !_replayState &&
        !(_gameStatePl != ST_GAME_IN_PROGRESS &&
          (_gameState == ST_PLAYERS_GOING_TO_SHOWER || _gameState == ST_RESULT_AFTER_THE_GAME)))
        md_abandoned = 1;
    return 0x1B;
}

static uint16_t pad_key(int port)
{
    uint16_t e = md_pad_edge[port], held = md_pad[port];
    if (md_demo) return (e & (PAD_START | PAD_A | PAD_B | PAD_C | PAD_MODE)) ? 0x1B : 0;
    if ((e & PAD_MODE) || ((e & PAD_START) && (held & PAD_A))) return quit_key();
    if ((e & PAD_A) && (held & PAD_START)) return quit_key();
    if (e & PAD_START) return 0x50;
    if (e & PAD_X) return 0x52;
    if (e & PAD_Y) return 0x53;
    return 0;
}

/* GetKey (src/menu_system.c): the next key, in _currentKey too. */
uint16_t GetKey(void)
{
    uint16_t k;
    md_input_poll();
    k = pad_key(0);
    if (!k) k = pad_key(1);
    _currentKey = k;
    return k;
}
