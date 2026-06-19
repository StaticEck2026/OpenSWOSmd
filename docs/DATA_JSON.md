# bin/data → JSON migration

Status: **Python tooling DONE (2026-06-18).** A read/write library + decode util
cover every record-based file in `bin/data/`; the layouts are fully decoded and
every file round-trips **binary → dict → binary with 0 mismatched bytes** (76/76:
72 `TEAM.*`, 3 `.TMD`, `POOLPLYR.DAT`). The C-side JSON loaders and eventual
authoring-as-JSON are the remaining steps.

This mirrors the save-format JSON migration (`src/save_format_json.c`,
`docs/SAVE_DATA.md`): replace raw, endian-/layout-coupled binary blobs with a
typed, named-field JSON representation decoupled from the runtime memory layout,
surviving rebuilds / 32↔64-bit / LE↔BE and human-editable.

---

## 1. The files

`bin/data/` holds three kinds of record-based, **RNC-ProPack-compressed** files:

| file(s)            | RNC | layout |
|--------------------|-----|--------|
| `TEAM.<country>` ×72 | v1 | `u16 BE count` + count × 684-byte `TeamFileEx` |
| `EUROCUP.TMD` / `EUROCWC.TMD` / `UEFACUP.TMD` | v2 | bare `TeamFileEx[]` (no count; N = size/684 → 16 / 32 / 32) |
| `POOLPLYR.DAT`     | v2 | bare `PoolPlayer[]` (no count; 58 B each → 900) |

- **`TEAM.<country>`** — one team database per country, the extension being the
  country number (`TEAM.008` = England, `TEAM.033` = Scotland). The country
  numbering is the `CN_*` enum in `src/game_types.h` (`CN_ALBANIA=0` …
  `CN_ENGLAND=8` … `CN_OCEANIA=85`). A `{country, ordinal}` **TeamId** (the
  2-byte team identity used throughout the game) resolves to a club by loading
  `TEAM.<country>` and taking the record whose `teamOrdinal == ordinal` — exactly
  what `competition_data.c`'s `teamSeedList[]` draws encode, e.g.
  `{ CN_SCOTLAND, 33 }` → **RANGERS**.
- **`*.TMD`** — the European-cup team pools, loaded into `tmdFileBuffer`
  (`TeamFileEx[80]`). `EUROCUP.TMD` is the 16 Champions Cup teams (it matches the
  `euroCup` seed list one-for-one: Auxerre, Ajax, Rangers, Juventus, AC Milan, …).
- **`POOLPLYR.DAT`** — the pool-player database (`_FindPoolplyrDatPlayer`),
  loaded into `poolplyrDatBuffer`.

RNC method (v1 vs v2) is the header byte (`RNC\x01` / `RNC\x02`); the
decompressor auto-detects it.

---

## 2. Record layouts (`src/game_types.h`)

### `TeamFileEx` = 684 bytes — `TeamFile` header (76) + 16 × `PlayerFile` (38)

`TeamFile` header:

| off | type        | field            | notes |
|-----|-------------|------------------|-------|
| 0   | u8          | countryNumber    | |
| 1   | u8          | teamOrdinal      | index within the country (the "ordinal") |
| 2   | i16 **BE**  | globalTeamNumber | global index across all teams (NOT the {country,ordinal} pack) |
| 4   | u8          | teamControls     | |
| 5   | char[17]    | teamName         | NUL-terminated, or fills the field (terminated by byte 22) |
| 22  | u8          | writesZeroHere   | name terminator slack |
| 23  | u8          | andWith0xFE      | |
| 24  | u8          | tactics          | |
| 25  | u8          | league           | |
| 26–30 | u8×5      | primary kit      | shirtType, stripes, basic, shorts, socks |
| 31–35 | u8×5      | secondary kit    | shirtType, stripes, basic, shorts, socks |
| 36  | char[23]    | coachName        | NUL-terminated |
| 59  | u8          | someFlag         | |
| 60–75 | u8×16     | playerNumbers    | |

`PlayerFile` (38 B):

| off | type     | field            | notes |
|-----|----------|------------------|-------|
| 0   | i8       | nationality      | |
| 1   | i8       | field_1          | runtime-scratch on Amiga; purpose unconfirmed |
| 2   | i8       | shirtNumber      | |
| 3   | char[22] | playerName       | terminated by field_19, so may use all 22 bytes |
| 25  | i8       | field_19         | NUL spacer after the name |
| 26  | u8       | positionAndFace  | hi 3 bits = position, lo 5 = skin/hair |
| 27  | u8       | cardsInjuries    | |
| 28  | i8       | passing          | low nibble (hi nibble is 0) |
| 29  | i8       | velocity·heading | hi nibble = velocity, lo = heading |
| 30  | i8       | tackling·control | hi nibble = tackling, lo = control |
| 31  | i8       | speed·finishing  | hi nibble = speed, lo = finishing |
| 32  | i8       | price            | base price id |
| 33–36 | u8×4   | goalStats        | per-competition goals scored |
| 37  | i8       | someFlag         | |

Position nibble (`positionAndFace >> 5`): `0=GK 1=RB 2=LB 3=CD 4=RW 5=LW 6=MF 7=ST`.

### `PoolPlayer` = 58 bytes (`POOLPLYR.DAT`)

| off | type     | field          | notes |
|-----|----------|----------------|-------|
| 0   | u16 **BE** | teamId       | the player's club id |
| 2   | i8       | playerOrdinal  | |
| 3   | char[17] | teamName       | NUL-terminated |
| 20  | PlayerFile | player       | embedded 38-byte player record |

### Byte-faithfulness invariants (verified by the 0-mismatch round-trip)

- **Names are not always NUL-terminated.** A name that fills its whole field has
  no in-field NUL and is terminated by the *next* struct field
  (`PlayerFile.field_19` for player names, `writesZeroHere` for team names). The
  encoder writes the full field; the decoder reads up to the first NUL.
- **Trailing spaces in names are real data** — not stripped in the faithful
  representation (display/comment generation may strip).
- **`passing` high nibble is always 0** across all shipped files (decoded nibble
  is lossless).
- **`globalTeamNumber` / `teamId` are big-endian.**

---

## 3. The tooling (this repo)

- **`scripts/swos_team_file.py`** — library. RNC v1/v2 de/compress (via
  `rnc_propack.py`), `parse_team`/`parse_player`/`parse_pool_player` →
  JSON-serialisable dicts + the faithful inverse `encode_*`, plus
  `parse_blob`/`parse_tmd_blob`/`parse_poolplyr_blob` and the dispatchers
  `file_type()`, `file_to_dict()`, `reencode()`, and helpers `load()`,
  `team_names()`, `resolve(country, ordinal)`, `country_name()`,
  `country_define()`.
- **`scripts/decode_data.py`** — CLI util:
  ```
  decode_data.py 33                       # Scotland, pretty-print
  decode_data.py bin/data/EUROCUP.TMD     # a European-cup TMD
  decode_data.py bin/data/POOLPLYR.DAT    # pool players
  decode_data.py 33 33                    # one team (Scotland ordinal 33)
  decode_data.py 33 --json                # JSON for the whole file
  decode_data.py --all --names            # every data file, names only
  decode_data.py --verify                 # round-trip every data file
  decode_data.py --dump-json data_json # export the whole corpus to JSON
  ```
- **`scripts/annotate_competition_teams.py`** — used the library to rewrite the
  `competition_data.c` `teamSeedList[]` draws as `{ CN_*, ordinal /* NAME */ }`.

`RNC_PROPACK_DIR` overrides where `rnc_propack.py` is found (default search
includes `/mnt/c/Projects/Python/SWOS`).

---

## 4. Proposed JSON schemas

`file_to_dict()` tags each document with a `"type"`. Every byte of every record
is covered by a named field, so the schema is complete and round-trippable — no
opaque-blob fallback (the end-state the save-format migration is still chasing).

### TEAM.<country>  (`"type": "team"`)

```json
{
  "type": "team", "country": 33, "country_name": "SCOTLAND", "count": 40,
  "teams": [
    {
      "country": 33, "ordinal": 0, "globalTeamNumber": 1360,
      "controls": 0, "name": "ABERDEEN", "coach": "ROY AITKEN",
      "tactics": 5, "league": 0,
      "writesZeroHere": 0, "andWith0xFE": 0, "someFlag": 0,
      "primaryKit":   { "shirtType": 0, "stripes": 4, "basic": 4, "shorts": 4, "socks": 4 },
      "secondaryKit": { "shirtType": 0, "stripes": 5, "basic": 5, "shorts": 5, "socks": 5 },
      "playerNumbers": [0, 2, 5, 6, 3, 8, 10, 11, 9, 14, 15, 1, 4, 7, 12, 13],
      "players": [
        {
          "shirt": 1, "name": "NICKY WALKER", "nationality": 31,
          "position": "GK", "face": 0,
          "skills": { "passing": 0, "velocity": 0, "heading": 0,
                      "tackling": 0, "control": 0, "speed": 0, "finishing": 0 },
          "price": 22, "cardsInjuries": 0, "goalStats": [0,0,0,0],
          "field_1": 0, "field_19": 0, "someFlag": 0
        }
      ]
    }
  ]
}
```

### *.TMD  (`"type": "tmd"`)

Same `teams[]` shape, no `country`/count header — `count = size/684`:

```json
{ "type": "tmd", "file": "EUROCUP.TMD", "count": 16, "teams": [ /* TeamFileEx... */ ] }
```

### POOLPLYR.DAT  (`"type": "pool"`)

```json
{
  "type": "pool", "file": "POOLPLYR.DAT", "count": 900,
  "players": [
    { "teamId": 516, "playerOrdinal": 2, "teamName": "CLUB BRUGES",
      "player": { "shirt": 7, "name": "ERIC DEFLANDRE", "position": "RB", "...": "..." } }
  ]
}
```

Proposed on-disk location for authored JSON: `data_json/` — `<CN_NAME>.json`
for the team databases (e.g. `SCOTLAND.json`), `EUROCUP.json` etc. for the TMDs,
`POOLPLYR.json` for the pool. The binary files are retained for the loader's
auto-detect during the transition. (`decode_data.py --dump-json data_json`
generates exactly this corpus.)

---

## 5. Migration plan (parallel to the save-format JSON migration)

1. **Python tooling — DONE.** Parse + faithful encode + `--verify` round-trip
   (76/76 files, 0 mismatched bytes). All layouts decoded.
2. **Finalise the JSON schemas** (this doc). Decide the on-disk naming/location.
3. **Export pass.** Generate the JSON corpus with `decode_data.py --dump-json`
   and check it in (or generate on demand).
4. **C-side JSON loaders.** Add mjson-based readers mirroring
   `save_format_json.c` for `TEAM.NNN` / `.TMD` / `POOLPLYR.DAT`: load named
   fields, re-derive nothing from addresses, keep endianness explicit. Each
   loader **auto-detects** format by header (RNC magic / `{` JSON) so legacy
   binary still loads — same policy as the save loader.
5. **Verify** byte-image parity (JSON-derived image == binary image) and
   cross-target (the Amiga build reads the same JSON).
6. **Author as JSON.** Once parity holds, JSON becomes the source of truth; the
   binary writer is only needed for export/back-compat. Back-port to the Amiga C
   port (user commits there manually).

### Policy note

`.TAC/.HIL/.EDT/.CFG` keep their binary readers (community-interop /
fixed-layout records) — see `docs/SAVE_DATA.md` "Format migration / read-support
policy". `.EDT` is the SWOS team-editor interop format and is a *different*
binary team representation from `TEAM.NNN`; this migration is for the default
`bin/data` databases, not `.EDT`.

---

## 6. Open questions

- `globalTeamNumber` / `PoolPlayer.teamId` — exact derivation / whether they must
  be regenerated on authoring. Round-trip keeps them verbatim for now.
- `field_1` / `field_19` / player & team `someFlag` semantics — kept as named raw
  bytes; rename if/when pinned down.
- Whether `playerNumbers[16]` is redundant with each player's `shirt` (looks
  duplicated; keep both until confirmed).
- `.TMD` / `POOLPLYR.DAT` carry no team count of their own (the consumer knows
  the count); the JSON keeps an explicit `count` for clarity.
