# DIY_Competition field_2 and field_3A Sub-field Analysis

## field_2 (offset 0x02, 48 bytes, spans 0x02-0x31)

IDA defines this as `DIY_COMPETITION_FIELD_2 EQU 2` with size 48. The asm accesses within it use `FIELD_2+offset`, so effective struct offset = 2 + sub-offset.

### Sub-fields

| Struct Offset | Sub-offset | Size | Type | Suggested Name | Init Value | Description |
|---------------|-----------|------|------|----------------|------------|-------------|
| 0x02 | +0x00 | 2 | uint16_t | `isActive` | 0 | Competition active flag. Set to 0 during init, 1 when competition is initialized/in use |
| 0x04 | +0x02 | 36 | int8_t[36] | `competitionName` | "DIY CUP" etc. | Competition name string buffer (displayed in menu titles) |
| 0x28 | +0x26 | 4 | uint32_t | `namePtr` | (already named) | Raw 68k pointer to competition name string |
| 0x2C | +0x2A | 1 | uint8_t | `compId` | (already named) | Competition type ID (0=league, 1=cup, 2=tournament) |
| 0x2D | +0x2B | 1 | uint8_t | `slotId` | — | Competition slot index for cache matching. ASM reads 0x2C as a word (`move.w $2C,d0`) then compares low byte (`cmp.b (a6),d0`), meaning `slotId` is the byte actually matched |
| 0x2E | +0x2C | 1 | uint8_t | `field_2E` | 0xFF in init | Team configuration byte (copied during slot setup) |
| 0x2F | +0x2D | 1 | uint8_t | `viewRandomPhase` | (already named) | Current month/phase for schedule tracking |
| 0x30 | +0x2E | 1 | uint8_t | `avgSkill` | (already named) | Average team skill level |
| 0x31 | +0x2F | 1 | uint8_t | `strengthTier` | (already named) | Strength bracket tier |

### Rename recommendation for field_2

Replace `uint8_t field_2[0x26]` with:
```c
uint16_t isActive;              /* 0x02: 0=inactive, 1=in use */
int8_t   competitionName[36];   /* 0x04: name string buffer */
```

The remaining bytes (0x28-0x31) are already broken out as individual fields.

---

## field_3A (offset 0x3A = 58, 36 bytes, spans 0x3A-0x5D)

IDA defines this as `DIY_COMPETITION_FIELD_3A EQU 58` with size 36. The asm accesses within it use `FIELD_3A+offset`, so effective struct offset = 58 + sub-offset.

### Sub-fields

| Struct Offset | Sub-offset | Size | Type | Current Name | Suggested Name | Init Value | Description |
|---------------|-----------|------|------|-------------|----------------|------------|-------------|
| 0x3A | +0x00 | 2 | int16_t | `resultCode` | `resultCode` | 0xFFFF | Match/competition result code (keep) |
| 0x3C | +0x02 | 2 | int16_t | `substitutesCount` | `substitutesCount` | 1 | Number of substitutes allowed (keep) |
| 0x3E | +0x04 | 2 | int16_t | `progressField4` | `slotStateA` | 3 | Competition slot state A — stores menu selection ordinal for team1/fixture. Init from `primaryCompSlotStateA`. Saved/restored by `Career_Load/StoreCompetitionSlotStateFromBuffer` |
| 0x40 | +0x06 | 2 | int16_t | `progressField6` | `slotStateB` | 2 | Competition slot state B. Init from `primaryCompSlotStateB` |
| 0x42 | +0x08 | 2 | int16_t | `progressField8` | `slotStateC` | 8 | Competition slot state C — stores menu selection ordinal for team2/fixture. Init from `primaryCompSlotStateC` |
| 0x44 | +0x0A | 2 | int16_t | `progressFieldA` | `matchStarted` | 0 | Set to 1 when match processing begins. Sync'd between `diyCompetitionStart` and `diyFileBufferCopy` |
| 0x46 | +0x0C | 2 | int16_t | `progressFieldC` | `careerTeamInvolved` | 0 | Set to 1 when the career team participates in a match |
| 0x48 | +0x0E | 2 | int16_t | `hasGroups` | `hasGroups` | 0 | Groups/divisions flag (0=no, 1=groups, 2=groups with final) (keep) |
| 0x4A | +0x10 | 2 | int16_t | `field4A` | `menuState` | 0 | Cup menu state machine: 0=play, 1=round draw, 2=show draw, 3=replays, 4=next round, 5=exit, 6=finished |
| 0x4C | +0x12 | 2 | int16_t | `nonNationalCount` | `roundIterator` | — | Round iteration counter. Also copied to extended 0x60 (`minSubstitutes`) during slot setup |
| 0x4E | +0x14 | 2 | int16_t | `field_4E` | `numTeamsInStage` | — | Number of teams in current stage/sub-round. Set from `numTeams` during group setup |
| 0x50 | +0x16 | 2 | int16_t | `halfTeams` | `matchPairsPerRound` | — | Number of match pairs (half the teams in the round) (rename for clarity) |
| 0x52 | +0x18 | 2 | int16_t | `totalRounds` | `totalMatchesInRound` | — | Total matches/fixtures in the current round. Also used as total round count for group scheduling |
| 0x54 | +0x1A | 2 | int16_t | `currentRound` | `roundComplete` | 0 | Round completion flag: 0=in progress, 1=just completed |
| 0x56 | +0x1C | 2 | int16_t | `finishedFlag` | `competitionFinished` | 0 | 1 when competition is fully complete (keep but rename for clarity) |
| 0x58 | +0x1E | 2 | int16_t | `exitFlag` | `currentRoundIndex` | 0 | Current round index (incremented after each completed round) |
| 0x5A | +0x20 | 2 | int16_t | `numRounds` | `numRounds` | — | Total number of rounds in competition (keep) |
| 0x5C | +0x22 | 2 | int16_t | `subRoundCounter` | `subRoundCounter` | 1 | Sub-round counter for multi-fixture rounds (keep) |

Note: `playEachTeam` at offset 0x5E is technically outside `field_3A` (which ends at 0x5D per the 36-byte size declaration). It's a separate field.

---

## Functions that access these fields

### field_2 (isActive + competitionName)
- `_InitDIYLeague`, `_InitDIYTournament`, `_InitDIYCup` — clear `isActive` to 0, copy name strings
- `_InitCompetitionFromRecord` — sets `isActive` to 1
- `_DIYCupInit`, `_DIY_PlayCupMatchAndAdvance` — set `isActive` to 1
- `_DIY_SyncProgressToDiyFileBufferCopy` / `_DIY_LoadProgressFromDiyFileBufferCopy` — copy between buffers
- `menu_data.c` — reads `competitionName` (offset 4) for menu title display
- `_Career_SetCurrentCompetitionSlot` — copies `compId`, `slotId`, `avgSkill`, etc. during slot setup

### field_3A sub-fields
- `_Career_SetCurrentCompetitionSlot` — initializes slotStateA/B/C from `primaryCompSlotState*` globals
- `_Career_Load/StoreCompetitionSlotStateFromBuffer` — sync slotStateA/B/C between runtime and save buffers
- `_DIYCupOnSelect3-8` — store menu ordinals into `slotStateA` (0x3E) and `slotStateC` (0x42)
- `_DIYCupInit` / `_DIYCupFinish` — read `menuState` (0x4A) to decide which UI to show
- `_DIY_PlayCupMatchAndAdvance` — sets `matchStarted`, `careerTeamInvolved`; checks `roundComplete`, `competitionFinished`
- `_DIY_UpdateCompetitionProgress` — increments `currentRoundIndex`, checks `totalMatchesInRound`
- `_Career_AutoAdvanceCupMatches` — reads `roundComplete`, `competitionFinished`

---

## Summary of recommended renames

| Offset | Current | Recommended |
|--------|---------|-------------|
| 0x02 | `field_2[0x26]` (first 2 bytes) | `isActive` (uint16_t) |
| 0x04 | `field_2[0x26]` (remaining 36 bytes) | `competitionName[36]` (int8_t) |
| 0x2D | `field_2D` | `slotId` |
| 0x3E | `progressField4` | `slotStateA` |
| 0x40 | `progressField6` | `slotStateB` |
| 0x42 | `progressField8` | `slotStateC` |
| 0x44 | `progressFieldA` | `matchStarted` |
| 0x46 | `progressFieldC` | `careerTeamInvolved` |
| 0x4A | `field4A` | `menuState` |
| 0x4C | `nonNationalCount` | `roundIterator` |
| 0x4E | `field_4E` | `numTeamsInStage` |
| 0x50 | `halfTeams` | `matchPairsPerRound` |
| 0x52 | `totalRounds` | `totalMatchesInRound` |
| 0x54 | `currentRound` | `roundComplete` |
| 0x56 | `finishedFlag` | `competitionFinished` |
| 0x58 | `exitFlag` | `currentRoundIndex` |
