#!/usr/bin/env python3
"""Compare the sound effects a netplay run played with a run that never rolled back.

    netplay_sound_compare.py <reference.snd> <run.snd> exact|superset

Each line of a trace (--netplay-sound-trace, src/netplay_match.c) is one effect played:
`frame kind channel id`, with ` late` added when a rollback started it.

  exact     the run must play exactly the reference's effects (synctest: every re-simulated
            frame is a repeat of one already heard, so nothing may be added)
  superset  every reference effect must be played, on its frame; extras are allowed (two
            peers: a frame played on a wrong prediction started effects the corrected
            frame does not, and they cannot be taken back once heard)

Prints one summary line; exit 0 on success, 2 on failure.
"""
import sys
from collections import Counter


def load(path):
    effects, late = Counter(), 0
    with open(path) as f:
        for line in f:
            fields = line.split()
            if len(fields) < 4:
                continue
            effects[tuple(fields[:4])] += 1
            late += len(fields) > 4
    return effects, late


def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ("exact", "superset"):
        print(__doc__.strip(), file=sys.stderr)
        return 1
    ref, _ = load(sys.argv[1])
    run, late = load(sys.argv[2])
    missing = ref - run
    extra = run - ref
    n_missing, n_extra = sum(missing.values()), sum(extra.values())
    ok = n_missing == 0 and (sys.argv[3] == "superset" or n_extra == 0)
    what = "sound %s: %d effects, %d missing, %d extra, %d late" % (
        "ok" if ok else "WRONG", sum(run.values()), n_missing, n_extra, late)
    if not ok:
        first = sorted(missing or extra, key=lambda e: int(e[0]))[0]
        what += " (first: frame %s kind %s channel %s id %s %s)" % (
            first + ("missing" if missing else "extra",))
    print(what)
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
