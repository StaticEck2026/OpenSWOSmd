# STRUCT_CONSOLIDATE.md — Merging the competition struct variant families

**Status: DONE.** All families collapsed to single merged structs in
`include/game_types.h` (`CompetitionCup`, `CompetitionCup2`, `CompetitionLeague`,
`Playoffs`, `Leagues`, `QualifyingGroup` + the `CompCupHeader`/`CompCupHeader2`/
`CompLeagueHeader` headers); the old `*_4`/`*_8`/`*_2` variant names survive only
in comments. `unkX` field naming finished (see `STRUCT_FIELDS_UNK.md`). Builds
x86_64/armhf/aarch64, cross-target save-validated. Plan kept below for history.

> Update (2026-09-30): the type-2 names changed afterwards — `CompetitionCup2` is
> now **`CompetitionTournament`** and `CompCupHeader2` is **`CompTournamentHeader`**
> (`aed8453`, 2026-06-18). The §9 `AnimTable` merge was **not** done:
> `AnimTable_20`/`_12`/`_8` and the dead `plTacklingAnimTable_2` are still in
> `include/game_types.h` / `src/sprite_data.c`.

**Scope:** the per-instance variant struct families in `include/game_types.h`:
`CompetitionCup_4` / `_6` / `_10` / `_6_2` / `_4_2` / `_6_2_8` / `_6_2_12` /
`_6_2_64`; the `CompetitionCup2_*` family; `CompetitionLeague_1`..`_4` and the
`CompetitionLeague_*_*` variants; `Playoffs_8`/`_4`; `Leagues_4`/`_2`/`_4_2_24`;
`QualifyingGroup_*`.

**Why they exist:** these structs were authored in IDA Pro, one per *instance
size*, because IDA cannot resize a struct without disturbing the data laid out
after it. So a 4-round national cup got `CompetitionCup_4`, an English cup with
a name got `CompetitionCup_6_2`, the UEFA Cup with 32 seeded teams got
`CompetitionCup_6_2_64`, and so on. They are all the *same conceptual record*
with a variable-length tail.

**Goal (roadmap step 3):** collapse each family into **one struct** with the
arrays sized to the maximum, leaving the unused tail bytes empty. Space is no
longer a concern. This document answers the questions you posed:

1. Do these structs use a **sentinel**, a **count**, or **fixed-size stepping**
   to find the end of their variable sequences? *(Answer: count / self-describing
   offset — never fixed-size stepping over the records. Consolidation is safe.)*
2. Should we add explicit **count** fields instead of sentinels? *(Recommendation
   below — mostly the counts already exist.)*
3. Is any data **overlapping** and in need of untangling? *(The static tables do
   not overlap each other; the runtime overlays are separate and already handled.)*
4. What **C code must change** to merge safely?

**Sources:** same as `STRUCT_FIELDS_UNK.md` (SWOS2.s 68k asm + EQU blocks, the
IDA `Data.c`, and the SDL port). Field semantics and the two *self-describing
offset bytes* are documented there — read §0 of that file first.

---

## 1. The variant families, enumerated

### 1a. `CompetitionCup` family — type-1 knockout cups (header = `CompetitionCup`, 14 B)

Layout: `CompetitionCup` header → `roundRuleFlags[R]` → `text[2]?` → `teamSeedList[T]?`

| Variant | rounds[R] | text[2] | teamSeedList | 32-bit size | Example instances |
|---|---|---|---|---|---|
| `CompetitionCup_4` | 4 | – | – | 18 | albanianCup … (37 national cups) |
| `CompetitionCup_6` | 6 | – | – | 20 | dutchCup, australianCup |
| `CompetitionCup_10` | 10 | – | – | 24 | *(declared; no live instance found)* |
| `CompetitionCup_4_2` | 4 | ✔ | – | 26 | japaneseCup, japaneseEmperorsCup |
| `CompetitionCup_6_2` | 6 | ✔ | – | 28 | englishCup, frenchCup, italianCup, … |
| `CompetitionCup_6_2_8` | 6 | ✔ | 8 (4 teams) | 36 | invitationTournament, tour, oceaniaWCQual |
| `CompetitionCup_6_2_12` | 6 | ✔ | 12 (6 teams) | 40 | WC/EC qualifying groups, friendly |
| `CompetitionCup_6_2_64` | 6 | ✔ | 64 (32 teams) | 92 | cupWinnersCup, uefaCup |

### 1b. `CompetitionCup2` family — type-2 group tournaments (header = `CompetitionCup2`, 34 B incl `Round[6]`)

Layout: `CompetitionCup2` header → `roundRuleFlags[R]` → `text[2]?` → `teamSeedList[T]?`

| Variant | rounds[R] | text[2] | teamSeedList | 32-bit size | Example instances |
|---|---|---|---|---|---|
| `CompetitionCup2_4` | 4 | – | – | 38 | *(base for _4_2*)* |
| `CompetitionCup2_6` | 6 | – | – | 40 | *(base for _6_2*)* |
| `CompetitionCup2_4_2` | 4 | ✔ | – | 46 | northAmericanCup |
| `CompetitionCup2_6_2` | 6 | ✔ | – | 48 | africanNationsCup, asianCup |
| `CompetitionCup2_4_2_24` | 4 | ✔ | 24 | 70 | copaAmerica |
| `CompetitionCup2_4_2_32` | 4 | ✔ | 32 | 78 | euroCup, europeanChampionships(2) |
| `CompetitionCup2_6_2_16` | 6 | ✔ | 16 | 64 | southAmericaWCQual |
| `CompetitionCup2_6_2_24` | 6 | ✔ | 24 | 72 | northAmericaWCQual |
| `CompetitionCup2_6_2_48` | 6 | ✔ | 48 | 96 | worldCup |
| `CompetitionCup2_6_2_60` | 6 | ✔ | 60 | 108 | asiaWCQual |
| `CompetitionCup2_6_2_64` | 6 | ✔ | 64 | 112 | worldCup2, africaWCQual |

### 1c. `CompetitionLeague` family — type-0 leagues (header = `CompetitionLeague`, 13 B)

Layout: `CompetitionLeague` header → `Division divisions[D]` → `unk1`(db) → `text[N]?` → trailing link arrays?

| Variant | div[D] | text[N] | trailing | Example instances |
|---|---|---|---|---|
| `CompetitionLeague_1` | 1 | – | – | ~50 single-division national leagues |
| `CompetitionLeague_2` | 2 | – | – | argentinianLeague, mexicanLeague |
| `CompetitionLeague_3` | 3 | – | – | *(base for _3_6)* |
| `CompetitionLeague_4` | 4 | – | – | elSalvadorLeague |
| `CompetitionLeague_3_6` | 3 | 6 | – | customsLeague, newZealandLeague |
| `CompetitionLeague_2_4` | 2 | 4 | – | french/german/italian/japanese |
| `CompetitionLeague_4_8` | 4 | 8 | – | australianLeague |
| `CompetitionLeague_2_2_24` | 2 | 2 | unk2[24] | dutchLeague |
| `CompetitionLeague_4_2_8` | 4 | 2 | unk2[8] | scottishLeague |
| `CompetitionLeague_4_10_24` | 4 | 10 | unk2[24] | *(declared)* |
| `CompetitionLeague_2_2_8` | 2 | – | unk2[2]ptr + unk3[8] | uruguayanLeague |
| `CompetitionLeague_2_4_2_24` | 2 | 4 | unk2[2]ptr + unk3[24] | spanishLeague |
| `CompetitionLeague_4_8_2_24` | 4 | 8 | unk2[2]ptr + unk3[24] | englishLeaguesBlock._main |

### 1d. `Playoffs` / `Leagues` / `QualifyingGroup`

| Family | Variants | Differ by |
|---|---|---|
| `Playoffs_8` / `Playoffs_4` | 2 | trailing `unk2[8]` vs `[4]` (qualified-team id buffer) |
| `Leagues_4` / `Leagues_2` | 2 | 4 vs 2 `{CompetitionLeague*, linkParams[8]}` records |
| `Leagues_4_2_24` | 1 | `Leagues_4` + playoffs ptr + next-group ptr + `unk2[24]` |
| `QualifyingGroup_3/_9_2/_2_8/_6_44/_3_4/_3_12` | 6 | array lengths only: `compCup[N]` (`-1`-terminated, N=2..9) + `countries[M]` (`0xFF`-terminated, M=0..44) |

---

## 2. THE iteration question — sentinel vs count vs fixed-stride

This is the crux of "can we consolidate safely". The answer, verified against
both the asm and the C port, is split by *what* is being iterated:

### 2a. The variable tails *inside* a record — **COUNT / self-describing offset. Never fixed-stride.**

| Tail | How its location & length are found | Verdict |
|---|---|---|
| `text[]` name table | located at `base + 5 + base[5]` (the `nameTableOffset` byte); per-division stride `[divIdx*2]` bounded by `divisionsCount` | self-describing offset |
| `teamSeedList[]` | located at `base + 7 + base[7]` (the `teamListOffset` byte); length = `2 × count`, count = `teams` / `firstRoundNo` / `rounds[0]` per type | self-describing offset + count |
| `divisions[]` | count = `divisionsCount` (offset 9), stride `divIdx*6` | count-driven |
| `roundRuleFlags[]` | count = `log2(teams/2)` (cups) / per-stage (Cup2) | count-driven |
| `Round[6]` (Cup2) | fixed `[6]`, indexed by stage | fixed array |

Verified: `teams.c:3089` (`compEntry + 5 + off16`), `diy_competition.c:9070`
(`a5 + 7 + seedOff`), `diy_competition.c:9001` (`for i ≤ log2(teams/2)`),
`teams.c:3105` (`divIdx != divisionsCount`). Amiga asm mirrors all of these
(SWOS2.s:52217/126050/52335/52179).

**Critical consequence:** *no instruction anywhere assumes the team list / text
table sits immediately after `rounds[specificN]`.* A `grep` of SWOS2.s for any
instruction referencing the variant-specific EQU offsets (`*_TEXT`, `*_UNK1`,
`*_ROUNDS`, `*_DIVISIONS`, `LEAGUES_*`, `PLAYOFFS_*`) returns **zero hits outside
the EQU definitions themselves** — those symbols are pure layout documentation.
Therefore **padding `rounds[]`/`divisions[]` to the maximum and relocating
`text[]`/`teamSeedList[]` is safe**, provided the two self-describing offset
bytes (and `careerContests[].length`) are recomputed for the new layout.

### 2b. Iterating *across* records — **SENTINEL, on pointer arrays. Never fixed-stride over the records.**

The competition records are never stored as a contiguous fixed-stride array.
They are referenced through **arrays of pointers**, terminated by a sentinel:

| Array | Terminator | Evidence |
|---|---|---|
| `nationalTeamCompetitionsAndQualifications[39]` | `0xFFFFFFFF` | SWOS2.s:118034, walker `IsNationalTeamCompetitionOrQualification` 52120 |
| `continentalCups[7]` | `0xFFFFFFFF` | SWOS2.s:118044 |
| `careerContests[35]` | `0xFFFFFFFF` triple | SWOS2.s:61015 (`cmpa.l #$FFFFFFFF,a0; beq`) |
| country tables (`englandTable[5]`…) + `QualifyingGroup.compCup[]` | **`-1` (0xFFFFFFFF)-terminated**; `-2`/`0xFC` are *in-band* markers (playoff separator / custom-teams), still inside the walk | `competition_data.c:907–996` (e.g. `republicIrelandTable[4] = {&irishLeague,-2,&irishCup,-1}`) |
| `QualifyingGroup.countries[]` | **`0xFF`-terminated** | `worldTable.countries = {…regions…, 0xFF, …}` (`competition_data.c:909`) |

`careerContests` is the one place a *length* is used: each `{template, copy,
length}` triple copies `length` bytes template→`*Copy` (`career_init.c:703`,
SWOS2.s:61021). **That `length` is a copy size, not a stride** — it is precisely
`sizeof(variant)`, so it self-adjusts when the variant type grows, *as long as
the template and `*Copy` storage grow together* (see §5).

> **Your specific worry — "as long as the translated assembly doesn't increment
> in fixed sizes" — is satisfied.** There is no fixed-size record stepping over
> these tables on either the asm or the C side. The only fixed sizes in play are
> (a) pointer strides in the pointer arrays (unaffected by record size) and
> (b) the `.skip`/`length` byte counts that are *defined as* `sizeof(variant)`
> and must be regenerated, not hand-stepped.

### 2c. Verdict

**Every variant family — cup, cup2, league, `Playoffs`, `Leagues`,
`QualifyingGroup` — can be consolidated into a single max-sized struct.** No
sentinel terminates a `Competition`-header record's tail (those tails are
count/offset-driven); the pointer-holding families (`QualifyingGroup`, country
tables) ARE sentinel-terminated (`-1`/`0xFF`), which padding preserves; and no
code steps record-to-record by a fixed size. The consolidation cost is entirely
in regenerating the *stored layout metadata* — the two offset bytes, the
`careerContests` lengths, and the BSS `.skip` reservations — not in rewriting
iteration logic.

---

## 3. Overlap check — do any records overlap?

- **The static competition tables do NOT overlap each other.** Each label is a
  self-contained record; its trailing arrays belong to it and are sized by the
  variant. `careerContests[].length` copies exactly `sizeof(variant)` — no read
  deliberately runs past a record into the next. (SWOS2.s data tables
  117994–121230; agent-verified.)
- **The only "label-into-next-label" pattern is the pointer arrays**, and those
  rely on a `0xFFFFFFFF` sentinel, not byte adjacency — sentinel-safe.
- **The real overlap is in the *runtime* DIY structures**, not these static
  tables. The static record is *unpacked* into `diyCompetitionStart` /
  `diyFileBufferCopy`, where many fields are type-overlaid (e.g. runtime `$5E` =
  awayGoalsMode for cups but playEachTeam for leagues). That is already tracked
  in `docs/OVERLAPPED_DATA.md` and the `DIYCompetitionEx` union work — **out of
  scope here**. For the *static* tables there is nothing to untangle: each can
  become a standalone fixed-layout struct.

**Conclusion:** no untangling of overlapped static data is required. This makes
the consolidation purely additive (pad + relocate), not a separation problem.

---

## 4. Proposed consolidated structs

Group within type families (the mixed pointer arrays cast everything to a common
base and dispatch on `Competition.type`, so the headers must stay compatible).

**Naming scheme.** The *clean* names `CompetitionCup` / `CompetitionCup2` /
`CompetitionLeague` are reused for the **merged record** (the thing the data
instances and pointer arrays actually are). The existing fixed-size *header*
structs that currently own those names are renamed with a `…Header` suffix:

| Current name (header struct) | Renamed to | Becomes the embedded `header` field of |
|---|---|---|
| `CompetitionCup` (14 B) | **`CompCupHeader`** | the new `CompetitionCup` record |
| `CompetitionCup2` (34 B, incl `Round[6]`) | **`CompCupHeader2`** | the new `CompetitionCup2` record |
| `CompetitionLeague` (13 B) | **`CompLeagueHeader`** | the new `CompetitionLeague` record |

`Competition` (the shared 9-byte base) keeps its name — it stays the polymorphic
cast target for the type-erased pointer arrays, and each `…Header` still embeds it
as its first member. No `…Rec` suffix anywhere.

```c
/* One struct per type family. Arrays sized to the observed maximum; unused
 * tail bytes left zero. Field names per STRUCT_FIELDS_UNK.md. */

#define COMP_MAX_ROUNDS      10   /* CompetitionCup_10 */
#define COMP_MAX_NAMES       10   /* CompetitionLeague_4_10_24 text[10] is the max; cups use [2] */
#define COMP_MAX_SEED_TEAMS  32   /* uefaCup/cupWinnersCup/worldCup2: 64 bytes */

/* type == 1 : knockout cups (merges CompetitionCup_4/_6/_10/_4_2/_6_2/_6_2_8/_12/_64) */
typedef struct __attribute__((packed)) {
    CompCupHeader header;                        /* 14 B (was CompetitionCup): Competition + cup fields */
    uint8_t  roundRuleFlags[COMP_MAX_ROUNDS];    /* per-round legs/ET/pen bits */
    char    *name[2];                            /* was text[2] */
    uint16_t teamSeedList[COMP_MAX_SEED_TEAMS];  /* BE {country,ordinal} pairs */
} CompetitionCup;

/* type == 2 : group tournaments (merges all CompetitionCup2_*) */
typedef struct __attribute__((packed)) {
    CompCupHeader2 header;                        /* 34 B (was CompetitionCup2), incl Round[6] */
    uint8_t  roundRuleFlags[COMP_MAX_ROUNDS];
    char    *name[2];
    uint16_t teamSeedList[COMP_MAX_SEED_TEAMS];
} CompetitionCup2;

/* type == 0 : leagues (merges ALL 13 CompetitionLeague_* variants:
 * _1.._4, _3_6, _2_4, _4_8, _4_10_24, _2_2_24, _4_2_8, _2_2_8,
 * _2_4_2_24, _4_8_2_24). The trailing region differs per variant only in
 * array length (text[2..10], unk2/unk3 link bytes, the char* unk2[2]
 * playoff pointers); all become fixed max-sized fields here. */
typedef struct __attribute__((packed)) {
    CompLeagueHeader header;                      /* 13 B (was CompetitionLeague) */
    Division divisions[4];                        /* divisionsCount says how many used */
    uint8_t  reserved1;                           /* the old per-variant `unk1` byte */
    char    *name[COMP_MAX_NAMES];                /* full+abbrev per division (max 10, _4_10_24) */
    char    *playoffLinkPtrs[2];                  /* was the char* unk2[2] in _*_2_* (→ Playoffs/Leagues) */
    uint8_t  promotionLinkData[24];               /* was the unk2[24]/unk3[24]/unk2[8] qualifier-index bytes */
} CompetitionLeague;
```

The remaining three families (`Playoffs`, `Leagues`, `QualifyingGroup`) are
covered in full below — they are NOT part of the `Competition`-header type
families above (they hold *pointers to* competitions, not competition headers),
so each gets its own merged struct.

> **Design note — keep the polymorphic accessor working.** Because the mixed
> pointer arrays (`continentalCups`, `nationalTeamCompetitionsAndQualifications`,
> the country tables) store records *type-erased* as `Competition*`/`CompetitionCup*`,
> a type-known `offsetof(CompetitionCup, name)` is NOT usable at the
> polymorphic read sites (they don't know whether the pointer is a cup, a cup2 or
> a league — those have `name[]` at different offsets). **The self-describing
> `nameTableOffset`/`teamListOffset` bytes are exactly what makes the polymorphic
> access work, so KEEP them** (renamed) and recompute their stored values for the
> new fixed layout. Use the typed `name[]`/`teamSeedList[]` fields only at sites
> where the concrete type is known.

### 4b. `Playoffs_8` / `Playoffs_4` → one `Playoffs`

Both variants are identical except the trailing qualified-team buffer
(`unk2[8]` vs `[4]`). Merge to the larger:

```c
typedef struct __attribute__((packed)) {
    uint8_t playoffHeader[16];   /* structured: [7]=offset-to-ids (=17), [10]=qual-team count */
    char   *name[2];             /* was text[2] */
    uint8_t qualifiedTeamIds[8]; /* was unk2[N]: BE-packed qualified team ids (max 8) */
} Playoffs;
```

- **Iteration / end-of-sequence:** the qualified-team list length is the
  **explicit count `playoffHeader[10]`** (`div1Playoffs.playoffHeader[10]=4`),
  and the buffer is located via the **self-offset byte `playoffHeader[7]`=17**
  (`teamData = base + 7 + base[7]`) — exactly the same self-describing-offset
  mechanism as the cup team list. No sentinel, no fixed stride.
- **Consolidation impact:** padding `qualifiedTeamIds` 4→8 is safe (count-driven);
  but because it's reached via `playoffHeader[7]`, that stored `17` must be
  recomputed if the `name[2]` pointers ever move (they don't here, since
  `playoffHeader` and `name` keep their offsets). Access sites:
  `career_season.c:1498` (`[10]` count), `:1522` (`[7]` offset), `:1533`
  (`&qualifiedTeamIds[0]`). Already typed — see `STRUCT_FIELDS_UNK.md` §8a.

### 4c. `Leagues_4` / `Leagues_2` / `Leagues_4_2_24` → one `Leagues`

These are inline arrays of `{CompetitionLeague *, uint8 linkParams[8]}` records
(promotion/playoff link tables): `Leagues_4` = 4 records, `Leagues_2` = 2,
`Leagues_4_2_24` = a `Leagues_4` + a `Playoffs_8 *` + a `Leagues_4 *` + a
24-byte qualifier-index list.

```c
typedef struct __attribute__((packed)) {
    CompetitionLeague *league;
    uint8_t            linkParams[8];   /* promotion/relegation link config */
} LeagueRec;

typedef struct Leagues {
    uint8_t       numLeagues;           /* NEW explicit count (was implied by the variant: 4 or 2) */
    LeagueRec records[4];           /* max = Leagues_4 */
    Playoffs     *playoffs;             /* Leagues_4_2_24 only (NULL otherwise) */
    struct Leagues *nextGroup;          /* Leagues_4_2_24 'leagues2' (NULL otherwise) */
    uint8_t       qualifierIndexPairs[24]; /* Leagues_4_2_24 unk2[24] (zeroed otherwise) */
} Leagues;
```

- **Iteration / end-of-sequence — NEEDS VERIFICATION (medium confidence).** The
  record data shows **no inter-record sentinel** (the `CompetitionLeague *` is a
  real pointer in every slot, e.g. `dutchLeagues1` = 4×`&dutchLeague`,
  `competition_data.c:606`). So the record count is either **fixed per variant**
  (4 vs 2, baked into the consumer) or driven by an **external count** (e.g. the
  parent league's `divisionsCount`). The SDL port reaches these only through the
  `englishLeaguesBlock._div1/_div2/_div3` typed fields (`career_season.c:1488`),
  not by a discoverable walk loop — confirm the walk count against `SWOS2.s`
  before merging `Leagues_2` into a 4-record struct.
- **Consolidation recommendation:** add the **explicit `numLeagues` count** shown
  above so the merged struct is unambiguous regardless of how the original walked
  it. This is the one family where I'd add a count rather than rely on the
  existing mechanism (§6).

### 4d. `QualifyingGroup_*` (6 variants) → one `QualifyingGroup`

The six variants differ only in the two array lengths: `compCup[N]` (the region's
competitions) and `countries[M]` (the region's countries). Merge to the maxima
(`compCup[9]` from `_9_2`, `countries[44]` from `_6_44`):

```c
typedef struct __attribute__((packed)) {
    CompetitionCup *compCup[9];        /* -1 (0xFFFFFFFF)-terminated; -2 / 0xFC = in-band markers */
    CountryNumbers  countries[44];     /* 0xFF-terminated */
} QualifyingGroup;
```

- **Iteration / end-of-sequence — SENTINEL, both arrays (high confidence, from the
  data).** `compCup[]` is walked until a **`-1` (0xFFFFFFFF)** entry, with
  **`-2` and `0xFC`** appearing as *in-band* special markers (playoff separator /
  custom-teams), e.g. `customsTable.compCup = {&customsLeague, -2, -1}`,
  `europeTable.compCup = {0xFC, &europeanChampionships, &euroCup, &cupWinnersCup,
  &uefaCup, -1}` (`competition_data.c:907/914`). `countries[]` is walked until
  **`0xFF`**, e.g. `worldTable.countries = {CN_EUROPE,…,CN_OCEANIA, 0xFF, …}`.
  This is the **same `-1`-terminated pattern as the country tables**
  (`republicIrelandTable[4] = {&irishLeague, -2, &irishCup, -1}`) and the
  `continentalCups`/`nationalTeamCompetitionsAndQualifications` pointer arrays.
- **Consolidation impact — this risk MATERIALIZED and is now fixed.** Because
  `countries[]` follows `compCup[]` directly in each variant, padding `compCup`
  from e.g. `[2]` (`worldTable`) to `[9]` **moves `countries[]` later**. This is
  safe **iff** the consumer reaches `countries[]` by the struct field rather than
  "the bytes immediately after the `compCup` `-1` terminator". The one consumer
  that does the latter — the region/world country-selection list in
  `teams.c` (`SELECT TEAM TO MANAGE`) — was NOT found by a `.countries` grep
  because it reads via a raw `a5` cursor left pointing just past the `-1`. After
  the merge it landed on `compCup` padding (zeros → country 0 = ALBANIA), so every
  country rendered "ALBANIA" while the real `countries[]` (the sub-regions) showed
  correctly further down. **Fixed** (`teams.c` loc_13E2F4): repoint `a5` to
  `((QualifyingGroup *)compListAddr)->countries` for the `d7==254 || 80..85` case,
  which corrects both the name display and the `a5[index]` selection lookups.
  *Lesson: a save-diff doesn't exercise selection screens — grep for raw-cursor
  reads (`a5`/pointer-after-terminator), not just `.countries`, and runtime-check
  the actual menu.*
- **No `text[]`/`teamListOffset` indirection here** — `QualifyingGroup` has no
  `Competition` header, so the two self-describing offset bytes don't apply.

---

## 5. What C code must change (do these together — partial changes crash)

### 5a. Re-author the two self-describing offset bytes — **OR** switch known-type sites to `offsetof`

Every tail access computes `base + 5 + base[5]` (text) or `base + 7 + base[7]`
(team list). Padding the arrays moves those tails, so **the stored byte values
become wrong**. Two options:

- **(Recommended) Keep the offset bytes, regenerate their values** in every
  static record in `competition_data.c` to `(new_text_off − 5)` and
  `(new_teamList_off − 7)`. Access code is unchanged. This preserves the
  polymorphic path (see §4 note).
- (Alternative) Convert the ~20 access sites to `offsetof(MergedType, …)`. Only
  works at type-known sites; the polymorphic sites still need the offset byte.

The access sites that depend on these bytes (must all agree on the new layout):

| File:line | Tail reached |
|---|---|
| `teams.c:3089/3109/3123` | `name[]` (league/cup/tournament display) |
| `diy_competition.c:8918/8974/9137` | `name[]` (DIY init) |
| `diy_competition.c:9070` | `teamSeedList[]` (DIY seed reorder — most critical) |
| `career_season.c:199/221/243/980/1055/1206/1254/1284/1315` | `teamSeedList[]` (euro/WC/EC/friendly/invite/tour) |
| `career_season.c:1310` | `name[0]` **write** (tour region name) |
| `career_eurocups.c:667/742` | `teamSeedList[]` **writes** (EC/WC qualifiers) |
| `career_teams.c:1151/1189/1213` | `teamSeedList[]` |
| `career_init.c:1394` | rebuild `*Copy` `name[]` |
| `save_format_json.c:2617` | restore `*Copy` `name[]` after rebuild |

⚠️ The comments at `career_eurocups.c:665` and `career_season.c:194` warn that
getting this delta wrong makes writes "land in `text[1]` and stomp the comp name
pointer (SIGSEGV)" — i.e. a half-done merge reproduces a known crash class. Also
preserve the existing **64-bit `text[2]` pointer-growth delta**
(`+ 2u*(sizeof(char*) − sizeof(uint32_t))`) at the writer sites.

### 5b. Regenerate the BSS `.skip` reservations — **WILL break otherwise**

- `src/career_comp_copies.c:51–253` — 34 per-record `.skip N` where `N =
  sizeof(variant)` (e.g. `euroCupCopy: .skip 78`, `cupWinnersCupCopy: .skip 92`),
  each with a `#if 64-bit .skip 8` for the `text[2]` growth. **Every `.skip` must
  equal the merged `sizeof`.** Merge changes all sizes → records overlap unless
  regenerated in lockstep. *(This file was the load-bearing surprise — it is not
  obvious from `game_types.h`.)*
- `src/swos_globals_career_overlay.c:316–327` — aggregate `.skip 1832` (sum of
  the 34 variant sizes, 32-bit) + `#if 64-bit .skip 272` (34×8). Must be the new
  sum, or the entire careerFileBuffer overlay offset model shifts.

### 5c. `careerContests[].length` self-adjusts — but storage must grow with it

`competition_data.c:805–846` sets each `length = sizeof(template)`. The
`memcpy(dst, src, length)` (`career_init.c:703`, `save_format_json.c:2659`)
self-corrects **only if** the template `*` and the `*Copy` BSS both grow to the
merged type. So §5b and the template/Copy type changes must land in the same
commit. (A past truncation bug from violating this is documented at
`competition_data.c:806–811`.)

### 5d. Re-verify the pointer-range identity checks

`career_status.c:1859–1916` and `statistics.c:1533–1539/1760–1763` test
`currentCompetitionPtr` against the address span `[&ecQualifyingGroup1Copy …
&group8Copy]` (`>=`/`<=`). These assume the `*Copy` symbols stay **adjacent and
ordered** — i.e. they depend on the `.skip` spacing in §5b. Re-verify after
regenerating sizes (ideally replace the address-span test with an explicit id
check, per the project's "no address-range gating" rule in CLAUDE.md).

### 5e. Save/load format — mostly already decoupled (good)

The 34 `*Copy` records are **reconstructed from templates on load**, not loaded
as a layout-baked blob (`save_format_json.c:2652` `reconstruct_comp_copy_region_
from_templates()` does `memcpy(dst, src, e->length)` per `careerContests` entry).
The only fixed offset the save format bakes in is **byte `0xD`** (the dynamic
team-count for two WC/Oceania records, `save_format_json.c:2664`) — which stays
in the header ahead of the padded arrays, so it survives the merge. **No
`FLD_BYTES` blob hard-codes a variant `sizeof` or a post-header offset.** Net
save-format impact: keep the header bytes (0..0xD) ahead of the arrays, and
ensure §5c holds.

### 5f. Nothing else breaks

No loop count is tied to a variant suffix (§2a); the suffix dimensions live only
in the `game_types.h` typedefs. Pointer casts between variants are layout-neutral
(the risk is only in the offset reads done *through* them, covered by §5a).

---

## 6. Count fields vs sentinels — recommendation

You asked whether to add explicit `count` entries instead of sentinels. Findings:

- **The records already use counts, not sentinels, for their tails** —
  `divisionsCount`, `teams`, the self-describing offset bytes. There is **no
  sentinel to remove inside a record**. So for the variant merge itself, no new
  count field is needed; the existing counts already bound the (now max-sized)
  arrays.
- **Where derivation is implicit, add an explicit count** for clarity/safety:
  - `roundRuleFlags[]` length is currently *derived* (`log2(teams/2)`). Add an
    explicit `uint8_t numRounds` to the merged cup struct so consumers stop
    recomputing it (and so a malformed `teams` can't drive an out-of-bounds walk).
  - `teamSeedList[]` length is `2 × (type-keyed field)`. Add an explicit
    `uint8_t numSeedTeams` rather than re-deriving which header field is the count
    per type — this also removes the `firstRoundNo`-is-also-a-count ambiguity
    (`STRUCT_FIELDS_UNK.md` §4).
- **The cross-record sentinels** (`0xFFFFFFFF` on `careerContests`,
  `continentalCups`, `nationalTeamCompetitionsAndQualifications`, the country
  tables, and **`QualifyingGroup.compCup[]`**; **`0xFF` on
  `QualifyingGroup.countries[]`**) **can optionally be replaced by explicit
  element counts.** Low priority and orthogonal to the struct merge — the
  sentinels are not fragile (they don't depend on record layout), and the
  `-2`/`0xFC` in-band markers must be preserved either way. Do this only as a
  separate readability pass if desired.
- **`Leagues` is the exception — DO add a count.** Its `{league*, linkParams[8]}`
  records have *no* sentinel and the per-variant count (4 vs 2) is implicit, so
  the merged `Leagues` should carry an explicit `numLeagues` (§4c) rather than
  rely on a walk whose terminator condition isn't in the data.

**Net recommendation:** keep the existing count/offset mechanism; add small
explicit counts where derivation is implicit — `numRounds`/`numSeedTeams` on the
merged cup structs and `numLeagues` on `Leagues`; leave the pointer-array /
`countries[]` sentinels as-is for now.

---

## 7. Suggested migration order (incremental, low-risk)

Follow the project's `.equ`-untangle cadence: one family per commit, `VERIFY=0`
save round-trip + cross-target (build all 3 Linux arches) after each, regenerate
a fresh `TEST.CAR` because the `*Copy` layout changes (see the save-format
heads-up note in CLAUDE.md / memory).

1. **Prerequisite:** land the `nameTableOffset` / `teamListOffset` /
   `seedReorderFlag` renames from `STRUCT_FIELDS_UNK.md` (pure renames, no layout
   change). This makes the offset-byte mechanism explicit before you change it.
2. **`Playoffs_8`/`_4` → `Playoffs`** — smallest, only `qualifiedTeamIds[8]`
   grows; few consumers (`career_season.c:1498–1547`).
3. **`Leagues_*` / `QualifyingGroup_*`** — pointer-bearing but few access sites.
   First **confirm against `SWOS2.s`**: the `Leagues` record-walk count (§4c) and
   the `QualifyingGroup.countries[]` access mode (§4d) — both are the only
   medium-confidence items in this plan.
4. **`CompetitionLeague_*` → `CompetitionLeague`** — `divisions[4]` + `name[10]`
   (max, `_4_10_24`) + link arrays; touches `teams.c`, `career_season.c`,
   `diy_competition.c`, `englishLeaguesBlock`.
5. **`CompetitionCup_* → CompetitionCup`** and **`CompetitionCup2_* →
   CompetitionCup2`** last — the largest blast radius (`career_comp_copies.c`
   `.skip` regeneration, `careerContests`, the seed-list writers, the
   pointer-range checks). Do cup and cup2 in separate commits.

For each family commit, the lockstep set is: (a) the merged typedef in
`game_types.h`; (b) the `*` template + `*Copy` extern types in
`competition_data.c/.h`; (c) regenerated `.skip` in `career_comp_copies.c` and
the aggregate in `swos_globals_career_overlay.c`; (d) recomputed
`nameTableOffset`/`teamListOffset` bytes in every static record (or `offsetof`
conversions); (e) re-verified pointer-range checks; (f) a fresh save +
cross-target validation.

---

## 8. Summary

**Can we consolidate?** **Yes — cleanly.** Each variant family is one conceptual
record with a count/offset-driven variable tail. No sentinel terminates a record
internally, no code steps record-to-record by a fixed size, and the static tables
do not overlap each other. The only blocker your question raised — "the
translated assembly incrementing in fixed sizes" — **does not occur**: the only
fixed sizes are the BSS `.skip` reservations and the `careerContests` lengths,
both *defined as* `sizeof(variant)` and regenerated automatically/by recompute,
not hand-stepped.

**Suggested changes:**
1. Merge into **5 structs** (`CompetitionCup`, `CompetitionCup2`,
   `CompetitionLeague`, `Playoffs`, `Leagues`) + a unified `QualifyingGroup`,
   arrays at max size, unused tail left zero.
2. **Keep the self-describing `nameTableOffset`/`teamListOffset` bytes** (renamed)
   — they are what lets the polymorphic, type-erased accessors work — and
   **recompute their stored values** for the new layout.
3. **Regenerate the `.skip` reservations** in `career_comp_copies.c` and
   `swos_globals_career_overlay.c` to the new `sizeof`s, in the same commit as the
   type change (this is the highest-risk break).
4. Add explicit **`numRounds` / `numSeedTeams`** counts to the merged cup structs
   and **`numLeagues`** to `Leagues` (its records have no sentinel, §4c) to retire
   implicit/derived lengths; keep existing counts otherwise; leave the
   cross-record `0xFFFFFFFF` / `0xFF` sentinels for a later, separate pass.
6. **Verify two medium-confidence items against `SWOS2.s` first** (§4c/§4d): the
   `Leagues` record-walk count, and whether `QualifyingGroup.countries[]` is
   reached by struct field vs. immediately after the `compCup[]` `-1` terminator.
5. **Migrate one family per commit**, smallest first (Playoffs → Leagues/Quals →
   League → Cup → Cup2), with `VERIFY=0` + cross-target + fresh `TEST.CAR` each.

**Other recommendations:**
- Replace the address-span identity checks (§5d) with explicit id comparisons
  while you're touching those files — they violate the CLAUDE.md "no address-range
  gating" rule and are a latent cross-target bug.
- Resolve the `Competition.type` numeric-convention conflict
  (`STRUCT_FIELDS_UNK.md` §9.1) **before** introducing any `CompType` enum or
  switch over the merged structs.
- The save format is already template-reconstructed, so the merge needs only the
  header bytes kept ahead of the arrays and `careerContests[].length` kept in sync
  — no new serializer work.

---

## 9. Appendix — the `AnimTable` variant family (same per-size pattern, SAFE; one inert overlap noted)

`AnimTable` / `AnimTable_20` / `AnimTable_12` / `AnimTable_8` (`game_types.h`)
differ only in `int16_t *animFrames[N]` (N = 32 / 20 / 12 / 8) — the same IDA
"one struct per instance size" pattern as the competition family. Instances live
in `src/sprite_data.c`; SWOS2.s defines them contiguously at lines 34716–35762.

### 9a. Table-struct consolidation → one 32-entry `AnimTable`: **SAFE**

- **`animFrames[slot]` is INDEXED, not sentinel-walked.** The slot is a direction
  index `slot = (teamIdx + gkAdj)*8 + direction`, range **0..31** (`computeAnimSlot`,
  `player_anim.c:236`); the referee path is direction-only, range **0..7**
  (`InitRefereeAnimationTable`, SWOS2.s:15212). A **zero entry is a TRAP, not an
  overflow gateway**: `_SetPlayerAnimationTable` panics `0x000E` on a NULL
  `animFrames[slot]` (`player_anim.c:262`), the referee path panics `0x1112`.
- **Max reachable slot per table:** 32-entry player/GK tables
  (`playerRunningAnimTable`, `playerNormalStandingAnimTable`, all `goalie*` tables)
  use 0..31; outfield-only tables (tackling, tackled, throw-in, cards, headers)
  use 0..15; referee tables (`AnimTable_8`/`_12`) use 0..7. **No table is indexed
  past its size on a reachable path, and none reads into the adjacent table.**
- **Verdict:** merge all variants into one 32-entry `AnimTable`, zero-filling the
  high slots. This is safe and *strictly improves* the safety contract — a stray
  index now hits a zero and panics (`0x000E`/`0x1112`) instead of reading a
  neighbouring global's bytes. Before merging, grep the C tree for
  `sizeof(AnimTable_8/_12/_20)` or any loop bound derived from it (none expected).
- **`plTacklingAnimTable_2` (`AnimTable_20`) is DEAD** — referenced nowhere in
  SWOS2.s or the port; a redundant duplicate of `plTacklingAnimTable[0..15]`
  (SWOS2.s 35004, only its own definition). **Drop it** during the merge.

### 9b. ⚠️ Special note — frame-sequence *embedding* (the overlap to watch for): REAL but INERT here

Each `animFrames[i]` points to an `int16_t` frame **sequence** walked by control
codes (`-999`=reset to 0, `-101`=hold, `<=-100`=relative jump, `-1..-99`=set
delay, `>=0`=show that picture). These sequences are where SWOS *does* pack data
adjacently and let one label span several logical sequences:

`team2TackledDownLeftFrames` is declared `[26]` (`sprite_data.c:248`,
SWOS2.s:34264) but is logically only `{596, -999}` — the trailing **24 words are
8 embedded `{-50, frameN, -999}` sub-sequences** ("team1 standing-recovery") that
the assembler placed contiguously and IDA folded under the one label.

**This was investigated specifically and is INERT — no copy needed:**
- The **only** reference to `team2TackledDownLeftFrames` is `animFrames[13]` of
  `playerTackledAnimTable`, pointing at the **whole label (offset 0)**
  (SWOS2.s:35044). There is **no `label+N` / `&label[N]` reference** to the
  embedded sub-sequences anywhere in SWOS2.s **or** `src/`.
- The walk **cannot reach** them: at index 1 the `-999` resets to index 0, so the
  sequence oscillates on frame `596` forever — indices ≥2 are unreachable.
- ⇒ `team2TackledDownLeftFrames` behaves identically to a 2-word `{596, -999}`;
  the 24-word tail is dead filler the C port carries verbatim (harmless). The
  struct merge does not touch the sequences at all.

**Safeguard rule (generalised — the pattern that *would* require a copy).** This
embedding is the one place the "label spans multiple sequences" overlap shows up,
so before **trimming** any oversized frame-sequence array to its logical length
(e.g. `team2TackledDownLeftFrames[26] → [2]`), grep **both** SWOS2.s and `src/`
for a pointer into its middle (`label+N`, `&label[N]`, `(int16_t*)label + N`). If
one exists, that embedded sub-sequence is **live and shared** — repoint the
consumer to a standalone copy (or keep the block intact) **before** trimming.
Here none exists, so even trimming would be safe; but the AnimTable *struct* merge
requires no sequence changes regardless.
