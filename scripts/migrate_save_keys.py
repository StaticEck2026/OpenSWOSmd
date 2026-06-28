#!/usr/bin/env python3
"""Rename JSON keys across existing SWOS save files (SDL + Amiga save folders).

When a save-format JSON key is renamed in src/save_format_json.c (because the
underlying C field was renamed), run this to update existing saves in place so
they keep loading — instead of carrying the old key name forever for back-compat.

The SWOS JSON saves (.CAR/.SEA/.PRE/.DIY) are plain JSON with no length/offset
header, so the rename is a targeted byte replace of the key token

    "<old>":   ->   "<new>":

The trailing colon anchors the match to OBJECT KEYS only, never to string values
(a value "<old>" is followed by , or }, never :). Everything else in the file is
byte-for-byte preserved, so the diff is exactly the renamed keys. Key-length
changes are fine (no length field). Binary saves (.TAC/.HIL/.EDT/.CFG) are NOT
touched — those are fixed-layout records, not JSON.

Usage:
  migrate_save_keys.py                 # apply RENAMES below to both save folders
  migrate_save_keys.py --dry-run       # show what WOULD change, write nothing
  migrate_save_keys.py old=new [...]   # ad-hoc renames instead of RENAMES
  migrate_save_keys.py --dir PATH ...  # extra save folder(s) to include

Edit RENAMES to match the current save-key change, then run it.
"""
import sys, os, glob

# Save folders: SDL bin/save + the Amiga FS-UAE virtual-HD save dir.
_HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_DIRS = [
    os.path.normpath(os.path.join(_HERE, "..", "bin", "save")),
    "/mnt/c/Projects/AI/SWOS/SWOS_C/uae/dh0/data/save",
]
# JSON save types only (NOT .TAC/.HIL/.EDT/.CFG — those are binary records).
SAVE_EXTS = ("CAR", "SEA", "PRE", "DIY")

# Current simple key renames (old -> new), applied as "<old>": -> "<new>":.
# Use this for keys that are UNIQUE to one struct. Keep in sync with
# src/save_format_json.c.
#   (prior migration, already applied: unkn->reserved, unk1->reserved1,
#    unk3->reserved3 [TeamGame/JobOffer], unk_table->pair_ids [TeamTactics])
RENAMES = {
    "field_1":   "cached_match_value",     # PlayerFile (player-only key)
    "some_flag": "player_values_dirty",    # TeamFile (player one already -> form via RAW below)
    "comp_id":   "comp_slot",              # DIYCompetition slot-word key (field compId -> compSlot)
}

# Context-ANCHORED renames, for keys SHARED by more than one struct where a blind
# "<old>": replace would hit the wrong one. Each entry is (find_bytes, repl_bytes).
# These run BEFORE the simple RENAMES above, so the shared key is disambiguated
# first and the remaining blind rename only sees the other struct's key.
# PlayerFile.form was "some_flag", but TeamFile ALSO had a "some_flag" key. The
# player one is the LAST field of a player object, after goal_stats
# ( ...],"some_flag":N} ); the team one follows coach_name ( ...","some_flag":N ).
# Anchor on the '],' prefix so the player key -> "form" first; then the simple
# RENAMES "some_flag" -> "player_values_dirty" only hits the team key.
RAW_RENAMES = [
    (b'],"some_flag":', b'],"form":'),   # PlayerFile.form (NOT TeamFile)
]


def save_files(dirs):
    out = []
    for d in dirs:
        if not os.path.isdir(d):
            print(f"  (skip missing folder: {d})")
            continue
        for ext in SAVE_EXTS:
            out += glob.glob(os.path.join(d, f"*.{ext}"))
            out += glob.glob(os.path.join(d, f"*.{ext.lower()}"))
    return sorted(set(out))


def migrate_file(path, renames, dry, raw=()):
    data = open(path, "rb").read()
    orig = data
    hits = {}
    # Anchored renames FIRST (disambiguate shared keys before the blind pass).
    for find, repl in raw:
        n = data.count(find)
        if n:
            data = data.replace(find, repl)
            hits[find.decode("latin1")] = n
    for old, new in renames.items():
        pat = b'"' + old.encode() + b'":'
        rep = b'"' + new.encode() + b'":'
        n = data.count(pat)
        if n:
            data = data.replace(pat, rep)
            hits[old] = n
    if data != orig and not dry:
        with open(path, "wb") as f:
            f.write(data)
    return hits


def main(argv):
    args = argv[1:]
    dry = "--dry-run" in args
    args = [a for a in args if a != "--dry-run"]
    dirs = list(DEFAULT_DIRS)
    pairs, i = {}, 0
    while i < len(args):
        if args[i] == "--dir":
            dirs.append(args[i + 1]); i += 2; continue
        if "=" in args[i]:
            o, n = args[i].split("=", 1); pairs[o] = n
        i += 1
    # Ad-hoc old=new args override RENAMES and skip the default anchored renames.
    renames = pairs or RENAMES
    raw = () if pairs else RAW_RENAMES

    desc = [f'{o}->{n}' for o, n in renames.items()] + \
           [f'{f.decode("latin1")} (anchored)' for f, _ in raw]
    print(f"{'DRY-RUN: ' if dry else ''}renaming save keys: " + ", ".join(desc))
    files = save_files(dirs)
    print(f"scanning {len(files)} JSON save file(s) across {len(dirs)} folder(s)\n")

    total = {}
    changed = 0
    for p in files:
        hits = migrate_file(p, renames, dry, raw)
        if hits:
            changed += 1
            for o, n in hits.items():
                total[o] = total.get(o, 0) + n
            print(f"  {os.path.relpath(p):55s} " +
                  ", ".join(f"{o} x{c}" for o, c in hits.items()))
    print(f"\n{'would change' if dry else 'changed'} {changed} file(s). "
          "Per-key totals: " +
          (", ".join(f"{o}={c}" for o, c in total.items()) or "(none)"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
