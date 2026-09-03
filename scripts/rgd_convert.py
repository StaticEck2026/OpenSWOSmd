#!/usr/bin/env python3
"""Decode swos-port .RGD recorded-game files (see docs/TESTS.md).

The recording is a packed dump of the DOS port's structs. Our own structs have
diverged (pointer growth, the untangle), so the C replay rig must never parse
these bytes directly — this decoder turns a recording into NAMED FIELDS, which
the rig then compares against our globals one at a time.

Layout is little-endian, #pragma pack(1). Sizes were derived from the C++
definitions and then CONFIRMED against the corpus: for every file,
(filesize - header) % frame == 0 and the frameNo dword runs 0,1,2,3,...

    Header      3610      Header, versions 1.0/1.1
    HeaderV1p2  3617      + trainingGameCareerSize, season, min/maxSubstitutes
    FrameV1p0   7404      base frame, all versions
      + 71   v1.3   BenchDataV1  (shirtNumberTable is 2 x 16)
      +  6   v1.4   userKey, fireBlocked, statsTimer, resultTimer
      +  4   v1.5   pl1/pl2TapTimeoutCounter

The component sizes that make 7404 add up — their TeamGame 1704, TeamStatsData
14, Sprite 110 — are cross-checked by build_frame_layout() below.

Usage:
    scripts/rgd_convert.py --info tests/dos/*.RGD.gz
    scripts/rgd_convert.py --verify tests/dos/zkz-12065730.RGD.gz
    scripts/rgd_convert.py --dump-frames 0:5 tests/dos/zkz-12065730.RGD.gz
"""
import argparse, gzip, struct, sys, os

# ---- struct sizes (theirs; see docs/TESTS.md section 4) --------------------
TEAM_FILE        = 684    # header 76 + 16 players x 38  == our TeamFileEx
TEAM_TACTICS     = 370
TEAM_GENERAL     = 145
TEAM_GAME        = 1704   # ours is its first 1018 bytes (before unknownTail[686])
TEAM_STATS       = 14
SPRITE           = 110    # == our 32-bit Sprite
NUM_SPRITES      = 33

HEADER_BASE      = 3610
HEADER_V1P2      = 3617
FRAME_BASE       = 7404
BENCH_V1         = 71


def header_size(major, minor):
    return HEADER_V1P2 if (major, minor) >= (1, 2) else HEADER_BASE


def frame_size(major, minor):
    n = FRAME_BASE
    if (major, minor) >= (1, 3): n += BENCH_V1
    if (major, minor) >= (1, 4): n += 6
    if (major, minor) >= (1, 5): n += 4
    return n


def build_frame_layout():
    """Field offsets within a frame, and the arithmetic proving they reach 7404."""
    off, lay = 0, []
    def f(name, size):
        nonlocal off
        lay.append((name, off, size)); off += size
    f("frameNo", 4); f("player1Controls", 2); f("player2Controls", 2)
    f("topTeam", TEAM_GENERAL); f("topTeamPlayers", 11)
    f("bottomTeam", TEAM_GENERAL); f("bottomTeamPlayers", 11)
    f("topTeamGame", TEAM_GAME); f("bottomTeamGame", TEAM_GAME)
    f("topTeamStats", TEAM_STATS); f("bottomTeamStats", TEAM_STATS)
    f("cameraX", 4); f("cameraY", 4)
    f("cameraXVelocity", 1); f("cameraYVelocity", 1)
    f("gameTime", 4)
    f("team1Goals", 1); f("team2Goals", 1); f("gameState", 1); f("gameStatePl", 1)
    f("sprites", SPRITE * NUM_SPRITES)
    assert off == FRAME_BASE, f"frame layout is {off}, expected {FRAME_BASE}"
    return dict((n, (o, s)) for n, o, s in lay)


LAYOUT = build_frame_layout()

CONTROL_BITS = [(1,"up"),(2,"down"),(4,"left"),(8,"right"),(32,"fire"),(64,"bench"),(128,"esc")]


def load(path):
    raw = open(path, "rb").read()
    return gzip.decompress(raw) if path.endswith(".gz") else raw


def read_header(d):
    major, minor, fixed_random = d[0], d[1], d[2]
    pts, pt, gl, ar, ape = struct.unpack_from("<5b", d, 3)
    h = dict(major=major, minor=minor, fixedRandomValue=fixed_random,
             pitchTypeOrSeason=pts, pitchType=pt, gameLength=gl,
             autoReplays=ar, allPlayerTeamsEqual=ape,
             topTeamName=cstr(d, 8 + 5, 17), bottomTeamName=cstr(d, 8 + TEAM_FILE + 5, 17),
             headerSize=header_size(major, minor), frameSize=frame_size(major, minor))
    base = 8 + 2 * TEAM_FILE
    (h["topTeamCoachNo"], h["topTeamPlayerNo"],
     h["bottomTeamCoachNo"], h["bottomTeamPlayerNo"],
     h["extraTime"], h["penaltyShootout"]) = struct.unpack_from("<6B", d, base)
    h["inputControls"] = struct.unpack_from("<H", d, base + 8 + 6 * TEAM_TACTICS)[0]
    h["initialFrame"]  = struct.unpack_from("<I", d, base + 10 + 6 * TEAM_TACTICS)[0]
    if (major, minor) >= (1, 2):
        h["trainingGameCareerSize"] = struct.unpack_from("<I", d, HEADER_BASE)[0]
        h["season"], h["minSubstitutes"], h["maxSubstitutes"] = struct.unpack_from("<3b", d, HEADER_BASE + 4)
    return h


def cstr(d, off, n):
    return d[off:off+n].split(b"\0")[0].decode("latin1", "replace").strip()


def frame_count(d, h):
    return (len(d) - h["headerSize"]) // h["frameSize"]


def read_frame(d, h, i):
    base = h["headerSize"] + i * h["frameSize"]
    g = lambda n: LAYOUT[n][0] + base
    gt = d[g("gameTime"):g("gameTime")+4]
    fr = dict(
        frameNo=struct.unpack_from("<I", d, g("frameNo"))[0],
        player1Controls=struct.unpack_from("<h", d, g("player1Controls"))[0],
        player2Controls=struct.unpack_from("<h", d, g("player2Controls"))[0],
        cameraX=struct.unpack_from("<i", d, g("cameraX"))[0],
        cameraY=struct.unpack_from("<i", d, g("cameraY"))[0],
        cameraXVelocity=struct.unpack_from("<b", d, g("cameraXVelocity"))[0],
        cameraYVelocity=struct.unpack_from("<b", d, g("cameraYVelocity"))[0],
        gameTimeDigits=list(gt),
        gameTimeMinutes=gt[3] + 10 * gt[2] + 100 * gt[1],
        team1Goals=d[g("team1Goals")], team2Goals=d[g("team2Goals")],
        gameState=d[g("gameState")], gameStatePl=d[g("gameStatePl")],
    )
    if (h["major"], h["minor"]) >= (1, 4):
        o = base + FRAME_BASE + BENCH_V1
        fr["userKey"], fr["fireBlocked"] = d[o], d[o+1]
        fr["statsTimer"], fr["resultTimer"] = struct.unpack_from("<HH", d, o + 2)
    return fr


def controls_str(v):
    return "".join(n[0] for bit, n in CONTROL_BITS if v & bit) or "-"


def cmd_info(paths):
    print(f"{'file':26s} {'ver':>4s} {'hdr':>5s} {'frame':>6s} {'frames':>7s} {'rem':>4s}  teams")
    for p in paths:
        d = load(p); h = read_header(d)
        rem = (len(d) - h["headerSize"]) % h["frameSize"]
        print(f"{os.path.basename(p):26s} {h['major']}.{h['minor']:<2d} "
              f"{h['headerSize']:>5} {h['frameSize']:>6} {frame_count(d,h):>7} {rem:>4}  "
              f"{h['topTeamName']} v {h['bottomTeamName']}"
              + ("   <-- REMAINDER, layout wrong" if rem else ""))


def cmd_verify(paths):
    """Sanity-check a decode: the invariants a correct layout must satisfy."""
    allok = True
    for p in paths:
        d = load(p); h = read_header(d); n = frame_count(d, h)
        rem = (len(d) - h["headerSize"]) % h["frameSize"]
        probs, prev_t = [], None
        if rem: probs.append(f"trailing {rem} bytes")
        step = max(1, n // 400)
        for i in range(0, n, step):
            f = read_frame(d, h, i)
            if f["frameNo"] != i + read_frame(d, h, 0)["frameNo"]:
                probs.append(f"frame {i}: frameNo={f['frameNo']}"); break
            if not 0 <= f["gameTimeMinutes"] <= 200: probs.append(f"frame {i}: time {f['gameTimeMinutes']}")
            if f["team1Goals"] > 30 or f["team2Goals"] > 30: probs.append(f"frame {i}: goals")
            if f["gameTimeDigits"][0] != 0: probs.append(f"frame {i}: gameTime[0]!=0")
            if prev_t is not None and f["gameTimeMinutes"] < prev_t: probs.append(f"frame {i}: time went backwards")
            prev_t = f["gameTimeMinutes"]
            if len(probs) > 3: break
        last = read_frame(d, h, n - 1)
        ok = not probs
        allok &= ok
        print(f"  {os.path.basename(p):26s} {'OK  ' if ok else 'FAIL'} "
              f"{n:>6} frames  final {last['gameTimeMinutes']:>3}min "
              f"{last['team1Goals']}-{last['team2Goals']}"
              + ("   " + "; ".join(probs[:3]) if probs else ""))
    return 0 if allok else 1


def cmd_dump(paths, rng):
    a, _, b = rng.partition(":")
    a, b = int(a), int(b or a)
    for p in paths:
        d = load(p); h = read_header(d)
        print(f"== {os.path.basename(p)}  v{h['major']}.{h['minor']}  "
              f"{h['topTeamName']} v {h['bottomTeamName']}  "
              f"pitch={h['pitchType']} len={h['gameLength']} rnd={h['fixedRandomValue']}")
        for i in range(a, min(b + 1, frame_count(d, h))):
            f = read_frame(d, h, i)
            print(f"  #{f['frameNo']:<6} p1={controls_str(f['player1Controls']):<7} "
                  f"p2={controls_str(f['player2Controls']):<7} "
                  f"cam=({f['cameraX']:>7},{f['cameraY']:>7}) v=({f['cameraXVelocity']:>3},"
                  f"{f['cameraYVelocity']:>3}) {f['gameTimeMinutes']:>3}min "
                  f"{f['team1Goals']}-{f['team2Goals']} st={f['gameState']}/{f['gameStatePl']}")


def main():
    ap = argparse.ArgumentParser(description="Decode swos-port .RGD recordings")
    ap.add_argument("files", nargs="+")
    ap.add_argument("--info", action="store_true", help="header + frame geometry")
    ap.add_argument("--verify", action="store_true", help="check decode invariants")
    ap.add_argument("--dump-frames", metavar="A[:B]", help="print decoded frames A..B")
    a = ap.parse_args()
    if a.dump_frames: cmd_dump(a.files, a.dump_frames); return 0
    if a.verify:      return cmd_verify(a.files)
    cmd_info(a.files); return 0


if __name__ == "__main__":
    sys.exit(main())
