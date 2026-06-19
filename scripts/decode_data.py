#!/usr/bin/env python3
"""decode_data — CLI to decode the SWOS bin/data team/player files.

Built on the swos_team_file library.  Handles every record-based data file:
TEAM.<country> (country databases), *.TMD (European cup pools), and
POOLPLYR.DAT (pool players).  Decompresses (RNC v1/v2), parses, and can emit
JSON — the foundation for the data->JSON migration (docs/DATA_JSON.md).

Examples:
    decode_data.py 33                       # Scotland, pretty-print
    decode_data.py bin/data/EUROCUP.TMD     # a European-cup TMD
    decode_data.py bin/data/POOLPLYR.DAT    # pool players
    decode_data.py 33 33                    # one team (Scotland ordinal 33)
    decode_data.py 33 --json                # JSON for the whole file
    decode_data.py --all --names            # every data file, names only
    decode_data.py --verify                 # round-trip every data file
    decode_data.py --dump-json              # export the whole corpus to data_json/
"""
import argparse
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swos_team_file as tf


def _all_targets():
    d = tf._data_dir()
    return (sorted(glob.glob(os.path.join(d, "TEAM.[0-9][0-9][0-9]")))
            + sorted(glob.glob(os.path.join(d, "*.TMD")))
            + sorted(glob.glob(os.path.join(d, "POOLPLYR.DAT"))))


def _skills(s):
    return "P%d V%d H%d Ta%d Co%d Sp%d F%d" % (
        s["passing"], s["velocity"], s["heading"],
        s["tackling"], s["control"], s["speed"], s["finishing"])


def _print_player(i, p, indent="    "):
    print("%s%2s %-3s #%-2d %-18s %s  price=%d" % (
        indent, i, p["position"], p["shirt"], p["name"], _skills(p["skills"]), p["price"]))


def print_team(t, full=True):
    print("[%d.%d] %-17s coach=%s tactics=%d league=%d  (%s)" % (
        t["country"], t["ordinal"], t["name"], t["coach"],
        t["tactics"], t["league"], tf.country_name(t["country"])))
    if full:
        for i, p in enumerate(t["players"]):
            _print_player(i, p)


def print_doc(d, full=True):
    if d["type"] in ("team", "tmd"):
        for t in d["teams"]:
            print_team(t, full)
    elif d["type"] == "pool":
        for i, pp in enumerate(d["players"]):
            print("[%d] teamId=%d ord=%d %-16s" % (i, pp["teamId"], pp["playerOrdinal"], pp["teamName"]))
            if full:
                _print_player("", pp["player"], indent="      ")


def names_of(d):
    if d["type"] in ("team", "tmd"):
        return ["%3d: %s" % (t["ordinal"], t["name"]) for t in d["teams"]]
    return ["%3d: %-16s %s" % (i, pp["teamName"], pp["player"]["name"])
            for i, pp in enumerate(d["players"])]


def out_name(target):
    """JSON output filename for a target (country name for TEAM, basename otherwise)."""
    ft = tf.file_type(target)
    if ft == "team":
        c = int(target) if (isinstance(target, int) or str(target).isdigit()) \
            else int(os.path.basename(str(target)).split(".")[1])
        return tf.country_name(c) + ".json"
    return os.path.splitext(os.path.basename(str(target)))[0] + ".json"


def cmd_verify():
    total = 0
    for p in _all_targets():
        blob = tf._read_blob(p)
        re = tf.reencode(p)
        n = sum(1 for a, b in zip(blob, re) if a != b) + abs(len(blob) - len(re))
        total += n
        print("  %-22s %s" % (os.path.basename(p), "OK" if n == 0 else "MISMATCH %d" % n))
    print("TOTAL mismatched bytes: %d across %d files" % (total, len(_all_targets())))
    return 0 if total == 0 else 1


def cmd_dump_json(out_dir):
    # resolve a relative dir against the repo root (parent of bin/data's parent)
    if not os.path.isabs(out_dir):
        repo = os.path.dirname(os.path.dirname(tf._data_dir()))
        out_dir = os.path.join(repo, out_dir)
    os.makedirs(out_dir, exist_ok=True)
    n = 0
    for p in _all_targets():
        doc = tf.file_to_dict(p)
        dest = os.path.join(out_dir, out_name(p))
        with open(dest, "w") as f:
            json.dump(doc, f, indent=2, ensure_ascii=False)
        n += 1
    print("wrote %d JSON files to %s" % (n, out_dir))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Decode SWOS bin/data team/player files.")
    ap.add_argument("target", nargs="?", help="country number or path to a data file")
    ap.add_argument("ordinal", nargs="?", type=int, help="optional team ordinal (TEAM/TMD)")
    ap.add_argument("--json", action="store_true", help="emit JSON")
    ap.add_argument("--names", action="store_true", help="compact name list")
    ap.add_argument("--all", action="store_true", help="sweep every bin/data file")
    ap.add_argument("--verify", action="store_true", help="round-trip every file (binary->dict->binary)")
    ap.add_argument("--dump-json", metavar="DIR", nargs="?", const="data_json", default=None,
                    help="export every file as JSON to DIR (default: data_json/, relative to repo root)")
    args = ap.parse_args(argv)

    if args.verify:
        return cmd_verify()
    if args.dump_json:
        return cmd_dump_json(args.dump_json)

    if args.all:
        targets = _all_targets()
        if args.json:
            print(json.dumps([tf.file_to_dict(p) for p in targets], indent=2, ensure_ascii=False))
        elif args.names:
            for p in targets:
                print("== %s ==" % os.path.basename(p))
                print("\n".join("  " + s for s in names_of(tf.file_to_dict(p))))
        else:
            for p in targets:
                print_doc(tf.file_to_dict(p), full=False)
        return 0

    if not args.target:
        ap.print_help()
        return 1

    doc = tf.file_to_dict(args.target)

    if args.names:
        print("\n".join(names_of(doc)))
        return 0

    if args.ordinal is not None and doc["type"] in ("team", "tmd"):
        doc = {**doc, "teams": [t for t in doc["teams"] if t["ordinal"] == args.ordinal]}

    if args.json:
        payload = doc
        if args.ordinal is not None and doc["type"] in ("team", "tmd"):
            payload = (doc["teams"][0] if doc["teams"] else None)
        print(json.dumps(payload, indent=2, ensure_ascii=False))
        return 0

    print_doc(doc, full=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:          # e.g. piping to `head`
        try:
            sys.stdout.close()
        except Exception:
            pass
        sys.exit(0)
