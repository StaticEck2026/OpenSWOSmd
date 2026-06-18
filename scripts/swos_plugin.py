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
    # _InitializeNewSeason does `currentSeasonShowing++` here. A CODE breakpoint
    # at this line is the stable way to stop after a season transition: the game
    # free-runs the result chain + catch-up sim at native speed and stops here.
    # (Per-frame menu-bp stepping is too slow for the catch-up sim, and
    # watchpoints / Ctrl-C interrupts wedge the multithreaded SDL inferior.)
    SEASON_INCR_LINE = "career_init.c:974"
    CONTEST_CONTINUE_ORDINAL = 6   # CONTINUE CAREER entry in the contest menu
    # In-season match-day RESULT handlers = INSTANT SIMULATE (vs the MATCH
    # handlers _DIYCupOnSelect3 / _DIYLeagueTournamentMatch = play the real
    # game). The CPU computes the score; fastest way through a season.
    # See project_career_play_vs_sim.
    CUP_RESULT_HANDLER = "_DIYCupOnSelect5"
    LEAGUE_RESULT_HANDLER = "_DIYLeagueTournamentResult"

    def _actions(self):
        return [a for _, a in self.enum() if a]

    def at_contest_menu(self):
        """True at the season-end contest hub (exposes CONTINUE CAREER)."""
        return "_ContinueCareer" in self._actions()

    def _fire_entry(self, symbol, timeout=12):
        """Select the entry whose onSelect == symbol and press it (shortFire),
        no prime() overhead. Returns False if the symbol isn't on this menu."""
        if self.goto(symbol) is None:
            return False
        self.h.set_var("shortFire", 1)
        self.h.cont(timeout)
        return True

    def sim_one_step(self):
        """One in-season step toward season end, simulating matches via RESULT.
        Returns 'hub' at the season-end contest menu, else the action taken."""
        acts = self._actions()
        if "_ContinueCareer" in acts:
            return "hub"
        if self.CUP_RESULT_HANDLER in acts:        # cup match-day: RESULT = sim
            self._fire_entry(self.CUP_RESULT_HANDLER); return "cup-result"
        if self.LEAGUE_RESULT_HANDLER in acts:     # league match-day: RESULT = sim
            self._fire_entry(self.LEAGUE_RESULT_HANDLER); return "league-result"
        if "_PlayMatchSelected" in acts:           # team-select -> match-day overview
            self._fire_entry("_ExitPlayMatch"); return "exit-playmatch"
        # chairman memo / cup view / other interstitial -> advance
        self.h.set_var("shortFire", 1); self.h.cont(8); return "shortfire"

    def sim_through_season(self, max_steps=200):
        """Simulate every human match-day (RESULT) until the season-end hub.

        WIP: the individual mechanisms are validated (cup RESULT sims + advances;
        _ExitPlayMatch -> match-day overview; contest-hub detection), but a full
        season hasn't been driven to completion yet — sim_one_step calls enum()
        which queries EVERY entry's onSelect (O(entries) daemon round-trips on
        the 68-entry match-day screen), so a season is thousands of round-trips
        (minutes) and the long run tends to wedge the multithreaded SDL daemon.
        TODO: detect the screen by currentMenu identity (==&diyCupMenu /
        &contestMenu / ...) and fire the known RESULT ordinal directly — O(1) per
        step instead of O(entries)."""
        for _ in range(max_steps):
            if self.sim_one_step() == "hub":
                return True
        return self.at_contest_menu()

    def advance_one_season(self):
        """From the season-end contest hub: CONTINUE -> transition (free-run to
        the season increment) -> sim the new season's matches -> next hub.
        Returns the new currentSeasonShowing (at the next season-end hub).

        The transition uses the only stable stop on the multithreaded SDL
        inferior: free-run to the SEASON_INCR_LINE code breakpoint. The in-season
        matches are simulated via RESULT (sim_through_season)."""
        # fire CONTINUE CAREER (selections gate on shortFire, bit 5)
        self.h.cmd(f"call (void)_SetCurrentEntry({self.CONTEST_CONTINUE_ORDINAL})")
        self.h.set_var("shortFire", 1)
        # free-run the transition (result chain + catch-up sim) to the increment
        self.h.delete_breakpoints(); self.h.break_at(self.SEASON_INCR_LINE); self.h.cont(60)
        # step past it back into the menu loop, then sim the new season's matches
        self.h.delete_breakpoints(); self.h.break_at(INJECT_LINE); self.h.cont(20)
        self.sim_through_season()
        return self.h.int_value("currentSeasonShowing")

    def run_seasons(self, n):
        """Advance n full seasons (all matches simulated via RESULT). Returns the
        list of season numbers reached. Drives to a season-end hub first."""
        self.prime()
        if not self.at_contest_menu():
            self.sim_through_season()
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
