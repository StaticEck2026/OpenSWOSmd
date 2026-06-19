# TEAM.NNN → JSON migration

Status: **Python tooling DONE (2026-06-18).** The read/write library and decode
util exist, the binary layout is fully decoded, and every shipped `TEAM.*` file
round-trips **binary → dict → binary with 0 mismatched bytes** (72/72). The
C-side JSON loader and the eventual authoring-as-JSON are the remaining steps.

This mirrors the save-format JSON migration (`src/save_format_json.c`,
`docs/SAVE_DATA.md`): replace a raw, endian-/layout-coupled binary blob with a
typed, named-field JSON representation that is decoupled from the runtime memory
layout, survives rebuilds / 32↔64-bit / LE↔BE, and is human-editable.

---

## 1. What the files are

`bin/data/TEAM.<country>` — one RNC-ProPack-compressed file **per country**, the
extension being the country number (`TEAM.008` = England, `TEAM.033` = Scotland).
72 files, country numbers 0–85 with gaps (the unused country slots have no file).
The country numbering is the `CN_*` enum in `src/game_types.h`
(`CN_ALBANIA=0` … `CN_ENGLAND=8` … `CN_OCEANIA=85`).

A `{country, ordinal}` **TeamId** (the 2-byte team identity used throughout the
game, see `game_types.h`) resolves to a concrete club by loading
`TEAM.<country>` and taking the record whose `teamOrdinal == ordinal`. That is
exactly what `competition_data.c`'s `teamSeedList[]` draws encode, e.g.
`{ CN_SCOTLAND, 33 }` → Scotland file, ordinal 33 → **RANGERS**.

### On-disk layout (after RNC decompression)

```
[0..1]   uint16  big-endian   team count N
[2..]    N × 684-byte TeamFileEx records
```

The leading 2 bytes are a big-endian team count; e.g. Scotland decompresses to
`27362 = 2 + 40×684` (40 teams). Files are compressed with RNC ProPack v1
(`RNC\x01` magic); the existing port `rnc_propack.py` handles de/compression.

### `TeamFileEx` = 684 bytes (`game_types.h`)

`TeamFile` header (76 B) + 16 × `PlayerFile` (38 B).

`TeamFile` (header) fields, by offset:

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

`PlayerFile` (38 B), by offset:

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

### Byte-faithfulness invariants (verified by 0-mismatch round-trip)

- **Names are not always NUL-terminated.** A name that fills its whole field has
  no in-field NUL and is terminated by the *next* struct field
  (`PlayerFile.field_19` for player names, `writesZeroHere` for team names).
  The encoder writes the full field; the decoder reads up to the first NUL.
- **Trailing spaces in names are real data** — do not strip them in the
  faithful representation (display/comment generation may strip).
- **`passing` high nibble is always 0** across all shipped files (so storing the
  decoded nibble is lossless).
- **`globalTeamNumber` is big-endian** and is a distinct global index, not
  `country<<8 | ordinal`.

---

## 2. The tooling (this repo)

- **`scripts/swos_team_file.py`** — library. RNC de/compress (via
  `rnc_propack.py`), `parse_team`/`parse_player`/`parse_blob` →
  JSON-serialisable dicts, the faithful inverse `encode_*`, and helpers
  `load()`, `file_to_dict()`, `team_names()`, `resolve(country, ordinal)`,
  `country_name()`, `country_define()`.
- **`scripts/decode_team_file.py`** — CLI util:
  ```
  decode_team_file.py 33            # pretty-print Scotland
  decode_team_file.py 33 33         # one team (Scotland ordinal 33)
  decode_team_file.py 33 --json     # JSON for the whole file
  decode_team_file.py 8 0 --json    # JSON for one team
  decode_team_file.py --all --names # every TEAM.* (names only)
  decode_team_file.py --verify      # round-trip every TEAM.* (binary→dict→binary)
  ```
- **`scripts/annotate_competition_teams.py`** — used the library to rewrite the
  `competition_data.c` `teamSeedList[]` draws as `{ CN_*, ordinal /* NAME */ }`.

`RNC_PROPACK_DIR` overrides where `rnc_propack.py` is found (default search
includes `/mnt/c/Projects/Python/SWOS`).

---

## 3. Proposed JSON schema

One JSON document per `TEAM.<country>` (matching `file_to_dict()`):

```json
{
  "country": 33,
  "country_name": "SCOTLAND",
  "count": 40,
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

Every byte of the 684-byte record is covered by a named field, so the schema is
a complete, round-trippable description — there is no opaque-blob fallback (the
end-state the save-format migration is still chasing).

Proposed on-disk location for authored JSON: `bin/data/teams/<CN_NAME>.json`
(e.g. `SCOTLAND.json`), keyed by country name for human navigability, with the
binary `TEAM.NNN` retained for the loader's auto-detect during the transition.

---

## 4. Migration plan (parallel to the save-format JSON migration)

1. **Python tooling — DONE.** Parse + faithful encode + `--verify` round-trip
   (72/72 files, 0 mismatched bytes). Binary format fully decoded.
2. **Finalise the JSON schema** (this doc). Decide the on-disk naming/location.
3. **Export pass.** Generate the JSON corpus from the binaries with
   `decode_team_file.py --json` and check it in (or generate on demand).
4. **C-side JSON loader.** Add a mjson-based team reader mirroring
   `save_format_json.c`: load named fields, re-derive nothing from addresses,
   keep endianness explicit. The team loader **auto-detects** format by header
   (RNC magic / `{` JSON) so legacy binary `TEAM.NNN` still loads — same policy
   as the save loader.
5. **Verify** byte-image parity (JSON-derived `TeamFileEx` image == binary
   image) and cross-target (the Amiga build reads the same JSON).
6. **Author as JSON.** Once parity holds, JSON becomes the source of truth; the
   binary writer is only needed for export/back-compat. Back-port to the Amiga C
   port (user commits there manually).

### Policy note

`.TAC/.HIL/.EDT/.CFG` keep their binary readers (community-interop /
fixed-layout records) — see `docs/SAVE_DATA.md` "Format migration / read-support
policy". `.EDT` is the SWOS team-editor interop format and is a *different*
binary team representation from `TEAM.NNN`; the JSON migration here is for the
default `TEAM.NNN` country databases, not `.EDT`.

---

## 5. Open questions

- `globalTeamNumber` — exact derivation / whether it must be regenerated on
  authoring or is free-form. Round-trip keeps it verbatim for now.
- `field_1` / `field_19` / player `someFlag` / team `someFlag` semantics — kept
  as named raw bytes; rename if/when pinned down.
- Whether `playerNumbers[16]` is fully redundant with each player's `shirt`
  (looks duplicated; keep both until confirmed).
