# STRUCT_FIELDS_UNK.md — Naming the `unkX` fields in the competition data structs

**Scope:** the competition-data struct family in `include/game_types.h` (lines ~1394–1801):
`Competition`, `Division`, `CompetitionLeague`, `CompetitionCup`, `Round`,
`CompetitionCup2`, all the `_N` / `_N_M` variant structs, `Playoffs_*`,
`Leagues_*`, `QualifyingGroup_*`, `CompetitionCareer`, `DIYStage`.

**Goal:** replace the `unkX` placeholders (and a couple of mislabelled fields)
with meaningful names, justified by how the original Amiga asm and the current
C port actually read each byte.

**Sources cross-checked:**
- `/mnt/c/Projects/AI/SWOS/OpenSWOS/data/SWOS2.s` — original 68k asm + EQU blocks (lines 1718–2134 define the struct field offsets; data tables at 117994–121230).
- `/mnt/c/Projects/AI/SWOS/OpenSWOS/data/Data.c` — IDA C export.
- SDL port: `src/diy_competition.c`, `src/teams.c`, `src/career_*.c`, `src/competition_data.c`, `src/save_format_json.c`.

**Convention for this report**
- *Suggested name* uses the project's camelCase style. A trailing **`?`** means the name is a best guess (medium/low confidence) — keep `unkX` until verified if you prefer.
- *Conf.* = High / Med / Low confidence in the meaning.
- Offsets are byte offsets inside the named struct. `db`=`int8_t`/`dc.b`, `dw`=`int16_t`/`dc.w`, `ptr`=`char *`/`dc.l`.

---

## Status (2026-06-18) — most of this is LANDED

This started as a backlog; nearly all of it is now applied in `include/game_types.h`:

- **Priority 1–4 — DONE.** `nameTableOffset`, `teamListOffset`, `seedReorderFlag`,
  `subsAndMaxSubs`, `subsAndPointsForWin`, `numRoundsOverride`, `roundStringBase`,
  `roundRuleFlags`, `teamSeedList`, `templateSize`, `playoffHeader`,
  `qualifiedTeamIds`, plus the merged `CompetitionLeague`/`CompetitionCup(2)`/
  `Leagues`/`Playoffs` consolidation (`name[]`, `playoffLinkPtrs`,
  `promotionLinkData`, `linkParams`, `qualifierIndexPairs`).
- **`CompetitionCup2.unk1/unk2/unk3` — DONE** (SWOS2.s cross-check, see §6):
  `seedingType` / `menuState` / `playEachTeam`.
- **Still `unkX` (deliberately).** `Competition.unk2`, `Division.unk1`,
  `CompetitionCup2.unk4`, `Leagues.linkParamsN[8]` — no confirmed runtime read;
  leave until proven against the SWOS2.s data tables.
- **Not a naming task: §9.1 `Competition.type` convention conflict** — still a
  latent bug to fix before any `CompType` enum.

> **Update (2026-09-30):** the "still `unkX`" list above is out of date — in
> `include/game_types.h` now: `Competition.unk2` (byte 6) is **`isPlayoff`**
> (Playoffs-vs-Cup discriminator); `Division.unk1` is **`qualifierLinkOffset`**;
> `CompetitionCup2.unk4` is **`reserved`** (confirmed unread; `a8af362`). Only the
> `Leagues.linkParams1..4[8]` bytes keep a generic name. Since then `CompetitionCup2`
> was renamed **`CompetitionTournament`** (header `CompTournamentHeader`, `aed8453`),
> and Untangle Step 4b (`481ee44`) moved `nameTableOffset`/`teamListOffset` reads
> behind accessors (`comp_name_table()`, `comp_team_seed_list()`), leaving those
> bytes as the inert `reserved1`/`reserved2`. The `game_types.h:NNN` line numbers
> in the sections below are stale — grep the struct name.

NB these competition templates are serialized as **opaque offset/size blobs** in
`save_format_json.c` (pointers re-derived from the static template on load), so
none of these renames touches a JSON key or the save format.

---

## 0. The single most important finding — two *self-describing offset bytes*

Almost nothing reads these structs by typed C field past the 9-byte `Competition`
header. Instead, **two stored bytes inside the header encode where the variable
tails live**, so one polymorphic accessor can walk a league, a cup, or a
group-tournament without knowing the concrete variant type:

| Header byte | Current name | What it really is | Access formula |
|---|---|---|---|
| offset **5** | `Competition.unk1` | byte distance from `base+5` to the `text[]` name-pointer table | `text = base + 5 + base[5]` (0 ⇒ use default string table) |
| offset **7** | `Competition.unk3` | byte distance from `base+7` to the trailing team-seed list | `teamList = base + 7 + base[7]` (0 ⇒ none / panic in DIY) |

Verified directly:
- `src/teams.c:3089` — `nameOff = compHdr->unk1; … nameTbl = (char**)(compEntry + 5 + off16);` (leagues index `[divIdx*2]`; cups/tournaments single ptr; default `divisionStringTable` / `aCup_0` / `aLeague_1`).
- `src/diy_competition.c:9070` — `seedTable = a5 + 7 + seedOff …;` then a `BE_READ_U16` loop over the team-seed pairs.
- Amiga asm: `lea 5(a5,d0.w),a0` (name table, SWOS2.s:52217/52307/52449) and `lea 7(a0,d0.w),a6` (team list, 18 sites incl. SWOS2.s:126050/126069/126086).

This is why every variant could be sized differently in IDA and still share one
accessor: the byte at +5 / +7 *absorbs* the variant's tail offset. Renaming
these two bytes is the highest-value change in this document, and it is also the
linchpin of the consolidation effort (see `STRUCT_CONSOLIDATE.md`).

---

## 1. `Competition` — 9-byte base header (`game_types.h:1398`)

```c
typedef struct { int8_t compNumber, type, teamNumber, beginMonth, endMonth,
                        unk1, unk2, unk3, unk4; } Competition;
```

| Current | Off | Sz | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|---|---|
| compNumber | 0 | 1 | db | *(keep)* | High | Competition id; copied to runtime `field_2E` (`diy_competition.c:9037/8867/8957`). |
| type | 1 | 1 | db | *(keep)* | High | Kind discriminator: **0=league, 1=cup, 2=group-tournament (Cup2)** (`diy_competition.c:9227` dispatch). ⚠️ See §9 — two *other* code paths use a different numeric mapping; reconcile before making an enum. |
| teamNumber | 2 | 1 | db | *(keep)* | High | **TEAM.XXX team-file selector.** Country/region number for national leagues & qualifiers (e.g. `frenchLeague`=13=`CN_FRANCE`→`TEAM.013`; WC-qual groups use region codes 80–85); **`0xFF` (255) = load from `.TMD` / special team file** (all the international cups). For national competitions it doubles as the `countriesTable` display-name index (`diy_competition.c:8813`, gated by `_IsNationalTeamCompetitionOrQualification`). NOTE: an earlier draft suggested `countryIndex` — rejected, because 255 isn't a country and 80–85 are regions; the team-file framing is the superset. |
| beginMonth | 3 | 1 | db | *(keep)* | High | Season **start** month, encoded **×8** (monthIdx = value/8; full range 0..0x60 = 12 months). Decoded `/8` and wrapped `+0x60` in scheduling (`diy_competition.c:779–826/9938`; SWOS2.s:67864). Not unknown — already correct, see note below. |
| endMonth | 4 | 1 | db | *(keep)* | High | Season **end** month, same ×8 encoding; `if (endMonth < beginMonth) endMonth += 0x60`. |
| **unk1** | 5 | 1 | db | **`nameTableOffset`** | High | Self-describing offset to `text[]` (see §0). 0 ⇒ default string table. |
| **unk2** | 6 | 1 | db | **`reserved6?`** | Low | No read found in asm or C; `$00` in every record. Almost certainly padding/alignment. Keep as `unk2`/`reserved` until SWOS2.s proves otherwise. |
| **unk3** | 7 | 1 | db | **`teamListOffset`** | High | Self-describing offset to the embedded team-seed list (see §0). |
| **unk4** | 8 | 1 | db | **`seedReorderFlag`** | High | `0` ⇒ shuffle the bracket/team-grid pairs (`_DIY_ShuffleTeamGridPairs`); non-zero ⇒ apply the fixed preset seed order from the team-seed list. `diy_competition.c:9020/9040`; SWOS2.s:52360/52385. |

> **Note on beginMonth/endMonth:** the raw values look strange (64, 32, −120…)
> only because they are month-index × 8 and IDA renders bytes ≥ 128 as signed.
> e.g. euroCup `begin=64 → month 8 (Sep)`, `end=32 → month 4 (May)`. These names
> are already correct; documenting the ×8 scale here so nobody "fixes" them.

---

## 2. `CompetitionLeague` — 13-byte league header (`game_types.h:1414`)

```c
typedef struct { Competition comp; int8_t divisionsCount, rounds, pointsForWin, subsReserves; } CompetitionLeague;
```

| Current | Off | Sz | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|---|---|
| divisionsCount | 9 | 1 | db | *(keep)* | High | Number of `Division` entries used; loop bound (`teams.c:3105`, `career_season.c:1601/1464`, SWOS2.s:52179/58063). **Count-driven array — no sentinel.** |
| rounds | 10 | 1 | dw* | `playEachTeamMode` | Med | Read as a **signed word**; high bit = group-phase flag, value = play-each-team count (`diy_competition.c:8874/8926`). The name "rounds" undersells it; consider `leagueFormat`. |
| pointsForWin | 11 | 1 | db | *(keep)* | High | → runtime `pointsForAWin` (`diy_competition.c:8924`). |
| subsReserves | 12 | 1 | db | `subsAndReserves` | Med | Packed nibbles: **hi = min subs, lo = MAX subs** (`diy_competition.c:8931`). ⚠️ Different packing from the cup field of the same name (§4). |

(No `unkX` in this struct — included for completeness and because the variant
families below extend it.)

---

## 3. `Division` — 6-byte division descriptor (`game_types.h:1426`)

| Current | Off | Sz | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|---|---|
| teams | 0 | 1 | db | *(keep)* | High | Teams in division; must be even (panic `0x1313`, `diy_competition.c:8850`). |
| promoted | 1 | 1 | db | *(keep)* | High | Auto-promotion slots → runtime `roundComplete` overlay. |
| promotedPo | 2 | 1 | db | *(keep)* | High | Promotion-playoff slots; sign-bit = playoff flag → runtime `competitionFinished`. |
| relegated | 3 | 1 | db | *(keep)* | High | Auto-relegation slots → runtime `currentRoundIndex` overlay. |
| relegatedPo | 4 | 1 | db | *(keep)* | High | Relegation-playoff slots → runtime `numRounds`. |
| **unk1** | 5 | 1 | db | **`qualificationOffset?`** | Low | **No runtime read** in the current C (the old qualification-offset walk was replaced by typed `englishLeaguesBlock` access — see `career_season.c:1422–1473`). Static values exist (46, 120, 194 in `englishLeaguesBlock._main.divisions[1..3]`, `competition_data.c:562`). Likely a byte offset into the English playoff link tables. Keep `unk1` until SWOS2.s confirms; medium confidence it *was* a qualification table offset, low confidence on the exact role today. |

> The static `promoted/relegated*` names describe what the bytes *mean*; they get
> copied into unrelated-sounding runtime overlay fields, but the static names are
> fine to keep.

---

## 4. `CompetitionCup` — 14-byte knockout-cup header (`game_types.h:1439`)

```c
typedef struct { Competition comp; int8_t unk1, teams, awayGoalsMode, subsReserves, firstRoundNo; } CompetitionCup;
```

| Current | Off | Sz | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|---|---|
| **unk1** | 9 | 1 | db | **`numRoundsOverride`** (a.k.a. third-place count) | Med-High | `if (cup->unk1 != 0) diyCompetitionStart.numRounds = cup->unk1;` (`diy_competition.c:9013`). The in-code comment calls it the **"third-place team count"**; the asm copies it into NUMROUNDS / `leagueFirstRoundNumber` (SWOS2.s:52355/61254). Name it for the *effect* (`numRoundsOverride`) and keep the third-place note. |
| teams | 10 | 1 | db | *(keep)* | High | Knockout bracket size, must be even (panic `0x1313`); `numRounds = log2(teams/2)` (`diy_competition.c:8945/8964/8985`); fills `cupTeamIds[0..teams-1]` (`career_status.c:4322`). **This is the team-list count for type-1 cups.** |
| awayGoalsMode | 11 | 1 | db | `awayGoalsTieBreakMode` | High | 0=none, 1=away-goals-count-double, etc. → runtime `$5E`; `cmpi.w #1,$5E` doubles away goals (SWOS2.s:50866/16979). (Note: `diy_competition.c:8979` also routes it to `playEachTeam` for one path — dual use.) |
| subsReserves | 12 | 1 | db | `subsAndPointsForWin` | Med | Packed nibbles: **hi = min subs, lo = points-for-a-win** (`diy_competition.c:8981`). ⚠️ lo nibble differs from the league field (§2, lo = max subs). This is byte 0xD — also the byte the save format persists as a dynamic counter for two WC/Oceania records (`save_format_json.c:2664`). |
| firstRoundNo | 13 | 1 | db | `roundStringBase` | Med | ⚠️ **Misnomer.** Per asm it's the cup-round-string / leg-label base (→ `$5C` → `GetNextCupRoundString`, SWOS2.s:52277/52894), **not** the bracket start index (that's a runtime field). BUT for friendly/invitation cups the C port reuses it as the participant count (`career_season.c:1251/1281`) and as a sub-round seed (`diy_competition.c:8958`). Dual-purpose; rename cautiously. |

---

## 5. `Round` — 3-byte sub-record, `CompetitionCup2.rounds[6]` (`game_types.h:1452`)

| Current | Off | Sz | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|---|---|
| teamCount | 0 | 1 | db | *(keep)* | High *(type-2 only)* | Teams entering this stage; round-robin math `(n-1)*n/2` (SWOS2.s:55241). |
| groupCount | 1 | 1 | db | *(keep)* | High *(type-2 only)* | Number of groups in this stage (`DIY_BuildTeamGridAndIndexMaps`, SWOS2.s:54614). |
| teamGroupCount | 2 | 1 | db | *(keep)* | High *(type-2 only)* | Teams per group. |

> **Important caveat:** these names are correct for the genuine `Round[6]`
> *inside* `CompetitionCup2` (group tournaments). They are **NOT** the same as
> the trailing `rounds[N]` / `roundSettings[N]` byte arrays in the cup variants —
> those are per-round packed flag bytes, see §7.

---

## 6. `CompetitionCup2` — 34-byte group-tournament header (`game_types.h:1462`)

```c
typedef struct { Competition comp; int8_t unk1, unk2, unk3, awayGoalsMode, subsReserves, firstRoundNo;
                 Round rounds[6]; int8_t unk4; } CompetitionCup2;
```

| Current | Off | Sz | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|---|---|
| ~~unk1~~ | 9 | 1 | db | **`seedingType`** ✅ | High | **RENAMED (SWOS2.s cross-check, 2026-06-18).** `move.b 9(a5),d0 → $160(a0)` copied to *every* stage slot's `seedingType` field (SWOS2.s:52485, `_InitDIYLeagueFromRecord`). The earlier `numStageRounds?` guess was wrong — `$160` is the slot's `seedingType` (offset 0x06), not a round count. |
| ~~unk2~~ | 10 | 1 | db | **`menuState`** ✅ | High | **RENAMED.** Direct `move.b $A(a5),d0 → $4A(a6)` = the named runtime `menuState` field (SWOS2.s:52458). Cleanest of the three — a straight copy into an already-named field. |
| ~~unk3~~ | 11 | 1 | db | **`playEachTeam`** ✅ | High | **RENAMED.** `move.b $B(a5),d6 → $166(a0)` copied to every stage slot's `playEachTeam` field (SWOS2.s:52488). |
| awayGoalsMode | 12 | 1 | db | `awayGoalsTieBreakMode` | High | Same as §4. |
| subsReserves | 13 | 1 | db | `subsAndPointsForWin` | Med | Same packing as §4. |
| firstRoundNo | 14 | 1 | db | `roundStringBase` / `rounds[0]` source | Med | Read at fixed `$F(a0)` as the first round/team count (`diy_competition.c:9073/9127`, SWOS2.s:126089). |
| rounds[6] | 15 | 18 | Round×6 | *(keep)* | High | The genuine group-stage table (§5). |
| **unk4** | 33 | 1 | db | **`firstRoundFlagByte?`** | Med-Low | The first byte of the trailing per-round flag array overlaps here; consumed by the round-flag loop, not as a scalar (SWOS2.s:52472). May just be padding/first-flag. |

---

## 7. The trailing arrays in the cup/league variants

These are the `rounds[N]` / `roundSettings[N]` / `unk1[N]` / `unk2[N]` / `unk3[N]`
arrays appended by each `_N` / `_N_M` variant. They are **not** distinct unknowns
per variant — they are three recurring roles:

### 7a. `rounds[N]` (in `CompetitionCup_4/_6/_10`) and `roundSettings[N]` (in `CompetitionCup2_*`)

| Current | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|
| `int8_t rounds[N]` / `roundSettings[N]` | db×N | **`roundRuleFlags[N]`** | High | One **packed flag byte per round**: bits 7-6 = legs (1=single, 2=two-leg), bits 5-4 = extra-time mode, bits 3-2 = penalties mode. Decoded in `diy_competition.c:9002–9005` (`roundLegs/roundExtraTime/roundPenalties = (flags>>6/4/2)&3`) and SWOS2.s:52335–52348. e.g. cupWinnersCup `0x94 = 10 01 01 00` → two-leg / ET / pens. **The IDA names "rounds"/"roundSettings" are misleading — these are per-round rules, walked count-driven (count = `log2(teams/2)`).** |

### 7b. Trailing `unk1[N]` in cup variants (`_6_2_8`, `_6_2_12`, `_6_2_64`, `_4_2_24`, …)

| Current | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|
| `int8_t unk1[N]` | db×N | **`teamSeedList[N]`** | High | The embedded competition draw: **big-endian `{countryNumber, teamOrdinal}` byte pairs** (for national teams the hi byte is a region/group prefix). Reached only via `base+7+teamListOffset` (§0). Length is **count-driven**: `N = 2 × count` where count = the type-keyed field (`teams` for cups / `firstRoundNo` for friendlies / `rounds[0]` for Cup2). Read with `BE_READ_U16` (`diy_competition.c:9089`; writers in `career_season.c`/`career_eurocups.c`; SWOS2.s:125219). Confirmed: cupWinnersCup `teams=32` → `unk1[64]`; WC-qual `firstRoundNo=6` → `unk1[12]`; euroCup 16 teams → `unk1[32]`. |

### 7c. Trailing `unk2[N]` / `unk3[N]` in league variants (`_2_2_24`, `_4_8_2_24`, `_2_4_2_24`, …)

| Current | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|
| `int8_t unk2[N]` / `unk3[N]` | db×N | **`promotionLinkData[N]`** / **`qualifierIndexPairs[N]`?** | Med | **NOT** a team-seed list (different role from §7b). Holds promotion/playoff structure: `englishLeaguesBlock._main.unk3[24]` and `._div1/._div2.unk2[24]` are walked as qualifier-index pair lists by the English playoff path (`career_season.c:1484/1488/1492`); contain `-1` sentinels and link to `Playoffs_*`/`Leagues_*` via the `text[]`/`unk2[]` pointers. |
| `char *unk2[2]` (in `_*_2_*` league variants) | ptr×2 | **`playoffLinkPtrs[2]`** | Med | Pointers to the league's `Playoffs_*` / `Leagues_*` records, e.g. `englishLeaguesBlock._main.unk2[0]` = `&div1Playoffs` (`career_season.c:1483`; `competition_data.c:564`). |

---

## 8. `Playoffs_*`, `Leagues_*`, `QualifyingGroup_*`, `CompetitionCareer`

### 8a. `Playoffs` (merged `Playoffs_8`/`_4`) — DONE ✅ (2026-06-18)

**FULLY DECODED.** The former opaque `playoffHeader[16]` is a CUP competition
record — a `CompCupHeader` (14 B) + `roundRuleFlags[2]`. `comp.type == 1`;
the qualified teams contest a knockout, initialised via the cup path
(`_Career_AutoAdvanceCupCompetitionFromRecord`). Name reached via
`comp.nameTableOffset` (=11 → `name[]`), team list via `comp.teamListOffset`
(=17 → `qualifiedTeamIds[]`). Cross-checked against the **in-game DIY editor**
(DUTCH / URUGUAYAN PLAYOFFS screenshots).

```c
typedef struct __attribute__((packed)) {
    Competition comp;            /* 0x00: cup header (type=1; teamNumber=country) */
    int8_t  numRoundsOverride;   /* 0x09: forces numRounds (Dutch=1 → single 4-team final) */
    int8_t  teams;               /* 0x0A: bracket size = qualified-team count (editor TEAMS) */
    int8_t  awayGoalsMode;       /* 0x0B: editor "AWAY GOALS" */
    int8_t  subsAndMaxSubs;      /* 0x0C: hi=subs, lo=bench (editor "SUBSTITUTES 3 FROM 5"=0x35) */
    int8_t  roundStringBase;     /* 0x0D */
    int8_t  roundRuleFlags[2];   /* 0x0E: per-round legs/ET/pens (0x94=2-leg+ET+pens, 0x14=1-leg) */
    char   *name[2];             /* 0x10: was text[2] */
    int8_t  qualifiedTeamIds[8]; /* was unk2: BE-packed qualified team ids, filled at runtime */
} Playoffs;
```

Screenshot ground-truth (Uruguay `{105,1,71,88,88,11,0,17,1,0,2,1,53,1,-108,0}`):
DEC–DEC = months 88/8=11; AWAY GOALS = byte11=1; SUBSTITUTES 3 FROM 5 = byte12
0x35; TEAMS 2 = byte10; FINAL 2 LEGS + ET + PENS = byte14 0x94. Dutch (4 teams,
`numRoundsOverride=1`) collapses to one FINAL → confirms byte 9.

### 8b. `Leagues_4` / `Leagues_2` / `Leagues_4_2_24` (`game_types.h:1723`)

```c
typedef struct { CompetitionLeague *text1; int8_t unk1[8]; CompetitionLeague *text2; int8_t unk2[8]; … } Leagues_4;
```

| Current | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|
| `text1..text4` | ptr | **`league1..league4`** | High | Pointers to the constituent `CompetitionLeague` records. |
| `unk1[8]..unk4[8]` | db×8 each | **`linkParams1..4[8]`?** | Low | 8-byte parameter block following each league pointer (promotion/relegation link config). Read only as static data in the current C; the runtime walk that consumed them was replaced by typed `englishLeaguesBlock` access. |
| `Leagues_4_2_24.playoffs` | ptr | **`playoffs`** | High | `Playoffs_8 *`. |
| `Leagues_4_2_24.leagues2` | ptr | **`nextLeagueGroup`** | Med | `Leagues_4 *`. |
| `Leagues_4_2_24.unk2[24]` | db×24 | **`qualifierIndexPairs[24]`** | Med | Same role as §7c (`._div1.unk2`, walked `career_season.c:1488`). |

### 8c. `QualifyingGroup_*` (`game_types.h:1762`)

| Current | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|
| `compCup[N]` | ptr×N | *(keep)* | High | Array of `CompetitionCup *` for the region's groups. |
| `countries[M]` | `CountryNumbers`×M | *(keep)* | High | The countries assigned to this qualifying region. |

### 8d. `CompetitionCareer` (`game_types.h:1752`)

| Current | Off | Type | Suggested | Conf. | Description / evidence |
|---|---|---|---|---|---|
| competition | 0 | ptr | *(keep)* | High | Pointer to the static template record. |
| competitionCopy | — | ptr | *(keep)* | High | Pointer to the BSS `*Copy` working buffer. |
| length | — | dw | **`templateSize`** | High | Byte size of the template = `sizeof(variant)`; drives the verbatim template→Copy `memcpy` (`career_init.c:703`, `save_format_json.c:2659`; SWOS2.s:61021). Not unknown, but the name `length` is vague. |

### 8e. `DIYStage` (`game_types.h:1795`)

No `unkX` fields; all named (`legs`, `extraTime`, `penalties`, `playCount`, `ptsForWin`). Nothing to do.

---

## 9. Cross-cutting issues uncovered while naming

1. **`Competition.type` has two conflicting numeric conventions in the C port.**
   The canonical dispatch (`diy_competition.c:9227`) uses **0=league / 1=cup / 2=cup2**,
   but `career_init.c:1714` treats `1=cup, 2=league` and `career_season.c:1564` /
   `career_eurocups.c:697` treat `2=cup, 0=league`. Same byte, different mapping per
   path. **Reconcile before introducing a `CompType` enum** — this is a latent bug
   magnet, not just a naming issue.

2. **`firstRoundNo` is a misnomer** (round-string base, not bracket start) and is
   *also* reused as a participant count for friendlies — flagged Med, rename with care.

3. **`subsReserves` packs differently for leagues vs cups** (lo nibble = max-subs vs
   points-for-win). Give the two structs *different* names (`subsAndMaxSubs` vs
   `subsAndPointsForWin`) so the divergence is visible.

4. **The `Round` field names are right only for type-2 group tournaments.** The
   same-looking trailing `rounds[N]`/`roundSettings[N]` byte arrays are per-round
   *flag* bytes (§7a) — do not let the names bleed across.

---

## 10. Summary — suggested renames (highest value first)

| Priority | Field(s) | Current → Suggested | Why |
|---|---|---|---|
| 1 | `Competition.unk1` | → **`nameTableOffset`** | Self-describing offset to `text[]`; read in ~10 sites. Renaming clarifies the whole accessor model and is prerequisite knowledge for `STRUCT_CONSOLIDATE.md`. |
| 1 | `Competition.unk3` | → **`teamListOffset`** | Self-describing offset to the team-seed list; read in ~15 sites. |
| 1 | `Competition.unk4` | → **`seedReorderFlag`** | 0=shuffle / nonzero=preset seed order. |
| 2 | cup variant `rounds[N]`/Cup2 `roundSettings[N]` | → **`roundRuleFlags[N]`** | Packed legs/ET/penalty bits per round — the "rounds" name hides that. |
| 2 | cup variant trailing `unk1[N]` | → **`teamSeedList[N]`** | BE `{country, ordinal}` draw pairs. |
| 2 | `CompetitionCareer.length` | → **`templateSize`** | `sizeof(variant)` for the template copy. |
| 3 | `CompetitionCup.unk1` | → **`numRoundsOverride`** | Overrides runtime `numRounds` (third-place count). |
| — | `Competition.teamNumber` | **keep as-is** | TEAM.XXX team-file selector (country/region for nationals, 255 = .TMD/special). An earlier `countryIndex` suggestion was rejected — 255 isn't a country. |
| 3 | `CompetitionCup.firstRoundNo` | → **`roundStringBase`** (cautiously) | Misnomer; round-string base + friendly participant count. |
| 3 | `*.subsReserves` | → **`subsAndMaxSubs`** (league) / **`subsAndPointsForWin`** (cup) | Different nibble packing per type. |
| 4 | `Playoffs_*.unk1[16]` | → **`playoffHeader[16]`** (break out `qualTeamCount`@10, `toUnk2Offset`@7) | Structured, not opaque. |
| 4 | `Playoffs_*.unk2[N]` | → **`qualifiedTeamIds[N]`** | Output id buffer. |
| ✅ done | `CompetitionCup2.unk1/unk2/unk3` | → **`seedingType`** / **`menuState`** / **`playEachTeam`** | SWOS2.s cross-check 2026-06-18 (§6): direct byte→slot/runtime-field copies. `unk1` was NOT `numStageRounds`. |
| 5 | `Competition.unk2`, `Division.unk1`, `CompetitionCup2.unk4`, `Leagues_*.unkN[8]` | leave as `unkX` / mark `?` | No confirmed runtime read; verify against SWOS2.s data values before naming. |

### Recommendations / next steps

1. **Land the priority-1 renames first** (`nameTableOffset`, `teamListOffset`,
   `seedReorderFlag`). They are pure renames (no layout change), touch known sites,
   and unlock the consolidation work. Do them as one commit per field with a
   `VERIFY=0` save round-trip and a cross-target check, per the project's
   `.equ`-untangle cadence.
2. **Do not rename the genuinely-unknown fields** (priority 5) yet — keep `unkX`
   with the `?`-tagged guesses in this doc as a backlog. Confirm their values
   against the SWOS2.s data tables (lines 117994–121230) first; several are likely
   true padding and can become `reservedN`.
3. **Treat the `type`-convention conflict (§9.1) as a bug to fix**, not a naming
   task. It must be resolved before any `CompType` enum is introduced.
4. **Keep this doc in sync** with `STRUCT_CONSOLIDATE.md`: the `nameTableOffset` /
   `teamListOffset` renames and the consolidation both touch the same stored bytes.

---

## 11. `TeamId` — the {country, ordinal} pair (partly done; TODO)

The fundamental SWOS team identity is a 2-byte `{countryNumber, teamOrdinal}`
pair (cf. `TeamFile`@0, `NationalTeamRecord`@0). A two-byte struct is
**endian-neutral** (each byte keeps its position on LE and BE), so it removes a
whole class of byte-pair-vs-host-uint16 bugs. The packed comparison key the game
uses is the big-endian word — `TEAMID_PACK(t)` = `PACK_BE_U16(country, ordinal)`.

**DONE (2026-06-18):** added `TeamId` + `TEAMID_PACK` (`game_types.h`) and applied
it to the **static** competition draws — `CompetitionCup`/`CompetitionCup2`
`teamSeedList` is now `TeamId[32]` (was `int8_t[64]`), the 6 non-empty
initializers re-bracketed into pairs (byte-identical, verified), and the
`diy_competition.c` seed-reorder reader uses `const TeamId *` + `TEAMID_PACK`.
No save-format impact (static `const` templates).

**TODO — migrate the RUNTIME team-id buffers to `TeamId`** (separate,
save-format-aware pass; do NOT drive-by):
- Buffers: `diyTournamentTeamIds`, `cupTeamIds`, `euroCupCopy`/`cwcCopy`/`uefaCopy`
  team lists, the WC/EC qualifier lists, `diyQualifiedTeamIndices`, and
  `Playoffs.qualifiedTeamIds` (currently `int8_t[8]`, written at runtime).
- These are **saved to disk** and read/written with `BE_READ_U16`/`BE_WRITE_U16`/
  `PACK_BE_U16` across `career_eurocups.c`, `career_season.c`, `career_status.c`,
  `diy_competition.c`. Converting them touches the JSON serializer and many
  load-bearing consumers — see the memory notes on BE-pair traps
  (`cup_team_list_be_pairs`, `offer_table_word_vs_be_pair`, score-swap).
- Approach: migrate one buffer at a time; replace `BE_READ_U16(p)` with
  `TEAMID_PACK(*(TeamId*)p)` and `BE_WRITE_U16(p, id)` with field stores; keep the
  packed key for comparisons; re-verify each save round-trips cross-target.
