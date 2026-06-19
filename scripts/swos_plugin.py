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
        """SDL build exposes the SDL palette symbol; the Amiga build does not.
        The result is CACHED only on a CONFIDENT answer. A timed-out/empty
        response (possible when the daemon is under load) is retried, never
        cached — caching a false 'Amiga' here misroutes screencap() to the
        Amiga bitplane decoder (the 'dump gamePalette' wrong-path bug)."""
        if self._is_sdl is None:
            for _ in range(3):
                r = self.h.cmd("print &s_palette", timeout=15)
                if "No symbol" in r:
                    self._is_sdl = False
                    break
                if "0x" in r:
                    self._is_sdl = True
                    break
                # ambiguous (timeout/partial) — retry rather than cache wrong
            if self._is_sdl is None:
                self._is_sdl = True   # this plugin's primary target is SDL
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
        buttons, etc.) gate on shortFire via the entry's controlMask, not fire.

        The press is a SINGLE-FRAME PULSE: fire/shortFire are cleared after the
        frame. A *held* fire mis-drives a nested modal entered by the press —
        e.g. firing CAREER opens careerMenu (a _ShowMenu do/while loop), and a
        still-set fire makes careerMenu immediately re-fire/exit its default
        entry on the next frame (the careerMenu 'bounce/hang' bug)."""
        self.prime()
        self.h.set_var("fire", 1)
        self.h.set_var("shortFire", 1)
        self.h.cont(20)
        self.h.set_var("fire", 0)
        self.h.set_var("shortFire", 0)
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

    # ---- label-driven selection (for dynamic lists) -----------------------
    # Some screens reuse ONE onSelect across many entries whose meaning is the
    # displayed STRING, not the callback — the career team picker (continents,
    # then countries, then divisions, then teams all share _SelectTeamsCommon /
    # _SelectLeaguesCommon / _ChooseTeamsFriendlyCommon2). goto(symbol) can't
    # disambiguate those, so navigate by the entry's TEXT instead. A string
    # entry (content == ENTRY_STRING == 2) keeps its text pointer in `fg`.
    def entry_text(self, ord):
        """The visible label of entry `ord`, or None if it's invisible / not a
        string entry."""
        e = f"((MenuEntry*)_CalcMenuEntryAddress({ord}))"
        if self.h.int_value(f"{e}->isInvisible"):
            return None
        if self.h.int_value(f"{e}->content") != 2:   # ENTRY_STRING
            return None
        m = re.search(r'"((?:[^"\\]|\\.)*)"', self.h.cmd(f"x/s (char*)({e}->fg)"))
        return m.group(1) if m else None

    def scan_text(self):
        """[(ordinal, label)] for every VISIBLE string entry of the current
        menu — the reliable way to 'see' a dynamic list without a screenshot."""
        n = self.num_entries() or 0
        out = []
        for i in range(n + 1):
            t = self.entry_text(i)
            if t is not None:
                out.append((i, t))
        return out

    def find_text(self, label):
        """Ordinal of the visible entry whose label == `label` (case-insensitive,
        also matches on a leading-substring so 'NOTT' finds "NOTT'M FOREST"), or
        None."""
        want = label.upper()
        cands = self.scan_text()
        for i, t in cands:
            if t.upper() == want:
                return i
        for i, t in cands:
            if t.upper().startswith(want):
                return i
        return None

    def fire_ordinal(self, ordinal, timeout=20):
        """Select `ordinal` by memory write, then a single-frame fire pulse.
        Use a long timeout on the press that triggers _InitCareer (~30-60s)."""
        self.h.cmd(f"call (void)_SetCurrentEntry({ordinal})")
        self.h.set_var("fire", 1)
        self.h.set_var("shortFire", 1)
        for v in ("up", "down", "left", "right"):
            self.h.set_var(v, 0)
        self.h.cont(timeout)
        self.h.set_var("fire", 0)
        self.h.set_var("shortFire", 0)

    def fire_text(self, label, timeout=20):
        """find_text(label) then fire_ordinal. Returns the ordinal, or None."""
        ordn = self.find_text(label)
        if ordn is None:
            return None
        self.fire_ordinal(ordn, timeout)
        return ordn

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

    def screencap_oriented(self, out_png):
        """Screenshot with the selection cursor pinned BRIGHT and blink-text
        visible, so the image reliably shows which entry is selected (the cursor
        normally pulses kColorTableShine={2,2,1,1,7,7,1,1} via menuCursorFrame and
        is faint on some frames). menuCursorFrame=8 -> after the frame's ++ -> 9
        -> index (9&14)>>1=4 -> colour 7 (brightest). One input-free frame is
        consumed to re-render with the pinned values. Use this to ORIENT — see
        docs/HARNESS.md 'Navigating by screenshot'."""
        self.prime()
        self.h.set_var("menuCursorFrame", 8)
        self.h.set_var("menuTextBlinkCounter", 0)
        for v in ("fire", "shortFire", "up", "down", "left", "right"):
            self.h.set_var(v, 0)
        self.h.cont(20)            # re-render one frame with cursor pinned bright
        return self.screencap(out_png)

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

    # ---- cold boot + fresh-career start (the NORMAL flow) -----------------
    def boot(self, timeout=180):
        """Cold-boot the inferior to the MAIN MENU. Assumes the daemon is up in
        SDL mode with a HEADLESS env (SDL_VIDEODRIVER=dummy /
        SDL_AUDIODRIVER=dummy) + the season-loop flags
        (SWOS_TEST_NO_INJURIES_CARDS=1, SWOS_SAVE_FORMAT_JSON_VERIFY=0) — see
        docs/HARNESS.md 'Reliability'. Sets the inject breakpoint, (re)starts
        the inferior with `run`, and waits for the first _MenuProc stop (the
        main menu). `set confirm off` + `kill` make a re-boot in the SAME daemon
        non-interactive (no 'restart from beginning?' prompt to hang on).
        Returns the active menu's onInit ('_SWOSMainMenuInit' on success)."""
        self.h.cmd("set confirm off")
        self.h.cmd("kill")                     # no-op if nothing is running
        if INJECT_LINE not in self.h.cmd("info breakpoints"):
            self.h.break_at(INJECT_LINE)
        self.h.cmd("run", timeout=4)           # boot is slow; catch the stop below
        self.h.waitstop(timeout)
        return self._current_init()

    # Default fresh-career drill-down through the team picker, BY LABEL:
    #   continent -> country -> league division -> club.
    FRESH_CAREER_PATH = ("EUROPE", "ENGLAND", "PREMIER LEAGUE", "ARSENAL")

    def start_fresh_career(self, path=None, init_timeout=120):
        """Start a brand-new career through the NORMAL menu flow (NO
        _LoadLiverpool). Call boot() first; this must run at the MAIN MENU. It
        drives the real path verified 2026-06-19:
            CAREER -> CHOOSE TEAM AND START CAREER
              -> <continent> -> <country> -> <division> -> <club>   (path, by label)
              -> _InitCareer(teamId) -> dismiss chairman memo(s) -> contest hub.
        The auto-generated manager name + default nationality are kept (a fresh
        career needs no text typed). Returns the hub onInit ('_ContestMenuInit')
        on success, else None (printing the labels it actually saw).

        Why label-driven and not goto(onSelect): the picker reuses ONE callback
        across every continent/country/division/club; only the displayed STRING
        identifies a row. Matching by label (case-insensitive, leading-substring
        fallback) also survives data edits that renumber ordinals — exactly what
        we want when testing the data-jailbreak changes. See docs/HARNESS.md
        'Starting a fresh career'."""
        path = path or self.FRESH_CAREER_PATH
        self.prime()
        if self._current_init() != "_SWOSMainMenuInit":
            print(f"start_fresh_career: not at main menu (at {self._current_init()})")
            return None
        if not self.select("_CareerMenu"):
            print("start_fresh_career: CAREER entry not found")
            return None
        # CHOOSE TEAM AND START CAREER opens the team picker (_SelectTeamsInit)
        if self.goto("_ChooseTeamAndStartCareer") is None:
            print("start_fresh_career: CHOOSE TEAM entry not found")
            return None
        self.fire()
        # Drill the picker by label; the LAST hop fires the club, which runs
        # _InitCareer (slow — give it init_timeout, not the 20s frame budget).
        for n, label in enumerate(path):
            last = (n == len(path) - 1)
            ordn = self.find_text(label)
            if ordn is None:
                print(f"start_fresh_career: '{label}' not found; visible: "
                      f"{[t for _, t in self.scan_text()]}")
                return None
            self.fire_ordinal(ordn, timeout=init_timeout if last else 20)
        # After the club press, _InitCareer runs then stops at the chairman
        # memo's inject bp; ensure stopped, then dismiss the welcome memo(s).
        if not self.h.is_stopped():
            self.h.waitstop(init_timeout)
        for _ in range(6):
            if self._current_init() != self.MEMO_INIT:
                break
            self.h.set_var("shortFire", 1)
            self.h.cont(20)
            self.h.set_var("shortFire", 0)
        return self._current_init()

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
    @staticmethod
    def _save_basename(filename):
        """The game's resolve_save_path() (swos_io.c) strips a 'drive:' prefix
        then UNCONDITIONALLY prepends 'save/'. So a caller passing 'save/X.CAR'
        gets the doubled 'save/save/X.CAR' -> fopen fails -> the load silently
        no-ops (a failure long masked by warm in-memory career state). Pass the
        bare basename; strip a leading 'save/' if a caller still includes it."""
        f = filename.replace("\\", "/")
        if f.lower().startswith("save/"):
            f = f[5:]
        return f

    # NOTE on timeouts: the inferior CALL of SaveJson_Load/WriteCareer parses/
    # emits JSON via mjson. On SDL that's a few seconds; on the EMULATED Amiga
    # (FS-UAE, cycle-exact 68020) it's 1-3 MINUTES. The daemon cmd returns as
    # soon as the call completes, so a high ceiling is safe on both — but it must
    # exceed the Amiga parse time or the proxy disconnects mid-call and wedges
    # the gdb client (looks like corruption; it's a timeout artifact). 300s.
    CALL_TIMEOUT = 300

    def _stage_filename(self, filename):
        """Stage a save basename into the scratch global as ONE bulk write
        (`set {char[N]}scratch = "..."`) — robust on the Amiga stub, where the
        per-byte write_bytes path (N separate M packets) is slow/flaky. Returns
        the staged basename."""
        name = self._save_basename(filename)
        n = len(name) + 1  # + NUL
        self.h.cmd(f'set {{char[{n}]}}{FILENAME_SCRATCH} = "{name}"')
        return name

    def load_career(self, filename):
        """Stage the basename into a scratch global and call the game's loader.
        Returns 0 on success (game convention: non-zero == failure). NOTE: this
        only loads DATA; to enter the career UI from a cold boot call
        enter_career() afterwards (the game's _LoadCareerFile path runs
        _StartContest, which load_career does not)."""
        self.ensure_stopped()
        self._stage_filename(filename)
        r = self.h.cmd(
            f"call (int)SaveJson_LoadCareer((unsigned char *){FILENAME_SCRATCH}, 0, 0)",
            timeout=self.CALL_TIMEOUT)
        m = re.search(r"\$\d+\s*=\s*(-?\d+)", r)
        return int(m.group(1)) if m else None

    def save_career(self, filename, timeout=None):
        """Write the live career to a .CAR via the game's SaveJson_WriteCareer.
        Pass a basename ('X.CAR'); resolve_save_path prepends 'save/'. Returns
        the writer's rc (0 == success)."""
        self.ensure_stopped()
        self._stage_filename(filename)
        r = self.h.cmd(
            f"call (int)SaveJson_WriteCareer((unsigned char *){FILENAME_SCRATCH})",
            timeout=timeout or self.CALL_TIMEOUT)
        m = re.search(r"\$\d+\s*=\s*(-?\d+)", r)
        return int(m.group(1)) if m else None

    def enter_career(self):
        """Enter the loaded career's UI from a cold boot. The game's load path
        runs _StartContest() -> _ShowMenu(contestMenu) (modal). Calling it with
        the inject breakpoint set makes the inferior call break INSIDE the now-
        active contest menu (the correct nested context the whole career runs
        in). Returns the active menu's onInit symbol afterwards."""
        if INJECT_LINE not in self.h.cmd("info breakpoints"):
            self.h.break_at(INJECT_LINE)
        self.h.cmd("call (void)_StartContest()", timeout=40)
        return self._current_init()
