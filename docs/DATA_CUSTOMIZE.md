# Safe data customization — add/remove countries, teams, players, competitions

Status: **DESIGN / ANALYSIS.** The data→JSON migration (`docs/DATA_JSON.md`,
`docs/DATA_INTERNAL_JSON.md`) makes the data editable. This document is about the
other half of "jailbreak": making edits **safe** — a missing, extra, or wrong
entry must produce a **clear error at load time**, never a crash, silent
corruption, or a confusing panic three menus later.

> **Status (2026-09-30): still a design.** The editing half landed (the JSON is
> loaded at run time and baked into C at build — `docs/DATA_PIPELINE.md`), but the
> §4/§5 backlogs are open: there is no `scripts/validate_data.py`, no
> `bin/data_json/manifest.json` and no data digest. The netplay handshake
> (`src/p2p_link.c`) refuses a peer whose build stamp, rollback state layout
> (`SimState_LayoutHash`) or rules (`P2P_RULE_*`) differ — it does **not** compare
> the game data.

The guiding rule (consistent with `.claude/CLAUDE.md` "panics are signals"): the
panic philosophy says *don't mask internal bugs*. Customization adds a **data
boundary** — so we add an **explicit validation layer at load** that reports
*user-data* errors plainly ("competition `euroCup` references team `{CN_X, 99}`
which does not exist"), distinct from internal-invariant panics. We validate the
data **once, at the door**; past the door the game keeps trusting its invariants.

---

## 1. What can break (failure modes)

### 1a. Fixed-size arrays / hard caps

The engine uses fixed C arrays. Adding past the cap overflows into adjacent
globals (silent corruption → a panic/crash elsewhere later). Known caps:

| array | cap | meaning |
|-------|-----|---------|
| `allCountriesList[66]` | 66 | selectable countries (the master order) |
| `careerContests[CAREER_CONTESTS_CAP]` | 48 | career competition index (was 35; `include/competition_data.h`) |
| `tmdFileBuffer` = 87552 B (128 × `TeamFileEx`) | 128 | European-cup team pool (was 80; `src/career_team_buffers.c`) |
| `TeamFileEx.players[16]` | 16 | players per team (**exact**, not a max) |
| `teamSeedList[32]` | 32 | cup/cup2 static draw |
| `CompetitionLeague.divisions[4]` | 4 | divisions per league |
| `CompetitionLeague.name[10]` | 10 | division name strings |
| `Leagues` (4 `{league,linkParams}`) | 4 | league-group links |
| `QualifyingGroup.compCup[9]` / `countries[44]` | 9 / 44 | qualifier members |
| `Playoffs.qualifiedTeamIds[8]` | 8 | playoff bracket size |
| teams per `TEAM.<country>` | (buffer-bound) | variable; see §1b |
| `POOLPLYR.DAT` pool players | `poolplyrDatBuffer` = 0x14000 / 58 ≈ 1448 | pool size (currently 900) |

A modder must respect these, **or** the cap must be raised (recompile) — the
loader must **detect overflow and refuse**, never write past the array.

### 1b. Count fields that must agree

Several records carry an explicit count that the engine trusts:
`TeamFileEx` count (the `TEAM.<country>` header / JSON array length),
`CompetitionLeague.divisions_count`, the cup `Round[]` counts, etc. If the JSON
array length and the count disagree (e.g. you add a team but the consumer reads a
stale count), the engine walks the wrong number of records. The loader must
**derive counts from the array length** (single source) and reject mismatches.

### 1c. Cross-reference integrity (dangling ids)

Competitions reference each other and the teams **by identity**, not address
(`docs/DATA_INTERNAL_JSON.md` §3): `careerContests.competition`, `Leagues.league/
playoffs/next_group`, `QualifyingGroup.comp_cup`, league `playoff_links`, and
every `team_seed_list` entry (`{country, ordinal}`). Remove a competition or team
that something points at → a **dangling reference**. The loader must resolve
every id against the registry and **fail with the offending id**, not deref null.

### 1d. Index / identity validity

A `{country, ordinal}` TeamId must resolve: the country must exist
(`countries.json`) and the ordinal must be `< teams_in_that_country`. An out-of-
range ordinal today indexes past the team array. The loader must **range-check**.

### 1e. Internal magic invariants

Beyond the data boundary the engine still asserts things (slot counts == 16,
magic words, `_VerifyTeamLoaded`, …). Customization that *satisfies* the load
validator but violates a deeper rule should still hit the original panic — that
is correct (the panic is a signal). The validator's job is to catch the
**data-shaped** errors up front so the *common* mistakes never reach a panic.

---

## 2. Robustness mechanisms

1. **Master registries (single source of mapping).**
   - `competitions/countries.json` — the country master: `{number, name, define,
     team_file}`. One place maps a country number → name → its `teams/<NAME>.json`.
     Add/remove a country = one entry here (+ its team file).
   - The competition objects' `id` fields are the competition registry; the
     loader builds `{id → object}` from them.
   These make "what exists" explicit and checkable.

2. **Load-time validation that FAILS LOUD, never clamps.** On load, in order:
   teams first, then competitions, then resolve cross-refs. Check: array sizes ≤
   caps; counts == array lengths; every cross-ref id resolves; every TeamId
   `{country, ordinal}` resolves. On any failure, print a specific message
   (file + entity + reason) and abort the load (return error to the caller's
   error menu) — do **not** silently drop/clamp (that hides the user's mistake)
   and do **not** let it fall through to a later panic.

3. **Generous / documented caps.** Where raising a cap is cheap (a bigger fixed
   array + a size-check static assert), do it so modders have headroom. Where a
   cap is structural (16 players/team), document it as exact and validate.

4. **Defaulting + zero-fill of omitted tails.** Optional fields and unused array
   slots default to 0 (the exporter already omits zero tails; the loader
   `memset`s before filling). Adding a field later stays backward-compatible.

5. **Pre-flight validator (lint) + mirrored C checks.** A host tool
   (`scripts/validate_data.py`, planned) walks a customized `bin/data_json` set
   and reports every problem **before** launch — same checks the C loader does,
   but all at once with line context. "Lint then run."

6. **Auto-detect / fallback.** The loaders auto-detect JSON vs the compiled-in
   static data (`docs/DATA_INTERNAL_JSON.md` §6). A broken JSON set can fall back
   to the built-in data (with a warning) so the game still boots — useful while
   editing.

---

## 3. Per-entity customization guide

### Add / remove a **country**
1. Add (or remove) the entry in `countries.json` (`{number, name, define,
   team_file}`). The `number` must be unique and have a `CN_*` define (or the
   loader maps by number directly).
2. Provide `teams/<NAME>.json` (a `TEAM.<country>` team set). Removing a country
   means nothing may reference it (validator checks competitions' seed lists /
   qualifier countries).
3. Caps: `allCountriesList` ≤ 66 (raise the array + assert to exceed).
4. Auto-derived: name↔number mapping, the team-file path.

### Add / remove a **team**
1. Edit `teams/<COUNTRY>.json` `teams[]` (append/remove a `{team, players}`).
2. The team **ordinal** is its index; removing a team **renumbers** the ones
   after it → any `team_seed_list`/pool reference to a later ordinal shifts.
   Prefer editing in place; the validator flags now-dangling ordinals.
3. Count is the array length (no separate count to update — the loader derives it).
4. Caps: teams-per-country is buffer-bound; 16 players each (exact).

### Add / remove a **player**
1. Edit a team's `players[]`. **Exactly 16** entries are expected per team
   (the bracket/positions assume 16). Fewer/more = validation error.
2. Packed bytes (`position_and_face`, the skill nibbles) follow `docs/DATA_JSON.md`
   §2; out-of-nibble-range values are masked but flagged.

### Add / remove a **competition**
1. Add an object to the right `competitions/*.json` with a unique `id`.
2. To make it playable in career, add a `career.json` contest entry referencing
   the `id` (the `*Copy` is derived).
3. Any cross-ref you introduce (`team_seed_list`, `comp_cup`, league links) must
   resolve — the validator checks the whole graph is closed (as the exporter
   already verifies: 0 dangling).
4. Caps: `careerContests` ≤ `CAREER_CONTESTS_CAP` (48); `teamSeedList` ≤ 32; etc.

---

## 4. What needs building (hardening backlog)

- [ ] **C load-time validators** for each loader (caps, counts, cross-ref
      resolution, TeamId range) returning a clear error — not a panic, not a clamp.
- [ ] **`scripts/validate_data.py`** pre-flight lint (mirrors the C checks; runs
      over `bin/data_json` and reports all issues with file/entity context).
- [ ] **Cap review**: decide which fixed arrays to enlarge for headroom vs keep
      exact, add `_Static_assert` size checks, document each in this file's table.
- [ ] **Registry** in C (`{id → object}`) shared by the competition loader and
      the validator; dangling-ref = hard load error naming the id.
- [ ] **Fallback policy**: on a validation failure, warn + fall back to built-in
      static data so the game still boots (editing safety net).
- [ ] **Euro-cup berth buckets** (`seed_buckets.json` `euro_cup_qual_seed_buckets`): the
      UEFA Cup berth allocation per country coefficient tier (Germany×4, Italy×4,
      …). Now stored readable as `{country, berths}` runs (was repeated country
      numbers). The C loader must re-expand to the raw `uint8` arrays + 0xFF
      sentinel. **Validate**: every `country` exists in the master AND has
      `>= berths` teams in its `teams/<NAME>.json` (else the UEFA draw picks a
      team that isn't there). Removing/renumbering a country requires fixing its
      bucket entry — the validator must flag it.

---

## 5. Security & integrity (multiplayer)

SWOS multiplayer needs both sides on **identical data**, for two reasons:

1. **Determinism / fairness** — the match sim must run on the same team/player
   stats on both machines. Different data → a divergent simulation (desync) or an
   unfair edge (a player who buffed their squad).
2. **Tamper detection** — a player can edit the JSON to cheat (max every stat).
   We must be able to tell the data isn't the agreed-upon set.

(Same-machine 2-player is trivially fine — shared data. This matters for
**network** play.)

### What to hash — the canonical image, not the JSON text

Hash the **canonical binary image** the loader produces (the `encode_*` output /
the in-memory structs in the endianness-explicit on-disk byte order), **not** the
raw JSON bytes. Pretty-vs-compact, key order and whitespace must NOT change the
hash, and LE/BE + 32/64-bit must agree. This reuses the cross-target byte image
the save/data format already defines (`docs/DATA_JSON.md`), so the digest is
stable across formatting and platform.

### Manifest

The exporter/validator emits `bin/data_json/manifest.json`:

```json
{
  "data_version": "1996.1",
  "files":  { "teams/SCOTLAND.json": "<hash>", "competitions/cups.json": "<hash>", "…": "…" },
  "digest": "<hash over all per-file hashes, sorted>"
}
```

Per-file hashes pinpoint **which** file differs; the top-level `digest` is the one
value exchanged for a quick match/no-match.

### CRC vs cryptographic hash — ship both

- **CRC32** — fast, and a primitive already exists (the RNC code). Enough for the
  always-on **accidental**-mismatch check ("we have different mod versions").
  Trivially forgeable, so not anti-cheat.
- **SHA-256** (small public-domain impl) — for **tamper / official-data**
  verification: an allowlist of known-good digests lets a host require unmodified
  data; any mod changes the digest.

### Protocol (at match setup)

1. Each side computes its digest at data load.
2. Exchange `data_version` + `digest` on connect.
3. Mismatch → **refuse or warn**, and (via the per-file manifest) name the files
   that differ; optionally offer "play anyway (unranked)".
4. Ranked/official play requires the digest to be on the known-good allowlist.

### Granularity

- **Whole-corpus** digest — simplest; both sides need the exact same data set.
- **Match-scoped** — hash only the two teams + the competition in play, so players
  with different *unrelated* mods can still play when the *relevant* subset
  matches. More flexible, more complex — a later option.

### Honest limits

P2P anti-cheat is **best-effort**: a determined cheater controls their binary and
can spoof any hash. Realistic guarantees: (a) detect accidental data mismatch,
(b) detect casual tampering against an agreed/official digest, (c) enable
official-data-only matches. **Strong** anti-cheat needs a trusted host/server that
owns the authoritative data and runs/validates the sim. And note: identical data
is *necessary but not sufficient* for determinism — the sim code + RNG must match
too (same build).

### Backlog

- [ ] `manifest.json` generation (exporter + `validate_data.py`): per-file hashes
      + overall digest over the **canonical image**.
- [ ] C digest at load (reuse the CRC primitive; add SHA-256 for official mode).
- [ ] Multiplayer handshake: exchange `data_version` + `digest`, refuse/warn on
      mismatch, name the differing files.
- [ ] Official-data allowlist for ranked play.

---

## 6. Relationship to the other data docs

- `docs/DATA_JSON.md` — the external `bin/data` (teams/TMD/pool) JSON format.
- `docs/DATA_INTERNAL_JSON.md` — the `competition_data.c` → JSON plan + the
  cross-reference model this validation protects.
- `docs/DATA_PIPELINE.md` — the pipeline as built (runtime load + build-time bake).
- `docs/DATA_BSON.md` — the Amiga fast-binary question (decided: the Amiga compiles
  C baked from the JSON at build time); whatever the Amiga
  reads, JSON stays the single editable source, so validation lives at the JSON
  boundary on the host/SDL side.
