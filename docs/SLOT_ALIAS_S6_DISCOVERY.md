# SLOT_ALIAS S6 — discovery + classification (slot 0 / primary)

> Status (2026-09-30): **complete** -- S6 landed (`b533c57`, follow-ups `b3a113d`, `4fc8f33`). The 18 slot-0 aliases no longer exist, and the `.equ` lines this doc cites at `src/swos_competition_slots.c:71-88` are gone. Kept as the record of how.

Status: **DISCOVERY ONLY — no code changed.** Read-only classification of every
slot-0 alias reference, produced before any migration edit. Pairs with
`docs/COMPETITION_SLOT_ALIAS_UNTANGLE.md` §3 (slot 0) and §6 (increment plan).
S6 is the last and biggest SLOT_ALIAS slot; S1–S5 are done.

The 18 slot-0 aliases re-base onto `primaryCompetitionBuf`
(`CompetitionSlotBuffer = { DIYTournamentState tourn; uint8_t stageSnapshot[...]; }`)
via `.equ` lines in `src/swos_competition_slots.c:71-88`. Goal: delete those 18
aliases, convert every consumer to typed field access, rewrite the save
descriptors — **byte-identical at the save level** (FX/FA store `&sym`, and every
typed field sits at the same address the alias pointed at). The only risk is a
**wrong field path** → silent save corruption. Hence this classification.

---

## 1. Headline numbers

- **18 aliases**, **11 consumer files** (the 10 with `competitionFileBuffer` +
  `career_eurocups.c`, found via coverage check — 2 clean `primaryCompWinnerTeam`
  refs, and a known sync-divergence red-flag → back-port carefully).
- **`competitionFileBuffer`: 96 code refs** — ≈ **12 VALUE / 69 BASE / 15 EXTERN**.
  This is the value-vs-base split the plan flagged as "the single most
  error-prone sub-task." **All 96 classified, zero unclassifiable.**
- The 5 definitional/stub files (`swos_globals.c`, `swos_globals_career_overlay.c`,
  `swos_data_typed_stubs.c`, `swos_data_bulk_imports.c`, `swos_amiga_data_stubs.c`)
  contain **only comments / commented-out markers** for these names — **no
  migration edits needed there** (optional cosmetic comment refresh).

The classification rule (confirmed clean everywhere):
- **VALUE** = read as the int16 competition type: `*(int16_t*)competitionFileBuffer`,
  `*(uint16_t*)competitionFileBuffer`, `competitionFileBuffer[0]`, scalar-extern
  compares (`== 1/2`). → `primaryCompetitionBuf.tourn.header.competitionType`.
- **BASE** = used as an address: any pointer cast (`(DIYCompetition*)`,
  `(DIYCompetitionEx*)`, `(uint8_t*)`, `(volatile uint16_t*)`), `&competitionFileBuffer`,
  passed to a fn taking a pointer, or an `FA(... FLD_BYTES)` blob.
  → `&primaryCompetitionBuf` (with the **same cast**; note `(T*)array` →
  `(T*)&struct` — the `&` is mandatory because the alias was an array and the
  buffer is a struct).
- **EXTERN** = a declaration line → all collapse to one file-scope
  `extern CompetitionSlotBuffer primaryCompetitionBuf;`.

---

## 2. The clean 15 (S4-pattern renames — every target field already exists)

The D1 unions landed in S1–S3, so all of these target names are present in
`DIYCompetition` today (`game_types.h:592-668`), each pinned by a `_Static_assert`.

| alias | offset | → typed field (`primaryCompetitionBuf.tourn.header.…`) | kind |
|---|---|---|---|
| `competitionFileBuffer` | 0 | `.competitionType` (VALUE) / `&primaryCompetitionBuf` (BASE) | dual value/base |
| `competitionOver` | 0x02 | `.competitionOver` (union w/ isActive) | clean |
| `diyPresetNameOfCompetition` | 0x04 | `.competitionName` (int8[36], addr-of for name uses) | clean |
| `majorTrophyBonusValue` | 0x30 | `.trophyBonusValue` (union w/ avgSkill, byte) | clean¹ |
| `numDiyTeams` | 0x32 | `.numTeams` | clean |
| `primaryCompWinnerTeam` | 0x3A | `.winnerTeam` (union w/ resultCode) | clean |
| `primaryCompSquadViewParam` | 0x3C | `.squadViewParam` (union w/ substitutesCount) | clean |
| `primaryCompSlotStateA/B/C` | 0x3E/40/42 | `.slotStateA/B/C` | clean |
| `diyTeamMinDivisionIndex` | 0x54 | `.minDivisionIndex` (union w/ roundComplete) | clean |
| `diyTeamMaxDivisionOffset` | 0x56 | `.maxDivisionOffset` (union w/ competitionFinished) | clean |
| `leaguePromotionCount` | 0x58 | `.promotionCount` (union w/ currentRoundIndex) | clean |
| `leagueRelegationCount` | 0x5A | `.relegationCount` (union w/ numRounds) | clean |

¹ `majorTrophyBonusValue → .trophyBonusValue` keeps **`FLD_U16`** in the save
descriptor even though the field is a byte — `&field`==0x30 and FLD_U16 reads
0x30-0x31 (avgSkill+strengthTier), byte-identical to the legacy alias read. This
is exactly the S4 precedent (`minor_trophy_bonus_value`). **Do not "fix" the width.**

---

## 3. The 3 flagged items (need a decision before migrating)

### FLAG-1 — `leagueWarningsEnabled`@0x60 has no `tourn`-view field
- Lands at exactly `sizeof(DIYCompetition)` (0x60 on 32-bit, 0x68 on 64-bit) =
  the **start of `DIYTournamentState._reserved_5A`** padding. When the buffer is
  viewed as a `DIYCompetitionEx`, that byte is `minSubstitutes`@0x60 (itself
  dual-use: minSubstitutes / playEachTeam).
- Uses: **2 value reads** (`career_status.c:1357, 1468`, both `== 0`) + **1 save
  descriptor** (`save_format_json.c:1149`, FX FLD_U16) + 3 extern decls. Plain
  host-native, **not BE**.
- **Recommendation (D-W1):** add a named leading member to the reserved region of
  `DIYTournamentState` — e.g. replace `uint8_t _reserved_5A[0x9A - sizeof(DIYCompetition)]`
  with `uint16_t leagueWarningsEnabled;` + a `uint8_t _reserved_5C[0x9A - sizeof(DIYCompetition) - 2]`
  pad, and add a `_Static_assert(offsetof(DIYTournamentState, teamIds) == 0x9A)`
  guard (already present) + one pinning `leagueWarningsEnabled` at
  `sizeof(DIYCompetition)`. Then consumers and the descriptor read
  `primaryCompetitionBuf.tourn.leagueWarningsEnabled` — clean, byte-identical,
  consistent with teamIds/groupMap/slots being named fields.
- Alternative (D-W2): `((DIYCompetitionEx*)&primaryCompetitionBuf)->minSubstitutes`
  — semantic-but-misnamed for the league-warnings use; rejected on clarity.

### FLAG-2 — `diyTeamIdListType0/1` are mid-field BE arrays spanning multiple regions
Historical contiguous flat views (sizes from bulk-imports comments):
`type2[148]`@0x9A, `type0[288]`@0x12E, `type1[2310]`@0x24E (each ends where the
next begins; together 0x9A→0xB54, overrunning `teamIds`+`groupMap`+`slots`+
`teamIndexOrder` and ~1.8KB into `stageSnapshot`). They do **not** align to single
`tourn`-view fields. **But** the code already accesses them via the
`(DIYCompetitionEx*)` cast typed paths, so the semantic mapping is known:

| alias | offset | semantic typed path (already used in code) | save anchor |
|---|---|---|---|
| `diyTeamIdListType2` | 0x9A | `primaryCompetitionBuf.tourn.teamIds` | clean (`&teamIds`==0x9A ✓) |
| `diyTeamIdListType0` | 0x12E | `((DIYCompetitionEx*)&primaryCompetitionBuf)->matchData.league.leagueTeamIds` | mid-`groupMap` |
| `diyTeamIdListType1` | 0x24E | `((DIYCompetitionEx*)&primaryCompetitionBuf)->cupTeamIds` | `&slots[2].numTeams` |

- **Access stays `BE_READ_U16` / `BE_WRITE` / `PACK_BE` verbatim** — only the base
  symbol changes (`[[project_cup_team_list_be_pairs]]`). Confirmed sites:
  `diy_competition.c:9934,9982` (BE_READ_U16), `transfer_market.c:986/988/990`
  (the three list casts, downstream BE loop).
- **Recommendation (D-A1):** consumers → the semantic `DIYCompetitionEx`-cast paths
  above (consistent with `diy_competition.c`/`menus.c`). Save descriptors (3 FA
  FLD_BYTES blobs, spans 148/288/2310) → use the same anchors **with a loud comment
  that the span intentionally overruns the anchor field into following regions**
  (legacy contiguous team-id views). If the cast-through anchor fails as a static
  address constant, fall back to `(uint8_t*)&primaryCompetitionBuf + 0xNN` with a
  comment (D-A2). All three are FLD_BYTES (raw copy, no endian swap in the
  descriptor itself).

### FLAG-3 — `transfer_market.c:4202` pre-existing raw-offset 64-bit hazard (NOT an S6 fix)
```c
uint16_t *compBuf = (uint16_t *)&competitionFileBuffer;
if (compBuf[0] == 2) insertPos = compBuf[0x4E/2];  else insertPos = compBuf[0x66/2];
```
`compBuf[0]` = VALUE (compType); `compBuf[0x4E/2]`/`compBuf[0x66/2]` are **unshifted
raw byte-offset reads** that bypass the typed struct — same class as
`[[project_raw_offset_64bit_shift_trap]]` (the +4 CFB_NP namePtr slide means these
likely already read the wrong field on 64-bit). **S6 only rebases the symbol
(`&competitionFileBuffer`→`&primaryCompetitionBuf`), preserving current behaviour.
Do NOT fix the offset during S6** — that changes behaviour and breaks bisect.
Leave a `/* TODO raw offset, see FLAG-3 */` and address it in a separate commit.

---

## 4. Pre-existing concern noted, out of S6 scope

`save_format_json.c:4750/4948` build a legacy `[lo,hi)` **address-range** emit/load
(`lo = &competitionFileBuffer`, `hi = &numSelectedTeams + 2`). This is the exact
address-span pattern CLAUDE.md forbids — but it is **pre-existing**, the rename
does not change the addresses, and the plan (§5) says blob/range dumps stay
byte-identical this pass and are promoted to typed fields later. **Migrate the
symbol as-is; do not touch the range logic in S6.**

---

## 5. Per-file site inventory (condensed)

`.X` = `primaryCompetitionBuf.tourn.header.X`. All EXTERN → one
`extern CompetitionSlotBuffer primaryCompetitionBuf;`.

### save_format_json.c (save-critical core)
- `competitionFileBuffer`: **1 VALUE** (978: FX `primary_slot_type` → `.competitionType`),
  ~28 BASE (function bodies 3560…5127, all `(DIYCompetitionEx*)`/`(DIYTournamentState*)`/
  `(uint8_t*)&…+offsetof(stageSnapshot)`; casts unchanged), 1 FA blob (1264
  `primary_competition_context` → `primaryCompetitionBuf`), 2 extern (96, 4473).
- Descriptor block `s_career_scalars[]` 1136-1149 + 1236-1238: the 17 FX/FA rewrites
  (see §2/§3 tables). `numDiyTeams` also has ~14 body scalar reads → `.numTeams`.
- 14 externs at 824-837 + per-region blocks 898-900 / 2006-2008 → delete (folded).

### diy_competition.c
- `competitionFileBuffer`: **0 VALUE**, 15 BASE, 2 EXTERN (113, 2390 in
  `#ifdef SWOS_DEBUG_AUTOSKIP`). Casts: `const uint8_t*`, `uint8_t*`,
  `(const)volatile uint16_t*` (copyWords 1144-1226), `DIYCompetitionEx*` (3486, 9900).
- `primaryCompSlotStateA/B/C`: 3 VALUE (3521-3523) + 3 EXTERN (149-151).
- **BE arrays carried here**: `_InitializeNewSeason_BuildTeamBudgetRankTable`
  (9900/9902 base) reads league→`compEx->matchData.league.leagueTeamIds`,
  cup→`compEx->cupTeamIds`, tournament→`(uint16_t*)(raw+0x9A)`; `BE_READ_U16` @9934.
  Swapping the base symbol migrates all three; `raw+0x9A`/`raw+0x5A` literals ride
  the new base unchanged.

### career_season.c
- `competitionFileBuffer`: 2 BASE (534 `computeCategoryForCompetition`, 605 local
  `comp=`), 1 EXTERN (422).

### career_status.c / statistics.c / job_offers.c (combined)
- `competitionFileBuffer`: 5 EXTERN, **3 VALUE** (status 1356/1467 `*(int16_t*)…!=2`,
  stats 942 debug log), 16 BASE (mostly `(DIYCompetition*)`/`(DIYCompetitionEx*)`
  casts; status 1730 reads `->base.numTeams` via Ex cast — leave the field access,
  swap base only).
- `majorTrophyBonusValue` 7 VALUE → `.trophyBonusValue`; `numDiyTeams` 5 VALUE →
  `.numTeams`; `primaryCompWinnerTeam` 1 VALUE (status 1892) → `.winnerTeam`;
  `leaguePromotionCount`+`leagueRelegationCount` (status 1360) → union names;
  `diyTeamMinDivisionIndex`+`diyTeamMaxDivisionOffset` (job_offers 1216) → union names.
- `leagueWarningsEnabled`: 2 VALUE (status 1357/1468) + 1 EXTERN → **FLAG-1**.
- **No BE-array refs in this group.**

### menus.c / career_init.c / transfer_market.c / career_teams.c (combined)
- `competitionFileBuffer`: 5 EXTERN, **8 VALUE** (menus 3567/3602/3635/3668/3751
  `*(uint16_t*)…` compType dispatch, 3542 byte[0] log, transfer 985/987 `== 1`/`== 2`),
  8 BASE (`careerCompetitionSlotPtrTable[0] = (uint8_t*)&…` init 1062/1324/1650,
  load-from-buffer 1708/1397, menus 6366 `(DIYCompetitionEx*)`, transfer 4202 → FLAG-3).
- `competitionOver` 8 VALUE (menus 433-435 clears, 2242/2248/3546/3550 reads, 3541
  log) → `.competitionOver`; `diyPresetNameOfCompetition` 3 BASE (menus 2241/2247
  `nameStr=`, 4465 `.fg = (uintptr_t)…competitionName`, **@0x04 name not @0**) →
  `.competitionName`; `primaryCompSlotStateA/B/C` 9 VALUE (init 703-705 init values,
  1154-1156/1170-1172 save/restore around namePtr) → `.slotState*`;
  `primaryCompSquadViewParam` 1 VALUE (menus 6336) → `.squadViewParam`; `numDiyTeams`
  1 VALUE (transfer 992) → `.numTeams`.
- **BE arrays**: transfer_market 986/988/990 (`diyTeamIdListType1/2/0` casts) → the
  §3 FLAG-2 typed paths; keep BE_READ_U16.

### career_eurocups.c (11th file — not in initial fork scope)
- `primaryCompWinnerTeam`: 1 EXTERN (669), 1 VALUE (789 `worldCupHostTeam =
  primaryCompWinnerTeam;` → `.winnerTeam`). Both clean. **Sync red-flag — back-port
  carefully** (`[[project_sync_list_gaps_amiga_divergence]]`).

---

## 6. Decisions needed before migration

1. **D-W1 vs D-W2 (FLAG-1):** name the 0x60 field in `DIYTournamentState`
   (recommended D-W1) or read `minSubstitutes` via Ex cast (D-W2)?
2. **D-A1 vs D-A2 (FLAG-2):** semantic `DIYCompetitionEx`-cast anchors for the two
   mid-field BE blobs (recommended D-A1) or `(uint8_t*)&buf + 0xNN` offset macros (D-A2)?
3. Confirm **FLAG-3** (transfer 4202) and the §4 address-range are left untouched in S6.

## 7. Migration shape (once decisions are made)

Single S6 commit (per plan §6 step 5): struct-prep (D-W1 field + asserts) → remove
the 18 SLOT_ALIAS lines → convert the 11 consumer files → rewrite the 17 FX/FA
descriptors → build x86_64/armhf/aarch64 (asserts pin offsets) → VERIFY=0 on a
re-saved career → cross-target Amiga test. Delegate the mechanical edit to a fork
that names `30f7aa7` (S4) as the template and is handed §2/§3/§5 verbatim plus the
BE-array DO-NOT-retype-access warning.
