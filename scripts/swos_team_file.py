#!/usr/bin/env python3
"""swos_team_file — read/write SWOS TEAM.NNN team-data files.

Each bin/data/TEAM.<country> is RNC-ProPack compressed.  Decompressed it is:

    [0..1]   uint16 big-endian   team count N
    [2..]    N * 684-byte TeamFileEx records   (see src/game_types.h)

A TeamFileEx is a 76-byte TeamFile header followed by 16 * 38-byte PlayerFile
records.  A {country, ordinal} TeamId (game_types.h) resolves to a team by
loading TEAM.<country> and finding the record whose teamOrdinal == ordinal.

This module is the parsing/encoding LIBRARY; `decode_team_file.py` is the CLI
util built on top of it.  The parse produces plain JSON-serialisable dicts and
`encode_*` is the faithful inverse, so a TEAM file can be round-tripped
(binary -> dict -> binary) and is the foundation for the TEAM->JSON migration
(see docs/TEAM_XXX_JSON.md).

RNC decompression reuses the existing port (rnc_propack.py).  Set the
RNC_PROPACK_DIR env var if it is not on one of the default search paths.
"""
import os
import sys

# ---- on-disk layout constants (src/game_types.h) ----
TEAM_REC_SIZE = 684          # sizeof(TeamFileEx)
PLAYER_REC_SIZE = 38         # sizeof(PlayerFile)
PLAYERS_PER_TEAM = 16
TEAM_NAME_OFF, TEAM_NAME_LEN = 5, 17
COACH_NAME_OFF, COACH_NAME_LEN = 36, 23
PLAYER_NAME_OFF, PLAYER_NAME_LEN = 3, 22

# positionAndFace high 3 bits -> position label (game_types.h)
POSITIONS = {0: "GK", 1: "RB", 2: "LB", 3: "CD", 4: "RW", 5: "LW", 6: "MF", 7: "ST"}
_POS_REV = {v: k for k, v in POSITIONS.items()}

_RNC_DIRS = [
    os.environ.get("RNC_PROPACK_DIR"),
    "/mnt/c/Projects/Python/SWOS",
    r"C:\Projects\Python\SWOS",
    os.path.dirname(os.path.abspath(__file__)),
]


# ======================================================================
# RNC ProPack (v1) decompression
# ======================================================================
def _import_rnc():
    for d in _RNC_DIRS:
        if d and os.path.isfile(os.path.join(d, "rnc_propack.py")):
            if d not in sys.path:
                sys.path.insert(0, d)
            import rnc_propack
            return rnc_propack
    raise ImportError("rnc_propack.py not found. Set RNC_PROPACK_DIR.")


def rnc_unpack(packed: bytes) -> bytes:
    """Decompress an RNC ProPack v1 blob (mirrors rnc_propack.main method-1 setup)."""
    rnc = _import_rnc()
    v = rnc.Vars()
    v.input = bytearray(packed)
    v.file_size = len(packed)
    v.read_start_offset = v.input_offset = v.output_offset = 0
    v.method = 1
    v.dict_size = 0x8000        # method-1 dictionary clamp
    v.max_matches = 0x1000
    v.output = bytearray(rnc.MAX_BUF_SIZE)
    v.temp = bytearray(rnc.MAX_BUF_SIZE)
    err = rnc.do_unpack(v)
    if err:
        raise ValueError("RNC unpack failed (code %d)" % err)
    return bytes(v.output[: v.output_offset])


def rnc_pack(raw: bytes) -> bytes:
    """Re-compress with RNC ProPack v1 (for writing TEAM files back, later use)."""
    rnc = _import_rnc()
    v = rnc.Vars()
    v.input = bytearray(raw)
    v.file_size = len(raw)
    v.read_start_offset = v.input_offset = v.output_offset = 0
    v.method = 1
    v.dict_size = 0x8000
    v.max_matches = 0x1000
    v.puse_mode = ord("p")
    v.output = bytearray(rnc.MAX_BUF_SIZE)
    v.temp = bytearray(rnc.MAX_BUF_SIZE)
    err = rnc.do_pack(v)
    if err:
        raise ValueError("RNC pack failed (code %d)" % err)
    return bytes(v.output[: v.output_offset])


# ======================================================================
# helpers
# ======================================================================
def _decode_str(raw: bytes) -> str:
    """Faithful name decode: bytes up to the first NUL, latin1.  No stripping/
    filtering so the value round-trips exactly (names may keep trailing spaces;
    a name that fills the whole field has no NUL and is terminated by the next
    struct field, e.g. PlayerFile.field_19)."""
    return raw.split(b"\x00")[0].decode("latin1")


def _put_str(buf: bytearray, off: int, field_len: int, text: str):
    # write the full field; a shorter name leaves trailing 0s (its NUL terminator),
    # a field-filling name uses all bytes (next struct field terminates it)
    enc = text.encode("latin1", "replace")[:field_len]
    buf[off : off + len(enc)] = enc


# ======================================================================
# PlayerFile  <->  dict
# ======================================================================
def parse_player(b: bytes) -> dict:
    paf, vh, tc, sf = b[26], b[29], b[30], b[31]
    return {
        "shirt": b[2],
        "name": _decode_str(b[PLAYER_NAME_OFF : PLAYER_NAME_OFF + PLAYER_NAME_LEN]),
        "nationality": b[0],
        "position": POSITIONS.get(paf >> 5, "?"),
        "face": paf & 0x1F,
        "skills": {
            "passing": b[28] & 0x0F,
            "velocity": (vh >> 4) & 0x0F,
            "heading": vh & 0x0F,
            "tackling": (tc >> 4) & 0x0F,
            "control": tc & 0x0F,
            "speed": (sf >> 4) & 0x0F,
            "finishing": sf & 0x0F,
        },
        "price": b[32],
        "cardsInjuries": b[27],
        "goalStats": list(b[33:37]),
        "field_1": b[1],
        "field_19": b[25],
        "someFlag": b[37],
    }


def encode_player(p: dict) -> bytearray:
    b = bytearray(PLAYER_REC_SIZE)
    b[0] = p["nationality"] & 0xFF
    b[1] = p["field_1"] & 0xFF
    b[2] = p["shirt"] & 0xFF
    _put_str(b, PLAYER_NAME_OFF, PLAYER_NAME_LEN, p["name"])
    b[25] = p["field_19"] & 0xFF
    b[26] = ((_POS_REV.get(p["position"], 0) << 5) | (p["face"] & 0x1F)) & 0xFF
    b[27] = p["cardsInjuries"] & 0xFF
    s = p["skills"]
    b[28] = s["passing"] & 0x0F
    b[29] = (((s["velocity"] & 0xF) << 4) | (s["heading"] & 0xF)) & 0xFF
    b[30] = (((s["tackling"] & 0xF) << 4) | (s["control"] & 0xF)) & 0xFF
    b[31] = (((s["speed"] & 0xF) << 4) | (s["finishing"] & 0xF)) & 0xFF
    b[32] = p["price"] & 0xFF
    gs = (list(p["goalStats"]) + [0, 0, 0, 0])[:4]
    b[33:37] = bytes(x & 0xFF for x in gs)
    b[37] = p["someFlag"] & 0xFF
    return b


# ======================================================================
# TeamFileEx  <->  dict
# ======================================================================
def parse_team(b: bytes) -> dict:
    return {
        "country": b[0],
        "ordinal": b[1],
        "globalTeamNumber": (b[2] << 8) | b[3],   # int16 big-endian on disk
        "controls": b[4],
        "name": _decode_str(b[TEAM_NAME_OFF : TEAM_NAME_OFF + TEAM_NAME_LEN]),
        "writesZeroHere": b[22],
        "andWith0xFE": b[23],
        "tactics": b[24],
        "league": b[25],
        "primaryKit": {
            "shirtType": b[26], "stripes": b[27], "basic": b[28],
            "shorts": b[29], "socks": b[30],
        },
        "secondaryKit": {
            "shirtType": b[31], "stripes": b[32], "basic": b[33],
            "shorts": b[34], "socks": b[35],
        },
        "coach": _decode_str(b[COACH_NAME_OFF : COACH_NAME_OFF + COACH_NAME_LEN]),
        "someFlag": b[59],
        "playerNumbers": list(b[60:76]),
        "players": [
            parse_player(b[76 + i * PLAYER_REC_SIZE : 76 + (i + 1) * PLAYER_REC_SIZE])
            for i in range(PLAYERS_PER_TEAM)
        ],
    }


def encode_team(t: dict) -> bytearray:
    b = bytearray(TEAM_REC_SIZE)
    b[0] = t["country"] & 0xFF
    b[1] = t["ordinal"] & 0xFF
    gtn = t["globalTeamNumber"] & 0xFFFF
    b[2], b[3] = (gtn >> 8) & 0xFF, gtn & 0xFF
    b[4] = t["controls"] & 0xFF
    _put_str(b, TEAM_NAME_OFF, TEAM_NAME_LEN, t["name"])
    b[22] = t["writesZeroHere"] & 0xFF
    b[23] = t["andWith0xFE"] & 0xFF
    b[24] = t["tactics"] & 0xFF
    b[25] = t["league"] & 0xFF
    pk, sk = t["primaryKit"], t["secondaryKit"]
    b[26], b[27], b[28], b[29], b[30] = (pk["shirtType"] & 0xFF, pk["stripes"] & 0xFF,
                                         pk["basic"] & 0xFF, pk["shorts"] & 0xFF, pk["socks"] & 0xFF)
    b[31], b[32], b[33], b[34], b[35] = (sk["shirtType"] & 0xFF, sk["stripes"] & 0xFF,
                                         sk["basic"] & 0xFF, sk["shorts"] & 0xFF, sk["socks"] & 0xFF)
    _put_str(b, COACH_NAME_OFF, COACH_NAME_LEN, t["coach"])
    b[59] = t["someFlag"] & 0xFF
    pn = (list(t["playerNumbers"]) + [0] * 16)[:16]
    b[60:76] = bytes(x & 0xFF for x in pn)
    for i, p in enumerate(t["players"][:16]):
        b[76 + i * PLAYER_REC_SIZE : 76 + (i + 1) * PLAYER_REC_SIZE] = encode_player(p)
    return b


# ======================================================================
# blob (count + records)  <->  list[dict]
# ======================================================================
def parse_blob(blob: bytes) -> list:
    count = (blob[0] << 8) | blob[1]
    teams = []
    for i in range(count):
        base = 2 + i * TEAM_REC_SIZE
        rec = blob[base : base + TEAM_REC_SIZE]
        if len(rec) < TEAM_REC_SIZE:
            break
        teams.append(parse_team(rec))
    return teams


def encode_blob(teams: list) -> bytes:
    out = bytearray(2)
    out[0], out[1] = (len(teams) >> 8) & 0xFF, len(teams) & 0xFF
    for t in teams:
        out += encode_team(t)
    return bytes(out)


# ======================================================================
# file-level API
# ======================================================================
def _data_dir() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(os.path.dirname(here), "bin", "data")


def team_file_path(country) -> str:
    return os.path.join(_data_dir(), "TEAM.%03d" % int(country))


def _read_blob(target) -> bytes:
    """Accept a country number (int/str of digits) or a path; return the decompressed blob."""
    if isinstance(target, int) or (isinstance(target, str) and target.isdigit()):
        path = team_file_path(int(target))
    else:
        path = target
    raw = open(path, "rb").read()
    return rnc_unpack(raw) if raw[:3] == b"RNC" else raw


def load(target) -> list:
    """Load a TEAM file (country number or path) -> list of team dicts."""
    return parse_blob(_read_blob(target))


def file_to_dict(target) -> dict:
    """Whole-file JSON-ready dict: {country, country_name, count, teams:[...]}."""
    teams = load(target)
    country = teams[0]["country"] if teams else None
    return {
        "country": country,
        "country_name": country_name(country) if country is not None else None,
        "count": len(teams),
        "teams": teams,
    }


def reencode(target) -> bytes:
    """Decompressed-blob round-trip: parse then re-encode (no RNC)."""
    return encode_blob(load(target))


_names_cache = {}


def team_names(country) -> dict:
    """{teamOrdinal: name} for a country number (cached)."""
    country = int(country)
    if country not in _names_cache:
        _names_cache[country] = {t["ordinal"]: t["name"] for t in load(country)}
    return _names_cache[country]


def resolve(country, ordinal) -> str:
    """Resolve a {country, ordinal} TeamId to a name ('' if unknown)."""
    return team_names(country).get(int(ordinal), "")


# ---- country number -> name (parsed from game_types.h CN_* defines) ----
_country_cache = None


def _load_country_map() -> dict:
    import re
    global _country_cache
    if _country_cache is None:
        _country_cache = {}
        repo = os.path.dirname(os.path.dirname(_data_dir()))   # .../<repo>/bin/data -> <repo>
        hdr = os.path.join(repo, "src", "game_types.h")
        try:
            for line in open(hdr):
                m = re.match(r"\s*#define\s+CN_([A-Z_]+)\s+(\d+)\b", line)
                if m:
                    _country_cache[int(m.group(2))] = m.group(1)
        except OSError:
            pass
    return _country_cache


def country_name(num) -> str:
    """Human country name (e.g. 'SCOTLAND') or 'COUNTRY_N' fallback."""
    return _load_country_map().get(int(num), "COUNTRY_%d" % int(num))


def country_define(num) -> str:
    """The CN_* macro for a country number (e.g. 'CN_SCOTLAND')."""
    nm = _load_country_map().get(int(num))
    return "CN_" + nm if nm else None
