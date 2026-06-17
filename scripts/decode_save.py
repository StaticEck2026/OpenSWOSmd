#!/usr/bin/env python3
"""Decode a SWOS JSON save (.CAR / .PRE / .DIY / .SEA) and summarise its
competition slots, with extra detail for type-2 (tournament/hybrid) slots.

This is a *write-side* inspector: the JSON is target-neutral text, so it
tells you what a build emitted, not how another build will load it. Use it
to confirm a tournament save actually carries `tournament_slots`, and to
eyeball the slot fields. For the cross-target read side, load the same file
on the 32- and 64-bit builds with SWOS_SAVE_FORMAT_JSON_VERIFY.

Usage:
    scripts/decode_save.py <save-file> [<save-file> ...]
    scripts/decode_save.py --diff <save-a> <save-b>     # compare slot fields
    scripts/decode_save.py --standings <save-file> ...  # name-resolved league/
                                                        # group tables only

The --standings view resolves each row's stored team identity (-1 careerTeam,
else selected_teams index) to a team name and prints the league/group table the
way the in-game screen shows it — for eyeballing an SDL save against an Amiga
(big-endian) load during cross-target verification.

Career (.CAR) saves carry a `comp_slots` object with up to five named slots
(primary/secondary/tertiary/diy/euro). Preset/DIY (.PRE/.DIY) saves carry a
single top-level `comp_slot`. Both are handled.
"""
import json
import sys

TYPE_NAMES = {0: "league", 1: "cup", 2: "TOURNAMENT(hybrid)"}

# DIYTournamentSlot fields as emitted by emit_comp_slot (game_types.h order).
SLOT_FIELDS = [
    "double_count", "is_league", "seeding", "num_teams", "division_factor",
    "play_each_team", "init_168", "groups", "teams_per_group", "format_16e",
    "init_170", "format_172", "matches_per_slot",
]

# Career comp_slots sub-keys, in slot order.
CAREER_SLOT_KEYS = ["primary", "secondary", "tertiary", "diy", "euro"]


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    return json.loads(data.decode("utf-8", "replace"))


def collect_slots(doc):
    """Return [(label, slot_obj), ...] for every comp slot in the save."""
    out = []
    if isinstance(doc.get("comp_slot"), dict):          # preset / DIY
        out.append(("comp_slot", doc["comp_slot"]))
    cs = doc.get("comp_slots")
    if isinstance(cs, dict):                            # career
        for k in CAREER_SLOT_KEYS:
            if isinstance(cs.get(k), dict):
                out.append((k, cs[k]))
    return out


def fmt_name(slot):
    n = slot.get("name", "")
    return n.strip() if isinstance(n, str) else ""


def slot_is_populated(ts):
    """A tournament_slots entry is 'empty' if every field is 0."""
    return any(int(ts.get(f, 0)) != 0 for f in SLOT_FIELDS)


def print_slot(label, slot):
    t = slot.get("type")
    tname = TYPE_NAMES.get(t, "?")
    name = fmt_name(slot)
    nteams = slot.get("num_teams")
    active = slot.get("is_active")
    compid = slot.get("comp_id")
    flag = "  <<< TYPE-2" if t == 2 else ""
    print(f"  [{label}] type={t} ({tname})  active={active}  "
          f"comp_id={compid}  num_teams={nteams}  name={name!r}{flag}")

    tids = slot.get("team_ids")
    if isinstance(tids, list):
        print(f"        team_ids: {len(tids)} bytes "
              f"({len(tids)//2} ids){' (empty)' if not tids else ''}")
    if "matchdata_raw" in slot:
        md = "present"
    elif t == 2:
        md = "absent (expected — type-2 rides the flat blob)"
    else:
        md = "absent (older save / not emitted)"
    print(f"        matchdata_raw: {md}")

    if t == 2:
        ts_list = slot.get("tournament_slots")
        if not isinstance(ts_list, list):
            print("        tournament_slots: !!! MISSING — save predates the "
                  "type-2 fix, or build lacks it")
            return
        npop = sum(1 for ts in ts_list if isinstance(ts, dict)
                   and slot_is_populated(ts))
        print(f"        tournament_slots: {len(ts_list)} records "
              f"({npop} populated)")
        # Compact per-slot table.
        hdr = "          #  " + "  ".join(f"{f}" for f in SLOT_FIELDS)
        print(hdr)
        for i, ts in enumerate(ts_list):
            if not isinstance(ts, dict):
                continue
            vals = "  ".join(f"{int(ts.get(f, 0))}" for f in SLOT_FIELDS)
            mark = "" if slot_is_populated(ts) else "  (empty)"
            print(f"          {i}  {vals}{mark}")


def print_stage(doc):
    """For a type-2 (.PRE/.DIY) save, print the current-STAGE slot and the
    phase-specific payload that resolves group names / knockout scores.

    The stage is the second DIYCompetitionEx (cfb+0x420): a league during the
    group phase (emits `stage_standings`), a cup during the knockout (emits
    `stage_score_tab/agg/pen`). The phase is keyed off WHICH payload was
    emitted, not the stage header type (which reads 0 for both)."""
    st = doc.get("comp_slot_stage")
    if not isinstance(st, dict):
        return
    has_standings = isinstance(doc.get("stage_standings"), list)
    has_scores = any(isinstance(doc.get(k), list) for k in
                     ("stage_score_tab", "stage_score_agg", "stage_score_pen"))
    phase = ("GROUP (league)" if has_standings else
             "KNOCKOUT (cup)" if has_scores else "?? (no stage payload)")
    print(f"  [comp_slot_stage] type={st.get('type')} "
          f"num_teams={st.get('num_teams')} "
          f"team_entry_offsets={len(st.get('team_entry_offsets', []) or [])} "
          f"team_offset_table={len(st.get('team_offset_table', []) or [])}  "
          f"=> PHASE: {phase}")

    if has_standings:
        rows = doc["stage_standings"]
        # Heuristic garbage flag: a clean group row has small counts; a
        # byte-swapped/overlay-corrupt row shows huge values (e.g. 768 = swap 3).
        def garbage(r):
            return any(int(r.get(f, 0)) > 200 for f in
                       ("gp", "w", "d", "l", "gf", "ga", "pts"))
        bad = [i for i, r in enumerate(rows) if isinstance(r, dict) and garbage(r)]
        print(f"        stage_standings: {len(rows)} rows"
              + (f"   !!! {len(bad)} look like garbage: rows {bad}" if bad
                 else "   (all stats in sane range)"))
        print("          row  gp   w   d   l  gf  ga pts")
        for i, r in enumerate(rows):
            if not isinstance(r, dict):
                continue
            mark = "  <<< GARBAGE" if i in bad else ""
            print("          %3d %3d %3d %3d %3d %3d %3d %3d%s" % (
                i, r.get("gp", 0), r.get("w", 0), r.get("d", 0),
                r.get("l", 0), r.get("gf", 0), r.get("ga", 0),
                r.get("pts", 0), mark))
    elif has_scores:
        for k in ("stage_score_tab", "stage_score_agg", "stage_score_pen"):
            arr = doc.get(k)
            if isinstance(arr, list):
                # Packed home/away byte pair per entry; show non-zero only.
                nz = [(i, v >> 8, v & 0xFF) for i, v in enumerate(arr) if v]
                pretty = " ".join(f"{i}:{h}-{a}" for i, h, a in nz) or "(all 0)"
                print(f"        {k}: {pretty}")


def build_name_resolver(doc):
    """Return resolve(identity) -> team-name string, mirroring diy_resolve_team:
    identity -1 == careerTeam, >= 0 == index into the selected_teams array.
    This is what turns a standings row's stored team identity into the name the
    Amiga league/group screen prints, so the two can be eyeballed side by side."""
    sel = doc.get("selected_teams")
    sel = sel if isinstance(sel, list) else []

    def team_name(entry):
        if isinstance(entry, dict):
            t = entry.get("team", entry)
            if isinstance(t, dict):
                return (t.get("team_name") or "").strip()
        return ""

    names = [team_name(e) for e in sel]
    ct = doc.get("career_team")
    career_name = team_name(ct) if isinstance(ct, dict) else ""

    def resolve(identity):
        try:
            i = int(identity)
        except (TypeError, ValueError):
            return f"?id={identity!r}"
        if i < 0:
            return f"{career_name or '(career team)'} *"
        if 0 <= i < len(names):
            return names[i] or f"(sel #{i})"
        return f"(sel #{i}?)"

    return resolve


def _row_int(r, *keys):
    for k in keys:
        if k in r:
            return int(r.get(k, 0) or 0)
    return 0


def _print_table(title, rows, resolve, id_keys, note="", sort=False):
    """rows: list of dicts with gp/w/d/l/gf/ga/pts and an id field (id_keys).

    Saved rows are in storage (alphabetical/entry) order; the in-game league
    screen sorts by points > goal-diff > goals-for > name (see
    _DIY_SortLeagueTableByStandings). sort=True mirrors that so a single-table
    league lines up row-for-row with the Amiga screen. Group comps keep storage
    order (a global sort across multiple groups would be misleading) — compare
    those by team name."""
    decorated = []
    for r in rows:
        if not isinstance(r, dict):
            continue
        ident = next((r[k] for k in id_keys if k in r), None)
        decorated.append((r, resolve(ident if ident is not None else 0)))
    if sort:
        decorated.sort(key=lambda rn: (
            -_row_int(rn[0], "pts"),
            -(_row_int(rn[0], "gf") - _row_int(rn[0], "ga")),
            -_row_int(rn[0], "gf"),
            rn[1].rstrip(" *")))
    print(f"\n  {title}{('   ' + note) if note else ''}")
    print("    Pos  Team                       GP   W   D   L  GF  GA  GD Pts")
    for pos, (r, name) in enumerate(decorated, 1):
        gf, ga = _row_int(r, "gf"), _row_int(r, "ga")
        print("    %3d  %-24s %3d %3d %3d %3d %3d %3d %+4d %3d" % (
            pos, name[:24], _row_int(r, "gp"), _row_int(r, "w"),
            _row_int(r, "d"), _row_int(r, "l"), gf, ga, gf - ga,
            _row_int(r, "pts")))


def print_standings_named(doc):
    """Human-readable standings for cross-target (Amiga screen) comparison.
    Covers all three Stage-2 paths: primary domestic LEAGUE
    (competition_overrides.league_rows), primary national GROUP
    (stage_standings), and per-context club-cup GROUP
    (<ctx>_comp_slot_stage_standings)."""
    resolve = build_name_resolver(doc)
    printed = False

    # --- Scenario 1: primary domestic league ---
    co = doc.get("competition_overrides")
    if isinstance(co, dict) and co.get("type") == 0 and isinstance(
            co.get("league_rows"), list):
        rows = co["league_rows"]
        prim = (doc.get("comp_slots") or {}).get("primary") or {}
        n = prim.get("num_teams")
        if not isinstance(n, int) or n <= 0:
            n = sum(1 for r in rows if isinstance(r, dict) and (
                _row_int(r, "gp") or _row_int(r, "id") < 0))
        cname = fmt_name(prim) or "league"
        _print_table(f"LEAGUE TABLE — {cname}  (primary, type-0)",
                     rows[:n], resolve, ("id",),
                     note="* = your career team", sort=True)
        printed = True

    # --- Scenario 2: primary national tournament group phase ---
    if isinstance(doc.get("stage_standings"), list):
        rows = doc["stage_standings"]
        live = [r for r in rows if isinstance(r, dict) and (
            _row_int(r, "gp") or _row_int(r, "pts") or _row_int(r, "gf"))]
        _print_table("GROUP STANDINGS — primary tournament (type-2)",
                     live or rows, resolve, ("so", "id"),
                     note="row i -> selected_teams[i]")
        printed = True

    # --- Context type-0 league (e.g. national FRIENDLY mini-league) ---
    for key in sorted(doc):
        if not key.endswith("_league_rows"):
            continue
        rows = doc.get(key)
        if not isinstance(rows, list):
            continue
        ctx = key[: -len("_league_rows")]
        # Only the first num_teams rows are this league's teams (the rest are
        # stale slot residue, same as the primary's 64-row array).
        slot = (doc.get("comp_slots") or {}).get(ctx) or {}
        n = slot.get("num_teams")
        shown = rows[:n] if isinstance(n, int) and n > 0 else rows
        _print_table(f"LEAGUE TABLE — {ctx} context (type-0)",
                     shown, resolve, ("id",),
                     note="* = your career team", sort=True)
        printed = True

    # --- Scenario 3: per-context club-cup group phase ---
    for key in sorted(doc):
        if not key.endswith("_comp_slot_stage_standings"):
            continue
        rows = doc.get(key)
        if not isinstance(rows, list):
            continue
        live = [r for r in rows if isinstance(r, dict) and (
            _row_int(r, "gp") or _row_int(r, "pts") or _row_int(r, "gf"))]
        ctx = key[: -len("_comp_slot_stage_standings")]
        _print_table(f"GROUP STANDINGS — {ctx} context (type-2 club cup)",
                     live or rows, resolve, ("so", "id"),
                     note="* = your career team")
        printed = True

    if not printed:
        print("\n  (no standings tables in this save — comp at season start, "
              "or a knockout phase with no league/group table)")


def _team_label(entry):
    """`team_name (coach NAME)` from a {team:{...}} or bare team {...} entry."""
    if not isinstance(entry, dict):
        return "?"
    t = entry.get("team", entry)
    if not isinstance(t, dict):
        return "?"
    name = (t.get("team_name") or "").strip() or "(none)"
    coach = (t.get("coach_name") or "").strip()
    return name + (f" (coach {coach})" if coach else "")


def _count_populated(lst):
    """Count entries with any truthy scalar field — a rough 'in use' count for
    the fixed-size tables (offers/market/reserves) that pad with empty records."""
    if not isinstance(lst, list):
        return 0
    n = 0
    for e in lst:
        if isinstance(e, dict):
            if any(v for v in e.values() if not isinstance(v, (list, dict))):
                n += 1
        elif e:
            n += 1
    return n


def print_career_state(doc):
    """Concise summary of the non-competition career state, so every top-level
    key in a .CAR is surfaced (nothing invisible). Summary level matches the
    rest of the inspector — counts + key fields, not a full field dump.
    All keys are presence-guarded, so this is a no-op on .PRE/.DIY/.SEA."""
    if "career_team" in doc or "current_team" in doc:
        print(f"  career_team:  {_team_label(doc.get('career_team'))}")
        print(f"  current_team: {_team_label(doc.get('current_team'))}")
    if doc.get("primary_country_code") is not None:
        print(f"  primary_country_code={doc.get('primary_country_code')}")

    teams = [f"{lbl}={len(doc[k])}" for k, lbl in
             (("selected_teams", "selected"), ("tmd_teams", "tmd"))
             if isinstance(doc.get(k), list)]
    if teams:
        print("  teams: " + "  ".join(teams))
    for k in ("career_team_reserves", "current_team_reserves",
              "career_foreign_market_players"):
        if isinstance(doc.get(k), list):
            print(f"  {k}: {len(doc[k])} ({_count_populated(doc[k])} populated)")
    for k in ("transfer_offer_table", "new_job_offers", "job_offers"):
        if isinstance(doc.get(k), list):
            print(f"  {k}: {len(doc[k])} ({_count_populated(doc[k])} active)")

    if isinstance(doc.get("season_informations"), list):
        print(f"  season_informations: {len(doc['season_informations'])} rows")
    if isinstance(doc.get("career_scalars"), dict):
        print(f"  career_scalars: {len(doc['career_scalars'])} fields")
    if isinstance(doc.get("competition_overrides"), dict):
        print(f"  competition_overrides: type={doc['competition_overrides'].get('type')}")
    if isinstance(doc.get("comp_copy_teamlists"), list):
        ct = doc["comp_copy_teamlists"]
        nonempty = sum(1 for a in ct if isinstance(a, list) and a)
        print(f"  comp_copy_teamlists: {len(ct)} records ({nonempty} non-empty)")
    if isinstance(doc.get("comp_copy_dynamic"), dict):
        print(f"  comp_copy_dynamic: {doc['comp_copy_dynamic']}")
    cache = [f"{k}={len(doc[k])}" for k in ("cache_strides", "cache_scores")
             if isinstance(doc.get(k), list)]
    if cache:
        print("  " + "  ".join(cache))


def summarise(path):
    doc = load(path)
    print(f"\n=== {path} ===")
    print(f"  schema_version={doc.get('schema_version')}  "
          f"file_kind={doc.get('file_kind')!r}  "
          f"num_selected_teams={doc.get('num_selected_teams')}  "
          f"num_diy_teams={doc.get('num_diy_teams')}")
    slots = collect_slots(doc)
    if not slots:
        print("  (no comp_slot / comp_slots — older save, or non-competition file)")
        return
    type2 = [lbl for lbl, s in slots if s.get("type") == 2]
    for label, slot in slots:
        print_slot(label, slot)
    print_stage(doc)
    print_standings_named(doc)
    print_career_state(doc)
    if type2:
        print(f"  => TYPE-2 tournament present in: {', '.join(type2)}")
    else:
        print("  => no type-2 tournament slot in this save")


def diff(path_a, path_b):
    """Compare the tournament_slots / key header fields of two saves."""
    da, db = load(path_a), load(path_b)
    sa = dict(collect_slots(da))
    sb = dict(collect_slots(db))
    keys = [k for k in (list(sa) + [x for x in sb if x not in sa])]
    print(f"\n=== diff {path_a}  vs  {path_b} ===")
    for k in keys:
        a, b = sa.get(k), sb.get(k)
        if a is None or b is None:
            print(f"  [{k}] present in only one save")
            continue
        hdr_fields = ["type", "num_teams", "is_active", "comp_id"]
        for hf in hdr_fields:
            if a.get(hf) != b.get(hf):
                print(f"  [{k}] {hf}: {a.get(hf)} != {b.get(hf)}")
        ta, tb = a.get("tournament_slots"), b.get("tournament_slots")
        if isinstance(ta, list) and isinstance(tb, list):
            for i in range(max(len(ta), len(tb))):
                ra = ta[i] if i < len(ta) else {}
                rb = tb[i] if i < len(tb) else {}
                for f in SLOT_FIELDS:
                    if int(ra.get(f, 0)) != int(rb.get(f, 0)):
                        print(f"  [{k}] slot {i} {f}: "
                              f"{ra.get(f)} != {rb.get(f)}")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    if argv[1] == "--diff":
        if len(argv) != 4:
            print("usage: decode_save.py --diff <save-a> <save-b>")
            return 2
        diff(argv[2], argv[3])
        return 0
    if argv[1] == "--standings":
        if len(argv) < 3:
            print("usage: decode_save.py --standings <save-file> [...]")
            return 2
        for p in argv[2:]:
            try:
                print(f"\n=== {p} ===")
                print_standings_named(load(p))
            except Exception as e:  # noqa: BLE001 - inspector, report and continue
                print(f"  ERROR: {e}")
        return 0
    for p in argv[1:]:
        try:
            summarise(p)
        except Exception as e:  # noqa: BLE001 - inspector, report and continue
            print(f"\n=== {p} ===\n  ERROR: {e}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
