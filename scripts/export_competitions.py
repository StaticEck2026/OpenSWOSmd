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
# asserts / offsetof fix them): CompetitionCup name/teamList = 15/21,
# CompetitionCup2 = 35/41, CompetitionLeague name = 33 (offsetof(name)-5).
CUP_OFFSETS = {False: (15, 21), True: (35, 41)}   # keyed by is_cup2
LEAGUE_NAME_OFFSET = 33


# ---------------------------------------------------------------- preprocessing
def preprocess() -> str:
    out = subprocess.run(
        ["gcc", "-E", "-P", "-I", os.path.join(REPO, "src"),
         "-DSWOS_SDL_BUILD", "-DCOMPETITION_DATA_C=1", SRC],
        capture_output=True, text=True, check=True).stdout
    return out


def load_strings() -> dict:
    """Map name-symbol -> string value from every src/*.c (strings_data.c,
    swos_amiga_strings.c, …); names are `char aX[opt] = "...";`."""
    import glob
    pat = re.compile(r'\bchar\s+(a[A-Za-z0-9_]+)\s*\[[0-9]*\]\s*=\s*"((?:[^"\\]|\\.)*)"')
    m = {}
    for path in glob.glob(os.path.join(REPO, "src", "*.c")):
        for sym, val in pat.findall(open(path, encoding="latin1").read()):
            m.setdefault(sym, val.encode().decode("unicode_escape"))
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
    """Yield (name, body) for each `typename <name>[opt] = { ... };` (body = inside braces)."""
    for m in re.finditer(r'\b' + typename + r'\s+([A-Za-z_]\w*)\s*(?:\[[0-9]*\])?\s*=\s*\{', text):
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


def parse_comp(tok: str, name_off: int = 0, team_off: int = 0) -> dict:
    vals = split_top(inner(tok))
    out = {}
    for i, (k, v) in enumerate(zip(COMP_FIELDS, vals)):
        n = num(v)
        if n is None:                       # offsetof macro -> derived constant
            n = name_off if i == 5 else team_off if i == 7 else 0
        out[k] = n
    return out


def xref(tok: str):
    """Resolve a cross-reference initializer to a symbol id, or None (NULL / -1 term)."""
    tok = tok.strip()
    m = re.search(r'&\s*([A-Za-z_][A-Za-z0-9_.]*)', tok)
    if m:
        return m.group(1)
    return None                              # 0 / NULL / (Type *)-1 terminator


def sentinel_array(tok: str, term):
    """Numbers from a {a,b,...,TERM,...} array, stopping at the sentinel value."""
    out = []
    for v in split_top(inner(tok)):
        n = num(v)
        if n is None or (n & 0xFF) == (term & 0xFF):
            break
        out.append(n)
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
         "comp": parse_comp(htoks[0], *CUP_OFFSETS[is_cup2])}

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


def parse_playoffs(name: str, body: str, strings: dict) -> dict:
    # { {comp9}, numRoundsOverride, teams, awayGoalsMode, subsAndMaxSubs, roundStringBase,
    #   {roundRuleFlags[2]}, {name[2]}, {qualifiedTeamIds[8]=0} }
    m = split_top(body)
    d = {"id": name, "comp": parse_comp(m[0], 11, 17)}   # Playoffs uses literal 11/17
    for k, v in zip(["num_rounds_override", "teams", "away_goals_mode",
                     "subs_and_max_subs", "round_string_base"], m[1:6]):
        d[k] = num(v)
    d["round_rule_flags"] = [num(x) for x in split_top(inner(m[6]))] if len(m) > 6 else []
    d["name"] = [resolve_name(x, strings) for x in split_top(inner(m[7]))] if len(m) > 7 else []
    return d


def parse_division(tok: str) -> dict:
    v = [num(x) for x in split_top(inner(tok))]
    keys = ["teams", "promoted", "promoted_po", "relegated", "relegated_po", "qualifier_link_offset"]
    return {k: (v[i] if i < len(v) else 0) for i, k in enumerate(keys)}


def parse_league(name: str, body: str, strings: dict) -> dict:
    # { {comp9, divisionsCount, rounds, pointsForWin, subsAndMaxSubs}, {divisions[]},
    #   reserved1, {name[]}, [{playoffLinkPtrs[2]}], [{promotionLinkData[24]}] }
    m = split_top(body)
    h = split_top(inner(m[0]))
    d = {"id": name, "comp": parse_comp(h[0], LEAGUE_NAME_OFFSET, 0)}
    for k, v in zip(["divisions_count", "rounds", "points_for_win", "subs_and_max_subs"], h[1:5]):
        d[k] = num(v)
    d["divisions"] = [parse_division(x) for x in split_top(inner(m[1]))] if len(m) > 1 else []
    d["reserved1"] = num(m[2]) if len(m) > 2 else 0
    d["name"] = [resolve_name(x, strings) for x in split_top(inner(m[3]))] if len(m) > 3 else []
    if len(m) > 4:
        d["playoff_links"] = [xref(x) for x in split_top(inner(m[4]))]
    if len(m) > 5:
        d["promotion_link_data"] = [num(x) for x in split_top(inner(m[5]))]
    return d


def parse_qualifying(name: str, body: str) -> dict:
    # { {compCup[9] -1-term cross-refs}, {countries[44] 0xFF-term} }
    m = split_top(body)
    return {"id": name,
            "comp_cup": [xref(x) for x in split_top(inner(m[0])) if xref(x)],
            "countries": sentinel_array(m[1], 0xFF) if len(m) > 1 else []}


def parse_career(name: str, body: str) -> list:
    # careerContests[35]: { &competition, &competitionCopy, sizeof(...) } — size derived on load
    out = []
    for e in split_top(body):
        t = split_top(inner(e))
        out.append({"competition": xref(t[0]), "competition_copy": xref(t[1])})
    return out


def parse_leagues(name: str, body: str) -> dict:
    # 4×{ league*, linkParams[8] } then [playoffs*], [nextGroup*], [{qualifierIndexPairs[24]}]
    m = split_top(body)
    d = {"id": name, "leagues": []}
    i = 0
    while i + 1 < len(m) and len(d["leagues"]) < 4 and m[i + 1].strip().startswith("{"):
        d["leagues"].append({"league": xref(m[i]),
                             "link_params": [num(x) for x in split_top(inner(m[i + 1]))]})
        i += 2
    rest = m[i:]
    ptrs = [t for t in rest if not t.strip().startswith("{")]
    if len(ptrs) >= 1:
        d["playoffs"] = xref(ptrs[0])
    if len(ptrs) >= 2:
        d["next_group"] = xref(ptrs[1])
    braces = [t for t in rest if t.strip().startswith("{")]
    if braces:
        d["qualifier_index_pairs"] = [num(x) for x in split_top(inner(braces[-1]))]
    return d


def split_designated(body: str):
    """Split a designated-initializer body into (member, value) pairs (.x = v, ...)."""
    for tok in split_top(body):
        m = re.match(r'\.(\w+)\s*=\s*(.+)$', tok.strip(), re.S)
        if m:
            yield m.group(1), m.group(2).strip()


def parse_english_block(name: str, body: str, strings: dict) -> dict:
    # EnglishLeaguesBlock { _main: CompetitionLeague, _div1/_div2/_div3: Leagues }
    out = {"id": name}
    for member, val in split_designated(body):
        if member == "_main":
            out[member] = parse_league(name + "._main", inner(val), strings)
        else:
            out[member] = parse_leagues(name + "." + member, inner(val))
    return out


def parse_designated_arrays(name: str, body: str) -> dict:
    """A struct of named numeric arrays (e.g. EuroCupQualSeedBucketBlock)."""
    out = {"id": name}
    for member, val in split_designated(body):
        out[member] = [num(x) for x in split_top(inner(val))] if val.startswith("{") else num(val)
    return out


def dump(fname: str, kind: str, key: str, items: list):
    with open(os.path.join(OUT_DIR, fname), "w") as f:
        json.dump({"type": kind, "count": len(items), key: items}, f, indent=2, ensure_ascii=False)
    print("wrote %-20s (%d)" % (fname, len(items)))


def main():
    text = preprocess()
    strings = load_strings()
    os.makedirs(OUT_DIR, exist_ok=True)

    dump("cups.json", "competition_cups", "competitions",
         [parse_cup(n, b, False, strings) for n, b in find_defs(text, "CompetitionCup")])
    dump("cups2.json", "competition_cups2", "competitions",
         [parse_cup(n, b, True, strings) for n, b in find_defs(text, "CompetitionCup2")])
    dump("playoffs.json", "playoffs", "playoffs",
         [parse_playoffs(n, b, strings) for n, b in find_defs(text, "Playoffs")])
    dump("leagues.json", "competition_leagues", "leagues",
         [parse_league(n, b, strings) for n, b in find_defs(text, "CompetitionLeague")])
    dump("qualifying.json", "qualifying_groups", "groups",
         [parse_qualifying(n, b) for n, b in find_defs(text, "QualifyingGroup")])

    # league blocks: the standalone Leagues groups + the English block (designated init)
    groups = [parse_leagues(n, b) for n, b in find_defs(text, "Leagues")]
    english = [parse_english_block(n, b, strings) for n, b in find_defs(text, "EnglishLeaguesBlock")]
    with open(os.path.join(OUT_DIR, "league_blocks.json"), "w") as f:
        json.dump({"type": "league_blocks", "groups": groups,
                   "english_block": english[0] if english else None},
                  f, indent=2, ensure_ascii=False)
    print("wrote %-20s (%d groups + %d english block)" % ("league_blocks.json", len(groups), len(english)))

    # career index (single table of 35 contests)
    contests = []
    for n, b in find_defs(text, "CompetitionCareer"):
        contests = parse_career(n, b)
    dump("career.json", "career_index", "contests", contests)

    # misc lookup tables (best-effort): country lists + euro-cup seed buckets
    misc = {"country_lists": [{"id": n, "countries": [num(x) for x in split_top(b)]}
                              for n, b in find_defs(text, "CountryNumbers")],
            "euro_cup_qual_seed_buckets": [parse_designated_arrays(n, b)
                                           for n, b in find_defs(text, "EuroCupQualSeedBucketBlock")]}
    with open(os.path.join(OUT_DIR, "misc.json"), "w") as f:
        json.dump({"type": "misc", **misc}, f, indent=2, ensure_ascii=False)
    print("wrote %-20s (%d country lists)" % ("misc.json", len(misc["country_lists"])))


if __name__ == "__main__":
    main()
