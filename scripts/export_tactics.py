#!/usr/bin/env python3
"""Export the built-in TeamTactics formations from src/tactics_data.c to JSON.

The C side is the static `TeamTactics _teamTactics442 = { … };` initializers (12
presets). Each is a fixed 370-byte struct of plain bytes — no pointers — so the
JSON is a straight, named serialization of the fields:

    name_bytes[9]   - tactic name + flags (raw bytes)
    players[10][5][7] - 10 outfield slots, each 5 ball-zones x 7 quadrants
                        (packed x/y nibbles: hi = x, lo = y)
    pair_ids[10]    - per-slot pair/marking id (signed; -1 = unlinked)
    ball_out_of_play - redirect tactic index

One JSON file per formation lands in bin/data_json/tactics/<id>.json. This is the
source of truth: emit_tactics.py bakes it back to C at build (Amiga path), and
src/tactics_load_json.c reads it at runtime (SDL). Mirrors export_competitions.py
/ export_menus.py.

Run from the repo root:  python3 scripts/export_tactics.py
"""
import os
import re
import json

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "src", "tactics_data.c")
OUT_DIR = os.path.join(REPO, "bin", "data_json", "tactics")

# The 12 presets, in _teamTacticsTable[] order (the loader/emit reuse this order).
IDS = ["442", "541", "451", "532", "352", "433", "424", "343",
       "Sweep", "523", "Attack", "Defend"]
LABEL = {"442": "4-4-2", "541": "5-4-1", "451": "4-5-1", "532": "5-3-2",
         "352": "3-5-2", "433": "4-3-3", "424": "4-2-4", "343": "3-4-3",
         "Sweep": "Sweep", "523": "5-2-3", "Attack": "Attack", "Defend": "Defend"}


def split_top(body):
    """Split a brace body into top-level comma tokens (nesting-aware)."""
    toks, depth, cur = [], 0, ""
    for ch in body:
        if ch in "{(":
            depth += 1; cur += ch
        elif ch in ")}":
            depth -= 1; cur += ch
        elif ch == "," and depth == 0:
            toks.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip():
        toks.append(cur.strip())
    return toks


def inner(tok):
    tok = tok.strip()
    assert tok.startswith("{") and tok.endswith("}"), tok[:60]
    return tok[1:-1]


def byte(tok):
    return int(tok.strip(), 0) & 0xFF


def hx(b):
    return "0x%02X" % (b & 0xFF)


def find_def(text, sym):
    """Return the brace body of `TeamTactics <sym> = { ... }` (nesting-aware)."""
    m = re.search(r'\bTeamTactics\s+' + re.escape(sym) + r'\s*=\s*\{', text)
    if not m:
        raise SystemExit("export_tactics: %s not found in tactics_data.c" % sym)
    i, depth = m.end() - 1, 0
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    return text[m.end():i]


def parse_player(pp):
    """One PlayerPositions initializer -> [5][7] of bytes. Tolerates the struct
    wrap brace `{ { {7}.. } }` and the elided form `{ {7}.. }`."""
    t = split_top(inner(pp))
    if len(t) == 1 and t[0].strip().startswith("{"):
        t = split_top(inner(t[0]))               # unwrap the struct -> the [5][7]
    zones = [[byte(x) for x in split_top(inner(z))] for z in t]
    assert len(zones) == 5 and all(len(z) == 7 for z in zones), \
        "player must be 5x7, got %r" % [len(z) for z in zones]
    return zones


def parse_tactics(sym, label):
    body = find_def(open(SRC, encoding="latin1").read(), sym)
    m = split_top(body)                          # name, positions, pairIds, ballOut
    name = [byte(x) for x in split_top(inner(m[0]))]
    players = [parse_player(p) for p in split_top(inner(m[1]))]
    pair = [byte(x) for x in split_top(inner(m[2]))]
    ball = byte(m[3])
    assert len(name) == 9 and len(players) == 10 and len(pair) == 10, \
        "%s: bad field counts" % sym
    return {
        "id": sym[len("_teamTactics"):] if sym.startswith("_teamTactics") else sym,
        "label": label,
        "name_bytes": [hx(b) for b in name],
        "players": [[[hx(b) for b in zone] for zone in pl] for pl in players],
        "pair_ids": [b - 256 if b >= 128 else b for b in pair],   # signed (-1 = unlinked)
        "ball_out_of_play": ball,
    }


def collapse_leaf_arrays(s):
    """Put each innermost array (a zone's 7 bytes, name_bytes, pair_ids) on one
    line; the nesting arrays (players, per-player zones) stay expanded."""
    def repl(m):
        items = [t.strip() for t in m.group(0)[1:-1].split(",") if t.strip()]
        return "[" + ", ".join(items) + "]"
    return re.sub(r'\[[^\[\]]*\]', repl, s)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for tid in IDS:
        d = parse_tactics("_teamTactics" + tid, LABEL[tid])
        d["id"] = tid
        text = collapse_leaf_arrays(json.dumps(d, indent=1, ensure_ascii=False))
        with open(os.path.join(OUT_DIR, tid + ".json"), "w") as f:
            f.write(text + "\n")
        print("wrote %-14s (%s)" % (tid + ".json", LABEL[tid]))


if __name__ == "__main__":
    main()
