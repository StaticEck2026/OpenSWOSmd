/*
 * input_sdl.c -- SDL keyboard / gamepad → Amiga input state.
 *
 * The Amiga code reads input through three globals that the original
 * hardware kept live:
 *
 *   keyState[128]   populated by Int2KeyboardHandler from CIAA SDR.
 *                   Indexed by Amiga raw scancode.
 *   joy0Dat         Paula JOY0DAT register snapshot: directional bits
 *                   for player 1 stick.
 *   joy1Dat         Same for player 2.
 *   ciaa_pra        CIAA Port A. Bit 7 (active low) is the Port-0 fire
 *                   button -- clear = pressed, set = released.
 *
 * On SDL we own all four as software globals (swos_globals.c) and write
 * them from this file based on SDL keyboard + game-controller events.
 *
 * Keyboard bindings (PC key → Amiga equivalent):
 *   Arrow keys        player-1 joystick direction + keyState
 *   Right-Alt / Right-Ctrl  player-1 fire (FS-UAE default for menu select)
 *   Left-Ctrl / Space / Enter  also fire (legacy convenience bindings)
 *   Enter             keyState[AKEY_ENTER] (menus also accept it)
 *   Escape            quit (handled in platform_sdl.c)
 */

#include "platform_sdl.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

/* ---- Game-side globals (defined in swos_globals.c) ---- */
extern uint8_t  keyState[128];
extern uint16_t joy0Dat;
extern uint16_t joy1Dat;
extern uint8_t  ciaa_pra;

/* ---- Keyboard FIFO consumed by _GetKey() ----
 * The Amiga build's level-2 ISR pushes raw scan codes into keyBuffer
 * and increments keyCount; _GetKey() pops one, runs it through
 * keycodeLut[] to get ASCII, and exposes that as currentKey for menu
 * input + _InputTextFilterKey. On SDL we have no ISR, so we push
 * directly here from SDL_KEYDOWN. */
extern volatile uint8_t  keyBuffer[12];
extern volatile uint16_t keyCount;

/* ---- Amiga raw-scancode table (full set, used by both
 * keyState[] and keyBuffer FIFO). keycodeLut[] in
 * swos_data_bulk_imports.c maps these back to ASCII for _GetKey(). */
#define AKEY_BACKTICK 0x00
#define AKEY_1        0x01
#define AKEY_0        0x0A
#define AKEY_MINUS    0x0B
#define AKEY_EQUALS   0x0C
#define AKEY_BACKSL   0x0D
#define AKEY_Q        0x10
#define AKEY_LBRACK   0x1A
#define AKEY_RBRACK   0x1B
#define AKEY_A        0x20
#define AKEY_SEMI     0x29
#define AKEY_QUOTE    0x2A
#define AKEY_Z        0x31
#define AKEY_COMMA    0x38
#define AKEY_PERIOD   0x39
#define AKEY_SLASH    0x3A
#define AKEY_SPACE    0x40
#define AKEY_BSPACE   0x41
#define AKEY_TAB      0x42
#define AKEY_ENTER    0x44
#define AKEY_ESC      0x45
#define AKEY_DEL      0x46
#define AKEY_UP       0x4C
#define AKEY_DOWN     0x4D
#define AKEY_RIGHT    0x4E
#define AKEY_LEFT     0x4F
#define AKEY_F1       0x50
#define AKEY_F10      0x59
#define AKEY_LSHIFT   0x60
#define AKEY_RSHIFT   0x61
#define AKEY_LCTRL    0x63
#define AKEY_LALT     0x64
#define AKEY_RALT     0x65

static int sdl_to_amiga(SDL_Keycode k)
{
    /* Letters: SDLK_a..SDLK_z are contiguous (0x61..0x7A), and the
     * Amiga A-Z scancodes lay out in QWERTY rows, so use a small LUT. */
    if (k >= SDLK_a && k <= SDLK_z) {
        static const uint8_t letter_to_amiga[26] = {
            /* a */ AKEY_A,        /* b */ 0x35,           /* c */ 0x33,
            /* d */ 0x22,          /* e */ 0x12,           /* f */ 0x23,
            /* g */ 0x24,          /* h */ 0x25,           /* i */ 0x17,
            /* j */ 0x26,          /* k */ 0x27,           /* l */ 0x28,
            /* m */ 0x37,          /* n */ 0x36,           /* o */ 0x18,
            /* p */ 0x19,          /* q */ AKEY_Q,         /* r */ 0x13,
            /* s */ 0x21,          /* t */ 0x14,           /* u */ 0x16,
            /* v */ 0x34,          /* w */ 0x11,           /* x */ 0x32,
            /* y */ 0x15,          /* z */ AKEY_Z
        };
        return letter_to_amiga[k - SDLK_a];
    }
    /* Top-row digits 0..9. Amiga has 1..9 then 0. */
    if (k >= SDLK_1 && k <= SDLK_9) return AKEY_1 + (k - SDLK_1);
    if (k == SDLK_0)                return AKEY_0;

    switch (k) {
    case SDLK_ESCAPE:       return AKEY_ESC;
    case SDLK_RETURN:       return AKEY_ENTER;
    case SDLK_KP_ENTER:     return AKEY_ENTER;
    case SDLK_SPACE:        return AKEY_SPACE;
    case SDLK_BACKSPACE:    return AKEY_BSPACE;
    case SDLK_TAB:          return AKEY_TAB;
    case SDLK_DELETE:       return AKEY_DEL;
    case SDLK_UP:           return AKEY_UP;
    case SDLK_DOWN:         return AKEY_DOWN;
    case SDLK_LEFT:         return AKEY_LEFT;
    case SDLK_RIGHT:        return AKEY_RIGHT;
    case SDLK_F1:           return AKEY_F1;
    case SDLK_F2:           return AKEY_F1 + 1;
    case SDLK_F3:           return AKEY_F1 + 2;
    case SDLK_F4:           return AKEY_F1 + 3;
    case SDLK_F5:           return AKEY_F1 + 4;
    case SDLK_F6:           return AKEY_F1 + 5;
    case SDLK_F7:           return AKEY_F1 + 6;
    case SDLK_F8:           return AKEY_F1 + 7;
    case SDLK_F9:           return AKEY_F1 + 8;
    case SDLK_F10:          return AKEY_F10;
    case SDLK_MINUS:        return AKEY_MINUS;
    case SDLK_EQUALS:       return AKEY_EQUALS;
    case SDLK_BACKSLASH:    return AKEY_BACKSL;
    case SDLK_LEFTBRACKET:  return AKEY_LBRACK;
    case SDLK_RIGHTBRACKET: return AKEY_RBRACK;
    case SDLK_SEMICOLON:    return AKEY_SEMI;
    case SDLK_QUOTE:        return AKEY_QUOTE;
    case SDLK_COMMA:        return AKEY_COMMA;
    case SDLK_PERIOD:       return AKEY_PERIOD;
    case SDLK_SLASH:        return AKEY_SLASH;
    case SDLK_BACKQUOTE:    return AKEY_BACKTICK;
    case SDLK_LSHIFT:       return AKEY_LSHIFT;
    case SDLK_RSHIFT:       return AKEY_RSHIFT;
    case SDLK_LCTRL:        return AKEY_LCTRL;
    case SDLK_LALT:         return AKEY_LALT;
    case SDLK_RALT:         return AKEY_RALT;
    default:                return -1;
    }
}

/* ---- Tracked keyboard state for joystick/fire synthesis ---- */
static int s_kbd_up, s_kbd_down, s_kbd_left, s_kbd_right, s_kbd_fire;

/* Up to two game controllers, mapped to players 1 and 2 respectively.
 * Pad 0 is shared with the keyboard for player 1; pad 1 drives player 2
 * exclusively (no keyboard binding for P2 — matches the Amiga which
 * always read P2 from physical Port 0). New pads fill the lowest empty
 * slot, so plugging a single pad always lands on P1.
 *
 * Each slot can be either an SDL_GameController (high-level mapped pad)
 * or a raw SDL_Joystick (no mapping in SDL's DB — we read axes/buttons
 * by index). The raw fallback is what makes generic "Gameport to USB"
 * adapters and other DIY pads work without the user having to author a
 * gamecontrollerdb.txt entry. */
#define INPUTSDL_NUM_PADS 2
static SDL_GameController *s_pads[INPUTSDL_NUM_PADS];
static SDL_Joystick       *s_raw_pads[INPUTSDL_NUM_PADS];   /* one of pads[i] / raw_pads[i] is set */
static SDL_JoystickID      s_pad_instance_id[INPUTSDL_NUM_PADS]; /* for matching DEVICEREMOVED */

static int pad_open_into_first_empty(int joystick_index)
{
    for (int i = 0; i < INPUTSDL_NUM_PADS; ++i) {
        if (!s_pads[i] && !s_raw_pads[i]) {
            if (SDL_IsGameController(joystick_index)) {
                SDL_GameController *p = SDL_GameControllerOpen(joystick_index);
                if (p) {
                    s_pads[i] = p;
                    s_pad_instance_id[i] = SDL_JoystickInstanceID(
                        SDL_GameControllerGetJoystick(p));
                    return i;
                }
            } else {
                /* No GameController mapping — open as a raw joystick.
                 * Standard "two axes + one fire" gameport pads (the
                 * Amiga/Atari shape SWOS expects) fall through here. */
                SDL_Joystick *j = SDL_JoystickOpen(joystick_index);
                if (j) {
                    s_raw_pads[i] = j;
                    s_pad_instance_id[i] = SDL_JoystickInstanceID(j);
                    return i;
                }
            }
            return -1;
        }
    }
    return -1;
}

static int pad_find_slot_for_instance(SDL_JoystickID id)
{
    for (int i = 0; i < INPUTSDL_NUM_PADS; ++i) {
        if ((s_pads[i] || s_raw_pads[i]) && s_pad_instance_id[i] == id)
            return i;
    }
    return -1;
}

int InputSDL_Init(void)
{
    memset(keyState, 0, sizeof(keyState));
    joy0Dat = 0;
    joy1Dat = 0;
    ciaa_pra = 0xFF;                  /* both fire bits set (idle) */

    /* Load extra controller mappings if the user dropped a
     * gamecontrollerdb.txt next to the binary. Without a matching
     * mapping, SDL_IsGameController() returns false for the device and
     * we silently skip it — that's the most common reason a pad
     * "isn't working". The file is the community-maintained mapping
     * DB (https://github.com/gabomdq/SDL_GameControllerDB). Built-in
     * SDL mappings cover popular pads (Xbox, PS4/5, Switch Pro, most
     * Logitechs); the file is for everything else. */
    {
        int loaded = SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
        if (loaded > 0)
            fprintf(stderr, "[input] loaded %d controller mappings from gamecontrollerdb.txt\n",
                    loaded);
        else if (loaded < 0)
            /* missing file is fine; SDL only sets the error on parse failure */
            fprintf(stderr, "[input] gamecontrollerdb.txt not loaded: %s\n", SDL_GetError());
    }

    int n = SDL_NumJoysticks();
    fprintf(stderr, "[input] SDL_NumJoysticks=%d\n", n);
    for (int i = 0; i < n; ++i) {
        const char *name = SDL_JoystickNameForIndex(i);
        int is_gc = SDL_IsGameController(i);
        fprintf(stderr, "[input]   joystick %d: \"%s\" %s\n",
                i, name ? name : "(null)",
                is_gc ? "→ game controller" : "→ raw joystick (no mapping)");
        int slot = pad_open_into_first_empty(i);
        if (slot >= 0)
            fprintf(stderr, "[input]     opened as P%d (%s)\n",
                    slot + 1, is_gc ? "GameController" : "raw Joystick");
        else
            fprintf(stderr, "[input]     skipped (both pad slots full or open failed)\n");
    }
    return 0;
}

void InputSDL_Shutdown(void)
{
    for (int i = 0; i < INPUTSDL_NUM_PADS; ++i) {
        if (s_pads[i]) {
            SDL_GameControllerClose(s_pads[i]);
            s_pads[i] = NULL;
        }
        if (s_raw_pads[i]) {
            SDL_JoystickClose(s_raw_pads[i]);
            s_raw_pads[i] = NULL;
        }
        s_pad_instance_id[i] = 0;
    }
}

void InputSDL_HandleEvent(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_KEYDOWN: {
        int a = sdl_to_amiga(e->key.keysym.sym);
        /* Arrow keys drive joy1Dat directly (s_kbd_up/down/left/right
         * below) so we DON'T also push their Amiga scancodes into
         * keyBuffer — `_GetKey()` then `keycodeLut` maps the arrow
         * scancodes to 0x10/0x11/0x12/0x13, the same numeric range
         * that `_MainKeysCheck` interprets as the bench-call hotkey
         * (0x10 = bench2, 0x11 = bench1) and that menu nav uses for
         * up/right/down/left. On Amiga the player used a joystick
         * in-game so arrow scancodes never fired during play; on
         * SDL we double-purpose the same physical keys. Without
         * this filter, holding DOWN arrow to control the keeper
         * fires bench1 every time the keypress lands in keyBuffer.
         * Menu navigation still works because the joystick path
         * drives menuControlsDirection from pl1Direction. */
        int is_movement_arrow = (e->key.keysym.sym == SDLK_UP   ||
                                  e->key.keysym.sym == SDLK_DOWN ||
                                  e->key.keysym.sym == SDLK_LEFT ||
                                  e->key.keysym.sym == SDLK_RIGHT);
        if (a >= 0 && !is_movement_arrow) {
            keyState[a] = 1;
            /* Push into the FIFO consumed by _GetKey() — but only on
             * the initial press (SDL re-fires KEYDOWN for OS key-repeat,
             * and the Amiga ISR uses its own keyDelay/repeat logic that
             * we don't currently emulate; ignoring repeats prevents
             * runaway double-inserts in InputText). Drop the event when
             * the buffer is full. */
            if (e->key.repeat == 0 && keyCount < (sizeof(keyBuffer) - 3)) {
                keyBuffer[keyCount] = (uint8_t)a;
                keyCount++;
            }
        } else if (a >= 0) {
            /* Arrow keys: still record key-down in keyState (some game
             * code polls keyState[AKEY_UP] etc.) but skip the keyBuffer
             * FIFO so _GetKey doesn't see them as menu/hotkey input. */
            keyState[a] = 1;
        }
        switch (e->key.keysym.sym) {
        case SDLK_UP:    s_kbd_up    = 1; break;
        case SDLK_DOWN:  s_kbd_down  = 1; break;
        case SDLK_LEFT:  s_kbd_left  = 1; break;
        case SDLK_RIGHT: s_kbd_right = 1; break;
        case SDLK_RALT:
        case SDLK_RCTRL:
        case SDLK_SPACE:
        case SDLK_LCTRL:
        case SDLK_RETURN: s_kbd_fire = 1; break;
        default: break;
        }
    } break;
    case SDL_KEYUP: {
        int a = sdl_to_amiga(e->key.keysym.sym);
        if (a >= 0) keyState[a] = 0;
        switch (e->key.keysym.sym) {
        case SDLK_UP:    s_kbd_up    = 0; break;
        case SDLK_DOWN:  s_kbd_down  = 0; break;
        case SDLK_LEFT:  s_kbd_left  = 0; break;
        case SDLK_RIGHT: s_kbd_right = 0; break;
        case SDLK_RALT:
        case SDLK_RCTRL:
        case SDLK_SPACE:
        case SDLK_LCTRL:
        case SDLK_RETURN: s_kbd_fire = 0; break;
        default: break;
        }
    } break;
    case SDL_JOYDEVICEADDED: {
        /* SDL fires JOYDEVICEADDED for every device. CONTROLLERDEVICEADDED
         * fires *additionally* for devices that have a GameController
         * mapping. We do all our opening here so a single code path
         * handles both mapped and unmapped pads — the
         * pad_open_into_first_empty helper picks the right SDL API
         * based on SDL_IsGameController. Skipping CONTROLLERDEVICEADDED
         * avoids opening the same device twice. */
        int idx = e->jdevice.which;
        const char *name = SDL_JoystickNameForIndex(idx);
        int is_gc = SDL_IsGameController(idx);
        int slot = pad_open_into_first_empty(idx);
        fprintf(stderr, "[input] hot-plug: \"%s\" (%s) %s\n",
                name ? name : "(null)",
                is_gc ? "GameController" : "raw Joystick",
                slot >= 0 ? (slot == 0 ? "→ P1" : "→ P2") : "(no free slot)");
    } break;
    case SDL_JOYDEVICEREMOVED: {
        int slot = pad_find_slot_for_instance(e->jdevice.which);
        if (slot >= 0) {
            if (s_pads[slot]) {
                SDL_GameControllerClose(s_pads[slot]);
                s_pads[slot] = NULL;
            }
            if (s_raw_pads[slot]) {
                SDL_JoystickClose(s_raw_pads[slot]);
                s_raw_pads[slot] = NULL;
            }
            s_pad_instance_id[slot] = 0;
            fprintf(stderr, "[input] pad P%d removed\n", slot + 1);
        }
    } break;
    default:
        break;
    }
}

/* Read directional + fire state from a single pad slot. */
static void read_pad_state(int slot,
                           int *up, int *down, int *left, int *right, int *fire)
{
    SDL_GameController *pad = s_pads[slot];
    SDL_Joystick *raw = s_raw_pads[slot];

    if (pad) {
        Sint16 ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (ay < -8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    *up    = 1;
        if (ay >  8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  *down  = 1;
        if (ax < -8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  *left  = 1;
        if (ax >  8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) *right = 1;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) ||
            SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
            *fire = 1;
        return;
    }

    if (raw) {
        /* Raw joystick path: no GameController mapping in SDL's DB.
         * The classic Amiga / gameport-shaped pad has 2 axes + 1+ fire
         * buttons. Read axes 0 (X) and 1 (Y) and treat any pressed
         * button as fire — keeps generic gameport-to-USB adapters
         * working without a hand-authored mapping. Also probe a hat
         * (POV) if the device has one (some converters expose
         * direction as a hat instead of axes). */
        int naxes  = SDL_JoystickNumAxes(raw);
        int nbtns  = SDL_JoystickNumButtons(raw);
        int nhats  = SDL_JoystickNumHats(raw);
        if (naxes >= 2) {
            Sint16 ax = SDL_JoystickGetAxis(raw, 0);
            Sint16 ay = SDL_JoystickGetAxis(raw, 1);
            if (ax < -8000) *left  = 1;
            if (ax >  8000) *right = 1;
            if (ay < -8000) *up    = 1;
            if (ay >  8000) *down  = 1;
        }
        if (nhats >= 1) {
            Uint8 h = SDL_JoystickGetHat(raw, 0);
            if (h & SDL_HAT_UP)    *up    = 1;
            if (h & SDL_HAT_DOWN)  *down  = 1;
            if (h & SDL_HAT_LEFT)  *left  = 1;
            if (h & SDL_HAT_RIGHT) *right = 1;
        }
        for (int b = 0; b < nbtns; ++b) {
            if (SDL_JoystickGetButton(raw, b)) {
                *fire = 1;
                break;
            }
        }
    }
}

/* Encode an up/down/left/right + fire state into Paula's JOY*DAT word.
 *
 * JOY*DAT uses 2-bit Gray-code quadrature per axis. SWOS decodes it as
 * (input.c:_Player1StatusProc / _Player2StatusProc):
 *   right = bit 1
 *   left  = bit 9
 *   down  = bit 0 XOR bit 1
 *   up    = bit 8 XOR bit 9
 *
 * Correct per-axis encoding:
 *   horizontal axis pair bits 1 (high) + 0 (low):
 *     bit 1 = right_pressed
 *     bit 0 = bit 1 XOR down_pressed
 *   vertical axis pair bits 9 (high) + 8 (low):
 *     bit 9 = left_pressed
 *     bit 8 = bit 9 XOR up_pressed
 *
 * Verification of all 9 cases (none + 4 cardinals + 4 diagonals):
 *   none:        v = 0x000  ->  R=0 L=0 D=0^0=0 U=0^0=0  ✓
 *   right:       v = 0x003  ->  R=1 L=0 D=1^1=0 U=0^0=0  ✓
 *   left:        v = 0x300  ->  R=0 L=1 D=0^0=0 U=1^1=0  ✓
 *   up:          v = 0x100  ->  R=0 L=0 D=0    U=0^1=1  ✓
 *   down:        v = 0x001  ->  R=0 L=0 D=1^0=1 U=0^0=0  ✓
 *   right+down:  v = 0x002  ->  R=1 L=0 D=0^1=1 U=0      ✓
 *   right+up:    v = 0x103  ->  R=1 L=0 D=1^1=0 U=0^1=1  ✓
 *   left+down:   v = 0x301  ->  R=0 L=1 D=0^1=1 U=1^1=0  ✓
 *   left+up:     v = 0x200  ->  R=0 L=1 D=0^0=0 U=1^1=1  ✓
 */
static uint16_t encode_joydat(int up, int down, int left, int right)
{
    uint16_t v = 0;
    if (right)                  v |= (1u << 1);
    if (right ^ (down ? 1 : 0)) v |= (1u << 0);
    if (left)                   v |= (1u << 9);
    if (left  ^ (up   ? 1 : 0)) v |= (1u << 8);
    return v;
}

void InputSDL_PostFrame(void)
{
    /* Player 1: keyboard + pad 0 (combined). */
    int p1_up = s_kbd_up, p1_down = s_kbd_down, p1_left = s_kbd_left, p1_right = s_kbd_right;
    int p1_fire = s_kbd_fire;
    read_pad_state(0, &p1_up, &p1_down, &p1_left, &p1_right, &p1_fire);

    /* Player 2: pad 1 only. No keyboard binding — keeping the keyboard
     * single-player avoids hijacking arrow / fire keys when a P2 pad is
     * plugged in but unused. */
    int p2_up = 0, p2_down = 0, p2_left = 0, p2_right = 0, p2_fire = 0;
    read_pad_state(1, &p2_up, &p2_down, &p2_left, &p2_right, &p2_fire);

    /* SWOS reads P1 from JOY1DAT (Amiga Port 1) and P2 from JOY0DAT
     * (Port 0). The port numbering is reversed from the player number —
     * see input.c _Player1StatusProc/_Player2StatusProc. */
    joy1Dat = encode_joydat(p1_up, p1_down, p1_left, p1_right);
    joy0Dat = encode_joydat(p2_up, p2_down, p2_left, p2_right);

    /* CIAA Port A fire bits (active low):
     *   bit 7 = Port 0 fire = Player 1 fire
     *   bit 6 = Port 1 fire = Player 2 fire
     * Clear bit to signal press; set to signal release. Other bits
     * stay at their default (1). */
    if (p1_fire) ciaa_pra &= (uint8_t)~(1u << 7);
    else         ciaa_pra |= (uint8_t)(1u << 7);
    if (p2_fire) ciaa_pra &= (uint8_t)~(1u << 6);
    else         ciaa_pra |= (uint8_t)(1u << 6);
}
