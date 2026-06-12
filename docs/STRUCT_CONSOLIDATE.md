# STRUCT_CONSOLIDATE.md — Merging the competition struct variant families

**Scope:** the per-instance variant struct families in `src/game_types.h`:
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
| `QualifyingGroup_3/_9_2/_2_8/_6_44/_3_4/_3_12` | 6 | `compCup[N]` count + `countries[M]` count |

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
| country tables (`englandTable[5]`, …) | fixed array length | `competition_data.c:932–996` |

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

**All three families can be consolidated into single max-sized structs.** No
sentinel terminates a record's tail (the tails are count/offset-driven), and no
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

```c
/* One struct per type family. Arrays sized to the observed maximum; unused
 * tail bytes left zero. Field names per STRUCT_FIELDS_UNK.md. */

#define COMP_MAX_ROUNDS      10   /* CompetitionCup_10 */
#define COMP_MAX_NAMES        6   /* CompetitionLeague_3_6 text[6]; cups use [2] */
#define COMP_MAX_SEED_TEAMS  32   /* uefaCup/cupWinnersCup/worldCup2: 64 bytes */

/* type == 1 : knockout cups (merges CompetitionCup_4/_6/_10/_4_2/_6_2/_6_2_8/_12/_64) */
typedef struct __attribute__((packed)) {
    CompetitionCup header;                       /* 14 B (Competition + cup fields) */
    uint8_t  roundRuleFlags[COMP_MAX_ROUNDS];    /* per-round legs/ET/pen bits */
    char    *name[2];                            /* was text[2] */
    uint16_t teamSeedList[COMP_MAX_SEED_TEAMS];  /* BE {country,ordinal} pairs */
} CompetitionCupRec;

/* type == 2 : group tournaments (merges all CompetitionCup2_*) */
typedef struct __attribute__((packed)) {
    CompetitionCup2 header;                      /* 34 B incl Round[6] */
    uint8_t  roundRuleFlags[COMP_MAX_ROUNDS];
    char    *name[2];
    uint16_t teamSeedList[COMP_MAX_SEED_TEAMS];
} CompetitionCup2Rec;

/* type == 0 : leagues (merges CompetitionLeague_1.._4 and the _N_M variants) */
typedef struct __attribute__((packed)) {
    CompetitionLeague header;                    /* 13 B */
    Division divisions[4];                        /* divisionsCount says how many used */
    uint8_t  reserved1;                           /* the old per-variant `unk1` byte */
    char    *name[COMP_MAX_NAMES];                /* full+abbrev per division */
    char    *playoffLinkPtrs[2];                  /* was the char* unk2[2] in _*_2_* */
    uint8_t  promotionLinkData[24];               /* was unk2[24]/unk3[24] */
} CompetitionLeagueRec;
```

`Playoffs_8`/`_4` → one `Playoffs` with `qualifiedTeamIds[8]`. `Leagues_2`/`_4`
→ one `Leagues` with 4 `{CompetitionLeagueRec*, linkParams[8]}` records +
`divisionsCount`-style used count. `QualifyingGroup_*` → one with `compCup[9]`
and `countries[44]` plus explicit `numGroups`/`numCountries`.

> **Design note — keep the polymorphic accessor working.** Because the mixed
> pointer arrays (`continentalCups`, `nationalTeamCompetitionsAndQualifications`,
> the country tables) store records *type-erased* as `Competition*`/`CompetitionCup*`,
> a type-known `offsetof(CompetitionCupRec, name)` is NOT usable at the
> polymorphic read sites (they don't know whether the pointer is a cup, a cup2 or
> a league — those have `name[]` at different offsets). **The self-describing
> `nameTableOffset`/`teamListOffset` bytes are exactly what makes the polymorphic
> access work, so KEEP them** (renamed) and recompute their stored values for the
> new fixed layout. Use the typed `name[]`/`teamSeedList[]` fields only at sites
> where the concrete type is known.

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
  `continentalCups`, `nationalTeamCompetitionsAndQualifications`) **can optionally
  be replaced by explicit element counts.** Low priority and orthogonal to the
  struct merge — the sentinels are not fragile (they don't depend on record
  layout). Do this only as a separate readability pass if desired.

**Net recommendation:** keep the existing count/offset mechanism; add two small
explicit counts (`numRounds`, `numSeedTeams`) to the merged cup structs to retire
the runtime derivation; leave the pointer-array sentinels as-is for now.

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
4. **`CompetitionLeague_*` → `CompetitionLeagueRec`** — `divisions[4]` + `name[6]`
   + link arrays; touches `teams.c`, `career_season.c`, `diy_competition.c`,
   `englishLeaguesBlock`.
5. **`CompetitionCup_* → CompetitionCupRec`** and **`CompetitionCup2_* →
   CompetitionCup2Rec`** last — the largest blast radius (`career_comp_copies.c`
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
1. Merge into **5 structs** (`CompetitionCupRec`, `CompetitionCup2Rec`,
   `CompetitionLeagueRec`, `Playoffs`, `Leagues`) + a unified `QualifyingGroup`,
   arrays at max size, unused tail left zero.
2. **Keep the self-describing `nameTableOffset`/`teamListOffset` bytes** (renamed)
   — they are what lets the polymorphic, type-erased accessors work — and
   **recompute their stored values** for the new layout.
3. **Regenerate the `.skip` reservations** in `career_comp_copies.c` and
   `swos_globals_career_overlay.c` to the new `sizeof`s, in the same commit as the
   type change (this is the highest-risk break).
4. Add explicit **`numRounds` / `numSeedTeams`** counts to the merged cup structs
   to retire the runtime length derivation; keep existing counts otherwise; leave
   the cross-record `0xFFFFFFFF` pointer-array sentinels for a later, separate pass.
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
