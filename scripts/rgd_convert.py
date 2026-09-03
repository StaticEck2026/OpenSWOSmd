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
    scripts/rgd_convert.py --emit tests/replay/kickoff.swr --level scalars \
                           --frames 0:400 tests/dos/zkz-12065730.RGD.gz
"""
import argparse, gzip, struct, sys, os, json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rgd_schema

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



# =========================================================================
# --emit: the neutral, name-keyed format the C rig reads (docs/TESTS.md 5a)
# =========================================================================
#
# Layout (all little-endian):
#
#   0   char magic[8]   "SWOSREC\0"
#   8   u32  fileVersion (1)
#   12  u32  setupOff,  setupLen     JSON, UTF-8 — the match setup
#   20  u32  schemaOff, schemaLen    TSV, one line per per-frame field:
#                                    name <TAB> type <TAB> level <TAB> cmp
#   28  u32  blobOff,   blobLen      the fixed-layout binary records
#   36  u32  framesOff, frameCount, frameSize
#   48  u32  reserved[4]
#   64  ... sections, in the order above
#
# Frames are the schema's fields packed back-to-back in schema order, which
# is why the schema carries the types: the reader never needs to know a DOS
# offset, only a name and a width.
#
# The blob section holds the two 684-byte team records and the six 370-byte
# user tactics. Those are fixed-layout binary records of exactly the kind
# CLAUDE.md keeps binary support for (.EDT / .TAC), and they are setup
# state, not per-frame state — the rig copies each in whole and then
# restores the one endian-sensitive field (TeamFile.globalTeamNumber) from
# the named scalar beside it.

MAGIC = b"SWOSREC\0"
SWR_HEADER_SIZE = 64

PACK = {"u8": "<B", "s8": "<b", "u16": "<H", "s16": "<h", "u32": "<I", "s32": "<i"}
READ = {"u8": "<B", "s8": "<b", "u16": "<H", "s16": "<h", "u32": "<I", "s32": "<i"}


def _team_setup(d, off):
    """The named, endian-sensitive parts of a 684-byte team record."""
    return dict(name=cstr(d, off + 5, 17),
                country_number=d[off + 0], team_ordinal=d[off + 1],
                global_team_number=struct.unpack_from("<h", d, off + 2)[0],
                team_controls=d[off + 4], tactics=d[off + 24])


def cmd_emit(paths, out_path, level, rng, quiet=False):
    if len(paths) != 1:
        print("--emit takes exactly one recording", file=sys.stderr)
        return 1
    d = load(paths[0])
    h = read_header(d)
    ver = (h["major"], h["minor"])
    total = frame_count(d, h)

    first, last = 0, total - 1
    if rng:
        a, _, b = rng.partition(":")
        first = int(a) if a else 0
        last = int(b) if b else total - 1
    first = max(0, first)
    last = min(total - 1, last)
    if last < first:
        print(f"empty frame range {first}:{last}", file=sys.stderr)
        return 1

    schema = rgd_schema.build_schema(h["major"], h["minor"], level)
    frame_size_out = sum(rgd_schema.TYPE_SIZE[f["type"]] for f in schema)

    base = 8 + 2 * TEAM_FILE
    tactics_off = base + 8
    setup = dict(
        source=os.path.basename(paths[0]),
        version=[h["major"], h["minor"]],
        level=level,
        first_frame=first, last_frame=last,
        fixed_random_value=h["fixedRandomValue"],
        pitch_type_or_season=h["pitchTypeOrSeason"],
        pitch_type=h["pitchType"],
        game_length=h["gameLength"],
        auto_replays=h["autoReplays"],
        all_player_teams_equal=h["allPlayerTeamsEqual"],
        # v1.0/1.1 headers stop before these three. Fall back to the DOS data
        # segment's own defaults for the substitute counts (swos-port vm.c:
        # minSubstitutes 2, maxSubstitutes 5) — leaving them at 0 benches
        # players 11..15 before the first frame and diverges immediately.
        # `season` has no equally safe default, so -1 means "not recorded" and
        # the rig keeps this build's value and says so.
        season=h.get("season", -1),
        min_substitutes=h.get("minSubstitutes", 2),
        max_substitutes=h.get("maxSubstitutes", 5),
        training_game_career_size=h.get("trainingGameCareerSize", 0),
        sprite_index_offset=rgd_schema.DOS_SPRITE_INDEX_OFFSET,
        view_width=rgd_schema.DOS_VIEW_WIDTH,
        view_height=rgd_schema.DOS_VIEW_HEIGHT,
        input_controls=h["inputControls"],
        initial_frame=h["initialFrame"],
        extra_time=h["extraTime"],
        penalty_shootout=h["penaltyShootout"],
        top_team_marked_player=struct.unpack_from("<b", d, base + 6)[0],
        bottom_team_marked_player=struct.unpack_from("<b", d, base + 7)[0],
        top_team_coach_no=h["topTeamCoachNo"], top_team_player_no=h["topTeamPlayerNo"],
        bottom_team_coach_no=h["bottomTeamCoachNo"], bottom_team_player_no=h["bottomTeamPlayerNo"],
    )

    blob = bytearray()
    teams = []
    for i, toff in enumerate((8, 8 + TEAM_FILE)):
        t = _team_setup(d, toff)
        t["blob_off"], t["blob_len"] = len(blob), TEAM_FILE
        blob += d[toff:toff + TEAM_FILE]
        teams.append(t)
    setup["teams"] = teams
    tac = []
    for i in range(6):
        o = tactics_off + i * TEAM_TACTICS
        tac.append(dict(name=cstr(d, o, 9), blob_off=len(blob), blob_len=TEAM_TACTICS))
        blob += d[o:o + TEAM_TACTICS]
    setup["tactics"] = tac

    setup_js = json.dumps(setup, indent=1).encode()
    schema_js = "".join("%s\t%s\t%d\t%s\n" % (f["name"], f["type"], f["level"], f["cmp"])
                        for f in schema).encode()

    frames = bytearray()
    for i in range(first, last + 1):
        fb = h["headerSize"] + i * h["frameSize"]
        for f in schema:
            v = struct.unpack_from(READ[f["type"]], d, fb + f["off"])[0]
            frames += struct.pack(PACK[f["type"]], v)

    setup_off = SWR_HEADER_SIZE
    schema_off = setup_off + len(setup_js)
    blob_off = schema_off + len(schema_js)
    frames_off = blob_off + len(blob)

    hdr = bytearray(SWR_HEADER_SIZE)
    hdr[0:8] = MAGIC
    struct.pack_into("<10I", hdr, 8, 1, setup_off, len(setup_js), schema_off,
                     len(schema_js), blob_off, len(blob), frames_off,
                     last - first + 1, frame_size_out)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as fp:
        fp.write(hdr); fp.write(setup_js); fp.write(schema_js)
        fp.write(blob); fp.write(frames)

    if not quiet:
        n = last - first + 1
        cmp_n = sum(1 for f in schema if f["cmp"] in ("eq", "bool"))
        view_n = sum(1 for f in schema if f["cmp"] == "view")
        print(f"{out_path}: v{h['major']}.{h['minor']} {h['topTeamName']} v {h['bottomTeamName']}")
        print(f"  level {level}: {len(schema)} fields/frame ({cmp_n} compared, "
              f"{view_n} view-dependent), {frame_size_out} bytes/frame")
        print(f"  frames {first}..{last} of {total}  ->  "
              f"{(frames_off + len(frames)) / 1048576.0:.1f} MB")
        used = 0
        for i in range(first, last + 1):
            fr = read_frame(d, h, i)
            used |= fr["player1Controls"] | fr["player2Controls"]
        extra = [n for bit, n in CONTROL_BITS if used & bit and n in ("bench", "esc")]
        if extra:
            print(f"  NOTE: range uses {', '.join(extra)} — not injected yet, "
                  f"the rig will diverge there")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Decode swos-port .RGD recordings")
    ap.add_argument("files", nargs="+")
    ap.add_argument("--info", action="store_true", help="header + frame geometry")
    ap.add_argument("--verify", action="store_true", help="check decode invariants")
    ap.add_argument("--dump-frames", metavar="A[:B]", help="print decoded frames A..B")
    ap.add_argument("--emit", metavar="OUT.swr", help="write the neutral format the C rig reads")
    ap.add_argument("--level", default="scalars", choices=rgd_schema.LEVELS,
                    help="how much per-frame state to emit (default: scalars)")
    ap.add_argument("--frames", metavar="A[:B]", help="limit --emit to frames A..B")
    ap.add_argument("-q", "--quiet", action="store_true")
    a = ap.parse_args()
    if a.emit:        return cmd_emit(a.files, a.emit, a.level, a.frames, a.quiet)
    if a.dump_frames: cmd_dump(a.files, a.dump_frames); return 0
    if a.verify:      return cmd_verify(a.files)
    cmd_info(a.files); return 0


if __name__ == "__main__":
    sys.exit(main())
