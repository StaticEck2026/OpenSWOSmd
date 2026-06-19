#!/usr/bin/env python3
"""decode_team_file — CLI to decode a SWOS TEAM.xxx file.

Built on the swos_team_file library.  Decompresses (RNC) and parses
TEAM.<country> into teams + players, and can emit JSON (the foundation for the
TEAM->JSON migration, docs/TEAM_XXX_JSON.md).

Examples:
    decode_team_file.py 33                 # Scotland, pretty-print
    decode_team_file.py bin/data/TEAM.008  # by path
    decode_team_file.py 33 33              # one team (Scotland ordinal 33)
    decode_team_file.py 33 --json          # JSON for the whole file
    decode_team_file.py 33 --names         # compact "ord: NAME"
    decode_team_file.py --all --names      # every TEAM.* (names only)
    decode_team_file.py --verify           # round-trip every TEAM.* (binary->dict->binary)
"""
import argparse
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swos_team_file as tf


def _skill_str(s):
    return "P%d V%d H%d Ta%d Co%d Sp%d F%d" % (
        s["passing"], s["velocity"], s["heading"],
        s["tackling"], s["control"], s["speed"], s["finishing"])


def print_team(t, full=True):
    cname = tf.country_name(t["country"])
    print("[%d.%d] %-17s  %-14s coach=%s tactics=%d league=%d  (%s)" % (
        t["country"], t["ordinal"], t["name"], "", t["coach"],
        t["tactics"], t["league"], cname))
    if not full:
        return
    for i, p in enumerate(t["players"]):
        print("    %2d %-3s #%-2d %-18s %s  price=%d" % (
            i, p["position"], p["shirt"], p["name"], _skill_str(p["skills"]), p["price"]))


def cmd_names(country):
    names = tf.team_names(country)
    print("TEAM.%03d  %s  (%d teams)" % (int(country), tf.country_name(country), len(names)))
    for ordn in sorted(names):
        print("  %3d: %s" % (ordn, names[ordn]))


def cmd_verify():
    files = sorted(glob.glob(os.path.join(tf._data_dir(), "TEAM.[0-9][0-9][0-9]")))
    total = 0
    for path in files:
        c = int(path.rsplit(".", 1)[1])
        blob = tf._read_blob(c)
        re = tf.reencode(c)
        n = sum(1 for a, b in zip(blob, re) if a != b) + abs(len(blob) - len(re))
        total += n
        flag = "OK" if n == 0 else ("MISMATCH %d" % n)
        print("  TEAM.%03d  %-14s %s" % (c, tf.country_name(c), flag))
    print("TOTAL mismatched bytes: %d across %d files" % (total, len(files)))
    return 0 if total == 0 else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="Decode a SWOS TEAM.xxx file.")
    ap.add_argument("target", nargs="?", help="country number or path to TEAM.xxx")
    ap.add_argument("ordinal", nargs="?", type=int, help="optional team ordinal")
    ap.add_argument("--json", action="store_true", help="emit JSON")
    ap.add_argument("--names", action="store_true", help="compact ordinal: name list")
    ap.add_argument("--all", action="store_true", help="sweep every TEAM.* file")
    ap.add_argument("--verify", action="store_true", help="round-trip every TEAM.* (binary->dict->binary)")
    args = ap.parse_args(argv)

    if args.verify:
        return cmd_verify()

    if args.all:
        countries = [int(p.rsplit(".", 1)[1])
                     for p in sorted(glob.glob(os.path.join(tf._data_dir(), "TEAM.[0-9][0-9][0-9]")))]
        if args.json:
            print(json.dumps([tf.file_to_dict(c) for c in countries], indent=2, ensure_ascii=False))
        elif args.names:
            for c in countries:
                cmd_names(c)
        else:
            for c in countries:
                for t in tf.load(c):
                    print_team(t, full=False)
        return 0

    if not args.target:
        ap.print_help()
        return 1

    if args.names:
        cmd_names(args.target)
        return 0

    if args.json:
        if args.ordinal is not None:
            teams = [t for t in tf.load(args.target) if t["ordinal"] == args.ordinal]
            print(json.dumps(teams[0] if teams else None, indent=2, ensure_ascii=False))
        else:
            print(json.dumps(tf.file_to_dict(args.target), indent=2, ensure_ascii=False))
        return 0

    teams = tf.load(args.target)
    if args.ordinal is not None:
        teams = [t for t in teams if t["ordinal"] == args.ordinal]
    for t in teams:
        print_team(t, full=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
