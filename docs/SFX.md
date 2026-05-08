# SWOS SFX research

The in-game sound effects are stored in `bin/sound/sfx.sng` (note/sequence
table) + `bin/sound/sfx.in1` (sample bank), both in RJP1 format
(`RJP1SMOD` header). The bank holds **72 instruments** and **13
"subsongs"** (the latter being the preset table — 4-byte records, one
per preset, each byte = a noteId routed to one of the 4 channels).

This document tracks what each wave/note ID actually plays, sourced
from three places:

1. **In-code triggers** — the game already calls
   `_AudioChannelQueueNextWaveIfEnabled(channel, waveId)` and
   `_AudioApplyPresetToChannels(presetId)` with hardcoded ids in
   `src/sfx_crowd.c`. The function name + the id give us a strong
   hint about what the sound is meant to be.
2. **By-ear mapping** — debug SFX-test mode in `src/platform_sdl.c`
   plays an arbitrary id when a number key is pressed. The user
   listens and writes down what each id sounds like.
3. **SWOS 2020 sample manifest** — a separate fan project that
   shipped the same bank with named samples
   (`BGCRD3L / CHANT4L / KICKX / BOUNCEX / …`), giving us the
   asset-naming convention and roughly how many of each there are.

## How wave IDs become samples

The game's audio engine is a script-based player that doesn't play
samples directly — instead each wave script blob (a "note record")
contains opcodes that select an instrument and a period. We host
this on top of the existing RJP1 player by mirroring two paths:

| Game call | RJP1 call | Indirection chain |
|---|---|---|
| `_AudioChannelNoteOnOff(ch, noteId)` | `RJP1_StartSequenceByNoteId(p, ch, noteId)` | `song_data[3] + noteId*4` (BE32 offset) `+ song_data[5]` → first byte = wave id → `song_data[4] + waveId*4 + song_data[6]` = sequence_data |
| `_AudioChannelQueueNextWaveIfEnabled(ch, waveId)` | `RJP1_StartSequenceByWaveId(p, ch, waveId)` | `song_data[4] + waveId*4 + song_data[6]` = sequence_data |
| `_AudioApplyPresetToChannels(presetId)` | iterates 4 noteIds via `RJP1_PresetEntry` then `StartSequenceByNoteId` per channel | `song_data[2] + presetId*4` = 4-byte (one noteId per channel) |

`song_data[i]` is RJP1's terminology for what the Amiga audio engine
calls `bankSectionPtrs[i]`. The numbering is the same.

## In-code wave/note IDs

Pulled from `src/sfx_crowd.c` — every literal SFX id the game triggers,
grouped by the function that calls it.

| C trigger function | Path | id (dec/hex) | Meaning (guessed from fn name) |
|---|---|---|---|
| `_PlayFoulWhistleSample` | wave | 4 / `0x04` | FOUL whistle |
| `_PlayCornerKickSample` | wave | 15 / `0x0F` | Corner kick — but user heard "aww" at id 15, so the function name may be wrong; could be a generic crowd reaction (MISSGOAL?) |
| `_PlaySfxSample16` | wave | 16 / `0x10` | Generic kick variant — user heard "soft bounce" at id 16 |
| `_PlayGoalKickOffSample` | wave | 18 / `0x12` | Kick-off after goal — but user heard "aww" at id 18, function name suspect |
| `_PlayEndGameWhistleSample` | wave | 22 / `0x16` | ENDGAMEW (final whistle) |
| `_PlayMissGoalSample` | wave | 38 / `0x26` | MISSGOAL ("aaah") |
| `_PlayGoodPassSample` | wave | 39 / `0x27` | Crowd appreciation? |
| `_PlaySample3IfNotTraining` | wave | 44 / `0x2C` | Unknown |
| `_PlayRefereeWhistleSample` | wave | 49 / `0x31` | REF_WH (kickoff/restart whistle) |
| `crowdSamples[0..3]` | wave | 55, 56, 58, 62 / `0x37, 0x38, 0x3A, 0x3E` | Background crowd chants — likely BGCRD3L / CHANT4L / CHANT8L / CHANT10L |
| `_PlaySample(1)` | preset | 1 | Used by `_PlaySample1IfNotTraining` and `_PlaySample1` — likely match-event preset |
| `_PlaySample(3)` | preset | 3 | Used by `_PlaySample2IfNotTraining` |
| `_PlaySample(4)` | preset | 4 | Used by `_PlaySample4IfNotTraining` |
| `_PlaySample(6)` | preset | 6 | Used by `_PlayPenaltySample` |
| `_PlaySample(7)` | preset | 7 | Used by `_PlaySample7IfNotTraining` |

The two `_HandleKickSampleAndCrowd` callers that pass a runtime sfxId
(`_PlayKickSample`, the dynamic 0x25/0x28 alternation) and
`_PlayKickSample`'s code path also use 0x25 / 0x28 for kick variants.

## By-ear mapping (wave mode, channel 3)

User pressed digits in SFX-test mode — each maps to a wave_id. This
is the working table; correct any rows as we learn more.

| Wave id (dec/hex) | User heard | Probable SWOS 2020 name | Confidence |
|---|---|---|---|
| 1 / `0x01` | low background crowd | `BGCRD3L` | high |
| 2 / `0x02` | low background crowd (same as 1) | `BGCRD3L` second loop, or another bg variant | medium |
| 3 / `0x03` | crowd cheer | `HOMEGOAL` or one of CHANT4L/8L/10L | medium |
| 4 / `0x04` | whistle | `FOUL` (matches game's `_PlayFoulWhistleSample`) | high |
| 5 / `0x05` | crowd cheer | crowd chant variant | medium |
| 6 / `0x06` | crowd cheer | crowd chant variant | medium |
| 7 | (untested) | ? | — |
| 8 / `0x08` | crowd fast fade → background | end-of-chant transition | medium |
| 9 / `0x09` | same as 8 | another transition | medium |
| 10 / `0x0A` | (untested) | ? | — |
| 11 / `0x0B` | (untested — Shift+1 not pressed) | ? | — |
| 12 / `0x0C` | kick | `KICKX` variant | high |
| 13 / `0x0D` | bounce | `BOUNCEX` variant | high |
| 14 / `0x0E` | click sound | `BOUNCEX` soft variant or tackle | medium |
| 15 / `0x0F` | "awww" | `MISSGOAL` (matches what _PlayCornerKickSample plays — function name is wrong, this isn't a corner-kick sound) | high |
| 16 / `0x10` | soft bounce | `KICKX` or `BOUNCEX` (matches `_PlaySfxSample16`) | high |
| 17 / `0x11` | hard bounce | `KICKX` variant | medium |
| 18 / `0x12` | "awww" | `MISSGOAL` (matches what `_PlayGoalKickOffSample` plays — function name suspect) | medium |
| 19 / `0x13` | unknown | ? | — |
| 20 / `0x14` | (untested — Shift+0 not pressed) | ? | — |

**Important code finding:** `_PlayCornerKickSample` uses wave id 15
and `_PlayGoalKickOffSample` uses wave id 18, but the user heard
"awww" (a MISSGOAL crowd reaction) at both ids. Either these helper
functions are misnamed in our reverse-engineered C source, or the
wave at those ids really is a generic crowd reaction that gets
played in those contexts (corner kick — crowd reaction; goal kickoff
— crowd reacts to the goal that just happened). Both interpretations
are plausible; needs in-game testing to confirm.

## Untested ranges — TODO

The bank has 72 instruments and the wave/note tables are larger still
(unknown exact size). These ranges have NOT yet been mapped:

- 7, 10, 11, 20 (gaps in page 0)
- 21–30 (page 2 — Shift+1..0 of page 2)
- 31–40 — should include id 0x31 (REF_WH at 49 — wait, 49 is in page 4)
- 41–60 (covers known ids 0x2C, 0x31, 0x37, 0x38, 0x3A, 0x3E)
- 61–72 (last few instruments)

**Priorities:**

1. Walk page 4 (ids 31–40 then 41–50) to confirm 0x31 = REF_WH and
   to identify the score-callout samples (ONENIL/TWONIL/...).
2. Walk page 5–7 (ids 51–72) to find the chants (0x37/38/3A/3E) and
   the ENDGAMEW / HOMEWINL / BOOWHISL / NOTSING samples.
3. Walk note mode (`w` key in test mode) on a few of the same ids to
   see whether the indirection produces a recognizably different
   sample from wave mode — that confirms the table layout.

## SWOS 2020 sample manifest (reference)

This is the asset-naming convention from the SWOS 2020 fan project,
shipped with the same bank but with named samples. Multiple "X"
suffixes indicate per-event variants (e.g. `KICK1, KICK2, KICK3`).

| Name | Description |
|---|---|
| `BGCRD3L` | Background crowd noise — always on |
| `CHANT4L`, `CHANT8L`, `CHANT10L` | Three different crowd chants |
| `EREWEGO` | "Here we go" between chants |
| `COMBLUE / BROWN / GREEN / RED / WHITE / YELO` | Per-team-colour chants |
| `HOMEGOAL` | Crowd cheers a scored goal |
| `MISSGOAL` | Crowd "aaah" for a missed chance |
| `ONENIL` / `TWONIL` / `THREENIL` / `FOURNIL` / `FIVENIL` | Score-line callouts (1-0..5-0) |
| `EASY` | "Easy easy easy" — 2+ goals in short time |
| `HOMEWINL` | Final-score screen, home team won |
| `BOOWHISL` | Final-score screen, home team lost |
| `NOTSING` | Crowd silence after equalizer |
| `REF_WH`, `WHISTLE` | Referee whistles (kickoff/restart) |
| `FOUL` | Foul whistle |
| `ENDGAMEW` | Final whistle |
| `BOUNCEX` | Ball bouncing — multiple variants |
| `KICKX` | Ball kicked / passed / headed — multiple variants |

## How to extend this

1. Run with SFX-test mode (`./bin/SWOS`). Press digit keys at the menu;
   the first digit press loads the SFX bank. `q`/`a` paginate.
2. Listen, write down the sound for each id.
3. Add a row to the by-ear table above and update `src/sfx_ids.h`
   with a `#define` matching the SWOS 2020 name.
4. When all 72 instruments are mapped, remove the SFX-test handler
   from `pump_events_any_thread` and the lazy `_LoadSfxResources`
   trigger.

## Spectral fingerprint match against SWOS 2020 RAW set (2026-04-26)

Used the standalone `rjp1player` to render every wave id (`-t 30 -w SFX`)
and matched against the SWOS 2020 sample set from
`D:\Temp\SensibleSoccer\SWOS 2020\swos\audio\fx\` (32 named `.RAW`
files, all unsigned 8-bit PCM, ~22 050 Hz). Fingerprint = log-energy in
8 log-spaced bands (120 Hz..5 kHz) computed via Goertzel single-bin DFTs
on trimmed audio; matches ranked by L2 distance on shape + duration
similarity. **This is acoustic similarity only — the SWOS 2020 samples
are re-recordings, not byte-for-byte the same as the Amiga bank, so a
match is "sounds like" not "is".** Needs ear confirmation.

### New high-confidence additions
- `0x32` → **HOMEGOAL** — 10.92s render vs 11.48s SWOS 2020, near-identical
  band envelope. Almost certainly the goal-scored crowd cheer.
- `0x21` → likely a short voice line (3.44s). Best match is COMBROWN
  (3.19s) but spectrum is consistent with several team-colour shouts.
- `0x29` → 0.96s speech-like sample. Best match EASY/HOMESUBL — possibly
  a substitution call.
- `0x2A`, `0x2B` → 0.20-0.22s short whistle-like clips. Top match REF_WH/EASY.
- `0x2D`, `0x2E` → 0.12s. Fingerprint matches BOUNCEX exactly. More
  bounce variants in the 0x2C-0x2E run.

### Silent / null waves (rendered to ≈0 amplitude)
The renderer either produced no audible output or a single isolated
click for these wave ids — they likely reference dummy/null instrument
slots in the bank:
- `0x07`, `0x0B`, `0x14` — extractor skipped them entirely (silent script)
- `0x11`, `0x13`, `0x1D`, `0x1E`, `0x1F` — rendered files but with peak
  ≤ 0.5 and effective duration 0.00s after silence trim

The existing `0x11 BOUNCE_HARD_2` and `0x13` placeholder may not be
playable in practice. Worth checking the call-site code (does anything
ever trigger them?) before keeping them in `sfx_ids.h`.

### 30-second loop cluster (chant / ambience candidates)
These wave ids all hit the 30s render cap (i.e. they loop forever) and
their spectra cluster around the chant samples (`CHANT4L/8L/10L`,
`COMBLUE/COMWHITE`, `HOMEWINL`, `EREWEGO`):
`0x01, 0x02, 0x03, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x14, 0x15, 0x17,
0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x20, 0x22, 0x23, 0x2F, 0x30, 0x37,
0x38, 0x3A, 0x3E`. The 4 already-named chant ids (`0x37/38/3A/3E`)
sit inside this cluster, confirming the categorisation.

### Short kick/bounce cluster
`0x0C, 0x0D, 0x0E, 0x10, 0x25, 0x26, 0x27, 0x28, 0x2C, 0x2D, 0x2E`
all fingerprint as < 0.2s low-frequency-dominant transients matching
`BOUNCEX` and `KICKX`. Likely a mix of kick/bounce/header/tackle
variants — disambiguating which is which needs ear testing.

### Caveats
- The `Rough frequency` SoX metric is useless on the rendered waves
  (always reports ~9925 Hz, an artefact of the 44.1 kHz Paula → mixer
  interpolation). Ignore it; rely on the 8-band log-energy fingerprint
  in the script at `/tmp/spectral_match.py`.
- 30s renders for looping chants are arbitrary slices of the loop body
  — durations are NOT comparable to the SWOS 2020 RAW lengths for
  loops. Only short (< 5s) one-shot waves can be duration-compared.
