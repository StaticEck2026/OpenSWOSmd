# Competition System — Dynamic / Pointer-Free Refactor (HANDOFF)

> Status (2026-09-30): **complete** -- engine Steps 1-7 (`cc75dcf`, `061e979`; `include/comp_identity.h`, `Comp_ById`/`Comp_AppendCareerContest`) and Copa Libertadores incl. the Step-8 Amiga bake (`a6f0c31`, `d66453a`, `580ca70`) and the interactive CONMEBOL-club play path listed as remaining in §8e (`206ffd3`) all landed. Kept as the record of how. Mentions of `sync_to_amiga_port.sh` / "the Amiga port" as a separate tree predate the 2026-08-12 merge (the script is now a tombstone; one tree builds both targets).

> Fresh-session handoff. Goal of this project: **break competitions free of
> hardcoded compiled addresses** so they are identified by id/index (not by the
> address of a compiled global), the competition tables are **built at runtime**
> from data, and a brand-new competition is addable as **pure data** (JSON) — no
> compiled record, no spare backings. This is the next logical step after the
> data-drive untangle; the user explicitly chose the full refactor over the
> interim "pool" approach. "We really don't want to do pointer comparisons anyway."

## 0. Status at handoff

- **Repo is clean.** The interim *pool* attempt (spare `*Copy` backings +
  `_dataCompPool[]` + `_careerContests[43]` spares) was **reverted** this session —
  do not look for it; it's gone. Start from a clean tree on `main`.
- **Committed + pushed earlier this session** (unrelated to this refactor): arcade
  DOS logo (`2bfe820`), Cygwin SIGHUP / VS Code gdb launch fix (`5785a74`),
  `bin/data_json` now tracked (`88def1a`), `grafs_aga` removed (`5459229`).
- `bin/data_json` is the data-drive source of truth (user keeps a `data_json.zip`
  backup and will **re-baseline it before any Copa data goes in**).
- **Motivating test case:** Copa Libertadores (a brand-new tournament). Spec in
  `docs/COPA_LIBERTADORES.md` and §6 below. It does **not** go in until the engine
  refactor is done and the user re-baselines `data_json`.

## 1. The core problem (verified by two thorough audits)

**The competition JSON loader is patch-only, not allocate.** `save_format_json.c`
("Competition JSON loader", ~line 2929; the `sg[]` known-tournaments table ~3487;
the cup/league/playoff equivalents nearby) maps each JSON competition **id → a
known compiled global** and overwrites it in place, gated by `comp_number` match
(e.g. `*(CompetitionTournament *)sg[gi].g = tmp;` ~3529). 162/162 records load
this way. There is **no path to create a competition with no compiled
counterpart** — which is why a new id in `tournaments.json` does nothing.

**Why it was built patch-in-place: the game's by-ADDRESS competition identity.**
That is the thing this refactor removes. Inventory + convert:

- **By-address gates** (compare a runtime `Competition*` against a compiled global):
  - `career_season.c:~1468` — `if (competition != (uint8_t *)&_englishLeague) return;`
    (this exact by-address pattern caused the 0x4444 panic earlier — it is
    load-bearing; convert carefully).
  - Grep the tree: `== &_`, `!= &_`, `(uint8_t *)&_`, `(Competition *)&_`,
    `&_*League`, `&_*Cup`, `&_*Playoffs`. Also by-NAME reads of `_englishLeagues1/2`.
- **Tables that hold `&_comp` pointers** (these become runtime-built / by-id-indexed):
  - `_careerContests[35]` (competition_data.c:1557) — `{ &template, &templateCopy, sizeof(template) }`, sentinel `{0xFFFFFFFF,0,0}`.
  - Region tables `QualifyingGroup` — `_southAmericaTable`, `_europeTable`,
    `_africaTable`, `_northAmericaTable`, `_asiaTable`, `_oceaniaTable`,
    `_worldTable`, `_customsTable` (competition_data.c:1645-1683). Shape:
    `{ Competition *comps[...,-1]; uint8 countryNums[...,255]; }`.
  - `_continentalCups[7]`, `_nationalTeamCompetitionsAndQualifications[39]` (1599-1628).
  - `_worldCupQualifyingGroups`, `_ecQualifyingGroups` (1635-1643).
  - Country tables `_albaniaTable[4]`…`_englandTable[5]`… (1689+) and the master
    `_competitionsTable[]` — NOTE the loader already runtime-swaps the per-country
    tables (`dj_graph_append_ids` / `s_djCountryTables`, save_format_json.c ~3451);
    that mechanism is a good model to generalise.

## 2. The `*Copy` mechanism (career working copies) — important

`career_comp_copies.c` reserves, as a `.bss .skip` region, one **runtime working
copy** per competition (`_euroCupCopy` … `_oceaniaWorldCupQualificationCopy`,
112 B + 8 on 64-bit for the `text[2]` pointers). These are **reconstructed from
the static template via `_careerContests[]`** — NOT byte-loaded:

- `career_init.c:InitCareer` (~684) iterates `_careerContests[]` and memcpy's
  `template -> *Copy`.
- The JSON save reconstructs them the same way: `reconstruct_comp_copy_region_-
  from_templates` (save_format_json.c ~4089-4205). Only 2 dynamic bytes are saved.

⇒ A dynamic competition needs its `*Copy` allocated **and** registered in whatever
`_careerContests` becomes, so both the InitCareer fill and the save reconstruction
pick it up by walking the (now runtime-built) contest table.

## 3. Save format interaction (be careful — fragile area)

- The **JSON save** (`src/save_format_json.c`, the live format) already serialises
  competition references by **semantic id**, not address, and re-derives pointers
  on load. So dynamic competitions are compatible *iff* keyed by a stable id.
  Keep `SWOS_SAVE_FORMAT_JSON_VERIFY=0` green throughout.
- The legacy **`.CAR`** is a raw address-based memory dump — it will NOT round-trip
  dynamic competitions. That's acceptable (roadmap step 4 retires it). Don't try to
  keep `.CAR` working for new competitions.
- **KEEP** binary readers for `.TAC / .HIL / .EDT / .CFG` (community/fixed-layout).
- Any save-encoding change → tell the user so they regenerate `TEST.CAR` (a stale
  save can masquerade as a load-path bug).

## 4. Already data-driven (NO work needed)

- Competition **name strings** — `dj_intern_names` resolves the `_aXxx` symbols
  from the JSON `name` array (save_format_json.c ~2997).
- **Top-3 + best-4th group advancement** — handled by `divisionFactor`/groups +
  global re-rank (diy_competition.c ~3848-3908). Copa's 15+1 needs no new code.

## 5. Refactor plan (incremental; verify each step on BOTH ports)

1. **Inventory** every by-address competition reference (gates + table
   initialisers) using the greps in §1. Produce the full list **before** changing
   anything. This is the spine of the project.
2. **Pick the identity.** Recommend the existing `comp_number` (already the loader's
   match key) or the JSON string `id`. Add an accessor — `Competition *comp_by_id(...)`
   and/or an index — and a stable `comp_id(Competition*)`.
3. **Convert the gates** `competition == &_x` → `comp_id(competition) == ID_x`
   across `career_season.c`, `career_eurocups.c`, `diy_competition.c`, season
   transition + qualification logic. One competition at a time; build + (ideally)
   the harness season-loop after each.
4. **Runtime tables.** Generalise the existing per-country table swap
   (`dj_graph_append_ids` / `s_djCountryTables`) to also build `_careerContests`,
   the region/continental tables, etc., at startup — pointing at existing compiled
   records **plus** any new heap records. Decide: heap arrays behind the `extern`
   symbols (cleanest — but the symbols are arrays today, so consumers that take the
   array base must move to a pointer) vs. compiled-with-runtime-fill.
5. **Dynamic new competitions.** Loader heap-allocates record + `*Copy` for any
   JSON id with no compiled record; appends to the runtime tables + contest table.
   No pool, no spare `.skip` backings.
6. **Save round-trip** — VERIFY=0 with a dynamic competition present and across a
   season + save/load.
7. **Amiga bake path** (`C:\Projects\AI\SWOS\SWOS_C`): the Python emitters bake the
   same tables (compiled) but the **shared C accesses them by-id**, so the by-id
   conversion is what makes the two ports share code. New competitions get baked
   into the Amiga tables. Verify the m68k build + a season. Do NOT auto-commit the
   Amiga port — `sync_to_amiga_port.sh` copies; the user reviews + commits there.
8. **Prove it:** add Copa Libertadores as pure data (§6); group phase + 2-leg
   knockout run; then Gap B (career qualifier).

## 6. Copa Libertadores — the test case (spec ready; add AFTER the engine)

Doc: `docs/COPA_LIBERTADORES.md`. **All 20 club teams already exist** in
`bin/data/TEAM.<cc>`; confirmed `{country, ordinal}` seed list in the §3 group
layout:

| Grp | | | |
|---|---|---|---|
| **A** | BARCELONA `{50,0}`, ESPOLI `{50,6}` | + | CERRO PORTENO `{64,2}`, OLIMPIA `{64,7}` |
| **B** | PENAROL `{71,17}`, DEFENSOR SPORT `{71,8}` | + | SPORTING CRISTAL `{65,12}`, UNIVERSITARIO `{65,15}` |
| **C** | AMERICA CALI `{49,0}`, ATLETICO JUNIOR `{49,4}` | + | SAN JOSE ORURO `{45,11}`, GUABIRA `{45,4}` |
| **D** | CORINTHIANS `{46,5}`, BOTAFOGO `{46,3}` | + | UNIV. DE CHILE `{48,15}`, UNIV. CATOLICA `{48,14}` |
| **E** | RIVER PLATE `{43,33}`, SAN LORENZO `{43,36}` | + | MINERVEN `{77,13}`, CARACAS `{77,2}` |

(Country disambiguates duplicates: RIVER PLATE exists in both Argentina `{43,33}`
and Uruguay `{71,22}`; the group draw resolves which.)

**Record design:** mirror `_copaAmerica` (competition_data.c:575 — S-American
group→knockout) with `playEachTeam=2` (double RR), `rounds[0]={20,5,4}` (5×4),
`awayGoalsMode=0`, the 20-team `teamSeedList`, and the 2-leg knockout
`roundRuleFlags` from `_euroCup` (competition_data.c:196-199 — `{0,-108,-108,20}`).
5 groups is supported (`_asianCup` uses `{30,5,6}`). Add a `_aCopaLibertadores`
name (JSON `name` array → already data-driven).

**Career qualification (Gap B):** champion + runner-up of the 10 CONMEBOL leagues →
next-season Copa. The 10 club leagues **exist**: `_argentinianLeague`,
`_bolivianLeague`, `_brasilianLeague`, `_chileanLeague`, `_colombianLeague`,
`_ecuadorLeague`, `_paraguayanLeague`, `_peruvianLeague`, `_uruguayanLeague`,
`_venezuelanLeague`. Qualification today is hardcoded by address in
`career_season.c:processDiyLeagueQualificationEntry` (~1446, only `_englishLeague`)
— it gets the **same by-id treatment** as everything else, plus a routing table
(which leagues + which finishing positions feed which cup). Model:
`career_eurocups.c`.

## 7. Key files

`competition_data.c` (records + all the tables), `competition_data.h` (externs),
`career_comp_copies.c` (`*Copy` storage), `save_format_json.c` (loader + save +
the per-country table-swap model `dj_graph_append_ids`), `career_init.c`
(InitCareer + `*Copy` reconstruct), `career_season.c` (qualification + by-address
gates), `career_eurocups.c` (euro qualification model), `diy_competition.c`
(group→knockout advancement), `bin/data_json/competitions/*.json`.

## 8. First moves in the fresh session

1. Confirm tree clean (pool reverted — done at handoff).
2. Run the by-address **inventory** greps (§1) → produce the complete list of
   gates + table initializers before touching anything.
3. Decide the **identity scheme** (§5.2) and present a concrete, ordered conversion
   plan (which gate / table first, build+test cadence) before editing.

## 8b. STATUS 2026-06-27 — ENGINE COMPLETE (Steps 1-7 done); Copa is next

**The whole competition engine refactor (Steps 1-7) is DONE, committed, pushed,
synced, and Amiga-verified (builds + runs).** Summary of what shipped this session
(see the memory note `project_competition_dynamic_refactor` for commit-level detail):

- **By-id identity** — every by-ADDRESS competition gate → `comp_id(c)==COMPID_x`
  (`include/comp_identity.h`: enum + accessor + `comp_is_wc_qualifier`/
  `_ec_qualifier`/`_continental_cup`/`_tour_or_friendly`). Cat-1 slot-occupancy +
  Cat-5 DIY buffers left as object identity (correct).
- **Runtime-fill tables** — `_careerContests[35]→[CAREER_CONTESTS_CAP=48]` +
  `Comp_AppendCareerContest()`; `comp_by_id` registry (`Comp_Register`/`Comp_ById`).
- **Dynamic comps** — `DataJson_InstallCompetitionGraph` heap-allocates a record +
  `*Copy` for any tournaments.json id whose compId matches no compiled global,
  appends to `_careerContests`, registers it. (Detect by compId, not id-string.)
- **Save-safe** (Step 7) — career load/save round-trips with a dynamic comp present;
  the load-time reconstruct rebuilds the dynamic heap `*Copy`. VERIFY=0 green.
- **id-prefix fix** — loader sg[]/sp[]/R[]/QT[] ids prefixed `_` to match the data
  (was no-op'ing into compiled fallback); install now drives all standalone+region
  comps from JSON, regionMiss=0 (byte-identical).
- **Naming convergence** — three things were all called "comp id"; now distinct:
  `compIndex`/`_currentCompetitionIndex` (the `_competitionsTable` country index),
  `compSlot` (DIYCompetition 0x2C slot/cache-key word, pairs w/ slotId), `compId`/
  `comp_id()`/`COMPID_*`/JSON `comp_id` (the byte-0 identity). Save key
  `comp_id`→`comp_slot`; existing saves migrated via
  `scripts/migrate_save_keys.py comp_id=comp_slot` (40 files / 216 keys, SDL+Amiga).
- All MILAN-verified no-op (rep=49, panic=0); builds x86_64+arm64+armhf.
- ⚠️ `data_json` changed (the comp_id key rename) — re-zip the `data_json.zip` backup.
- ⚠️ Known deferred: the 0x2C `compSlot` is overloaded — read as `compType` in
  `UpdateCareerFinancesAfterMatch` (`career_status.c`, commented). A future bigger
  rename session may reconcile it with `competitionType@0x00`.

## 8c. COPA LIBERTADORES — grounded plan (NEXT; user chose "replicate euroCup")

User direction: **make Copa like the European Champions Cup** (`euroCup`) — i.e.
the SIMPLE thing, no custom rules. This resolves §c's open questions: seeded
bracket, draw from the qualified teams, NO same-nation-before-QF constraint, NO
holder berth, top-3+best-4th for the 16th. Prereqs CONFIRMED: the 10 CONMEBOL club
leagues exist as compiled comps; all CONMEBOL `TEAM.<cc>` files exist (20 clubs
present). **No new tournament-engine wiring needed** — `euroCup` already does
group→2-leg-knockout, and `_asianCup` (comp_id 34) already does a 5-group bracket.

**THE TEMPLATE: `_asianCup`'s shape + `_euroCup`'s 2-leg flags.**
```
asianCup rounds = [{30,5,6},{16,0,16},{8,0,8},{4,0,4},{2,0,2},{1,1,1}]  (single-leg)
COPA     rounds = [{20,5,4},{16,0,16},{8,0,8},{4,0,4},{2,0,2},{1,1,1}]  (2-leg KO)
                   group     R16        QF       SF       final   winner
```
`rounds[6]` fits exactly (no terminal {0,0,0} needed; winner at [5]). Copa flags:
group `{0,0,0}`, then 2-leg knockout from euroCup `{2,1,1}` for R16/QF/SF (final:
euroCup uses single-leg `{0,1,1}`, but Copa spec wants 2-leg — minor, pick one).
play_each_team=2 (double RR). away_goals: euroCup=3(on); Copa-spec=0(off) — user
said replicate euroCup, so 3 is defensible (flag if it matters).

**NOTE on round_rule_flags array size:** euroCup emits 4 flag entries, asianCup
emits 6 — confirm the struct/emitter handles Copa's needed count when authoring.

**20-team seed list (group order A-E, {country, ordinal}) — from handoff, confirmed:**
```
A: {50,0} BARCELONA, {50,6} ESPOLI, {64,2} CERRO PORTENO, {64,7} OLIMPIA
B: {71,17} PENAROL, {71,8} DEFENSOR SPORT, {65,12} SPORTING CRISTAL, {65,15} UNIVERSITARIO
C: {49,0} AMERICA CALI, {49,4} ATLETICO JUNIOR, {45,11} SAN JOSE ORURO, {45,4} GUABIRA
D: {46,5} CORINTHIANS, {46,3} BOTAFOGO, {48,15} UNIV. DE CHILE, {48,14} UNIV. CATOLICA
E: {43,33} RIVER PLATE, {43,36} SAN LORENZO, {77,13} MINERVEN, {77,2} CARACAS
```
(country = high byte of team id; verify each {cc,ord} against `bin/data/TEAM.<cc>`.)

**Increments:**
1. **Copa record as DATA** — add to `bin/data_json/competitions/tournaments.json`:
   clone `_asianCup` shape with `{20,5,4}` round 0, `_euroCup` 2-leg flags, the 20
   seeds above, a FREE `comp_id` (e.g. 60 — confirm unused; compNumbers max ~127),
   `play_each_team=2`, and a name entry. Add the `_aCopaLibertadores` name symbol
   (JSON `name` array → already data-driven via `dj_intern_names`). The dynamic
   loader (Step 6) heap-loads it automatically — NO compiled record needed.
2. **Verify** via harness: boot → `Comp_ById(copaId)` non-NULL, in `_careerContests`,
   `*Copy` reconstructs; then make it active and play a group→knockout run.
3. **CONMEBOL qualifier (Gap-B)** — the real new logic. Generalise
   `career_eurocups.c`: at season end, champion+runner-up of each of the 10 leagues
   (`_argentinianLeague`.._venezuelanLeague) → next-season Copa's 20-team field,
   drawn into groups A-E (nation-pairing, see docs/COPA_LIBERTADORES.md §3/§6).
   Career qualification today is hardcoded by address — `career_season.c`
   `processDiyLeagueQualificationEntry` (~1446, only `_englishLeague`); it now uses
   the by-id treatment + a routing table. Model: `career_eurocups.c`.
4. **Strings/screens** — `_aCopaLibertadores` title, group labels, captions.

## 8d. INC1 DONE + INC2 (load) DONE; INC3 grounded design (2026-06-28)

**INC1 (record) — DONE, runtime-verified, UNCOMMITTED.** `_copaLibertadores` added to
`tournaments.json` (count 13→14). Runtime-loaded with no rebuild (data is read at
startup): SWOS.log `dynamic tournament '_copaLibertadores' compId=128 -> heap record +
careerContests` / `1 dynamic competition(s)`. gdb-verified `Comp_ById(128)` = heap record
(type2, teamNumber-1, playEachTeam2, awayGoals0, rounds `{20,5,4}{16,0,16}{8,0,8}{4,0,4}{2,0,2}{1,1,1}`,
flags `00 84 84 84 84 00`, 20 seeds), `_careerContests[34]`=record + sentinel re-planted
[35]. All 20 `{cc,ord}` seeds verified vs `bin/data_json/teams/*.json` (20/20).
- ⚠️ **comp_id 60 was NOT free** (handoff error — it's a league). **ALL 0-127 are taken**;
  new comps use **128-255**. `Competition.compId` is **`int8_t` (signed)** — 128 stores as
  byte 0x80 but works because every identity read casts `(uint8_t)` (dj_comp_base store,
  dynamic-detect, Comp_Register index, comp_id()). **Before Step-8 Amiga bake: widen compId
  int8_t→uint8_t** or emit_competitions.py warns on `{128,...}` (layout/size/save unchanged).
- **MATCH RULES = REAL-COPA (user-chosen 2026-06-28):** away_goals **OFF (0)**, **no ET**,
  **2-leg KO incl. final**, penalties on. (NOT euroCup's away-on/ET-on/single-leg-final.)

**INC2 (load) — DONE.** Verified above. Play-through validation folds into Inc3 (Copa plays
via the career path, per user choice). Standalone activation path if ever needed:
`_selectedContest = Comp_ById(128)` → menus.c:3440 dispatch.

**INC3 (CONMEBOL career qualifier) — fully RE'd, NOT yet implemented. The MODEL is
`Career_ProcessCompetitionForEuropeanCupQualification` (career_eurocups.c), NOT
`processDiyLeagueQualificationEntry`** (that's English promotion-PLAYOFF handling — a
handoff conflation). Season transition = `Career_AdvanceToNextSeason_UpdateCompetitions`
(career_season.c:1642):
1. `Career_PrepareEuropeanCupQualificationState()` (career_season.c:173) — per cup: set
   `_xCupTeamListPtr/WritePtr` INTO the cup `*Copy`'s embedded seed list (`base[7]` offset +
   `2*(sizeof(char*)-4)` 64-bit delta), reset count, build the eligible-id list via
   `Career_FillRandomTeamListExcludingSelected(excl, seedLists, eligibleIds)`. **KEY: the
   qualified-team storage IS the comp's `*Copy` seed list — Copa's lives in
   `_careerContests[34].competitionCopy`'s teamSeedList (heap, save-reconstructed). Minimal
   new BSS: just transient `_copaTeamListPtr/WritePtr` + `_copaQualifiedTeamCount`.**
2. `Career_UpdateEuroCup*CompetitionSlot()` (×3) — rebuild each cup `*Copy` team list for
   next season (interactive if player's quaternary comp, else auto-advance + copy grid).
3. In the per-competition loop (player comp @1668 + `_careerCompetitionIdRemapTable` walk
   @1673): `Career_ProcessCompetitionForNextSeason(idx)` (advances comp, caches top-9
   finishers into `_primaryQualifiedTeamGrids` via DIY_CachePrimaryQualifiedTeamsGrid, sets
   `_currentCompetitionIndex`) THEN `Career_ProcessCompetitionForEuropeanCupQualification()`
   (if `_countriesTable[idx][0]==0` i.e. European, append finishers to cup lists + TMD pool).
4. `if (_careerFileBuffer == 0x50)` → `Career_DrawEuropeanCupTeamsAndCopyGrids()` + validate
   `euroCup==16 && CWC==32 && UEFA==32` (else panic 0x8998).

**Routing CONFIRMED:** the 10 CONMEBOL country numbers {43,45,46,48,49,50,64,65,71,77} are ALL
in `_careerCompetitionIdRemapTable` (competition_data.c:1834) → simulated every season →
each yields champion(slot0)+runner-up(slot1) in `_primaryQualifiedTeamGrids`. Copa eligible
list = those 10. Copa field = top-2 × 10 = exactly 20 (DETERMINISTIC — no random fill needed,
unlike euro).

**The Copa siblings to write (mirror euro):**
- `Career_PrepareCopaLibertadoresQualificationState()` — ptr into Copa `*Copy` seed list
  (look up `_careerContests` entry whose `.competition==Comp_ById(128)`, use
  `.competitionCopy`), reset `_copaQualifiedTeamCount`, eligible-id list = the 10 countries.
- `Career_ProcessCompetitionForCopaLibertadoresQualification()` — if `_currentCompetitionIndex`
  is CONMEBOL (consume from eligible list), append `_primaryQualifiedTeamGrids` slot0+slot1 to
  the Copa list via `appendTeamToList` (BE byte pairs + TMD pool).
- `Career_UpdateCopaLibertadoresCompetitionSlot()` — rebuild Copa `*Copy` for next season
  (auto-advance; interactive only if player manages a CONMEBOL club — DEFER, auto-only first).
- Hook all three into `Career_AdvanceToNextSeason_UpdateCompetitions` as siblings of the euro
  calls (Prepare after euro-prepare; Update after euro-updates; Qualify after each euro-qualify).

**⚠️ THE GNARLY BIT — RESOLVED (2026-06-28): the `0x50` gate is a TMD-APPEND COUNT, not a
region code.** `_careerFileBuffer` (career-overlay HEAD, `uint16_t`) is the TMD-pool team
count: `Career_AppendTeamToTmdTeamPool` (career_teams.c:444 `count=_careerFileBuffer` /
:451 `_careerFileBuffer++`) increments it per appended team. It's reset to 0 at
career_season.c:1664, then the qualification loop appends the euro cups' teams; at the end
`if (_careerFileBuffer == 0x50)` means **80 teams appended = 16 euroCup + 32 CWC + 32 UEFA**.
The block draws title holders, validates `euroCup==16 && CWC==32 && UEFA==32 && ==0x50`, and
`return`s; **falling through (count != 0x50) hits `panic 0x8999`** at career_season.c:1701.
So every career ALWAYS ends with exactly 80 euro appends (the euro cups qualify in every
career regardless of player region — they're all in the remap table). **⇒ Copa's qualifier
appends 20 MORE teams to the TMD pool (via the same Career_AppendTeamToTmdTeamPool), pushing
`_careerFileBuffer` to 100 (0x64) → the `==0x50` gate FAILS → panic 0x8999 on EVERY career.**
The 3b fix: generalise the magic `0x50` to `0x50 + Career_CopaLibertadoresQualifiedTeamCount()`
(80→100 when Copa is present) and add `&& copaCount==20` to the validation. Copa needs NO
title-holder draw (no holder berth). This is a season-transition + BOTH-PORTS change → verify
with a full-season harness run; the magic constant lives in the shared C so it back-ports.

**⚠️ Save/scope notes:** transient state (`_copaTeamListPtr/count`) is recomputed each
transition (likely no save change — the team data lives in the Copa `*Copy` which already
saves). If any persistent Copa state is added (e.g. a title holder), it's a SAVE-FORMAT
change → heads-up the user. Slices: **3a — DONE (2026-06-28, x86_64 build green, gdb-unit-tested).** New
`src/career_copa.c` (Makefile auto-globs `src/*.c`, no edit needed): the 10-CONMEBOL routing
(`s_copaEligibleMaster` country nums + consume-once working copy), transient
ptr/count state, `Career_PrepareCopaLibertadoresQualificationState`,
`Career_ProcessCompetitionForCopaLibertadoresQualification`,
`Career_CopaLibertadoresQualifiedTeamCount`. gdb test (simulate InitCareer memcpy
template→copy, _careerFileBuffer=0 so the TMD append is safe): qualify ARG(43)→count2,
Copa *Copy seed[0]=43/33 seed[1]=43/36; BRA(46)→count4 seed[2]=46/5 seed[3]=46/3;
non-CONMEBOL(7) rejected; re-ARG consumed→no dup. `COMPID_copaLibertadores=128` added to
comp_identity.h. NOT yet hooked into the season transition.
**3b-part-1 — DONE (2026-06-28, HARNESS-VERIFIED on a real MILAN career, UNCOMMITTED).**
Hooked `Career_PrepareCopaLibertadoresQualificationState` (after career_season.c:1649) +
`Career_ProcessCompetitionForCopaLibertadoresQualification` (after :1669 player comp + :1681
remap loop) into `Career_AdvanceToNextSeason_UpdateCompetitions`. **Field-building only —
the TMD-pool append is DEFERRED** (commented in appendTeamToCopaList), so `_careerFileBuffer`
stays 80, the `0x50` count gate is untouched (no panic 0x8999), and no buffer/save change.
Harness (VERIFY=0 build, gdb-proxy daemon, MILAN.CAR loads at the CareerOverInit hub):
season 3→4 transition ran the hooks → **count AFTER = 20, panic = 0**, Copa *Copy seed list
rebuilt from the preset 1996 field to the EARNED field = champion+runner-up of all 10
CONMEBOL leagues (2 per nation: 43/24,43/38 45/4,45/0 46/1,46/9 48/14,48/7 49/10,49/4
50/5,50/4 64/6,64/2 65/1,65/6 71/8,71/16 77/12,77/17 — countries {43,45,46,48,49,50,64,65,71,77}).
The season-4 `DIYCupInit` drive_inseason stall is the pre-existing documented harness
limitation, NOT this change. Harness recipe in scripts/copa_harness.py (scratch).
**3b-part-2 — DONE (2026-06-28, HARNESS-VERIFIED, UNCOMMITTED). Copa is now CONTESTED.**
- `Career_UpdateCopaLibertadoresCompetitionSlot` (career_copa.c) auto-advances Copa each
  season via `Career_AutoAdvanceLeagueCompetitionFromRecord(copaCopy)` (mirrors euroCup's
  non-interactive branch; that fn loads its own field from the record seed list + InitTeams,
  so no pre-staging needed; no title-holder grid — no holder berth). Hooked after the euro
  UpdateSlots (career_season.c, after `Career_UpdateUefaCupCompetitionSlot()`). Player-in-
  CONMEBOL INTERACTIVE path still deferred.
- TMD-pool append RE-ENABLED in appendTeamToCopaList.
- `_tmdFileBuffer` ENLARGED 80→**128** (career_team_buffers.c `.skip 54720`→`87552`; logical
  max is 100 = 80 euro + 20 copa, +margin). Save is COUNT-DRIVEN (`tmd_team_count`) so
  backward-compatible: old ≤80 saves load fine; new saves carry up to 100 `tmd_teams`.
  ⚠️ SAVE heads-up given to user. The auto-advance does NOT grow the TMD count (only the
  qualifier append does), so the pool peaks at 100.
- `0x50` gate GENERALISED (career_season.c:1699 + the validation): `0x50 +
  Career_CopaLibertadoresQualifiedTeamCount()`, and the validation adds
  `Career_CopaLibertadoresQualifiedTeamCount() == Career_CopaLibertadoresExpectedTeamCount()`
  (expected = 20 if Copa installed else 0 — so a build WITHOUT Copa data stays correct, gate
  reverts to the original 0x50).
- HARNESS (MILAN.CAR, VERIFY=0): season 3→4 with all of the above — **panic=0, TMD count=100,
  Copa count=20, earned 20-team CONMEBOL field**, season reached 4 (then the pre-existing
  DIYCupInit stall). Isolated `Career_UpdateCopaLibertadoresCompetitionSlot()` gdb call:
  **panic=0** (the 5-group type-2 club auto-advance — a previously-untested engine path — runs).
- Files (UNCOMMITTED): bin/data_json/competitions/tournaments.json, include/comp_identity.h,
  src/career_copa.c (NEW), src/career_season.c, src/career_team_buffers.c.

**INC4 (strings) — DONE (2026-06-28, gdb-verified).** Added `_aCopaLibertadores` +
`_aCopaLibertadores_0` = "COPA LIBERTADORES" to strings_data.c; regenerated string_symbols.c
(gen_string_symbols.py, 1074 symbols) + en.json (export_translations.py) + the 5 non-en files
(gen_translations.py — clean +2 lines each, "COPA LIBERTADORES" as a proper-noun English
fallback). gdb: `Comp_ById(128)->name[0]` now resolves to "COPA LIBERTADORES" via tr() (was
the literal symbol). Group labels (A-E) + round captions are SHARED tables (round_string_base),
so no Copa-specific strings needed — NOTE round_string_base=4 (euroCup) labels the bracket
euroCup-style; if the interactive path shows a wrong "ROUND OF n" caption, switch to 5
(asianCup's 16-team set). en.json is gitignored (regen from committed strings_data.c).

**compId WIDENING — DONE (2026-06-28).** game_types.h Competition.compId int8_t→uint8_t
(+ loader store cast save_format_json.c:2975). Audited all direct `.compId` reads — safe
(cast uint8_t or compare same-type fields, values ≤127). Build clean, dynamic install intact
(compId=128), full season harness PASS. Unblocks the Amiga bake (no `{128}` int8_t overflow).

**BACK-PORT (sync) — DONE (2026-06-28).** `career_copa.c` added to sync_to_amiga_port.sh
SRC_FILES; `--apply` copied 12 files (8 changed src + career_copa.c new + 3 changed include)
to /mnt/c/Projects/AI/SWOS/OpenSWOS. CORRECTION: the Amiga Makefile DOES auto-glob `*.c`
(Makefile:27 `$(wildcard *.c) $(wildcard $(subdirs)*.c)`) — so NO Amiga Makefile edit needed;
the handoff's "not auto-globbed" was wrong. Safe by design: on Amiga, Copa isn't installed
(no runtime JSON parse, not baked) → `Comp_ById(128)`=NULL → career_copa.c functions no-op +
the gate reverts to 0x50 → Amiga behaves identically until Step 8. ⚠️ NO m68k toolchain on
this box → USER builds m68k + tests + commits (do NOT auto-commit the Amiga port).

**REMAINING for the full Copa feature:** (a) the INTERACTIVE play path (human manages a
CONMEBOL club → plays Copa in real-time — the euro `a4 == _quaternaryCompetitionPtr` branch;
needs a CONMEBOL-club career to test, which the MILAN harness doesn't cover); (b) USER: m68k
build + test + commit the Amiga back-port; (c) Step 8 Amiga bake (emit_competitions.py bakes
Copa into competition_data.c) — GATED on (d); (d) USER re-baselines data_json + re-zips
data_json.zip. Validate via the MILAN (European) career: the 10 CONMEBOL leagues
auto-advance, so Copa should populate to 20 even though the player isn't South American —
BUT only if the readiness gate is generalized so the Copa draw runs for a non-SA career.

**Then add Copa to data PERMANENTLY → user re-baselines data_json.** Amiga: Copa
gets BAKED into competition_data.c via `scripts/emit_competitions.py` (Step 8 — the
Amiga doesn't runtime-parse JSON; it bakes). The shared by-id C makes both ports
work; verify the m68k build + a season after.

## 8e. STEP 8 — AMIGA BAKE — DONE (2026-06-28); approach: amiga-only via a gated file

User chose **amiga-only bake** (SDL stays dynamic — keeps the pure-data demo). The
naive "emit Copa into competition_data.c" does NOT work, for two reasons found while
scoping:
1. `emit_competitions.py` only **replaces** existing `TYPE name = {...}` definitions
   by name — it cannot **insert** a brand-new comp, and Copa (pure data) has no
   compiled record to replace.
2. The **Amiga build does not run the emitters** — it compiles the *synced*
   `competition_data.c` (identical to SDL's). So an emit-into-competition_data.c bake
   would be empty on the Amiga, or (if baked for both) leak onto SDL and kill the
   dynamic path.

**Done instead:** `src/competition_data_copa_baked.c` — whole-file `#ifndef
SWOS_SDL_BUILD` (synced; compiled ONLY on the Amiga, EMPTY on SDL). It defines a
compiled `_copaLibertadores` (literal generated by `emit_tournament` from
tournaments.json — regenerate if the JSON changes), a `_copaLibertadoresCopy`, and
`CopaBaked_Install()` that calls `Comp_AppendCareerContest` + `Comp_Register` — the
*same* path the SDL dynamic loader uses. `frontend.c` calls `CopaBaked_Install()` in
the `#else` of the SDL `DataJson_InstallCompetitionGraph` block. **`career_copa.c` is
unchanged** — `Comp_ById(128)` resolves the registered comp on both ports.

- SDL: file empty → Copa stays DYNAMIC (verified: still `dynamic tournament
  '_copaLibertadores'`, `1 dynamic competition(s)`).
- Amiga: **VERIFIED on real FS-UAE** — `Comp_ById(128)` = `0x2c64b0 <_copaLibertadores>`
  (was NULL/inert), `Career_CopaLibertadoresExpectedTeamCount()` = 20 (was 0),
  `compId` = 128. m68k build clean. Amiga files (competition_data_copa_baked.c new,
  frontend.c) await USER review/commit.

**REMAINING:** only the INTERACTIVE play path (human manages a CONMEBOL club → plays
Copa in real time; needs a CONMEBOL-club career to test). Tracked separately.

## 9. House rules (carry over)

Build all 3 Linux arches (`make X86_64=1` / `ARM64=1` / `ARM=1`, `make clean`
between — objects are shared). The Cygwin `bin/SWOS` is the user's Windows test
build (rebuilt by the user). Don't `git commit`/`push` the Amiga port. Stage source
only (skip `bin/` binaries). Standard debug: ask the user to run
`gdb ./bin/SWOS` + `run 2> SWOS.log` + `bt`.

---

# Appendix A — By-address inventory (session 2026-06-27)

Produced by three parallel sweeps (gates / tables / identity machinery) before any
edit. This is the spine of the project; line numbers are as of HEAD `5459229`.

## A.1 Identity machinery (what already exists)

- **`Competition` record** = 9-byte packed header (`game_types.h:1559`). Byte 0 is
  `compNumber` — the identity field. `type` (byte 1) discriminates
  league/cup/playoff. Wrapper structs (`CompetitionTournament`, `CompetitionCup`,
  `Playoffs`, `CompetitionLeague`) embed `Competition` as their head.
- **`compNumber` values are unique but SPARSE** (gaps 5–12, 17–31, 46–49, …). Plenty
  of free slots for new competitions.
- **Loader already keys by JSON string id** (`dj_find_comp_record`) and **verifies
  `compNumber` equality** before patching (`save_format_json.c:3516-3534`,
  tables `sg[]` ~3487 tournaments/playoffs, `sp[]` ~3558 cups/leagues). Patch-in-
  place: `*(CompetitionTournament *)sg[gi].g = tmp;`.
- **Runtime-build model already proven** for per-country tables:
  `dj_graph_resolve(reg, id) → Competition*` over a `{id, Competition*}` registry,
  `dj_graph_append_ids` walks a JSON id-array and installs
  `_competitionsTable[country] = tbl` (`save_format_json.c:3240-3463`). **This is
  the mechanism to generalise.**
- **`*Copy` rebuild** is driven by `_careerContests[]`: `InitCareer`
  (`career_init.c:691`) and `reconstruct_comp_copy_region_from_templates`
  (`save_format_json.c:4161`) both `memcpy(copy, template, templateSize)` walking the
  sentinel-terminated contest table. The `*Copy` therefore carries the **same
  compNumber** as its template.
- **No `comp_id()` / `comp_by_id()` accessor exists yet.** Only `comp_name_table()`
  / `comp_team_seed_list()` polymorphic field accessors and the JSON-only
  `dj_graph_resolve`.

## A.2 Gates — 50 sites, but they CLUSTER into 5 categories

Not 50 equal problems. By category:

**Cat 1 — Slot-occupancy (object identity). LIKELY NO CHANGE NEEDED.**
Compare a runtime comp ptr to a career SLOT ptr (`_currentCompetitionPtr`,
`_secondary/tertiary/quaternaryCompetitionPtr`). These ask "is this comp the one in
slot X", not "is this compiled global X". The slot ptr is re-derived by the JSON
loader, so a heap comp's ptr compares equal to itself — dynamic-safe.
Sites: `career_init.c:1767,1782,1788,1792,1796`; `career_season.c:735,757,1567,1605`;
`career_eurocups.c:476,494,524,670,723`; `diy_competition.c:3534`;
`career_status.c:1925`(null-check). *Verify each during conversion, don't blindly skip.*

**Cat 2 — Behaviour-dispatch by `*Copy` address. THE BULK. → `comp_id()==ID_x`.**
Per-competition behaviour keyed off which `*Copy` a ptr equals. The `*Copy` carries
the template's compNumber, so each collapses to an integer compNumber compare (and
this FIXES latent template-vs-copy mismatches).
- `career_season.c:303-327` `GetCompetitionAbbrevStr` (≈12 comps → abbrev string)
- `career_season.c:446-460` `adjustWeightsForCompetition` (weight factor by comp)
- `career_status.c:916-938` `IsMajorInternationalTournament`
- `career_status.c:1813-1823` WC-qualifier slot counts
- `career_status.c:1893-1941` trophy detection (primary+secondary)
- `career_status.c:2047-2052` reputation bonus buffer select
- `menus.c:4459` hide menu entry for `_tourCopy`/`_friendlyCopy`

**Cat 3 — RANGE checks across the contiguous `*Copy` block. HIGHEST RISK.**
`comp >= _wcQualifyingGroup1Copy && comp <= _oceaniaWorldCupQualificationCopy` etc.
These depend on the `*Copy` `.skip` region being laid out in a fixed order — exactly
what dynamic heap allocation breaks. MUST become by-id set-membership helpers
(`comp_is_wc_qualifier(id)`, `comp_is_ec_qualifier(id)`).
Sites: `career_season.c:307,312,466`; `career_status.c:1898,1902`.

**Cat 4 — By-TEMPLATE address gate. The original 0x4444 family.**
- `career_season.c:1468` `competition != &_englishLeague` (English playoff-link walk;
  also reads `_englishLeagues1/2` by name — adjacency reads of the same struct).
- `career_init.c:926` `comp != &_englishLeague`.
- `career_init.c:1774` `comp == _europeanChampionships`, `:1776` `comp == _worldCup`.
- `diy_competition.c:1843` `compId == _europeanChampionships[0]` — **already by-field
  (reads compNumber byte), the model for the conversion.**

**Cat 5 — DIY/special buffers (not a competition template).**
- `career_status.c:3498` `comp == &_diyCompetitionStart`
- `diy_competition.c:9843` `comp == &_diyFileBufferCopy`
Distinct buffer objects; treat as object identity (likely leave, or dedicated flag).

## A.3 Tables holding `&_comp` (→ runtime-built / appendable)

- **`_careerContests[35]`** (`competition_data.c:1557`) — 34 `{Competition *orig,
  Competition *copy, uint16 templateSize}` + sentinel `{0xFFFFFFFF,0,0}`. CENTRAL:
  drives every `*Copy` fill+save. Consumers: `career_init.c:692,1466,1765`,
  `save_format_json.c:4122,4164,4205,4260`. A dynamic comp MUST get appended here.
- **`_competitionsTable[256]`** (sparse master, `competition_data.c:1926`) — **already
  runtime-swappable** (loader installs heap tables). Country tables (64×
  `_albaniaTable[]`…), region tables (slots 80-85), qualifier groups (252-254).
- **Region/continental**: 8 `QualifyingGroup` structs (`_southAmericaTable` etc.,
  1645-1683 — `{Competition *comps[…,-1]; uint8 countryNums[…,255]}`),
  `_continentalCups[7]` (1625), `_nationalTeamCompetitionsAndQualifications[39]`
  (1599). Consumers mostly `teams.c`, `career_season.c`.
- **National-team career scaffolding** (lower priority for Copa, which is club):
  `_ecQualifyingGroupsTable[8]`/`_worldCupQualifyingGroupsTable[8]` (career_season.c
  consumers), 36 `_cupPick*[5]` lists (passed by base ptr to `setCupPickListCommon`).
- All externs are **ARRAY-typed** (consumers take the base) — converting a table to a
  heap pointer means its consumers must move base→pointer. `_competitionsTable` is the
  exception (already pointer-indexed/heap-installable).

## A.0 Decisions locked (2026-06-27)

- **Identity scheme: `compNumber` internal + JSON string-id external.** `compNumber`
  (byte 0 of every record, already the loader's verify key) is the runtime id; gates
  become `comp_id(c) == ID_x` integer compares. JSON string-id stays the persistent
  key. New competitions get a free `compNumber` slot.
- **Table strategy: runtime-FILL the compiled arrays.** Keep the array symbols (same
  base addresses), over-size them, and overwrite/append contents at startup. Consumers
  that take the array base are UNCHANGED (matches how `_competitionsTable` already
  works). Heap-pointer arrays are a possible later cleanup, not now.

## A.5 Gate conversion — DONE (Steps 1-4, 2026-06-27, builds x86_64+arm64+armhf)

`include/comp_identity.h` is the identity module: `COMPID_*` enum, `comp_id(void*)`
(byte 0), and set predicates `comp_is_wc_qualifier` (5-17), `comp_is_ec_qualifier`
(23-30), `comp_is_continental_cup` (21,31-35), `comp_is_tour_or_friendly` (19,20).
NULL slots map to a `0xFF` sentinel (matches no COMPID) so the original
"NULL/unknown falls through" semantics are preserved where `comp_id` would
otherwise deref.

Converted to by-id (every competition identity gate except the intentional ones):
- `career_season.c`: GetCompetitionAbbrevStr, adjustWeightsForCompetition,
  the WC/EC group8 exact checks, the englishLeague playoff-link gate.
- `career_status.c`: IsMajorInternationalTournament, WC-qual slot counts, EC-qual
  block, trophy detect (primary+secondary, incl. the verbatim primary-ptr-in-
  secondary quirk), tertiary league bonus, reputation bonus, English-cup aliasing
  (118/119→120/121).
- `menus.c`: ViewCompetitionsOnReturn (primary/secondary/tertiary) + ViewCompetitionsInit.
- `statistics.c`: EncodeCompetitionPositionForSeasonInfo_impl + the season-info qualifier check.
- `career_init.c`: DIY_SelectCompetitionSlot4 englishLeague gate, ViewOrEditCompetitionSlot
  EC/WC template→copy swap, RandomizeInvitationTournamentCup (sentinel-guarded for -1/-2).
- `diy_competition.c`: normalised the already-by-field `_worldCup[0]`/`_europeanChampionships[0]`
  reads to `COMPID_*` (removes the by-name template-array dependency).
- `teams.c`: copaAmerica preset check (`_selectedContest` is `uintptr_t`, full-width — not the uint32 trap).

LEFT AS-IS (correct): Cat-1 slot-occupancy compares (`comp == _currentCompetitionPtr`
etc. — object identity, slot ptr re-derived by loader); Cat-5 DIY buffers
(`&_diyCompetitionStart`, `&_diyFileBufferCopy`); `_careerContests[]` template-lookup
in ViewOrEditCompetitionSlot (object identity).

RUNTIME-VALIDATED via the gdb harness (2026-06-27, x86_64, VERIFY=0 +
NO_INJURIES_CARDS=1 build):
- MILAN.CAR driven season 3→4: clean transition, `panic=0`, `gameType=4`,
  reputation 49 (Milan won nothing → no bonus, correct), `wonWC/wonCont=0`. This
  runs `SeasonFinished_ComputeSeasonReview` (the heavy trophy/reputation/weights/
  IsMajor converted code). No wrong-comp dispatch in the HLOG; cup type-byte branch
  correct. (Season 4 then parked on the documented in-season cup-stepper "KNOWN
  LIMITATION" — a harness best-effort stall, not a crash.)
- Direct unit-test of `GetCompetitionAbbrevStr` via gdb inferior calls on all 15
  international `*Copy` records (a club career can't navigate to them): **15/15
  correct** compNumber + abbreviation, covering comp_id, both qualifier RANGE
  checks at their endpoints (WCQ 5/12/17, ECQ 23/30), all 6 continental cups, and
  the exact matches (FR/WC/EC/TNM/TR). Proves the by-id logic == the old by-address
  logic across the full international set.

Harness gotcha hit: `scripts/harness_lib.py:30 DEFAULT_PROXY` is a STALE path
(`SWOS_C/gdb_proxy.py`); the file is now at `SWOS_C/scripts/amiga/gdb_proxy.py`.
Pass `SwosHarness(proxy=".../scripts/amiga/gdb_proxy.py")` until that default is
fixed, else every client cmd errors and all reads return None.

Next: Step 5 (runtime-fill tables).

## A.6 Steps 5-6 — dynamic competition engine (DONE, runtime-validated 2026-06-27)

A brand-new competition is now addable as **pure data** (a tournaments.json record
with a compNumber no compiled global uses):

- **`_careerContests` appendable** (step 5): `[35] → [CAREER_CONTESTS_CAP=48]` +
  `Comp_AppendCareerContest(template, copy, size)` (writes over the sentinel,
  re-plants it). Sentinel-terminated walks pick up appends. Inert for the 34
  compiled entries.
- **`comp_by_id` registry** (step 6): `Comp_Register(c)` / `Comp_ById(n)` —
  `s_compById[256]` indexed by compNumber. Dynamic comps register their heap record
  (compiled comps re-derive slot ptrs via competitionsTable, so they don't need it).
- **Dynamic loader** (step 6, in `DataJson_InstallCompetitionGraph` after the `sg[]`
  loop): iterate tournaments.json; for any record whose compNumber matches no
  compiled tournament global, `calloc` a `CompetitionTournament` + a `*Copy`,
  `DataJson_LoadCompetitionTournament` it, `Comp_AppendCareerContest`, `Comp_Register`.
  **Detection is by compNumber, NOT id string** (see finding below).

Validated via gdb: a temporary `_testDynamicCup` (compNumber 60) injected into
tournaments.json → install log "1 dynamic competition(s)", `Comp_ById(60)` returns
the heap record (compNumber 60, type 2), `_careerContests[34]` == that record with
the sentinel moved to [35], `Comp_ById(99)`==NULL. Test record then reverted.

### sg[]/sp[]/R[]/QT[] id-prefix mismatch — RESOLVED (2026-06-27)

The data_json competition ids are **`_`-prefixed** (C-symbol style: `_euroCup`),
but the loader's FOUR hardcoded id lists — `sg[]` (tournaments+playoffs), `sp[]`
(special cups/leagues), `R[]` (region comp refs), `QT[]` (region table ids) —
used **non-prefixed** ids, so those patch-loops found **0 records** (install log
`0 tournaments / 0 playoffs / 0 special / 0 region`). Harmless (compiled fallback
identical) but meant the JSON wasn't actually driving the standalone/region comps.

Per the chosen convention ("everything that references a variable is `_`-prefixed,
as in the code"), the fix is to **prefix the loader ids** (88 strings across the
four lists). Result: `13 tournaments, 7 playoffs, 26 special records, 10 region
tables (0 kept-compiled)` now load. **Proven a faithful no-op:** the region
loop's `memcmp` guard accepted all 10 tables byte-identically (`regionMiss=0`, no
`JSON != compiled`), compNumber spot-checks all matched, and a MILAN season is
behaviourally identical (`rep=49, panic=0`, same 3→4 transition). Builds green on
all 3 Linux arches.

## A.4 Scope read

Copa Libertadores is a **continental CLUB** cup → models on `_euroCup` (standalone
`sg[]`/`sp[]` loader + career_eurocups qualifier routing), NOT the national-team
cup-pick tables. So the table work Copa needs is: (a) make the standalone loader
**allocate** not just patch, (b) **append to `_careerContests`** so its `*Copy` is
built+saved, (c) a by-id **qualifier routing table** (10 CONMEBOL leagues → Copa),
modelled on `career_eurocups.c`. The 36 cup-pick / 2 qualifier-copy tables are NOT on
Copa's path and can be deferred.
