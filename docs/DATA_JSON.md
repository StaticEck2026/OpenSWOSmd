# bin/data → JSON migration

Status: **COMPLETE.** A read/write library + decode util cover every record-based
file in `bin/data/`; the layouts are fully decoded and every file round-trips
**binary → dict → binary with 0 mismatched bytes** (76/76: 72 `TEAM.*`, 3 `.TMD`,
`POOLPLYR.DAT`). Authoring-as-JSON is live: `scripts/emit_teams.py` regenerates the
RNC-compressed files 1:1 from the JSON, and the C-side data-drive runs.

> This doc is the **teams / TMD / POOLPLYR binary deep-dive**. For the whole
> data-as-JSON pipeline — competitions + menus → C, the build-time bake/flip, and the
> edit workflows — see **`docs/DATA_PIPELINE.md`**.

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
  decode_data.py --dump-json              # export the whole corpus to bin/data_json/teams/
  ```
- **`scripts/annotate_competition_teams.py`** — used the library to rewrite the
  `competition_data.c` `teamSeedList[]` draws as `{ CN_*, ordinal /* NAME */ }`.

`RNC_PROPACK_DIR` overrides where `rnc_propack.py` is found (default search
includes `/mnt/c/Projects/Python/SWOS`).

---

## 4. JSON schema (matches the .CAR save format exactly)

The keys, nesting, packing and signedness mirror `save_format_json.c`
(`emit_team_file_ex_body` / `s_team_file_fields`, `emit_player_file_body` /
`s_player_fields`, `emit_market_player_body`) so the `.CAR` and `bin/data` JSON
share **one** representation and **one** C loader: `load_struct_object` with the
existing field tables can read these documents directly.

Consequences of matching `.CAR` (rather than a "prettier" decode):

- snake_case keys (`writes_zero_here`, `global_team_number`, `cards_injuries`).
- Skill/position bytes stay **packed** — `position_and_face`, `velocity_heading`,
  `tackling_control`, `speed_finishing` are single (signed) bytes, not decoded
  nibbles. `decode_data.py` decodes them for human display only.
- `kit_colors` is one 10-byte array; `player_numbers` 16; `goal_stats` 4.
- A `TeamFileEx` is `{ "team": { …13 header fields… }, "players": [16] }`.
- `global_team_number` / `team_id` are signed (`s16`); the many byte fields are
  emitted signed (`s8`) exactly as the save format does — both signed and
  unsigned JSON decode to the same byte via `(uint8_t)`.

`file_to_dict()` tags each document with a `"type"`. Every byte of every record
is covered by a named field — no opaque-blob fallback.

### TEAM.<country>  (`"type": "team"`)

```json
{
  "type": "team", "country": 33, "country_name": "SCOTLAND", "count": 40,
  "teams": [
    {
      "team": {
        "country_number": 33, "team_ordinal": 0, "global_team_number": 1360,
        "team_controls": 0, "team_name": "ABERDEEN",
        "writes_zero_here": 0, "and_with_0xfe": 0, "tactics": 5, "league": 0,
        "kit_colors": [0, 4, 4, 4, 4, 0, 5, 5, 5, 5],
        "coach_name": "ROY AITKEN", "some_flag": 0,
        "player_numbers": [0, 2, 5, 6, 3, 8, 10, 11, 9, 14, 15, 1, 4, 7, 12, 13]
      },
      "players": [
        {
          "nationality": 31, "field_1": 0, "shirt_number": 1,
          "player_name": "NICKY WALKER", "field_19": 0,
          "position_and_face": 0, "cards_injuries": 0,
          "passing": 0, "velocity_heading": 0, "tackling_control": 0,
          "speed_finishing": 0, "price": 22, "goal_stats": [0, 0, 0, 0],
          "some_flag": 0
        }
      ]
    }
  ]
}
```

### *.TMD  (`"type": "tmd"`)

Same `teams[]` (TeamFileEx) shape, no `country` field — `count = size/684`:

```json
{ "type": "tmd", "file": "EUROCUP.TMD", "count": 16, "teams": [ /* { "team":…, "players":… } */ ] }
```

### POOLPLYR.DAT  (`"type": "pool"`)

```json
{
  "type": "pool", "file": "POOLPLYR.DAT", "count": 900,
  "players": [
    { "team_id": 516, "player_ordinal": 2, "team_name": "CLUB BRUGES",
      "player": { "shirt_number": 7, "player_name": "ERIC DEFLANDRE", "position_and_face": 32, "...": "..." } }
  ]
}
```

### On-disk export

`decode_data.py --dump-json` writes the corpus to **`bin/data_json/teams/`** —
`<CN_NAME>.json` for the team databases (e.g. `SCOTLAND.json`), and the source
stem for the rest: `EUROCUP.json`, `EUROCWC.json`, `UEFACUP.json`,
`POOLPLYR.json`. Files are written **expanded
(indented)** by default while developing (readable); `--compact` minifies them
(~7.8 MB vs ~16 MB) for release. The dir lives under `bin/` so the game can load
JSON in place of the binaries during testing; the binary files are retained for
the loader's auto-detect during the transition. The corpus is git-ignored
(regenerable).

---

## 5. Migration plan (parallel to the save-format JSON migration)

1. **Python tooling — DONE.** Parse + faithful encode + `--verify` round-trip
   (76/76 files, 0 mismatched bytes). All layouts decoded.
2. **Finalise the JSON schemas** (this doc). Decide the on-disk naming/location.
3. **Export pass.** Generate the JSON corpus with `decode_data.py --dump-json`
   and check it in (or generate on demand).
4. **C-side JSON loaders.** Because the schema matches the save format, the
   readers can **reuse `save_format_json.c`'s `load_struct_object` with the
   existing `s_team_file_fields` / `s_player_fields` tables** (and a small
   PoolPlayer table) rather than new parsers — load named fields, re-derive
   nothing from addresses, keep endianness explicit. Each loader
   **auto-detects** format by header (RNC magic / `{` JSON) so legacy binary
   still loads — same policy as the save loader.
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
