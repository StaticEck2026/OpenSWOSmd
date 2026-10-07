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
#include <stdlib.h>
#include <string.h>
#include "match_step.h"   /* MatchStep_LocalBenchButton: the DOS second button */
#include "dos_mode.h"     /* _gDosGameplay */

/* ---- Game-side globals (defined in swos_globals.c) ---- */
extern uint8_t  _keyState[128];
extern uint16_t _joy0Dat;
extern uint16_t _joy1Dat;
extern uint8_t  _ciaa_pra;
/* gameState transitions into the ST_STARTING_GAME (21) … ST_PENALTIES (31)
 * range while a match is on screen; everything else (frontend, season-end
 * menus, edit screens, text entry) is outside that window. Used to gate
 * the SPACE multi-role binding so SPACE during a live match falls through
 * to the Amiga highlight-capture path only, not the menu fire/skip path. */
extern uint16_t _gameState;

/* ---- Keyboard FIFO consumed by GetKey() ----
 * The Amiga build's level-2 ISR pushes raw scan codes into keyBuffer
 * and increments keyCount; GetKey() pops one, runs it through
 * keycodeLut[] to get ASCII, and exposes that as currentKey for menu
 * input + InputTextFilterKey. On SDL we have no ISR, so we push
 * directly here from SDL_KEYDOWN. */
extern volatile uint8_t  _keyBuffer[12];
extern volatile uint16_t _keyCount;

/* ---- Amiga raw-scancode table (full set, used by both
 * keyState[] and keyBuffer FIFO). keycodeLut[] in
 * swos_data_bulk_imports.c maps these back to ASCII for GetKey(). */
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

/* ---- Controller buttons -> in-match key functions --------------------------
 * Play uses only TWO pad buttons — A and B for fire, B doubling as skip — plus
 * direction. Everything else the match understands is a KEY: pause, the
 * statistics overlay, the two replay kinds, quit. On a
 * console there is no keyboard at all, so without this none of it is reachable;
 * on a desktop pad it meant reaching for the keyboard mid-match.
 *
 * Pushed as RAW Amiga scancodes into the same _keyBuffer a keypress goes
 * through, so `_keycodeLut` and every consumer downstream are untouched — this
 * adds a producer, not a code path. (That LUT is also why the match compares
 * against 0x50 for pause: 0x50 is ASCII 'P' AFTER the translation, not the raw
 * scancode for P, which is 0x19.)
 *
 * Deliberately NOT mapped: A and B (play), the D-pad and sticks (direction), the
 * shoulder buttons (below), and GUIDE — which is HOME on a Switch and belongs to the system. */
static const struct { int button; uint8_t akey; const char *what; } k_padKeys[] = {
    { SDL_CONTROLLER_BUTTON_START,         0x19, "P   pause"                    },
    { SDL_CONTROLLER_BUTTON_BACK,          0x21, "S   statistics"               },
    { SDL_CONTROLLER_BUTTON_Y,             0x13, "R   instant replay"           },
    { SDL_CONTROLLER_BUTTON_X,             0x25, "H   replay after the match"   },
    /* The shoulder buttons do NOTHING until there is a CONTROLS menu to assign them
     * in (ReportBot 0030, the owner's decision). They used to send UP and DOWN -- the
     * keys that call the two substitutes' benches -- but those are the cursor keys
     * too: they moved a menu's cursor and steered the keyboard's player, by accident,
     * mid-match. The bench is still the game's own way: three taps left or right
     * with the ball out of play, or the second fire button in the PC style. */
    /* Quit is on a stick CLICK on purpose: it ends the match, and a click is
     * not something you hit by accident mid-game the way a face button is. */
    { SDL_CONTROLLER_BUTTON_RIGHTSTICK,    0x45, "ESC quit the match"           },
};

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
/* The arrows as they physically are, beside the four above, which are what the game
 * is told. A keyboard, unlike a stick, can hold LEFT and RIGHT at once -- and does,
 * for a few frames, every time a player rolls from one arrow to the other. DOS SWOS
 * never lets that through: its keyboard handler (SetKeyboardControlFlags, swos.asm)
 * sets a direction and CLEARS ITS OPPOSITE on every press, so the arrow pressed last
 * wins. Passing both on instead let Player1StatusProc's fixed order decide -- Right
 * over Left, Up over Down -- so a roll from RIGHT to LEFT, or from UP to DOWN, kept
 * the player going the old way until the old key came up: about a hundred frames a
 * match of a keyboard player turning late, always mid-turn and in two of the four
 * directions, so worse attacking one way than the other (ReportBot 0014: "ball
 * control ... second half"). And as SWOS 2020 does for its online players ("improved
 * keyboard control", controls.cpp), letting go of the newer arrow with the older one
 * still held goes back to the older one, where DOS would leave the player stopped. */
static int s_kbd_held_up, s_kbd_held_down, s_kbd_held_left, s_kbd_held_right;

static void kbd_arrow(SDL_Keycode k, int down)
{
    switch (k) {
    case SDLK_UP:    s_kbd_held_up    = down; break;
    case SDLK_DOWN:  s_kbd_held_down  = down; break;
    case SDLK_LEFT:  s_kbd_held_left  = down; break;
    case SDLK_RIGHT: s_kbd_held_right = down; break;
    default: return;
    }
    if (down) {
        /* The press wins over its opposite. */
        if (k == SDLK_UP)    { s_kbd_up = 1;    s_kbd_down  = 0; }
        if (k == SDLK_DOWN)  { s_kbd_down = 1;  s_kbd_up    = 0; }
        if (k == SDLK_LEFT)  { s_kbd_left = 1;  s_kbd_right = 0; }
        if (k == SDLK_RIGHT) { s_kbd_right = 1; s_kbd_left  = 0; }
    } else {
        /* The release goes, and an opposite still held takes over. */
        if (k == SDLK_UP)    { s_kbd_up = 0;    s_kbd_down  = s_kbd_held_down; }
        if (k == SDLK_DOWN)  { s_kbd_down = 0;  s_kbd_up    = s_kbd_held_up; }
        if (k == SDLK_LEFT)  { s_kbd_left = 0;  s_kbd_right = s_kbd_held_right; }
        if (k == SDLK_RIGHT) { s_kbd_right = 0; s_kbd_left  = s_kbd_held_left; }
    }
}
/* Fire is bound to THREE keys (Right-Ctrl / Left-Ctrl / Enter) but the Amiga
 * has ONE button, so the three have to behave as one: held while ANY of them
 * is down. A single shared s_kbd_fire flag got that wrong — releasing one key
 * cleared it even with another still held, and since a held key sends no
 * further KEYDOWN, fire stayed off until the player pressed something again.
 * Mid-charge that reads as a release: the fire counter goes positive and the
 * hold becomes a pass (counter <= 4) or an early shot, then nothing at all
 * while the key is still down — i.e. "I'm holding fire and he won't shoot".
 * One bit per key, OR'd. */
#define KBD_FIRE_RCTRL  0x1
#define KBD_FIRE_LCTRL  0x2
#define KBD_FIRE_RETURN 0x4
static int s_kbd_fire_keys;
/* SDL-only "skip auto-advance pause" input. Pressing SPACE (kb) or the
 * second button on a controller sets pauseSkipRequested so dwell loops
 * (e.g. season-end menu chains, future auto-advance pauses) can bail
 * early. Independent of fire so it doesn't interfere with menu select. */
static int s_kbd_skip;
volatile uint16_t _pauseSkipRequested;
/* Set during the season-end / catch-up transition (_Career_AdvanceToNextSeason_
 * UpdateCompetitions). While set there is no live interactive match on screen —
 * used to let SPACE drive the season-skip even though gameState can be left
 * lingering in the match-state range (21..31) after the player's last match. */
extern uint16_t _seasonUpdateInProgress;

/* Every connected controller is opened (up to INPUTSDL_MAX_DEVICES), and two of
 * them are PLAYED: s_player_dev[0] drives player 1, shared with the keyboard, and
 * s_player_dev[1] player 2 exclusively (no keyboard binding for P2 -- matches the
 * Amiga, which always read P2 from physical Port 0). A new device takes the lowest
 * free player, so plugging a single pad always lands on P1, and a device that is
 * left over waits: it steps in when a played one is unplugged, or claims P1 by
 * pressing fire on the intro (InputSDL_SetIntroClaim). That is for a player with
 * more pads connected than players -- three, say, and the one they want is the
 * Amiga joystick on an Arduino Leonardo, which was opened third and so never at all.
 *
 * Each device is either an SDL_GameController (high-level mapped pad) or a raw
 * SDL_Joystick (no mapping in SDL's DB -- we read axes/buttons by index). The raw
 * fallback is what makes generic "Gameport to USB" adapters and other DIY pads work
 * without the user having to author a gamecontrollerdb.txt entry. */
#define INPUTSDL_NUM_PADS    2
#define INPUTSDL_MAX_DEVICES 8
static SDL_GameController *s_dev_gc[INPUTSDL_MAX_DEVICES];
static SDL_Joystick       *s_dev_raw[INPUTSDL_MAX_DEVICES];  /* one of gc[i] / raw[i] is set */
static SDL_JoystickID      s_dev_id[INPUTSDL_MAX_DEVICES];   /* for matching DEVICEREMOVED */
static int                 s_player_dev[INPUTSDL_NUM_PADS] = { -1, -1 };
static int                 s_intro_claim;                     /* the intro is on screen */
static int                 s_frames;                          /* PostFrame calls, for the test knobs */

static int dev_open(int d) { return s_dev_gc[d] || s_dev_raw[d]; }

static const char *dev_name(int d)
{
    const char *nm = s_dev_gc[d] ? SDL_GameControllerName(s_dev_gc[d])
                   : s_dev_raw[d] ? SDL_JoystickName(s_dev_raw[d]) : NULL;
    return nm ? nm : "controller";
}

/* A device nobody plays gets the lowest free player. -1 if both are taken. */
static int dev_assign(int d)
{
    for (int p = 0; p < INPUTSDL_NUM_PADS; ++p) {
        if (s_player_dev[p] == d)
            return p;
    }
    for (int p = 0; p < INPUTSDL_NUM_PADS; ++p) {
        if (s_player_dev[p] < 0) {
            s_player_dev[p] = d;
            return p;
        }
    }
    return -1;
}

/* Push a raw Amiga scancode into the FIFO GetKey() drains, exactly as a
 * keyboard press does: same full-buffer drop, and no repeat handling, since a
 * pad button generates one event per press. */
static void push_amiga_key(uint8_t akey)
{
    if (_keyCount < (sizeof(_keyBuffer) - 3)) {
        _keyBuffer[_keyCount] = akey;
        _keyCount++;
    }
}

/* Open a device and give it a player if one is free. Returns the player (0 or 1),
 * -1 if it is open but waiting for a player, -2 if it could not be opened. */
static int pad_open_into_first_empty(int joystick_index)
{
    for (int i = 0; i < INPUTSDL_MAX_DEVICES; ++i) {
        if (!dev_open(i)) {
            if (SDL_IsGameController(joystick_index)) {
                SDL_GameController *p = SDL_GameControllerOpen(joystick_index);
                if (p) {
                    static int announced;
                    s_dev_gc[i] = p;
                    s_dev_id[i] = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p));
                    /* Print the in-match button map once. On a console there
                     * is no manual to check and no keyboard to fall back on,
                     * so the log is where you find out what the pad does. */
                    if (!announced) {
                        const char *nm = SDL_GameControllerName(p);
                        size_t k;
                        announced = 1;
                        fprintf(stderr, "[input] pad: %s\n", nm ? nm : "controller");
                        fprintf(stderr, "[input]   A / B          -> fire (B also skips)\n");
                        for (k = 0; k < sizeof k_padKeys / sizeof k_padKeys[0]; k++)
                            fprintf(stderr, "[input]   %-14s -> %s\n",
                                    SDL_GameControllerGetStringForButton(
                                        (SDL_GameControllerButton)k_padKeys[k].button),
                                    k_padKeys[k].what);
                    }
                    return dev_assign(i);
                }
            } else {
                /* No GameController mapping — open as a raw joystick.
                 * Standard "two axes + one fire" gameport pads (the
                 * Amiga/Atari shape SWOS expects) fall through here. */
                SDL_Joystick *j = SDL_JoystickOpen(joystick_index);
                if (j) {
                    s_dev_raw[i] = j;
                    s_dev_id[i] = SDL_JoystickInstanceID(j);
                    return dev_assign(i);
                }
            }
            return -2;
        }
    }
    return -2;
}

static int dev_find_instance(SDL_JoystickID id)
{
    for (int i = 0; i < INPUTSDL_MAX_DEVICES; ++i) {
        if (dev_open(i) && s_dev_id[i] == id)
            return i;
    }
    return -1;
}

/* SWOS_TEST_VIRTUAL_PADS=N attaches N virtual gameport sticks (two axes, two
 * buttons, no mapping: the raw path) before the real ones are counted, and
 * SWOS_TEST_VIRTUAL_FIRE=<pad>:<frame> holds fire on virtual pad <pad> (0-based) for
 * ten frames from the <frame>-th -- how the intro's "fire picks player 1" is checked
 * without three controllers on the desk. */
#if SDL_VERSION_ATLEAST(2, 0, 14)
static int s_virt_first = -1, s_virt_count;
static void virtual_pads_attach(void)
{
    const char *v = getenv("SWOS_TEST_VIRTUAL_PADS");
    int n = v ? atoi(v) : 0;
    /* A test runs windowless, and SDL drops joystick input with no window focused. */
    if (n > 0)
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    for (int i = 0; i < n && i < INPUTSDL_MAX_DEVICES; ++i) {
        int idx = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 2, 0);
        if (idx < 0) {
            fprintf(stderr, "[input] test: virtual pad %d not attached: %s\n", i, SDL_GetError());
            break;
        }
        if (s_virt_first < 0)
            s_virt_first = idx;
        s_virt_count++;
    }
}

static void virtual_pads_frame(void)
{
    const char *v = getenv("SWOS_TEST_VIRTUAL_FIRE");
    int pad, at, frame = s_frames;
    if (s_virt_count)
        SDL_JoystickUpdate();       /* a virtual stick's state lands on an update */
    if (!v || s_virt_count == 0 || sscanf(v, "%d:%d", &pad, &at) != 2 || pad < 0 || pad >= s_virt_count)
        return;
    if (frame == at || frame == at + 10) {
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(s_virt_first + pad);
        SDL_Joystick *j = SDL_JoystickFromInstanceID(id);
        if (!j || SDL_JoystickSetVirtualButton(j, 0, frame == at ? SDL_PRESSED : SDL_RELEASED) != 0)
            fprintf(stderr, "[input] test: virtual fire not set (%s)\n", j ? SDL_GetError() : "not open");
        if (frame == at)
            fprintf(stderr, "[input] test: fire on virtual pad %d at frame %d%s\n", pad, frame,
                    s_intro_claim ? " (the intro is on)" : "");
    }
}
#else
static void virtual_pads_attach(void) {}
static void virtual_pads_frame(void) {}
#endif

int InputSDL_Init(void)
{
    memset(_keyState, 0, sizeof(_keyState));
    _joy0Dat = 0;
    _joy1Dat = 0;
    _ciaa_pra = 0xFF;                  /* both fire bits set (idle) */

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

    virtual_pads_attach();

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
        else if (slot == -1)
            fprintf(stderr, "[input]     opened, not played: both players have a pad "
                            "(press fire on it during the intro to play it as P1)\n");
        else
            fprintf(stderr, "[input]     skipped (open failed, or %d devices already open)\n",
                    INPUTSDL_MAX_DEVICES);
    }
    return 0;
}

void InputSDL_Shutdown(void)
{
    for (int i = 0; i < INPUTSDL_MAX_DEVICES; ++i) {
        if (s_dev_gc[i]) {
            SDL_GameControllerClose(s_dev_gc[i]);
            s_dev_gc[i] = NULL;
        }
        if (s_dev_raw[i]) {
            SDL_JoystickClose(s_dev_raw[i]);
            s_dev_raw[i] = NULL;
        }
        s_dev_id[i] = 0;
    }
    s_player_dev[0] = s_player_dev[1] = -1;
}

void InputSDL_HandleEvent(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_KEYDOWN: {
        int a = sdl_to_amiga(e->key.keysym.sym);
        /* Arrow keys drive joy1Dat directly (s_kbd_up/down/left/right
         * below) so we DON'T also push their Amiga scancodes into
         * keyBuffer — `GetKey()` then `keycodeLut` maps the arrow
         * scancodes to 0x10/0x11/0x12/0x13, the same numeric range
         * that `MainKeysCheck` interprets as the bench-call hotkey
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
        /* Alt is dropped from the input layer entirely. The OS owns
         * Alt-Tab and Alt+space (window menu) and these complete in
         * orders SDL can't always linearise — the Alt KEYDOWN reaches
         * us, the game polls keyBuffer / keyState before SDL gets the
         * matching KEYUP, and the loader-intro is skipped + the
         * highlighted menu entry "clicked" on focus return. The fire
         * role is still covered by RCTRL / LCTRL / SPACE / ENTER, and
         * Alt+Enter for fullscreen reads KMOD_ALT off the modifier mask
         * (not our keyState), so it keeps working. */
        int is_alt = (e->key.keysym.sym == SDLK_LALT ||
                      e->key.keysym.sym == SDLK_RALT);
        /* A netplay match: no key reaches the game's own keyboard state at all.
         * keyBuffer feeds GetKey/MainKeysCheck and keyState is polled directly,
         * and either one acted on THIS machine only -- Q called the bench, S the
         * statistics -- so the two games parted (ReportBot 0011). The match's
         * controls do not come from here: InputSDL_LocalMatchControls reads the
         * s_kbd_* flags below and they travel in the shared input. ESC asks the
         * match to end cleanly instead (netplay_match.c). */
        extern int32_t NetplayMatch_Enabled(void);
        extern void    NetplayMatch_RequestLeave(void);
        int in_match = NetplayMatch_Enabled();
        if (in_match && e->key.keysym.sym == SDLK_ESCAPE && e->key.repeat == 0)
            NetplayMatch_RequestLeave();
        if (is_alt || in_match) {
            /* Don't record keyState, don't push to keyBuffer, don't
             * trigger fire. Alt is purely a modifier on this build. */
        } else if (a >= 0 && !is_movement_arrow) {
            _keyState[a] = 1;
            /* Push into the FIFO consumed by GetKey() — but only on
             * the initial press (SDL re-fires KEYDOWN for OS key-repeat,
             * and the Amiga ISR uses its own keyDelay/repeat logic that
             * we don't currently emulate; ignoring repeats prevents
             * runaway double-inserts in InputText). Drop the event when
             * the buffer is full. */
            if (e->key.repeat == 0 && _keyCount < (sizeof(_keyBuffer) - 3)) {
                _keyBuffer[_keyCount] = (uint8_t)a;
                _keyCount++;
            }
        } else if (a >= 0) {
            /* Arrow keys: still record key-down in keyState (some game
             * code polls keyState[AKEY_UP] etc.) but skip the keyBuffer
             * FIFO so GetKey doesn't see them as menu/hotkey input. */
            _keyState[a] = 1;
        }
        switch (e->key.keysym.sym) {
        case SDLK_UP: case SDLK_DOWN: case SDLK_LEFT: case SDLK_RIGHT:
            if (e->key.repeat == 0)
                kbd_arrow(e->key.keysym.sym, 1);
            break;
        case SDLK_RCTRL:  s_kbd_fire_keys |= KBD_FIRE_RCTRL;  s_kbd_fire = 1; break;
        case SDLK_LCTRL:  s_kbd_fire_keys |= KBD_FIRE_LCTRL;  s_kbd_fire = 1; break;
        case SDLK_RETURN: s_kbd_fire_keys |= KBD_FIRE_RETURN; s_kbd_fire = 1; break;
        case SDLK_SPACE:
            /* In a live match SPACE is SWOS's highlight-capture key
             * (game_loop.c MainKeysCheck case 0x20 — sets
             * fadeAndSaveHighlightScene, which fades the screen).
             * Don't route it to fire/skip on the game pad in-match —
             * that would mash the player's fire button at the same
             * time as the highlight capture.
             *
             * Outside a match SPACE drives the season-end auto-skip
             * latch (pauseSkipRequested) ONLY. Fire (Right Ctrl /
             * Left Ctrl / Enter) is intentionally separate so SPACE
             * stays usable as a literal space character in filename /
             * tactics-name text input fields, which read shortFire
             * via the same MenuCheckControls path the menus do.
             *
             * gameState in [ST_STARTING_GAME .. ST_PENALTIES] means
             * a match is on screen — see game_types.h. EXCEPT: gameState
             * is never reset after a match, so it lingers at a post-match
             * value (ST_RESULT_AFTER_THE_GAME=26 / ST_GAME_ENDED=30, both
             * in 21..31) while the career runs the season-end / catch-up
             * playback. That playback is exactly when the user wants to
             * press SPACE to skip — and there is no live match then — so
             * `seasonUpdateInProgress` overrides the gameState gate. */
            if (_seasonUpdateInProgress != 0 ||
                !(_gameState >= 21 /*ST_STARTING_GAME*/ &&
                  _gameState <= 31 /*ST_PENALTIES*/)) {
                s_kbd_skip = 1;
            }
            break;
        default: break;
        }
    } break;
    case SDL_KEYUP: {
        int a = sdl_to_amiga(e->key.keysym.sym);
        int is_alt = (e->key.keysym.sym == SDLK_LALT ||
                      e->key.keysym.sym == SDLK_RALT);
        if (!is_alt && a >= 0) _keyState[a] = 0;
        switch (e->key.keysym.sym) {
        case SDLK_UP: case SDLK_DOWN: case SDLK_LEFT: case SDLK_RIGHT:
            kbd_arrow(e->key.keysym.sym, 0);
            break;
        /* Clear only THIS key's bit — fire stays down while another
         * fire key is still held (see s_kbd_fire_keys). */
        case SDLK_RCTRL:  s_kbd_fire_keys &= ~KBD_FIRE_RCTRL;
                          s_kbd_fire = (s_kbd_fire_keys != 0); break;
        case SDLK_LCTRL:  s_kbd_fire_keys &= ~KBD_FIRE_LCTRL;
                          s_kbd_fire = (s_kbd_fire_keys != 0); break;
        case SDLK_RETURN: s_kbd_fire_keys &= ~KBD_FIRE_RETURN;
                          s_kbd_fire = (s_kbd_fire_keys != 0); break;
        case SDLK_SPACE:  s_kbd_skip = 0; break;
        default: break;
        }
    } break;
    case SDL_CONTROLLERBUTTONDOWN: {
        /* Only for pads SDL has a GameController mapping for — the raw
         * joystick path has no button NAMES to key off, so a gameport adapter
         * keeps working exactly as before and simply has no key buttons. */
        size_t i;
        for (i = 0; i < sizeof k_padKeys / sizeof k_padKeys[0]; i++) {
            if (e->cbutton.button == k_padKeys[i].button) {
                push_amiga_key(k_padKeys[i].akey);
                break;
            }
        }
        break;
    }

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
        int slot;
        /* SDL also announces every device that was already connected when it
         * started, and InputSDL_Init has opened those: opening one again gave it
         * a second slot -- a lone pad drove player 2 as well. */
        if (dev_find_instance(SDL_JoystickGetDeviceInstanceID(idx)) >= 0)
            break;
        slot = pad_open_into_first_empty(idx);
        fprintf(stderr, "[input] hot-plug: \"%s\" (%s) %s\n",
                name ? name : "(null)",
                is_gc ? "GameController" : "raw Joystick",
                slot >= 0 ? (slot == 0 ? "→ P1" : "→ P2")
                          : slot == -1 ? "(not played: both players have a pad)" : "(not opened)");
    } break;
    case SDL_JOYDEVICEREMOVED: {
        int d = dev_find_instance(e->jdevice.which);
        if (d >= 0) {
            if (s_dev_gc[d]) {
                SDL_GameControllerClose(s_dev_gc[d]);
                s_dev_gc[d] = NULL;
            }
            if (s_dev_raw[d]) {
                SDL_JoystickClose(s_dev_raw[d]);
                s_dev_raw[d] = NULL;
            }
            s_dev_id[d] = 0;
            for (int p = 0; p < INPUTSDL_NUM_PADS; ++p) {
                if (s_player_dev[p] != d)
                    continue;
                s_player_dev[p] = -1;
                fprintf(stderr, "[input] pad P%d removed\n", p + 1);
                /* A device that was waiting steps in. */
                for (int w = 0; w < INPUTSDL_MAX_DEVICES; ++w) {
                    if (dev_open(w) && s_player_dev[0] != w && s_player_dev[1] != w) {
                        s_player_dev[p] = w;
                        fprintf(stderr, "[input] P%d is now \"%s\"\n", p + 1, dev_name(w));
                        break;
                    }
                }
            }
        }
    } break;
    default:
        break;
    }
}

/* Read directional + fire state from a single pad slot. `skip` (may be
 * NULL) is set when the conventional "second button" (B on mapped pads,
 * button index 1 on raw joysticks) is pressed — used for the SDL-only
 * "skip auto-advance pause" feature, independent of fire. */
static void read_device_state(int d,
                              int *_up, int *_down, int *left, int *right,
                              int *_fire, int *skip)
{
    SDL_GameController *pad;
    SDL_Joystick *raw;

    if (d < 0 || d >= INPUTSDL_MAX_DEVICES)
        return;
    pad = s_dev_gc[d];
    raw = s_dev_raw[d];

    if (pad) {
        Sint16 ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (ay < -8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    *_up    = 1;
        if (ay >  8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  *_down  = 1;
        if (ax < -8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  *left  = 1;
        if (ax >  8000 || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) *right = 1;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) ||
            SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
            *_fire = 1;
        if (skip && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
            *skip = 1;
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
            if (ay < -8000) *_up    = 1;
            if (ay >  8000) *_down  = 1;
        }
        if (nhats >= 1) {
            Uint8 h = SDL_JoystickGetHat(raw, 0);
            if (h & SDL_HAT_UP)    *_up    = 1;
            if (h & SDL_HAT_DOWN)  *_down  = 1;
            if (h & SDL_HAT_LEFT)  *left  = 1;
            if (h & SDL_HAT_RIGHT) *right = 1;
        }
        for (int b = 0; b < nbtns; ++b) {
            if (SDL_JoystickGetButton(raw, b)) {
                *_fire = 1;
                break;
            }
        }
        /* Button index 1 (the "second" button on classic gameport-shaped
         * pads) also raises skip. Button 0 alone never triggers skip. */
        if (skip && nbtns >= 2 && SDL_JoystickGetButton(raw, 1))
            *skip = 1;
    }
}

/* The same for player `slot` (0 or 1): whichever device that player has. */
static void read_pad_state(int slot,
                           int *_up, int *_down, int *left, int *right,
                           int *_fire, int *skip)
{
    read_device_state(s_player_dev[slot], _up, _down, left, right, _fire, skip);
}

/* While the intro is on screen, a device other than player 1's that presses fire
 * becomes player 1: "the one I press fire on is the one I play with". The one it
 * replaces takes its place -- player 2's pad if it was player 2's, or waits if it
 * was waiting -- so a two-player setup only ever swaps. The same press then skips
 * the intro, as fire always has, because it is now player 1's. */
void InputSDL_SetIntroClaim(int on)
{
    s_intro_claim = on != 0;
    if (getenv("SWOS_TEST_VIRTUAL_PADS"))
        fprintf(stderr, "[input] test: intro %s at frame %d\n", on ? "begins" : "ends", s_frames);
}

static void intro_claim_poll(void)
{
    for (int d = 0; d < INPUTSDL_MAX_DEVICES; ++d) {
        int up = 0, down = 0, left = 0, right = 0, fire = 0;
        int old = s_player_dev[0];
        if (!dev_open(d) || d == old)
            continue;
        read_device_state(d, &up, &down, &left, &right, &fire, NULL);
        if (!fire)
            continue;
        if (s_player_dev[1] == d)
            s_player_dev[1] = old;
        s_player_dev[0] = d;
        fprintf(stderr, "[input] fire on the intro: P1 is now controller %d, \"%s\"", d + 1, dev_name(d));
        if (old >= 0)
            fprintf(stderr, " (was controller %d, now %s)", old + 1, s_player_dev[1] == old ? "P2" : "not played");
        fprintf(stderr, "\n");
        return;
    }
}

/* Encode an up/down/left/right + fire state into Paula's JOY*DAT word.
 *
 * JOY*DAT uses 2-bit Gray-code quadrature per axis. SWOS decodes it as
 * (input.c:Player1StatusProc / Player2StatusProc):
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

/* Drop every kind of "key/button is currently down" state. Called on
 * SDL_WINDOWEVENT_FOCUS_LOST so an Alt-Tab away doesn't leave Alt
 * stuck pressed, which would (a) skip the intro on focus return and
 * (b) instantly fire the currently-highlighted menu entry as a "click".
 *
 * The Alt-Tab sequence on Windows takes focus away before SDL gets a
 * KEYUP for the Alt key, so without this clear our keyState[AKEY_LALT]
 * (and the keyBuffer FIFO entry that SDL_KEYDOWN pushed) stay set
 * indefinitely. The very first thing the menu/loader-intro does on
 * focus return is poll those, and acts as if the user was holding the
 * fire/select button down. */
void InputSDL_ClearKeyState(void)
{
    memset(_keyState, 0, sizeof(_keyState));
    memset((void *)_keyBuffer, 0, sizeof(_keyBuffer));
    _keyCount = 0;
    s_kbd_up = s_kbd_down = s_kbd_left = s_kbd_right = s_kbd_fire = 0;
    s_kbd_held_up = s_kbd_held_down = s_kbd_held_left = s_kbd_held_right = 0;
    s_kbd_fire_keys = 0;
    s_kbd_skip = 0;
    _pauseSkipRequested = 0;
    /* joy0Dat / joy1Dat / ciaa_pra are recomputed each frame by
     * InputSDL_PostFrame from the cleared s_kbd_* + read_pad_state, so
     * they don't need explicit clearing here. */
}

/* Everything released: the keyboard state, and the joystick and fire registers the
 * game reads (joy0Dat, joy1Dat, CIAA port A).
 *
 * For a live netplay match, called as it starts. InputSDL_PostFrame stops writing
 * the registers the moment a netplay match is enabled -- the synced inputs own them
 * from the first frame -- but the pre-match screens run BEFORE that frame and read
 * them, and they would still hold whatever was down when the match was started. The
 * player who pressed START with fire started it with fire down, so the stadium
 * screen's wait for fire to be released (InitGameSaveTeams) waited forever. Released
 * is also the one state both peers can be sure to agree on. */
void InputSDL_ReleaseAll(void)
{
    InputSDL_ClearKeyState();
    _joy0Dat = 0;
    _joy1Dat = 0;
    _ciaa_pra |= (uint8_t)((1u << 7) | (1u << 6));     /* active low: both fire buttons up */
}

/* The DOS SECOND FIRE BUTTON (pad X), which opens the substitutes menu of the team this
 * player controls -- swos.asm routes it through TeamGeneralInfo.secondaryFire and
 * BenchCheckControls enters the menu on it. The AMIGA HAS NO SECOND BUTTON, so every
 * caller checks _gDosGameplay first and the Amiga input path is byte-for-byte unchanged.
 *
 * X, not B: A and B are both already fire (B is also the skip button), and a button that
 * shoots and opens the bench at once is not a second button. A keyboard player keeps the
 * 'W' scancode, which is the port's bench key in both modes. */
static int pad_secondary(int slot)
{
    int d;
    if (slot < 0 || slot >= INPUTSDL_NUM_PADS)
        return 0;
    d = s_player_dev[slot];
    if (d >= 0 && s_dev_gc[d])
        return SDL_GameControllerGetButton(s_dev_gc[d], SDL_CONTROLLER_BUTTON_X) ? 1 : 0;
    return 0;
}

/* This machine's own controls, as MATCH_* bits, for a netplay match.
 *
 * A netplay peer always drives the LOCAL half from player 1's keyboard and
 * pad 0 -- which side of the pitch that is belongs to the session, not to the
 * hardware, so this deliberately does not care. The other half arrives over the
 * wire.
 *
 * The KEY is not sampled. A match input carries one (MatchPlayerInput.key) and
 * the recordings use it for the bench and the in-match hotkeys, but R, H, P, ESC
 * and S all LEAVE the match loop, which a session cannot roll back
 * (docs/NETPLAY.md section 5). Sending no key at all is how they stay out,
 * rather than filtering them one at a time and missing one. */
uint8_t InputSDL_LocalMatchControls(void)
{
    int up = s_kbd_up, down = s_kbd_down, left = s_kbd_left, right = s_kbd_right;
    int fire = s_kbd_fire, skip = 0;
    uint8_t bits = 0;

    read_pad_state(0, &up, &down, &left, &right, &fire, &skip);

    if (up)    bits |= 0x01;    /* MATCH_UP    */
    if (down)  bits |= 0x02;    /* MATCH_DOWN  */
    if (left)  bits |= 0x04;    /* MATCH_LEFT  */
    if (right) bits |= 0x08;    /* MATCH_RIGHT */
    if (fire)  bits |= 0x20;    /* MATCH_FIRE  */
    /* DOS's second button, on the wire as MATCH_BENCH -- the same bit 6 DOS reads it
     * from. The frame step turns it into callBench for the team this player controls, so
     * a netplay peer needs nothing else. Amiga mode never sets it. */
    if (_gDosGameplay && pad_secondary(0))
        bits |= 0x40;          /* MATCH_BENCH */
    return bits;
}

void InputSDL_PostFrame(void)
{
    /* The replay rig writes joy0Dat/joy1Dat/ciaa_pra itself, once per game
     * loop iteration. Recomputing them from the real keyboard/pads here
     * would overwrite the recorded stick with whatever is physically held. */
    { extern int ReplayTest_Running(void); if (ReplayTest_Running()) return; }

    /* A netplay match owns them for the same reason, and more sharply: they
     * carry the SYNCED inputs of both players, so recomputing them from this
     * machine's pad would replace the opponent's input with nothing and make
     * every re-simulated frame differ from the first. */
    { extern int32_t NetplayMatch_Enabled(void); if (NetplayMatch_Enabled()) return; }

    s_frames++;
    virtual_pads_frame();
    if (s_intro_claim)
        intro_claim_poll();

    /* A KEYBOARD AND ONE PAD ARE TWO PLAYERS. Player 1 is the keyboard plus the first
     * pad, player 2 a second pad -- so with one pad there is no player 2, and a local
     * two-player friendly cannot start: the second team's PLAY MATCH screen listens to
     * player 2 alone (PlayMatchMenuInit clears player 1's input by playMatchControlFlags,
     * as SWOS2.s does), and neither CTRL nor the stick's fire -- both player 1's -- can
     * press it (a tester, 2026-09-29). So when both teams are human here (_twoPlayers)
     * and player 2 has no pad of their own, player 1's pad goes to player 2 and the
     * keyboard stays player 1's. One player, two pads, or an online match (which reads
     * InputSDL_LocalMatchControls, not these registers): as before. */
    extern uint16_t _twoPlayers;
    extern int32_t  NetplayMatch_Enabled(void);
    int split = _twoPlayers != 0 && s_player_dev[1] < 0 && s_player_dev[0] >= 0 &&
                !NetplayMatch_Enabled();
    {
        static int s_wasSplit;
        if (split != s_wasSplit) {
            s_wasSplit = split;
            if (split)
                fprintf(stderr, "[input] two players, one pad: \"%s\" is player 2, the keyboard "
                                "player 1\n", dev_name(s_player_dev[0]));
            else
                fprintf(stderr, "[input] the pad is player 1's again\n");
        }
    }

    /* Player 1: keyboard + pad 0 (combined), or the keyboard alone when split. */
    int p1_up = s_kbd_up, p1_down = s_kbd_down, p1_left = s_kbd_left, p1_right = s_kbd_right;
    int p1_fire = s_kbd_fire;
    int skip = s_kbd_skip;
    if (!split)
        read_pad_state(0, &p1_up, &p1_down, &p1_left, &p1_right, &p1_fire, &skip);

    /* Player 2: pad 1 only -- or pad 0 when split. No keyboard binding — keeping the
     * keyboard single-player avoids hijacking arrow / fire keys when a P2 pad is
     * plugged in but unused. */
    int p2_up = 0, p2_down = 0, p2_left = 0, p2_right = 0, p2_fire = 0;
    read_pad_state(split ? 0 : 1, &p2_up, &p2_down, &p2_left, &p2_right, &p2_fire, &skip);

    _pauseSkipRequested = (uint16_t)(skip ? 1 : 0);

    /* SWOS reads P1 from JOY1DAT (Amiga Port 1) and P2 from JOY0DAT
     * (Port 0). The port numbering is reversed from the player number —
     * see input.c Player1StatusProc/Player2StatusProc. */
    _joy1Dat = encode_joydat(p1_up, p1_down, p1_left, p1_right);
    _joy0Dat = encode_joydat(p2_up, p2_down, p2_left, p2_right);

    /* CIAA Port A fire bits (active low):
     *   bit 7 = Port 0 fire = Player 1 fire
     *   bit 6 = Port 1 fire = Player 2 fire
     * Clear bit to signal press; set to signal release. Other bits
     * stay at their default (1). */
    if (p1_fire) _ciaa_pra &= (uint8_t)~(1u << 7);
    else         _ciaa_pra |= (uint8_t)(1u << 7);
    if (p2_fire) _ciaa_pra &= (uint8_t)~(1u << 6);
    else         _ciaa_pra |= (uint8_t)(1u << 6);

    /* DOS's second button in LOCAL play. The registers above carry what the Amiga has;
     * this is the one input DOS has and the Amiga does not, so it goes in beside them
     * rather than through them, and only in DOS mode. */
    if (_gDosGameplay) {
        MatchStep_LocalBenchButton(1, split ? 0 : pad_secondary(0));
        MatchStep_LocalBenchButton(2, pad_secondary(split ? 0 : 1));
    }
}
