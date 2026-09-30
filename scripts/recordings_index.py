#!/usr/bin/env python3
"""recordings_index.py -- who played whom, for the playback page (docs/PLAYBACK.md).

The recorder writes a CARD beside every recording it keeps -- <name>.json: the players'
names, the teams, the score, their builds, how it ended -- and index.json, every card in
the folder newest first, which /playback/ lists (third_party/matchmaker/Server/
Recorder.cpp, WriteMatchCard). This does the same for recordings made before the
recorder wrote cards, from the ledger's `match` lines, and rewrites the index.

    python3 scripts/recordings_index.py                  # the server's own folders
    python3 scripts/recordings_index.py --recordings DIR --ledger FILE

A card the recorder wrote is never replaced -- except to fill in a player's NAME it lacked.
A player who does not record ([netplay] record off, or an older build) never streams, so
older cards have only one side. The name comes from the other player's HELLO (the ledger's
`peer`, since the recorder keeps it) or from the match report's rating.txt, which names
both players (reports/<match id>/player<N>.zip). Addresses are never written: the page is
public, and it says who played whom, not where from. Stdlib only.
"""
import argparse
import json
import os
import re
import struct
import sys
import zipfile

DEFAULT_DIR = "/srv/swos-matchmaker"
SRP_META_SCORE_OFF = 2 + 2 + 2 + 2 + 4 + 32     # include/srp_writer.h


META_TEAMS_OFF = SRP_META_SCORE_OFF + 4 + 2 + 2 + 2 + 40 + 40     # the two teams, 1018 bytes each
TEAM_BYTES, TEAM_NAME_OFF = 1018, 22


def srp_meta(path):
    """(teams, score, frames) from a .SRP's own header and META, or None."""
    try:
        with open(path, "rb") as f:
            head = f.read(32)
            if len(head) < 32 or head[:4] != b"SWRP":
                return None
            frames = struct.unpack(">I", head[16:20])[0]
            n = struct.unpack(">H", head[20:22])[0]
            dirs = f.read(16 * n)
            for i in range(n):
                fourcc, off, stored, raw = struct.unpack(">4sIII", dirs[i * 16:i * 16 + 16])
                if fourcc == b"META" and stored == raw and raw >= META_TEAMS_OFF + 2 * TEAM_BYTES:
                    f.seek(off)
                    m = f.read(raw)
                    teams = []
                    for k in range(2):
                        b = m[META_TEAMS_OFF + k * TEAM_BYTES + TEAM_NAME_OFF:][:17]
                        teams.append(b.split(b"\0")[0].decode("latin-1").strip())
                    return teams, list(struct.unpack(">HH", m[SRP_META_SCORE_OFF:SRP_META_SCORE_OFF + 4])), frames
    except OSError:
        pass
    return None


def card_from_srp(recordings, file, reports):
    """A card from the recording alone, for one the ledger has no line for (the server
    stopped before it closed the match, say): its teams, score and length; the players'
    names only if a match report for it came in."""
    meta = srp_meta(os.path.join(recordings, file))
    if not meta:
        return None
    teams, score, frames = meta
    m = re.match(r"^(\d{4})(\d{2})(\d{2})-(\d{2})(\d{2})(\d{2})-([0-9a-f]{8})\.SRP$", file, re.I)
    match = m.group(7) if m else ""
    if match and reports and os.path.isdir(reports):         # the report's folder has the full id
        full = [d for d in os.listdir(reports) if d.startswith(match)]
        if len(full) == 1:
            match = full[0]
    card = {"file": file, "match": match,
            "started": "%s-%s-%sT%s:%s:%s" % m.groups()[:6] if m else "",
            "teams": teams, "score": score, "players": [None, None], "frames": frames,
            "gameplay": 0, "relayed": False, "desync": None, "why": "no ledger line"}
    fill_names(card, None, reports)
    return card


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


def names_from_report(reports, match):
    """{1: name, 2: name} from a match report's rating.txt, or {}."""
    folder = os.path.join(reports, match) if reports and match else ""
    for z in ("player1.zip", "player2.zip"):
        try:
            with zipfile.ZipFile(os.path.join(folder, z)) as f:
                text = f.read("rating.txt").decode("utf-8", "replace")
        except (OSError, KeyError, zipfile.BadZipFile):
            continue
        names = {int(n): v.strip() for n, v in re.findall(r"^player ([12]) +(.+?)(?: \(host\))?\r?$",
                                                        text, re.M)}
        names = {k: v for k, v in names.items() if v and v != "?"}
        if names:
            return names
    return {}


def fill_names(card, ledger_line, reports):
    """Give a card's empty player slots a name. True if it changed."""
    players = card.get("players") or [None, None]
    while len(players) < 2:
        players.append(None)
    missing = [i for i in range(2) if not players[i] or not players[i].get("name")]
    if not missing:
        return False
    found = {}
    lp = (ledger_line or {}).get("players") or [None, None]
    for i in range(2):
        other = lp[1 - i] if len(lp) > 1 - i else None
        if other and other.get("peer"):
            found[i + 1] = other["peer"]
    if len(found) < len(missing):
        for k, v in names_from_report(reports, card.get("match", "")).items():
            found.setdefault(k, v)
    changed = False
    for i in missing:
        if found.get(i + 1):
            players[i] = {"player": i + 1, "name": found[i + 1], "recorded": False}
            changed = True
    card["players"] = players
    return changed


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


def backfill(recordings, ledger, reports):
    """Cards for recordings the ledger names and that have none, and names for the cards
    missing one. Returns (made, named)."""
    made = named = 0
    by_file = {}
    if not os.path.isfile(ledger):
        ledger = None
    with open(ledger, encoding="utf-8", errors="replace") if ledger else open(os.devnull) as f:
        for line in f:
            try:
                m = json.loads(line)
            except ValueError:
                continue
            if m.get("kind") != "match" or not m.get("recording"):
                continue
            file = os.path.basename(m["recording"])
            by_file[file] = m
            srp = os.path.join(recordings, file)
            card_path = srp[:-4] + ".json"
            if not file.upper().endswith(".SRP") or not os.path.isfile(srp) or os.path.exists(card_path):
                continue
            card = card_from_ledger(m, file)
            card["score"] = srp_score(srp)
            fill_names(card, m, reports)
            write(card_path, json.dumps(card, separators=(",", ":")) + "\n")
            made += 1
    # Recordings the ledger never closed: a card from the recording itself.
    for name in os.listdir(recordings):
        if not name.upper().endswith(".SRP") or name in by_file:
            continue
        card_path = os.path.join(recordings, name[:-4] + ".json")
        if os.path.exists(card_path):
            continue
        card = card_from_srp(recordings, name, reports)
        if card:
            write(card_path, json.dumps(card, separators=(",", ":")) + "\n")
            made += 1
    # Cards already there (the recorder's, or an earlier run's) with a player missing.
    for name in os.listdir(recordings):
        if not name.endswith(".json") or name == "index.json":
            continue
        path = os.path.join(recordings, name)
        try:
            with open(path, encoding="utf-8") as f:
                card = json.load(f)
        except (OSError, ValueError):
            continue
        if fill_names(card, by_file.get(card.get("file", "")), reports):
            write(path, json.dumps(card, separators=(",", ":")) + "\n")
            named += 1
    return made, named


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
    ap.add_argument("--reports", default=os.path.join(DEFAULT_DIR, "reports"),
                    help="match reports, for players' names a card lacks")
    args = ap.parse_args()
    if not os.path.isdir(args.recordings):
        sys.exit("recordings_index: no folder %s" % args.recordings)
    remove_orphans(args.recordings)
    made, named = backfill(args.recordings, args.ledger, args.reports)
    n = write_index(args.recordings)
    print("recordings_index: %d new card(s) from the ledger, %d named from reports or the ledger; "
          "index.json lists %d match(es)" % (made, named, n))


if __name__ == "__main__":
    main()
