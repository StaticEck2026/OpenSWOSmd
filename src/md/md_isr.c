/* md_isr.c -- the game's clock, its VBlank work, and the controls.
 *
 * The Amiga match runs on its VBlank interrupt (src/interrupts.c _Int3TimerHandler):
 * once the game loop has finished a frame and is waiting, the interrupt swaps the
 * buffers and re-sets the camera; every VBlank it steps the fades and the music and
 * counts the two ticks the simulation reads (currentTick, and currentGameTick unless
 * the game is paused). Everything in the match is "per 50 Hz tick".
 *
 * The Mega Drive's VBlank is that interrupt. On a PAL console it is the same 50 Hz.
 * An NTSC console interrupts at 60 Hz, and the game clock still has to run at 50: one
 * VBlank in six does no game work (the frame on screen stays up one more refresh), so a
 * match lasts as long, and a ball travels as fast, as on the Amiga. `md_clock60` (the
 * options screen's "60 HZ GAME SPEED") ticks on every VBlank instead -- the game 20%
 * faster, as an NTSC Amiga ran it.
 *
 * Controls: the engine reads the Amiga joystick registers (src/input.c); md_input_poll
 * writes them from the pads, the same encoding src/input_sdl.c uses.
 */
#include "md_hw.h"
#include "md_game.h"
#include "game_types.h"

extern uint16_t _writeOnly01;
extern volatile uint16_t _lastVBlankServiceTick;
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
volatile uint16_t md_lagTicks;      /* ticks at which the game loop was still busy: a slow frame */
volatile uint16_t md_gameFrames;    /* frames the game loop completed */

/* ---- the frame boundary (game_loop.c waitVBlankSyncPresent) ---- */
void MD_FrameDone(void)
{
    md_render_frame();              /* the finished frame, staged for the swap */
    md_gameFrames++;
    _waitingInterrupt = 1;
    while (_waitingInterrupt != 0) {}
}

/* _Int3TimerHandler (src/interrupts.c, SWOS2.s:5305-5364), one game tick. */
static void game_tick(void)
{
    if (_waitingInterrupt == 0) md_lagTicks++;
    if (_waitingInterrupt != 0) {
        uint16_t delta = (uint16_t)(_currentGameTick - _lastVBlankServiceTick);
        if (delta >= _writeOnly01) {
            int16_t camX, camY;
            _lastVBlankServiceTick = _currentGameTick;
            camX = _cameraX.s.i;
            camY = _cameraY.s.i;
            SwapBuffersAndCameraViewState();
            SetCameraCoordinates(camX, camY);
            md_render_commit();     /* the staged frame goes to the VDP in this VBlank */
            _waitingInterrupt = 0;
            _didSwapThisFrame = 1;
        }
    }
    FadeOutTick();
    FadeInTick();
    md_sound_tick();
    _currentTick++;
    /* The stats overlay freezes the game clock on the Amiga (interrupts.c says why it
     * matters); so does a pause. */
    if (_paused == 0 && _showingUserStats == 0)
        _currentGameTick++;
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
 *   START        space: pause / resume
 *   X            R: the instant replay
 *   Y            S: the statistics (at a stoppage)
 *   Z            Q/W: call your bench (pad 1 the top team's bench key, pad 2 the other)
 *   MODE, or START+A   ESC: leave the match */
static uint16_t pad_key(int port)
{
    uint16_t e = md_pad_edge[port], held = md_pad[port];
    if ((e & PAD_MODE) || ((e & PAD_START) && (held & PAD_A))) return 0x1B;
    if (e & PAD_START) return 0x20;
    if (e & PAD_X) return 0x52;
    if (e & PAD_Y) return 0x53;
    if (e & PAD_Z) return port == 0 ? 0x11 : 0x10;
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
