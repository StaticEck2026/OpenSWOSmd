# competition_data.c → JSON (full data jailbreak)

Status: **EXPORT DONE (2026-06-18).** `scripts/export_competitions.py` writes the
**entire** competition graph to `bin/data_json/competitions/` — cups (76), cups2
(13), playoffs (7), leagues (65), qualifying groups (10), league blocks (4 + the
English block), the 35-entry career index, and misc tables. Validated: 132 seed
entries match `annotate_competition_teams.py` exactly; every `name[]` symbol
resolves; the cross-reference graph is **closed** (180 ids, 90 refs, 0 dangling;
`*Copy` derived). The C-side loader (phase 4) is next.

This extends the data→JSON migration (`docs/DATA_JSON.md`) from the external
`bin/data` files (teams/TMD/pool) to the **internal** competition tables compiled
into `src/competition_data.c`. The end goal is a fully data-driven SWOS:
**add/remove countries, teams, players and competitions by editing JSON, no
recompile.**

The teams half is done (`docs/DATA_JSON.md`). This document covers the harder
half — the competition graph — and the C-side loaders that read it all back.

---

## 1. Directory layout

```
bin/data_json/
  teams/            <CN_NAME>.json (72), EUROCUP.json, EUROCWC.json,
                    UEFACUP.json, POOLPLYR.json     ← from bin/data (DATA_JSON.md)
  competitions/     the competition_data.c tables (this document)
```

`bin/data_json/` is git-ignored and regenerable; it lives under `bin/` so the
game can load JSON in place of the compiled data during testing.

---

## 2. What is in competition_data.c

~1430 lines, all inside `#if COMPETITION_DATA_C`. Inventory (top-level objects):

| type | count | what |
|------|-------|------|
| `CompetitionCup`      | ~76 | domestic/continental cups (merged header + rounds + name[2] + teamSeedList[32]) |
| `CompetitionLeague`   | ~65 | leagues |
| `CompetitionCup2`     | ~13 | type-2 group/seeded cups (EURO/CWC/UEFA/WC etc.) |
| `QualifyingGroup`     | ~10 | qualifier group tables (compCup[], countries[]) |
| `Playoffs`            | ~7  | promotion/relegation playoffs |
| `Leagues`             | ~4  | league-group link blocks (`{league*, linkParams}` ×4 + playoffs + nextGroup) |
| `EnglishLeaguesBlock` | 1   | embeds several `Leagues`/`Playoffs` sub-records |
| `CompetitionCareer`   | 1 (`careerContests[35]`) | the master index: `{competition*, competitionCopy*, templateSize}` ×35 |
| `EuroCupQualSeedBucketBlock`, `CountryNumbers`, misc `uint8/16` tables | a few | seeds / lookups |

Struct definitions live in `src/game_types.h` (the merged
`CompetitionCup`/`CompetitionCup2`/`CompetitionLeague`/`Playoffs`/`Leagues`/
`QualifyingGroup`/`CompetitionCareer`, and the `Competition`/`CompCupHeader`/
`CompCupHeader2`/`Round` building blocks).

---

## 3. The hard parts (and how JSON handles each)

1. **Name pointers** — `char *name[2]` point to string literals in
   `src/strings_data.c` (`const char aEuropeanChampi_1[] = "EUROPEAN CHAMPIONS CUP";`).
   → JSON stores the **strings** directly (`"name": ["EUROPEAN CHAMPIONS CUP", "EUROPEAN CUP"]`).
   On load they are interned / pointed at a string pool. This is the same
   pointer-to-disk fix the save format already uses for competition names.
2. **Cross-object pointers** — `CompetitionCareer{competition*, competitionCopy*}`,
   `Leagues{league1..4*, playoffs*, nextGroup*}`, `EnglishLeaguesBlock` embedding
   `Leagues`. → JSON stores a **stable string ID** = the object's C symbol name
   (`"competition": "euroCup"`). A **registry** (id → object) resolves them on
   load. Never store an address.
3. **The `*Copy` runtime templates** — `euroCupCopy`, `uefaCupCopy`, … are NOT
   authored data: they are runtime scratch in the careerFileBuffer region,
   populated at career init by copying the template (`careerContests[i]` does a
   `templateSize` memcpy template→Copy; see `career_comp_copies.c`). → JSON
   **authors only the templates**; the `competitionCopy` reference is recorded by
   id and the Copy is **derived** on load (mirrors how the save format
   reconstructs `*Copy`). `careerContests[i].templateSize` is kept.
4. **Self-relative offset bytes** — `CUP_NAME_TABLE_OFFSET`,
   `CUP_TEAMLIST_TABLE_OFFSET`, `CUP2_*` are layout artifacts of the merged
   structs (asserted == 15/21/35/41). → **Derived on load**, not authored.
5. **`teamSeedList`** — the `{country, ordinal}` draws already understood
   (`annotate_competition_teams.py`). → JSON stores them resolved and readable:
   `{ "country": "CN_SCOTLAND", "ordinal": 33, "team": "RANGERS" }` (the `team`
   field is a comment-like convenience resolved from the teams JSON; `country`
   may be the `CN_*` name or the number — loader accepts either).
6. **Merged-struct partial records** — e.g. `Leagues` leaves records 3–4 zero for
   the 2-record variants; `QualifyingGroup` arrays are sentinel-terminated. →
   Schema allows omitted/zero tails; the loader zero-fills.

---

## 4. JSON schema

Reuse the `.CAR`/teams conventions (`docs/DATA_JSON.md` §4): snake_case keys,
arrays for fixed byte runs, numbers host-order-independent. Each addressable
object carries an `"id"` (its C symbol name) so cross-refs resolve.

`CompetitionCup` / `CompetitionCup2` (mirrors the merged structs):

```json
{
  "id": "euroCup", "type": "cup2",
  "comp": { /* Competition base: comp_type, num_teams, ... + derived offset bytes */ },
  "seeding_type": 1, "menu_state": 1, "play_each_team": 2, "away_goals_mode": 3,
  "subs_and_points_for_win": 53, "round_string_base": 4,
  "rounds": [ { "teams": 16, "legs": 4, "x": 4 }, … 6 ],
  "round_rule_flags": [0, -108, -108, 20, 0, 0],
  "name": ["EUROPEAN CHAMPIONS CUP", "EUROPEAN CUP"],
  "team_seed_list": [
    { "country": "CN_FRANCE",   "ordinal": 1,  "team": "AUXERRE" },
    { "country": "CN_SCOTLAND", "ordinal": 33, "team": "RANGERS" }
  ]
}
```

`CompetitionLeague`, `Playoffs`, `QualifyingGroup` — one object schema each,
fields named after the struct. `Leagues` and `EnglishLeaguesBlock` store member
references **by id** (`"league1": "englishPremier"`, `"playoffs": "div1Playoffs"`,
`"next_group": null`) plus `link_params` arrays. `careerContests` (the master
index) is an ordered array of `{ "competition": "<id>", "template_size": N }` —
the `competitionCopy` is derived.

### File grouping (one file per type — diffable, not 175 tiny files)

```
competitions/cups.json            CompetitionCup[]
competitions/cups2.json           CompetitionCup2[]
competitions/leagues.json         CompetitionLeague[]
competitions/playoffs.json        Playoffs[]
competitions/qualifying.json      QualifyingGroup[]
competitions/league_blocks.json   Leagues[] + EnglishLeaguesBlock
competitions/career.json          careerContests[35] (the index)
competitions/country_competitions.json  competitionsTable[country] -> {leagues, cups} (the <country>Table[] map)
competitions/countries.json       country master: {number, name, define, team_file} (allCountriesList)
competitions/seed_buckets.json    euro-cup qualification berth allocation (EuroCupQualSeedBucketBlock)
```

`countries.json` is the **master registry** that maps a country number → name →
its `teams/<NAME>.json` — the single place to add/remove a country (see
`docs/DATA_CUSTOMIZE.md`).

(Per-competition files under `competitions/<id>.json` are an option if finer
add/remove granularity is wanted; grouped-by-type is the recommended default.)

---

## 5. Export (bootstrap: C tables → JSON)

Two mechanisms; we use both, for different stages:

- **(A) Python bootstrap exporter** — preprocess `competition_data.c` with
  `gcc -E` (expands the `CUP*_*_OFFSET`/macros already proven in the annotation
  work), parse the numeric initializers, resolve `name[]` symbols from
  `strings_data.c`, and emit cross-refs **symbolically** (the source already
  writes `&euroCup`, so the id is right there — no address map needed). Runs with
  no build/run; ideal to **seed the JSON now**. Starts with the cups (where the
  seed-list parser already exists), then leagues/playoffs/career.
- **(B) C emitter in the SDL build** (production) — a `competition_data_json.c`
  that reuses `save_format_json.c`'s `emit_*`/`emit_struct_object` helpers and a
  **competition registry** (object↔id) to emit cross-refs, triggered by an env
  var at startup after the tables are initialised. Robust (evaluates
  macros/`offsetof`, dereferences real string pointers). Shares the registry with
  the loader, so it is the long-term path; the Python exporter is the bootstrap.

Both must reproduce the compiled tables exactly (a re-encode/round-trip check
like the teams `--verify`).

---

## 6. C-side loaders

- New `competition_data_json.c` (or a section of `save_format_json.c`): at startup
  (before the menus/career use the tables), load `competitions/*.json` into the
  static competition structs, reusing `load_struct_object` + per-type
  `StructFieldDesc` tables (the same machinery the teams/save loaders use).
- A **competition registry**: `{ id → (object pointer, type, template_size) }`,
  built from the same symbol set. Used to (a) resolve `team_seed_list` is local
  but cross-refs (`careerContests.competition`, `Leagues.*`) need it, and (b)
  rebuild `careerContests` + derive each `*Copy`.
- **Auto-detect / fallback**: if `bin/data_json/competitions/` is present (and a
  flag/env opt-in), load JSON; otherwise use the compiled-in static initializers.
  The static initializers in `competition_data.c` stay as the fallback **until**
  JSON loading is proven, then they can be removed and the file becomes just the
  struct storage (`.bss`/`.data`).
- **Teams**: a parallel loader reads `teams/*.json` into the team databases
  (reusing `s_team_file_fields`/`s_player_fields`), so `TEAM.NNN`/`.TMD`/
  `POOLPLYR` can come from JSON too — the same auto-detect/fallback.

---

## 7. Migration phases

1. **Directory reorg** — teams JSON → `bin/data_json/teams/`; create
   `competitions/`. ✅ DONE.
2. **Schema + bootstrap export of cups** — `cups.json` / `cups2.json`. ✅ DONE.
3. **Extend export** to leagues / playoffs / qualifying / career / league-blocks
   (incl. the English block) with cross-ref ids; full competition corpus.
   ✅ DONE — graph closed (0 dangling refs).
4. **C loaders** (teams + competitions) table-driven + registry + auto-detect.
   Teams loader ✅ DONE — `DataJson_LoadTeamArray` in save_format_json.c reuses
   `load_team_file_ex_slice`/`s_team_file_fields`; `SWOS_DATA_JSON_SELFTEST=1`
   loads SCOTLAND.json → PASS (40 teams, RANGERS/Gascoigne verified). Competitions
   loader + registry + auto-detect/fallback ← NEXT.
5. **Switch default to JSON**; retire the static initializers (competition_data.c
   → storage only). Game is data-driven.
6. **Amiga path** — *open question* (see §8).

---

## 8. Open question: the Amiga side

`docs/DATA_BSON.md` recommended the Amiga keep a fixed binary (fastest on m68k),
generated from JSON by a host build step. **The user is not sold on returning to
structs for the Amiga.** Options still on the table:

- JSON parsed on the Amiga (slow on 68000, see DATA_BSON.md §1 — needs measuring;
  maybe acceptable at load time on 020+/with caching).
- A compact self-describing binary (MessagePack — big-endian, compact) parsed on
  the Amiga, authored from the same JSON.
- The host-export-binary pipeline (DATA_BSON.md §3).

Deferred — decided after the SDL JSON path works. The SDL loaders (this document)
are endianness-explicit and re-derive nothing from addresses, so whichever Amiga
representation we pick, the JSON stays the single source of truth.

---

## 9. Risks

- **Cross-ref integrity** — the registry must cover every referenced id; a
  dangling `careerContests.competition` must be a hard error, not a silent skip
  (panics are signals — `.claude/CLAUDE.md`).
- **`*Copy` derivation** — author templates only; never author/serialize the
  runtime Copies (they belong to the careerFileBuffer overlay).
- **Layout artifacts** — derive `offsetof`-based bytes; do not author them.
- **Partial merged records** — `Leagues` 2-vs-4, sentinel-terminated
  `QualifyingGroup` arrays; the loader zero-fills omitted tails.
- **Load order** — competitions reference each other and the teams; load teams
  first, then competition objects, then resolve cross-refs / rebuild the index.
