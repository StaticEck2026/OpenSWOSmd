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

# Current key renames (old -> new). Keep in sync with src/save_format_json.c.
RENAMES = {
    "unkn":      "reserved",    # JobOffer
    "unk1":      "reserved1",   # TeamGame
    "unk3":      "reserved3",   # TeamGame
    "unk_table": "pair_ids",    # TeamTactics
}


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


def migrate_file(path, renames, dry):
    data = open(path, "rb").read()
    orig = data
    hits = {}
    for old, new in renames.items():
        pat = b'"' + old.encode() + b'":'
        rep = b'"' + new.encode() + b'":'
        n = data.count(pat)
        if n:
            data = data.replace(pat, rep)
            hits[old] = n
        # Safety: warn if the NEW key already exists distinctly (possible clash /
        # already-migrated file being re-run with a different mapping).
        if old != new and data.count(b'"' + new.encode() + b'":') and not n:
            pass
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
    renames = pairs or RENAMES

    print(f"{'DRY-RUN: ' if dry else ''}renaming save keys: " +
          ", ".join(f'{o}->{n}' for o, n in renames.items()))
    files = save_files(dirs)
    print(f"scanning {len(files)} JSON save file(s) across {len(dirs)} folder(s)\n")

    total = {o: 0 for o in renames}
    changed = 0
    for p in files:
        hits = migrate_file(p, renames, dry)
        if hits:
            changed += 1
            for o, n in hits.items():
                total[o] += n
            print(f"  {os.path.relpath(p):55s} " +
                  ", ".join(f"{o}->{renames[o]} x{c}" for o, c in hits.items()))
    print(f"\n{'would change' if dry else 'changed'} {changed} file(s). "
          "Per-key totals: " +
          ", ".join(f"{o}={total[o]}" for o in renames if total[o]) or "(none)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
