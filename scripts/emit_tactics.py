#!/usr/bin/env python3
"""Bake bin/data_json/tactics/*.json into the static TeamTactics initializers in
src/tactics_data.c — the Amiga JSON->C path (the SDL build also reads the JSON at
runtime via src/tactics_load_json.c). Mirrors emit_competitions.py / emit_menus.py.

  python3 scripts/emit_tactics.py --out src/tactics_data.c   # write the baked C
  python3 scripts/emit_tactics.py                            # round-trip self-check only

Four formations also have a `<id>_original.json` holding the PRISTINE Amiga
data, baked alongside as `_teamTactics<id>Original` and selected by
-DSWOS_ORIGINAL_TACTICS. See the header comment in src/tactics_data.c for what
they are and why the shipped ones differ.

The self-check re-parses the spliced C and confirms every formation reproduces the
JSON byte-for-byte (the 370-byte TeamTactics image).
"""
import os
import re
import sys
import json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import export_tactics as ex


def json_bytes(d):
    b = [int(x, 0) & 0xFF for x in d["name_bytes"]]
    for pl in d["players"]:
        for z in pl:
            b += [int(x, 0) & 0xFF for x in z]
    b += [v & 0xFF for v in d["pair_ids"]]
    b += [d["ball_out_of_play"] & 0xFF]
    return b


def c_bytes(text, sym):
    """The 370-byte image of `TeamTactics <sym>` parsed out of C source text."""
    m = ex.split_top(ex.find_def(text, sym))
    b = [ex.byte(x) for x in ex.split_top(ex.inner(m[0]))]
    for p in ex.split_top(ex.inner(m[1])):
        for z in ex.parse_player(p):
            b += z
    b += [ex.byte(x) for x in ex.split_top(ex.inner(m[2]))]
    b += [ex.byte(m[3])]
    return b


def build_init(d):
    name = "{ " + ", ".join("0x%02X" % (int(v, 0) & 0xFF) for v in d["name_bytes"]) + " }"
    players = []
    for pl in d["players"]:
        zones = ", ".join("{ " + ", ".join("0x%02X" % (int(b, 0) & 0xFF) for b in z) + " }"
                          for z in pl)
        players.append("{ { %s } }" % zones)
    pos = ",\n        ".join(players)
    pair = "{ " + ", ".join("0x%02X" % (v & 0xFF) for v in d["pair_ids"]) + " }"
    ball = "0x%02X" % (d["ball_out_of_play"] & 0xFF)
    return "{\n    %s,\n    {\n        %s\n    },\n    %s,\n    %s\n}" % (name, pos, pair, ball)


def splice(text, sym, init):
    """Replace the `= { ... }` initializer body of `TeamTactics <sym>` (brace-matched)."""
    m = re.search(r'(\bTeamTactics\s+' + re.escape(sym) + r'\s*=\s*)\{', text)
    if not m:
        raise SystemExit("emit_tactics: %s not found in %s" % (sym, ex.SRC))
    i, depth = m.end() - 1, 0
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    return text[:m.start()] + m.group(1) + init + text[i + 1:]


# The formations that also ship their pristine pre-fix data (see the header
# comment in src/tactics_data.c). Any <id>_original.json present is baked as
# `_teamTactics<id>Original`, so -DSWOS_ORIGINAL_TACTICS is a rebuild, not a
# file swap.
ORIG_SUFFIX = "_original"


def original_ids():
    return [tid for tid in ex.IDS
            if os.path.exists(os.path.join(ex.OUT_DIR, tid + ORIG_SUFFIX + ".json"))]


def main():
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else None
    text = open(ex.SRC, encoding="latin1").read()
    data = {tid: json.load(open(os.path.join(ex.OUT_DIR, tid + ".json"))) for tid in ex.IDS}
    for tid in ex.IDS:
        text = splice(text, "_teamTactics" + tid, build_init(data[tid]))

    orig_ids = original_ids()
    orig = {tid: json.load(open(os.path.join(ex.OUT_DIR, tid + ORIG_SUFFIX + ".json")))
            for tid in orig_ids}
    for tid in orig_ids:
        text = splice(text, "_teamTactics" + tid + "Original", build_init(orig[tid]))

    # round-trip self-check: the spliced C must reproduce each JSON image exactly.
    bad = 0
    for tid in ex.IDS:
        if c_bytes(text, "_teamTactics" + tid) != json_bytes(data[tid]):
            print("  MISMATCH %s" % tid); bad += 1
    for tid in orig_ids:
        if c_bytes(text, "_teamTactics" + tid + "Original") != json_bytes(orig[tid]):
            print("  MISMATCH %s%s" % (tid, ORIG_SUFFIX)); bad += 1
    if bad:
        raise SystemExit("emit_tactics: %d formation(s) failed round-trip" % bad)

    if out:
        open(out, "w", encoding="latin1").write(text)
        print("wrote %s (%d formations + %d pristine variants, round-trip OK)"
              % (out, len(ex.IDS), len(orig_ids)))
    else:
        print("round-trip OK: %d formations + %d pristine variants reproduce the JSON"
              % (len(ex.IDS), len(orig_ids)))


if __name__ == "__main__":
    main()
