#!/usr/bin/env python3
"""SWOS gdb test-harness CLI — drive menus + read state via the gdb_proxy daemon,
WITHOUT screenshots (fast + token-light). Screenshots (`cap`) are for
orientation only; navigation feedback comes from reading the menu state.

This is a thin CLI over the layered harness (see docs/HARNESS.md):
  gdb_proxy.py (transport) -> harness_lib.py (generic) -> swos_plugin.py (game).

Driving works on BOTH the SDL build and the Amiga build (FS-UAE), via one
injection point in the shared _MenuProc — no per-target input code. The game
must be AT A MENU. Commands:
  where                  selected ordinal + onSelect symbol + numEntries
  enum                   list every entry's onSelect symbol
  nav up|down|left|right one directional step; prints where after
  fire                   press fire on the current entry
  combo CTRL...          hold controls together one frame (e.g. combo fire left)
  goto SYMBOL            move selection to entry whose onSelect == SYMBOL
  select SYMBOL          goto SYMBOL then fire
  load NAME.CAR          load a career save via the game's own loader
  cap OUT.png            screenshot (SDL only; orientation aid)
  check                  self-test that gdb writes reach the target (read-only
                         stub detector)
"""
import argparse
import sys
from swos_plugin import SwosHarness


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--proxy", default=None, help="gdb_proxy.py path override")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("where")
    sub.add_parser("enum")
    sub.add_parser("prime")
    sub.add_parser("check")
    p = sub.add_parser("nav"); p.add_argument("dir", choices=["up", "down", "left", "right"])
    sub.add_parser("fire")
    p = sub.add_parser("combo"); p.add_argument("ctrls", nargs="+")
    p = sub.add_parser("goto"); p.add_argument("symbol")
    p = sub.add_parser("select"); p.add_argument("symbol")
    p = sub.add_parser("load"); p.add_argument("name")
    p = sub.add_parser("cap"); p.add_argument("out")
    a = ap.parse_args(argv[1:])

    H = SwosHarness(proxy=a.proxy)

    def show(w):
        print(f"selected: ordinal={w['ordinal']}  action={w['action']}  "
              f"(numEntries={w['num_entries']})")

    if a.cmd == "where":
        H.ensure_stopped(); show(H.where())
    elif a.cmd == "enum":
        H.ensure_stopped()
        print(f"target={'SDL' if H.is_sdl() else 'Amiga'}")
        for i, act in H.enum():
            if act:
                print(f"  {i:2d} -> {act}")
    elif a.cmd == "prime":
        H.prime(); show(H.where())
    elif a.cmd == "check":
        ok, rb = H.h.write_check("numSelectedTeams", 88)
        H.h.set_var("numSelectedTeams", 0)
        print(f"write {'OK' if ok else 'FAILED (read-only stub?)'} "
              f"(set 88 -> read {rb})")
        return 0 if ok else 1
    elif a.cmd == "nav":
        show(H.nav(a.dir))
    elif a.cmd == "fire":
        show(H.fire())
    elif a.cmd == "combo":
        show(H.combo(a.ctrls))
    elif a.cmd == "goto":
        t = H.goto(a.symbol)
        print(f"goto {a.symbol}: {'ordinal '+str(t) if t is not None else 'not found'}")
        return 0 if t is not None else 1
    elif a.cmd == "select":
        ok = H.select(a.symbol)
        print(f"select {a.symbol}: {'fired' if ok else 'not found'}")
        return 0 if ok else 1
    elif a.cmd == "load":
        rc = H.load_career(a.name)
        print(f"load {a.name}: returned {rc}")
    elif a.cmd == "cap":
        print(H.screencap(a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
