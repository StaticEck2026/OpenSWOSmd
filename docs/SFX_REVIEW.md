# SWOS Amiga SFX bank — expert review checklist

> **Status (2026-09-30):** a tester's by-ear pass (`987705e`, 2026-05-05) has
> since renamed most ids in `include/sfx_ids.h` (e.g. `SFX_CROWD_BG_1`,
> `SFX_WHISTLE_FOUL_1`, `SFX_BALL_KEEPER_SAFE_2`, `SFX_CROWD_CHANT_1..4`). The
> **Macro** column below still shows the pre-review placeholder names, which no
> longer exist in the source; the header is the current authority. The preset
> macros (`SFX_PRESET_*`) are unchanged. Kept as the checklist for the ids and
> presets still marked untested there.

This file is for a SWOS expert to fill in the *correct* identity of every
sound effect in `bin/sound/SFX.{SNG,IN1}`. We've already mapped some by
hand (in-game testing, source-symbol names) and made educated guesses
for others. Many remain unknown.

## How to listen to each sample

The standalone player at `C:\msys64\home\headk\rjp1player\` can render
every wave id to a separate `.WAV` file:

```
cd C:\msys64\home\headk\rjp1player
.\rjp1.exe -p -t 30 -w SFX
```

This writes `SFX_wave01.WAV` … `SFX_wave3E.WAV` in the same folder.
`-p` selects the PAL Paula clock, `-t 30` caps each render at 30s
(looped samples will fill the cap; one-shots end naturally and the
trailing silence is trimmed). 51 ids extract; 21 are silent / null
slots in the bank and are skipped automatically.

The SWOS 2020 `.RAW` set lives at
`D:\Temp\SensibleSoccer\SWOS 2020\swos\audio\fx\` — those are the
named, English-described variants of the same SFX library and are
the canonical reference for what each sound *should* be called.

## How to fill this in

Each row below is one wave id from the Amiga bank. We've prefilled:

- **Macro** — the C `#define` we currently use for this id (in
  `include/sfx_ids.h`). May be a placeholder name like `SFX_VOICE_LINE_21`.
- **Length / Peak** — duration in seconds + max amplitude (0..1) of
  the rendered WAV. "29.x" + a peak < 1.0 means the sample loops
  forever and we capped at 30s.
- **Conf** — confidence: `src` = id appears as a literal in
  `src/sfx_crowd.c` (so we know the *event* it's wired to, but not
  the actual SWOS 2020 name); `ear` = a previous in-game listen
  guessed it; `?spec` = nearest match by acoustic fingerprint to
  the SWOS 2020 RAW set (needs verification); `?` = untested.
- **Our guess** — what we currently *think* the sound is.

**You please fill in:**

- **SWOS 2020 name** — the matching `.RAW` filename (e.g. `KICKX`,
  `BOUNCEX`, `REF_WH`, `HOMEGOAL`, `EREWEGO`, etc.). If two ids
  share the same SWOS 2020 sample, write the same name in both rows.
  If an id's sound has no equivalent in the SWOS 2020 set (e.g.
  silence/click/test tone), write `(silent)` or `(unused)`.
- **Suggested macro** — what the constant in `include/sfx_ids.h`
  *should* be called. If our placeholder is fine, just write "ok"
  or leave blank. Otherwise propose a clearer name (e.g.
  `SFX_KICK_HARD` instead of `SFX_KICK_25`).
- **Notes** — any extra context: "this is a pair with id 0x0D",
  "alias for 0x37", "only ever triggered after a save", etc.

---

## Wave id table (51 rows)

| Wave id | Macro | Len (s) | Peak | Conf | Our guess | **SWOS 2020 name** | **Suggested macro** | **Notes** |
|---|---|---|---|---|---|---|---|---|
| 0x01 | `SFX_BG_CROWD_1` | 29.6 (loop) | 0.06 | ear | low background crowd loop | | | |
| 0x02 | `SFX_BG_CROWD_2` | 29.9 (loop) | 0.06 | ear | low background crowd (= 0x01?) | | | |
| 0x03 | `SFX_CROWD_CHEER_1` | 29.7 (loop) | 0.43 | ear | crowd cheer / chant | | | |
| 0x04 | `SFX_FOUL_WHISTLE` | 0.26 | 0.34 | src+ear | FOUL whistle (used by `PlayFoulWhistleSample`) | | | |
| 0x05 | `SFX_CROWD_CHEER_2` | 29.8 (loop) | 0.73 | ear | crowd cheer / chant | | | |
| 0x06 | `SFX_CROWD_CHEER_3` | 29.9 (loop) | 0.73 | ear | crowd cheer / chant | | | |
| 0x07 | (none — silent) | — | — | ?spec | not extracted (silent script) | | | |
| 0x08 | `SFX_CROWD_FADE_1` | 29.9 (loop) | 0.59 | ear | crowd-chant tail / transition | | | |
| 0x09 | `SFX_CROWD_FADE_2` | 29.9 (loop) | 0.59 | ear | same as 0x08 | | | |
| 0x0A | (none) | 29.9 (loop) | 0.44 | ?spec | crowd loop variant — untested | | | |
| 0x0B | (none — silent) | — | — | ? | not extracted (silent script) | | | |
| 0x0C | `SFX_KICK_1` | 0.16 | 0.80 | ear | kick — KICKX or BOUNCEX | | | |
| 0x0D | `SFX_BOUNCE_HARD` | 0.15 | 0.77 | ear | hard bounce — BOUNCEX | | | |
| 0x0E | `SFX_BOUNCE_CLICK` | 0.10 | 0.87 | ear | click / soft bounce / tackle | | | |
| 0x0F | `SFX_MISSGOAL_1` | 0.85 | 0.52 | src+ear | "awww" reaction (passed by `PlayCornerKickSample` — fn name is wrong) | | | |
| 0x10 | `SFX_KICK_SAMPLE16` | 0.16 | 0.39 | src+ear | soft bounce (passed by `PlaySfxSample16`) | | | |
| 0x11 | `SFX_BOUNCE_HARD_2` | 0.02 | 0.04 | ?spec | rendered to silence — likely null / dummy slot | | | |
| 0x12 | `SFX_MISSGOAL_2` | 1.01 | 0.52 | src+ear | "awww" reaction (passed by `PlayGoalKickOffSample` — fn name suspect) | | | |
| 0x13 | (none — silent) | 0.02 | 0.91 | ?spec | one click then silence — likely null | | | |
| 0x14 | (none — silent) | 29.9 (loop) | 0.45 | ?spec | extracted but inaudible after trim — possibly DC-loop | | | |
| 0x15 | (none) | 29.7 (loop) | 0.57 | ? | crowd loop variant — untested | | | |
| 0x16 | `SFX_END_GAME_WHISTLE` | 1.82 | 0.28 | src | ENDGAMEW (final whistle, used by `PlayEndGameWhistleSample`) | | | |
| 0x17 | (none) | 29.6 (loop) | 0.40 | ? | crowd loop variant | | | |
| 0x18 | (none) | 29.6 (loop) | 0.43 | ? | crowd loop variant | | | |
| 0x19 | (none) | 30.0 (loop) | 0.40 | ? | crowd loop variant | | | |
| 0x1A | (none) | 30.0 (loop) | 0.40 | ? | crowd loop variant | | | |
| 0x1B | (none) | 29.5 (loop) | 0.41 | ? | crowd loop variant | | | |
| 0x1C | (none) | 29.7 (loop) | 0.98 | ?spec | crowd loop variant (loud) | | | |
| 0x1D | (none — silent) | 0.02 | 0.03 | ?spec | rendered to silence — likely null | | | |
| 0x1E | (none — silent) | 0.02 | 0.02 | ?spec | rendered to silence — likely null | | | |
| 0x1F | (none — silent) | 0.02 | 0.03 | ?spec | rendered to silence — likely null | | | |
| 0x20 | (none) | 29.5 (loop) | 0.45 | ? | crowd loop variant | | | |
| 0x21 | `SFX_VOICE_LINE_21` | 3.46 | 0.95 | ?spec | short voice line — closest match COMBROWN (3.19s) | | | |
| 0x22 | (none) | 29.9 (loop) | 0.66 | ? | crowd loop variant | | | |
| 0x23 | (none) | 29.8 (loop) | 0.67 | ? | crowd loop variant | | | |
| 0x25 | `SFX_KICK_25` | 0.16 | 0.71 | src | kick variant A (`PlayKickSample` alternates 0x25/0x28 each tick) | | | |
| 0x26 | `SFX_MISSGOAL` | 0.12 | 0.68 | src | MISSGOAL (used by `PlayMissGoalSample`) | | | |
| 0x27 | `SFX_GOOD_PASS` | 0.11 | 0.80 | src | crowd appreciation (used by `PlayGoodPassSample`) | | | |
| 0x28 | `SFX_KICK_28` | 0.09 | 0.70 | src | kick variant B (alternates with 0x25) | | | |
| 0x29 | `SFX_SUB_CALL` | 0.98 | 0.28 | ?spec | speech-like ~1s — closest match EASY/HOMESUBL | | | |
| 0x2A | `SFX_REF_WHISTLE_2A` | 0.24 | 0.25 | ?spec | short whistle blip | | | |
| 0x2B | `SFX_REF_WHISTLE_2B` | 0.22 | 0.23 | ?spec | short whistle blip | | | |
| 0x2C | `SFX_NOT_TRAINING_3` | 0.15 | 0.67 | src | unknown — passed by `PlaySample3IfNotTraining` | | | |
| 0x2D | `SFX_BOUNCE_2D` | 0.14 | 0.66 | ?spec | bounce variant — fingerprint matches BOUNCEX | | | |
| 0x2E | `SFX_BOUNCE_2E` | 0.13 | 0.66 | ?spec | bounce variant — fingerprint matches BOUNCEX | | | |
| 0x2F | (none) | 30.0 (loop) | 0.27 | ? | crowd loop variant | | | |
| 0x30 | (none) | 29.6 (loop) | 0.27 | ? | crowd loop variant | | | |
| 0x31 | `SFX_REFEREE_WHISTLE` | 0.10 | 0.23 | src | REF_WH / WHISTLE (used by `PlayRefereeWhistleSample`) | | | |
| 0x32 | `SFX_HOME_GOAL` | 10.94 | 0.66 | ?spec | goal cheer — duration matches HOMEGOAL (11.48s) | | | |
| 0x37 | `SFX_CHANT_4L` | 29.5 (loop) | 0.88 | src | CHANT4L (`crowdSamples[0]`) | | | |
| 0x38 | `SFX_CHANT_8L` | 29.7 (loop) | 0.70 | src | CHANT8L (`crowdSamples[1]`) | | | |
| 0x3A | `SFX_CHANT_10L` | 29.1 (loop) | 0.66 | src | CHANT10L (`crowdSamples[2]`) | | | |
| 0x3E | `SFX_EREWEGO` | 29.1 (loop) | 0.58 | src | EREWEGO (`crowdSamples[3]`) | | | |

## Preset table (route through `AudioApplyPresetToChannels`)

A "preset" is a bank entry that triggers up to 4 noteIds, one per
channel — used for multi-sample stings (e.g. a goal celebration that
combines a cheer, a whistle, and a chant). The Amiga code triggers
these by ID; we don't have full visibility into what each one
actually plays.

| Preset id | Macro | Triggered by | **What it sounds like** | **Suggested macro** |
|---|---|---|---|---|
| 1 | `SFX_PRESET_NOT_TRAINING_1` | `PlaySample1IfNotTraining`, `PlaySample1` | | |
| 3 | `SFX_PRESET_NOT_TRAINING_2` | `PlaySample2IfNotTraining` | | |
| 4 | `SFX_PRESET_NOT_TRAINING_4` | `PlaySample4IfNotTraining` | | |
| 6 | `SFX_PRESET_PENALTY` | `PlayPenaltySample` | | |
| 7 | `SFX_PRESET_NOT_TRAINING_7` | `PlaySample7IfNotTraining` | | |
| 0, 2, 5, 8..N | (none) | — | untested presets — what's in the bank? | |

## SWOS 2020 sample manifest (reference)

These are the named samples in the SWOS 2020 fan release. Use them
to populate the **SWOS 2020 name** column above. For samples that
appear in SWOS 2020 but *don't* have an Amiga match in the wave
table above, please flag them at the bottom — that tells us which
events use a *preset* instead of a single wave id.

```
AWAYSUBL    Away-team substitution announcement
BGCRD3L     Background crowd noise (always-on layer)
BOOWHISL    Boo / disapproval whistle (final-score, lost)
BOUNCEX     Ball bouncing — multiple variants
CHANT4L     Crowd chant variant
CHANT8L     Crowd chant variant
CHANT10L    Crowd chant variant
CHEER       Generic cheer
COMBLUE/BROWN/GREEN/RED/WHITE/YELO  Per-team-colour chants
EASY        "Easy easy easy" — 2+ goals in short time
ENDGAMEW    Final whistle
EREWEGO     "Here we go here we go here we go"
FIVENIL/FOURNIL/THREENIL/TWONIL/ONENIL  Score-line callouts
FOUL        Foul whistle
GOALKICK    Goal kick
HOMEGOAL    Crowd cheers a scored goal (home team)
HOMESUBL    Home-team substitution announcement
HOMEWINL    Final-score screen, home team won
KICKX       Ball kicked / passed / headed — multiple variants
MISSGOAL    Crowd "aaah" for a missed chance
NOTSING     Crowd silence after equalizer
REF_WH      Referee whistle (kickoff/restart)
WHISTLE     Referee whistle (alias of REF_WH? please confirm)
```

### SWOS 2020 samples *not yet matched* in the wave table

Please list any SWOS 2020 samples above that don't appear anywhere
in your filled-in wave-id table. Those are likely triggered by a
*preset* (multi-channel sting) rather than a single wave id, and
the preset table above is what we need to figure out for them.

```
(your notes here)
```

---

## When done

Send this back filled in and we'll:
1. Rename every `#define` in `include/sfx_ids.h` to the suggested macro.
2. Promote `?spec`/`?` confidences to `expert`.
3. Update `SFX.md` with the canonical SWOS 2020-name → wave id table.
4. The literals in `src/sfx_crowd.c` already use the macros, so the
   renames propagate automatically — no other source-file edits
   needed.
