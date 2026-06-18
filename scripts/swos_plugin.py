#!/usr/bin/env python3
"""SWOS-specific PLUGIN for the gdb test harness.

This is the game-knowledge layer on top of the generic harness_lib.GdbHarness
(itself on top of gdb_proxy.py). Everything SWOS-specific lives here: the Menu
struct accessors, the menu-input injection point + direction encoding, the
framebuffer/palette geometry, and career save loading. Lifting the harness to
another game means swapping THIS file, not the framework. See docs/HARNESS.md.

Driving works on BOTH targets (SDL native + Amiga FS-UAE) because the menu
system is shared C and the FS-UAE stub is now write-capable (see
docs/HARNESS.md "FS-UAE write support"). The injection point is a single line in
the shared _MenuProc, so one mechanism drives both.
"""
import re
import subprocess
import time
from harness_lib import GdbHarness, _write_png

# _MenuProc(), right after _MenuCheckControls() (line 1565) sets the input
# globals and BEFORE the d7/onSelect dispatch (1575+) and the nav read (1650).
# Setting the input globals here replicates real input for the whole frame:
# fire feeds the onSelect d7 bitmask; menuControlsDirection feeds the nav.
INJECT_LINE = "menu_system.c:1567"

# menuControlsDirection encoding: (dir & 6) >> 1 indexes nextEntryDirectionOffsets.
# Verified against the live table [2,1,3,0] + neighbor links.
DIR = {"up": 0, "right": 2, "down": 4, "left": 6}
DIR_GLOBAL = {"up": "up", "right": "right", "down": "down", "left": "left"}

MENU = "((Menu *)currentMenu)->selectedEntry"
# Scratch global to stage a filename string (visible on both builds).
FILENAME_SCRATCH = "menuSongData"

# Framebuffer geometry. SDL is 8bpp chunky (stride == width). The Amiga build
# is interleaved bitplanes (src/include/display.h): each scanline holds
# NUM_PLANES planes of PLANE_STRIDE bytes; pixel (x,y) bit p lives at
# base + y*PLANE_STRIDE*NUM_PLANES + p*PLANE_STRIDE + x/8, bit 7-(x%8).
SCREEN_W, SCREEN_H, SCREEN_STRIDE = 336, 272, 336
AMIGA_PLANE_STRIDE = 44


class SwosHarness:
    def __init__(self, proxy=None, timeout=8):
        self.h = GdbHarness(proxy, timeout) if proxy else GdbHarness(timeout=timeout)
        self._is_sdl = None

    # ---- target detection --------------------------------------------------
    def is_sdl(self):
        """SDL build exposes the SDL palette symbol; the Amiga build does not."""
        if self._is_sdl is None:
            r = self.h.cmd("print &s_palette")
            self._is_sdl = ("No symbol" not in r and "0x" in r)
        return self._is_sdl

    # ---- being at the injection point -------------------------------------
    def prime(self, timeout=120):
        """Ensure a breakpoint at the inject line and stop the target on it."""
        if "menu_system.c:1567" not in self.h.cmd("info breakpoints"):
            self.h.break_at(INJECT_LINE)
        self.h.cmd("continue", timeout=3)
        self.h.waitstop(timeout)

    def ensure_stopped(self):
        if not self.h.is_stopped():
            self.prime()

    # ---- menu state (read-only) -------------------------------------------
    def ordinal(self):
        return self.h.int_value(f"{MENU}->ordinal")

    def num_entries(self):
        return self.h.int_value("((Menu *)currentMenu)->numEntries")

    def action(self):
        return self.h.symbol(f"{MENU}->onSelect")

    def entry_action(self, i):
        return self.h.symbol(f"((MenuEntry *)_CalcMenuEntryAddress({i}))->onSelect")

    def where(self):
        return {"ordinal": self.ordinal(), "action": self.action(),
                "num_entries": self.num_entries()}

    def enum(self):
        n = self.num_entries() or 0
        return [(i, self.entry_action(i)) for i in range(n)]

    # ---- input injection (drives SDL + Amiga) -----------------------------
    def nav(self, direction):
        """One directional step. direction in up/down/left/right."""
        self.prime()
        self.h.set_var("menuControlsDirection", DIR[direction])
        self.h.set_var(DIR_GLOBAL[direction], 1)
        self.h.set_var("fire", 0)
        self.h.cont(20)            # consume this frame; lands back on inject bp
        return self.where()

    def fire(self):
        """Press fire on the current entry (activates its onSelect). Sets BOTH
        fire (d7 bit 0) and shortFire (bit 5): most menu *selections* (CONTINUE
        buttons, etc.) gate on shortFire via the entry's controlMask, not fire."""
        self.prime()
        self.h.set_var("fire", 1)
        self.h.set_var("shortFire", 1)
        self.h.cont(20)
        return self.where()

    def combo(self, controls):
        """Hold several controls for one frame (e.g. ['fire','left'] -> OTHER
        nationality). controls = subset of fire/up/down/left/right."""
        self.prime()
        if "fire" in controls:
            self.h.set_var("fire", 1)
            self.h.set_var("shortFire", 1)
        for d in ("up", "down", "left", "right"):
            if d in controls:
                self.h.set_var(DIR_GLOBAL[d], 1)
                self.h.set_var("menuControlsDirection", DIR[d])
        self.h.cont(20)
        return self.where()

    # ---- direct selection (no path-finding; uses a memory write) ----------
    def goto(self, symbol):
        """Move selection to the entry whose onSelect == symbol (no fire)."""
        self.ensure_stopped()
        n = self.num_entries() or 0
        target = next((i for i in range(n) if self.entry_action(i) == symbol), None)
        if target is None:
            return None
        self.h.cmd(f"call (void)_SetCurrentEntry({target})")
        return target if self.action() == symbol else None

    def select(self, symbol):
        """goto(symbol) then fire."""
        if self.goto(symbol) is None:
            return False
        self.fire()
        return True

    # ---- screen capture (orientation only) --------------------------------
    def is_aga(self):
        """AGA build = 8 planes / 256 colours; OCS = 4 / 16. bitPlane4Base is
        an AGA-only symbol (#ifdef AGA in display.c)."""
        r = self.h.cmd("print &bitPlane4Base")
        return ("No symbol" not in r and "0x" in r)

    def screencap(self, out_png):
        if self.is_sdl():
            return self.h.screencap(out_png, "frontbufferBaseAltPtr",
                                    SCREEN_W, SCREEN_H, SCREEN_STRIDE,
                                    palette_expr="&s_palette[0]", pal_fmt="rgba")
        return self._screencap_amiga(out_png)

    def _screencap_amiga(self, out_png):
        """Decode the Amiga interleaved-bitplane display -> PNG. Handles AGA
        (8 planes/256 col) and OCS (4 planes/16 col). Menu use case: camera
        offsets are 0, so the displayed base is the flag-selected frontbuffer."""
        planes = 8 if self.is_aga() else 4
        ncolors = 256 if planes == 8 else 16
        w, h, ps = SCREEN_W, SCREEN_H, AMIGA_PLANE_STRIDE
        row_stride = ps * planes
        # Displayed base (matches _UpdatePlanePtrsAndBlitParams selection).
        if self.h.int_value("usePlaneOffsetFlag"):
            base = "frontbufferMaskBuffer"
        elif self.h.int_value("useAltBaseFlag"):
            base = "frontbufferBaseAltPtr"
        else:
            base = "frontbufferBasePtr"
        fb = self.h.dump_mem(base, row_stride * h, "/tmp/harness_amiga_fb.bin")
        # gamePalette: big-endian uint16, 12-bit 0x0RGB -> 8-bit RGB (nibble*0x11)
        praw = self.h.dump_mem("&gamePalette[0]", ncolors * 2,
                               "/tmp/harness_amiga_pal.bin")
        pal = []
        for i in range(ncolors):
            c = (praw[i * 2] << 8) | praw[i * 2 + 1]
            pal.append((((c >> 8) & 0xF) * 0x11, ((c >> 4) & 0xF) * 0x11,
                        (c & 0xF) * 0x11))
        rows = []
        for y in range(h):
            rb = y * row_stride
            row = bytearray()
            for x in range(w):
                col, bit = x >> 3, 7 - (x & 7)
                idx = 0
                for p in range(planes):
                    idx |= ((fb[rb + p * ps + col] >> bit) & 1) << p
                row += bytes(pal[idx])
            rows.append(row)
        _write_png(out_png, w, h, rows)
        nz = sum(1 for b in fb if b)
        return (f"wrote {out_png} {w}x{h} (Amiga {planes}-plane/{ncolors}col, "
                f"base={base}, {nz} non-zero bytes)")

    # ---- season-loop driver -----------------------------------------------
    # Per-screen INPUT MODEL (reverse-engineered 2026-06-17; see docs/HARNESS.md
    # "Season loop"). The career advances through THREE kinds of screen, each
    # taking a DIFFERENT input. Using the wrong one is exactly what made the
    # driver appear to "wedge" — it was NEVER gdb / multithread instability:
    #
    #  1. Season TRANSITION (seasonUpdateInProgress != 0): the cup/league result
    #     displays are AUTO-ADVANCE menus — _DIYCupInit pre-selects entry-66
    #     (controlMask 0xFFFF; see _Career_AutoAdvanceCupMatches). Setting
    #     s_seasonSkipLatched=1 drains the WHOLE chain at once; the latch persists
    #     until seasonUpdateInProgress drops to 0. DO NOT move the selection
    #     (that clobbers entry-66 and the modal sticks — the original bug). The
    #     ONLY screen in the chain that needs a keypress is the chairman memo
    #     (_ChairmanMemosInit, a plain modal): shortFire on its Continue button.
    #     The transition ends at the season increment (_InitializeNewSeason).
    #
    #     The OLD driver free-ran straight to the increment line — which hangs
    #     forever at the chairman memo (a modal waiting for input). That hang,
    #     mistaken for a gdb/thread wedge, is the whole reason this rewrite
    #     exists. Skip-latch + a conditional memo breakpoint = no free-run into
    #     an input-waiting screen, no wedge, threads untouched.
    #
    #  2. In-season menus (memo / contest hub / match-day list): shortFire.
    #
    #  3. The pre-match SQUAD screen (_PlayMatchMenuInit): _PlayMatchSelected
    #     gates on the RAW joystick fire pl1Fire (NOT the menu shortFire). Set
    #     pl1Fire=1 to start/sim the fixture (CPU-vs-CPU -> instant result).
    SEASON_INCR_LINE = "career_init.c:974"
    MEMO_INIT = "_ChairmanMemosInit"
    SQUAD_INIT = "_PlayMatchMenuInit"
    HUB_INIT = "_CareerOverInit"         # season-end hub (CONTINUE CAREER)
    HUB_CONTINUE = "_ContinueCareer"     # the CONTINUE CAREER entry's onSelect

    def _current_init(self):
        """onInit symbol of the active menu (identifies the screen). ONE read,
        O(1) — reliable, unlike currentMenuPtr (which shifts in sub-menus)."""
        return self.h.symbol("((Menu *)currentMenu)->onInit")

    def at_contest_menu(self):
        """True at the season-end hub (onInit == _CareerOverInit)."""
        return self._current_init() == self.HUB_INIT

    # ---- interrupt fallback (only for an UNEXPECTED input-waiter) ----------
    def _gdb_pid(self):
        r = subprocess.run(["pgrep", "-f", "gdb .*bin/SWOS"],
                           capture_output=True, text=True)
        return r.stdout.split()[0] if r.stdout.strip() else None

    def _interrupt(self):
        """Stop a running inferior after a cont timed out (it hit a screen the
        driver did not anticipate). kill -INT on the gdb process is the only
        method that works through the daemon pipe — stdin Ctrl-C does not (no
        tty). A SAFETY NET for discovery; the proven transition path never needs
        it. Returns True if the inferior is stopped afterward."""
        pid = self._gdb_pid()
        if pid:
            subprocess.run(["kill", "-INT", pid])
            time.sleep(1.5)
        return self.h.is_stopped()

    # ---- the season transition (skip-latch; NO free-run into a modal) ------
    def drive_transition(self, max_stops=80, timeout=14):
        """Drive ONE season transition to the next season's increment. Assumes
        CONTINUE CAREER was just fired at the hub (or seasonUpdateInProgress is
        already set). Returns the new currentSeasonShowing, or None if it stuck
        on an unanticipated input-waiter (call stack printed to stdout).

        Mechanism (see the class comment): s_seasonSkipLatched drains the
        cup/league auto-advance displays; a CONDITIONAL breakpoint catches only
        chairman-memo frames (shortFire dismisses them); a code breakpoint at the
        season increment ends the drive. No blind free-run, so no wedge — this is
        the proven-reliable core (season 3->4 verified, health OK throughout,
        threads untouched)."""
        h = self.h
        start = h.int_value("currentSeasonShowing")
        h.set_var("s_seasonSkipLatched", 1)
        h.delete_breakpoints()
        h.break_at(self.SEASON_INCR_LINE)
        h.cmd(f'break {INJECT_LINE} '
              f'if ((Menu*)currentMenu)->onInit == {self.MEMO_INIT}')
        for _ in range(max_stops):
            h.set_var("s_seasonSkipLatched", 1)    # keep latched across screens
            h.cont(timeout)
            if not h.is_stopped():                  # unanticipated waiter
                self._interrupt()
                print("drive_transition: stuck (unknown waiter)\n" + h.cmd("bt"))
                return None
            s = h.int_value("currentSeasonShowing")
            if s is not None and s != start:
                return s                            # hit the season increment
            if self._current_init() == self.MEMO_INIT:
                h.set_var("shortFire", 1)           # dismiss the chairman memo
        return None

    # ---- the in-season fixtures (sim each via pl1Fire on the squad) --------
    def drive_inseason(self, max_steps=800, timeout=20, stuck_limit=30):
        """From just after a season increment, drive the new season to the next
        season-end hub (_CareerOverInit). Returns True if the hub was reached.

        How the season self-drives: shortFire on the default selection advances
        the menus AND the cup/league match-day state machines — _DIYCupInit /
        _DIYLeagueTournamentInit pre-select the correct entry at each state (play
        the human's fixture, auto-advance an AI fixture, or exit when the round
        is done, e.g. _DIYCupOnSelect10 at mState=6/roundComplete=1). The ONLY
        screen needing different input is the pre-match squad (_PlayMatchMenuInit):
        _PlayMatchSelected gates on the RAW joystick fire pl1Fire, not the menu
        shortFire. When the season's fixtures are exhausted (seasonFinished=1),
        the contest hub's PLAY MATCH runs _SeasonFinished -> careerOverMenu.

        A season legitimately bounces match-day <-> squad and cycles a cup round
        across several _DIYCupInit states, so 'stuck' is keyed on (screen,
        selected-ordinal) staying IDENTICAL for stuck_limit steps — genuine
        progress always changes one of them, while a true freeze repeats. A full
        season is many fixtures, so this can take several minutes (intended for
        unattended verification runs)."""
        h = self.h
        h.delete_breakpoints()
        h.break_at(INJECT_LINE)
        last_key, same = None, 0
        for _ in range(max_steps):
            h.cont(timeout)
            if not h.is_stopped():                  # a match/menu over-ran timeout
                self._interrupt()
                return False
            init = self._current_init()
            if init == self.HUB_INIT:
                return True
            key = (init, self.ordinal())
            same = same + 1 if key == last_key else 0
            last_key = key
            if same >= stuck_limit:
                print(f"drive_inseason: stuck at {init} ord={key[1]}")
                return False
            if init == self.SQUAD_INIT:
                h.set_var("pl1Fire", 1)             # sim the human fixture
            h.set_var("shortFire", 1)              # advance menu / state machine
        return self.at_contest_menu()

    def advance_one_season(self):
        """From a season-end hub: fire CONTINUE -> drive the transition to the
        season increment -> drive the new season's fixtures to the next hub.
        Returns the new currentSeasonShowing (at the increment), or None."""
        if not self.at_contest_menu():
            return None
        # fire CONTINUE CAREER (selections gate on shortFire, bit 5)
        self.goto(self.HUB_CONTINUE)
        self.h.set_var("shortFire", 1)
        s = self.drive_transition()
        if s is None:
            return None
        self.drive_inseason()
        return s

    def run_seasons(self, n):
        """Advance up to n full seasons. Returns the list of season numbers
        reached (None marks a season that stuck). Drives to a season-end hub
        first."""
        self.prime()
        if not self.at_contest_menu():
            self.drive_inseason()
        out = []
        for _ in range(n):
            s = self.advance_one_season()
            out.append(s)
            if s is None or s == 19:   # 19 = retirement
                break
        return out

    # ---- career save loading (game's own native load; no UI nav needed) ---
    def load_career(self, filename):
        """Stage the filename into a scratch global and call the game's loader.
        Works on both targets now that memory + register writes are supported."""
        self.ensure_stopped()
        self.h.write_bytes(FILENAME_SCRATCH, filename.encode() + b"\x00")
        r = self.h.cmd(
            f"call (int)SaveJson_LoadCareer((unsigned char *){FILENAME_SCRATCH}, 0, 0)",
            timeout=60)
        m = re.search(r"\$\d+\s*=\s*(-?\d+)", r)
        return int(m.group(1)) if m else None
