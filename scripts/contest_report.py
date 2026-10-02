#!/usr/bin/env python3
"""What each recorded online match decided whenever two players met at the ball.

A recording replays exactly from its inputs (bin/SWOS.x86_64 --srp-resim), so the game can
be asked, after the fact, what it did at every contact: docs/CONTEST_LOG.md. This runs
`--srp-resim=<file> --contest-log=<csv>` for each recording, reads what each player's screen
showed from the recording's own PRED / SEEN, and reports

  - every ROLL for a contested ball: who, the ratings used, the odds, the number drawn, who
    was locked out -- and whether that is what the rule gives for that number;
  - every contact with NO roll, by the rule that decided it;
  - the other three ways a ball changes hands without a roll: a slide tackle that reaches
    it, a dribbler who turns for longer than his ball control allows, a trapped ball;
  - observed against expected, over every recording given.

  scripts/contest_report.py build/recordings/*.SRP
  scripts/contest_report.py <file>.SRP --ranges ranges.txt     # reported frame ranges
  scripts/contest_report.py <file>.SRP --rolls                  # every roll, one a line

ranges.txt: `<match id prefix> <label> <first>-<last>` a line.

The binary must be the build the recording was made on, or one with the same rules and
state layout: a tester recording needs `make X86_64=1 SWOS_TEST_NO_INJURIES_CARDS=1`.
A recording it refuses is listed as skipped, with the game's own reason.
"""
import argparse
import csv
import math
import os
import struct
import subprocess
import sys
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ODDS = [16, 17, 18, 19, 20, 21, 22, 23]       # plAvgTacklingBallControlDiffChance, of 32
REACH2 = 32                                    # "very close": the squared distance a roll needs
FAST = 1536                                    # a fast ball: trapped from "close" (72)


def load_srp(path):
    b = open(path, "rb").read()
    if b[:4] != b"SWRP":
        raise ValueError("not a .SRP")
    count = struct.unpack(">H", b[20:22])[0]
    chunks = {}
    for i in range(count):
        fourcc, off, stored, raw = struct.unpack(">4sIII", b[32 + 16 * i:48 + 16 * i])
        data = b[off:off + stored]
        if stored != raw:
            data = zlib.decompress(data, -15)
        chunks[fourcc.decode()] = data
    frames = struct.unpack(">I", b[16:20])[0]
    names = ["P1", "P2"]
    plyr = chunks.get("PLYR")
    if plyr and len(plyr) > 3:
        at, got = 2, []
        for _ in range(2):
            n = plyr[at]
            got.append(plyr[at + 1:at + 1 + n].decode("latin-1").strip())
            at += 1 + n
        names = [g or d for g, d in zip(got, names)]
    pred = chunks.get("PRED")
    lag = None
    if pred and len(pred) >= 8 + 2 * frames:
        lag = [(pred[8 + 2 * f], pred[9 + 2 * f]) for f in range(frames)]
    seen = {}
    s = chunks.get("SEEN")
    if s and len(s) >= 8:
        n, at = struct.unpack(">I", s[4:8])[0], 8
        for _ in range(n):
            frame, who, objects = struct.unpack(">IBB", s[at:at + 6])
            at += 6
            worst = 0.0
            for _ in range(objects):
                dx, dy, fx, fy = struct.unpack(">hhhh", s[at + 1:at + 9])
                worst = max(worst, math.hypot(dx - fx, dy - fy))
                at += 9
            seen[(frame, who)] = worst
    inputs = chunks.get("INPT")
    return {"frames": frames, "names": names, "lag": lag, "seen": seen, "inputs": inputs,
            "tick": struct.unpack(">H", b[8:10])[0] or 50}


def guess_wrong(srp, frame, player):
    """Was the frame first played on a guess of the other player's input that was wrong?"""
    lag, inputs = srp["lag"], srp["inputs"]
    if not lag or not inputs or lag[frame][player] == 0xFF:
        return None
    n = lag[frame][player] & 15
    if n == 0 or n > frame:
        return False
    other = (1 - player) * 2
    guess = inputs[(frame - n) * 4 + other:(frame - n) * 4 + other + 2]
    return any(inputs[k * 4 + other:k * 4 + other + 2] != guess for k in range(frame + 1 - n, frame + 1))


def screens(srp, frame, after=12):
    """What each player's screen was drawn from around a frame: (lag, wrong guess, px off)."""
    out = []
    for p in (0, 1):
        if not srp["lag"] or frame >= srp["frames"] or srp["lag"][frame][p] == 0xFF:
            out.append(None)
            continue
        off = max((srp["seen"].get((f, p + 1), 0.0) for f in range(frame, min(srp["frames"], frame + after + 1))),
                  default=0.0)
        out.append((srp["lag"][frame][p] & 15, guess_wrong(srp, frame, p), off))
    return out


def run_log(binary, srp_path, out_csv):
    note = out_csv + ".txt"
    if os.path.exists(out_csv) and os.path.exists(note) and \
            os.path.getmtime(out_csv) >= max(os.path.getmtime(srp_path), os.path.getmtime(binary)):
        return None
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    p = subprocess.run([binary, "--contest-log=" + out_csv, "--srp-resim=" + os.path.abspath(srp_path)],
                       cwd=os.path.dirname(binary), env=env, capture_output=True, text=True, timeout=1800)
    lines = [l for l in (p.stdout + p.stderr).splitlines() if l.startswith("[resim]")]
    if not any("every one as recorded" in l for l in lines):
        if os.path.exists(out_csv):
            os.remove(out_csv)
        return lines[-1] if lines else "the game gave no result (exit %d)" % p.returncode
    with open(note, "w") as f:          # the game's own account, the view replay's self-check in it
        f.write("\n".join(lines) + "\n")
    return None


def view_check(out_csv):
    """The view replay's self-check, from the game: [(frames replayed, frames exact, not replayable)] x 2."""
    import re
    try:
        text = open(out_csv + ".txt").read()
    except OSError:
        return None
    m = re.search(r"player 1's game replayed for (\d+) guessed frames, (\d+) put .*?\((\d+) not replayable\); "
                  r"player 2's (\d+), (\d+) \((\d+)\)", text)
    return [tuple(int(x) for x in m.group(1, 2, 3)), tuple(int(x) for x in m.group(4, 5, 6))] if m else None


def read_log(path, views=False):
    """The match's own lines; with views=True, (match lines, {1: player 1's view lines, 2: ...})."""
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        r["frame"] = int(r["frame"])
    match = [r for r in rows if r.get("view", "0") in ("0", "", None)]
    if not views:
        return match
    return match, {v: [r for r in rows if r.get("view") == str(v)] for v in (1, 2)}


def run_views(binary, srp_path, out_csv):
    """The game's own check of the view replay: (frames replayed, frames exact) a player, or None."""
    return None


def compare_views(srp, match, views, near=3):
    """What each player's own game showed at the ball against what the match did.

    A view line is that player's game on the frame it first showed (played on a guess of
    the other's input). For every roll, slide tackle and over-turn the match had, what did
    each player's game show on that frame; and for every one a player's game showed, did
    the match have it."""
    out = {"saw_right": 0, "real_input": 0, "other_outcome": 0, "not_shown": 0, "shifted": 0,
           "phantom": 0, "won_then_lost": 0, "lost_then_won": 0, "events": []}
    kinds = ("roll", "tackle", "turn")

    def locked(r):
        return r["locked"]

    def same_men(a, b):
        return {a["name"], a["opp_name"]} == {b["name"], b["opp_name"]}

    by_frame = {}
    for r in match:
        if r["event"] in kinds:
            by_frame.setdefault(r["frame"], []).append(r)
    for p in (1, 2):
        me = "P%d" % p
        vrows = [r for r in views[p] if r["event"] in kinds]
        vby = {}
        for r in vrows:
            vby.setdefault(r["frame"], []).append(r)
        lag = srp["lag"]
        # 1. every event of the match: what this player's game showed on that frame
        for f, evs in by_frame.items():
            for e in evs:
                if not lag or f >= len(lag) or lag[f][p - 1] == 0xFF:
                    continue
                if (lag[f][p - 1] & 15) == 0:
                    out["real_input"] += 1
                    continue
                shown = [v for v in vby.get(f, []) if v["event"] == e["event"]]
                if shown and any(locked(v) == locked(e) for v in shown):
                    out["saw_right"] += 1
                elif shown:
                    out["other_outcome"] += 1
                    kind = "won_then_lost" if locked(e) == me else "lost_then_won"
                    out[kind] += 1
                    out["events"].append((f, p, "showed %s locked out, the match locked out %s" % (
                        locked(shown[0]), locked(e)), e))
                else:
                    # shown a frame or two early or late in his own game?
                    nearby = [v for g in range(f - near, f + near + 1) for v in vby.get(g, [])
                              if v["event"] == e["event"] and same_men(v, e)]
                    if nearby and any(locked(v) == locked(e) for v in nearby):
                        out["shifted"] += 1
                    elif nearby:
                        out["other_outcome"] += 1
                        kind = "won_then_lost" if locked(e) == me else "lost_then_won"
                        out[kind] += 1
                        out["events"].append((f, p, "showed %s locked out (at frame %d), the match locked out %s" % (
                            locked(nearby[0]), nearby[0]["frame"], locked(e)), e))
                    else:
                        out["not_shown"] += 1
                        out["events"].append((f, p, "did not show it on that frame", e))
        # 2. every event this player's game showed that the match never had near that frame
        last = -99
        for v in vrows:
            f = v["frame"]
            real = [e for g in range(f - near, f + near + 1) for e in by_frame.get(g, [])
                    if e["event"] == v["event"] and same_men(v, e)]
            if not real:
                if f - last > near:         # one a run of frames, not one a frame
                    out["phantom"] += 1
                    out["events"].append((f, p, "showed a %s the match never had" % v["event"], v))
                last = f
    return out


def rating(t, c):
    return (int(t) + int(c)) >> 1


def check_roll(r):
    """The rule, applied to the logged numbers: who must be locked out for this draw?"""
    mine, theirs = int(r["used_who"]), int(r["used_opp"])
    weaker = r["opp"] if mine - theirs >= 0 else r["who"]      # a tie counts against the man on the ball
    stronger = r["who"] if weaker == r["opp"] else r["opp"]
    expect = weaker if int(r["drawn"]) < int(r["odds"]) else stronger
    ok = expect == r["locked"] and int(r["odds"]) == ODDS[abs(mine - theirs)]
    return ok, weaker


def clock(frame, tick):
    s = frame // tick
    return "%d:%02d" % (s // 60, s % 60)


def name_of(srp, tag):
    return srp["names"][0] if tag == "P1" else srp["names"][1] if tag == "P2" else tag


def describe(r, srp):
    """One event as a sentence."""
    a, b = name_of(srp, r["who"]), name_of(srp, r["opp"])
    man = "%s's %s (T%s C%s)" % (a, r["name"], r["tackling"], r["control"])
    opp = "%s's %s (T%s C%s)" % (b, r["opp_name"], r["opp_tackling"], r["opp_control"]) if r["opp_name"] else b
    if r["event"] == "roll":
        ok, weaker = check_roll(r)
        own = rating(r["tackling"], r["control"])
        used = "%s %s" % (a, r["used_who"])
        if own != int(r["used_who"]):
            used += " (read from %s; %s's own is %d)" % (r["used_from"], r["name"], own)
        return ("ROLL between %s and %s. Ratings used: %s, %s %s. The weaker side (%s) is locked out on a draw "
                "under %d of 32; drew %s: %s is locked out%s" % (
                    man, opp, used, b, r["used_opp"], name_of(srp, weaker), int(r["odds"]), r["drawn"],
                    name_of(srp, r["locked"]), "" if ok else "  ** NOT what the rule gives **"))
    if r["event"] == "tackle":
        return "SLIDE TACKLE: %s reaches the ball (%s); %s is locked out, no roll" % (
            man, "taken off " + opp if r["reason"] == "opp-on-ball" else "a loose ball", b)
    if r["event"] == "turn":
        return "TURN: %s turned on the ball for %s steps, his ball control allows %s: he loses it, no roll" % (
            man, r["drawn"], r["odds"])
    if r["event"] == "trap":
        jump = math.sqrt(int(r["jump"]))
        return "TRAP: %s takes a ball moving at %s (%s's), which jumps %.0f px to his feet" % (
            man, r["ball_speed"], name_of(srp, r["from"]) if r["from"] else "nobody", jump)
    return r["event"]


def screen_text(srp, frame):
    out = []
    for p, s in enumerate(screens(srp, frame)):
        if s is None:
            out.append("%s: no data" % srp["names"][p])
        elif s[0] == 0 and s[2] == 0:
            out.append("%s: real input, drawn as played" % srp["names"][p])
        else:
            out.append("%s: %d guessed%s, %s" % (srp["names"][p], s[0], " (wrong)" if s[1] else "",
                                                 "drawn as played" if s[2] == 0 else "up to %.0f px off" % s[2]))
    return "; ".join(out)


def interesting_trap(r):
    return int(r["ball_speed"]) >= FAST and int(r["jump"]) >= 16 and r["from"] and r["from"] != r["who"]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("recordings", nargs="+")
    ap.add_argument("--bin", default=os.path.join(REPO, "bin", "SWOS.x86_64"))
    ap.add_argument("--out", default=os.path.join(REPO, "build", "contest"), help="where the logs are kept")
    ap.add_argument("--ranges", help="reported frame ranges: `<match id prefix> <label> <first>-<last>` a line")
    ap.add_argument("--rolls", action="store_true", help="list every roll")
    ap.add_argument("--views", action="store_true",
                    help="list every moment a player's own game showed something else at the ball")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    ranges = []
    if args.ranges:
        for line in open(args.ranges):
            p = line.split()
            if len(p) >= 3 and not line.startswith("#"):
                a, b = p[-1].split("-")
                ranges.append((p[0], " ".join(p[1:-1]), int(a), int(b)))

    total = {"rolls": 0, "bad": 0, "by_diff": {}, "drawn": [0] * 32, "reasons": {}, "tackles": 0, "turns": 0,
             "traps": 0, "magnets": 0, "frames": 0, "matches": 0, "guessed_rolls": 0, "wrong_rolls": 0,
             "off_rolls": 0, "far_rolls": 0, "own_differs": 0, "own_flips": 0, "episodes": 0, "episodes_rolled": 0,
             "unexplained": 0, "views": {}}
    skipped = []

    for path in args.recordings:
        base = os.path.splitext(os.path.basename(path))[0]
        out_csv = os.path.join(args.out, base + ".csv")
        why = run_log(args.bin, path, out_csv)
        if why:
            skipped.append((base, why))
            continue
        srp = load_srp(path)
        rows, views = read_log(out_csv, views=True)
        rolls = [r for r in rows if r["event"] == "roll"]
        total["matches"] += 1
        total["frames"] += srp["frames"]
        print("## %s  --  %s v %s, %d frames (%s)" % (base, srp["names"][0], srp["names"][1], srp["frames"],
                                                     clock(srp["frames"], srp["tick"])))

        for r in rolls:
            ok, weaker = check_roll(r)
            d = abs(int(r["used_who"]) - int(r["used_opp"]))
            slot = total["by_diff"].setdefault(d, [0, 0])
            slot[0] += 1
            slot[1] += r["locked"] == weaker
            total["rolls"] += 1
            total["bad"] += not ok
            total["drawn"][int(r["drawn"])] += 1
            own = rating(r["tackling"], r["control"])
            if own != int(r["used_who"]):
                total["own_differs"] += 1
                # Would the challenger's OWN rating have named another weaker side or other odds?
                if (own - int(r["used_opp"]) >= 0) != (int(r["used_who"]) - int(r["used_opp"]) >= 0) or \
                        abs(own - int(r["used_opp"])) != d:
                    total["own_flips"] += 1
            sc = screens(srp, r["frame"])
            if any(s and s[0] > 0 for s in sc):
                total["guessed_rolls"] += 1
            if any(s and s[1] for s in sc):
                total["wrong_rolls"] += 1
            if any(s and s[2] > 0 for s in sc):
                total["off_rolls"] += 1
            if any(s and s[2] >= 8 for s in sc):
                total["far_rolls"] += 1

        # Contacts: runs of frames in which the game reached the decision with two men at the ball.
        contact = [r for r in rows if r["event"] in ("roll", "no-roll")]
        episodes, cur = [], []
        for r in contact:
            if cur and r["frame"] - cur[-1]["frame"] > 4:
                episodes.append(cur)
                cur = []
            cur.append(r)
        if cur:
            episodes.append(cur)
        at_ball = 0
        unrolled = []
        for e in episodes:
            reasons = {r["reason"] for r in e if r["event"] == "no-roll"}
            rolled = any(r["event"] == "roll" for r in e)
            # Both within reach at some point: a roll, or a no-roll for any reason but "not within reach".
            if rolled or (reasons - {"not-on-it"}):
                at_ball += 1
                total["episodes"] += 1
                total["episodes_rolled"] += rolled
                if not rolled:
                    unrolled.append((e[0]["frame"], e[-1]["frame"], sorted(reasons - {"not-on-it"})))
                if not rolled and not (reasons - {"not-on-it"}):
                    total["unexplained"] += 1
        for r in rows:
            if r["event"] == "no-roll":
                total["reasons"][r["reason"]] = total["reasons"].get(r["reason"], 0) + 1
        tackles = [r for r in rows if r["event"] == "tackle"]
        turns = [r for r in rows if r["event"] == "turn"]
        traps = [r for r in rows if r["event"] == "trap"]
        magnets = [r for r in traps if interesting_trap(r)]
        total["tackles"] += len(tackles)
        total["turns"] += len(turns)
        total["traps"] += len(traps)
        total["magnets"] += len(magnets)
        bad = [r for r in rolls if not check_roll(r)[0]]
        print("   %d rolls (%d not as the rule gives), %d slide tackles that reached the ball, %d lost on a turn, "
              "%d traps (%d of a fast ball off the other side, jumping 4 px or more); %d contacts with both men "
              "in reach" % (len(rolls), len(bad), len(tackles), len(turns), len(traps), len(magnets), at_ball))

        for a, b, why in unrolled:
            near = [r for r in rows if r["event"] in ("tackle", "turn", "roll") and a - 26 <= r["frame"] <= b]
            print("   contact not rolled for, frames %d-%d: %s%s" % (
                a, b, ", ".join(why), "; just before: " + ", ".join("%s at %d" % (r["event"], r["frame"]) for r in near)
                if near else ""))
        if views[1] or views[2]:
            cv = compare_views(srp, rows, views)
            for k in ("saw_right", "real_input", "other_outcome", "not_shown", "shifted", "phantom",
                      "won_then_lost", "lost_then_won"):
                total["views"][k] = total["views"].get(k, 0) + cv[k]
            total["views"]["matches"] = total["views"].get("matches", 0) + 1
            vc = view_check(out_csv)
            if vc:
                for k, i in (("replayed", 0), ("exact", 1), ("skipped", 2)):
                    total["views"][k] = total["views"].get(k, 0) + vc[0][i] + vc[1][i]
            print("   what each player's own game showed, for the %d rolls, slide tackles and over-turns (x2 "
                  "players): %d on real input, %d on a guess and the same, %d a frame or so apart, %d with the "
                  "OTHER side locked out, %d not shown on that frame; and %d it showed that the match never had" % (
                      len([r for r in rows if r["event"] in ("roll", "tackle", "turn")]), cv["real_input"],
                      cv["saw_right"], cv["shifted"], cv["other_outcome"], cv["not_shown"], cv["phantom"]))
            if args.views:
                for f, p, what, e in sorted(cv["events"], key=lambda t: t[0]):
                    sc = screens(srp, f)[p - 1]
                    print("     %6d  %s's game %s  [%s between %s and %s]%s" % (
                        f, srp["names"][p - 1], what.replace("P1", srp["names"][0]).replace("P2", srp["names"][1]),
                        e["event"], e["name"], e["opp_name"] or "-",
                        "  (%d guessed, then up to %.0f px corrected)" % (sc[0], sc[2]) if sc else ""))
        if args.rolls:
            for r in rolls:
                print("   %6d  %s  %s" % (r["frame"], clock(r["frame"], srp["tick"]), describe(r, srp)))
                print("           screens: %s" % screen_text(srp, r["frame"]))

        mine = [g for g in ranges if base.split("-")[-1].startswith(g[0]) or g[0] in base]
        for _, label, a, b in mine:
            inside = [r for r in rows if a - 6 <= r["frame"] <= b + 6 and
                      (r["event"] in ("roll", "tackle", "turn") or (r["event"] == "trap" and interesting_trap(r)))]
            print("   - reported as %s, frames %d-%d:%s" % (label, a, b, "" if inside else
                                                              "  NOTHING the log explains in this range"))
            for r in inside:
                print("       %d  %s" % (r["frame"], describe(r, srp)))
                print("              screens: %s" % screen_text(srp, r["frame"]))
        print()

    if skipped:
        print("## Skipped (%d)" % len(skipped))
        for base, why in skipped:
            print("   %s: %s" % (base, why))
        print()
    if not total["matches"]:
        return 1

    print("## Over %d matches, %d frames (%.0f minutes of play)" % (
        total["matches"], total["frames"], total["frames"] / 50 / 60))
    print("   rolls: %d, of which %d are NOT what the rule gives for the number drawn" % (total["rolls"], total["bad"]))
    print("   ratings apart | rolls | weaker side locked out | the rule's odds")
    for d in sorted(total["by_diff"]):
        n, lost = total["by_diff"][d]
        print("   %13d | %5d | %4d = %5.1f%%          | %d in 32 = %.1f%%" % (
            d, n, lost, 100.0 * lost / n, ODDS[d], 100.0 * ODDS[d] / 32))
    n = total["rolls"]
    if n:
        exp = sum(ODDS[d] / 32.0 * c[0] for d, c in total["by_diff"].items())
        var = sum(ODDS[d] / 32.0 * (1 - ODDS[d] / 32.0) * c[0] for d, c in total["by_diff"].items())
        got = sum(c[1] for c in total["by_diff"].values())
        print("   weaker side locked out %d times; the odds give %.1f, give or take %.1f" % (got, exp, math.sqrt(var)))
        low = sum(total["drawn"][:16])
        print("   numbers drawn: %d below 16, %d from 16 up (an even split is expected)" % (low, n - low))
        print("   in %d of %d rolls the rating used for one side was a team-mate's and differed from the man's "
              "own (the originals' squad-slot lookup)" % (total["own_differs"], n))
        print("   screens at a roll: %d of %d were first played on a guess by at least one player, %d on a guess "
              "that was wrong; in the 12 frames after, a screen drew something out of place in %d, by 8 px (half "
              "a player) or more in %d" % (
                  total["guessed_rolls"], n, total["wrong_rolls"], total["off_rolls"], total["far_rolls"]))
    print("   contacts with both men in reach of the ball: %d, of which %d were rolled for" % (
        total["episodes"], total["episodes_rolled"]))
    print("   the game's decision at a contact, every time it was asked (one a team step):")
    label = {"not-on-it": "the asker is near but not within reach of the ball",
             "self-locked": "the asker is locked out (he lost a roll, was tackled, or over-turned)",
             "opp-locked": "the other man is locked out: the asker plays the ball freely",
             "opp-not-on-ball": "the other man is not on the ball (fire held, shooting, or out of reach)",
             "opp-still": "the other man holds no direction: the asker takes the ball",
             "opp-nobody": "the other side has no controlled player",
             "ball-high": "the ball is in the air"}
    for k, v in sorted(total["reasons"].items(), key=lambda kv: -kv[1]):
        print("     %7d  no roll: %s" % (v, label.get(k, k)))
    print("     %7d  rolled" % total["rolls"])
    v = total["views"]
    if v.get("matches"):
        shown = v["real_input"] + v["saw_right"] + v["shifted"] + v["other_outcome"] + v["not_shown"]
        print("   what each player's OWN GAME showed at those moments (%d matches with both players' data; each "
              "roll, slide tackle and over-turn counted once a player = %d):" % (v["matches"], shown))
        if v.get("replayed"):
            print("     (each player's game was replayed for the %d frames it first played on a guess; %d of them "
                  "put every man and the ball exactly where the recording says that screen drew them; %d could "
                  "not be replayed)" % (v["replayed"], v["exact"], v.get("skipped", 0)))
        print("     %7d  played on real input: shown as it happened" % v["real_input"])
        print("     %7d  played on a guess, and shown the same" % v["saw_right"])
        print("     %7d  shown the same, a frame or so early or late" % v["shifted"])
        print("     %7d  shown with the OTHER side locked out, then corrected (%d: he saw himself win and lost; "
              "%d: saw himself lose and won)" % (v["other_outcome"], v["won_then_lost"], v["lost_then_won"]))
        print("     %7d  not shown on that frame: it appeared with the correction" % v["not_shown"])
        print("     %7d  shown by a player's game and never in the match (a phantom)" % v["phantom"])
    print("   without a roll: %d slide tackles reached the ball, %d dribblers over-turned, %d traps (%d of a "
          "fast ball off the other side, jumping 4 px or more)" % (
              total["tackles"], total["turns"], total["traps"], total["magnets"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
