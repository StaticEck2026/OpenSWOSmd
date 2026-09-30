#!/usr/bin/env python3
"""recordings_index.py -- who played whom, for the playback page (docs/PLAYBACK.md).

The recorder writes a CARD beside every recording it keeps -- <name>.json: the players'
names, the teams, the score, their builds, how it ended -- and index.json, every card in
the folder newest first, which /playback/ lists (third_party/matchmaker/Server/
Recorder.cpp, WriteMatchCard). This does the same for recordings made before the
recorder wrote cards, from the ledger's `match` lines, and rewrites the index.

    python3 scripts/recordings_index.py                  # the server's own folders
    python3 scripts/recordings_index.py --recordings DIR --ledger FILE

A card the recorder wrote is never replaced. Addresses are never written: the page is
public, and it says who played whom, not where from. Stdlib only.
"""
import argparse
import json
import os
import struct
import sys

DEFAULT_DIR = "/srv/swos-matchmaker"
SRP_META_SCORE_OFF = 2 + 2 + 2 + 2 + 4 + 32     # include/srp_writer.h


def srp_score(path):
    """The final score in a .SRP's META, or None."""
    try:
        with open(path, "rb") as f:
            head = f.read(32)
            if len(head) < 32 or head[:4] != b"SWRP":
                return None
            n = struct.unpack(">H", head[20:22])[0]
            dirs = f.read(16 * n)
            for i in range(n):
                fourcc, off, stored, raw = struct.unpack(">4sIII", dirs[i * 16:i * 16 + 16])
                if fourcc == b"META" and stored == raw and raw >= SRP_META_SCORE_OFF + 4:
                    f.seek(off + SRP_META_SCORE_OFF)
                    return list(struct.unpack(">HH", f.read(4)))
    except OSError:
        pass
    return None


def card_from_ledger(m, file):
    def player(p):
        if not p:
            return None
        return {"player": p.get("player"), "name": p.get("name", ""), "build": p.get("build", ""),
                "platform": p.get("platform", ""), "outcome": p.get("outcome", "none")}
    players = m.get("players") or [None, None]
    frames = max([(p or {}).get("replayFrames", 0) for p in players] + [0])
    desync = m.get("desync")
    return {"file": file, "match": m.get("match", ""), "started": m.get("started", ""),
            "teams": m.get("teams") or ["", ""], "score": None,
            "players": [player(p) for p in players[:2]], "frames": frames,
            "gameplay": m.get("gameplay", 0), "relayed": bool(m.get("relay")),
            "desync": desync.get("frame") if isinstance(desync, dict) else None,
            "why": m.get("why", "")}


def write(path, text):
    tmp = path + ".part"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(text)
    os.replace(tmp, path)


def backfill(recordings, ledger):
    """Cards for recordings the ledger names and that have none. Returns how many."""
    made = 0
    if not os.path.isfile(ledger):
        return 0
    with open(ledger, encoding="utf-8", errors="replace") as f:
        for line in f:
            try:
                m = json.loads(line)
            except ValueError:
                continue
            if m.get("kind") != "match" or not m.get("recording"):
                continue
            file = os.path.basename(m["recording"])
            srp = os.path.join(recordings, file)
            card_path = srp[:-4] + ".json"
            if not file.upper().endswith(".SRP") or not os.path.isfile(srp) or os.path.exists(card_path):
                continue
            card = card_from_ledger(m, file)
            card["score"] = srp_score(srp)
            write(card_path, json.dumps(card, separators=(",", ":")) + "\n")
            made += 1
    return made


def write_index(recordings):
    """index.json: every card whose .SRP is still there, newest first. Returns the count."""
    cards = []
    for name in sorted(os.listdir(recordings), reverse=True):
        if not name.endswith(".json") or name == "index.json":
            continue
        if not os.path.isfile(os.path.join(recordings, name[:-5] + ".SRP")):
            continue
        try:
            with open(os.path.join(recordings, name), encoding="utf-8") as f:
                text = f.read().strip()
            json.loads(text)
        except (OSError, ValueError):
            continue
        cards.append(text)
    write(os.path.join(recordings, "index.json"), "[\n" + ",\n".join(cards) + "\n]\n")
    return len(cards)


def remove_orphans(recordings):
    """Cards whose .SRP has been cleared."""
    for name in os.listdir(recordings):
        if name.endswith(".json") and name != "index.json" and \
                not os.path.isfile(os.path.join(recordings, name[:-5] + ".SRP")):
            try:
                os.remove(os.path.join(recordings, name))
            except OSError:
                pass


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--recordings", default=os.path.join(DEFAULT_DIR, "recordings"))
    ap.add_argument("--ledger", default=os.path.join(DEFAULT_DIR, "matches.jsonl"))
    args = ap.parse_args()
    if not os.path.isdir(args.recordings):
        sys.exit("recordings_index: no folder %s" % args.recordings)
    remove_orphans(args.recordings)
    made = backfill(args.recordings, args.ledger)
    n = write_index(args.recordings)
    print("recordings_index: %d new card(s) from the ledger; index.json lists %d match(es)" % (made, n))


if __name__ == "__main__":
    main()
