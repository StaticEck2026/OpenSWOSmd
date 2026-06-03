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
    scripts/decode_save.py --diff <save-a> <save-b>   # compare slot fields

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
    for p in argv[1:]:
        try:
            summarise(p)
        except Exception as e:  # noqa: BLE001 - inspector, report and continue
            print(f"\n=== {p} ===\n  ERROR: {e}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
