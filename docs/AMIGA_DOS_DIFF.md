# Amiga vs DOS SWOS — sprite index differences (`SPR_` defines)

Generated comparison of the `SPR_` sprite-index `#define`s between the two SWOS
reverse-engineering trees, to drive sprite remapping when loading DOS/2020
`.RPL`/`.HIL` replays (their frame streams store **DOS** sprite indices).

| Source | File | `SPR_` defines |
|--------|------|----------------|
| DOS    | `swos-port/swos/defs.h`     | 221 (decimal) |
| Amiga  | `SWOS-RE/SWOS2/SWOS2.h`     | 43 (hex) |

## Summary

- **43** `SPR_` names are common to both; **178** exist only in the DOS
  header (it defines many more individual frames).
- Of the 43 common names: **15 match**, **28 differ**.
- **Every differing sprite is offset by exactly `+114`** (DOS index = Amiga index + 114).
  These are the in-match / gameplay sprites — the DOS sprite set inserts 114 extra
  sprites ahead of them. The 0-offset matches are menu / UI sprites.

## Provenance & caveats

The Amiga `SWOS2.h` `SPR_` list was produced by **translating the DOS IDA database
to the Amiga**: the DOS enum was copied in, then the sprites that were *identified
in the Amiga* were corrected to their Amiga value (the `−114` gameplay shift). So:

- The **28 `+114` differs are confirmed Amiga corrections** (gameplay, DOS ≥ 227).
- The **15 matches are mostly menu/UI sprites that genuinely share an index**, but a
  few may simply be DOS values **carried over unchanged / not yet corrected** rather
  than independently-verified Amiga indices — treat them as "same unless proven
  otherwise".
- The Amiga list is **not exhaustive**: DOS sprites that don't exist in (or weren't
  found in) the Amiga version are absent — that's most of the 178 DOS-only names.

For the RPL loader this means: apply `dosIdx − 114` for the replay range, but expect
a few indices with no clean Amiga equivalent and handle them gracefully (skip /
fallback) rather than indexing past the sprite table.

### Mapping rule

The split is **clean at DOS index 227**: every matching sprite has a DOS index
≤ 226 (menu / UI), every `+114` sprite has a DOS index ≥ 227 (gameplay). So to
render a DOS replay sprite index with the **Amiga** sprite set:

```
amigaIdx = (dosIdx < 227) ? dosIdx : dosIdx - 114;
```
(the `- 114` matches the existing `color_sprites.c::_DosOverlaySprites`
`amigaIdx = dosIdx - 114`.)

**For `.RPL`/`.HIL` replays specifically** the frame stream only references sprite
indices ≥ 341, which are all ≥ 227 — so the remap is uniformly `dosIdx - 114`.

> Note: some Amiga indices are shared by more than one `SPR_` name (alias/reuse):
> `0xb7` (183) = SPR_SCROLL_ARROW_UP, SPR_RESULT_BIG_0
> `0xd6` (214) = SPR_8_MINS, SPR_LITTLE_PLAYER_GENERIC

## Differing sprites — DOS = Amiga + 114

| `SPR_` name | DOS | Amiga (hex) | Amiga (dec) | Δ (DOS−Amiga) |
|-------------|-----|-------------|-------------|---------------|
| `SPR_ADVERT_1_PART_1` | 227 | 0x71 | 113 | +114 |
| `SPR_ADVERT_2_PART_1` | 235 | 0x79 | 121 | +114 |
| `SPR_ADVERT_3_PART_1` | 243 | 0x81 | 129 | +114 |
| `SPR_RED_CARD_SMALL` | 253 | 0x8b | 139 | +114 |
| `SPR_YELLOW_CARD_SMALL` | 254 | 0x8c | 140 | +114 |
| `SPR_TEAM1_PLAYER_NAMES_START` | 255 | 0x8d | 141 | +114 |
| `SPR_TEAM2_PLAYER_NAMES_START` | 271 | 0x9d | 157 | +114 |
| `SPR_BIG_0` | 287 | 0xad | 173 | +114 |
| `SPR_RESULT_BIG_0` | 297 | 0xb7 | 183 | +114 |
| `SPR_TEAM1_NAME` | 307 | 0xc1 | 193 | +114 |
| `SPR_TEAM2_NAME` | 308 | 0xc2 | 194 | +114 |
| `SPR_BIG_DASH` | 309 | 0xc3 | 195 | +114 |
| `SPR_8_MINS` | 328 | 0xd6 | 214 | +114 |
| `SPR_GOALIE1_CAUGHT_BALL_RIGHT` | 976 | 0x35e | 862 | +114 |
| `SPR_GOALIE1_CAUGHT_BALL_LEFT` | 978 | 0x360 | 864 | +114 |
| `SPR_GOALIE2_CAUGHT_BALL_RIGHT` | 1106 | 0x3e0 | 992 | +114 |
| `SPR_GOALIE2_CAUGHT_BALL_LEFT` | 1108 | 0x3e2 | 994 | +114 |
| `SPR_BALL_SHADOW` | 1183 | 0x42d | 1069 | +114 |
| `SPR_CORNER_FLAG_FRAME_0` | 1184 | 0x42e | 1070 | +114 |
| `SPR_CORNER_FLAG_FRAME_3` | 1187 | 0x431 | 1073 | +114 |
| `SPR_SQUARE_GRID` | 1207 | 0x445 | 1093 | +114 |
| `SPR_REPLAY_FRAME_00` | 1209 | 0x447 | 1095 | +114 |
| `SPR_SELECT_SUBSTITUTE_ARROW` | 1284 | 0x492 | 1170 | +114 |
| `SPR_SUBSTITUTES` | 1288 | 0x496 | 1174 | +114 |
| `SPR_FORMATION` | 1289 | 0x497 | 1175 | +114 |
| `SPR_RED_CROSS_INJURY` | 1290 | 0x498 | 1176 | +114 |
| `SPR_RED_CARD` | 1291 | 0x499 | 1177 | +114 |
| `SPR_YELLOW_CARD` | 1292 | 0x49a | 1178 | +114 |

## Matching sprites — same index in both

| `SPR_` name | hex | dec |
|-------------|-----|-----|
| `SPR_SMALL_ZERO` | 0x8 | 8 |
| `SPR_EDIT_TACTICS_SMALL_PL_WHITE_GUY` | 0x7c | 124 |
| `SPR_NOT_PLAYING_WHOLE_SEASON` | 0xa1 | 161 |
| `SPR_SCROLL_ARROW_UP` | 0xb7 | 183 |
| `SPR_PL_FACE_WHITE_GUY` | 0xba | 186 |
| `SPR_CHECK_BIG` | 0xce | 206 |
| `SPR_NOT_CHECK_BIG` | 0xcf | 207 |
| `SPR_CHECK_SMALL` | 0xd0 | 208 |
| `SPR_NOT_CHECK_SMALL` | 0xd1 | 209 |
| `SPR_LITTLE_PLAYER_GOALIE` | 0xd5 | 213 |
| `SPR_LITTLE_PLAYER_GENERIC` | 0xd6 | 214 |
| `SPR_PREMIERE_DIVISION_WINNER` | 0xd7 | 215 |
| `SPR_INVITATION_TOURNAMENT` | 0xd8 | 216 |
| `SPR_CUP_WINNER` | 0xd9 | 217 |
| `SPR_TACTICS_PITCH` | 0xe2 | 226 |

