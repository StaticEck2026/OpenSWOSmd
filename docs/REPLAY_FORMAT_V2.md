# Replay Format v2 — Design Doc

Status: **proposal / design** (no code yet). Goal: a resolution-independent,
seekable, fully-audible match-replay format to replace/supersede the current
`.HIL` / `.RPL` (+ `.IDX`) family.

---

## 1. Why

The current replay formats were reverse-engineered from the Amiga (`.HIL`) and
the DOS/SWOS-2020 community build (`.RPL` + `.IDX`). They work, but they were
never designed for what we now want:

- **No recorded audio.** A replay only ever plays the *crowd* (and even that
  only when the crowd-chant state machine is enabled — see §3). The ball
  bounces, kicks, tackles, whistle and goal cheers you hear in a live match are
  produced by the **live game engine** as physics/events happen. A replay does
  **not** re-run the engine — it plays back recorded sprite positions — so none
  of those SFX exist in the stream. There is nowhere in `.HIL`/`.RPL` to put
  them.
- **Resolution-coupled camera.** The camera is recorded in the *recording's*
  screen space (336 native, or 480 for SWOS-2020 widescreen). Playing back at a
  different render width needs re-centering and, for tight following, a
  reconstructed ball-tracked camera (we implemented both in `highlight.c`, but
  it's a workaround layered on a format that bakes in a fixed view width).
- **Seek is bolted on.** `.IDX` was added in 2020 purely to enable
  fast-forward / rewind / slow-mo / speed-up by indexing frame offsets. It's a
  side-car file, easy to lose or desync from its `.RPL`.
- **One opaque blob.** No versioning room, no metadata, no clean place to add
  features without breaking older readers.

We want to **record a whole match once** and play it back perfectly at any
resolution, with all the sound, and with proper trick-play — in a single,
versioned, self-describing file.

---

## 2. What the current formats actually contain

(From `highlight.c` `SaveCoordinatesForHighlights` / the playback decode, and
`dos_replay_load.c` `convert_frames`.)

Per **frame**, big-endian 32-bit words:

| Word | Meaning |
|------|---------|
| `0x8000xxxx \| Y` | **frame-start marker** (bit 31 set): camera `X<<16 \| Y` |
| `team1Goals<<16 \| team2Goals` | scoreboard |
| `animPatternsState` (×2) | crowd/fan animation state |
| `idx<<20 \| x<<10 \| y` (bit 31 clear) | **sprite** — one per on-pitch object |
| `0xFFFFFFFF` | end-of-frame |

- `.HIL` — Amiga-native, multi-**scene** (each saved highlight is a scene),
  big-endian, 19000-byte scene slots.
- `.RPL` — DOS/SWOS-2020, little-endian, 3626-byte header, **one** whole-match
  scene (~1–3 MB), streamed from a dedicated buffer.
- `.IDX` — SWOS-2020 seek index (frame→offset) for FF/RW/slomo/speed.

**There are no key/event/audio markers in the stream.** The crowd toggle
(`currentKey == 0x89` → `TurnCrowdChantsOn`) is **generated internally at scene
transitions**, not recorded — so a one-scene `.RPL` never toggles the crowd on
at all (hence we force `TurnCrowdChantsOn()` at replay start for RPLs).

---

## 3. Why replays are (mostly) silent today — and what's correct

- **Crowd:** `PlayCrowdChants` (random crowd samples on channel 2, gated by
  `_crowdChantsOn`) is ticked every frame during playback, but it's only
  *enabled* by the `0x89` scene-transition key. RPLs have no scene transitions,
  so the crowd stays off unless forced on. *(Open: the crowd-chant sample uses
  the RJP1 wave-id trigger path; confirm on real hardware that an enabled chant
  is audible — match SFX use the note/preset path.)*
- **Action SFX (kick/bounce/whistle/cheer):** never existed in a replay, in any
  SWOS version, because the replay doesn't re-simulate. **This is by design**,
  not a regression — and it's the main thing v2 fixes by recording an explicit
  audio-event track.

---

## 4. Goals for v2

1. **Self-describing & versioned.** Magic + version + a TLV/chunked container so
   we can add features without breaking old readers, and detect & up-convert
   legacy `.HIL`/`.RPL` on load.
2. **Resolution-independent.** Record positions in **world/pitch coordinates**,
   not screen space. The camera is *derived at playback* from the ball (we
   already reconstruct ball position from the shadow sprite) or stored as an
   optional hint — never as a baked-in fixed-width pan.
3. **Full audio.** A timestamped **audio-event track**: every SFX the engine
   fired (kick, header, tackle, post, whistle, goal cheer, crowd-state changes),
   with the same ids the SFX bank uses, so playback re-triggers them exactly.
4. **First-class trick play.** Built-in **keyframe/seek index** (no side-car):
   fast-forward, rewind, slow-mo, variable speed, frame-step, jump-to-goal.
5. **Whole-match or clips.** Support both a full match and trimmed highlight
   clips, with chapter marks (goals, cards, half-time).
6. **Backward read.** Keep reading `.HIL`/`.RPL` (+ `.IDX`); offer "re-save as
   v2" to migrate.

---

## 5. Proposed structure (chunked container)

```
[ FILE HEADER ]
  magic   "SWRP"            ; SWos RePlay
  version u16               ; format version
  flags   u16               ; widescreen-source, has-audio, has-index, ...
  tickRate u16              ; recording tick rate (e.g. 50 or 70)
  quirks  u16               ; WHICH GAME this was recorded against — see below
  pitchW, pitchH u16        ; world dimensions (decouples from view width)
  teamsBlobOffset u32       ; -> team/kit/lineup metadata
  frameStreamOffset u32
  audioTrackOffset u32
  seekIndexOffset u32
  chapterTableOffset u32

[ METADATA chunk ]   teams, kits, score, competition, date, recorded-version
[ FRAME STREAM ]     per-tick: camera-hint?, ball(world x,y,z), sprites(world x,y, idx, dir)
[ AUDIO TRACK ]      sorted by tick: { tick, channel, sfxId, param } events
[ SEEK INDEX ]       keyframe every N ticks: { tick, frameOffset, audioOffset }
[ CHAPTER TABLE ]    { tick, kind(goal/card/half), team } for jump-to-goal
```

Key decisions:

- **World coordinates + z for the ball** → resolution-independent rendering and
  a proper ball-tracked camera at any width (z lets the shadow/height render
  correctly without guessing).
- **Audio as an event track**, not interleaved opcodes → trivial to mix, mute,
  re-time under slow-mo (pitch-preserve or not, our choice), and to *omit* on
  playback engines that don't want it.
- **Keyframes** carry both a frame offset *and* an audio offset so seeking lands
  the audio track in sync; between keyframes we replay deltas forward.
- **Sprites store direction** (already in the engine) so the camera/renderer can
  do smarter framing and we can re-derive facing without the bitmap.
- **`quirks` records which game the file was recorded against.** We now
  deliberately depart from the original in places (the first is the fire-hold
  lockout — an original-Amiga bug where an opponent's tackle wipes your held
  shot charge; `_gOriginalFireHoldLockout` in `team_control.c`, docs/GAMEPLAY.md
  §6.3). A recording made under the fix and one made under the original are
  recordings of *different games*, and only the file can say which. One bit per
  deliberate departure, room to grow.

  **Owner's call, 2026-09-13: new recordings are made with the fixes ACTIVE —
  fixed is the new norm.** So the bit is normally 0 and set only when something
  was deliberately recorded against original behaviour.

  This is the same shape as `tickRate`, and for the same reason. **Decide it per
  FILE, never per format.** That shortcut has already been paid for once here:
  the authored playback rate turned out to live in a per-file byte (SWOS-2020's
  `gameStyle` at offset 5) and the first implementation hardcoded it per format,
  which broke every reel that disagreed with its neighbours. `quirks` will fail
  exactly the same way if it is inferred from the extension.

---

## 6. Trick play (replaces `.IDX`)

- **FF / RW:** jump to nearest keyframe, replay deltas; audio either muted or
  time-compressed.
- **Slow-mo / speed-up:** scale the tick advance; the audio track is resampled
  or event-rate-scaled (design choice per SFX class — crowd loops stretch,
  one-shots fire at their real pitch).
- **Frame-step / jump-to-goal:** keyframes + chapter table.

---

## 7. Migration / compatibility

- Loader sniffs magic: `SWRP` → v2; legacy 3626-byte LE header → `.RPL`; Amiga
  `.HIL` → as today. `.IDX` still read if present alongside a legacy `.RPL`.
- A "Re-save as v2" path can up-convert a legacy file (positions only; audio
  track empty/crowd-only since the source never had it).
- **Legacy files have no `quirks` field, so up-conversion must set it to
  "original", not 0.** Every `.HIL` / `.RPL` / `.RGD` that exists today predates
  the deliberate departures, and a missing field is not the same as a field
  saying "fixed". Same rule anywhere else the flag is read: absent ⇒ original.
  (Today `--replay-test` hardcodes exactly that for the `.RGD` corpus — see the
  `opts.fire_hold_lockout` line in `main.c`, which carries a pointer back here
  and stops being correct the moment a v2 recording reaches the rig.)
- v2 is authored by a **recorder** that taps the live engine: it already has the
  ball physics + every `PlaySample`/SFX trigger, so populating the audio track is
  just logging those at record time.

---

## 8. Implementation phases

1. **Recorder hooks** — tap world positions + every SFX trigger during a live
   match into in-memory v2 buffers (no playback yet). Verifies we *can* capture
   everything.
2. **Writer** — serialize the chunked container + seek index.
3. **Reader/player** — play frame stream (reuse the existing render path with
   world→screen at playback width) + drive audio from the event track.
4. **Trick play** — wire FF/RW/slomo/speed/seek to the built-in index.
5. **Migration** — sniff + up-convert legacy `.HIL`/`.RPL`.

---

## 9. Open questions

- Crowd: keep the chant state-machine as the ambient bed, or record an explicit
  crowd-intensity envelope in the audio track? (Latter gives goal-roar swells.)
- Audio under slow-mo: pitch-shift vs event-rescale per SFX class.
- File size vs delta-compression of sprite positions (most move a few px/tick).
- `quirks`: one bit per deliberate departure, or a single "game revision" ordinal
  the reader maps to a set? Bits are simpler now; an ordinal ages better once
  there are several and they interact. Either way it is per file, never per
  format, and absent means original.
- Do we record inputs too (enabling true re-simulation as an alternative to
  position playback)? Re-sim would give *perfect* SFX but risks desync across
  engine changes; position playback is robust. Likely: positions + audio track
  (robust), inputs optional/experimental.
