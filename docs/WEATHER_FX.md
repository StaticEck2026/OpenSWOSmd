# Weather effects

SWOS already has weather — it just never shows you any. The pitch arrives frozen,
muddy, wet, soft, normal, dry or hard (`_pitchNumberAndType`), the ball behaves
differently on each, and the only visible sign is that the grass is a different
colour. This is what it would take to see it.

Everything here is **decoration**, and that word is load-bearing: see §1.

---

## 1. The rule everything obeys

The crowd taught us this one (`docs/CROWD_ANIMATION.md` §7): it *looks* like
decoration and it is actually simulation, because picking a cell consumes
`Rand()` and its state is in the rollback registry and in every recording.

So, for every effect below:

> **VIEW tier. Never `Rand()`, never written back, never read by anything the
> simulation reads.** An effect that needs randomness gets its own generator,
> seeded per match from the match seed and advanced only by the effect system.
> Timing is in **frames**, never milliseconds, so a replay at frame N draws what
> the live match drew at frame N.

Break that and the symptom is not "the rain looks wrong" — it is two netplay
peers desynchronising at the first raindrop, and every existing `.SRP` replaying
differently.

The one effect that could legitimately cross the line is **mud affecting the
ball** (§5). That is a gameplay change, it belongs in the simulation with a
registry entry and a netplay handshake bit, and it is deliberately out of scope
here.

## 2. What the renderer can do today, and what it cannot

| | today | needs |
|---|---|---|
| palette flash (lightning) | **yes** — rewrite entries for a frame or two | nothing |
| colour cycling (rain on glass, water) | **yes** — one palette write per frame | nothing |
| sprite-shaped particles (rain, snow) | **yes** — they are sprites | a particle list |
| **alpha-blended overlay** (clouds) | **no** | `docs/SHADERS.md`, or a blend LUT |
| drawing *over* the players | **no** | the over-sprite pass, `PITCH_FORMAT_V2` §5 |

The pipeline is 8bpp indexed end to end, so "50% of this colour over that one"
has no answer without either a 64 KB blend LUT per palette pair or the GPU path.
That is the single gate between the cheap effects and the good ones.

> Truecolour pitch layers would have made the alpha problem disappear
> (`PALETTE_V2` §8), but that is parked. Alpha comes from the GPU path.

## 3. Lightning and thunder — cheapest, ship first

The whole effect is a palette write and a sound, and it works on **every target
including the Amiga**, which is exactly the trick the Amiga demos used.

- **The flash**: for 2–3 frames, push every entry in the background bank toward
  white (or toward a cold blue-white, which reads better on grass). One pass over
  256 entries, no per-pixel cost at all. A double flicker — 2 frames on, 3 off,
  2 on — reads far more like lightning than a single ramp.
- **The thunder**: the sound follows the flash by a delay, because that is what
  thunder does. Distance in frames: near strikes 10–20, far ones 100+. Vary it
  and the storm acquires a geography for free.
- **When**: on a `wet` pitch, at intervals from the effect RNG. The interval is
  the only random thing and it is drawn once per strike.

The palette flash must apply to the background bank *and* the sprite bank
(`PALETTE_V2` §2), or the players stay lit while the world flashes.

## 4. Rain and snow — particles, as sprites

Rain is a sprite that falls. There is no new rendering concept needed, only a
list.

- **The list**: a fixed-size array of particles (x, y in 16.16, velocity,
  lifetime), stepped once per frame, no allocation. Fixed size means a fixed
  cost and no surprises mid-match.
- **The draw**: a 1×3 or 1×4 pixel streak for rain, a 2×2 dot for snow, drawn
  through the existing sprite path. Rain wants to be drawn *over* the players
  (it is between them and the camera), which needs the over-sprite pass; until
  that exists it draws under them and still reads.
- **Density from the pitch**: `wet` gives heavy rain, `soft` a drizzle,
  `frozen` gives snow, everything else gives nothing. One table, indexed by the
  pitch type the match already has.
- **Wind**: a horizontal component that drifts over the match, shared with the
  cloud layer so they agree. Worth it — vertical rain looks like a screensaver.
- **Landing**: a snow particle that reaches the ground can leave its pixel in a
  fading accumulation buffer. This is where "cheap" ends; see §5.

Scale matters more than count. A few hundred particles at the right size reads as
weather; a few thousand reads as static.

## 5. Mud, puddles and trails

The wanted effect: a muddy pitch that *shows* the game being played on it —
studs churning, a ball leaving a track through a puddle.

**The honest version of this is a second surface**, not the tile map. Writing
trails into the pitch tiles themselves would be quick and wrong three times over:
the tile map is in the rollback registry, the crowd animation writes it too, and
a rolled-back frame would leave footprints from a future that no longer happened.

So: a **trail layer** — one byte per pitch cell (or per half-cell) of "how
churned is this", written by players and the ball as they pass, decaying slowly.
It is VIEW tier, it is not in the registry, and a rollback simply leaves it
slightly wrong for a second, which nobody can see.

- **Drawing it**: pick the tile variant for that churn level, or in the GPU path
  darken the pitch there. Without alpha it is a tile swap, so a muddy pitch needs
  two or three churn variants of its grass tiles — which `PITCH_FORMAT_V2`'s
  layers already express.
- **Puddles** are authored, not simulated: a muddy pitch's layer marks where
  water sits, and the trail layer makes splashes when something crosses one.
- **The ball's track** is the same mechanism with a narrower brush.

This is the most expensive effect here in both work and art, and the most
distinctive if done. It wants the GPU path.

## 6. Clouds

An alpha-blended layer drifting over pitch *and* players — `PITCH_FORMAT_V2`'s
`shift` track on a layer with positive `swos:z`, which is already specified. It
needs the two things the renderer does not have yet (alpha, over-sprite), so it
is gated behind the GPU path, and it is the single best advertisement for it: a
stadium with cloud shadows crossing the pitch is the frame that sells the work.

Shared wind vector with the rain (§4), or the storm looks assembled from parts.

## 7. Order

| | effect | gate |
|---|---|---|
| 1 | Lightning + thunder | none — palette and sound, works everywhere |
| 2 | Colour cycling for rain-on-water, puddle shimmer | none — `PITCH_FORMAT_V2` §2.3 |
| 3 | Rain and snow particles | a particle list; better with the over-sprite pass |
| 4 | Trail layer and puddles | churn tile variants, or the GPU path |
| 5 | Clouds | alpha + over-sprite, i.e. `docs/SHADERS.md` |

The first two are a day's work and need no rendering changes at all, which is
the right place to start: a thunderstorm over a wet pitch, made entirely of a
palette and a `.wav`.

## 8. Settings

Weather is atmosphere, and atmosphere is taste. Everything here is off by
default until it has been played with:

```ini
[weather]
enabled   = false    ; master switch
particles = true     ; rain and snow
lightning = true     ; flash and thunder on a wet pitch
clouds    = false    ; needs the GPU path
trails    = false    ; churn and puddles on a muddy pitch
intensity = 100      ; 0..200, scales density and frequency
```

And one hard rule for all of them: **a weather setting must never change the
simulation**, so two netplay peers with different weather settings still play the
same match. That is only true while §1 holds.

## 9. Related

- `docs/CROWD_ANIMATION.md` — why "it is only decoration" is a claim that has to
  be checked rather than assumed.
- `docs/PITCH_FORMAT_V2.md` — layers, the over-sprite pass, colour cycling, and
  the animation system these effects would be authored in.
- `docs/PALETTE_V2.md` — the two banks a palette flash has to touch.
- `docs/SHADERS.md` — where alpha comes from.
