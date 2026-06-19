#!/usr/bin/env python3
"""Export competition_data.c competition tables to JSON (data jailbreak bootstrap).

See docs/DATA_INTERNAL_JSON.md. This is the Python bootstrap exporter (mechanism
A): it preprocesses competition_data.c with gcc -E (expanding the CN_* /
CUP*_OFFSET macros), parses the static initializers, resolves name[] pointers to
the strings in strings_data.c, resolves teamSeedList {country,ordinal} pairs to
team names via swos_team_file, and writes one JSON file per competition type to
bin/data_json/competitions/.

Phase 2 (this commit): the cup competitions — CompetitionCup (cups.json) and
CompetitionCup2 (cups2.json). Leagues / playoffs / qualifying / career index /
league-blocks follow (mechanism B, the in-build C emitter, is the production
path; see the doc).

Run from the repo root:  python3 scripts/export_competitions.py
"""
import os
import re
import subprocess
import sys
import json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swos_team_file as tf

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "src", "competition_data.c")
STRINGS = os.path.join(REPO, "src", "strings_data.c")
OUT_DIR = os.path.join(REPO, "bin", "data_json", "competitions")

# Self-relative offset bytes are derived layout artifacts (game_types.h static
# asserts fix them): CompetitionCup name/teamList = 15/21, CompetitionCup2 = 35/41.
CUP_OFFSETS = {False: (15, 21), True: (35, 41)}   # keyed by is_cup2


# ---------------------------------------------------------------- preprocessing
def preprocess() -> str:
    out = subprocess.run(
        ["gcc", "-E", "-P", "-I", os.path.join(REPO, "src"),
         "-DSWOS_SDL_BUILD", "-DCOMPETITION_DATA_C=1", SRC],
        capture_output=True, text=True, check=True).stdout
    return out


def load_strings() -> dict:
    """Map name-symbol -> string value from strings_data.c."""
    text = open(STRINGS, encoding="latin1").read()
    m = {}
    for sym, val in re.findall(r'\bchar\s+(a[A-Za-z0-9_]+)\s*\[\]\s*=\s*"((?:[^"\\]|\\.)*)"', text):
        m[sym] = val.encode().decode("unicode_escape")
    return m


# ------------------------------------------------------------- token utilities
def split_top(body: str):
    """Split a brace/paren body into top-level comma tokens (nesting-aware)."""
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


def inner(tok: str) -> str:
    tok = tok.strip()
    assert tok.startswith("{") and tok.endswith("}"), tok[:40]
    return tok[1:-1]


def num(tok: str):
    """Parse an integer initializer token; None if it is an offsetof macro."""
    tok = tok.strip()
    if "offsetof" in tok:
        return None
    return int(tok, 0)


def find_defs(text: str, typename: str):
    """Yield (name, body) for each `typename <name> = { ... };` (body = inside braces)."""
    for m in re.finditer(r'\b' + typename + r'\s+([A-Za-z_]\w*)\s*=\s*\{', text):
        i, depth = m.end() - 1, 0
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        yield m.group(1), text[m.end():i]


# ----------------------------------------------------------------- comp parsing
COMP_FIELDS = ["comp_number", "type", "team_number", "begin_month", "end_month",
               "name_table_offset", "reserved", "team_list_offset", "seed_reorder_flag"]


def parse_comp(tok: str, is_cup2: bool) -> dict:
    vals = split_top(inner(tok))
    name_off, team_off = CUP_OFFSETS[is_cup2]
    out = {}
    for i, (k, v) in enumerate(zip(COMP_FIELDS, vals)):
        n = num(v)
        if n is None:                       # offsetof macro -> derived constant
            n = name_off if i == 5 else team_off if i == 7 else 0
        out[k] = n
    return out


def resolve_name(tok: str, strings: dict):
    tok = tok.strip()
    if tok in ("0", "NULL"):
        return None
    return strings.get(tok, tok)            # fall back to the symbol if unresolved


def parse_seed_list(tok: str):
    out = []
    for e in split_top(inner(tok)):
        e = e.strip()
        if not (e.startswith("{") and e.endswith("}")):
            continue                        # skip bare padding scalars
        pair = split_top(inner(e))
        if len(pair) < 2:
            continue
        country, ordinal = int(pair[0], 0), int(pair[1], 0)
        out.append({"country": tf.country_define(country) or country,
                    "ordinal": ordinal,
                    "team": tf.resolve(country, ordinal)})
    return out


def parse_cup(name: str, body: str, is_cup2: bool, strings: dict) -> dict:
    # members: header [, roundRuleFlags [, name[2] [, teamSeedList ]]] — domestic
    # cups stop after roundRuleFlags (no name table / static draw, brace-elided).
    members = split_top(body)
    header = members[0]
    rule_flags_tok = members[1] if len(members) > 1 else None
    name_tok = members[2] if len(members) > 2 else None
    seed_tok = members[3] if len(members) > 3 else None

    htoks = split_top(inner(header))
    d = {"id": name, "type": "cup2" if is_cup2 else "cup",
         "comp": parse_comp(htoks[0], is_cup2)}

    if is_cup2:
        for k, v in zip(["seeding_type", "menu_state", "play_each_team",
                         "away_goals_mode", "subs_and_points_for_win",
                         "round_string_base"], htoks[1:7]):
            d[k] = num(v)
        rounds = []
        for r in split_top(inner(htoks[7])):
            rt = split_top(inner(r))
            rounds.append({"team_count": int(rt[0], 0), "group_count": int(rt[1], 0),
                           "team_group_count": int(rt[2], 0)})
        d["rounds"] = rounds
        d["header_reserved"] = num(htoks[8]) if len(htoks) > 8 else 0
    else:
        for k, v in zip(["num_rounds_override", "teams", "away_goals_mode",
                         "subs_and_points_for_win", "round_string_base"], htoks[1:6]):
            d[k] = num(v)

    d["round_rule_flags"] = [num(x) for x in split_top(inner(rule_flags_tok))] if rule_flags_tok else []
    d["name"] = [resolve_name(x, strings) for x in split_top(inner(name_tok))] if name_tok else []
    d["team_seed_list"] = parse_seed_list(seed_tok) if seed_tok else []
    return d


def main():
    text = preprocess()
    strings = load_strings()
    os.makedirs(OUT_DIR, exist_ok=True)

    cups = [parse_cup(n, b, False, strings) for n, b in find_defs(text, "CompetitionCup")]
    cups2 = [parse_cup(n, b, True, strings) for n, b in find_defs(text, "CompetitionCup2")]

    for fname, data in [("cups.json", cups), ("cups2.json", cups2)]:
        with open(os.path.join(OUT_DIR, fname), "w") as f:
            json.dump({"type": "competition_cups", "count": len(data), "competitions": data},
                      f, indent=2, ensure_ascii=False)
        print("wrote %s (%d competitions)" % (fname, len(data)))


if __name__ == "__main__":
    main()
