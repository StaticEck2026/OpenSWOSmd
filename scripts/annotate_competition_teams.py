#!/usr/bin/env python3
"""Annotate the TeamId seed lists in src/competition_data.c.

For every CompetitionCup / CompetitionTournament `teamSeedList` (the LAST aggregate of
each initializer), rewrite each {country, ordinal} pair so the country is its
CN_* #define and the team name is appended as a comment:

    {33,33}  ->  { CN_SCOTLAND, 33 /* RANGERS */ }

The country->CN_ map comes from game_types.h; the team name is resolved from
TEAM.<country> via the swos_team_file library.  CN_X is #define'd to the same
number, so the change is byte-identical to the compiler — only readability
changes.  Scoped structurally to the last brace group, so Round records, the
9-int Competition base, roundRuleFlags and the Playoffs struct are never touched.

    annotate_competition_teams.py            # dry-run: summary + euroCup preview
    annotate_competition_teams.py --apply     # rewrite the file in place
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swos_team_file as tf

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "src", "competition_data.c")

# {N, M} or already-converted {CN_X, M /* ... */} ; captures country token + ordinal
PAIR = re.compile(r"\{\s*(CN_\w+|-?\d+)\s*,\s*(-?\d+)\s*(?:/\*.*?\*/\s*)?\}", re.S)
DEF = re.compile(r"\bCompetition(?:Cup|Tournament)\s+[A-Za-z_]\w*\s*=\s*\{")


def name2num():
    return {"CN_" + v: k for k, v in tf._load_country_map().items()}


def last_group(src, body_open):
    """Return (start,end) of the last depth-1 brace group inside the initializer
    that opens at body_open ('{'), or None."""
    depth = 0
    gstart = None
    groups = []
    i = body_open
    n = len(src)
    while i < n:
        c = src[i]
        if c == "{":
            depth += 1
            if depth == 2:
                gstart = i
        elif c == "}":
            if depth == 2:
                groups.append((gstart, i))
            depth -= 1
            if depth == 0:
                break
        i += 1
    return groups[-1] if groups else None


def build_group(group_text, indent, n2n):
    pairs = PAIR.findall(group_text)
    if not pairs:
        return None, 0
    rows = []
    for ctok, ords in pairs:
        num = n2n[ctok] if ctok.startswith("CN_") else int(ctok)
        cn = tf.country_define(num) or ("CN_%d" % num)
        nm = (tf.resolve(num, int(ords)) or "?").strip().replace("*/", "* /") or "?"
        rows.append((cn, ords, nm))
    wcn = max(len(r[0]) for r in rows)
    word = max(len(r[1]) for r in rows)
    lines = ["{"]
    for cn, ords, nm in rows:
        cell = (cn + ",").ljust(wcn + 1)
        lines.append("%s    { %s %s /* %s */ }," % (indent, cell, ords.rjust(word), nm))
    lines.append(indent + "}")
    return "\n".join(lines), len(rows)


def process(src):
    edits = []
    total_pairs = 0
    for m in DEF.finditer(src):
        grp = last_group(src, m.end() - 1)
        if not grp:
            continue
        gstart, gend = grp
        gtext = src[gstart : gend + 1]
        line_start = src.rfind("\n", 0, gstart) + 1
        indent = re.match(r"\s*", src[line_start:gstart]).group(0)
        new, npairs = build_group(gtext, indent, n2n)
        if new and new != gtext:
            edits.append((gstart, gend + 1, new))
            total_pairs += npairs
    out = src
    for s, e, new in sorted(edits, reverse=True):
        out = out[:s] + new + out[e:]
    return out, len(edits), total_pairs


n2n = name2num()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true", help="rewrite competition_data.c in place")
    args = ap.parse_args()

    src = open(SRC).read()
    out, n_lists, n_pairs = process(src)
    print("seed lists rewritten: %d   team-id pairs annotated: %d" % (n_lists, n_pairs))

    # preview the euroCup seed list
    m = re.search(r"CompetitionTournament euroCup =.*?\n\};", out, re.S)
    if m:
        print("\n--- euroCup (after) ---")
        print(m.group(0))

    if args.apply:
        open(SRC, "w").write(out)
        print("\nWROTE", SRC)
    else:
        print("\n(dry-run; pass --apply to write)")


if __name__ == "__main__":
    main()
