#!/usr/bin/env python3
"""SWOS gdb-driven test harness — drive menus + read state over the gdb_proxy
daemon, WITHOUT screenshots (fast + token-light). Screenshots (swos_screencap.py)
are for orientation only; navigation feedback comes from reading the menu state
internally.

Mechanism (SDL; Amiga is analogous once the daemon is --mode amiga):
  - state:  ((Menu *)currentMenu)->selectedEntry gives the live selection;
            ->ordinal identifies it, ->onSelect resolves to the action symbol
            (e.g. <_CareerMenu>) — gdb prints the function name.
  - input:  set the InputSDL keyboard-state statics s_kbd_up/down/left/right/fire;
            InputSDL_PostFrame folds them into joy1Dat/ciaa each frame, which
            _Player1StatusProc decodes (pl1Direction) and _MenuCheckControls
            consumes. A clean tap = release (clear the edge) -> press -> release,
            frame-stepped on a breakpoint at _MenuCheckControls (once per menu
            frame). Verified: down moved ordinal 6->7.

The game must already be AT A MENU (so _MenuCheckControls runs). Commands:
  where                  print selectedEntry ordinal + action symbol + numEntries
  nav up|down|left|right one directional tap; prints where afterwards
  fire                   select the current entry (one fire tap)
  cap OUT.png            screenshot (orientation only; shells to swos_screencap.py)

All ops leave the target stopped at the _MenuCheckControls breakpoint.
"""
import argparse
import os
import re
import subprocess
import sys

PROXY = "/mnt/c/Projects/AI/SWOS/SWOS_C/gdb_proxy.py"
HERE = os.path.dirname(os.path.abspath(__file__))
MENU = "((Menu *)currentMenu)->selectedEntry"


def gdb(command, timeout=6):
    r = subprocess.run(["python3", PROXY, "cmd", command, "--timeout", str(timeout)],
                       capture_output=True, text=True)
    return r.stdout + r.stderr


def gset(var, val):
    gdb(f"set variable {var} = {val}")


def ensure_menu_bp():
    """Set a breakpoint on _MenuCheckControls (once) for per-menu-frame stepping."""
    info = gdb("info breakpoints")
    if "_MenuCheckControls" not in info:
        gdb("break _MenuCheckControls")


def frame():
    """Advance to the next _MenuCheckControls (one menu frame)."""
    gdb("continue", timeout=4)


def read_val(expr):
    m = re.search(r"\$\d+\s*=\s*(.+)", gdb(f"print {expr}"))
    return m.group(1).strip() if m else "?"


def where():
    ordinal = read_val(f"{MENU}->ordinal")
    nentries = read_val("((Menu *)currentMenu)->numEntries")
    action = read_val(f"{MENU}->onSelect")
    sym = re.search(r"<([^>+]+)", action)
    action = sym.group(1) if sym else action
    print(f"selected: ordinal={ordinal}  action={action}  (numEntries={nentries})")


def tap(var, press_frames=3):
    """Clean edge tap of an s_kbd_* control: clear -> press -> release."""
    ensure_menu_bp()
    gset(var, 0)
    for _ in range(4):
        frame()                      # clear the held-direction edge
    gset(var, 1)
    for _ in range(press_frames):
        frame()                      # fresh edge -> action
    gset(var, 0)
    for _ in range(2):
        frame()                      # release


def sym_of(expr):
    """print <expr> and return the resolved <symbol> name (function pointer)."""
    m = re.search(r"<([^>+]+)", gdb(f"print {expr}"))
    return m.group(1) if m else None


def num_entries():
    m = re.search(r"=\s*(-?\d+)", gdb("print ((Menu *)currentMenu)->numEntries"))
    return int(m.group(1)) if m else 0


def entry_action(i):
    """onSelect action symbol of menu entry `i` (platform-independent via
    _CalcMenuEntryAddress, so it works on SDL and Amiga)."""
    return sym_of(f"((MenuEntry *)_CalcMenuEntryAddress({i}))->onSelect")


def enum():
    n = num_entries()
    print(f"menu: {n} entries")
    for i in range(n):
        a = entry_action(i)
        if a:
            print(f"  {i:2d} -> {a}")


def goto(symbol):
    """Move the selection to the entry whose onSelect == symbol (direct, via
    _SetCurrentEntry — no path-finding, NO fire). Returns the ordinal or None.
    Use this to land on a value-cycling control (e.g. nationality) before
    applying a combo; use `select` to actually activate an entry."""
    ensure_menu_bp()
    n = num_entries()
    target = next((i for i in range(n) if entry_action(i) == symbol), None)
    if target is None:
        print(f"goto: '{symbol}' not in this menu (try `enum`)")
        return None
    gdb(f"call (void)_SetCurrentEntry({target})")
    got = sym_of("((Menu *)currentMenu)->selectedEntry->onSelect")
    if got != symbol:
        print(f"goto: set failed (selected={got})")
        return None
    return target


def select(symbol):
    """goto(symbol) then fire to activate it."""
    target = goto(symbol)
    if target is None:
        return 1
    tap("s_kbd_fire")
    print(f"selected+fired {symbol} (ordinal {target})")
    return 0


def tap_multi(vars_, press_frames=3):
    """Hold several s_kbd_* controls together for one clean tap (clear edge ->
    press -> release). For combos like fire+left (nationality -> OTHER)."""
    ensure_menu_bp()
    for v in vars_:
        gset(v, 0)
    for _ in range(4):
        frame()
    for v in vars_:
        gset(v, 1)
    for _ in range(press_frames):
        frame()
    for v in vars_:
        gset(v, 0)
    for _ in range(2):
        frame()


def combo(ctrls):
    tap_multi([f"s_kbd_{c}" for c in ctrls])
    print("combo " + "+".join(ctrls))


def main(argv):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("where")
    sub.add_parser("enum")
    p_nav = sub.add_parser("nav")
    p_nav.add_argument("dir", choices=["up", "down", "left", "right"])
    sub.add_parser("fire")
    p_sel = sub.add_parser("select")
    p_sel.add_argument("symbol")
    p_goto = sub.add_parser("goto")
    p_goto.add_argument("symbol")
    p_combo = sub.add_parser("combo")
    p_combo.add_argument("ctrls", nargs="+",
                         help="controls to hold together, e.g. fire left")
    p_cap = sub.add_parser("cap")
    p_cap.add_argument("out")
    a = ap.parse_args(argv[1:])

    if a.cmd == "where":
        where()
    elif a.cmd == "enum":
        enum()
    elif a.cmd == "nav":
        tap(f"s_kbd_{a.dir}")
        where()
    elif a.cmd == "fire":
        tap("s_kbd_fire")
        print("fired.")
    elif a.cmd == "select":
        return select(a.symbol)
    elif a.cmd == "goto":
        ok = goto(a.symbol)
        if ok is not None:
            where()
        return 0 if ok is not None else 1
    elif a.cmd == "combo":
        combo(a.ctrls)
        where()
    elif a.cmd == "cap":
        subprocess.run(["python3", os.path.join(HERE, "swos_screencap.py"), a.out])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
