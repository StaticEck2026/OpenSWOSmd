#!/usr/bin/env python3
"""JSON -> binary emitter for the SWOS team data — the inverse of the decode_data.py
--dump-json export. Regenerates the RNC-compressed bin/data/ files from
bin/data_json/teams/*.json:

  team -> TEAM.NNN   (RNC v1)
  tmd  -> *.TMD      (RNC v2: EUROCUP/EUROCWC/UEFACUP)
  pool -> POOLPLYR.DAT (RNC v2)

The split player fields (skills, position/face, cards) are recombined by encode_player,
and rnc_pack reproduces the original RNC compression byte-for-byte. By default this VERIFIES
1:1 against the originals (no write); `--write` overwrites bin/data/ with the regenerated
files.
"""
import json, os, sys, glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swos_team_file as tf

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JSON_DIR = os.path.join(REPO, "bin", "data_json", "teams")
DATA_DIR = os.path.join(REPO, "bin", "data")


def main():
    write = "--write" in sys.argv
    files = sorted(glob.glob(os.path.join(JSON_DIR, "*.json")))
    n = ok = bad = miss = 0
    by_type = {}
    for f in files:
        d = json.load(open(f))
        if "type" not in d:
            continue
        n += 1
        raw = tf.dict_to_blob(d)
        packed = tf.rnc_pack(raw, tf.dict_rnc_method(d))
        name = tf.dict_filename(d)
        orig_path = os.path.join(DATA_DIR, name)
        by_type[d["type"]] = by_type.get(d["type"], 0) + 1
        if not os.path.exists(orig_path):
            miss += 1
            print("  MISSING original: %s" % name)
        elif open(orig_path, "rb").read() == packed:
            ok += 1
        else:
            bad += 1
            orig = open(orig_path, "rb").read()
            print("  DIFF %-16s orig=%dB packed=%dB" % (name, len(orig), len(packed)))
        if write:
            open(orig_path, "wb").write(packed)

    print("%d files (%s) -> %d 1:1 MATCH, %d differ, %d missing%s"
          % (n, ", ".join("%d %s" % (v, k) for k, v in sorted(by_type.items())),
             ok, bad, miss, "  [WROTE bin/data/]" if write else ""))
    return bad + miss


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
