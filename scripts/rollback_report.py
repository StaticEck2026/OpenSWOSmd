#!/usr/bin/env python3
"""Summarise rollback rig reports (docs/ROLLBACK.md).

    scripts/rollback_report.py REPORT.json [REPORT.json ...]

Reports of one mode are merged by symbol (name + source file), so runs over
different recordings and both gameplays add up to one table.

discover  Classifies every symbol a window wrote, by what leaving it out of a
          rollback did:
            GAME          changed the game state in at least one window
            OTHER         changed other state, never the game (presentation, audio)
            INERT         tested, changed nothing
          and lists where the evidence and src/sim_state.c disagree: GAME symbols
          missing from the registry, and registered symbols no window needed.
scan      The union of symbols written, largest change count first.
check     Registry misses per symbol, merged.
"""
import json
import sys


def load(paths):
    reports = []
    for p in paths:
        with open(p) as f:
            r = json.load(f)
        r["_path"] = p
        if "mode" not in r and "frames" in r:
            r["mode"] = "scan"          # reports written before the field existed
        reports.append(r)
    modes = {r.get("mode") for r in reports}
    if len(modes) != 1:
        sys.exit("rollback_report: mix of report modes %s" % sorted(modes))
    return reports, modes.pop()


def key(s):
    return (s["name"], s["file"])


def label(s):
    return s["name"] + ("  [%s]" % s["file"] if s["file"] else "")


def discover(reports):
    merged = {}
    windows = tests = invalid = 0
    for r in reports:
        windows += r["windows"]
        tests += r["tests"]
        invalid += r["invalid_windows"]
        for s in r["symbols"]:
            m = merged.setdefault(key(s), dict(s, tested=0, affects_game=0, affects_any=0, crashed=0,
                                               runs=0, game_runs=0, examples=set()))
            m["tested"] += s["tested"]
            m["affects_game"] += s["affects_game"]
            m["affects_any"] += s["affects_any"]
            m["crashed"] += s.get("crashed", 0)
            m["runs"] += 1
            m["game_runs"] += 1 if s["affects_game"] else 0
            if s["example_game"]:
                m["examples"].add(s["example_game"])
            m["registered"] = s["registered"]

    groups = {"GAME": [], "OTHER": [], "INERT": []}
    for m in merged.values():
        if m["affects_game"]:
            groups["GAME"].append(m)
        elif m["affects_any"]:
            groups["OTHER"].append(m)
        else:
            groups["INERT"].append(m)

    print("%d report(s): %d windows, %d leave-one-out tests, %d invalid window(s)"
          % (len(reports), windows, tests, invalid))
    if invalid:
        print("  !! invalid windows: the whole-data re-simulation did not reproduce -- "
              "the other results are not trustworthy")
    reg = {0: "no", 1: "part", 2: "yes"}
    for name, rows in groups.items():
        rows.sort(key=lambda m: (-m["affects_game"], -m["affects_any"], m["name"]))
        size = sum(m["size"] for m in rows)
        print("\n%s: %d symbols, %d bytes" % (name, len(rows), size))
        print("  %-44s %7s %4s %6s %6s %6s %5s  %s" % ("symbol", "size", "reg", "tested", "game", "any",
                                                      "crash", "game changed via"))
        for m in rows:
            print("  %-44s %7d %4s %6d %6d %6d %5d  %s"
                  % (label(m)[:44], m["size"], reg[m["registered"]], m["tested"],
                     m["affects_game"], m["affects_any"], m["crashed"],
                     ", ".join(sorted(m["examples"]))[:60]))

    missing = [m for m in groups["GAME"] if m["registered"] != 2]
    unneeded = [m for m in merged.values() if m["registered"] and not m["affects_game"]]
    print("\nGAME symbols not (fully) in the registry: %d, %d bytes"
          % (len(missing), sum(m["size"] for m in missing)))
    for m in sorted(missing, key=lambda m: m["name"]):
        print("  " + label(m))
    print("\nregistered symbols no window needed: %d, %d bytes"
          % (len(unneeded), sum(m["size"] for m in unneeded)))
    for m in sorted(unneeded, key=lambda m: m["name"]):
        print("  %-44s tested %d" % (label(m), m["tested"]))


def scan(reports):
    merged = {}
    for r in reports:
        for s in r["symbols"]:
            m = merged.setdefault(key(s), dict(s, frames_changed=0, runs=0))
            m["frames_changed"] += s["frames_changed"]
            m["runs"] += 1
    rows = sorted(merged.values(), key=lambda m: (-m["runs"], -m["frames_changed"]))
    print("%d report(s), %d symbols written" % (len(reports), len(rows)))
    for m in rows:
        flags = ("volatile " if m.get("volatile") else "") + ("driver " if m.get("driver") else "")
        print("  %-44s %7d  runs %2d  frames %7d  reg %d  %s"
              % (label(m)[:44], m["size"], m["runs"], m["frames_changed"], m["registered"], flags))


def check(reports):
    merged = {}
    for r in reports:
        print("%s: %s %d windows, %d failed" % (r["_path"], r["mode"], r["checks"], r["failures"]))
        for s in r["symbols"]:
            m = merged.setdefault(key(s), dict(s, diverged_windows=0, changed_unrestored_windows=0))
            m["diverged_windows"] += s["diverged_windows"]
            m["changed_unrestored_windows"] += s["changed_unrestored_windows"]
    rows = sorted(merged.values(), key=lambda m: -m["diverged_windows"])
    for m in rows:
        if m.get("volatile") or m.get("driver"):
            continue
        print("  %-44s %7d  reg %d  diverged %5d  unrestored %5d"
              % (label(m)[:44], m["size"], m["registered"], m["diverged_windows"],
                 m["changed_unrestored_windows"]))


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        return
    reports, mode = load(sys.argv[1:])
    {"discover": discover, "scan": scan, "check": check, "oracle": check}[mode](reports)


if __name__ == "__main__":
    main()
