# SWOS (Amiga) — Gameplay Specification

A complete, implementation-level description of the **in-match gameplay** of
Sensible World of Soccer (Amiga). It covers the world model, player movement,
camera, ball physics, possession, passing/shooting, heading, tackling, fouls,
cards, injuries, goalkeeper behaviour, computer AI, tactical off-ball
positioning, set pieces, the referee, the match clock and the statistics — with
every constant table the engine reads.

**Rendering is deliberately out of scope.** Only the two places where rendering
feeds *back* into gameplay are specified (`Sprite.onScreen` and the sprite
`imageIndex`/animation-frame walker), because the simulation reads them.

Everything here is derived from the reversed Amiga original
(`SWOS2.s` → this C port). Where the DOS release differs, the DOS value is given
alongside; the Amiga value is the reference.

> **Document version 2.0** — 2026-09-30. Five rounds of independent
> re-verification against the port sources (the last two including blind
> implementation tests), then a sixth check of a different kind: replaying
> recorded DOS matches against the port frame by frame (docs/TESTS.md). That
> pass corrected three DOS values here (1.8) — and, as the rig kept going, a
> run of port defects that this document had faithfully copied (2.0: goalkeeper
> diving-contact thresholds, nearest-player tie order, three AI fire gates, the
> goalpost arms, the Y-quadrant leading bound, and more).
> See [Revision history](#revision-history) before editing.

---

## Table of contents

1. [World model and units](#1-world-model-and-units) — incl. §1.3a global lifecycles (set / stepped / cleared)
2. [Core data structures](#2-core-data-structures)
3. [Fixed-point math and the direction system](#3-fixed-point-math-and-the-direction-system)
4. [Random number generators](#4-random-number-generators)
5. [The frame loop](#5-the-frame-loop)
6. [Input and the control model](#6-input-and-the-control-model)
7. [Player selection (who you control)](#7-player-selection-who-you-control)
8. [UpdatePlayers — the per-player state machine](#8-updateplayers--the-per-player-state-machine) — incl. §8.10a the bench and the substitution decision
9. [Player movement and animation](#9-player-movement-and-animation) — incl. §9.4 which animations gameplay knows by name
10. [Ball physics](#10-ball-physics)
11. [Ball prediction](#11-ball-prediction)
12. [Possession, passing, shooting, aftertouch](#12-possession-passing-shooting-aftertouch)
13. [Heading](#13-heading)
14. [Tackling, fouls, cards, injuries](#14-tackling-fouls-cards-injuries) — incl. §14.2a the restart a foul produces · §14.5 trainingGame
15. [Goalkeeper](#15-goalkeeper) — incl. §15.7 a worked example, one frame end to end
16. [Computer AI](#16-computer-ai)
17. [Off-ball positioning (tactics)](#17-off-ball-positioning-tactics)
18. [Ball out of play and restarts](#18-ball-out-of-play-and-restarts)
19. [The referee](#19-the-referee)
20. [Camera](#20-camera)
21. [Match clock, halves, extra time, penalties](#21-match-clock-halves-extra-time-penalties) — incl. §21.4a the stopped-state dispatcher
22. [Statistics](#22-statistics)
23. [Match initialisation order](#23-match-initialisation-order)
24. [Complete constant tables](#24-complete-constant-tables) — incl. §24.2a aftertouch spin tables, §24.12 where the rest of the data lives
25. [Porting checklist and known traps](#25-porting-checklist-and-known-traps)

— [Revision history](#revision-history) · [Credits](#credits)

---

## 1. World model and units

### 1.1 The pitch

The world is a fixed 2-D plane in *game units* (≈ pixels at the original
display scale). It does not scale with the display.

```
world width      = 672      (0x2A0)
world height     = 880      (0x370)
```

Play area and landmarks (all values are exact and load-bearing — they appear as
literals in the collision and restart code):

| Feature                         | Coordinates                                   |
|---------------------------------|-----------------------------------------------|
| Left touchline                  | `x = 81`                                      |
| Right touchline                 | `x = 590`                                     |
| Top goal line                   | `y = 129`                                     |
| Bottom goal line                | `y = 769`                                     |
| Centre spot / halfway line      | `(336, 449)`                                  |
| Goal mouth (between the posts)  | `x ∈ [302, 366]`                              |
| Post band (outer)               | `x ∈ [296, 372]`                              |
| Crossbar height                 | `z = 15` (ball above 15 hits the bar)         |
| Top of net                      | `z = 19`                                      |
| Top goal net depth              | `y ∈ [113, 128]`, back net at `y < 119`       |
| Bottom goal net depth           | `y ∈ [770, 784]`, back net at `y > 778`       |
| Penalty area X span             | `x ∈ [193, 478]`                              |
| Top penalty area                | `y ≤ 216`                                     |
| Bottom penalty area             | `y ≥ 682`                                     |
| Goal (6-yard) area X span       | `x ∈ [273, 398]`                              |
| Top goal area                   | `y ≤ 158`                                     |
| Bottom goal area                | `y ≥ 740`                                     |
| Top penalty spot                | `(336, 187)`                                  |
| Bottom penalty spot             | `(336, 711)`                                  |
| Top-left corner spot            | `(86, 134)`                                   |
| Top-right corner spot           | `(585, 134)`                                  |
| Bottom-left corner spot         | `(86, 764)`                                   |
| Bottom-right corner spot        | `(585, 764)`                                  |
| Top goal-kick spots             | `(276, 154)` / `(396, 154)`                   |
| Bottom goal-kick spots          | `(276, 744)` / `(396, 744)`                   |
| Throw-in X (left / right)       | `81` / `590`                                  |
| Off-pitch "sent off" park spot  | `(-20, 449)`                                  |
| Kick-off ball park (pre-match)  | `(1672, 449)` — deliberately off-world        |

The camera view is `336 × 272` units. See §20.

**Y increases downward.** The team defending the *top* goal is
`topTeamData`; the team defending the *bottom* goal is `bottomTeamData`.
`topTeamData` attacks downward (+Y), `bottomTeamData` attacks upward (−Y).
This is independent of which side is "team 1"; `teamPlayingUp` selects that and
flips at half time.

### 1.2 Heights

`z` is the ball's (or a sprite's) height above the pitch, in the same units.
`z = 0` is the ground. Meaningful thresholds:

```
z <= 4     "ballLessEqual4"   ground / dribbling height
z 5..8     "ball4To8"         low
z 9..12    "ball8To12"        header height (low)
z 13..17   "ball12To17"       header height (high)
z > 17     "ballAbove17"      too high to play with feet
z 8..15    the window in which a header actually connects
z <= 15    passes under the crossbar
z <= 19    still inside the goal frame
```

### 1.3 Time

The simulation is a **fixed-step 50 Hz loop** (PAL VBlank). Every constant
below (speeds, timers, gravity) is *per frame*, not per second.

`lastFrameTicks` is the number of 50 Hz ticks elapsed since the previous frame;
under normal conditions it is `1`. Every timer that uses it must accumulate it
rather than assuming 1, so a dropped frame doesn't stall them. The full set is
`stoppageTimerTotal`, `stoppageTimerActive`, `inGameCounter`,
`stoppageEventTimer`, `resultTimer`, `endGameCounter` and `refTimer` — see §1.3a
for each one's arm/step/clear sites.

The DOS release runs at 70 Hz and retunes a set of constants (§24.10). A port
that targets a different frame rate should either keep 50 Hz for the simulation
and interpolate for display, or apply the same style of retune.

**The two stoppage timers.** Both are zeroed by every restart
(`writeRestartGlobalsAndStopPlayers`, §18.1) and both are read in several
sections, but they accumulate under different conditions and are not
interchangeable:

* `stoppageTimerTotal += lastFrameTicks` on **every** frame the game is not in
  progress. Read by §9.1 (the wind-down/wind-up speed scaling) and §16.2 (the
  CPU-vs-CPU result dismissal).
* `stoppageTimerActive += lastFrameTicks` **only** while
  `gameStatePl == ST_WAITING_ON_PLAYER` — i.e. only once the restart is armed and
  the engine is waiting for someone to take it. Read by §8.3-A (the CPU's
  50-tick wake-up) and §16.2 (the AI's randomised restart wait), and compared
  against `gsKickoffStoppage` (750) for the CPU auto-restart (§18.3).

`inGameCounter` is the mirror of those two for **live** play:
`inGameCounter += lastFrameTicks` on every frame `gameStatePl ==
ST_GAME_IN_PROGRESS` (it is the early-return arm at the top of the same
dispatcher), and it is zeroed by `InitTeamsData` and by §18.3 states 2 and 8. It
is read by §8.8's post-restart freeze gate — which means that gate only holds for
the first 25 frames *after* the hand-over completes. Omit the increment and the
gate is permanently satisfied, so no off-ball player ever repositions again after
the first corner or free kick.

`showFansCounter`, the highest-priority camera state (§20.1), is a third such
counter: it is set to `100` at match start for non-training games, cleared when
`ST_STARTING_GAME` is dismissed, and decremented once per frame by
`DrawAnimatedPatterns` (§5).

### 1.3a Global lifecycles

The single most common way to mis-port this engine is to implement a global's
*readers* and miss one of its *writers* — the symptom looks like your own bug, not
like missing documentation. Every load-bearing shared global, with all three
phases:

| Global | Set / armed | Stepped | Cleared |
|--------|-------------|---------|---------|
| `stoppageTimerTotal` | — | `+= lastFrameTicks` every frame not in progress | every restart (§18.1) |
| `stoppageTimerActive` | — | `+= lastFrameTicks` only while `ST_WAITING_ON_PLAYER` | every restart (§18.1) |
| `inGameCounter` | — | `+= lastFrameTicks` every frame **in** progress | §18.3 states 2 and 8; `InitTeamsData` |
| `stoppageEventTimer` | all 10 transition functions (§21.4) + the §18.3 state-0 reload | `-= lastFrameTicks` at the top of the timer dispatcher | on expiry, and by the end-game handler |
| `ballInPlay` / `ballOutOfPlay` | §8.3-C/D, §18.3 state 7 | — | **`StopAllPlayers()`** (§18.1) — the easy one to miss |
| `controlledPlayer` / `passToPlayerPtr` | §7.1 / §7.2 each team update | — | `StopAllPlayers()` (§18.1) |
| `playerSwitchTimer` | 25 on any auto hand-over | `--` per team update | `StopAllPlayers()` |
| `spinTimer` | 0 by `DoPass`/`PlayerKickingBall` | `++` per team update in §12.5 | `-1` at 10, or `ResetBothTeamSpinTimers()` |
| `wonTheBallTimer` | 12 / 8 (§12.4) | `--` per team update | on expiry |
| `passKickTimer` | 25 on any release | `--` per team update | clears `passingKickingPlayer` at 0 |
| `runSlower` | 1 by `GoalScored` | — | §18.3 state 3; `InitTeamsData` |
| `goalScored` | 1 by `GoalScored` | — | §18.3 state 2 |
| `whichCard` / `bookedPlayer` | the card routines (§14.4) | — | §18.3 state 5; end of the referee sequence (§19) |
| `showFansCounter` | 100 at kick-off (non-training) | `--` per frame by the pattern code | when `ST_STARTING_GAME` is dismissed |
| `breakState` | §18.3 states 2 and 8 | — | `InitTeamsData` only (§23) — so within a match it holds the *previous* restart's type during the current one |
| `ballOutOfGameTimer` | 0 in §18.3 state 6 | `++` in state 7 | anti-stall fires at 500 |
| `gameSeconds` | **`-1` on the first pass of each of the four boundary minutes (§21.2)** | `++` per in-game second; `= 0` on a minute rollover | **`= 0` on the SECOND pass of a boundary arm — the only writer that clears the `-1`**; `ResetTime` |
| `endGameCounter` | `gsEndGameCounter` (50) alongside every `gameSeconds = -1`, and **re-armed on every "keep waiting" exit** of §21.2 | `-= lastFrameTicks` while `gameSeconds < 0` | never explicitly — it simply stops being read once `gameSeconds >= 0` |
| `secondsSwitchAccumulator` | — | `-= timeDelta` per live frame; `+= gsFramesPerGameSecond` when it goes negative | `ResetTime` |
| `refState` | `REF_INCOMING` when a foul arms the referee; `REF_WAITING_PLAYER`, `REF_ABOUT_TO_GIVE_CARD` (§8.3 booking path), `REF_BOOKING`, `REF_LEAVING` as the sequence runs (§19) | — | `0` at the end of the sequence; `RemoveReferee()` from `InitPlayersBeforeEnteringPitch` |
| `refTimer` | `0` by both card routines (§14.4) | `+= lastFrameTicks`, then `>> 3` to index the card animation table | — (re-armed rather than cleared) |
| `aiCounter` | `15` when `AI_DecideWhetherToTriggerFire` arms a shot (§16.4) | `--` once per team update, guarded `!= 0` | on reaching 0 |
| `aiResumePlayTimer` | `15` on **every** AI release path (§16.2/§16.4/§16.5) | `--` once per team update, guarded `!= 0` | on reaching 0 |
| `aiTurnDirection` | `+1` at init; flipped to `∓1` only when `playerTurnFlags` blocks the step | `±1` per gated tick while outside `{-1,+1}` | — (a shared global, never reset per match) |
| `TeamGeneralInfo.field_84` | `4` when the AI arms its release cooldown (§16.5) | `--` on each "no fire" fallback that sees it non-zero | `0` on the immediate-turn path |
| `isGoalAttempt` | `1` by the §22 arm test | — | the §22 clear test; unconditionally whenever `gameStatePl != ST_GAME_IN_PROGRESS`; `ResetGoalAttempt()` at match init (§23) |

### 1.4 Speed units

`Sprite.speed` is a 16-bit scalar. It is converted to a per-frame displacement
by `CalculateDeltaXAndY` (§3):

```
deltaX (16.16) = (cos_q8 * speed)      where cos_q8 ∈ [-128, 127]
deltaY (16.16) = (sin_q8 * speed)
```

so a unit of `speed` is worth `127/65536 ≈ 0.00194` units/frame along the axis
of travel; **`speed / 516` ≈ units per frame**, i.e. `speed / 10.3` ≈ units per
second. A fast outfielder (`speed = 1250`) covers ≈ 2.42 units/frame ≈ 121
units/second, crossing the 640-unit-long pitch in ≈ 5.3 s.

### 1.5 Fixed point

Positions and velocities are **16.16 signed fixed point** stored big-endian on
the 68000 (integer part in the high word):

```c
typedef union {
    int32_t raw;                 /* whole 16.16 value                        */
    struct { int16_t i; uint16_t frac; } s;   /* BE: i is the high word      */
} FixedPoint;
```

Almost all gameplay reads `.s.i` (the integer part) and almost all integration
writes `.raw`. A port on a little-endian host must lay the struct out so that
`.s.i` still aliases the **high** half of `.raw` (see the `AMIGA_FIXEDPOINT_LE`
switch in `include/game_types.h`). Getting this wrong silently reads the
fractional half as the position and nothing works.

---

## 2. Core data structures

Sizes are the original 68000 (32-bit pointer) sizes. Offsets matter only if you
intend to load original save data; a fresh port may reorder freely.

### 2.1 `Sprite` — one moving object (110 bytes)

Used for all 22 players, the ball, the ball shadow, both goalkeepers, the
referee, the goals and every HUD element. Gameplay-relevant fields:

```c
struct Sprite {
    int16_t    teamNumber;          /* 1 or 2; 0 for non-player objects       */
    int16_t    playerOrdinal;       /* 1..11; 1 == goalkeeper                 */
    int16_t    frameOffset;         /* added to every animation frame index   */
    AnimTable *animationTable;      /* current animation set                  */
    int16_t    startingDirection;   /* direction when the anim was selected   */
    uint8_t    playerState;         /* PL_* (§2.5)                            */
    int8_t     playerDownTimer;     /* frames left in the current state       */
    int16_t   *frameIndicesTable;   /* current per-direction frame list       */
    int16_t    frameIndex;          /* cursor into frameIndicesTable          */
    int16_t    frameDelay;          /* frames per animation step              */
    int16_t    cycleFramesTimer;    /* countdown to next animation step       */
    int16_t    frameSwitchCounter;  /* frames emitted since the anim started  */
    FixedPoint x, y, z;             /* world position                         */
    int16_t    direction;           /* 0..7 (§3.3)                            */
    int16_t    speed;               /* scalar speed (§1.4)                    */
    FixedPoint deltaX, deltaY, deltaZ;  /* per-frame velocity                 */
    int16_t    destX, destY;        /* movement target                        */
    int16_t    visible;             /* draw enable                            */
    int16_t    imageIndex;          /* current sprite id; -1 = hidden         */
    int32_t    ballDistance;        /* SQUARED distance to the ball           */
    int16_t    fullDirection;       /* 0..255 angle from the BALL TO ME (§3.3)*/
    int16_t    onScreen;            /* set by the renderer; read by gameplay  */
    int16_t    playerDirection;     /* direction at the start of this frame   */
    int16_t    isMoving;            /* -1 if it had a delta last frame        */
    int16_t    tackleState;         /* 0 clean, 1 tackle, 2 "good tackle"     */
    int16_t    heading;             /* 1 once a header has connected          */
    int16_t    destReachedState;    /* 0..3 restart-formation handshake       */
    int16_t    cards;               /* 0,1,2; -1 once sent off                */
    int16_t    injuryLevel;         /* frames to lie injured; 0 = none. Only ever
                                     * WRITTEN as 0 or a kInjuryRecoveryTime
                                     * value. Two comparisons against -1 (§8.2)
                                     * and -2 (§8.10) exist in the engine but
                                     * nothing ever writes those values, so both
                                     * branches are dead. Keep the field signed
                                     * so the comparisons behave, but do not
                                     * expect them to fire.                    */
    int16_t    tacklingTimer;       /* >=0 human held fire; -1 CPU/released   */
    int16_t    sentAway;            /* 1 = red-carded, off the pitch          */
};
```

`ballDistance` is **squared** Euclidean distance and every comparison against it
in the engine is against a squared constant (e.g. `<= 32`, `<= 72`, `<= 2450`,
`<= 648`, `>= 512`, `> 5000`). Never take a square root.

### 2.2 `TeamGeneralInfo` — per-team runtime state (144 bytes)

One instance per side (`topTeamData`, `bottomTeamData`). This is where all the
control and possession state lives.

```c
struct TeamGeneralInfo {
    TeamGeneralInfo *opponentsTeam;
    int16_t   playerNumber;         /* 0 = CPU, 1 = joystick 1, 2 = joystick 2 */
    int16_t   playerCoachNumber;    /* 0 = none, else which human coaches it   */
    int16_t   isPlCoach;
    TeamGame *inGameTeamPtr;        /* squad data (§2.3)                       */
    TeamStatisticsData *teamStatsPtr;
    int16_t   teamNumber;           /* 1 or 2                                  */
    Sprite  **spritesTable;         /* 11 player sprites; [0] is the keeper    */
    GoalieSkill *shotChanceTable;   /* keeper behaviour table (§15.1)          */
    int16_t   tactics;              /* index into teamTacticsTable             */
    int16_t   updatePlayerIndex;    /* round-robin cursor, 0..10               */
    Sprite   *controlledPlayer;     /* the player the human/AI steers          */
    Sprite   *passToPlayerPtr;      /* the designated pass receiver            */
    int16_t   playerHasBall;        /* set this frame if in contact            */
    int16_t   allowedDirections;    /* previous frame's playerHasBall          */
    int16_t   currentAllowedDirection; /* input direction 0..7, -1 = centred   */
    int16_t   direction;            /* raw input direction                     */
    int8_t    quickFire;            /* fire tap  (<= 4 frames)                 */
    int8_t    normalFire;           /* fire hold (>  4 frames)                 */
    int8_t    firePressed;          /* fire is down right now                  */
    int8_t    fireThisFrame;        /* fire went down this frame               */
    int16_t   headerOrTackle;       /* set when a tackle/header was launched   */
    int16_t   fireCounter;
    int16_t   controlledPlDirection;/* direction the last action was aimed     */
    int16_t   shooting;
    /* ---- ball proximity classification, recomputed per player ---- */
    /* mutually exclusive proximity BANDS — at most one is ever set (§8.2) */
    int8_t    plVeryCloseToBall;    /*        ballDistance <= 32               */
    int8_t    plCloseToBall;        /*   32 < ballDistance <= 72               */
    int8_t    plNotFarFromBall;     /*   72 < ballDistance <= 2450             */
    int8_t    ballLessEqual4, ball4To8, ball8To12, ball12To17, ballAbove17;
    int8_t    prevPlVeryCloseToBall;
    Sprite   *lastHeadingTacklingPlayer;
    int16_t   goalkeeperSavedCommentTimer;   /* >0 saved, <0 beaten            */
    int16_t   goalkeeperDivingRight, goalkeeperDivingLeft;
    int16_t   ballOutOfPlayOrKeeper, goaliePlayingOrOut;
    int16_t   passingBall, passingToPlayer;
    int16_t   playerSwitchTimer;    /* lock-out after an auto player switch    */
    int16_t   ballInPlay, ballOutOfPlay;
    int16_t   ballX, ballY;         /* where the receiver should run to        */
    int16_t   passKickTimer;        /* 25-frame lock after a pass/shot         */
    Sprite   *passingKickingPlayer; /* who just released the ball              */
    int16_t   ballControlTurnTimer; /* frames spent turning while dribbling    */
    int16_t   ballCanBeControlled;
    int16_t   ballControllingPlayerDirection;
    int16_t   spinTimer;            /* 0..9 aftertouch window; -1 = disabled   */
    int16_t   leftSpin, rightSpin, longPass, longSpinPass;
    int16_t   passInProgress;       /* 1 = last release was a pass, 0 = shot   */
    int16_t   AI_timer;
    int16_t   AI_afterTouchStrength;  /* 0..2                                  */
    int16_t   AI_ballSpinDirection;   /* -1, 0, +1                             */
    int16_t   wonTheBallTimer;      /* possession-contest lock-out             */
    int16_t   field_46;             /* keeper "already intervened this dive"
                                       latch — set as 0x0100, cleared &= 0xFF (§15.5) */
    int16_t   field_84;             /* AI 4-tick decision cooldown (§16.5)     */
    int16_t   goalkeeperPlaying;    /* keeper is acting as an outfielder       */
    int16_t   resetControls;
};
```

### 2.3 `TeamGame` / `PlayerInfo` — the squad in the match (1018 / 61 bytes)

```c
struct PlayerInfo {              /* one squad member, 61 bytes                 */
    int8_t skip;                 /* non-zero = substituted out                 */
    int8_t playerIndex;          /* index back into the team file              */
    int8_t goalsScored;
    int8_t shirtNumber;
    int8_t position;             /* 0 GK, 1 RB, 2 LB, 3 DEF, 4 RW, 5 LW,
                                    6 MID, 7 ATT — see §8.10a for the two
                                    positional GROUPS the engine derives     */
    int8_t face;
    int8_t isInjured;
    int8_t field_7, field_8;
    int8_t previousCards;
    int8_t numYellowCards;       /* card state, 0..4 (see §14.4)               */
    int8_t field_B;
    int8_t shortName[15];
    /* ---- the seven skills, each 0..7 ---- */
    int8_t passing, shooting, heading, tackling, ballControl, speed, finishing;
    int8_t goalieSkill;          /* 0..7, only meaningful for position 0       */
    int8_t injuriesBitfield;     /* top 3 bits = injury severity 0x20..0xE0    */
    int8_t halfPlayed;
    int8_t face2;
    int8_t fullName[23];
};

struct TeamGame {                /* 1018 bytes                                  */
    int16_t   prShirtType, prShirtCol, prStripesCol, prShortsCol, prSocksCol;
    int16_t   secShirtType, secShirtCol, secStripesCol, secShortsCol, secSocksCol;
    int16_t   markedPlayer;      /* index of the "marked" player, -1 = none    */
    int8_t    teamName[17];
    int8_t    _pad;
    int8_t    numOwnGoals;
    int8_t    _pad2;
    PlayerInfo players[16];      /* [0..10] on the pitch, [11..15] on the bench*/
};
```

**All seven skills are 0..7** and are used as *direct table indices* everywhere.
There is no scaling, no normalisation, and no clamping at the use site — a value
above 7 reads past the end of an 8-entry table. The one place the engine masks
is the goalkeeper table lookup (`goalieSkill & 7`, §15.1); everywhere else you
must guarantee the range when you build `PlayerInfo`.

The `players[]` array is indexed by `playerOrdinal - 1`. Byte offsets are also
reachable through:

```
ingameTeamPlayerOffsets[16] = { 0, 61, 122, 183, 244, 305, 366, 427,
                                488, 549, 610, 671, 732, 793, 854, 915 }
```

which is just `i * sizeof(PlayerInfo)`; the original addressed players by this
table plus the 42-byte `TeamGame` header.

### 2.4 `GoalieSkill` — keeper behaviour table (60 bytes / 30 words)

There are eight of these (one per `goalieSkill` 0..7), plus a ninth used when an
outfield player is standing in as keeper, and a tenth (`penaltyGoalieSkillTable`)
read from exactly one site during penalties — see §24.7. Only these fields are
read:

| Word | Field       | Meaning                                                        |
|------|-------------|----------------------------------------------------------------|
| 0    | `field_0`   | unused by gameplay (see `field_2A`)                             |
| 1    | `field_2`   | unused by gameplay (see `field_2C`)                             |
| 2    | `field_4`   | unused by gameplay                                              |
| 3    | `speed_unk` | keeper positioning speed                                        |
| 4    | `speed_unk2`| keeper "no save attempted" run speed                            |
| 5-20 | `index[16]` | dive-reach index (0..7) sampled by `(tick & 0x3C) >> 2`          |
| 21   | `field_2A`  | "try the clean catch" tick threshold on diving contact (§15.5)  |
| 22   | `field_2C`  | controlled-vs-scattered parry band on the same path (§15.5)     |
| 23   | `field_2E`  | unused by gameplay                                              |
| 24   | `field_30`  | catch-vs-parry threshold in `PL_GOALIE_CATCHING_BALL` (§15.4)    |
| 25   | `field_32`  | unused by gameplay                                              |
| 26   | `field_34`  | strong-vs-medium deflection threshold                           |
| 27   | `field_36`  | medium-vs-weak deflection threshold                             |
| 28   | `field_38`  | unused by gameplay                                              |
| 29   | `field_3A`  | "may attempt a catch this frame" tick threshold (0..15)         |

Full data in §24.7.

### 2.5 State enumerations

```c
/* Sprite.playerState */
#define PL_NORMAL               0
#define PL_TACKLING             1
#define PL_TACKLED              3
#define PL_GOALIE_CATCHING_BALL 4
#define PL_THROW_IN             5
#define PL_GOALIE_DIVING_HIGH   6
#define PL_GOALIE_DIVING_LOW    7
#define PL_STATIC_HEADING       8
#define PL_JUMP_HEADING         9
#define PL_DOWN                 10
#define PL_GOALIE_CLAIMED       11
#define PL_BOOKED               12
#define PL_ROLLING_INJURED      13
#define PL_SAD                  14
#define PL_HAPPY                15

/* gameState — what is happening / what restart is pending */
#define ST_PLAYERS_TO_INITIAL_POSITIONS 0    /* after a goal                   */
#define ST_GOAL_OUT_LEFT                1    /* goal kick                      */
#define ST_GOAL_OUT_RIGHT               2    /* goal kick                      */
#define ST_KEEPER_HOLDS_BALL            3
#define ST_CORNER_LEFT                  4
#define ST_CORNER_RIGHT                 5
#define ST_FREE_KICK_LEFT1              6
#define ST_FREE_KICK_LEFT2              7
#define ST_FREE_KICK_LEFT3              8
#define ST_FREE_KICK_CENTER             9
#define ST_FREE_KICK_RIGHT1             10
#define ST_FREE_KICK_RIGHT2             11
#define ST_FREE_KICK_RIGHT3             12
#define ST_FOUL                         13   /* free kick outside the FK band  */
#define ST_PENALTY                      14
#define ST_THROW_IN_FORWARD_RIGHT       15
#define ST_THROW_IN_CENTER_RIGHT        16
#define ST_THROW_IN_BACK_RIGHT          17
#define ST_THROW_IN_FORWARD_LEFT        18
#define ST_THROW_IN_CENTER_LEFT         19
#define ST_THROW_IN_BACK_LEFT           20
#define ST_STARTING_GAME                21
#define ST_CAMERA_GOING_TO_SHOWERS      22
#define ST_GOING_TO_HALFTIME            23
#define ST_PLAYERS_GOING_TO_SHOWER      24
#define ST_RESULT_ON_HALFTIME           25
#define ST_RESULT_AFTER_THE_GAME        26
#define ST_FIRST_EXTRA_STARTING         27
#define ST_FIRST_EXTRA_ENDED            28
#define ST_FIRST_HALF_ENDED             29
#define ST_GAME_ENDED                   30
#define ST_PENALTIES                    31

/* gameStatePl — the coarse simulation phase */
#define ST_GAME_IN_PROGRESS             100  /* ball live, clock running       */
#define ST_STOPPED                      101  /* a restart is being set up      */
#define ST_WAITING_ON_PLAYER            102  /* restart armed, waiting for fire*/
/* 0x65 (101) and 0x66 (102) also appear as literals; they are the same values */
```

The pair `(gameStatePl, gameState)` is the whole match state machine.
`gameStatePl == ST_GAME_IN_PROGRESS` means the ball is live; otherwise
`gameState` says which restart is pending and `breakCameraMode` (§18.3) drives
the hand-over back to live play.

---

## 3. Fixed-point math and the direction system

### 3.1 `CalculateDeltaXAndY`

Every moving object — players, ball, referee — gets its per-frame velocity from
this one routine. It is the single most important function to port exactly:
tiny differences change every trajectory in the game.

```
CalculateDeltaXAndY(destX, destY, srcX, srcY, speed) -> (dir, deltaX, deltaY)

  dx = destX - srcX
  dy = destY - srcY
  xNeg = (dx < 0)
  yNeg = (dy < 0)
  adx = |dx| ; ady = |dy|

  /* normalise into a 32x32 grid by repeated halving — note both axes
     are halved together, so the ratio is preserved */
  while (adx >= 32) { adx >>= 1; ady >>= 1; }
  while (ady >= 32) { adx >>= 1; ady >>= 1; }

  t = kAngleCoeficients[(ady << 5) | adx]        /* 0..64, or -1 */
  if (t < 0) { deltaX = deltaY = 0; return dir = 0xFFFF; }   /* no movement */

  t <<= 1                                        /* 0..128 */

  /* map into a 0..511 "half-degree" circle */
  if (!xNeg &&  !yNeg) dir512 = 128 - t
  if (!xNeg &&   yNeg) dir512 = 128 + t
  if ( xNeg &&  !yNeg) dir512 = 384 + t
  if ( xNeg &&   yNeg) dir512 = 384 - t
  dir512 &= 0x1FF

  cosIdx = dir512
  sinIdx = (dir512 + 128) & 0x1FF

  cos_q8 = kSineCosineTable[cosIdx >> 1] >> 8    /* arithmetic shift, -128..127 */
  sin_q8 = kSineCosineTable[sinIdx >> 1] >> 8

  deltaX = cos_q8 * speed                        /* 16.16 fixed point */
  deltaY = sin_q8 * speed

  dir = ((512 - dir512 + 256) & 0x1FF) >> 1      /* 0..255 */
  return dir
```

Notes that matter:

* The original halves in two sequential loops (`adx` first, then `ady`), but
  because both loops halve *both* axes the result is identical to the single
  loop `while (adx >= 32 || ady >= 32) { adx >>= 1; ady >>= 1; }` — verified
  exhaustively over the whole 16-bit input range. Either form is bit-exact; the
  thing you must **not** do is halve one axis independently of the other.
* `>> 8` on the 16-bit sine value is an **arithmetic** shift of a signed value,
  giving `-128..127`. Do not use `/ 256` on an unsigned type.
* The returned `dir` is a 0..255 angle where **0 = up (−Y)** and it increases
  clockwise. `0xFFFF` means "source equals destination".
* The 8-way direction is derived as `((dir + 16) & 0xFF) >> 5` — i.e. round to
  the nearest of 8 sectors.

### 3.2 The two tables

`kSineCosineTable[257]` — entries 0..255 are `round(32767 * sin(2πi / 256))`
with **14** entries off by exactly ±1 (at i = 25, 29, 93, 125, 163, 227, 231,
234, 238, 239, 241, 246, 248, 252 — the original was hand-tabulated). Entry 256
is the assembler pad `0x1A00` and is **never indexed**: the lookup index is
`(dir512 & 0x1FF) >> 1`, which cannot exceed 255.

The *sign* of each of those 14 deviations is not derivable from this description,
so a bit-exact table cannot be generated — **transcribe the literal table from
the file named in §24.12** if you want bit-exact trajectories. A generated table
differs by at most 1/32767 at 14 of 256 entries and is visually identical.

`kAngleCoeficients[1025]` — a 32×32 grid indexed `[ady*32 + adx]` giving a
*pseudo-angle* `t ∈ [0, 64]`, where `t = 0` means "along +X" and `t = 64` means
"along +Y". Entry `[0]` is `-1` (the zero-vector sentinel). It is **not**
`atan2`; it is a **scaled-tangent** approximation (`32·tan θ` in the ≤45° octant,
mirrored about the diagonal — *not* the standard "diamond angle" `y/(x+y)`),
generated by:

```
t(adx, ady):
    if (adx == 0 && ady == 0) return -1
    if (ady == 0)             return 0
    if (adx == 0)             return 64
    a = min(adx, ady); b = max(adx, ady)
    q = (32 * a + (b >> 1)) / b          /* integer division, truncating */
    return (ady <= adx) ? q : (64 - q)
```

This reproduces every one of the 1024 table entries exactly. Either embed the
table or generate it with this formula — both are correct.

The practical consequence is that SWOS movement angles are **noticeably** off
true angles: over the 32×32 grid the median error is ≈ 2.9° and the worst case is
≈ 4.7° (at `adx=15, ady=31`: 68.9° instead of 64.2°); 682 of the 1023 non-zero
entries are more than 2° out. Substituting a real `atan2` changes shooting and
passing feel and is a common source of "it doesn't play like SWOS" ports.

### 3.3 Directions

Two representations coexist:

* **8-way direction** (`Sprite.direction`, `TeamGeneralInfo.currentAllowedDirection`):

  ```
  0 = N (up, -Y)      1 = NE      2 = E (+X)      3 = SE
  4 = S (down, +Y)    5 = SW      6 = W (-X)      7 = NW
  ```

  `-1` (`0xFFFF`) means "no direction / stick centred".

* **Full direction** (`Sprite.fullDirection`, 0..255) — the fine angle from the
  **ball to the player** (or from the foul spot when the ball is dead),
  recomputed every frame in the tail of `UpdatePlayers`. Note the sense: to make
  a player *face* the ball the engine adds 128 (`(fullDirection + 128) & 0xFF`),
  so do not invert this field. AI and pass-target selection
  compare these as **signed bytes** so the wrap-around at 255→0 is free:

  ```c
  int8_t diff = (int8_t)((uint8_t)a->fullDirection - (uint8_t)b->fullDirection);
  if (diff >= -16 && diff <= 16) /* roughly the same heading */;
  ```

  Reproduce the 8-bit wrap exactly; using ints and a modulo breaks the AI.

### 3.4 Turn flags

`playerTurnFlags` is a bitmask of *legal* 8-way directions during a restart
(bit `d` set = direction `d` allowed). It stops a player taking a corner from
facing out of the pitch, etc.

| Value  | Allowed directions      | Used for                                |
|--------|-------------------------|-----------------------------------------|
| `0xFF` | all                     | free kick / foul                        |
| `0x83` | N, NE, NW               | penalty at the top goal, penalties      |
| `0x38` | SE, S, SW               | penalty at the bottom goal              |
| `0x7C` | E, SE, S, SW, W         | kick-off downward, top goal kick        |
| `0xC7` | N, NE, E, W, NW         | kick-off upward, bottom goal kick       |
| `0x1C` | E, SE, S                | corner, top-left                        |
| `0x70` | S, SW, W                | corner, top-right                       |
| `0x07` | N, NE, E                | corner, bottom-left                     |
| `0xC1` | N, W, NW                | corner, bottom-right                    |
| `0x1F` | N, NE, E, SE, S         | throw-in from the left touchline        |
| `0xF1` | N, S, SW, W, NW         | throw-in from the right touchline       |

On a **goal kick** or a **keeper-holds-ball** restart taken by a CPU team
(`playerNumber == 0`) the flags are additionally masked with `& 0xBB`, clearing E
and W (bits 2 and 6) so the AI never kicks straight along the goal line. Corners,
throw-ins and free kicks are **not** masked.

---

## 4. Random number generators

Two independent, byte-wide PRNGs over a fixed 256-byte table. They are trivial
but must be reproduced exactly for **simulation determinism** — i.e. so that the
same inputs from the same state produce the same match. That is *not* the same as
replay fidelity: SWOS's HIL/RPL replays are position recordings and playback never
re-simulates, so a replay plays back correctly even on a port whose RNG diverges.
See §25.5 for which kind of determinism each requirement serves.

```c
uint8_t  seed,  randXorIndex,  randXorKey;      /* generator 1 */
uint8_t  seed2, randXorIndex2, randXorKey2;     /* generator 2 */

uint16_t Rand(void) {            /* gameplay RNG */
    uint8_t d0 = seed;
    if (d0 == 0) {                                /* wrapped: rekey */
        randXorIndex++;
        randXorKey = kRandomTable[randXorIndex];
    }
    d0 = kRandomTable[d0] ^ randXorKey;
    seed++;
    return d0;                                     /* 0..255 */
}

uint16_t Rand2(void)  { /* identical, using seed2/randXorIndex2/randXorKey2 */ }
uint16_t GetRandomWord(void) { uint16_t lo = Rand2(); return (Rand2() << 8) | lo; }
```

`Rand()` is used by gameplay (injuries, cards, coin tosses, referee jitter,
kick-off jitter). `Rand2()` is used by the non-realtime simulation
(match-result simulation, fake scorers). There is a 5-deep save/restore stack
for `Rand2` state (`SaveRand2State` / `RestoreRand2State`, 3 bytes each) used
when the season simulator needs to branch and come back.

**Determinism note:** the engine also uses `currentGameTick` — a free-running
50 Hz counter — as a pseudo-random source in dozens of places
(`currentGameTick & 0x1F`, `(currentGameTick & 0xF0) >> 4`, …). This is *not*
an RNG; it is a deterministic function of match time. Every such site must read
the same counter, incremented once per frame, or behaviour diverges.

**Seeding.** All six state bytes start at 0 in BSS, and a zero `seed` makes the
very first `Rand()` take the rekey arm — which is observable, so the seeding
matters. The primary pair is re-seeded from the free-running tick counter on
**entry to a game mode** (five menu entry points) — *not* per match: nothing in
the match-setup chain touches it, so a career seeds once and plays hundreds of
matches off one continuous stream.

```
seed         = (currentTick >> 8) & 0xFF
randXorIndex =  currentTick       & 0xFF
```

**Only those two bytes** — on the 68000 this was a single word write to the
adjacent `seed`/`randXorIndex` pair. `randXorKey` is *not* seeded: it starts at 0
and is only ever written inside `Rand()`'s own wrap arm. Reproduce exactly this,
including leaving `randXorKey` alone; seeding it as well diverges the first 255
values of every match.

`kRandomTable` is in §24.11.

---

## 5. The frame loop

One iteration = one 50 Hz tick. Order is load-bearing; several systems read
state written earlier in the same frame.

```
GameLoop:
  wait for VBlank / present the previous frame
  ReadTimerDelta()                    -> lastFrameTicks

  while (paused) { pump input; handle keys; play queued audio }

  frameCount++
  if (spaceReplayTimer) spaceReplayTimer--

  poll keyboard -> MainKeysCheck()    (pause, replay, stats, subs, ESC)
  if (!playGame) goto end of match

  if (a replay was requested) { run the replay loop; restart the frame }

  ---- simulation ----
  MoveCamera()                        §20
  ScrollToCurrent()                   (render; also latches camera validity)
  record camera position for highlights
  play queued sound effects
  UpdateTime()                        §21 — clock, half/full time, extra time
  UpdateAndApplyTeamControls()        §6/§7/§8  — input + one team's UpdatePlayers.
                                      Its FIRST statement is
                                      UpdateGameTimersAndCameraBreakMode()
                                      (§18.3/§21.4a): the penalty timer, the
                                      stoppage timers, the stopped-state
                                      dispatcher and the restart state machine
                                      all run here, every frame, BEFORE the
                                      frameCount&1 team split.
  UpdateBall()                        §10
  MovePlayers()                       §9  — integrate all 22 players, step anims
  UpdateReferee()                     §19
  (injury tick)
  update corner flag / decorative sprites
  ManageAdvertisements()
  DoGoalkeeperSprites()               §15.6 — pin ball to a diving keeper
  UpdateControlledPlayerNumbers()     (HUD)
  MarkPlayer()                        (HUD)
  SetCurrentPlayerName()              (HUD)
  UpdateBookedPlayerNumberSprite()    §19
  ShowResult()                        (HUD)
  DrawAnimatedPatterns()              (also ticks the goal-celebration counter)
  DrawSprites()                       (render; WRITES Sprite.onScreen)
  DrawBenchAndSubsMenu() / BenchCheckControls()
  UpdateAndDrawStatistics()           §22 — also accumulates possession AND
                                      hosts the goal-attempt tracker

  handle highlight capture / instant replay flags
```

Two ordering facts to preserve:

1. **`UpdateAndApplyTeamControls` runs before `UpdateBall`.** Players decide
   what to do with the ball using *last* frame's ball position; the ball is then
   integrated. Swapping these introduces a one-frame lead and changes
   first-touch behaviour.
2. **`DrawSprites` writes `onScreen`**, and the simulation reads it: auto
   player-selection (§7.1, §7.2), the animation walker and `movePlayer` (§9.2,
   §9.3), the restart hand-over's "all on-screen players have arrived" gate
   (§18.3 state 3), the referee update (§19) and the bench camera (§20.1). It is
   the only channel by which rendering feeds back into gameplay.

   ```
   for each sprite s in the sorted draw list:
       if (s.imageIndex < 0) continue      /* hidden: onScreen NOT written */
       g  = spriteGraphics[s.imageIndex]   /* per-IMAGE metrics            */
       dx = s.x - g.centerX - cameraX
       dy = s.y - g.centerY - cameraY - s.z
       s.onScreen = !(   dx >=  viewWidth        /* 336 in the original */
                      || dy >=  272
                      || dx <= -g.pixWidth
                      || dy <= -g.nlines )
   ```

   All four comparisons are signed 16-bit; the positive edges use `>=` and the
   negative edges `<=`.

   `centerX`, `centerY`, `pixWidth` and `nlines` are **per-image asset data** —
   the operands of the sprite-extraction command that produced that image — so
   they change on **every animation frame**. Across an outfielder's range
   `pixWidth` runs 8..16, `nlines` 10..20, `centerX` 0..13, `centerY` 0..17: a
   typical running frame is `12 × 15` anchored at `(4, 12)`. So `centerX` is
   *not* half the width — it is the artist's anchor, nearer a third — and
   `centerY` sits down at the feet. The ball is `4 × 4` anchored `(1, 3)`.

   Three further rules:

   * **Hidden sprites keep a stale value.** A negative `imageIndex` is skipped
     without writing `onScreen`. Clearing it changes which players are eligible
     for auto-selection during substitutions and sendings-off.
   * **Invisible sprites are not tested at all.** The sorted list is rebuilt only
     by `InitDisplaySprites()`, which drops everything with `visible == 0`; such
     a sprite keeps its last `onScreen` until the list is rebuilt.
   * **`viewWidth` is 336 in both releases — a constant, not the render
     width.** Only the height edge follows the display (`272` here, `200` on
     DOS). An earlier revision said "the render width"; that was this port's
     widescreen bug, since fixed — see §25.4, which also lists the camera and
     referee literals that are functions of the same view.

   **A port with its own artwork cannot match the original's `onScreen`
   exactly.** The metrics are authored data that differ per frame and the test is
   exact, so any substitute disagrees on frames where a player is within a
   sprite-width of a view edge — and that feeds into which player you control.
   This does **not** affect *playback* fidelity (replays are position recordings,
   not input recordings — §25.5); it matters if you build input-level
   re-simulation on top of this engine, and it also matters when **making** a
   recording: `SaveCoordinatesForHighlights` is called from the sprite-draw pass,
   so the cull decides which sprites a newly-made recording even contains.

### 5.1 Team alternation

`UpdateAndApplyTeamControls` processes **one team per frame**, alternating on
`frameCount & 1`:

```
team = (frameCount & 1) ? topTeamData : bottomTeamData
UpdateControlledPlayer(team)
UpdatePlayerBeingPassedTo(team)
... read that team's input, then UpdatePlayers(team)
```

So each side's `UpdatePlayers` runs at **25 Hz**, interleaved. This is not an
optimisation you can undo: the whole feel of contested balls, tackles and
possession changes if both teams are updated every frame, because the team that
runs second sees state the first already mutated. Keep the alternation.

`UpdateBall`, `MovePlayers` and the camera run every frame for both teams.

---

## 6. Input and the control model

### 6.1 Raw joystick decode

Each player port produces, per frame, four direction booleans and a fire
boolean. The Amiga hardware decode (reproduce the *result*, not the register
reads) is:

Note the port numbering is inverted relative to the player numbering: **player 1
reads JOY1DAT, player 2 reads JOY0DAT**.

```
right = bit1 of JOYnDAT
left  = bit9 of JOYnDAT
d     = JOYnDAT ^ (JOYnDAT >> 1)
down  = bit0 of d
up    = bit8 of d
fire  = CIAA_PRA bit7 (player 1) / bit6 (player 2), active LOW
```

Booleans are stored as `0xFF` / `0x00`. Diagonals are the AND of the two axes.

### 6.2 Direction priority

Diagonals win over cardinals:

```
if (upRight)   dir = 1
else if (downRight) dir = 3
else if (downLeft)  dir = 5
else if (upLeft)    dir = 7
else if (up)        dir = 0
else if (right)     dir = 2
else if (down)      dir = 4
else if (left)      dir = 6
else                dir = -1        /* centred */
```

### 6.3 Fire: tap vs hold

This is the core of SWOS control — a **tap passes, a hold shoots**, and the
length of the hold also drives aftertouch. The mechanism is a signed counter.

Per **team update** (25 Hz — the port is only polled on the frame its team is
being updated, §5.1), in the input reader:

```
if (wasFiring) {
    shortFire = 0                        /* only true on the press edge  */
    if (!fire) {                          /* released this frame          */
        wasFiring   = 0
        fireCounter = -fireCounter        /* sign marks "release pending" */
    } else if (fireCounter != 0) {
        fireCounter--                     /* counts DOWN from 0xFFFF      */
    }
} else {
    shortFire = fire                      /* press edge                   */
    wasFiring = fire
    if (fire) fireCounter = 0xFFFF        /* == -1, so it counts up in |.| */
}
```

`fireCounter` starts at `0xFFFF` (i.e. `-1`) on the press and decrements each
frame it is held, so after *n* held frames it is `-(n+1)`. On release it is
negated, becoming `+(n+1)` — a positive "a press of length n just completed"
token.

Then, once per frame for the team being updated:

```
team.currentAllowedDirection = joystick direction (0..7 or -1)
team.direction               = same
team.fireThisFrame           = shortFire            /* press edge          */
team.firePressed             = fire                 /* level               */
team.fireCounter             = fire ? team.fireCounter + 1 : 0   /* unused below */

/* IMPORTANT: the classification reads the GLOBAL per-port counter written by
   the input reader (pl1FireCounter / pl2FireCounter) — NOT team.fireCounter,
   which merely counts held frames upward and carries no press/release token. */
fc = plNFireCounter                                 /* the signed counter  */
if (fc == 0) {
    quickFire = 0; normalFire = 0
} else if (fc < 0) {                                /* still held          */
    quickFire  = 0
    normalFire = (fc < -4) ? TRUE : FALSE           /* held > 4 updates    */
    if (fc < -4) plNFireCounter = 0
} else {                                            /* released            */
    if (fc <= 4) { quickFire = TRUE;  normalFire = FALSE }   /* tap        */
    else         { quickFire = FALSE; normalFire = TRUE  }   /* hold       */
    plNFireCounter = 0
}
```

The resulting semantics:

| Signal        | Meaning                                          | Consumed by                                   |
|---------------|--------------------------------------------------|-----------------------------------------------|
| `firePressed` | fire is down right now                           | aftertouch gating, referee/result dismissal   |
| `fireThisFrame` | fire went down this frame                      | tackle / header launch                        |
| `quickFire`   | a **tap** (≤ 4 frames) completed                 | pass, throw-in short                          |
| `normalFire`  | a **hold** (> 4 frames), fires while still held  | shot, throw-in long                           |

Because `normalFire` fires *while still held* (the `fc < -4` branch) and the
counter is then zeroed, a long hold produces exactly one shot on the 5th **team
update** — ≈ the 10th display frame, ≈ 0.2 s — not a stream. Everywhere in this
section "frame" means *team update* (25 Hz, §5.1): the counter only advances on
frames where that port's team is the one being updated, so the tap window is
≈ 0.16 s of real time.

A consequence worth knowing: the `fc > 4` (release-after-a-long-hold) arm of the
classifier is effectively **dead code**. Once `fc < -4` zeroes the counter it can
never grow again, and the input reader only decrements a non-zero counter, so the
release negates 0 and yields `fc == 0`. Every `normalFire` in practice comes from
the still-held arm.

If `resetControls` is set the team's control fields are still written and
classified, but `UpdatePlayers` is not called for that team this update — the
team simply does not act. (Do not model this by zeroing the inputs: other systems
read `firePressed` to dismiss result screens.) While the
substitutes menu is open all six control fields are forced to
`currentAllowedDirection = -1`, everything else `0`.

`headerOrTackle` is a one-shot flag: `UpdatePlayers` sets it when it launches a
tackle or header, and the caller clears it and zeroes `fireCounter`, so the same
press cannot also trigger a pass or shot.

> **A bug in the original, and the one place this port knowingly departs from
> it.** A team whose `playerNumber` is 0 — the **computer** — has no tail of its
> own in the asm: `beq.w post_input_common_p1` (SWOS2.s:25599) drops it into the
> PLAYER 1 tail, so the `clr.w (pl1FireCounter)` there fires on an **AI**
> tackle and zeroes the **human's** counter. Nothing of the human's press was
> consumed; an opponent merely lunged at him. And because a zeroed counter can
> never restart under a still-held button (above), his hold is dead until he
> releases and presses again — so holding fire through a challenge silently
> never shoots. Measured over the corpus: 21 such wipes per match against 40
> shots that armed, and they cluster exactly where a player notices, in the box
> with a defender on him.
>
> The port fixes this: the computer team's tail no longer touches the human's
> counter. `--fire-hold-lockout=on` / `[gameplay] fire_hold_lockout=true`
> restores the original bit-exactly, and the replay rig forces it on, because
> the corpus compares against a reference implementation of the original.
>
> The **human** team's own tail is untouched in both modes — zeroing there is
> deliberate ("one press, one action"), and reviving the counter would make
> letting go after your own tackle fire a surprise pass, since the zero is also
> what makes the release inert.

### 6.4 Fire blocking

`fireBlocked` is set when a menu/overlay consumed a press. While set,
`UpdateAndApplyTeamControls` does nothing except poll both sticks and wait for
**both** fire buttons to be released, then clears `fireBlocked` and both fire
counters. This prevents a menu-dismissing press leaking into gameplay.

### 6.5 In-match keys

`MainKeysCheck(key)` — gating rules matter as much as the actions:

| Key      | Action                                                              |
|----------|---------------------------------------------------------------------|
| `P`      | toggle pause (works even while paused; blocked in the subs menu)     |
| `S`      | show statistics — only when the ball is dead and not in a break state|
| `R`      | request instant replay — not during result screens or the subs menu  |
| `H`      | watch highlights — only on the after-the-game result screen          |
| `SPACE`  | mark the current moment as a highlight (100-frame cooldown)          |
| `Q` / `W`| open the bench for the bottom / top team (if it has a human manager)  |
| `ESC`    | abandon the match (see below)                                        |

While paused, only `P` is accepted; while the stats overlay is up, only `S`;
while a replay is playing, only `P`.

**ESC** ends the match. If exactly one side is human, the *other* side is
awarded a walkover: `+5` goals, then further goals until it leads, and
`AssignFakeGoalsToScorers` distributes those goals among that team's players
using `kAbortLossScorerWeightTable` (§24.9) — with a `Rand2() >= 248` chance
(3.1 %) of each being logged as an own goal instead. If both sides are human or
both CPU, the match is simply cancelled.

---

## 7. Player selection (who you control)

Two pointers per team decide who acts:

* `controlledPlayer` — the player the human/AI steers.
* `passToPlayerPtr` — the designated receiver; it runs to meet the ball and can
  be promoted to `controlledPlayer` on first touch.

Both are recomputed each time that team is updated, **before** `UpdatePlayers`.

### 7.1 `UpdateControlledPlayer` — nearest eligible player to the ball

```
for i in 0..10:
    p = team.spritesTable[i]
    p.ballDistance = (p.x - ball.x)^2 + (p.y - ball.y)^2      /* always update */

    if (gameStatePl == ST_GAME_IN_PROGRESS && !p.onScreen) continue
    if (p.sentAway)                                            continue
    if (!team.goaliePlayingOrOut && p.playerOrdinal == 1)      continue
    if (p == team.passingKickingPlayer)                        continue
    if (p.playerState is TACKLING, TACKLED, JUMP_HEADING,
                         STATIC_HEADING or ROLLING_INJURED)    continue
    if (p == team.passToPlayerPtr)                             continue
    if ((uint32)p.ballDistance >= (uint32)best)                continue
    best = p.ballDistance ; nearest = p

if (playingPenalties && gameStatePl == ST_GAME_IN_PROGRESS) return
if (!nearest)          return
if (!team.ballOutOfPlay) return          /* switching is locked while live */

if (nearest != controlledPlayer && controlledPlayer is PL_NORMAL)
        controlledPlayer.dest = controlledPlayer.pos     /* stop the old one */
controlledPlayer = nearest
if (nearest == passToPlayerPtr) {
    if (passToPlayerPtr is PL_NORMAL) passToPlayerPtr.dest = passToPlayerPtr.pos
    passToPlayerPtr = NULL
}
```

Key points:

* `ballDistance` is refreshed for **every** player including skipped ones —
  many other systems read it.
* The distance comparison is **unsigned**, and `best` starts at `0xFFFFFFFF`.
* The loop visits **0 → 10** and the comparison is strict (`>=` skips), so on a
  tie the *lower* index — the first one visited — keeps the slot; the keeper
  (`spritesTable[0]`) is visited first. The asm's counter runs *down*
  (`move.w #10,d6` … `dbf d6`) while the table pointer runs *up*
  (`movea.l (a1)+,a2`); translating the counter into the index reverses the
  walk, which is invisible until two players tie on `ballDistance` (the replay
  rig caught exactly that — docs/TESTS.md). The same holds for §7.2, §12.2's
  `GetClosestNonControlledPlayerInDirection` and §14.2's last-man search.
  *(Revisions before 2.0 said 10 → 0 here, copying a port bug.)*
* `team.ballOutOfPlay` gates the actual switch. It is set when the ball is
  released (pass/shot/throw) and cleared once someone takes possession, which is
  what makes SWOS switch player only when the ball leaves your feet.

### 7.2 `UpdatePlayerBeingPassedTo` — the second-nearest

Runs immediately after, with a filter that excludes `controlledPlayer` instead of
`passToPlayerPtr` and is otherwise *stricter*: rather than blacklisting five
states it requires `playerState == PL_NORMAL`, so heading, tackling, tackled,
injured, down, booked, throw-in and every keeper state are all excluded:

```
team.ballX = ball.x ; team.ballY = ball.y     /* default receiver target */

if (!team.ballInPlay)          return
if (team.playerSwitchTimer)    return         /* 25-frame lock-out       */
if (team.passingToPlayer && team.passToPlayerPtr) return   /* keep target */

if (playingPenalties) {                        /* penalties: forced shooter */
    if (gameStatePl != ST_PENALTIES) return
    team.ballInPlay = 0
    team.passToPlayerPtr = penaltyShooterSprite
    return
}

for i in 0..10:                                /* ascending — see §7.1 */
    p = team.spritesTable[i]
    if (gameStatePl == ST_GAME_IN_PROGRESS && !p.onScreen) continue
    if (p.sentAway)                                        continue
    if (!team.ballOutOfPlayOrKeeper && p.playerOrdinal == 1) continue
    if (p == team.controlledPlayer)                        continue
    if (p == team.passingKickingPlayer)                    continue
    if (p.playerState != PL_NORMAL)                        continue
    if ((uint32)p.ballDistance >= (uint32)best)            continue
    best = p.ballDistance ; nearest = p

if (!nearest) return
if (nearest != passToPlayerPtr && passToPlayerPtr is PL_NORMAL)
        passToPlayerPtr.dest = passToPlayerPtr.pos
passToPlayerPtr = nearest
```

When the ball is dead two things differ: the `onScreen` test is skipped (so a
receiver can be chosen off-camera for a restart), and the "stop the previous
receiver" step is omitted — the dead-ball path assigns `passToPlayerPtr` directly
without snapping the old receiver's destination, so a de-selected player keeps
running during a restart.

### 7.3 `playerSwitchTimer`

Set to `25` frames whenever control is handed over automatically (first touch by
the receiver, keeper claim, break hand-over). While non-zero,
`UpdatePlayerBeingPassedTo` will not pick a new receiver. It decrements once per
team update (so ≈ **1 second** of real time given the 25 Hz per-team rate). It is
the main reason SWOS does not flip control back and forth right after a pass is
received.

---

## 8. UpdatePlayers — the per-player state machine

`UpdatePlayers(team)` is the heart of the simulation: ~2700 lines of the
original. It is called **once per frame, for one team** — the two teams alternate
on `frameCount & 1` (§5.1), so each team is updated at 25 Hz — and it walks all 11
of that team's players.

### 8.1 Preamble

```
prevLastPlayer     = lastPlayerPlayed
prevLastTeamPlayed = lastTeamPlayed

if (lastKeeperPlayed && lastPlayerPlayed is not a goalkeeper sprite)
        lastKeeperPlayed = NULL

/* timers, all per team update */
if (team.goalkeeperSavedCommentTimer) step it toward zero (either sign)
if (team.passKickTimer)  { if (--team.passKickTimer == 0) team.passingKickingPlayer = NULL }
if (team.wonTheBallTimer)    team.wonTheBallTimer--
if (team.playerSwitchTimer)  team.playerSwitchTimer--

ApplyBallAfterTouch(team)                        /* §12.5 */

/* ball zone flags — GLOBAL, recomputed by whichever team runs this frame */
ballInUpperPenaltyArea = ballInLowerPenaltyArea = ballInGoalkeeperArea = 0
if (ball.x in [193,478] and ball.y in [129,769]) {
    if (ball.y <= 216) ballInUpperPenaltyArea = 1
    else if (ball.y >= 682) ballInLowerPenaltyArea = 1
    if (either set and ball.x in [273,398] and (ball.y <= 158 or ball.y >= 740))
        ballInGoalkeeperArea = 1
}
if (gameStatePl == ST_GAME_IN_PROGRESS && goalOut
    && !ballInUpperPenaltyArea && !ballInLowerPenaltyArea)
        goalOut = 0                              /* goal kick has left the box */

team.updatePlayerIndex = (team.updatePlayerIndex + 1) % 11
```

`updatePlayerIndex` is a round-robin cursor: only the *one* idle player whose
loop index matches it gets a fresh tactical destination this update (§8.8). So
each idle player is repositioned roughly every 11 team updates ≈ 0.44 s. This
is what makes SWOS off-ball movement look deliberate rather than twitchy — do
not reposition every player every frame.

### 8.2 Per-player dispatch

The loop runs `i = 10 .. 0` over `team.spritesTable[0..10]` (note: the *pointer*
walks forward while `i` counts down; `i` is the loop counter used by the
round-robin test, so `spritesTable[k]` is visited when `i == 10 - k`).

```
for i = 10 downto 0:
    player = *sprPtr++
    player.playerDirection = player.direction        /* remember for anim */
    player.isMoving = (player.deltaX | player.deltaY) ? -1 : 0

    if (player.playerState == PL_TACKLED)         -> §8.7 tackled recovery
    if (player.playerState == PL_ROLLING_INJURED) -> §8.7 injury recovery

    /* "active" players get the full treatment; everyone else is idle */
    if (player == team.controlledPlayer
     || player.playerOrdinal == 1                    /* the goalkeeper */
     || player == team.passToPlayerPtr
     || player.playerState != PL_NORMAL)          -> ACTIVE
    if (injuriesForever && player.injuryLevel == -1) -> injury-forever tick
    -> IDLE (§8.8)
```

**ACTIVE branch — proximity classification.** Before dispatching further, the
team's ball-proximity flags are recomputed *for this player*. This is worth
stating twice, because the fields live on `TeamGeneralInfo` and read like
team-wide summaries: they are **not**. They are cleared and re-derived from the
current player's own `ballDistance` on every pass through this branch, so any
later read of `team.plVeryCloseToBall` (§8.3, §8.10, §15.2-D, §16.3) sees *that
player's* band, not "any of my 11". `prevPlVeryCloseToBall` holds the value from
whichever player was processed immediately before, which is the only cross-player
information here.

Note also that this branch is reached only for the controlled player, the keeper,
the pass target, and any player not in `PL_NORMAL` — an ordinary off-ball player
in `PL_NORMAL` skips it, so the flags never reflect him at all:

```
team.prevPlVeryCloseToBall = team.plVeryCloseToBall
/* an if / else-if chain: these are MUTUALLY EXCLUSIVE BANDS, and if the ball
   is further than 2450 away all three stay clear */
team.plVeryCloseToBall = (       ballDistance <=   32)   /*   0  .. 5.6 units */
team.plCloseToBall     = (32   < ballDistance <=   72)   /* 5.6  .. 8.5 units */
team.plNotFarFromBall  = (72   < ballDistance <= 2450)   /* 8.5  .. 49  units */

team.ballLessEqual4 = (ball.z <= 4)
team.ball4To8       = (ball.z in  5..8)
team.ball8To12      = (ball.z in  9..12)
team.ball12To17     = (ball.z in 13..17)
team.ballAbove17    = (ball.z > 17)
```

These are *team* fields overwritten per player — a later player's classification
clobbers an earlier one's. That is intentional; consumers read them immediately.

**The bands are exclusive, and that is load-bearing.** Every gate that tests a
*single* flag means a ring, not a disc. Most importantly the tackle launch
(§8.3-E, §8.9) requires `plNotFarFromBall` on its own, so a slide can only be
started from the 8.5–49 unit annulus — never at point-blank range, where you take
the ball instead. Gates that test several flags together (the header launch tests
all three) do behave like the cumulative reading, but for a different reason.

Then dispatch by state:

```
PL_TACKLING            -> §8.6
PL_JUMP_HEADING        -> §8.5
PL_STATIC_HEADING      -> §8.5
PL_THROW_IN            -> §8.4
PL_GOALIE_DIVING_*     -> §15.5
PL_GOALIE_CATCHING_BALL-> §15.4
PL_DOWN                -> countdown, then PL_NORMAL
PL_BOOKED              -> wait for whichCard == 0, then PL_NORMAL
PL_SAD / PL_HAPPY      -> revert to PL_NORMAL at half/full time
PL_GOALIE_CLAIMED      -> countdown, then PL_NORMAL
playerOrdinal == 1     -> §15 goalkeeper logic
player == controlledPlayer -> §8.3
player == passToPlayerPtr  -> §8.9 receiver
otherwise              -> PANIC (unreachable)
```

### 8.3 The controlled player

```
if (playingPenalties && gameState != ST_PENALTIES) -> finalize

if (team.playerNumber == 0) AI_SetControlsDirection(team)     /* §16 */

team.allowedDirections = team.playerHasBall     /* remember last frame */
team.playerHasBall     = 0                      /* re-evaluated below  */
if (team.shooting && !team.firePressed && !team.fireThisFrame)
        team.shooting = 0
```

**A. Ball dead (`gameStatePl != ST_GAME_IN_PROGRESS`).** Only the team that has
the restart (`lastTeamPlayedBeforeBreak`) may act; everyone else finalises.

```
/* force the facing into the legal set for this restart */
if (!(playerTurnFlags & (1 << player.direction)))
        player.direction = cameraDirection
if (!(playerTurnFlags & (1 << player.direction))) {
        scan d = 7..0 for the first set bit in playerTurnFlags
        cameraDirection = d ; player.direction = d
}

/* A CPU team wakes up once the stoppage has run more than 50 ticks; a human
   team wakes up on ANY stick deflection or fire signal. (The separate 750-tick
   gsKickoffStoppage hard limit lives in the timer code — §18.3.)
   "toggle timers" below means: NEGATE `timeVar` and `resultTimer` in place —
   those two only. It is NOT `negateTimers()` (§21.4a), which also negates
   `statsTimer`. Sign is meaningful state, not a flag: a negative `resultTimer`
   is what tells ShowResult to hide the banner. */
if (team.playerNumber == 0) {
    if (stoppageTimerActive > 50) { toggle timers }   /* note: no statsTimer test */
    else goto fireCheck
} else if (team.currentAllowedDirection >= 0) {
    if (statsTimer) fireBlocked = 1 ; toggle timers
} else {
fireCheck:
    if (firePressed || quickFire || normalFire) {
        if (statsTimer) fireBlocked = 1 ; toggle timers
    }
}

d = team.currentAllowedDirection
if (d < 0 || !(playerTurnFlags & (1 << d)))
        team.currentAllowedDirection = player.direction    /* keep facing */
else {
        player.direction = d
        UpdatePlayerWithBall(player)     /* snap the player onto the ball */
}
```

**B. Common tail — reached from both live *and* dead play.** Section A falls
through into this; it is not an alternative to it. (That fall-through is how
every throw-in, free kick and goal kick is actually taken: the pass/shot gates in
C are downstream of here, which is why C's own
`gameStatePl == ST_GAME_IN_PROGRESS || (playerTurnFlags & (1<<d))` clause exists
at all.) If the player is airborne (`player.z != 0`) it finalises immediately —
on the dead-ball path too. Otherwise:

```
if (team.currentAllowedDirection < 0)
        player.dest = player.pos            /* stick centred -> stand still */

/* --- can we take possession / head / dribble? --- */
if (gameStatePl != ST_GAME_IN_PROGRESS
    || team.quickFire || team.normalFire || team.shooting)
        goto C     /* NOTE: this ALSO skips the static-header branch and the
                      whole dribble/contest branch below — while `shooting` is
                      latched the player can only reach the pass/shot/tackle
                      gates, so a shooter cannot keep the ball at his feet */

if (team.plVeryCloseToBall || team.plCloseToBall) {
    if (!team.allowedDirections) team.ballControlTurnTimer = 0
    team.playerHasBall = 1
    team.opponentsTeam.spinTimer = -1            /* kill their aftertouch */
}
/* falls through to the direction test below whether or not possession was granted */
```

Then, with `d = team.currentAllowedDirection`:

```
if (d < 0) {
    /* stick centred: the only thing available is a standing header */
    if (team.playerNumber != 0 && player.playerOrdinal != 1
        && team.fireThisFrame
        && (plVeryCloseToBall || plCloseToBall || plNotFarFromBall)
        && (ball8To12 || ball12To17 || ballAbove17)) {
            team.headerOrTackle = 1
            AttemptStaticHeader(player, player.direction)      /* §13.3 */
            team.lastHeadingTacklingPlayer = player
            team.ballCanBeControlled = 0
    }
    -> finalize
}

/* --- direction held: dribble / contest the ball --- */
if (!team.opponentsTeam.playerHasBall) team.wonTheBallTimer = 0
if (team.wonTheBallTimer)  -> boundary mask (below)

if (team.plVeryCloseToBall && !team.ballAbove17 && !team.ball12To17) {
    if (!team.prevPlVeryCloseToBall) team.ballCanBeControlled = 0
    if (d != team.ballControllingPlayerDirection) team.ballCanBeControlled = 0
    team.ballCanBeControlled++
    team.ballControllingPlayerDirection = d

    lastTeamPlayed = team ; lastPlayerPlayed = player
    penalty = 0 ; playerHadBall = (opponent.playerHasBall) ? 1 : 0

    CalculateIfPlayerWinsBall(d, player, team)        /* §12.4 */
    opponent.spinTimer = -1
    opponent.passingToPlayer = 0
    opponent.passingKickingPlayer = NULL
    -> boundary mask
}
```

**C. Pass (quick fire).**

```
if (team.quickFire && d >= 0
    && (plVeryCloseToBall || plCloseToBall)
    && (gameStatePl == ST_GAME_IN_PROGRESS || (playerTurnFlags & (1<<d)))
    && (player.playerOrdinal == 1 || team.ballLessEqual4))
{
    lastTeamPlayed = team ; lastPlayerPlayed = player
    penalty = 0 ; playerHadBall = 0
    ball.z = 0
    DoPass(player, team)                                  /* §12.2 */
    team.lastHeadingTacklingPlayer = player
    if (ball was dead) { ThrowInDeadProc() }
    if (gameState == ST_PENALTY) penalty = 1
    gameStatePl = gameState = ST_GAME_IN_PROGRESS
    team.ballInPlay = team.ballOutOfPlay = 1
    opponent.ballInPlay = opponent.ballOutOfPlay = 1 ; opponent.spinTimer = -1
    team.controlledPlayer = NULL
    team.passingKickingPlayer = player
    team.passKickTimer = 25
    team.ballCanBeControlled = 0
    opponent.passingToPlayer = 0 ; opponent.passingKickingPlayer = NULL
    -> snap dest, finalize
}
```

**D. Shot (normal fire).** Identical gating and bookkeeping except that it does
**not** zero `ball.z` first — a shot can be struck off a bouncing ball, a quick
pass cannot, which is half of why volleys behave differently from passes — it
calls `PlayerKickingBall` (§12.3), and it additionally sets `team.shooting = 1`.

**E. Tackle.** Only if a pass/shot did not happen:

```
if (player.playerOrdinal != 1 && team.fireThisFrame && d >= 0
    && team.plNotFarFromBall
    && (team.ballLessEqual4 || team.ball4To8)
    && (team.passToPlayerPtr == NULL
        || team.passToPlayerPtr.ballDistance >= player.ballDistance))
{
    team.headerOrTackle = 1
    PlayerBeginTackling(d, player, team)                   /* §14.1 */
    team.ballCanBeControlled = 0
    -> finalize
}
```

**F. Jump header.** Same press, higher ball:

```
if (player.playerOrdinal != 1 && team.fireThisFrame && d >= 0
    && (plVeryCloseToBall || plCloseToBall || plNotFarFromBall)
    && (ball8To12 || ball12To17 || ballAbove17))
{
    team.headerOrTackle = 1
    PlayerAttemptingJumpHeader(d, player)                  /* §13.1 */
    team.lastHeadingTacklingPlayer = player
    team.ballCanBeControlled = 0
    -> finalize
}
```

**G. Boundary mask and default destination.** The last thing a live controlled
player does is set a destination one "stride" ahead, unless that would run off
the pitch:

```
mask = 0
if (player.x < 79)  mask |= 0xE0     /* forbid SW, W, NW  (bits 5,6,7)    */
if (player.x > 592) mask |= 0x0E     /* forbid NE, E, SE                  */
if (player.y < 127) mask |= 0x83     /* forbid N, NE, NW                  */
if (player.y > 771) mask |= 0x38     /* forbid SE, S, SW                  */

if (mask & (1 << d))  player.dest = player.pos            /* blocked */
else                  player.dest = player.pos + kDefaultDestinations[d]
```

`kDefaultDestinations` is a ±1000-unit vector per direction (§24.2) — a
deliberately distant target so `CalculateDeltaXAndY` produces a clean unit
heading; the player never actually arrives.

### 8.4 Throw-in

Entered when a player is put into `PL_THROW_IN` by the restart hand-over.

```
if CPU team: AI_SetControlsDirection(team)
force player.direction into playerTurnFlags (same two-step as §8.3-A)

if (substituteInProgress and this is the substituted player) -> unhide ball, PL_NORMAL
if (gameState not in THROW_IN_*)                             -> unhide ball, PL_NORMAL

if (player.playerDownTimer != 0) {          /* a throw is under way */
    if (--playerDownTimer == 0) -> PL_NORMAL
    if (playerDownTimer == 18) {            /* release frame */
        if (inSubstitutesMenu) { re-arm the "about to throw" pose; break }
        ball.z = 12 ; hideBall = 0
        if (throwInPassOrKick) DoPass(...)  else PlayerKickingBall(...)
        ... same "ball is now live" bookkeeping as §8.3-C, EXCEPT that
        playerHadBall is set to 1 (not 0), `penalty` is left alone, and a long
        throw does not set team.shooting; plus, for a pass:
            ball.deltaZ.raw = 1   /* raw 16.16 -> ~0.0000153 units/frame: a
                                     token positive value so gravity starts the
                                     throw's descent, NOT a 1-unit-per-frame
                                     lift */
    }
    -> finalize
}

/* not throwing yet: aim */
d = team.currentAllowedDirection
if (d >= 0 && (playerTurnFlags & (1<<d)) && d != player.direction) {
    player.direction = d
    SetThrowInPlayerDestinationCoordinates(player)   /* stand 3 units behind the ball */
    set "about to throw in" animation ; playerState = PL_THROW_IN ; downTimer = 0
}
if (team.quickFire && (playerTurnFlags & (1<<d))) {
    throwInPassOrKick = 1 ; set short-throw animation ; playerDownTimer = 20
}
else if (team.normalFire && (playerTurnFlags & (1<<d))) {
    throwInPassOrKick = 0 ; set long-throw animation ; playerDownTimer = 25
}
```

`SetThrowInPlayerDestinationCoordinates`: `x = ball.x - 3` if `ball.x < 336`,
else `ball.x + 3`; `y = ball.y`.

Release happens at `playerDownTimer == 18`, i.e. 2 frames into a short throw and
7 frames into a long one.

### 8.5 Heading states

**`PL_STATIC_HEADING`** (jump on the spot):

```
if (--playerDownTimer == 0) -> PL_NORMAL, snap dest
player.speed -= 16 ; clamp to >= 0
SetStaticHeaderDirection(player, team)          /* steer ±1 per frame, §13.2 */

if (live && !player.heading && player.ballDistance <= 64
    && ball.z in [8,15] && team.currentAllowedDirection >= 0)
{
    lastTeamPlayed = team ; lastPlayerPlayed = player
    penalty = 0 ; playerHadBall = 1
    PlayerHittingStaticHeader(player, team)     /* §13.2 */
    team.passKickTimer = 25
    opponent.passingToPlayer = 0 ; opponent.passingKickingPlayer = NULL
}
```

**`PL_JUMP_HEADING`** (running/diving header):

```
if (--playerDownTimer == 0) -> PL_NORMAL, snap dest

if (player.speed != 0) {
    SetJumpHeaderHitAnimTable(player)           /* §13.1 */
    airborneWindow = (anim == jumpHeaderAttempt) ? 37 : 17
    if ((uint8)playerDownTimer > airborneWindow) {
        player.speed -= kPlayerAirConstant (72) ; clamp >= 0
    } else {
        player.speed >>= 1                      /* landing: halve each frame */
        -> finalize        /* NOTE: exits the handler — the boundary block and
                              the connect test below are SKIPPED for the rest
                              of the landing phase */
    }
}
/* the boundary block and the connect test are only reached while the player is
   still airborne, or once speed has already decayed to 0 */

/* boundary handling. NOT a flat sequence — there is a fall-in label, and if y is
   inside [132,766] the WHOLE block is skipped and control goes straight to
   "connect". */
if (x < 73 || x > 598)  -> heavy slowdown (§below)
if (y < 132 || y > 766) {
    if (x in [296,375]) {
        if (x in [305,366])  -> goal-line band checks      /* falls IN */
        speed = 0 ; playerDownTimer = 0 -> stop dead
    }
    /* x outside [296,375] also falls into the label below */
goal-line band checks:
    if (y in [129,769])  -> connect                        /* may still connect */
    if (x in [290,381]) { speed = 0 ; playerDownTimer = 0 ; -> stop dead }
    if (y < 121 || y > 777)  -> heavy slowdown
    -> connect
}

stop dead:  player.playerState = PL_NORMAL
            set the normal standing animation
            player.dest = player.pos                      /* snapped */

/* connect */
if ((uint8)playerDownTimer >= 42 && live && !player.heading
    && player.ballDistance <= 64 && ball.z in [8,15])
{
    lastTeamPlayed/lastPlayerPlayed = ... ; penalty = 0 ; playerHadBall = 1
    PlayerHittingJumpHeader(player, team)       /* §13.1 */
    team.passKickTimer = 25
    opponent.passingToPlayer = 0 ; opponent.passingKickingPlayer = NULL
}
```

Heavy slowdown is `speed -= speed>>2; speed -= (speed>>2)>>1` (≈ ×0.625).

Both header states use `player.heading` as a one-shot latch so a single jump can
only touch the ball once.

### 8.6 Tackle state (`PL_TACKLING`)

```
if (player.tacklingTimer >= 0) player.tacklingTimer++   /* BOTH human and CPU — ungated */

if (--player.playerDownTimer == 0) -> PL_NORMAL, snap dest

/* human released fire early -> convert to a "weak" tackle */
if (player.tacklingTimer >= 0 && team.playerNumber != 0 && !team.firePressed) {
    player.tacklingTimer = -player.tacklingTimer
    if (player.tacklingTimer >= -2) player.tacklingTimer = -1
        /* `cmpi.w #$FFFE / blt` skips only when STRICTLY below -2, so -2
           folds to -1 as well */
}

if (player.speed == 0) -> finalize
player.speed -= kPlayerGroundConstant (96)
if (player.speed <= 0) { player.speed = 0; SetPlayerDowntimeAfterTackle(); finalize }

/* off-pitch damping */
if (x < 73 || x > 598)                     -> speed -= speed>>3 ; finalize
if (y < 129 || y > 769) {
    if (x in [275,396]) { speed >>= 4 ; speed |= 1 ; finalize }   /* into the goal */
        /* FOUR `lsr`s, /16 (SWOS2.s:30738-30741). The asm comment on the `ori`
           says "/= 32" and is wrong in both releases' listings; revisions
           before 2.0 said >>= 5, copying a port that followed the comment. */
    if (y < 121 || y > 777) -> speed -= speed>>3
    finalize
}

/* on the pitch: did the slide reach the ball? */
if (live && !player.tackleState && player.ballDistance <= 64
    && (team.ballLessEqual4 || team.ball4To8))
{
    lastTeamPlayed = team ; lastPlayerPlayed = player
    penalty = 0 ; playerHadBall = opponent.playerHasBall ? 1 : 0
    if (player.tacklingTimer == -1) PlayerTackledTheBallWeak(player, team)
    else                            PlayerTackledTheBallStrong(player, team)
    team.lastHeadingTacklingPlayer = player
    team.passKickTimer = 25
    opponent.passingToPlayer = 0 ; opponent.passingKickingPlayer = NULL
    opponent.spinTimer = -1 ; opponent.wonTheBallTimer = 12
    finalize
}

/* missed the ball but still fast -> may be a foul */
if (player.speed >= 512) PlayerTacklingTestFoul(player, team)      /* §14.2 */
```

`tacklingTimer` semantics: `0` at launch, then incremented every update while it
is still `>= 0` — for **both** human and CPU tacklers. Only a *human* team
(`playerNumber != 0`) that has released fire negates it, folding `-1` and `-2`
to `-1`. So `-1` means exactly one thing: **a human released fire within the
first two updates after launching** → weak tackle. Everything else — a human
still holding, a human who released later, and *every CPU tackle* — is a strong
tackle.

### 8.7 Recovery states

```
PL_TACKLED:
    if (--playerDownTimer == 0) {
        if (player.injuryLevel != 0) {
            player.speed = 0
            player.playerState = PL_ROLLING_INJURED
            player.playerDownTimer = (int8)player.injuryLevel
            set injured animation
        } else -> PL_NORMAL, snap dest
    } else if (player.speed) {
        if ((y < 159 || y > 739) && x in [265,406]) speed -= speed>>2  /* in the box */
        speed -= kPlayerGroundConstant (96) ; clamp >= 0
    }

PL_ROLLING_INJURED:
    if (injuriesForever) -> finalize            /* debug mode: never recover */
    if (--playerDownTimer == 0) -> PL_NORMAL, snap dest

injury-forever tick (debug):
    player.speed = kInjuryForeverSpeed (768) ; player.destY -= 8
```

`injuryLevel` is the number of frames to lie down and comes from
`kInjuryRecoveryTime[severity]` (§14.3).

### 8.8 Idle players

Reached by everyone who is not controlled, not the receiver, not the keeper and
in `PL_NORMAL`.

```
if (substituteInProgress and this is the substituted player) -> common path

if (team.updatePlayerIndex != i) -> next player       /* round-robin gate */

/* post-match celebration (only at the moment the game ends) */
if (gameState == ST_GAME_ENDED && winningTeamPtr && player is idle PL_NORMAL
    && not the keeper && Rand() <= 64)                 /* 25.4% per eligible tick */
        playerState = (team.inGameTeamPtr == winningTeamPtr) ? PL_HAPPY : PL_SAD

COMMON PATH:
  /* during the first 25 frames of live play after a corner/free kick,
     leave everyone where the restart put them */
  if (live && inGameCounter <= 25 && breakState is CORNER_* or FREE_KICK_*)
        -> next player

  if (player.destReachedState == 1) player.destReachedState = 2   /* §18.3 */

  if (!team.controlledPlayer && team.playerNumber == 0)
        AI_SetControlsDirection(team)

  if (player == bookedPlayer)      -> walk to the referee (§19)
  if (substituteInProgress)        -> walk on/off (§8.10)
  if (player.sentAway)             -> finalize (parked off-pitch)

  if (playingPenalties)            -> use the penalties restart table
  else if (live) {
        SetPlayerWithNoBallDestination(player, team, ball.x, ball.y)   /* §17 */
        -> free-kick spacing check
  }
  else -> restart formation table (§18.4)
```

**Free-kick spacing** (applies whenever the ball is dead, the player is not a
keeper and this is the *defending* team):

```
d2 = (player.destX - foulX)^2 + (player.destY - foulY)^2
if (d2 <= 4225)                                  /* within 65 units */
        player.destX = (foulX >= 336) ? foulX - 70 : foulX + 70
```

This is the "wall must retreat" rule, applied to destinations rather than
positions.

### 8.9 The pass receiver

```
if (player.x outside [81,590] or player.y outside [129,769]) {
        team.passingToPlayer = 0 ; team.passingBall = 0 ; snap dest ; finalize
}

/* --- lead the receiver onto a curling pass (human teams only) --- */
if (team.playerNumber != 0
    && (team.longPass || team.longSpinPass || team.leftSpin || team.rightSpin)
    && ball.speed >= 512 && team.passingToPlayer)
{
        /* `tst.l TEAMGENERALINFO_LONGPASS` is a LONG test on a word field, so
           it spans longSpinPass (the next word) too — here, in the lead below,
           and in §9.1's jog test. Test the pair. */
    if (ball.direction < 0) { team.ballX/Y = player.x/y }
    else {
        diff = (uint8)(ball.fullDirection - player.fullDirection)
        if (diff > 64 && diff < 192) {                 /* ball going away  */
                team.passingToPlayer = 0 ; team.passingBall = 0
        } else if (diff <= 4 || diff >= 252) {         /* dead in line     */
                team.ballX/Y = player.x/y
        } else {                                       /* lead the runner  */
                lead = (team.longPass || team.longSpinPass) ? -32 : -64
                if ((int8)diff >= 0) lead = -lead
                idx = (((ball.fullDirection + lead + 4) & 0xFF) >> 3) << 1
                team.ballX = player.x + kBallFriction[idx]
                team.ballY = player.y + kBallFriction[idx + 1]
        }
    }
}

if (team.playerNumber == 0) AI_Kick(player, team)         /* §16.7 */
```

**Ball dead** → this is the player who will take the restart:

```
if (team != lastTeamPlayedBeforeBreak) -> finalize
if (gameState == ST_KEEPER_HOLDS_BALL && player is the keeper) -> take control
if (player.ballDistance != 0)          -> follow the ball (below)

TAKE CONTROL:
    if (cameraDirection >= 8) cameraDirection = player.direction
    player.direction   = cameraDirection
    team.controlledPlayer = team.passToPlayerPtr
    team.passToPlayerPtr  = NULL
    team.playerSwitchTimer = 25
    team.passingBall = team.passingToPlayer = team.shooting = 0
    ball.speed = 0
    lastTeamPlayed = team ; lastPlayerPlayed = player
    penalty = 0 ; playerHadBall = 0
    UpdatePlayerWithBall(player)             /* snap the player to the ball */
    team.passKickTimer = 0 ; team.passingKickingPlayer = NULL
    if (gameState is a THROW_IN) {
        SetThrowInPlayerDestinationCoordinates(player)
        set "about to throw" animation ; playerState = PL_THROW_IN ; hideBall = 1
    }
```

**Live play**:

```
/* a receiver may also tackle */
if (not the keeper && team.fireThisFrame && d >= 0 && team.plNotFarFromBall
    && (ballLessEqual4 || ball4To8)
    && (controlledPlayer == NULL
        || controlledPlayer.ballDistance >= player.ballDistance))
        -> team.headerOrTackle = 1 ; PlayerBeginTackling(d, player, team)
           team.ballCanBeControlled = 0
           opponent.passingToPlayer = 0 ; opponent.passingKickingPlayer = NULL
           finalize
           /* the receiver's tackle differs from the controlled player's
              (§8.3-E) precisely by those two opponent writes */

/* first touch -> promote to controlled player */
if (team.ballLessEqual4
    && (team.plVeryCloseToBall
        || (ball.speed >= 1536 && team.plCloseToBall)))
{
    team.controlledPlayer = team.passToPlayerPtr
    team.passToPlayerPtr  = NULL
    team.playerSwitchTimer = 25
    team.passingBall = team.passingToPlayer = team.shooting = 0
    player.dest = player.pos
    if (keeper) UpdateBallWithControllingGoalkeeper(player)
    else        UpdateControllingPlayer(player)
    team.passKickTimer = 0 ; team.passingKickingPlayer = NULL
    if (keeper) { either flag goalkeeperPlaying, or GoalkeeperClaimedTheBall }
    lastTeamPlayed = team ; lastPlayerPlayed = player
    penalty = 0 ; playerHadBall = 0
    finalize
}

/* otherwise chase */
if (!team.passingBall) {
    c = team.controlledPlayer
    if (c && c.ballDistance <= 3200 && c.ballDistance <= player.ballDistance) {
        if (player.ballDistance <= 3200) { player.dest = player.pos ; finalize }
    } else team.passingBall = 1
}
player.dest = (team.ballX, team.ballY)

/* keepers must not leave their box while chasing */
if (live && keeper && !team.goalkeeperPlaying) {
    inBox = bottom team ? (y >= 692 && x in [203,468])
                        : (y <= 206 && x in [203,468])
    if (!inBox) { team.passToPlayerPtr = NULL; passingBall = passingToPlayer = 0;
                  snap dest }
}

/* after a goal kick, outfielders keep out of the box */
if (goalOut && not the keeper && x in [183,488] && (y <= 226 || y >= 672))
        snap dest
```

### 8.10 Substitutions on the pitch

```
if (substituteInProgress < 0) {            /* incoming player still walking  */
    if (player == substitutedPlSprite) {
        player.speed = kSubstitutedPlayerSpeed (1536)
        if (player.deltaX == 0 && player.deltaY == 0) substituteInProgress = 0
    }
    -> the IDLE-TARGET path (the destination-choosing code of §8.8)
        /* NOT a fall-through to finalise, and this is the whole answer to
           "where does the incoming player run?" — phase -1 hands EVERY player
           (substituted or not) to the ordinary out-of-play / tactical
           destination code, which overwrites destX/destY with a real on-pitch
           target. The teleport below leaves dest holding phase 2's off-pitch
           (plComingX, plComingY), so a port that ends phase -1 at `finalize`
           instead sends the incoming player further AWAY from the pitch, where
           he arrives, stops, and the phase completes with him standing in the
           tunnel. Route phase -1 into the idle-target path. */
}
else if (player == substitutedPlSprite) {
    if (player.injuryLevel == -2)  -> teleport phase (below)
                                   /* DEAD: nothing ever writes -2 (§2.1) */
    if (player reached (substitutedPlDestX, substitutedPlDestY)) {
        if (substituteInProgress == 2) -> teleport phase
        substituteInProgress = 2
        substitutedPlDest = (plComingX, plComingY)      /* walk back on */
    }
    player.dest  = substitutedPlDest
    player.speed = kSubstitutedPlayerSpeed
}

teleport phase:
    substituteInProgress = -1
    player.pos     = (plSubstitutedX, plSubstitutedY)   /* the new player */
    player.speed   = kSubstitutedPlayerSpeed
    player.sentAway = 0
```

### 8.10a The bench and the substitution decision

`BenchCheckControls()` runs once per frame from the game loop
(`src/bench_subs.c:251`) and owns the whole substitution state machine; the
menu *drawing* is a separate pass (`DrawBenchAndSubsMenu`) and is out of scope
here. §8.10 is the tail of this system: the walk on and off the pitch.

**When the bench may be opened.** The gate list runs in this exact order and is
only reached while `inSubstitutesMenu == 0` (`bench_subs.c:287-319`):

```
if (inSubstitutesMenu != 0) -> the menu is already up: read the owning team's
                               stick and fall through to the menu handler.
                               (the `inSubstitutesMenu < 0` early-out is dead —
                                the variable only ever holds 0 or 30)

if (benchOff && cameraCoordinatesValid) benchOff = 0   /* vestigial, see below */

if (waitForPlayerToGoInTimer) { waitForPlayerToGoInTimer-- ; return }
if (cameraLeavingSubsTimer)   { cameraLeavingSubsTimer--   ; return }
if (substituteInProgress)     return          /* a sub is already walking      */
if (refState)                 return          /* referee sequence, §19         */
if (injuriesForever)          return          /* dead test flag, always 0      */
if (statsTimer)               return          /* stats overlay is up           */

/* these four do not just return — they RESET the tap gesture and clear
   bench1Called / bench2Called, so a press made during them is discarded: */
if (gameStatePl == ST_GAME_IN_PROGRESS)                  -> reset
if (whichCard)                                           -> reset
if (playingPenalties)                                    -> reset
if (gameState in [ST_STARTING_GAME .. ST_GAME_ENDED])    -> reset   /* 21..30 */
```

So the bench is available **only while the ball is dead**, outside a card
sequence, outside penalties, and outside every ceremonial state. It is not
available at all to a team with no human (`playerNumber == 0 &&
playerCoachNumber == 0`).

**Two triggers.**

1. `bench1Called` / `bench2Called` — set by the `W` / `Q` keys (§6.5) and
   checked *before* the alternation, so the keys work for either side at any
   time. `MainKeysCheck` refuses them during replay playback and for a
   CPU-only team (`src/game_loop.c:327`, `:341`). *(DOS mode also opens the
   bench from the second fire button, which the Amiga does not have —
   `src/match_step.c`.)*
2. The stick gesture, tested on **one team per frame**, selected by
   `frameCount & 1` exactly as §5.1 (`bench_subs.c:362`): odd frame = top team,
   even = bottom. *(DOS alternates on its own `benchPickTeamCounter`, which
   advances only when this check runs — a different phase, not a different
   constant.)* The input read is that team's already-decoded
   `TeamGeneralInfo.direction` / `.firePressed` if it has a joystick player;
   for a coach-only team it re-polls player 1's stick (or player 2's when
   `playerCoachNumber == 2`).

```
gesture (per team; bench_subs.c:398-456):
  direction held (>= 0):
      if (blockWhileHoldingDirection) return         /* one event per press */
      if (prevDirection >= 0) {
          if (dir != prevDirection) -> reset, return /* a different way = restart */
          tapCount++ ; timeoutCounter = 0
          if (tapCount == 2) -> OPEN THE BENCH
      }
      prevDirection = dir ; blockWhileHoldingDirection = 1
  stick centred:
      blockWhileHoldingDirection = 0
      if (++timeoutCounter == 15) -> reset (prevDirection = -1, tapCount = 0)
```

Because `tapCount` starts at 0 and the first press only records
`prevDirection`, this is **three taps of the same direction**, each release
shorter than 15 frames. Any of the eight directions works — it is not
specifically "down".

**Opening the bench** (`bench_subs.c:458-549`) sets:

```
benchTeam = the calling team ; benchTeamGame = its TeamGame
inSubstitutesMenu = 30 ; goToSubsTimer = 15    /* camera pan-in delay        */
subsState = 0 ; benchPlayerIndex = 0 ; plToBeSubstitutedPos = -1
subsBlockFire = subsBlockDirections = 1        /* swallow the opening press  */
timeVar = -timeVar ; resultTimer = -resultTimer ; statsTimer = -statsTimer
benchY / opponentBenchY = 389 / 485, swapped if bit 1 of the top team's
        in-game name[0] is set, and swapped again if the bench team is not the
        top team.  In training both become 456 (389+67 and 485-29 coincide, so
        the exg is moot) and subsTrainingTopTeam selects which half is drawn.
plComingY = plComingYTmp = 449 ; benchXLimit = 27
plLeavingX = 39 ; plComingX = 26
CheckForThrowInAndGoalkeepersBall()    /* cancel a pending throw-in pose */
```

The three timer negations are SWOS's "park a countdown by making it negative"
idiom, so the half-time / full-time result screens do not tick while the bench
is up; they are flipped back by the fire-press path in §8.3. The `statsTimer`
negation is a no-op — gate 7 above guarantees it is already 0.

**`subsState` — which sub-menu has focus** (only 0, 1, 2 and 4 are reachable;
nothing ever writes 3, so `state3HandleFirePress` and the `field_B` test in
`isPlayerOkToSubstitute` are dead code):

| value | meaning                                                            |
|-------|--------------------------------------------------------------------|
| `0`   | bench list. `benchPlayerIndex` 0 = the marking/formation entry, 1..5 = bench slots (squad slots 11..15). Left or right closes the bench. |
| `1`   | "who comes off" — `plToEnterGameIndex` is fixed, the user picks `plToBeSubstitutedOrd`/`Pos`. Fire commits. |
| `2`   | formation menu (`selectedFormationEntry`, 0..0x11); fire writes `benchTeam->tactics` and `pl1Tactics`/`pl2Tactics`, then `ApplyTeamTactics`. |
| `4`   | marking / swap-two-players menu. A fire held for exactly 8 frames toggles `markedPlayer` (§17.3); two short presses on two different rows swap their `PlayerInfo` **and** their 110-byte `Sprite` records (ordinals swapped back). |

Cursor movement in state 0 is blocked while `gameState == ST_KEEPER_HOLDS_BALL`
(`bench_subs.c:724`, `:742`) unless this is a training game — so with the keeper
holding the ball the bench opens but only the marking/formation entry is
reachable.

**`substituteInProgress` — the walk-on/walk-off state machine.** The value
names the phase; §8.10 is the consumer, `bench_subs.c` is the producer.

| value | set by | meaning |
|-------|--------|---------|
| `0`  | `InitGameRestoreTeams` (§23) and §8.10 when the sprite finally stops | idle — no substitution in flight |
| `1`  | `selectPlayerToSubstituteMenuHandler` on the confirming fire press (`bench_subs.c:925`) | the outgoing player is walking to the touchline exit `(plLeavingX, plComingYTmp) = (39, 449)` |
| `2`  | §8.10 when that destination is reached | he is walking the last stretch to the bench, `(plComingX, plComingY) = (26, 449)` |
| `-1` | §8.10 on the teleport (arrival while already `2`, **or** `injuryLevel == -2`) | the same sprite has been snapped to `(plSubstitutedX, plSubstitutedY) = (39, 449)` and is now the *incoming* player running on |

Supporting globals, all written by the commit press
(`bench_subs.c:954-962`) and all read by §8.10 and §20.1:

```
teamThatSubstitutes = benchTeam                       /* read by ball_physics.c:652 */
substitutedPlSprite = benchTeam->spritesTable[plToBeSubstitutedPos]
substitutedPlSprite->cards = 0                        /* recycle the sprite       */
substitutedPlX/Y    = his position at the moment of the press (recorded only)
substitutedPlDestX/Y= (39, 449)  -> re-pointed to (26, 449) when phase 2 starts
plSubstitutedX/Y    = (39, 449)  -> the teleport target
plToBeSubstitutedPos= squad slot (0..10) of the man coming off
plToBeSubstitutedOrd= his row in the tactics-ordered on-screen list
plToEnterGameIndex  = benchPlayerIndex + 10 -> squad slot 11..15
```

There is exactly **one** sprite involved: the man going off is re-used as the
man coming on. `kSubstitutedPlayerSpeed` is `0x600` (1536) for every leg.

`waitForPlayerToGoInTimer` (100), `cameraLeavingSubsTimer` (50),
`leavingSubsMenu` and `benchXLimit` (27) are the camera hand-off:

* `waitForPlayerToGoInTimer` is set by `substitutePlayer()` and counted down at
  the top of `BenchCheckControls`. While non-zero it holds the camera on the
  bench with margin 51 (§20.1) and freezes the restart state machine
  (`src/game_timers.c:290`).
* `cameraLeavingSubsTimer = 50` and `leavingSubsMenu = 1` are set by
  `leaveSubsMenu()` on every exit. `leavingSubsMenu` pins the camera to
  `(211, 449)` (§20.1) until `DrawBenchAndSubsMenu` sees the bench clear the
  left edge, then clears it.
* `benchXLimit` (27) is both the bench camera's target X (§20.1
  "bench camera" = `(benchXLimit, 449)`) and the draw gate: the bench is not
  drawn once `cameraX >= benchXLimit + 8`.
* `benchOff` is vestigial — set by `leaveSubsMenu`, cleared by the first
  `BenchCheckControls` with a valid camera, and read nowhere else.

**What the commit actually does to the squad.** Note the split: the *menu*
press only starts the walk and books the counters; the squad edit happens in
`substitutePlayer()` (`bench_subs.c:992`), which `BenchCheckControls` calls
once, on the first frame that `substituteInProgress` is negative:

```
on the fire press (subsState 1):
    substituteInProgress = 1
    teamN NumSubs++
    if (plToEnterGameIndex == 11) teamN GoalkeeperReplaced++
    subsState = 0 ; overlayActiveFlag = 0 ; subsBlockFire = 1

when the walk reaches -1:
    sprite.injuryLevel = 0 ; sprite.sentAway = 0
    if (markedPlayer == outgoing slot) markedPlayer = -1
    swap plOrdinalsTable / plNamesLengthTableCopy entries (name sprites)
    PlayerInfo[outgoing].position = 0xFF          /* written BEFORE the swap */
    swap the two 61-byte PlayerInfo records (bench slot <-> pitch slot)
    InitializePlayerSpriteFrameIndices() ; ApplyTeamTactics(benchTeamGame)
    waitForPlayerToGoInTimer = 100 ; leaveSubsMenu()
```

Consequences a porter must reproduce:

* The swap is **by slot**, so the incoming player inherits the outgoing man's
  pitch slot, sprite and tactics position; the outgoing man ends up in the
  bench slot carrying `position = 0xFF` — he can never come back on. Note
  *where* he is refused: not by `isThisPlayerOkToSelect` (`bench_subs.c:809`),
  whose negative-position path returns the slot's byte offset
  `(benchPlayerIndex + 10) * 61` — positive, which every caller reads as
  "selectable" — so the cursor **does** stop on him; it is the fire press
  (`firePressedInSubsMenu`, `position < 0 -> return`) that refuses him. Its
  other refusals (`skip`, two yellows, the substitution limit) return −1 and
  make the cursor skip the slot. *(Revisions before 2.0 said
  `isThisPlayerOkToSelect` rejects him; a port that does that moves the cursor
  differently from the original — the replay rig caught it, docs/TESTS.md.)*
* `markedPlayer` indexes the team's own slots. Substituting the marked player
  clears the mark to `-1`; any other substitution leaves it pointing at the
  same slot (now occupied by the incoming player). The second test in
  `substitutePlayer` (`markedPlayer == plToEnterGameIndex`, i.e. 11..15) can
  never fire, because the marking menu only ever stores 0..10.
* **`PlayerInfo.skip` is NOT a substitution flag despite its comments.** It is
  set once, at match init (`src/game_init.c:438,447,453`): 1 for a bench slot
  whose player is injured/suspended (`IsPlayerInjured`) or whose index is
  `>= gameMaxSubstitutes + 11`. It is read by `isThisPlayerOkToSelect`, by the
  bench draw, and by the three post-match write-back loops
  (`game_init.c:334, 504, 559`), which skip those records. A substitution never
  writes it — the outgoing player is marked with `position = 0xFF` instead, and
  is still written back post-match.
* Goalkeeper replacement: bench slot 11 is the reserve keeper. Committing him
  bumps `team1GoalkeeperReplaced` / `team2GoalkeeperReplaced` (used only by the
  bench drawing to keep the GK jersey sprite). When `gameMaxSubstitutes > 5`,
  picking slot 11 forces `plToBeSubstitutedPos = 0` (the keeper), skips
  `findInitialPlayerToBeSubstituted` (`bench_subs.c:872`) and locks the
  selection cursor on him (`cmpi.w #5,(gameMaxSubstitutes) / bhi exit`,
  `bench_subs.c:1261`). When `gameMaxSubstitutes <= 5` — every real game — the
  reserve keeper is an ordinary substitution: the man coming off is pre-selected
  by position as below and the cursor moves freely. *(Revisions before 2.0 had
  the lock on the `<= 5` side, copying a port bug that froze the cursor for
  every goalkeeper substitution.)*
  *(`gameMaxSubstitutes > 5` is unreachable from every menu and from all 89
  shipped presets, which clamp 0..5; only a hand-edited `.HIL` header can produce
  it. Implement the branch or not, as you prefer.)*

**The two limit variables have misleading names.** They come from
`InitializeInGameTeamsAndStartGame(d1 = minSubs, d2 = maxSubs)`
(`src/game_init.c:1040`):

```
maxSubsPerTeam     = the menu's MIN SUBSTITUTES  = how many substitutions a team may MAKE
gameMaxSubstitutes = the menu's MAX SUBSTITUTES  = how many players sit ON THE BENCH (0..5)
```

`team1NumSubs` / `team2NumSubs` count substitutions made and are zeroed at
match setup alongside `substituteInProgress` (`src/match_setup.c:939, 957`). Once
`teamNumSubs == maxSubsPerTeam`, `isThisPlayerOkToSelect` returns −1 for
**every** bench entry, so the cursor's scan-forward-then-scan-back loop
(`bench_subs.c:723-760`) walks to slot 6, back down, and parks on entry 0.
The bench still opens; the user can still change formation and marking; no
bench player can be reached. There is no message and no separate "limit"
branch — the cursor simply cannot leave entry 0.

**Injuries do not force or bias a substitution.** `PlayerTackled` (§14.3) puts
the victim on the ground for 60..130 frames and sets
`PlayerInfo.isInjured` / `injuriesBitfield`, which costs him speed for the rest
of the match (`kInjuriesSpeedHandicap`, `src/ball_possession.c:671`) — but
nothing opens the bench, nothing pre-selects him, and he plays on.
`findInitialPlayerToBeSubstituted` (`bench_subs.c:1408`) picks the man to come
off by **position only**: exact position match > positional group
(DEF/RB/LB, or MID/RW/LW) > first player with fewer than 2 yellows. It never
looks at injury state. `PlayerInfo.position` is:

| Value | Role         | Group        |
|-------|--------------|--------------|
| 0     | Goalkeeper   | —            |
| 1     | Right back   | defensive    |
| 2     | Left back    | defensive    |
| 3     | Defender     | defensive    |
| 4     | Right wing   | midfield     |
| 5     | Left wing    | midfield     |
| 6     | Midfielder   | midfield     |
| 7     | Attacker     | —            |

Only those two groups exist. A goalkeeper or an attacker coming on gets **no**
group tier — if no player on the pitch has the identical `position`, the choice
drops straight to "first man with fewer than 2 yellow cards", which is normally
the goalkeeper. The scan is a single pass over the 11 on-field slots recording
all three candidates at once, each latched on its first hit, so every tier
resolves to the **lowest slot index** that qualifies. The one place the engine *would* respond to an injury —
`substitutedPlSprite->injuryLevel == -2`, which short-circuits the walk
straight to the teleport (§8.10) and draws a red cross beside the player in the
subs menu (`bench_subs.c:2136`) — is unreachable: no code path in SWOS2.s or in
this port ever writes `-2` to `Sprite.injuryLevel` (the only writers are `clr.w`
and the `kInjuryRecoveryTime` lookup). Treat it as a designed-but-unused hook.

---

### 8.11 Finalise (runs for every player, every state)

```
UpdatePlayerSpeed(player, team)                        /* §9.1 */

dir = CalculateDeltaXAndY(player.dest, player.pos, player.speed,
                          &player.deltaX, &player.deltaY)

/* facing */
if (keeper && live && !team.goalkeeperPlaying
    && (ballInUpperPenaltyArea || ballInLowerPenaltyArea))
        dir = (player.fullDirection + 128) & 0xFF      /* always face the ball */
else if (dir < 0) {
        if (player == team.controlledPlayer
         || player == team.passingKickingPlayer)
                -> leave player.direction untouched AND skip the quantise below
        else dir = (player.fullDirection + 128) & 0xFF
}
player.direction = ((dir + 16) & 0xFF) >> 5   /* 8-way; skipped on the path above —
                                                 quantising dir == -1 would snap a
                                                 stationary player to face North */

/* angle to the ball (or to the foul spot while dead) */
target = (live || gameState > ST_THROW_IN_BACK_LEFT) ? ball.pos : foul.pos
player.fullDirection = CalculateDeltaXAndY(player.pos, target, 256).dir   /* 0 if -1 */

/* animation */
if (player.playerState == PL_NORMAL) {
    nowMoving = (deltaX | deltaY) != 0
    if (nowMoving != (player.isMoving != 0) || player.playerDirection != player.direction)
            SetPlayerAnimationTable(nowMoving ? running : standing, player)
}
```

Note the argument order in the `fullDirection` call: it is
`CalculateDeltaXAndY(dest = player, src = target)`, i.e. the angle **from the
ball to the player**, which is why the facing correction adds 128 (180°).

### 8.12 Post-loop: goalkeeper possession tracking

```
if (live or gameState == ST_KEEPER_HOLDS_BALL) {
    if (lastPlayerPlayed is a goalkeeper sprite and it was not last frame) {
        lastPlayerBeforeGoalkeeper = prevLastPlayer
        lastTeamScored             = prevLastTeamPlayed
        nobodysBallTimer           = 50
    }
}
```

This preserves "who last touched it before the keeper picked it up" so a
deflection into the keeper's hands is still credited correctly.

---

## 9. Player movement and animation

`MovePlayers()` runs once per frame for **all 22 players** (both teams, unlike
`UpdatePlayers`). For each: step the animation, then integrate position.

### 9.1 `UpdatePlayerSpeed` — how fast a player runs

Called at the top of the finalise step for every player.

```
if (player.playerState != PL_NORMAL) return          /* special states own speed */
if (live && keeper && player != team.controlledPlayer) return   /* keepers frozen */

sk = PlayerInfo.speed                                 /* 0..7 */
player.speed = live ? kPlayerSpeedsGameInProgress[sk]  /*  928..1250 */
                    : kPlayerSpeedsGameStopped[sk]     /* 1136..1248 */

if (runSlower)  player.speed -= (speed>>2) + ((speed>>2)>>1)     /* ×0.625 */

/* injuries only slow human/coached teams */
if (team.playerNumber || team.playerCoachNumber) {
    inj = PlayerInfo.injuriesBitfield
    if (inj) player.speed += kInjuriesSpeedHandicap[((inj & 0xE0) >> 4) / 2]
}

/* carrying the ball costs 12.5% */
if (player == team.controlledPlayer && team.playerHasBall)
        player.speed -= player.speed >> 3

/* a human's pass receiver running onto a curling ball is slowed to a jog
   so the ball can catch up */
if (team.playerNumber && player == team.passToPlayerPtr && team.passingToPlayer
    && (team.longPass || team.longSpinPass || team.leftSpin || team.rightSpin)
    && ball.speed) {
    diff = (int8)(ball.fullDirection - (int8)player.fullDirection)
    if (diff in [-7,7])  player.speed = (|diff| > 5) ? 512 : 256
}

/* ball dead */
if (gameState == ST_FIRST_HALF_ENDED || ST_GAME_ENDED)
        player.speed -= stoppageTimerTotal * 32 ; clamp >= 0     /* wind down */
else if (gameState == ST_GOING_TO_HALFTIME || ST_PLAYERS_GOING_TO_SHOWER) {
        f = min(stoppageTimerTotal * 4, 100)
        player.speed = player.speed * f / 100                    /* wind up   */
}

/* animation cadence follows speed */
player.frameDelay = max(0, (1280 - player.speed)) >> 7  +  6
```

`runSlower` is set to 1 by `GoalScored` and cleared by the restart machine in
`breakCameraMode` state 3 — i.e. once all 22 players have reached their kick-off
positions — so it applies a flat **37.5 % speed cut to every player on both
teams** for the whole goal celebration and walk back. It is zeroed per match by
`InitTeamsData`. Omitting it makes the post-goal reset noticeably too fast.

`frameDelay` links stride rate to speed: at `speed = 1250` it is 6, at
`speed = 256` it is 14.

### 9.2 Integration

```
movePlayer(s):
    if (!s.onScreen && live) { integrate only, no animation switch }

    for axis in {x, y}:
        d = s.delta[axis].raw
        if (d != 0) {
            s.pos[axis].raw += d                       /* full 16.16 add   */
            if (d < 0 ? s.pos[axis].i <= s.dest[axis]
                      : s.pos[axis].i >= s.dest[axis]) {
                s.pos[axis].i     = s.dest[axis]       /* snap, keep frac  */
                s.delta[axis].raw = 0                  /* and stop         */
            }
        }

    if (s.playerState != PL_NORMAL) return
    if (s.deltaX || s.deltaY) return                   /* still moving     */

    /* just arrived */
    if (!live && breakCameraMode == 3 && s.destReachedState == 2)
            s.destReachedState = 3                     /* §18.3 handshake  */
    if (s.animationTable != standing) SetPlayerAnimationTable(standing, s)
```

Two subtleties worth preserving:

* The clamp writes only the **integer half** of the position and leaves the
  fraction untouched. Overwriting the fraction produces visible micro-jitter
  when a player parks on a destination.
* Because the destination for a moving player is 1000 units away (§8.3-G) the
  clamp essentially never fires during normal running; it fires for restarts,
  tackles and set-piece formations, where the destination is a real point.

### 9.3 Animation walker

Animation is data-driven and gameplay reads two of its outputs
(`frameSwitchCounter` for header/keeper timing, `imageIndex` for the keeper's
sprite-width overlap test), so the walker must be ported even in a project with
completely different rendering.

An `AnimTable` is `{ int16 frameDelay; int16 *animFrames[32]; }`. The slot is
chosen by team and role:

```
slot = (teamNumber - 1) + (playerOrdinal == 1 ? 2 : 0)      /* 0..3 */
slot = slot * 8 + direction                                  /* 0..31 */
frameIndicesTable = animTable.animFrames[slot]
```

so there are four sprite sets (team 1 outfield, team 2 outfield, team 1 keeper,
team 2 keeper) × 8 directions.

A frame list is a `int16_t[]` of **commands**:

```
>= 0        emit this image index (plus Sprite.frameOffset); frameSwitchCounter++
            During a goal celebration a further +24 selects the arms-raised
            variant: the scorer while (tick & 0x7F) <= 100, his team-mates on a
            staggered ((ordinal << 2) + tick) & 63 <= 31 flash — keepers
            excluded, and only for direction 0 or 4.
-999        rewind to index 0 and continue decoding
-101        step back one index and stop (hold on the previous frame)
<= -100     relative jump: frameIndex += (value + 100); continue decoding
-1 .. -99   set frameDelay = -value AND reload cycleFramesTimer with it,
            advance one index, continue decoding
```

Per frame, for each sprite:

```
setNextPlayerFrame(s):
    if (!s.onScreen) return                    /* off-camera anims freeze   */
    if (--s.cycleFramesTimer != 0) return
    s.frameIndex++
    s.cycleFramesTimer = s.frameDelay
    decode commands from s.frameIndicesTable[s.frameIndex] as above
```

`SetPlayerAnimationTable` resets the walker (`frameSwitchCounter = -1`,
`frameIndex = -1`, `cycleFramesTimer = 1`, records `startingDirection`).
`SetPlayerAnimationTableAndPictureIndex` swaps the table **without** resetting
the cursor and immediately re-emits the current frame — used mid-header and
mid-keeper-dive so the pose changes without restarting the sequence.

The ball has its own two-entry frame tables and a speed-scaled cadence
(§10.1).

### 9.4 Which animations gameplay knows by name

Animation tables are *data*, but five of them are also *identities*: gameplay
compares `Sprite.animationTable` against them and changes behaviour. Those five
must exist as distinguishable objects in a port; the rest are assigned but never
tested, so a port may substitute its own artwork and its own frame lists freely.

All 33 tables and the 266 frame lists they point at are in `src/sprite_data.c`.
The slot layout of a 32-slot table is
`[0-7] team-1 outfield · [8-15] team-2 outfield · [16-23] team-1 keeper · [24-31] team-2 keeper`,
each run being the 8 directions in the §3.3 order (up, up-right, right, down-right,
down, down-left, left, up-left). Unused slots are `0` and reaching one is
`panicReasonCode = 0x000E`.

**Tested by identity — a port MUST be able to distinguish these five:**

| Identifier | Tested where, and what changes |
|---|---|
| `playerNormalStandingAnimTable` | `movePlayer` (§9.2): a `PL_NORMAL` player who has stopped is switched to it *unless he is already in it* — the guard is what stops the walker being re-cursored every frame while standing still. |
| `jumpHeaderAttemptAnimTable` | §8.5: `airborneWindow = (anim == jumpHeaderAttempt) ? 37 : 17` — the frame at which a jump header stops coasting and starts halving its speed. |
| `jumpHeaderHitAnimTable` | §13.1 `SetJumpHeaderHitAnimTable` returns immediately if already set, so the pose is applied once per jump and the accompanying `speed >>= 1` cannot repeat. |
| `staticHeaderAttemptAnimTable` | §13.2 `SetStaticHeaderDirection` returns unless the player is still in it — and since the same call replaces it with `staticHeaderAimAnimTable`, this is what makes static-header steering a **one-shot** ±1 step. |
| `staticHeaderHitAnimTable` | §13.2 `PlayerHittingStaticHeader` skips its two-step turn if already set, so a second contact in the same jump does not rotate the player again. |

Two more identity tests are *not* on `animationTable`:

* **The ball** is switched between `ballStaticFrameIndices` and
  `ballMovingFrameIndices` by comparing `Sprite.frameIndicesTable` directly
  (§10.1, `src/ball_physics.c:541, 547`), resetting `frameIndex = 0` only on a
  change. Those two lists are in `src/swos_amiga_data_imports.c`, not
  `sprite_data.c`.
* **The keeper's "caught ball" pose** (§15.5) is detected by **`imageIndex`**, not
  by a table: `imageIndex ∈ {0x35E, 0x360, 0x3E0, 0x3E2, 0x3D2, 0x3D4, 0x36C, 0x36E}`
  — i.e. the four named `SPR_GOALIE{1,2}_CAUGHT_BALL_{RIGHT,LEFT}` constants
  (`0x35E/0x360/0x3E0/0x3E2`) plus four unnamed literals `0x3D2, 0x3D4, 0x36C,
  0x36E` — selects `kGoalKeeperRecoveryStepAlt (192)` instead of the
  base 112 (`src/update_players.c:1032-1046`). *(Revisions before 2.0 gave the
  port's old, wrong names `0x3C4/0x3C6/0x3D0/0x3D2` here; see §25.4.)* A port that renumbers its sprites
  must re-derive this set; a port that keeps only the *table* identity cannot
  reproduce it, because `goalieCatchingBallAnimTable` is not what is being tested.

**Assigned only — substitutable.** Nothing reads these back; a port may use any
frame data it likes, provided the *state* that assigns them is right.

| Identifier | Assigned when |
|---|---|
| `playerRunningAnimTable` | a `PL_NORMAL` player starts moving (§8.3) |
| `aboutToThrowInAnimTable` | throw-in wind-up (§8.4) and the restart hand-over, state 7 (§18.3) — the same state that gives everyone else `playerNormalStandingAnimTable` |
| `throwInPassAnimTable` / `throwInKickAnimTable` | short vs long throw release (§8.4) |
| `staticHeaderAimAnimTable` | the aim pose set by `SetStaticHeaderDirection` (§13.2) |
| `plTacklingAnimTable` | tackle launch (§14.1) |
| `playerTackledAnimTable` | the tackled player going down (§14.1/§14.3) — **20-slot** table, see the size note in §9.3 |
| `plInjuredAnimTable` | injury (§14.3) |
| `plGettingYellowCardAnimTable`, `plGettingRedCardAnimTable`, `plGetting2ndYellowCardAnimTable` | the booked player on arrival at the referee (§19) |
| `playerWinningReactionAnimTable`, `playerLosingReactionAnimTable` | full-time reactions (§21.4) |
| `goalieCatchingBallAnimTable` | keeper catch/claim (§15.4) — keeper slots 16-31 only |
| `leftGoalieJumpingHighAnimTable`, `rightGoalieJumpingHighAnimTable`, `leftGoalieJumpingLowAnimTable`, `rightGoalieJumpingLowAnimTable` | the four dive poses (§15.3); "left/right" is the *facing*, "high/low" the dive height |
| `refComingAnimTable` (8 slots), `refWaitingAnimTable` (12 slots), `refYellowCardAnimTable`, `refRedCardAnimTable`, `refSecondYellowAnimTable` (8 slots each) | §19; indexed by **direction only** — `animFrames[direction]`, no team/role term |

**Defined but never referenced** in the ported code: `goalieGoalKickAnimTable`,
`goalieDiveHighHoldAnimTable`, `goalieDiveLandAnimTable`,
`goalieDeflectBallAnimTable`, `plTacklingAnimTable_2`. They are transcribed from
the original data and declared in `include/sprite_data.h`, but no call site
assigns them. A port can omit them.

---

## 10. Ball physics

`UpdateBall()` runs once per frame, after that frame's team-control pass (one
team per frame — §5.1). This is the single function that most defines how SWOS
feels.

### 10.1 Ball animation

```
if (hideBall) { ball.imageIndex = -1 ; shadow.imageIndex = -1 ; skip to movement }
shadow.imageIndex = SPR_BALL_SHADOW

table = (ball.deltaX || ball.deltaY) ? ballMovingFrameIndices    /* 4 frames */
                                     : ballStaticFrameIndices    /* 1 frame  */
if (table changed) { ball.frameIndicesTable = table ; ball.frameIndex = 0 }

if (ball.speed) {
    ball.cycleFramesTimer -= (ball.speed >> 9) + 1     /* faster ball spins faster */
    if (ball.cycleFramesTimer < 0) {
        ball.frameIndex++
        ball.cycleFramesTimer = ball.frameDelay
        emit frame (wrapping to 0 on the -1 terminator)
    }
}
else if (ball.imageIndex < 0) emit frame
    /* if the timer did not expire, or the ball is stopped, the frame is still
       emitted when the ball currently has NO picture — this is what restores
       the sprite after hideBall cleared imageIndex to -1 */
```

### 10.2 Direction, friction and integration

```
savedX = ball.x.raw ; savedY = ball.y.raw ; savedZ = ball.z.raw

dir = CalculateDeltaXAndY(ball.dest, ball.pos, ball.speed,
                          &ball.deltaX, &ball.deltaY)
if (dir >= 0) { ball.fullDirection = dir ; dir8 = ((dir + 16) & 0xFF) >> 5 }
ball.direction = (dir >= 0) ? dir8 : -1

/* friction */
f = kBallGroundConstant                       /* 16 (DOS 13) */
if (!topTeam.playerHasBall && !bottomTeam.playerHasBall)
        f += pitchBallSpeedFactor             /* pitch condition, §24.5 */
if (ball.z != 0)
        f = kBallAirConstant                  /* 10 (DOS 4) — overrides */
ball.speed = max(0, ball.speed - f)

ball.x.raw += ball.deltaX.raw
ball.y.raw += ball.deltaY.raw
```

Two things to note. Friction is subtracted from *speed*, and `speed` is
re-projected into `deltaX/deltaY` at the **start** of the next frame — so the
ball always travels exactly toward `destX/destY`, and "curving" the ball is done
by moving the destination (§12.5), never by rotating a velocity vector. And
while a player has the ball, the pitch-condition term is dropped, so dribbling
feels identical on every surface.

### 10.3 Gravity and bouncing

```
dz = ball.deltaZ.raw

if (gameState == ST_KEEPER_HOLDS_BALL) {
    /* the keeper is holding it: bob the ball between z=0 and z=5 */
    if (a substitution is walking on for the team in possession
        and it has no controlled player) { ball.z = 0 ; dz = 0 }
    else {
        if (ball.speed) UpdateBallWithControllingGoalkeeper(team.controlledPlayer)
        if (ball.z == 5)      dz = 0
        else if (ball.z < 6)  dz =  0x00020004        /* rise */
        else                  dz = -0x00010002        /* fall (0xFFFEFFFE) */
    }
    ball.z.raw += dz
} else if (dz != 0) {
    dz -= kGravityConstant                 /* 4608 == 0.0703 units/frame^2 */
    dz |= 1                                /* never let it reach exactly 0  */
    ball.z.raw += dz

    if (ball.z.raw < 0) {                   /* hit the ground: bounce */
        ball.speed -= (ball.speed * ballSpeedBounceFactor) >> 8
        ball.z.i    = 0
        dz          = -dz
        dz         -= (uint16)(dz >> 8) * ballBounceFactor
        dz         |= 1
        if ((uint32)dz <= 0xA000) dz = 0                 /* settle */
        else                      play the bounce sample
    }
}
ball.deltaZ.raw = dz
```

`ballSpeedBounceFactor` and `ballBounceFactor` come from the pitch type
(§24.5): on a frozen pitch the ball keeps almost all its speed and most of its
bounce; on a muddy one it dies.

The `|= 1` on `dz` is essential — it stops the fixed-point value landing exactly
on zero and freezing the ball in the air.

### 10.4 Out-of-bounds containment while the ball is dead

When `gameStatePl != ST_GAME_IN_PROGRESS` the ball is not allowed to leave a
generous box; instead it reflects and halves its speed:

```
if (ball.x < 53 || ball.x > 618) {
    ball.destX = ball.x - (ball.destX - ball.x)     /* mirror the target */
    ball.speed >>= 1
    restore ball.x/y/z from savedX/savedY/savedZ
}
if (ball.y < 100 || ball.y > 799) { same on Y }
```

### 10.5 Goal frame collisions

Tested with `bx = ball.x - 1` (the original's 1-unit hot-spot bias).

**Inside the goal (behind the line)** — only when `by` is outside `[129, 769]`:

```
Top goal, by in [113,128], bz <= 19, bx in (295, 372]:
    barHeight = (by > 123) ? 15 : 10
    if (bz > barHeight)                     -> CROSSBAR
    else if (bx < 302 || bx > 366)          -> SIDE NET/POST
    else if (by < 119)                      -> BACK NET
Bottom goal, by in [770,784]: same with barHeight = 15 and back net at by > 778.
```

Reactions:

```
CROSSBAR:
    /* the discriminator is the ball's HEIGHT, not its speed */
    if (upper word of ball.z <= 15) {         /* LOW: drop dead on the bar  */
        ball.deltaZ = 1 ; ball.speed = 0 ; restore saved x/y/z
    } else {                                  /* HIGH: deflect over         */
        ball.deltaZ = 1
        ball.destY  = ball.y ∓ 1000           /* further BEHIND the goal    */
        ball.speed  = 512
        restore saved z
    }

BACK NET:
    mirror destY about the saved Y ; reset both spin timers
    ball.speed >>= 3 ; restore saved x/y

SIDE NET/POST:
    mirror destX about the saved X ; reset both spin timers
    ball.speed >>= 2 ; restore saved x/y
```

**The posts themselves**, only while live, when `by` is in `(128,132]` or
`[766,770)`:

```
if (bz > 19)                       -> no contact
if (bx <= 295 || bx > 372)         -> no contact (outside the post band)
if (bz <= 15 && bx in [302,366])   -> no contact (through the mouth)

jitter = ((currentGameTick & 0x1F) << 4) - 0x100          /* -256..+240 */
strong = (ball.deltaY < -0x5000 || ball.deltaY > 0x5000)  /* full 16.16 */

if (strong) {                      /* either height — loc_10984C */
    if (the ball is travelling away from this goal) -> no contact
                                   /* by <= 449: deltaY >= 0 ; else deltaY < 0 */
    mirror destY ; reset spin timers ; ball.destX += jitter
} else if (bz <= 15) {             /* the SIDE of a post — loc_109810 */
    mirror destY ; reset spin timers ; ball.destY += jitter
} else {                           /* level with the crossbar — loc_109878 */
    ball.deltaZ = -ball.deltaZ ; restore saved z ; savedY += 1.0
}

speed reduction:  play the woodwork sample
                  ball.speed -= ball.speed >> 2          /* ×0.75 */
                  restore saved x/y
```

Three arms, not two (SWOS2.s:14829-14869; the DOS listing is identical). The
jitter is the same expression in each; what separates them is *where* it is
added. Revisions before 2.0 routed a weak-Y ball at post height into the
crossbar arm, copying a port bug: a ball resting against the side of a post then
crept a pixel down the pitch every frame instead of rattling along the goal line
(found by the replay rig, docs/TESTS.md).

### 10.6 Out of play

```
if (live && (ball.x < 81 || ball.x > 590 || ball.y < 129 || ball.y > 769))
        CheckIfBallOutOfPlay()                 /* §18 */
```

### 10.7 Shadow

```
shadow.x = ball.x + (ball.z >> 1) + 1
shadow.y = ball.y + (ball.z >> 2) + 1 - 10
shadow.z = -10
```

(The shadow offset is what makes the ball's height readable; keep the ratio even
if your renderer differs.)

### 10.8 Ball quadrants (drives all off-ball positioning)

After the shadow, `UpdateBall` calls `CalculateNextBallPosition()` (§11.1) — so
`ballNextX/Y` are recomputed from this frame's position and velocity — and then,
as its last act, classifies the ball into a 5 × 7 grid used by the tactics system
(§17). Nothing else calls that predictor; run it anywhere else in the frame and
the whole formation is driven off a one-frame-stale prediction.

```
if (live || gameState == ST_FIRST_HALF_ENDED || gameState == ST_GAME_ENDED)
        (qx, qy) = (ballNextX, ballNextY)        /* the PREDICTED position */
else {
        (qx, qy) = (foulX, foulY)
        if (gameState is KEEPER_HOLDS_BALL or GOAL_OUT_*) (qx, qy) = (336, 449)
}

/* X: 4 thresholds -> 0..4.  `i` is declared OUTSIDE the loop and is read after
   it; on fall-through (no threshold matched) it holds 4. */
xq = 0 ; q = 0 ; i = 0
while (i < 4 && (uint16)qx >= (uint16)ballXQuadrantLimits[i]) { xq++; q++; i++ }
xRef = (i == 0) ? 81                             /* the pad word BEFORE the array */
                : ballXQuadrantLimits[min(i,4) - 1]
ballXQuadrantDead    = xq
playerXQuadrantOffset = ((qx - xRef - 51) * 5) / 15

/* Y: 6 thresholds -> 0..6, contributing 5 per step; `i` holds 6 on fall-through */
yq = 0 ; i = 0
while (i < 6 && (uint16)qy >= (uint16)ballYQuadrantLimits[i]) { yq++; q += 5; i++ }
yRef = (i == 0) ? 129                            /* the word BEFORE the array =
                                                   ballXQuadrantLimits[4] */
                : ballYQuadrantLimits[min(i,6) - 1]
ballYQuadrantDead     = yq
playerYQuadrantOffset = ((qy - yRef - 45) * 5) / 15

ballQuadrantIndex = q                        /* 0..34, = xq + 5*yq */
```

with

```
ballXQuadrantLimits[5] = { 183, 285, 387, 489, 129 }  /* only [0..3] are X
                                                         thresholds; [4] is the
                                                         Y loop's leading bound */
ballYQuadrantLimits[6] = { 220, 312, 403, 495, 586, 678 }
```

Both loops read their `i == 0` reference from the word *before* their own
table, by the original's `sub.w -4(a1)` with `a1` one past the base: for X that
is a pad word (`81`), for Y it is `ballXQuadrantLimits[4] = 129` — so the
"padding" 5th X entry is in fact the first Y bound (the DOS build spells it out:
`ballYQuadrantLimits dw 129, 220, ...`). Revisions before 2.0 used
`ballYQuadrantLimits[0] = 220` there, copying a port bug that put every
outfield player's target 30 units too far up the pitch whenever the ball was
in the top Y band.

Both threshold comparisons are **unsigned**. `ballNextX/Y` is unclamped, so a
hard shot can predict off-pitch: a negative predicted position reads as a huge
value, matches no threshold and lands in the *last* quadrant rather than the
first — flipping the tactical shape to the opposite end of the pitch at exactly
the moment the ball is cleared past a goal line.

`ballQuadrantIndex` (0..34) selects a formation cell; the two
`player*QuadrantOffset` values are the sub-cell interpolation that makes the
formation slide smoothly instead of snapping between 35 fixed shapes.

**The prediction, not the ball, drives positioning.** Players move to where the
ball *will land*, which is why SWOS defenders look like they read the game.

---

## 11. Ball prediction

Three separate predictors, all pure integrators over the same physics as §10.3
but without collision. They are cheap because they run at fixed-point step size
and terminate on a sign change.

### 11.1 `CalculateNextBallPosition` → `ballNextX/Y`

Where the ball will next touch the ground. Uses an adaptive step so a
high lob doesn't cost hundreds of iterations:

```
d5 = ball.x.raw ; d6 = ball.y.raw ; d7 = ball.z.raw
if (ball.speed == 0) -> store

d1 = ball.deltaX ; d2 = ball.deltaY ; d3 = ball.deltaZ ; d4 = kGravityConstant

scale = 8                                     /* default for a rising ball */
if (d3 <= 0) {
    if      (ball.z <= 20) scale = 1
    else if (ball.z <= 30) scale = 2
    else if (ball.z <= 35) scale = 4
    else                   scale = 8
}
d1 *= scale ; d2 *= scale ; d4 *= scale ; d7 /= scale
do { d5 += d1 ; d6 += d2 ; d3 -= d4 ; d7 += d3 } while (d7 >= 0)

store: ballNextX = d5 >> 16 ; ballNextY = d6 >> 16
```

Note the comparisons on `ball.z` are **unsigned**, so a negative `z` reads as a
huge value, fails all three tests and falls through to the `scale = 8` path.
(`d7 /= scale` is an arithmetic shift, so it rounds toward −∞ for a negative `z`,
not toward zero.)

### 11.2 `CalculateBallNextGroundXYPositions` → `ballNextGroundX` / `ballNextYGroundY`

(The Y global really is spelled `ballNextYGroundY` — the original's own inconsistency,
preserved here so the name is greppable. There is no `ballNextGroundY`.)

Used by the goalkeeper: where the ball crosses a *catchable* height band.
Returns `ballNextGroundX = -1` if there is no such point.

All of the working values are **raw 16.16**, and every one is initialised from the
ball before the gate:

```
d1 = ball.x.raw ; d2 = ball.y.raw ; d3 = ball.z.raw
d4 = ball.deltaX.raw ; d5 = ball.deltaY.raw
d6 = ball.deltaZ.raw ; d7 = kGravityConstant

if (|d5| <= 1.0 && |d4| <= 1.0) { ballNextGroundX = -1; return }
        /* the test is deltaY first, then deltaX, and both bounds are
           EXCLUSIVE-of-equal: `d5 < -0x10000 || d5 > 0x10000 || ...`, so exactly
           ±1.0 counts as "not moving". */

d0 = 20.0                                        /* trial height */
loop {
    d3 -= d0
    if (d3 >= 0) break                            /* ball is above d0 */
    if (d0 == 13.0) { ballNextGroundX = -1; return }
    d3 += d0 ; d0 -= 1.0
}
do { d1 += d4 ; d2 += d5 ; d6 -= d7 ; d3 += d6 } while (d3 >= 0)
d3 += d0                                          /* dead: d3 is not read again */
ballNextGroundX  = (int16_t)((uint32_t)d1 >> 16)
ballNextYGroundY = (int16_t)((uint32_t)d2 >> 16)
```

Note `d3` is decremented **in place** by the trial loop (`d3 -= d0`, then `d3 += d0`
to undo on a failed trial), so on entry to the integrator `d3` is
`ball.z - d0` — the height *above the trial plane*, not the absolute height. The
integrator then runs until that residual goes negative, i.e. until the ball drops
through the trial plane. `d0` itself is never used again after the loop; the
trailing `d3 += d0` is a dead store.

So it answers "where does the ball drop through 13..20 units of height?" — the
window in which a keeper can take it cleanly.

### 11.3 `UpdateBallVariables` → defensive / not-high / strike points

Called with the keeper and the keeper's team. It samples the trajectory at three
Y (or X) planes and stores three points:

| Output               | Sample plane                                            |
|----------------------|---------------------------------------------------------|
| `ballDefensive{X,Y,Z}` | where the ball crosses the **keeper's own Y**         |
| `ballNotHigh{X,Y,Z}`   | where it crosses `y = 135` (top) / `y = 763` (bottom) |
| `strikeDest{X,Y,Z}`    | where it crosses `y = 129` (top) / `y = 769` (bottom) — the goal line |

```
if      (ball.deltaY < -1.0) { if (team != topTeam)    -> SAMPLE-AT-BALL (below)
                               else -> integrate toward the top goal (3 planes) }
else if (ball.deltaY >  1.0) { if (team != bottomTeam) -> SAMPLE-AT-BALL (below)
                               else -> integrate toward the bottom goal (3 planes) }
else if (ball.deltaX < -1.0) { if (ball.x <  keeper.x) -> SAMPLE-AT-BALL
                               else -> integrate to the keeper's X (leftwards) }
else if (ball.deltaX >  1.0) { if (ball.x >= keeper.x) -> SAMPLE-AT-BALL
                               else -> integrate to the keeper's X (rightwards) }
else                         -> SAMPLE-AT-BALL (below)

SAMPLE-AT-BALL:  ballDefensive* = the current ball position
/* shared tail, also reached by the two deltaX arms after they integrate: */
                 ballNotHigh*   = ballDefensive*
                 strikeDest*    = (0, 0, 0)      <-- ZEROED, not the ball position

/* The Y dispatch is decided BEFORE the team test: a fast vertical ball for the
   WRONG keeper does NOT fall through to the deltaX cases — it goes straight to
   "current ball position". The two deltaX cases produce only ballDefensive*;
   ballNotHigh* is copied from it and strikeDest* is zeroed. */

each integration:
    do { d6 -= gravity ; d3 += d6 ; d1 += d4 ; d2 += d5 }
    while (the plane has not been crossed)
    d1 -= d4 ; d3 -= d6            /* back off one step in X and Z */
    clamp z to >= 0, store
```

**There is no ground and no z termination.** The loop condition tests only the
plane coordinate (`d2` for the three Y planes, `d1` for the two keeper-X arms);
`d3` (z) is allowed to go arbitrarily negative and is clamped to `0` only at the
*store*. A ball that would physically land short of `y = 129` therefore still
produces a crossing point — the integrator flies it underground until it reaches
the plane. Adding a "stop when `z <= 0`" test is the single easiest way to get
plausible-looking but wrong answers out of this routine: it silently converts
"shot on target" into "not on target" for any dipping shot, with no error
anywhere. Termination is guaranteed without it, because these arms are only
entered when the relevant delta exceeds ±1.0 in the direction of the plane, so
`d2`/`d1` advances by at least 1.0 per iteration.

**The three stages chain, and only the first undoes `d2`.** `d1`, `d2`, `d3` and
`d6` carry forward from one plane to the next — each stage subtracts its plane
constant from the running `d2`, integrates, and then either restores the constant
(the "already past" arms, `d2 += const`) or overwrites `d2` with the constant
outright (the crossing arms). Only the *keeper-Y* stage does `d2 -= d5` in its
back-off; the `135`/`763` and `129`/`769` stages skip it because they discard
`d2` anyway. Getting the chaining wrong makes `ballNotHigh*` and `strikeDest*`
disagree with `ballDefensive*` in a way that only shows up on dipping shots.

If the ball never reaches the goal-line plane — meaning it is *already past* it
when the stage is entered — `strikeDest*` is set to `(0,0,0)`, which the keeper
logic tests as "not a shot on target" (`strikeDestX < 295 || > 376`). The
`135`/`763` stage handles the same case by copying `ballDefensive*` instead of
zeroing.

---

## 12. Possession, passing, shooting, aftertouch

### 12.1 Sticking the ball to a player

Three tiny helpers do all the "ball at your feet" work:

```
UpdateControllingPlayer(player):          /* player keeps the ball */
    p = player.pos + kBallPlOffsets[player.direction]     /* ±1 unit ahead */
    ball.speed = 0 ; ball.pos = p ; ball.dest = p ; ball.z = 0
    ball.deltaZ >>= 1
    ResetBothTeamSpinTimers()

UpdateBallWithControllingGoalkeeper(player):   /* same, but force the ball down */
    ... as above but ball.z is NOT zeroed, and
    ball.deltaZ >>= 1 ; if (ball.deltaZ >= 0) ball.deltaZ = -ball.deltaZ

UpdatePlayerWithBall(player):             /* move the PLAYER onto the ball */
    p = ball.pos + kPlayerWithBallOffsets[player.direction]
    player.pos = p ; player.dest = p
    ResetBothTeamSpinTimers()
```

`kBallPlOffsets` puts the ball 1 unit in front of the player;
`kPlayerWithBallOffsets` is its negation (§24.2).

`ResetBothTeamSpinTimers()` sets both teams' `spinTimer` to `-1`, closing the
aftertouch window. Any event that "re-grabs" the ball cancels aftertouch.

### 12.2 `DoPass` — a tap of fire

```
dir = player.direction
team.controlledPlDirection = dir
me     = PlayerInfo of the passer
target = GetClosestNonControlledPlayerInDirection(dir, team)

if (target found) {
    team.passToPlayerPtr = target
    team.passingBall = team.passingToPlayer = 1

    /* CPU teams can miscue */
    if (live && team.playerNumber == 0) {
        if (((currentGameTick & 0x1E) >> 1) < kAIFailedPassChance[me.passing]) {
            /* botched: aim at a point between the ball and the target,
               halving one axis depending on bit 5 of the tick */
            dx = target.x - ball.x ; if (!(tick & 0x20)) dx >>= 1
            ball.destX = ball.x + (dx << 5)
            dy = target.y - ball.y ; if  (tick & 0x20)   dy >>= 1
            ball.destY = ball.y + (dy << 5)
            goto set_speed
        }
    }

    /* aim: scale (target - ball) up until the destination leaves the world,
       so the pass is a ray through the target, not a point at it  */
    d1 = target.x - ball.x ; d2 = target.y - ball.y
    if (d1 == 0 && d2 == 0) d1 = 1
    while (ball.x + d1 in [0,672) && ball.y + d2 in [0,880)) { d1 <<= 1 ; d2 <<= 1 }
    ball.dest = ball.pos + (d1, d2)

set_speed:
    dist = target.ballDistance                 /* squared */
    speed = kPassingSpeedCloserThan2500        if dist <   2500
          | kPassingSpeed_2500_10000           if dist <  10000
          | kPassingSpeed_10000_22500          if dist <  22500
          | kPassingSpeed_22500_40000          if dist <  40000
          | kPassingSpeed_40000_62500          if dist <  62500
          | kPassingSpeed_62500_90000          if dist <  90000
          | kPassingSpeed_90000_122500         if dist < 122500
          | kPassingSpeedFurtherThan122500     otherwise
    ball.speed = speed + kBallSpeedPassingIncrease[me.passing]
} else {
    /* nobody in that direction: a free pass along the stick */
    tbl = GetBallDestCoordinatesTable()        /* set-piece aware, §12.6 */
    ball.dest  = ball.pos + tbl[dir]
    ball.speed = kFreePassReleasingBallSpeed   /* 1792 */
}

ResetBothTeamSpinTimers()
if (team.playerNumber) team.spinTimer = 0      /* open the aftertouch window */
team.passInProgress = 1
play the pass sample (except during a throw-in while the ball is dead)
```

`GetClosestNonControlledPlayerInDirection(dir, team)` scans the team's 11
sprites `0 → 10` (ties to the lowest slot, §7.1) and picks the one nearest the ball that is not the controlled
player, not sent off, in `PL_NORMAL`, and whose `fullDirection` is within ±16
(as a signed byte difference) of `dir * 32`. So the pass target is chosen by
**which teammate lies along the stick direction from the ball**, then by
proximity to the ball — not by proximity to the passer.

Only a human team gets `spinTimer = 0`; the CPU aftertouches through its own
path (§16.6).

### 12.3 `PlayerKickingBall` — a held fire

```
dir = player.direction
team.controlledPlDirection = dir
tbl = GetBallDestCoordinatesTable()
ball.dest   = ball.pos + tbl[dir]              /* ±1000 units: a straight ray */
ball.speed  = kBallKickingSpeed                /* 2208 */
ball.deltaZ = kBallKickingDeltaZ               /* 1.25 units/frame upward */
ResetBothTeamSpinTimers()

/* A long throw-in taken while the ball is dead RETURNS HERE: no shot bonus, no
   aftertouch window, no passInProgress reset and no kick sample. */
if (!live && gameState is a THROW_IN) return

/* is this a shot at the opponent's goal? */
if (live || gameState is not a THROW_IN) {
    shooting =  (team == topTeam) ? (ball.y >= 556 && dir in {3,4,5})
                                  : (ball.y <= 342 && dir in {7,0,1})
    if (shooting) {
        me = PlayerInfo of the kicker
        closeRange = (ball.x in [241,431]) && (ball.y < 204 || ball.y >= 694)
        ball.speed += closeRange ? kBallSpeedFinishing[me.finishing]
                                 : kBallSpeedKicking [me.shooting]
    }
}

/* open the aftertouch window, unless a keeper is punting sideways or the
   opponent keeper has just been beaten */
if (!(keeper && dir in {2,6}) && opponent.goalkeeperSavedCommentTimer >= 0)
        team.spinTimer = 0
team.passInProgress = 0
play the kick sample
```

The two skill tables are signed: `kBallSpeedKicking[0] = -384` (a weak shooter
takes pace *off* a long shot) up to `+384`; `kBallSpeedFinishing` runs `-288 …
+608`, so finishing matters far more than shooting inside the box. This is the
mechanism behind SWOS's "get in the area before you hit it".

### 12.4 `CalculateIfPlayerWinsBall` — contesting and dribbling

Called every frame the controlled player is very close to a live ball with a
direction held.

```
team.passInProgress = 0
opp = team.opponentsTeam

/* --- a 50/50? --- */
if (!opp.wonTheBallTimer && opp.playerHasBall && opp.currentAllowedDirection >= 0
    && opp.controlledPlayer)
{
    /* NOTE: the original looks the opponent's player up in OUR squad first.
       This is a genuine bug in SWOS; it is preserved because the resulting
       skill value feeds the contest. */
    a = (theirPlayer.tackling + theirPlayer.ballControl) >> 1     /* our table  */
    b = (theirPlayer.tackling + theirPlayer.ballControl) >> 1     /* their table*/
    diff = (int8)(a - b)
    loser = opp                                  /* the side with the lower value */
    if (diff < 0) { diff = -diff ; loser = team }
    if ((Rand() & 0x1F) >= plAvgTacklingBallControlDiffChance[diff])
            loser = loser.opponentsTeam          /* the upset: the stronger side loses */
    loser.wonTheBallTimer = 12                   /* locked OUT -- see below */
    ball.speed = 0 ; ball.dest = ball.pos
    return
}

/* --- uncontested: dribble --- */
team.controlledPlDirection = dir
if (ball.deltaZ > 0) ball.deltaZ = -1                 /* pull a rising ball down */
if (dir == 0 && |ball.x - player.x| < 4)
        ball.x += (ball.x >= player.x) ? 1 : -1       /* nudge onto the foot */

ball.dest = player.pos + kDefaultDestinations[dir]

speedDelta = 0
if (currentTick & 2) speedDelta = kBallSpeedDeltaWhenControlled[me.ballControl]
ball.speed = player.speed + speedDelta

/* running at an angle to your facing pushes the ball further ahead */
if ((uint8)((player.fullDirection + 128) - player.direction * 32) in [64,192])
        ball.speed += 256

/* turning with the ball */
if (team.controlledPlDirection != player.direction) {
    team.ballControlTurnTimer++
    if (team.ballControlTurnTimer >= ballControlTurnThresholds[me.ballControl])
            team.wonTheBallTimer = 8               /* you lose it */
}
ResetBothTeamSpinTimers()
```

`kBallSpeedDeltaWhenControlled` is *inverted*: `130` for ball-control 0 down to
`32` for ball-control 7. A poor dribbler pushes the ball further ahead of
himself each touch. `ballControlTurnThresholds` runs `4 … 21` — a bad player can
only hold a turn for 4 update ticks before losing it.

`wonTheBallTimer = 12` is a possession lock-out, and it goes to the side that LOST
the roll, whatever its name says: a team with the timer running skips the whole
win-ball sequence (`if (team.wonTheBallTimer) -> boundary mask`, §8.3) and so cannot
claim the ball, while the other side meets `!opp.wonTheBallTimer` above and dribbles
away. The side with the lower skill value is locked out with probability
`plAvgTacklingBallControlDiffChance[diff]`/32 (16..23 of 32, so 50% at equal skill),
for 12 of its own updates -- about 24 frames, since only one team updates per frame
(§5.1). That also prevents a 50/50 resolving 25 times a second.

There is no team-1 or team-2 advantage in the contest. Teams update on alternate
frames, and whichever sees the other's claim first starts it -- decided by who
arrived when, not by which team is which. Checked against the DOS recordings
(2026-09-17): with each recording's pinned random value this model predicts all 186
contests in the corpus. The one asymmetry is the challenger/defender skill read in
the NOTE above, and it is original SWOS.

### 12.5 Aftertouch and spin (`ApplyBallAfterTouch`)

Runs at the top of every `UpdatePlayers` call, i.e. once per team update, and
gives SWOS its banana shots.

The window is `spinTimer`, `0 … 9`; `-1` disables it. It is opened by `DoPass`
(**human teams only**) and by `PlayerKickingBall` (**either team** — CPU shots do
get an aftertouch window, which §16.6 depends on), and closed by
`ResetBothTeamSpinTimers` or by reaching 10.

The routine has two mirror-image halves selected by `team.passInProgress`
(`1` = the last release was a pass, `0` = a shot, header or tackle deflection).
"SHOT" below means `passInProgress == 0`.

```
if (opponent.goalkeeperSavedCommentTimer < 0) -> close the window
if (!live && gameState == ST_KEEPER_HOLDS_BALL) -> close the window
if (team.spinTimer < 0) return
if (team.spinTimer == 0) clear leftSpin/rightSpin (+ longPass/longSpinPass for a pass)

/* which way is the stick pushed relative to the aim of the shot? */
if (!leftSpin && !rightSpin && currentAllowedDirection >= 0) {
    diff = (controlledPlDirection - currentAllowedDirection) & 7
    if (diff != 0 && diff != 4)
        (diff < 4) ? (leftSpin = 1, side = 0) : (rightSpin = 1, side = 4)
}

/* apply the curve by MOVING THE BALL'S DESTINATION */
if (a side was chosen) {
    m   = kSpinMultiplierFactor[spinTimer]          /* 5,4,3,2,2,2,2,1,1,1 */
    idx = (SHOT ? controlledPlDirection : ball.direction) * 4 + (side >> 1)
    tbl = SHOT ? kKickSpinFactor : kPassingSpinFactor
    ball.destX += tbl[idx]     * m
    ball.destY += tbl[idx + 1] * m
}
```

The multiplier decays (5 → 1), so the first frames of a held direction bend the
ball hardest — hold the stick early and let go.

**Lob / chip (shots only), evaluated once at `spinTimer == 4`:**

```
if (currentAllowedDirection < 0) -> NORMAL (see below)
                                    /* a centred stick is NOT a no-op — it takes
                                       the NORMAL arm, same as sideways */
diff = (controlledPlDirection - currentAllowedDirection) & 7
if (diff == 0)               -> nothing (straight)
else if (diff == 2 || 6)     -> NORMAL: deltaZ = kNormalKickDeltaZ (1.375),
                                        speed  = kNormalKickBallSpeed (2560)
else if (diff in 3..5)       -> HIGH:   deltaZ = kHighKickDeltaZ (2.0),
                                        speed  = kHighKickBallSpeed (2688)
else                         -> nothing

then, for either:
if (controlledPlDirection == 0 or 4)  speed -= speed >> 2       /* ×0.75 */
else if (controlledPlDirection is odd) speed = speed - (speed>>2) + (speed>>3)
```

So pulling **back** on the stick 4 frames into a held shot is the classic SWOS
chip (the HIGH arm); pulling sideways keeps it comparatively flat but curls it.
Letting the stick return to centre is *not* a no-op — it takes the same NORMAL
arm as sideways, which raises `deltaZ` from the kick's 1.25 to 1.375 and re-sets
speed to 2560. Only the HIGH arm (2.0) is a true chip.

**Long pass (passes only):** the same `diff` test at any point in the window,
gated on `longPass`/`longSpinPass` not already set:

```
currentAllowedDirection < 0 -> longPass     = 1 ; ball.speed += ball.speed >> 3
diff == 2 or 6              -> longPass     = 1 ; ball.speed += ball.speed >> 3
diff in 3..5                -> longSpinPass = 1 ; ball.speed += ball.speed >> 3
```

Finally `spinTimer++`, and at 10 the window closes (`spinTimer = -1`).

### 12.6 Set-piece aim tables

`GetBallDestCoordinatesTable()` returns the 8-direction destination table used
by `DoPass` (free pass) and `PlayerKickingBall`:

```
THROW_IN_*                     -> kLeftThrowInBallDestDelta   (foulX <= 336)
                                  kRightThrowInBallDestDelta  (foulX >  336)
ST_PENALTY / ST_PENALTIES      -> kPenaltyBallDestDelta
ST_CORNER_LEFT / _RIGHT        -> selected by the FOUL SPOT, not by which of the
                                  two corner states it is (§18.1 awards
                                  ST_CORNER_LEFT at both ends, so the state alone
                                  does not identify the corner):
                                    foulY <= 449, foulX <= 336 -> kUpperLeftCorner…
                                    foulY <= 449, foulX >  336 -> kUpperRightCorner…
                                    foulY >  449, foulX <= 336 -> kLowerLeftCorner…
                                    foulY >  449, foulX >  336 -> kLowerRightCorner…
anything else                  -> kDefaultDestinations
```

The set-piece tables bias the aim away from the touchline/goal line so a corner
or throw cannot be played straight out (§24.3).

---

## 13. Heading

### 13.1 Jump header

Launched from `UpdatePlayers` (§8.3-F) by `PlayerAttemptingJumpHeader`:

```
player.heading   = 0
player.direction = dir
set jumpHeaderAttempt animation
player.playerDownTimer = 50
player.playerState     = PL_JUMP_HEADING
player.dest  = player.pos + kDefaultDestinations[dir]
player.speed = kJumpHeaderSpeed          /* 2048 */
```

While airborne, `SetJumpHeaderHitAnimTable` may switch the pose to the "hitting"
frame — but only in a narrow window, which is what gives the header its timing:

```
SetJumpHeaderHitAnimTable(player):
    if (already the hit animation)            return
    if (player.heading)                       return
    if ((uint8)player.playerDownTimer != 40)  return     /* frame 10 of 50 */
    if ((uint16)player.frameSwitchCounter > 2) return    /* UNSIGNED: the value is
        0xFFFF right after SetPlayerAnimationTable, so the very first tick of a
        fresh animation FAILS this test */
    if (!(low byte of currentGameTick & 2))   return     /* 50% of ticks    */
    switch to the hit animation (keeping the frame cursor)
    player.speed >>= 1
```

Contact (`PlayerHittingJumpHeader`) is evaluated in `UpdatePlayers` when
`playerDownTimer >= 42`, `ballDistance <= 64` and `ball.z in [8,15]`:

```
team.passInProgress = 0
input = team.currentAllowedDirection ; if (input < 0) input = player.direction

ball.deltaZ = kBallJumpHeaderDeltaZ (0.625)
ball.speed  = player.speed + (player.speed >> 2)        /* ×1.25 */

delta = (player.direction - input)

/* the STICK is re-read here, BEFORE the switch, and a centred stick SKIPS the
   switch — this arm is unreachable if you fold it into the switch, and folding
   it in turns every stick-centred jump header into case 0 */
if (team.currentAllowedDirection < 0) { FLYING header ; keep direction ;
                                        -> the shared tail }
        /* "-> the shared tail" means jump to the tail below and CONTINUE
           through it. It is NOT a return: the ball must still get its
           destination and player.heading must still latch, or the contact
           re-fires on the next update. */

delta &= 7
switch (delta) {                     /* each arm sets finalDir, a LOCAL */
  case 0: finalDir = direction                               /* straight  */
  case 1: finalDir = (direction - 1) & 7                     /* glance    */
  case 2: FLYING header ; finalDir = (direction - 1) & 7
  case 3: LOB header    ; finalDir = (direction - 1) & 7
  case 4: LOB header    ; finalDir = direction               /* backwards */
  case 5: LOB header    ; finalDir = (direction + 1) & 7
  case 6: FLYING header ; finalDir = (direction + 1) & 7
  case 7: finalDir = (direction + 1) & 7                     /* glance    */
}

FLYING: ball.deltaZ = kHeaderLowJumpHeight  (2.0)  ; ball.speed -= ball.speed>>2
LOB:    ball.deltaZ = kHeaderHighJumpHeight (2.25) ; ball.speed -= ball.speed>>4
        (both also force the "hit" pose if frameSwitchCounter <= 2)

the shared tail:
finalDir &= 7                        /* finalDir is a LOCAL — player.direction is
                                        never written by this routine, and every
                                        +/-1 arm already masked with & 7, so the
                                        table index below is always 0..7 */
team.controlledPlDirection = finalDir
ball.dest   = ball.pos + kDefaultDestinations[finalDir]
ball.speed += kPlayerHeaderSpeedIncrease[me.heading]      /* -336 .. 0     */
player.speed >>= 1
player.heading = 1
play the kick sample ; ResetBothTeamSpinTimers()
```

So: push the stick the way you're already facing for a flat, powerful header;
push it *across* your run for a flying header; push it *back* for a lob; and
**release it entirely for a flying header at 75 % pace** — the pre-switch arm
above, which is easy to lose when transcribing this routine (it skips the switch
but not the tail).
`kPlayerHeaderSpeedIncrease` is entirely negative — heading skill 7 is the
baseline and everyone worse loses pace.

### 13.2 Static header

```
AttemptStaticHeader(player, dir):
    player.heading   = 0
    player.direction = dir
    player.dest      = player.pos + kDefaultDestinations[dir]
    player.speed     = kStaticHeaderPlayerSpeed     /* 256 — a small hop  */
    set staticHeaderAttempt animation
    player.playerState     = PL_STATIC_HEADING
    player.playerDownTimer = 20
```

While in the air the player can be steered — but **exactly once**, not once per
frame. The third gate requires the "attempt" pose, and the function's own last
statement replaces that pose with `staticHeaderAim`, which nothing ever undoes.
So on the first frame with `playerDownTimer <= 18` *and* a direction held the
player turns ±1 and the routine goes inert for the rest of the jump. Total aim
authority for a static header is **±1 step before contact plus up to ±2 at
contact** — a port that loops this lets a player rotate 180° in the air:

```
SetStaticHeaderDirection(player, team):
    if (team.currentAllowedDirection < 0)                  return
    if ((uint8)player.playerDownTimer > 18)                return
    if (animation != staticHeaderAttempt)                  return
    delta = (currentAllowedDirection - player.direction) & 7
    if (delta != 0 && delta != 4)
        player.direction = (player.direction + (delta < 4 ? +1 : -1)) & 7
    switch to the staticHeaderAim pose
```

Contact (`PlayerHittingStaticHeader`) turns up to **two** steps toward the input
in a single frame:

```
PlayerHittingStaticHeader(player, team):
    team.passInProgress = 0
    if (animation == staticHeaderHit) -> apply_hit    /* already hitting: no turn */

    delta = team.currentAllowedDirection - player.direction
    if (delta == 0) -> apply_hit
    delta &= 7
    if (delta == 4) -> apply_hit                      /* exact 180: no turn      */
    step = (delta < 4) ? +1 : -1
    player.direction = (player.direction + step) & 7      /* first step  */
    if (player.direction != team.currentAllowedDirection)
        player.direction = player.direction + step        /* second step */
    player.direction &= 7
        /* Note the SECOND step's own wrap is deferred to that final &7 — the
           original leaves an unmasked value in the register in between, so
           chasing the intermediate is pointless; mask at the end.
           The delta test is on the RAW difference for == 0 but on the masked
           value for == 4, which is equivalent. Two steps is the maximum: after
           two, the loop stops whether or not the input was reached, so an input
           3 steps away gets you 2 steps and a 45-degree error. */

apply_hit:
    switch to the staticHeaderHit pose

    shootDir = team.currentAllowedDirection
    if (shootDir < 0) shootDir = player.direction
        /* THE STICK, NOT THE FACING, DECIDES WHERE THE BALL GOES. A stick held
           3 steps away turns the player only 2 but still heads the ball the full
           3 — the header does not have to go where the man is facing. Only a
           released stick falls back to the facing. */
    team.controlledPlDirection = shootDir

    ball.dest   = ball.pos + kDefaultDestinations[shootDir]
    ball.speed  = kStaticHeaderBaseBallSpeed (1792) + kPlayerHeaderSpeedIncrease[me.heading]
    ball.deltaZ = (-ball.deltaZ) >> 1                  /* reverse and halve   */
    player.heading = 1
    play the kick sample ; ResetBothTeamSpinTimers()
```

The `deltaZ` reversal is what makes a static header a downward nod when the ball
is dropping and a loop when it is rising.

### 13.3 Which header you get

From §8.3: with the stick **centred** and fire tapped you get a *static* header;
with a **direction held** you get a *jump* header. Both require the ball in the
`z >= 9` to launch — any height above that, `ballAbove17` included, so you can
commit to a header under a ball that is far too high to reach (which is how you
get beaten in the air) — and `z ∈ [8,15]` to actually connect.

---

## 14. Tackling, fouls, cards, injuries

### 14.1 Launching and resolving a tackle

```
PlayerBeginTackling(dir, player, team):
    player.tackleState = 0
    team.controlledPlDirection = dir
    player.direction = dir
    set the tackling animation
    player.playerState     = PL_TACKLING
    player.playerDownTimer = 0xFF            /* ~255 frames; ended by speed decay */
    player.dest  = player.pos + kDefaultDestinations[dir]
    player.speed = kPlayerTacklingSpeed      /* 1792 */
    player.tacklingTimer = 0
```

The slide is ended by friction, not by the timer: §8.6 subtracts
`kPlayerGroundConstant` (96) per frame, so from 1792 it lasts ≈ 19 frames, and
when speed hits zero `SetPlayerDowntimeAfterTackle` sets the time on the ground:

```
SetPlayerDowntimeAfterTackle(player, team):
    table = (player.tacklingTimer == -1) ? kComputerTacklingDownTime
                                         : kPlayerTacklingDownTime
    player.playerDownTimer = table[me.tackling]
```

`kPlayerTacklingDownTime[8] = {30,27,24,21,18,15,12,9}` — a poor tackler stays
down 30 frames, a good one 9. `kComputerTacklingDownTime` is a flat `3`, but its
name is a red herring: the table is chosen by `tacklingTimer == -1`, which the
CPU can **never** produce (only a human team negates the timer — §8.6). In
practice the flat-3 table is used *only* for a human who tapped and released fire
within the first two updates — the "stab" tackle — and CPU tacklers get the normal skill-indexed
30..9 down-time.

**Winning the ball** — §8.6 calls one of two routines when the slide reaches the
ball. Both compute the same deflection direction:

```
input = team.currentAllowedDirection ; if (input < 0) input = player.direction
delta = (player.direction - input) & 7
finalDir = (delta == 0 || delta == 4) ? player.direction
         : (delta  < 4)               ? player.direction - 1
                                      : player.direction + 1
finalDir &= 7
team.controlledPlDirection = finalDir
ball.dest = ball.pos + kDefaultDestinations[finalDir]
```

then differ in the pace they put on the ball:

```
WEAK (tacklingTimer == -1):
    half = player.speed >> 1
    player.speed = (player.speed - half) | 1
    ball.speed   = player.speed + (player.speed >> 1)      /* ×1.5 of remainder */

STRONG:
    ball.speed   = (team.playerNumber) ? player.speed + (player.speed >> 2)  /* ×1.25 */
                                       : player.speed
    player.speed >>= 1
```

Both then set `player.tackleState = 1`, upgrade it to `2` ("good tackle") if the
opponent's controlled player is at least 9 (squared) from the ball **and** more
than 32 (squared) from the tackler, play the kick sample and reset spin timers.

`tackleState` is what the foul test reads: `0` = never touched the ball (late),
`1` = touched it but the man was on top of you, `2` = clean.

### 14.2 Fouls (`PlayerTacklingTestFoul`)

Called when a slide is still travelling at `speed >= 512` and did not win the
ball.

```
victim = opponent.controlledPlayer ; if (!victim) return
if ((tackler.pos - victim.pos) squared > 32)  return        /* no contact  */

if (victim is a goalkeeper) { tackler.speed = (speed>>2)|1 ; return }
if (victim is outside [81,590] x [129,769])   return

tackler.speed = (tackler.speed >> 2) | 1                    /* stopped dead */
PlayerTackled(victim, opponentTeam)                          /* injury roll */

if (victim.ballDistance > 800)  return                       /* off the ball, no foul */
if (tackler.tackleState != 0) {
    if (tackler.tackleState == 2) return                     /* clean       */
    dirDiff = tackler.direction - victim.direction
    if (dirDiff < -1 || dirDiff > 1) return                  /* from the side */
}

/* it IS a foul */
tacklerTeam.stats.foulsConceded++
if (cardsDisallowed || trainingGame) -> no card, just award the free kick
```

**Card decision.** The zone test decides whether a card is even considered:

```
if (victim in the central band x in [193,478], y in [129,769]) {
    if (tackler is topTeam ? victim.y <= 216 : victim.y >= 682)  -> consider a card
}
/* otherwise, find the tackling team's man nearest their own goal centre */
ref = (336, tackler team defends top ? 129 : 769)
nearest = the tackling team's outfielder (not the tackler, not the keeper)
          with the smallest squared distance to ref, starting from the victim
          /* slots scanned 0..10 with `<=`, so a tie goes to the HIGHEST slot
             and also beats the victim (src/tackle_foul.c) */
if (nearest == victim)  {          /* last man! the odds are SWAPPED here,
                                      and the playerCardChance gate is SKIPPED */
        if (Rand() < 32)  -> yellow attempt          /* 12.5 % */
        else              -> straight red attempt    /* 87.5 % */
}
else -> consider a card

consider a card:
    if (((currentGameTick & 0x1E) >> 1) >= playerCardChance)  -> no card
    if (Rand() < 32) -> straight red attempt      /* 12.5% */
    else             -> yellow attempt
```

`playerCardChance` is drawn once per match from
`playerCardChances{3,5,7,10}min[(Rand() & 0x1E) >> 1]` (§24.8) — a short match
has a much higher per-foul card chance so bookings still happen.

The "last man" rule is the notable one: if the fouling team has nobody closer to
their own goal than the man who was fouled, the tackle is treated as a clear
goal-scoring opportunity — a card is issued **unconditionally** (the per-match
`playerCardChance` gate is skipped) and the red/yellow odds are *swapped*, making
it a 7-in-8 straight red instead of the usual 1-in-8.

### 14.2a The restart a foul produces (`TestFoulForPenaltyAndFreeKick`)

Called from all three foul exits above — after a booking, after a sending-off,
and on the no-card path. This is the **producer** of the `ST_PENALTY`,
`ST_FREE_KICK_*` and `ST_FOUL` states that §3.4, §12.6 and §18.4 all consume.

Note the parameter: `team` is the team that **committed** the foul, so the
penalty-area test below is against that team's *own* defensive area.

```
TestFoulForPenaltyAndFreeKick(victim, team):   /* team = the FOULING team */
    if (gameStatePl == ST_STOPPED) return
    play the foul whistle
    vx = victim.x ; vy = victim.y

    cameraDirection = (team == topTeamData) ? 0 : 4

    /* penalty — the fouling team's own box. NOTE both arms set
       breakCameraMode = -1 themselves and then jump straight to apply_break,
       skipping the commit block below. */
    if (team == topTeamData    && vy <= 216 && vx in [193,478]) {
            gameState = ST_PENALTY ; breakCameraMode = -1
            playerTurnFlags = 0x83 ; foul = (336, 187)
            play the penalty sample -> apply_break }
    if (team == bottomTeamData && vy >= 682 && vx in [193,478]) {
            gameState = ST_PENALTY ; breakCameraMode = -1
            playerTurnFlags = 0x38 ; foul = (336, 711)
            play the penalty sample -> apply_break }

    /* free-kick band, else a plain foul */
    if (team == topTeamData    && vy in [216, 330]) -> quadrant select
    if (team == bottomTeamData && vy in [567, 682]) -> quadrant select
    otherwise                                        gameState = ST_FOUL

    quadrant select — 7 X bands, mirrored for the bottom team:
                    vx < 153  < 261   < 309   < 362    < 410   < 518   else
        top team:    LEFT1    LEFT2   LEFT3   CENTER   RIGHT1  RIGHT2  RIGHT3
        bottom team: RIGHT3   RIGHT2  RIGHT1  CENTER   LEFT3   LEFT2   LEFT1

    commit (free kick or plain foul):
        breakCameraMode = -1 ; playerTurnFlags = 0xFF ; foul = (vx, vy)

apply_break:
    if (forceLeftTeam == 1) team = bottomTeamData
    gameStatePl = ST_STOPPED
    lastTeamPlayedBeforeBreak = team.opponentsTeam   /* the FOULED side takes it */
    stoppageTimerTotal = stoppageTimerActive = 0
    StopAllPlayers() ; camera velocity = 0
```

The seven free-kick states then index `freeKickFactorsX[gameState - 6]` when the
defending wall is built (§18.4), and `ST_PENALTY` selects
`kPenaltyBallDestDelta` for the aim table (§12.6).

### 14.3 Injuries (`PlayerTackled`)

Runs on the **victim** for every contact tackle, foul or not:

```
if (trainingGame && (Rand() & 3) != 0)   -> no injury
if (that team's remaining injury allowance == 0) -> no injury

probTable = (victim's injuriesBitfield & 0xE0) == 0x20
            ? kTackleInjuryProbabilityAlreadyInjured    /* {96,57,41,28} */
            : kTackleInjuryProbability                  /* {48,28,20,14} */
if (Rand() >= probTable[gameLengthInGame])  -> no injury

/* pick a severity bucket from a 6-bit roll */
sevTable = alreadyInjured ? kInjuryLevelAlreadyInjured {14,15,12,9,7,5,2}
                          : kInjuryLevels              {42, 7, 5,4,3,2,1}
roll = Rand() & 0x3F ; severity = 0x20
for i in 0..6 { if (roll < sevTable[i]) break ; roll -= sevTable[i] ; severity += 0x20 }

PlayerInfo.isInjured = 1
/* UNSIGNED byte compare — injuriesBitfield is int8_t and the severity values
   0x20..0xE0 read as -128..-32 signed, so a signed compare inverts the rule */
if ((uint8)severity > (uint8)PlayerInfo.injuriesBitfield)
        PlayerInfo.injuriesBitfield = severity
player.injuryLevel = kInjuryRecoveryTime[(severity & 0xE0) >> 5]   /* frames down */
decrement that team's injury allowance

/* regardless of injury */
player.playerState     = PL_TACKLED
player.playerDownTimer = 50
set the tackled animation
```

`kInjuryRecoveryTime[8] = {0,60,70,80,90,100,110,130}`: a severity-1 knock is 60
frames on the ground, the worst is 130. Note the probability is per **game
length setting** (0..3 = 3/5/7/10 minutes) — a 3-minute match injures nearly
twice as often per tackle so the per-match rate stays similar.

Each side starts a match with `4` allowed injuries
(`team1NumAllowedInjuries` / `team2NumAllowedInjuries`); this budget is shared
with red cards.

### 14.4 Cards

```
TryBookingThePlayer(player, team) -> 0 if a card was given, 1 if not eligible
    if (team.playerNumber == 0 && team.playerCoachNumber == 0)  -> not eligible
        /* "human-managed" means EITHER a joystick player OR a coach — the same
           pair §8.10a's bench gate uses. A CPU-vs-CPU match issues no cards at
           all in the plgD3Param == 0 path. In the competition path this test is
           replaced by "the player has no card yet" for such a team, so
           CPU-vs-CPU competitive matches do book players. */
    if (player already has a card && injury allowance 0) -> not eligible
    PlayerInfo.numYellowCards = (previousCards == 0 ? cardAnimStatesNoFlag
                                                    : cardAnimStatesWithFlag)
                                [player.cards + 1]
    player.cards++
    lastTeamBooked = team ; bookedPlayer = player ; refTimer = 0
    if (player.cards == 2) {          /* second yellow */
        whichCard = CARD_SECOND_YELLOW (3)
        stats.bookings-- ; stats.sendingsOff++
        decrement the injury allowance
    } else {
        whichCard = CARD_YELLOW (1)
        stats.bookings++
    }

TrySendingOffThePlayer(player, team)  -> straight red
    STRICTER eligibility: always requires a human-managed team (no highlight-mode
    exemption) and always requires a non-zero injury allowance, then
    PlayerInfo.numYellowCards = lut[(player.cards ^ 1) + 3]
    lastTeamBooked = team ; bookedPlayer = player ; refTimer = 0
            /* the same three the booking path sets, and all three are REQUIRED
               by §19: refTimer >> 3 indexes the card table, and the terminator
               arm calls SendPlayerAway(bookedPlayer, lastTeamBooked) */
    whichCard = CARD_RED (2)
    if (player.cards == 1) stats.bookings--
    stats.sendingsOff++ ; decrement the injury allowance
            /* NOTE: unlike the booking path, this does NOT increment
               player.cards */
```

`cardAnimStatesNoFlag[5] = {0,1,3,3,3}`, `cardAnimStatesWithFlag[5] = {1,2,4,4,4}`
map "cards in this match" onto the persistent `numYellowCards` value the career
mode reads (the "WithFlag" table is used when the player already carried a
booking into the match, i.e. `PlayerInfo.previousCards != 0`).

**`numYellowCards` is a state, not a count.** Resolving the two index
expressions gives only five reachable values:

| Value | Meaning                                    | How it is reached |
|-------|--------------------------------------------|-------------------|
| 0     | clean                                      | initial           |
| 1     | booked once, no prior booking              | `NoFlag[1]`  — first yellow |
| 2     | booked once, **carried a prior booking in** | `WithFlag[1]` — first yellow |
| 3     | off the pitch, no prior booking            | `NoFlag[2..4]` — second yellow or straight red |
| 4     | off the pitch, carried a prior booking in  | `WithFlag[2..4]` — second yellow or straight red |

The booking path indexes with `player.cards + 1`, so a first booking hits index 1
and a second hits index 2. The sending-off path indexes with
`(player.cards ^ 1) + 3`, which is `4` when the player had no card and `3` when he
had one — both of which land on the same LUT value, so **a straight red always
produces 3 (or 4 with the flag) regardless of what came before it**. Indices
`3` and `4` of the tables are only ever reached from that expression; nothing
reaches index 0 at runtime.

Everything in the engine that asks "is this player unavailable?" tests
`(uint8_t)numYellowCards >= 2` — the bench draw and selector (§8.10a), the
half-played bumps (§21.3). Note that this makes value **2** unavailable: a player
who came into the match already carrying a booking and picks up one more is out,
which is the cross-match suspension rule falling out of the same field. Read it
unsigned; the field is `int8_t`.

Setting `whichCard` non-zero triggers the referee sequence (§19). When it
finishes, if `whichCard & 2` (red or second yellow) `SendPlayerAway` runs:

```
SendPlayerAway(player, team):
    if (player's index == teamGame.markedPlayer) teamGame.markedPlayer = -1
    player.cards    = -1
    player.sentAway = 1
    player.dest     = (-20, 449)          /* walks off the left touchline */
```

`cards == -1` also removes the player from the free-kick wall count (§18.4).

Both routines have a second personality when `plgD3Param` is non-zero.
**`plgD3Param` is the competition-match flag**, not a highlight flag: it is
written in exactly one place, from the match-setup parameter, and is `0` for
friendlies *and* training but the competition's substitutes-count field
(normally non-zero) for every league, cup, DIY and career match. Highlight
recording is a different global (`hilStarted`).

So the branch below is the **normal** case for competitive play, and the
`cardAnimStates*` LUT is the friendly/training special case — the opposite of
what the naming suggests:

```
plgD3Param != 0 (competition):
    TryBookingThePlayer drops the "human-managed team" requirement, replacing it
    with "the player has no card yet"
    the cardAnimStates* LUT is bypassed entirely:
        TryBookingThePlayer:    numYellowCards = (numYellowCards == 0) ? 1 : 3
        TrySendingOffThePlayer: numYellowCards = 3
    i.e. the "carried a prior booking" values 2 and 4 are unreachable in
    competitive play, and the decision is made from the CURRENT numYellowCards
    rather than from player.cards or previousCards
plgD3Param == 0 (friendly / training):
    the eligibility and LUT rules described above apply
```

A port that implements only the LUT path will diverge on every competitive match.

### 14.5 `trainingGame`

A *training match* is the career practice fixture: the career squad is split into
two halves of itself ("<TEAM> A" vs "<TEAM> B"), played on a random pitch with
min = max = 2 substitutes, through the ordinary friendly entry point.
`trainingGame` is 1 for the duration of that match and 0 in every other —
friendly, league, cup, career, highlight playback.

Exactly **three** rules inside the simulation depend on it:

| rule | effect |
|------|--------|
| no cards | `cardsDisallowed \|\| trainingGame` → a foul is awarded but no booking is even considered (§14.2) |
| injury suppression | `Rand() & 3` — three times in four the injury roll is skipped entirely and the victim goes straight to `PL_TACKLED` (§14.3) |
| camera Y clamp | target Y clamps to `[0x50, 0x220]` instead of `[0x10, 0x250]` (§20.3) |

Everything else it touches is outside the simulation: bench layout, the kick-off
crowd cutaway, fan-tile animation, every crowd/whistle sample, pitch selection,
adverts, the HUD, and all post-match write-back. Note `IsPlayerInjured` also
skips its injury and card tests in training, which is why a suspended player can
appear in a training squad.

---

## 15. Goalkeeper

The keeper is `spritesTable[0]`, `playerOrdinal == 1`. It has its own branch in
`UpdatePlayers` and its own behaviour table.

### 15.1 The skill table

```
UpdatePlayerShotChanceTable(player, team):
    if (PlayerInfo.position != 0) team.shotChanceTable = teamPlOfs24Table  /* outfield */
    else team.shotChanceTable = kGoalieSkillTables[PlayerInfo.goalieSkill & 7]
```

`& 7` is mandatory — a goalie skill above 7 (possible in friendlies) would index
past the 8-entry pointer table.

`shotChanceTable` is **never** repointed for a penalty.
`penaltyGoalieSkillTable` is substituted for one *local* variable, in one place
only — the parry/catch resolution after a diving keeper's body contacts the ball
(§15.5), when `penalty || playingPenalties`. Everything else that reads
`shotChanceTable` during a penalty (the §15.2 tick gates, `speed_unk`/
`speed_unk2`, `ShouldGoalkeeperDive`'s `index[]`, §15.4's `field_30`) still reads
the keeper's own skill table. In particular `penaltyGoalieSkillTable.index[]` is
dead: `ShouldGoalkeeperDive` returns from its penalty arm before reaching the
`index[]` lookup. Data: §24.7.

### 15.2 Positioning

The **positioning cascade** (branches A–E below) only runs while live, when the
ball is in this keeper's penalty area, and when this keeper was not the last
player to play the ball.

The *routine* runs for every keeper on every team update regardless — and its
prologue is load-bearing even when the ball is dead, because that is where
`goaliePlayingOrOut` / `ballOutOfPlayOrKeeper` are re-derived and the keeper's
`speed` is reset. Do not gate the whole function on `live`; gate only the
cascade.

**The predicates this section uses.** Every one is a literal comparison; none of
them is a helper function in the engine. Defined once here so the cascade below
can be read as arithmetic:

```
/* zone flags — recomputed ONCE per team update, before the player loop
   (update_players.c:262-284), from the ball's integer position only */
ballInUpperPenaltyArea = (ball.x in [193,478] && ball.y in [129,216])
ballInLowerPenaltyArea = (ball.x in [193,478] && ball.y in [682,769])
ballInGoalkeeperArea   = ((ballInUpperPenaltyArea || ballInLowerPenaltyArea)
                          && ball.x in [273,398]
                          && (ball.y <= 158 || ball.y >= 740))
        /* NB: the goal-area test is nested INSIDE the penalty-area test, so
           ballInGoalkeeperArea implies ballIn*PenaltyArea. */

/* height flags — set in the §8.2 preamble that the keeper also passes through
   (update_players.c:355-372), on team, from ball.z: */
ballAbove17 = (ball.z > 17) ; ball12To17 = (12 < ball.z <= 17)
ball8To12   = (8 < ball.z <= 12)

/* "this keeper's own half" (update_players.c:450-458) — the test is on the
   KEEPER's Y, not the ball's, and y == 449 belongs to the TOP half: */
inOwnHalf = (team == topTeamData)    ? (keeper.y <= 449)
          : /* bottomTeamData */       (keeper.y >  449)

/* "this keeper's penalty area" (update_players.c:469-477) — selected by TEAM: */
ballInMyBox = (team == bottomTeamData) ? ballInLowerPenaltyArea
                                      : ballInUpperPenaltyArea

/* "my goal line" (§1.1) */
myGoalLineY = (team == topTeamData) ? 129 : 769

/* the two tick draws. tickHi is bits 4-7 of currentGameTick, so it holds for
   16 consecutive frames and cycles over 256; tickDive is bits 2-5. */
tickHi   = (currentGameTick & 0xF0) >> 4      /* 0..15 */
tickDive = (currentGameTick & 0x3C) >> 2      /* 0..15 */
```

`player.ballDistance` is **squared** (§2.1) and every comparison against it here
is an *unsigned* 32-bit compare in the original (`cmpi.l`/`bcc`/`bhi`). It is
never negative, so a signed compare is equivalent.


```
if (playingPenalties && penaltiesTimer > 50)   -> finalize (frozen)
UpdatePlayerShotChanceTable(player, team)

if (goalkeeperSkipFlag != 0) -> idle
    /* a fifth prologue exit, BEFORE ballNextGroundX = -1. No writer of
       goalkeeperSkipFlag exists in SWOS2.s or in either C port, so this is
       dead today — but it is load-bearing for ORDERING. Keep the exit above
       the ballNextGroundX store: hoisting the store changes nothing now and
       will change behaviour the day the flag is wired up. */

ballNextGroundX = -1
if (team.goalkeeperPlaying) {                   /* keeper is out as a field player */
    team.goaliePlayingOrOut = 1 ; team.ballOutOfPlayOrKeeper = 0
    if (player == team.controlledPlayer) -> the controlled-player branch
    team.goalkeeperPlaying = 0
}
/* every keeper update RE-DERIVES these two flags; the values written inside the
   goalkeeperPlaying block above survive only if the keeper was the controlled
   player and jumped out to §8.3 */
if (!live) { team.goaliePlayingOrOut = -1 ; team.ballOutOfPlayOrKeeper = -1
             player.speed = kGoalkeeperSpeedWhenGameStopped (1024) }
else       { team.goaliePlayingOrOut =  0 ; team.ballOutOfPlayOrKeeper =  0 }
player.speed = kGoalkeeperGameSpeed (1024)   /* unconditional — always overwrites
                                                the store above */

/* the wrong-half exit is tested FIRST — a keeper in the opposite half never
   reaches the re-tests below, controlled or not. The threshold is the pitch
   centre line, and the compare is STRICT on one side only:
        (keeper.y > 449 && team == topTeamData)    -> idle
        (keeper.y <= 449 && team == bottomTeamData) -> idle
   so y == 449 exactly counts as the UPPER half. */
if (the keeper is in the wrong half for its team) -> idle

/* these three re-tests are then the only way a keeper that is being controlled,
   or is the pass target, reaches §8.3/§8.9 after goalkeeperPlaying was cleared */
if (player == team.controlledPlayer) -> the controlled-player branch (§8.3)
if (player == team.passToPlayerPtr)  -> the receiver branch (§8.9)
if (!live)                           -> the idle branch (§8.8)
if (the ball is not in this keeper's penalty area) {
    team.ballOutOfPlayOrKeeper = 0
    if (this keeper was the pass target) drop the pass
    -> idle
}
if (player == lastPlayerPlayed) -> idle

UpdateBallVariables(player, ball, team)             /* §11.3 */
CalculateBallNextGroundXYPositions(ball)            /* §11.2 */
```

Then a cascade of decisions:

```
/* A. rush out for a loose ball inside the box */
if (!ballInGoalkeeperArea) {
    if (((currentGameTick & 0xF0) >> 4) >= shotChanceTable.field_3A) -> B
}
if (ballNextGroundX < 0) -> B
if (the predicted landing point is inside this keeper's penalty area trimmed away
    from the goal line — x in [193,478], y in [137,216] top / [682,761] bottom
    && sqDist(keeper, landing)*4 <= sqDist(ball, landing)) {
        /* both are SQUARED distances (§2.1), so the *4 is a factor of 2 in
           linear terms: the keeper commits when he is at most half as far from
           the landing point as the ball is */
        player.dest  = the landing point
        player.speed = kGoalkeeperMoveToBallSpeed (1024)
        -> catch check
}

/* B. shot on target? */
if (!live) -> idle
if (strikeDestX in [295,376]) -> D (goal attempt)

/* C. not on target: cover the near post / smother */
if (player.ballDistance < 512 && !ballAbove17) {
        player.dest = ball.pos -> catch check                /* smother      */
}
if (ballAbove17)                    -> E (evaluate interception)
if (ballInGoalkeeperArea) { player.destX = ball.x ; -> shared destY step }
        /* NOT an exit — it jumps INTO the middle of the block below, so this
           arm also gets the halfway destY and the table speed */
if (the ball is moving away from this goal, OR is vertically stationary) -> E
        /* the deltaY test is <= 0 / >= 0, so zero vertical velocity exits */
player.destX = 336 + ((ball.x - 336) >> 1)
shared destY step:
        if (ball.y >= 449) player.destY = ball.y + ((769 - ball.y) >> 1)
        else               player.destY = 129   + ((ball.y - 129) >> 1)
                /* halfway between the ball and the goal line the ball is
                   nearer to. Arithmetic shift, so it rounds toward -inf. */
        player.speed = shotChanceTable.speed_unk
        -> catch check

/* D. goal attempt */
if (team == lastTeamPlayed && !playerHadBall)   -> burst out (speed 512)
if (player.ballDistance < 512 && !ballAbove17)  { player.dest = ball.pos ; -> F }
if (player.ballDistance <= 2048 && !ballAbove17) {
        /* MIDRANGE POSITIONING — step to a point halfway between the ball and
           the middle of this keeper's own goal. Same two formulas C uses. */
        player.destX = 336 + ((ball.x - 336) >> 1)
        if (ball.y >= 449) player.destY = ball.y + ((769 - ball.y) >> 1)
        else               player.destY = 129   + ((ball.y - 129) >> 1)
                /* NOTE the destY formula is chosen by where the BALL is, not by
                   which team this is — a top-team keeper facing a ball in the
                   lower half gets the 769 arm. In practice the wrong-half exit
                   and the penalty-area gate above make that unreachable. */
        player.speed = shotChanceTable.speed_unk
        -> F
}
    /* far / awkward */
    if (opponent.playerHasBall || team.playerHasBall
        || team.plVeryCloseToBall)                                  -> chase
            /* three team-level fields, tested in that order. The two
               playerHasBall flags are genuinely per-team (§12.4 owns them).
               plVeryCloseToBall, despite living on the team, was rewritten a
               few lines earlier from THIS KEEPER'S ballDistance (§8.2) — so
               here it means "this keeper is within 32", and on the far arm
               (ballDistance > 2048) it is necessarily 0. */
    if (team == lastTeamPlayed && !playerHadBall) -> burst out
            /* a SECOND copy of the burst-out test; omitting it loses the
               "keeper charges out at a ball its own side last touched"
               behaviour for every far ball */
    if (ball.speed > kShotAtGoalMinumumSpeed (512))                 -> F
    if (player.ballDistance > 5000 || ball.z > 12
        || |ball.deltaZ| > 0.5)                                     -> run, no save
    -> chase

/* E. can this keeper get there first? */
if (ball.z > 16 || ballDefensiveX < 0)          -> idle
if (any of: our controlled player, our pass target, their controlled player,
    their pass target is closer to the ball)     -> idle
player.dest = (ballDefensiveX, ballDefensiveY)   -> catch check
```

The keeper's `destY` is clamped to `[129, 769]` on the **cascade** exits. Four
paths skip it: §15.4's "keeper gets the ball as a field player" arm, both `!live`
and `frameCount & 0xE == 0` arms of `-> idle` (which drop into §8.8), and the two
prologue hand-offs to §8.3 / §8.9. `GoalkeeperCaughtTheBall` applies its own
tighter clamp, `[137, 761]`.

**Where each exit actually sends the keeper.** The shorthand above hides the
destinations, and two of them are the reason a keeper steps along its line at all:

```
burst out:    player.speed = kGoalkeeperBurstSpeed (512)
              player.dest  = (ballNotHighX, ballNotHighY)      -> catch check
run, no save: player.speed = shotChanceTable.speed_unk2
              player.dest  = (ballNotHighX, ballNotHighY)      -> catch check
chase:        player.dest  = ball.pos
              player.speed = shotChanceTable.speed_unk
              (but if ballAbove17 this becomes "run, no save" instead)
```

`ballNotHigh{X,Y}` is the trajectory crossing at `y = 135` / `y = 763` (§11.3) —
it is the **only** consumer of that predictor, and a port that leaves the
destination unchanged on these exits produces a keeper that never moves for a shot.

`-> idle` for a keeper is also not the plain idle branch. It is:

```
if (!live)                 -> the idle branch (§8.8), round-robin gated
else if (frameCount & 0xE) -> keep the existing destination, clamp destY, finalise
else                       -> the idle common path (§8.8)
```

so a **live** keeper only re-positions on 2 frames in 16.

### 15.3 Saving (`F`)

```
if (ball.z > 16) -> run, no save

/* skill duel: can the striker beat this keeper? */
if (goalkeeperSavedCommentTimer <= 0 && player.ballDistance <= 128
    && !ballAbove17 && opponent.firePressed
    && (opponent.playerHasBall || opponent.passKickTimer >= 22))
{
    attacker = opponent.lastHeadingTacklingPlayer     /* NOT controlledPlayer */
    finishing = 0
    if (attacker != NULL)
        finishing = keeperTeam.inGameTeamPtr
                        ->players[attacker.playerOrdinal - 1].finishing
            /* !! read that squad pointer again: it is the KEEPER'S team, not the
                  attacker's. See the note below — reproduce it, it is what the
                  original does. A NULL attacker contributes 0 rather than
                  skipping the duel, giving idx = 7 - goalieSkill (idx 0, i.e.
                  1 chance in 16, against a skill-7 keeper). */
    idx  = finishing - keeper.goalieSkill + 7                   /* 0..14 */
    tick = (currentGameTick >> 1) & 0xF                          /* 0..15 */
    if (tick < goalScoredChances[idx]) {
        /* BEATEN: the keeper dives but the ball goes past */
        team.goalkeeperSavedCommentTimer = -5
        keeper.direction = (ball.x < keeper.x) ? 6 : 2
        opponent.spinTimer = -1
        ball.speed = min(ball.speed, 1536)
        GoalkeeperJumping(...)
        team.passKickTimer = 0 ; team.passingKickingPlayer = NULL
        -> DONE: clamp destY and finish. The "beaten" arm EXITS here; it does
           NOT fall through to the dive test below, or the keeper would arm a
           second dive in the same frame.
    } else {
        team.goalkeeperSavedCommentTimer = +5                    /* SAVE   */
    }
}

if (!ShouldGoalkeeperDive(player, ball, team)) -> run, no save

/* choose a dive side, and (in a penalty only) possibly guess wrong */
if (ballDefensiveX >= keeper.x) {                    /* ball is to the RIGHT */
    variant = 0 ; dir = 2
    if (playingPenalties || penalty) {
        if ((currentGameTick & 6) == 0) { variant = 1 ; dir = 6 }  /* GUESSED LEFT */
    }
} else {                                             /* ball is to the LEFT  */
    variant = 0 ; dir = 6
    if (playingPenalties || penalty) {
        if ((currentGameTick & 0xC) == 0) { variant = 1 ; dir = 2 } /* GUESSED RIGHT */
    }
}
   /* Two DIFFERENT masks, one per side, each 2 bits: the guess fires on 1 tick
      in 4 either way, but on different ticks (bits 1-2 vs bits 2-3), so the two
      sides are not symmetric across the tick sequence. Outside a penalty the
      keeper always dives the correct way with variant 0 — variant 1 exists
      ONLY on a wrong penalty guess, which is why the slower 1280 speed in the
      table below is the "dived the wrong way" speed. */
keeper.direction = dir
GoalkeeperJumping(dir, variant, dir, keeper, ball, team)
      /* destDir is the same value as dir on both arms */
team.passKickTimer = 0 ; team.passingKickingPlayer = NULL
```

`goalScoredChances[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0}` compared
against a 0..15 tick: an idx of 7 scores 8/16 = 50 % of these duels; idx 14
scores 15/16; idx 0 scores 1/16.

**The "attacker's finishing" is looked up in the wrong squad — faithfully.** The
original is `exg a0,a1 / bsr GetPlayerPointerFromShirtNumber / exg a0,a1`
(`SWOS2.s:29542-29546`): the `exg` swaps only the *sprite* pointer, `a6` still
holds the **keeper's** team, and `GetPlayerPointerFromShirtNumber` indexes
`a6->inGameTeamPtr->players[sprite.playerOrdinal - 1]`. So the value fed into
`idx` is the finishing rating of *the defending side's* player wearing the
attacker's shirt number. The C port reproduces this exactly
(`src/update_players.c:725` passes `team`, the keeper's team). This is almost
certainly an original-SWOS bug, but it is *behaviour*: it shifts the whole
one-on-one distribution, and a port that "fixes" it to the attacking squad will
not match Amiga SWOS. Implement the wrong lookup.

`goalkeeperSavedCommentTimer` is a small signed latch: positive = "just made a
save" (it will parry rather than be beaten), negative = "just been beaten" (it
disables aftertouch and lets the ball through).

**`ShouldGoalkeeperDive`:**

```
dy = ball.y - keeper.y ; if (bottom team) dy = -dy
if (dy < 0)  return (dy >= -10)              /* ball already past: dive if close */

if (playingPenalties || penalty) {
    limit = (Rand() & 0x18) ? kKeeperPenaltySaveDistanceNear (12)
                            : kKeeperPenaltySaveDistanceFar  (20)
    return dy <= limit
}
if (dy > kKeeperSaveDistance (24))  return 0

framesY = frames for the BALL to reach the keeper's Y   (from ball.deltaY)
if (framesY == 0)                   return 0
framesX = frames for the KEEPER to reach ballDefensiveX (from keeper.deltaX)
if (framesX == 0)                   return 0
    /* the helper's do/while always returns >= 1 once it starts, so
       framesY == 0  <=>  ball.deltaY.raw   == 0   and
       framesX == 0  <=>  keeper.deltaX.raw == 0.
       Consequence: a keeper with NO HORIZONTAL VELOCITY never dives, and
       neither does a keeper facing a ball with no vertical velocity. */
if (framesX <= framesY)             return 0            /* can walk it, no dive */
    /* both frames comparisons are UNSIGNED 16-bit. The helper can overflow
       int16 for a near-stationary delta over a long distance, and that
       negative-as-signed value must compare LARGE. */

reach   = kGoalkeeperDiveDeltas[shotChanceTable.index[(currentGameTick & 0x3C) >> 2]]
framesDive = frames to cover |ballDefensiveX - keeper.x| at that reach
return (framesDive != 0 && framesDive >= framesY)
       /* framesDive != 0 is dead — all eight dive deltas are >= 3.0.
          goalkeeperDiveAttemptCount++ fires on every evaluation that
          reaches the index[] lookup. */
```

`GetFramesNeededToCoverDistance(delta, distance)` is a shift-scaled repeated
subtraction: normalise `|delta|` up to at least 1.0 (doubling `step` each time),
then count `step` per subtraction of `delta` from `distance << 16`.

`kGoalkeeperDiveDeltas[8] = 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5` units/frame,
indexed through the skill table's 16-entry `index[]` sampled by the tick.

**Mind the direction of the final test.** It is `framesDive >= framesY`, so the
keeper commits to a dive only when the modelled dive would *not* comfortably beat
the ball — a **smaller** delta yields **more** frames and therefore makes a dive
**more** likely. Combined with the `index[]` distributions in §24.7 (a skill-7
keeper draws index 0 → 3.0 on 13 ticks in 16; a skill-0 keeper draws index 7 →
6.5 on 13 in 16), the net effect is the expected one: good keepers dive far more
often than bad ones. Implement the comparison exactly as written rather than
"fixing" it to the intuitive direction.

**`GoalkeeperJumping`:**

```
keeper.speed = (keeper.ballDistance <= 128) ? kGoalkeeperNearJumpSpeed (1024)
             : (variant == 0)               ? kGoalkeeperFarJumpSpeed (2048)
             : (variant == 1)               ? kGoalkeeperFarJumpSlowerSpeed (1280)
                                            : (currentGameTick & 0xFF) + 1024
team.field_46 &= 0xFF          /* clear the "already intervened this dive" latch —
                                  without this the keeper's body stops
                                  intercepting the ball after the first save */
team.goalkeeperDivingRight = 0
keeper.playerState = (ballNotHighZ > 5) ? PL_GOALIE_DIVING_HIGH : PL_GOALIE_DIVING_LOW
set the matching left/right, high/low dive animation
keeper.playerDownTimer = 75
team.controlledPlDirection = diveDir
keeper.dest = keeper.pos + kDefaultDestinations[destDir]
```

Which `speedVariant` each call site passes — the table above is unusable without it:

```
beaten arm (§15.3)      GoalkeeperJumping(dir, 2, dir, ...)  -> (tick & 0xFF) + 1024
normal dive             GoalkeeperJumping(dir, 0, dir, ...)  -> 2048
penalty + guessed flip  GoalkeeperJumping(dir, 1, dir, ...)  -> 1280
```

Variant 1 is used **only** on the branch where the penalty side-guess
(`tick & 6` / `tick & 0xC`) flips the dive direction; it is reset to 0 when the
guess test fails. So a keeper that guesses and flips dives ~40 % slower than one
that does not.

### 15.4 Catching

The catch test runs after any positioning decision:

```
if (team == lastTeamPlayed && !playerHadBall)              -> cannot catch
if (!ballInGoalkeeperArea
    && ((currentGameTick & 0xF0) >> 4) >= shotChanceTable.field_3A) -> cannot catch
if (ballNextGroundX < 0)             -> cannot catch
if (ballDefensiveZ > 22)             -> cannot catch      /* DOS: 27 */
if (ballDefensiveZ <= 12)            -> cannot catch
if (player.ballDistance > 2116)      -> cannot catch      /* 46 units */
if (|keeper.x - ballDefensiveX| > 12) -> cannot catch
if (|keeper.y - ballDefensiveY| > 12) -> cannot catch
GoalkeeperCaughtTheBall(keeper, team)
```

**Whether or not the catch fired, the keeper then tries to simply GATHER the
ball.** This is the only path by which a keeper picks up a ball that has rolled
to its feet — no dive, no catch animation — and without it a ported keeper stands
over a stationary ball forever:

```
if (player == lastPlayerPlayed)  -> clamp destY, done
if (player.ballDistance > 72)    -> clamp destY, done       /* 8.5 units */
if (ballAbove17)                 -> clamp destY, done
if (playerState != PL_GOALIE_CATCHING_BALL && ball12To17) -> clamp destY, done

player.dest = player.pos
if (team == lastTeamPlayed && !playerHadBall) {
        /* our own keeper picks it up and plays on as an outfielder */
        UpdateBallWithControllingGoalkeeper(keeper)
        team.controlledPlayer = keeper ; team.passToPlayerPtr = NULL
        team.playerSwitchTimer = 25 ; passingBall = passingToPlayer = 0
        lastTeamPlayed = team ; lastPlayerPlayed = keeper
        penalty = 0 ; playerHadBall = 0
        team.passKickTimer = 0 ; team.passingKickingPlayer = NULL
        team.field_74 = 0
        team.goalkeeperPlaying = goaliePlayingOrOut = ballOutOfPlayOrKeeper = 1
} else {
        lastKeeperPlayed = lastPlayerPlayed
        lastTeamPlayed = team ; lastPlayerPlayed = keeper
        penalty = 0 ; playerHadBall = 0
        UpdateBallWithControllingGoalkeeper(keeper)
        GoalkeeperClaimedTheBall(keeper, ball, team)        /* §15.6 */
}
```

```
GoalkeeperCaughtTheBall:
    keeper.direction = (top team) ? 4 : 0
    team.goalkeeperDivingLeft = 0
    set the catching animation
    keeper.playerState     = PL_GOALIE_CATCHING_BALL
    keeper.playerDownTimer = 15
    keeper.speed = kGoalkeeperCatchSpeed (768)
    keeper.dest  = (ballNextGroundX, clamp(ballNextYGroundY, 137, 761))
```

While in `PL_GOALIE_CATCHING_BALL`, each frame:

```
if (--playerDownTimer == 0) { back to PL_NORMAL; if diving-left flag was set,
                              claim the ball and set ball.z = 5 }
if (goalkeeperDivingLeft)     -> hold
if (!team.plVeryCloseToBall)  -> hold
d1 = ((currentGameTick & 0xF0) >> 4) - shotChanceTable.field_30
if (d1 < 0) { keeper.speed = 0 ; ball.speed = 0 ; goalkeeperDivingLeft = 1
              GoalkeeperClaimedTheBall(...) }          /* clean catch */
else        GoalkeeperDeflectedBall(ball, team)        /* parry       */
```

`field_30` is the catch-vs-parry threshold: `12` for a skill-7 keeper (75 % of
ticks catch) down to `5` for skill 0 (31 %).

### 15.5 Diving, parrying and recovery

`PL_GOALIE_DIVING_HIGH/LOW` counts `playerDownTimer` down from 75. The field is
`int8_t`, and the two threshold compares are on `(uint8_t)playerDownTimer` — it
never actually goes negative because the `== 0` arm exits first, but keep the
unsigned form.

```
if (--playerDownTimer == 0)                          -> stand up, snap dest
if (goalkeeperDivingRight && playerDownTimer <= 42)  -> claim the ball, ball.z = 5,
                                                        stand up, snap dest
if (live && playerDownTimer <= 60)                   -> stand up, snap dest
        /* so a dive lasts 15 frames while live, and up to 33 while the game is
           stopped (the diving-right claim window opens at 42) */

/* speed decay while sliding — FIRST match wins */
step = kGoalKeeperRecoveryStepBase (112)
if (team.field_46)                                   /* already intervened this
                                                        dive: KEEP the base step */ ;
else if (keeper.imageIndex is a "caught ball" frame) step = kGoalKeeperRecoveryStepAlt (192)
else {
    dy = ball.y - keeper.y
    if (team == topTeamData) { if (dy < -5) step = kGoalKeeperRecoveryStepFar (128) }
    else                     { if (dy >  5) step = kGoalKeeperRecoveryStepFar (128) }
        /* "the ball is behind the keeper by > 5" = the ball is more than 5 units
           on the GOAL side of the keeper. Y grows downward, so that is
           ball.y < keeper.y - 5 for the top team and ball.y > keeper.y + 5 for
           the bottom team. */
}
keeper.speed = max(0, keeper.speed - step)

/* the "caught ball" pose is a literal sprite-id set, 8 compares in source
   order: */
   { SPR_GOALIE1_CAUGHT_BALL_RIGHT, SPR_GOALIE1_CAUGHT_BALL_LEFT,
     SPR_GOALIE2_CAUGHT_BALL_RIGHT, SPR_GOALIE2_CAUGHT_BALL_LEFT,
     0x3D2, 0x3D4, 0x36C, 0x36E }
   /* The four named ids are 0x35E, 0x360, 0x3E0, 0x3E2 — the symmetric set
      {base+5, base+7, base+19, base+21} taken from goalie 1's dive base 0x359
      and goalie 2's base 0x3CD, the same four offsets DoGoalkeeperSprites uses.
      Before 2026-07-30 this port mis-defined these four names, which
      collided one with the literal 0x3D2 and put two outside every dive
      sequence (§25.4). */

/* did the diving body intercept the ball this frame? */
if (!live)                                           -> done
if (team.field_46 /* already intervened */)          -> done
if (team == bottomTeamData) { if (ball.deltaY <= 0)  -> done }
else                        { if (ball.deltaY >= 0)  -> done }
        /* "the ball is not moving toward this goal". Unlike the §15.2 version of
           this test, the discriminator here is the TEAM, not ball.y. Full 16.16
           compare; deltaY == 0 exits on both arms. */
if (keeper.imageIndex < 0)                           -> done   /* sprite hidden */

/* --- the sprite-width overlap test --- */
pixWidth = spriteGraphicsPtr[keeper.imageIndex]->pixWidth
        /* pixWidth is the sprite's width in pixels, written by the
           image-command loader — it is ASSET data, not a constant, so the
           contact window differs between the Amiga and DOS sprite banks. The
           asm indexes the table with `lsl.w #2` on a 4-byte pointer slot; index
           it as a typed pointer table instead, since SpriteGraphics* is 8 bytes
           on a 64-bit host. */
w = pixWidth + 6
if (keeper.deltaX < 0) {                 /* sliding LEFT: box extends right */
    px = keeper.x - 5
    if (px      >= ball.x) -> done
    if (px + w  <= ball.x) -> done
    /* contact window:  keeper.x - 5  <  ball.x  <  keeper.x + pixWidth + 1 */
} else {                                 /* sliding RIGHT or still: box extends left */
    px = keeper.x + 5
    if (px      <  ball.x) -> done
    if (px - w  >  ball.x) -> done
    /* contact window:  keeper.x - pixWidth - 1  <=  ball.x  <=  keeper.x + 5 */
}
        /* Both windows are pixWidth+6 wide and both are open on one side: the
           box is thrown 5 units the way the keeper is NOT travelling and
           extended pixWidth+6 back through him. The original composes the 5 as
           two adds (`subq #2` + `subq #3`) — collapse it. All four compares are
           UNSIGNED (`bcc`/`bls`/`bcs`/`bhi`); that never matters on the pitch
           (keeper.x >= 81, pixWidth is small) but a keeper parked at the
           off-pitch x = -20 (§1.1) would wrap. deltaX == 0 takes the RIGHT arm
           (`bpl` on the full 32-bit delta). */

if (ball.z > 20)                                     -> done
if (playingPenalties || penalty) {
    dy = keeper.y - ball.y
    if (dy > 5 || dy < -5)                           -> done
    ball.speed -= ball.speed >> 2
} else {
    dy = keeper.y - ball.y
    if (dy > 7 || dy < -7)                           -> done
}

/* CONTACT */
team.field_46 = intervened
        /* a one-BYTE latch that the asm sets with `move.b #1` at offset 0x46.
           On big-endian that is the HIGH byte of the int16 field, so a port that
           models the field as an int16 must store 0x0100 for GoalkeeperJumping's
           `field_46 &= 0xFF` to clear it. */
keeper.speed -= keeper.speed >> (playerState == PL_GOALIE_DIVING_HIGH ? 2 : 1)
skillTbl = (penalty || playingPenalties) ? penaltyGoalieSkillTable : shotChanceTable
penalty = 0 ; playerHadBall = 0        /* the save ENDS penalty mode */
team.passKickTimer = 25
opponent.passingToPlayer = 0
if (goalkeeperSavedCommentTimer >= 0) opponent.passingKickingPlayer = NULL
if (!ballInGoalkeeperArea) opponent.stats.goalAttempts++
ResetBothTeamSpinTimers()

d1 = ((currentGameTick & 0xF0) >> 4) - skillTbl.field_2A
        /* The asm writes these two reads as `sub.w SPRITE_DIRECTION(a0)` and
           `sub.w SPRITE_SPEED(a0)` — Sprite equates applied to the GoalieSkill
           pointer, i.e. byte offsets 42 and 44 (SWOS2.s:29998-30009), which are
           field_2A / field_2C, NOT the table's first two words. Revisions before
           2.0 said field_0 / field_2, copying a port that took the names at face
           value (found by the replay rig, docs/TESTS.md). */
if (d1 < 0) {
    if (|keeper.x - ball.x| <= 6) {
        /* CLEAN CATCH off the dive */
        lastTeamPlayed = team ; lastPlayerPlayed = keeper
        ball.pos = keeper.pos ; ball.dest = ball.pos ; ball.speed = 0
        team.goalkeeperDivingRight = 1
        GoalkeeperClaimedTheBall(...)
    } else -> scattered parry
} else {
    d1 -= skillTbl.field_2C
    if (goalkeeperSavedCommentTimer >= 0) (d1 < 0) ? controlled parry : scattered parry
    else                                  (d1 < 0) ? scattered parry  : controlled parry
        /* A genuine tick-dependent four-way: with tick nibble n, n < field_2A
           tries the clean catch, field_2A <= n < field_2A + field_2C is the
           "d1 < 0" band, and the rest is the other arm. */
}

controlled parry: lastKeeperPlayed = lastPlayerPlayed   /* ONLY on this arm */
                  ball.speed -= ball.speed>>2 ; clamp to 1792
                  if the keeper was beaten (savedCommentTimer < 0):
                      ball.destX += ((currentGameTick & 0xF) << 7) - 960
                                    /* -960..+960 in steps of 128 */
                  else: /* ease the ball's DESTINATION back toward the ball's
                           current Y — the keeper has controlled it */
                        sh = kKeeperEaseBallTable[(currentGameTick & 0xE) >> 1]
                        ball.destY = ball.y + ((ball.destY - ball.y) >> sh)
                            /* the original spells this as
                                   dy = destY - y ; dy >>= sh ; dy = -dy
                                   by = y - dy    ; destY = by
                               i.e. two negations that cancel. Arithmetic shift,
                               so it rounds toward -inf on a ball headed up the
                               pitch. */
                        ball.destX = keeper.destX
scattered parry:  mirror destY about the ball, then add
                      ((currentGameTick & 0x1F) << 4) - 256   /* -256..+240 in
                      steps of 16 — ASYMMETRIC, not ±256 */
                  s = originalSpeed >> 2 ; ball.speed -= s
                  if (tick bit 4) { s >>= 1 ; ball.speed -= s }
                      /* the second subtraction reuses the s cached BEFORE the
                         first, so it is originalSpeed >> 3. Final speed is
                         originalSpeed * 5/8 with tick bit 4 set, else 3/4 */
                  clamp to 1536

both: ball.y = keeper.y ± 1 (away from the keeper's own goal)
      team.passKickTimer = 25
      opponent.passingToPlayer = 0                       /* a SECOND time */
      if (goalkeeperSavedCommentTimer >= 0) opponent.passingKickingPlayer = NULL
      play the "miss" sample                             /* the clean-catch arm
                                                            plays it too */
```

`field_2A` is the gate that decides whether a diving keeper even *tries* the
clean catch: `3, 4, 5, 6, 6, 7, 8, 9` for skills 0..7 (§24.7), so on the 0..15
tick nibble a skill-0 keeper tries on 3 ticks in 16 and a skill-7 keeper on 9 —
and the 6-unit X test can still deny it. `field_2C` (`5` for skills 0..3, `6` for
4..7) is the width of the next band: a keeper who has not been beaten
(`goalkeeperSavedCommentTimer >= 0`) makes a controlled parry on those ticks and
a scattered one on the rest; a beaten keeper gets the reverse. Skill 7: 9 catch
attempts, 6 controlled, 1 scattered; skill 0: 3, 5 and 8. The penalty table
(`field_2A = 2`, `field_2C = 0`) leaves no controlled band at all for an
unbeaten keeper.

**`GoalkeeperDeflectedBall`** (the punch/parry from a catch attempt):

```
base = (bottom team) ? direction 0 : direction 4      /* away from own goal */
ball.dest  = ball.pos + kDefaultDestinations[base]
ball.destX += ((currentGameTick & 0x1F) << 5) - 512    /* ±512 scatter      */

d1 = ((currentGameTick & 0x3C) >> 2) - shotChanceTable.field_34
speed = (d1 < 0) ? kGoalkeeperStrongDeflectBallSpeed (1536)
      : ((d1 - field_36) < 0) ? kGoalkeeperMediumDeflectBallSpeed (1024)
                              : kGoalkeeperWeakDeflectBallSpeed   (512)
ball.speed  = speed + ((currentGameTick & 0x1FF) - 256)
ball.deltaZ = kGoalkeeperDeflectDeltaZ (0.75) + (((tick & 0x7F) << 8) - 16384)
ResetBothTeamSpinTimers() ; play the kick sample
```

### 15.6 Holding the ball

```
GoalkeeperClaimedTheBall(keeper, ball, team):
    lastPlayerBeforeGoalkeeper = lastPlayerPlayed
    lastTeamScored             = lastTeamPlayed
    ball.speed = 0
    if (bottom team) { cameraDirection = 0 ; playerTurnFlags = 0xC7 }
    else             { cameraDirection = 4 ; playerTurnFlags = 0x7C }
    if (team.playerNumber == 0) playerTurnFlags &= 0xBB
    gameState   = ST_KEEPER_HOLDS_BALL
    breakCameraMode = -1
    foul = ball.pos
    gameStatePl = ST_STOPPED
    lastTeamPlayedBeforeBreak = team
    stoppageTimerTotal = stoppageTimerActive = 0
    StopAllPlayers()
    if (the keeper is mid-dive) return                 /* control comes later */
    team.controlledPlayer = keeper
    team.ballOutOfPlayOrKeeper = 1 ; team.goaliePlayingOrOut = 1
    keeper.direction = cameraDirection
    UpdatePlayerWithBall(keeper)
    camera velocity = 0
```

While the keeper holds it, `UpdateBall` bobs the ball between `z = 0` and
`z = 5` (§10.3) and `DoGoalkeeperSprites` pins the ball to a diving keeper:

```
DoGoalkeeperSprites():
    find the team whose goalkeeperDivingRight/Left flag is set (none -> return)
    yOff = (top team) ? +1 : -1
    ball.x = keeper.x ; ball.y = keeper.y + yOff
    ball.deltaZ = 0 ; ball.speed = 0
    if (diving)  ball.z = goalkeeperDiveBallHeights[keeper.imageIndex - animBase]
    else         ball.z = kGoalKeeperClaimingBallHeight[keeper.frameSwitchCounter]

    /* animBase is the first sprite id of the keeper's current dive sequence:
       goalie 1 -> 0x359, or 0x393 when imageIndex is in [0x393, 0x3AF)
       goalie 2 -> 0x3CD, or 0x407 when imageIndex is in [0x407, 0x423)
       so (imageIndex - animBase) is the frame's index into the 28-entry table.
       An index >= 28 is an engine invariant violation and panics. */
```

The two height tables (§24.6) make the ball follow the keeper's hands through
the dive animation.

### 15.7 Worked example: a falling ball over the top box

The keeper logic is the densest part of the engine and the branch letters above
only make sense once you have walked one frame end to end. This example fixes a
scenario, then names — at every step — exactly which unfixed variable decides the
next branch. Use it as a self-test: if your port reaches different letters, the
disagreement is localised for you.

> The ball is at `z = 10` falling with `deltaZ = -0.5`, `speed = 1200`, over the
> top penalty area, and the top team's keeper has `goalieSkill = 5`. Walk the
> keeper's decision for this frame.

**Fixed by the scenario.** `ball.x ∈ [193,478]`, `ball.y ∈ [129,216]`,
`ball.z = 10`, `ball.deltaZ.raw = -0x8000`, `ball.speed = 1200`,
`team == topTeamData`, `PlayerInfo.goalieSkill == 5`.

**Not fixed by the scenario**, and called out at each step: `currentGameTick`,
`frameCount`, `player.ballDistance`, `keeper.x`, `keeper.deltaX`,
`ball.deltaX/deltaY` (only the *magnitude* of the resultant is given),
`lastTeamPlayed`/`playerHadBall`, and whether the ball is also inside the
6-yard box.

**0 — the §8.2 preamble.** The zone flags were computed once for this team
update: `ballInUpperPenaltyArea = 1` (x in `[193,478]`, y ≤ 216), and
`ballInGoalkeeperArea = 1` only if additionally `ball.x ∈ [273,398]` and
`ball.y <= 158`. The height flags come out of the `z` cascade as
`8 < 10 <= 12` ⇒ `ball8To12 = 1`, **`ballAbove17 = 0`**, **`ball12To17 = 0`** —
those last two are what most of §15 keys off. `playerState == PL_NORMAL`, so the
state dispatch falls through to the `playerOrdinal == 1` test and enters the
goalkeeper section (**not** the controlled-player section — the ordinal test is
checked first).

**1 — prologue.** Not penalties, so no freeze. `UpdatePlayerShotChanceTable`
sees `PlayerInfo.position == 0` and stores
`shotChanceTable = kGoalieSkillTables[5 & 7] = kGoalieSkill5Table`. The fields
that matter downstream:

```
field_2A = 7  field_2C = 6     speed_unk = 992   speed_unk2 = 240
field_30 = 10  field_3A = 10
index[16] = { 0,1,1,2,2,2,2,2,2,2,2,2,2,3,4,5 }
```

`goalkeeperSkipFlag` is 0. `ballNextGroundX` is pre-set to −1. `live`, so
`goaliePlayingOrOut = ballOutOfPlayOrKeeper = 0` and `speed = 1024`. The
half test passes (a top keeper sits near `y ≈ 137..190`, so `keeper.y <= 449`).
Assuming the keeper is neither `controlledPlayer` nor `passToPlayerPtr` nor
`lastPlayerPlayed`, `ballInMyBox = ballInUpperPenaltyArea = 1`, so we reach the
two predictors.

**2 — `CalculateBallNextGroundXYPositions` (§11.2) returns −1.** The
"is it moving" gate (`|deltaX| > 1.0 || |deltaY| > 1.0`) passes for every
direction at `speed = 1200`: the worst case is 45°, where
`max(|cos_q8|,|sin_q8|) = 90` and `90*1200/65536 = 1.65 > 1.0`. But the
trial-height loop starts at `d0 = 20.0` and steps down to `13.0`, breaking only
when `ball.z >= d0`; with `ball.z = 10` no trial succeeds and the routine
returns **`ballNextGroundX = -1`**. `deltaZ` is irrelevant to this loop — only
`z` is tested. (The other arm, "ball standing", also returns −1, so the value is
unconditional here regardless of the gate.)

Two consequences fall out of this single value, and they are the most important
things about the frame:

* **branch A can never fire** — its second gate is `ballNextGroundX < 0`;
* **§15.4 can never catch** — its third gate is the same test. A ball at
  `z < 13` is not catchable by *any* keeper at *any* skill.

**3 — branch A ⇒ B, on every tick.** Two ways in, both landing on B:

* `ballInGoalkeeperArea == 1` → straight to the `ballNextGroundX < 0` test → **B**;
* `ballInGoalkeeperArea == 0` → `tickHi >= field_3A (10)` is true on 6 ticks in
  16 (`tickHi ∈ 10..15`) → **B** directly; on the other 10 ticks →
  `ballNextGroundX < 0` → **B** anyway.

So the value of `currentGameTick` does not change the outcome here.

**4 — branch B: C or D, decided by `strikeDestX`.** `live`, so the only test is
the goal-mouth window. `strikeDestX` came from `UpdateBallVariables` (§11.3),
which dispatched on `ball.deltaY`:

* **`ball.deltaY >= -1.0`** (the ball is not travelling at the top goal faster
  than 1 unit/frame in Y — `deltaY.raw = sin_q8 * speed` (§1.4), so at
  `speed = 1200` this needs `|sin_q8| < 55`, i.e. the direction within
  ≈ 25.4° of horizontal; figures are for the 50 Hz path, where the DOS
  `speed_factor` of 41/64 is a no-op): the routine takes a `deltaX`
  arm or "sample at ball", both of which reach the shared tail and set
  `strikeDestX = 0`. `0 < 295` ⇒ **C**.
* **`ball.deltaY < -1.0`**: the three-plane integration runs and `strikeDestX`
  is the X at which the trajectory crosses `y = 129`. On target
  (`295 <= strikeDestX <= 376`) ⇒ **D**; wide or never reaching the line
  (`strikeDestX == 0`) ⇒ **C**.

**5a — the C walk.** `ballAbove17 = 0` throughout, so no arm of C exits to E on
height.

* `ballDistance < 512` (keeper within 22.6 units): `dest = ball.pos`, speed
  stays 1024, → catch check.
* otherwise, `ballInGoalkeeperArea == 1`: `destX = ball.x`, then the shared
  destY step.
* otherwise `ball.y < 449`, so the vertical-motion gate is
  `ball.deltaY >= 0 → E`. Since we are in C's `strikeDestX == 0` case, this is
  where a ball rolling *across* or *away from* the box leaves: any
  `deltaY >= 0`, including exactly 0, goes to **E**.
* if `ball.deltaY < 0`: `destX = 336 + ((ball.x - 336) >> 1)`,
  `destY = 129 + ((ball.y - 129) >> 1)`, `speed = 992`.
  Concretely for a ball at `(400, 200)`: `destX = 368`, `destY = 164`.
* on the E arm: `ball.z = 10 <= 16` passes, then the four "someone is closer"
  comparisons decide. If the keeper is nearest, `dest = (ballDefensiveX,
  ballDefensiveY)`; otherwise `-> idle`, which for a live keeper means
  "hold the previous destination" unless `frameCount mod 16 ∈ {0,1}`.

**5b — the D walk.** Assume `team != lastTeamPlayed || playerHadBall`, so the
burst-out arm does not fire (if it does: `speed = 512`,
`dest = ballNotHigh{X,Y}`, → catch check, and the frame ends there).
`ballAbove17 = 0`, so the two gates are purely distances:

* `ballDistance < 512` → `dest = ball.pos` → F.
* `512 <= ballDistance <= 2048` → midrange: `destX = 336 + ((ball.x-336)>>1)`,
  `destY = 129 + ((ball.y-129)>>1)`, `speed = 992` → F.
* `ballDistance > 2048` → far branch. `plVeryCloseToBall` is necessarily 0 here
  (it means `ballDistance <= 32`), so only the two `playerHasBall` flags and the
  second burst-out test can divert. Then **`ball.speed = 1200 > 512` ⇒ F,
  deterministically** — the `> 5000` / `z > 12` / `|deltaZ| > 0.5` tests below
  are never reached at this speed. (Note that even if they were: `z = 10` is not
  `> 12`, and `deltaZ.raw = -0x8000` is not `< -0x8000`, so **neither** would
  fire. The scenario's `deltaZ = -0.5` sits exactly on the boundary of a strict
  compare — a good reminder to get those compares' strictness right.)

**6 — F (§15.3).** `ball.z = 10` is not `> 16`, so no immediate "run, no save".
The striker-vs-keeper duel then runs **only** if
`goalkeeperSavedCommentTimer <= 0 && ballDistance <= 128 && opponent.firePressed
&& (opponent.playerHasBall || opponent.passKickTimer >= 22)`. On the far-branch
route `ballDistance > 2048`, so the duel is skipped; on the near route it needs
`ballDistance <= 128` (≤ 11.3 units). If it does run, `idx = (finishing − 5) + 7`
— where `finishing` is read out of the *keeper's* squad by the attacker's shirt
number, per the note in §15.3 — and the keeper is beaten when
`((currentGameTick >> 1) & 0xF) < goalScoredChances[idx]`.

`ShouldGoalkeeperDive` then decides, and two of its gates are not visible in the
prose form:

```
dy = ball.y - keeper.y          /* top team: not negated */
if (dy < 0)  return (dy >= -10)         /* ball already goal-side */
if (dy > kKeeperSaveDistance (24)) return 0
framesY = frames for the ball to cover |dy| at ball.deltaY
framesX = frames for the keeper to cover |ballDefensiveX - keeper.x| at keeper.deltaX
```

`GetFramesNeededToCoverDistance` returns 0 **if and only if its delta argument
is 0** — the loop is a do/while, so even a zero distance returns `step >= 1`.
So:

* `ball.deltaY == 0` ⇒ `framesY == 0` ⇒ **no dive**;
* `keeper.deltaX == 0` ⇒ `framesX == 0` ⇒ **no dive**. A stationary keeper never
  dives — the dive test is scaled by *last* frame's keeper velocity.

Then `framesX <= framesY ⇒ no dive` ("can walk it"), and finally
`reach = kGoalkeeperDiveDeltas[index[tickDive]]`. Skill 5's `index[]` draws
`4.0` on 10 of 16 ticks, `3.5` on 2, and `3.0 / 4.5 / 5.0 / 5.5` once each, so
`reach ∈ [3.0, 5.5]`; `framesDive >= framesY ⇒ dive`. (`framesDive == 0` is
dead: every entry of `kGoalkeeperDiveDeltas` is ≥ 3.0.) Each evaluation that
reaches this point also increments `goalkeeperDiveAttemptCount`.

With the keeper on its line and the ball anywhere in the upper half of the box,
`dy = ball.y - keeper.y > 24` is the common case, so the common answer is
**no dive** ⇒ "run, no save": `speed = speed_unk2 = 240`,
`dest = ballNotHigh{X,Y}`, → catch check. If it does dive:
`direction = (ballDefensiveX >= keeper.x) ? 2 : 6`, `GoalkeeperJumping(dir, 0,
dir, …)` ⇒ `speed = 1024` if `ballDistance <= 128` else `2048`, state
`PL_GOALIE_DIVING_HIGH` if `ballNotHighZ > 5` else `..._LOW`,
`playerDownTimer = 75`; the frame ends at the destY clamp without a catch check.

**7 — the catch check (§15.4) and the gather.** For every non-dive exit:

* `ballNextGroundX < 0` ⇒ **cannot catch**. No `GoalkeeperCaughtTheBall` is
  possible this frame at `z = 10`, whatever the tick or the distances.
* Falling into the gather block: `ballAbove17 = 0` does not exit, and
  `ball12To17 = 0` does not exit either — at `z = 10` the
  `playerState != PL_GOALIE_CATCHING_BALL && ball12To17` gate is *open*. So the
  only remaining gates are `player == lastPlayerPlayed` and
  `ballDistance > 72`.
* Therefore **if `ballDistance <= 72` (8.49 units), the keeper picks the ball up
  this frame** — `dest = own position`, then either
  `goalkeeperPlaying = 1` and play on as a field player
  (`team == lastTeamPlayed && !playerHadBall`) or `GoalkeeperClaimedTheBall`
  ⇒ `ST_KEEPER_HOLDS_BALL` and a dead ball. This is the outcome for a ball
  dropping onto the keeper, and it happens *instead of* a catch animation.

**Summary of what is determined, and by what.**

| Determined | Value | Because |
|---|---|---|
| `ballNextGroundX` | `-1` | `z = 10 < 13` (§11.2) |
| branch A | never commits | `ballNextGroundX < 0` |
| §15.4 catch | never fires | `ballNextGroundX < 0` |
| `ballAbove17`, `ball12To17` | 0, 0 | `8 < 10 <= 12` |
| every `ballAbove17` gate in §15.2 | false | as above |
| §15.5 / §15.3 `z > 16` / `z > 20` gates | false | `z = 10` |
| far-branch `\|deltaZ\| > 0.5` | false | `-0x8000` is not `< -0x8000` |
| far-branch routing | → F | `speed 1200 > 512` |
| `shotChanceTable` | `kGoalieSkill5Table` | `goalieSkill 5` |
| positioning speed | 992 (`speed_unk`) / 240 (`speed_unk2`) | skill-5 table |
| gather possible | yes, if `ballDistance <= 72` | `ball12To17 == 0` |

| Undetermined | What it decides |
|---|---|
| `ball.deltaY` | C vs D (via `strikeDestX`), and C's E exit |
| `player.ballDistance` | smother / midrange / far inside C and D; duel eligibility; gather |
| `keeper.deltaX` | `framesX == 0` ⇒ no dive at all |
| `ball.y - keeper.y` | `ShouldGoalkeeperDive`'s 24-unit gate |
| `currentGameTick` | which A gate is used (outcome unchanged); dive reach; duel roll |
| `frameCount` | whether an `-> idle` exit re-positions |
| `lastTeamPlayed` / `playerHadBall` | burst-out; which gather arm |
| `ballInGoalkeeperArea` | whether A's tick gate is consulted; C's `destX = ball.x` arm |

---

## 16. Computer AI

A CPU team is one with `playerNumber == 0`. The AI **does not have its own
movement code** — it synthesises exactly the same four control signals a
joystick produces (`currentAllowedDirection`, `firePressed`, `fireThisFrame`,
`quickFire`, `normalFire`) and then the ordinary player logic runs. Porting the
AI therefore means porting a decision function, not a second engine.

### 16.1 `AI_SetControlsDirection` — the main controller

Called once per team update from the controlled-player branch (and from the idle
branch when the team has no controlled player).

```
if (team.resetControls) return
if (aiCounter)        aiCounter--
if (aiResumePlayTimer) aiResumePlayTimer--
aiRand = Rand()
team.AI_timer++
team.currentAllowedDirection = -1
team.firePressed = fireThisFrame = quickFire = normalFire = 0
if (inSubstitutesMenu) return

a5 = team.controlledPlayer ; a4 = team.passToPlayerPtr
d7 = a5 ? a5.direction : -1

goalY = (team == bottomTeam) ? 129 : 769         /* the goal we attack */
d6    = (ball.x - 336)^2 + (ball.y - goalY)^2    /* squared distance ball->goal */
d5    = CalculateDeltaXAndY(336, goalY, ball.x, ball.y, 256).dir   /* 0..255 */
```

### 16.2 Ball dead

```
if (team != lastTeamPlayedBeforeBreak) return             /* not our restart */

/* CPU vs CPU: dismiss result screens automatically */
if (gameState in [ST_STARTING_GAME .. ST_GAME_ENDED] && both teams are CPU) {
    limit = (gameState is a RESULT state) ? gsResultAutoAdvance (600)
                                          : gsHalftimeStoppage  (350)
    if (stoppageTimerTotal >= limit) team.firePressed = 1
    return
}

/* wait a randomised while before taking the restart */
if (stoppageTimerActive < ((currentGameTick & 0x3F) + 100))  -> "set_direction"
        /* "set_direction" = AIM ONLY, never release:
               if (resultTimer != 0) return
               if (gameState is GOAL_OUT_*, KEEPER_HOLDS_BALL or a THROW_IN)
                                                  -> the "turn away" oscillator
               if (gameState == ST_FOUL || a ST_FREE_KICK_*)
                        currentAllowedDirection = ((d5 + 16) & 0xFF) >> 5
               otherwise                          return
           So during the wait the AI shuffles the taker's facing toward the goal
           (or oscillates it on a keeper's ball / throw-in) and never fires. */

if (playingPenalties || gameState == ST_PENALTY)  -> penalty aim
if (gameState is KEEPER_HOLDS_BALL or GOAL_OUT_*) -> keeper's-ball logic
if (gameState == ST_PLAYERS_TO_INITIAL_POSITIONS) -> kick-off logic
if (gameState is a FREE_KICK or ST_FOUL)          -> foul logic
if (gameState is a THROW_IN)                      -> throw-in logic
otherwise                                         -> aim at the goal
```

* **keeper's ball / goal kick**: with probability 1/2 (`aiRand & 1`) and if the
  keeper is already facing an acceptable way it releases immediately; otherwise
  it looks for a teammate facing the same way. The acceptable set is:

  ```
  if (aiRand & 1) {
      if (d7 == cameraDirection)                       -> release
      if (ball.x <= 336) { if (d7 == 1 || d7 == 3)      -> release }
      else               { if (d7 == 5 || d7 == 7)      -> release }
  }
  -> look for a facing teammate
  ```

  So the two diagonals are `{1 (NE), 3 (SE)}` when the ball is on the left half
  of the pitch and `{5 (SW), 7 (NW)}` when it is on the right — always the pair
  pointing *across* toward the far touchline. Both pairs are inside the set left
  by §3.4's `& 0xBB` restart mask (which only removes 2 and 6, E and W), so the
  keeper never picks a facing the mask would reject; this test is the narrower of
  the two.
* **kick-off**: waits until `stoppageTimerActive >= 150`, then releases if
  already facing `cameraDirection`.
* **throw-in**: `aiThrowInDirections[gameState - ST_THROW_IN_FORWARD_RIGHT]` is
  a bitmask of acceptable facings, bit `d` = direction `d`. For the **top** team
  the byte is rotated by 4 (`(b << 4) | (b >> 4)`, an 8-bit rotate — this mirrors
  the mask, since the 8 directions wrap at 8). With probability 1/16
  (`(aiRand & 0xF) == 0`) it releases if the current facing is in the mask;
  otherwise it looks for a facing teammate. Read the table's entries as unsigned
  bytes: `-127` is `0x81` = directions 0 and 7.
* **foul / free kick**: 1/16 of the time it looks for a facing teammate,
  otherwise it turns toward the goal angle `d5` and releases when aligned.
* **penalty**: picks a random direction `aiRand & 7`. If that direction is legal
  in `playerTurnFlags` it aims there and waits. If it is **not** legal the AI
  *strikes the kick* — unless the shooter faces straight up or down
  (`direction == 0 || 4`), in which case it does nothing this frame. The strike
  goes through the shared release path (`AI_afterTouchStrength = 0`,
  `currentAllowedDirection = d7`, `normalFire = 1`, `AI_ballSpinDirection = 0`).
  Note the release is on the *rejected*-direction arm: a CPU team that only ever
  aims will never take a penalty.

"Look for a facing teammate" is `FindClosestPlayerToBallFacing(d7 * 32, team)`:
scan **both** teams' 22 players for the one nearest the ball that is not the
controlled player, not sent off, in `PL_NORMAL`, and whose `fullDirection` is
within ±16 of the given angle. If the winner is one of ours: clear `team.field_84`, then abort if
`aiResumePlayTimer != 0` or `team.passKickTimer >= 13` (the same
`AI_ResumeGameDelay` pair used in §16.4) — those two gates are what make a
restart a single tap rather than a `quickFire` re-issued every frame — otherwise
release the ball (`aiResumePlayTimer = 15`, `currentAllowedDirection = d7`,
`quickFire = 1`). If it is an opponent, turn away, but **only** when
`plVeryCloseToBall`; otherwise the AI just copies the current facing.

"Turn away" when nobody is facing:

```
if ((currentGameTick & 0xE) != 0) return         /* only 2 ticks in 16 */

/* aiTurnDirection is a signed GLOBAL (shared by both teams), initialised to +1.
   In steady state it is exactly -1 or +1; the two inner arms only matter if
   something ever parks it outside that pair, and nothing does. */
if (aiTurnDirection < 0) {
    if (aiTurnDirection != -1) { aiTurnDirection++ ; if (aiTurnDirection != -1) return }
    flipTo = +1
} else {
    if (aiTurnDirection != 1)  { aiTurnDirection-- ; if (aiTurnDirection != 1)  return }
    flipTo = -1
}
d0 = (d7 + aiTurnDirection) & 7
if (playerTurnFlags & (1 << d0)) { team.currentAllowedDirection = d0 ; return }
                                  /* accepted: aiTurnDirection is LEFT ALONE, so
                                     the AI keeps turning the same way next time */
aiTurnDirection = flipTo          /* only a REJECTED step flips the sign, and it
                                     does NOT also set a direction this tick */
```

The "delay" is the `(currentGameTick & 0xE) != 0` gate and nothing else — there is
no second counter. Two consequences worth getting right: the oscillator only
reverses when `playerTurnFlags` *blocks* the step (not on a timer), and on the
frame it reverses it issues no direction at all, so the player stands still for
that tick.

### 16.3 Live play

```
if ((playingPenalties || penalty) && team.spinTimer >= 0)  -> aftertouch (§16.6)
if (aiCounter) AI_SetDirectionTowardOpponentsGoal(team)
if (!a5) return
if (team.spinTimer >= 0)                                   -> aftertouch (§16.6)
if (team.plVeryCloseToBall || team.plCloseToBall)          -> "player near"
else                                                       -> "nobody near"
```

`AI_SetDirectionTowardOpponentsGoal` is a crude 3-way aim used for the ~15
frames after a shot decision:

```
if (aiCounter == 0) return
if (aiAttackHalf == 1) { only the bottom team; dir = ball.x < 300 ? 1 : ball.x > 371 ? 7 : 0 }
else                   { only the top team;    dir = ball.x < 300 ? 3 : ball.x > 371 ? 5 : 4 }
team.currentAllowedDirection = dir
```

### 16.4 "A player is near the ball"

```
if (AI_DecideWhetherToTriggerFire(a5, team, d7) == FIRE) return    /* shot armed */

/* shooting gates */
if (a5.direction == 2 || a5.direction == 6) {         /* facing E or W          */
    if (team == topTeamData) { if (ball.y >= 740) -> no fire }
    else                     { if (ball.y <= 158) -> no fire }
}
        /* 740 / 158 are the SIX-YARD-BOX Y bounds (§8.2's ballInGoalkeeperArea
           uses the same two numbers; the top team attacks y = 769, the bottom
           team y = 129). So a CPU player facing straight across the pitch does
           NOT shoot from inside the opposing 6-yard box — there the fallback
           turns him toward goal instead; anywhere else the gate passes.
           Both branches in the asm target the REJECT label (SWOS2.s:32052-32059);
           revisions before 2.0 had them the other way round, copying a port
           bug that shot from six yards out while facing across the goal. */
if (a5.direction < 0)                 -> no fire   /* no facing yet             */
if (d6 > 28800)                       -> no fire                  /* > 170 units */
if (d6 >= 12800 && (aiRand & 3))      -> no fire                  /* 3/4 refuse  */
tolerance = (d6 <= 3200) ? 0x32 : 0x0F                            /* 50 : 15     */
e = (int8)(a5.direction*32 - d5)
if (e > tolerance || e <= -tolerance) -> no fire
        /* ASYMMETRIC: fires on -tolerance < e <= tolerance. The asm's second
           test (`neg.b d1 / cmp.b d1,d2 / bgt fire`) is strict, so e exactly on
           the negative bound does not fire. */
if (aiResumePlayTimer) return
if (team.passKickTimer >= 13) return
team.AI_afterTouchStrength = 0
-> release: aiResumePlayTimer = 15, currentAllowedDirection = d7,
   normalFire = 1, AI_ballSpinDirection = (d2 < 0) ? +1 : -1
                   /* d2 = (int8)(a5.direction*32 - d5): the NEGATED sign of the
                      facing-minus-goal-angle error, and -1 when d2 == 0 */
```

**Every `-> no fire` above lands on the same fallback**, and the fallback is a
gated tree, not a flat path. It belongs to *this* section — a CPU team with a man
near the ball that decides not to shoot ends up here on almost every update, so
this is the CPU's ordinary passing/positioning behaviour, not an edge case:

```
no fire:
if (team.field_84 != 0) {
    team.field_84--
    p = FindClosestPlayerToBallFacing(d7 * 32, team)
    if (p is valid && p is ours) -> RELEASE
    -> turn-or-copy
}
if (d6 < 9800)                -> aim-at-goal            /* skips the whole tree */
a2 = opponent.controlledPlayer
if (a2 == NULL) {
    a2 = opponent.passToPlayerPtr
    if (a2 == NULL)           -> aim-at-goal
}
if (a2.ballDistance <   800)  -> midrange evaluate
if (a2.ballDistance <  5000)  -> far gate
                              -> aim-at-goal

midrange evaluate:
    if (d6 > 180000)          -> the long-ball decision (below)
    p = FindClosestPlayerToBallFacing(d7 * 32, team)
    if (p is valid && p is ours) -> RELEASE
    -> arm cooldown

far gate:
    if (aiRand <= 8 && d6 > 48400) -> the long-ball decision (below)
    if (currentGameTick & 0xC)      -> aim-at-goal
    p = FindClosestPlayerToBallFacing(d7 * 32, team)
    if (p is valid && p is ours) -> RELEASE
    /* falls through */         -> arm cooldown

arm cooldown:
    if (field_84 != 0)                  -> aim-at-goal
    if ((frameCount & 0x7F) >= 0x20)    -> aim-at-goal
    team.field_84 = 4
    /* falls through */                 -> turn-or-copy
```

The four leaves, which are the only things that actually write the stick:

```
RELEASE:      team.field_84 = 0
              if (aiResumePlayTimer || team.passKickTimer >= 13) return
              aiResumePlayTimer = 15 ; currentAllowedDirection = d7
              quickFire = 1
turn-or-copy: if (!team.plVeryCloseToBall) currentAllowedDirection = a5.direction
              else currentAllowedDirection =
                       (a5.direction + ((currentGameTick & 0x80) ? +1 : -1)) & 7
aim-at-goal:  if (!team.plVeryCloseToBall) currentAllowedDirection = a5.direction
              else currentAllowedDirection = ((d5 + 16) & 0xFF) >> 5
copy-facing:  currentAllowedDirection = a5.direction
                  /* the shared !plVeryCloseToBall arm of the two above */
```

Note that `aim-at-goal` and `turn-or-copy` share the same
`!plVeryCloseToBall -> copy the current facing` guard, so a CPU player who is *not*
very close to the ball never aims at all from this tree — it simply keeps running
the way it is already facing. Only the very-close case aims or turns.

These gates are what make the CPU release the ball in bursts rather than every
frame, and they keep `FindClosestPlayerToBallFacing` (a 22-player scan) off the
per-frame path.

`AI_DecideWhetherToTriggerFire` is the *volley/first-time-shot* test:

```
if (player is the keeper)                               return NO
if (not facing the attacking third: top team must face 3/4/5,
    bottom team must face 7/0/1)                        return NO
if (player.ballDistance > 648)                          return NO   /* ~25 units */
if (ball.deltaZ < 0)  { if (ball.z < 12 || ball.z > 20) return NO }  /* dropping */
else                  { if (ball.z <  8 || ball.z > 14) return NO }
team.fireThisFrame = 1
d = (((player.fullDirection + 128) & 0xFF) >> 5)        /* turn to FACE the ball */
team.currentAllowedDirection = d
if ((((player.fullDirection + 144) & 0xFF) >> 5) != player.direction) return NO
aiCounter = 15 ; aiAttackHalf = (team == topTeam) ? 2 : 1
return FIRE
```

### 16.5 "Nobody is near the ball"

```
if (AI_DecideWhetherToTriggerFire(...) == FIRE) return

/* the pass target has fallen >= 50 (squared) BEHIND the controlled player:
   hand control to the pass target and demote the current player to receiver */
if (a4 && a4.ballDistance - a5.ballDistance >= 50) { swap controlledPlayer and
                                                     passToPlayerPtr ; return }
if (!a4)                                -> steer by the fine angle
if (either team is human)               -> steer by the fine angle
/* CPU vs CPU with a pass target: occasionally rotate randomly */
if (player.x in [273,398] && (player.y <= 158 || player.y >= 740)) {
    currentAllowedDirection = -1          /* in the goal-mouth corridor AND
                                             already deep in a box: unforced */
} else {
    if (currentGameTick & 0x18) { currentAllowedDirection = d7 ; return }  /* 75% */
    /* so the random-rotate below is reached 25% of the time (mask 0x18 is
       zero for 8 of every 32 tick values) */
    rotate = aiRandomRotateTable[(aiRand & 2) >> 1]        /* -32 or +32 */
    currentAllowedDirection = (((player.fullDirection + rotate + 128 + 16) & 0xFF) >> 5)
}
```

The position test's x arms branch *to* the gate and its y arms branch *past* it
(SWOS2.s:32460-32469); revisions before 2.0 had the y sense inverted (a central
box `y[159,739]` giving −1), copying a port bug. The DOS build has no position
test at all: every player reaches the gate.

"Steer by the fine angle" (`derive_allowed_dir_from_full_angle`) is reached **only**
from this section, and is a different leaf from §16.4's `aim-at-goal`: it steers by
the player's own `fullDirection` toward the ball, not by `d5` toward the goal.

```
d = (((player.fullDirection + 128 + 16) & 0xFF) >> 5)
if (d != player.direction && player.ballDistance >= 800 && (currentGameTick & 0xE))
        currentAllowedDirection = player.direction      /* commit to the run */
else    currentAllowedDirection = d
```

**This section is the whole of "nobody near the ball" — there is no fallback tree
here.** The three exits above are the only ones; the gated tree lives in §16.4,
where it is the "no fire" fallback. The two sections share no code beyond the
`AI_DecideWhetherToTriggerFire` test at the top and the long-ball decision below.

Beware the three similar names, which are three *different* routines:

| Name | Reached from | Writes |
|------|--------------|--------|
| steer by the fine angle | §16.5 only | from `player.fullDirection` (toward the ball), with a "commit to the run" override |
| aim-at-goal | §16.4's tree only | `((d5 + 16) & 0xFF) >> 5` (toward the goal), and only when `plVeryCloseToBall` |
| copy-facing | the `!plVeryCloseToBall` arm of both §16.4 leaves | `a5.direction` unchanged |

The long-range decision (`decide_immediate_turn_or_gate`, reached from §16.4's
tree and from §16.5) fires a long ball when the goal angle's 8-way bucket is the
current facing or **one step clockwise** of it:

```
bucket = ((d5 + 16) & 0xFF) >> 5
if (((bucket - d7) & 7) in {0,1}) {
        /* The asm also compares against $FFFF (SWOS2.s:32234-32240), but after
           `andi.w #7,d0` that arm can never be taken. Revisions before 2.0
           read it as 7 and gave the AI a counter-clockwise arm it lacks. */
    team.AI_afterTouchStrength = (d6 <= 115600) ? 1 : 2
    team.currentAllowedDirection = d7 ; team.normalFire = 1
    aiResumePlayTimer = 15

    /* the base spin is the SIGN of the residual fine-angle error, as a signed
       BYTE difference between the chosen 8-way step and the true angle */
    d2 = (int8_t)((uint8_t)(d7 << 5) - (uint8_t)d5)
    d0 = (d2 < 0) ? +1 : -1

    /* --- then the pitch/tick bias. Two early outs first: if the ball is
       already moving into the far third it keeps the base spin unmodified. --- */
    if (ball.deltaY < 0) { if (ball.y < 342) -> commit d0 }
    else                 { if (ball.y > 555) -> commit d0 }

    t = (currentGameTick & 0x1C) >> 2                    /* 0..7 */
    bx = ball.x
    if (bx >= 193 && bx < 478) {                         /* central corridor;
                                            `cmpi.w #478 / bcs`: 478 is NOT in it */
        if (t == 0) d0 = 0
        else if (t < 5) /* keep d0 */ ;
        else d0 = -d0
    } else if (bx >= 118 && bx <= 553) {                 /* the two half-spaces */
        if (t == 0) d0 = -d0
        else if (t < 4) d0 = 0
        else /* keep d0 */ ;
    } else {                                             /* wide of 118 / 553  */
        if (t < 4) /* keep d0 */ ;
        else d0 = 0
                /* NOTE: the wide gate has no explicit else — falling off its
                   `t < 4` test lands on set_zero_turn, so t >= 4 out wide means
                   NO spin at all. This is the one arm where the fall-through is
                   the whole behaviour. */
    }
    team.AI_ballSpinDirection = d0
}
```

The three X bands are nested tests, not a `switch`: the second is only reached
when the first fails, so its true range is `[118,192] ∪ [478,553]`. The tick
bucket splits 8 ways but the arms only distinguish `0`, `1..3 / 1..4`, and the
rest — so the CPU's curve on a given long ball is a 3-way choice that changes
every 4 frames, which is what stops its crosses being identical.

### 16.6 AI aftertouch

Once the ball is released, `spinTimer >= 0` routes the AI here every update.
It steers the *stick*, and the ordinary `ApplyBallAfterTouch` (§12.5) does the
curving.

**The CPU's stick is consumed one team update late, and that is correct.**
`ApplyBallAfterTouch(team)` sits in the *preamble* of `UpdatePlayers` (§8.1),
while `AI_SetControlsDirection` runs inside the per-player dispatch that follows
it. For a **human** team the order is fine — `processInputForPlayer` writes the
stick and *then* calls `UpdatePlayers`, so aftertouch sees this frame's input. For
a **CPU** team `UpdatePlayers` is called with nothing written, so the value
`ApplyBallAfterTouch` reads is whatever the AI left at the end of its *previous*
update, two frames earlier (§5.1's 25 Hz alternation). Consequences to reproduce:
the `spinTimer == 0` frame — the one with §12.5's ×5 multiplier, the hardest bend
— uses the stick state left over from the release itself, not the AI's first spin
choice; and the AI's last spin choice is never consumed at all, because the window
closes at `spinTimer == 10`. Move `ApplyBallAfterTouch` after the AI and the CPU's
curve becomes noticeably stronger than the original's.

```
d1 = team.controlledPlDirection
if (!playingPenalties && !penalty) {
    if ((aiRand & 1) == 0)                -> long-kick path
    if (AI_ballSpinDirection < 0) { d1 = (d1 + aiLeftSpinTable  [min(strength,2)]) & 7
                                    team.currentAllowedDirection = d1 ; return }
    if (AI_ballSpinDirection > 0) { d1 = (d1 + aiRotateRightTable[min(strength,2)]) & 7
                                    team.currentAllowedDirection = d1 ; return }
    /* AI_ballSpinDirection == 0 falls THROUGH into the long-kick path */
}
long-kick path:
    if (AI_afterTouchStrength == 1) { team.currentAllowedDirection = -1 ; return }
    d1 = (d1 + aiLongKickTable[min(strength,2)]) & 7
    team.currentAllowedDirection = d1
```

**Every arm assigns `currentAllowedDirection`** — including the two spin arms.
That is easy to lose, and losing it is not a no-op: §16.1 has already written
`currentAllowedDirection = -1` for this update, and §12.5 treats a centred stick
as a *decision* (the NORMAL-lift arm at `spinTimer == 4`, the `longPass` arm for a
pass), not as "leave the ball alone". Drop the two assignments and every CPU
curled shot becomes a lofted straight one.

Both spin arms also compute a `(d1 ∓ 1) & 7` into a scratch register and never
read it — a dead store in the original, safe to omit.

```
aiLeftSpinTable [3] = { -1, -2, -3 }
aiRotateRightTable[3] = {  1,  2,  3 }
aiLongKickTable  [3] = {  0, -999, 4 }      /* -999 & 7 == 1 */
aiRandomRotateTable[2] = { -32, 32 }
```

`AI_afterTouchStrength` (0..2) selects how far the AI pulls the stick off the shot
line — i.e. how much the ball bends. It is written at release time, and **each
release path has its own rule**; there is no single formula:

| Release path | `AI_afterTouchStrength` |
|--------------|-------------------------|
| §16.4 near-ball shot | `0` — unconditionally, so a CPU shot from close range never bends |
| §16.5 long ball (`decide_immediate_turn_or_gate`) | `1` if `d6 <= 115600`, else `2` |
| §16.2 restart, `ST_PENALTY` / `ST_PENALTIES` | `0` |
| §16.2 restart, `ST_CORNER_LEFT` / `ST_CORNER_RIGHT` | `1` |
| §16.2 restart, a `ST_FREE_KICK_*` state | by distance: `d6 < 28800 → 0`, `< 57800 → 1`, else `2` |
| §16.2 restart, anything else | `2` if `(aiRand & 0x18) == 0` (1 time in 4), otherwise the same distance rule as a free kick |

The `28800 / 57800` split and §16.5's `115600` are different partitions of the same
`d6` (squared distance to the target goal) and are **not** interchangeable. If you
implement only one of these rules, CPU set pieces and CPU long balls will bend by
the wrong amount in opposite directions.

### 16.7 `AI_Kick` — the receiver's interception

Called for a CPU team's pass target:

```
if (player.ballDistance > 200) return                  /* ~14 units */
if (!opponent.playerHasBall || !opponent.controlledPlayer) return
diff = (int8)(opponentPlayer.direction * 32 - player.direction * 32)
if (diff in [-32, 32]) return                          /* facing similarly */
team.currentAllowedDirection = player.direction
team.firePressed = team.fireThisFrame = team.normalFire = 1
```

i.e. a CPU player standing next to an opponent who is facing across him will
lash at the ball.

---

## 17. Off-ball positioning (tactics)

Every idle player's destination comes from one function.

### 17.1 The formation data

A `TeamTactics` block is 370 bytes:

```c
typedef struct {
    uint8_t positions[35];           /* one packed cell per ballQuadrantIndex
                                        (§10.8). NOT [5][7]: since q = xq + 5*yq
                                        the run is yq-major with stride 5, so a
                                        2-D form would have to be [7][5]. */
} PlayerPositions;

typedef struct {
    int8_t          name[9];
    PlayerPositions positions[10];   /* the 10 OUTFIELD players */
    uint8_t         pairIds[10];     /* Edit-Tactics pair links ONLY, no runtime
                                        effect (§17.3); 0xFF = unpaired         */
    uint8_t         ballOutOfPlayTactics;   /* redirect index for dead-ball */
} TeamTactics;
```

Each of the 10 outfield players has a **35-cell grid**, one cell per ball
quadrant (`ballQuadrantIndex`, 0..34 = `xq + 5*yq` from §10.8). Each cell is one
byte packing the target as two nibbles:

```
xIdx = (cell >> 4) & 0xF        /* 0..14 -> playerXQuadrantsCoordinates */
yIdx =  cell       & 0xF        /* 0..15 -> playerYQuadrantCoordinates  */
```

There are 18 formation slots: 12 built-in (4-4-2, 5-4-1, 4-5-1, 5-3-2, 3-5-2,
4-3-3, 4-2-4, 3-4-3, SWEEPER, 5-2-3, ATTACKING, DEFENDING) and 6 user slots
(`USER A..F`) loaded from `.TAC` files. `teamTacticsTable[19]` indexes them.

### 17.2 `SetPlayerWithNoBallDestination`

```
tactics = teamTacticsTable[team.tactics]

/* dead-ball redirect: some formations use a different shape for restarts */
if (gameState is KEEPER_HOLDS_BALL or GOAL_OUT_LEFT/RIGHT)
        tactics = teamTacticsTable[tactics.ballOutOfPlayTactics]

n = player.playerOrdinal - 2                 /* -1 for the keeper, 0..9 outfield */

if (n < 0) {
    /* GOALKEEPER: linear interpolation across the goal, tracking the ball */
    destX = 285 + ((ballX - 81) * 103) / 510        /* sweeps x in [285,387] */
    if (top team)  destY = 135 + ((ballY - 129) * 27) / 641   /* y in [135,161] */
    else           destY = 737 + ((ballY - 129) * 27) / 641   /* y in [737,763] */
    return
}

/* OUTFIELD */
if (team == topTeam) {
        cell = tactics.positions[n][ballQuadrantIndex]
} else {
        cell = 0xEF - tactics.positions[n][34 - ballQuadrantIndex]  /* mirrored */
}

destX = playerXQuadrantOffset + playerXQuadrantsCoordinates[(cell >> 4) & 0xF]
destY = playerYQuadrantOffset + playerYQuadrantCoordinates [ cell       & 0xF]

destX += (team == topTeam) ? -4 : +4          /* small side bias */

clamp destX to [81, 590] and destY to [129, 769]
```

Three things make this work:

* **The 35-cell grid is indexed by the *predicted* ball position** (§10.8), so
  the shape leads the play.
* **The bottom team mirrors both the index (`34 - q`) and the cell
  (`0xEF - cell`).** `0xEF` is `(14 << 4) | 15`, so subtracting flips both
  nibbles within their ranges — one formation table serves both directions.
* **`playerXQuadrantOffset` / `playerYQuadrantOffset`** add a sub-cell offset of
  roughly `±17` / `±15` units derived from where inside the quadrant the ball
  actually is, so the whole shape glides rather than snapping.

Coordinate tables (§24.4) are irregular — `playerXQuadrantsCoordinates` steps by
34 units, `playerYQuadrantCoordinates` by 40 — giving 15 × 16 reachable target
points spread across the pitch.

### 17.3 The player marker (this is *not* man-marking)

`TeamGame.markedPlayer` holds the index (0..10) of one of the team's **own**
players — the squad member the user has highlighted — or `-1`. It is seeded in
`InitInGameTeamStructure` from the selected-player index, can be re-set from the
bench menu, and is cleared if that player is sent off. `MarkPlayer()` draws a
marker sprite over `teamInfo->spritesTable[markedPlayer]`, alternating between
the two teams on bit 4 of `currentGameTick`.

**It has no gameplay effect at all** — no positioning, tackling or AI code reads
it. `TeamTactics.pairIds[10]` is likewise **editor-only**: it links two formation
slots in the Edit Tactics screen (row colouring, paired swap), and nothing in
`SetPlayerWithNoBallDestination` or `UpdatePlayers` reads it.

SWOS has no runtime man-marking system. Do not build one, and do not wire
`markedPlayer` to the opposition squad — it indexes your own sprite table.

---

## 18. Ball out of play and restarts

### 18.1 `CheckIfBallOutOfPlay`

Called from `UpdateBall` (§10.6) when the live ball leaves
`[81,590] × [129,769]`.

```
playRefereeWhistle = 0
if (gameStatePl == ST_STOPPED) return           /* already handled */
playRefereeWhistle = 1

bx = ball.x - 1 ; by = ball.y ; bz = ball.z   /* the -1 applies to the GOAL test only */

/* ---- GOAL? ---- */
if (bz <= 15 && bx in [302,366]) {
    scoringTeam = (by <= 449) ? bottomTeamData : topTeamData
    handleGoalScored(scoringTeam, ...)   /* plays the goal sample and CLEARS
                                            playRefereeWhistle */
    -> return.  A goal does NOT blow the whistle.
}

/* after the goal test the ball position is re-read WITHOUT the -1 bias, so every
   side test below (bx < 336 / bx >= 336) uses the raw ball.x */
bx = ball.x ; by = ball.y ; bz = ball.z

/* goal-kick / near-post sound cue */
if (ball.speed >= 768 && ball.x in [290,381] && (ball.z + 2) <= 25) {
        play the goal-kick sample ; playRefereeWhistle = 0
}

/* ---- past the TOP goal line ---- */
if (by < 129) {
    if (lastTeamPlayed == topTeamData) {                 /* they put it out  */
        restartTeam = bottomTeamData ; bottomTeam.stats.cornersWon++
        if (bx < 336) { spot=(86,134);  camDir=2 ; flags=0x1C ; ST_CORNER_LEFT  }
        else          { spot=(585,134); camDir=6 ; flags=0x70 ; ST_CORNER_RIGHT }
    } else {                                              /* goal kick        */
        restartTeam = topTeamData
        if (bx < 336) { spot=(276,154); camDir=4 ; flags=0x7C ; ST_GOAL_OUT_RIGHT }
        else          { spot=(396,154); camDir=4 ; flags=0x7C ; ST_GOAL_OUT_LEFT  }
        if (restartTeam is CPU) flags &= 0xBB
        goalOut = 1
    }
}
/* ---- past the BOTTOM goal line: mirrored ---- */
else if (by > 769) {
    if (lastTeamPlayed == bottomTeamData) {
        restartTeam = topTeamData ; topTeam.stats.cornersWon++
        if (bx < 336) { spot=(86,764);  camDir=2 ; flags=0x07 ; ST_CORNER_RIGHT }
        else          { spot=(585,764); camDir=6 ; flags=0xC1 ; ST_CORNER_LEFT  }
    } else {
        restartTeam = bottomTeamData
        if (bx < 336) { spot=(276,744); camDir=0 ; flags=0xC7 ; ST_GOAL_OUT_LEFT  }
        else          { spot=(396,744); camDir=0 ; flags=0xC7 ; ST_GOAL_OUT_RIGHT }
        if (restartTeam is CPU) flags &= 0xBB
        goalOut = 1
    }
}
/* ---- THROW-IN ---- */
else {
    restartTeam = lastTeamPlayed.opponentsTeam
    if (bx >= 336) { spotX = 590 ; camDir = 6 ; flags = 0xF1 }
    else           { spotX = 81  ; camDir = 2 ; flags = 0x1F }
    /* the six throw-in variants encode "how far up the pitch, which side".
       The mapping is explicit — all twelve cases: */
    zone = (by < 342) ? 0 : (by < 556) ? 1 : 2      /* 0 = low Y, 2 = high Y */

    if (bx >= 336) {                        /* right touchline */
        if (opTeam != topTeamData)  gameState = { BACK_LEFT,  CENTER_LEFT,  FORWARD_LEFT  }[zone]
        else                        gameState = { FORWARD_RIGHT, CENTER_RIGHT, BACK_RIGHT }[zone]
    } else {                                /* left touchline  */
        if (opTeam != topTeamData)  gameState = { BACK_RIGHT, CENTER_RIGHT, FORWARD_RIGHT }[zone]
        else                        gameState = { FORWARD_LEFT, CENTER_LEFT, BACK_LEFT    }[zone]
    }
    spotY = by
}

breakCameraMode = -1
writeRestartGlobalsAndStopPlayers(restartTeam, spotX, spotY, camDir, flags)
if (playRefereeWhistle) play the whistle
```

`forceLeftTeam == 1` collapses the bottom-half cases onto the top-half ones so
replays always render from the same end. It is also read in
`writeRestartGlobalsAndStopPlayers` (overriding the restart team to
`topTeamData`), in `GoalkeeperClaimedTheBall` and in the foul commit (§14.2a).
**Nothing in the engine ever writes it** — it is initialised to `2` and stays
there, so all **eight** `== 1` read sites are dead. A port should hard-code it
non-1 and delete them. Watch for the disguised one: the eighth site is spelled
`forceLeftTeam == ST_GOAL_OUT_LEFT` (in the idle-player restart-table selection),
and that constant *is* `1` — so it reads as a state comparison and survives a
search for `== 1`.

```
writeRestartGlobalsAndStopPlayers(team, x, y, camDir, flags):
    gameStatePl = ST_STOPPED
    foulX = x ; foulY = y
    cameraDirection = camDir ; playerTurnFlags = flags
    lastTeamPlayedBeforeBreak = team
    stoppageTimerTotal = stoppageTimerActive = 0
    StopAllPlayers()
    camera velocity = 0
```

**`StopAllPlayers()` is load-bearing and its field list matters.** For each team
it snaps every `PL_NORMAL`/`PL_HAPPY`/`PL_SAD` player that is not sent off to
`dest = pos`, and then clears **eight** `TeamGeneralInfo` fields:

```
ballInPlay = 0 ; ballOutOfPlay = 0
controlledPlayer = NULL ; passToPlayerPtr = NULL
passingBall = 0 ; passingToPlayer = 0
playerSwitchTimer = 0 ; passingKickingPlayer = NULL
```

(plus `bottomTeamData.goalkeeperPlaying = 0`, and it saves `cameraDirection` /
`playerTurnFlags` into their `last*` copies first). The `ballInPlay` and
`ballOutOfPlay` clears are the ones easy to miss: **`ballOutOfPlay` is what gates
player switching in §7.1**, so if it survives a restart, §7.1 re-installs a
`controlledPlayer` on the very next team update — for *both* teams, since both
have it set after any pass — and that player has no `1 → 2` path, which pins
§18.3 state 3 forever. Clearing all eight here is what makes the hand-over
terminate.

### 18.2 Goals

```
handleGoalScored(scoringTeamData, goalType, teamNum):
    scorer = lastPlayerPlayed
    if (lastKeeperPlayed && scorer is a keeper sprite) scorer = lastKeeperPlayed
    GoalScored(teamNum, scorer)
    goalCameraMode = 1 ; teamNumThatScored = teamNum
    teamScoredDataPtr = scoringTeamData

    opponent = scoringTeamData.opponentsTeam       /* they kick off */
    camDir  = (opponent == topTeamData) ? 4 : 0
    flags   = (opponent == topTeamData) ? 0x7C : 0xC7

    /* celebration length is biased by how important the goal was */
    base = (Rand() >> 1) + 100
    if (!playingPenalties) {
        if (the score is now level)                     len = 200
        else if (it was level before)                   len = 300
        else if (the lead was 1 and is now 2, or vice versa) len = 200
        else                                            len = 100
        len += Rand()
    } else len = base
    goalCounter = len ; patternsGoalCounter = 1

    gameState = ST_PLAYERS_TO_INITIAL_POSITIONS
    breakCameraMode = -1
    writeRestartGlobalsAndStopPlayers(opponent, 336, 449, camDir, flags)
```

`GoalScored(scoringTeam, scorerSprite)` then does the bookkeeping:

```
goalScored = 1 ; runSlower = 1
lastTeamScoredNumber = scoringTeam ; lastPlayerScored = scorerSprite
if (the team is already on 99) return
if (playingPenalties) teamNPenaltyGoals++ else teamNTotalGoals++
statsTeamNGoals++ ; update the two score digits
if (playingPenalties) return

goalType = GT_NORMAL
if (scorerSprite.teamNumber != scoringTeam) goalType = GT_OWN_GOAL
if (penalty)                                goalType = GT_PENALTY

if (own goal) { the CONCEDING team's numOwnGoals++ ; lastPlayerScored = NULL }
else          { the scorer's PlayerInfo.goalsScored++ }

key = the scorer's shirt number (+100 for an own goal)
find or create that key's slot in the 8-entry scorer list
append { goalType, the current match time as 4 ASCII bytes } to its goals[10]
(re-render the scorer caption sprite)
```

`goalCameraMode = 1` is what makes the camera follow the ball into the net and
triggers the auto-replay/highlight capture in §18.3.

`goalCounter` — the randomised celebration length computed above — has **no
gameplay consumer**. It is a presentation timer: the audio code counts it down
and fires the post-celebration crowd sample at zero; `EndFirstHalf`/`EndOfGame`
arm the half/full-time whistle cue only if it has already expired; and the fan
tiles re-arm `patternsGoalCounter` from it. No restart timing, camera decision,
AI branch or clock behaviour reads it — a port with no crowd audio can drop it.

### 18.3 The restart hand-over (`breakCameraMode`)

After any stoppage, a 9-state machine (run from
`UpdateGameTimersAndCameraBreakMode`) walks the world back to live play. It is
skipped entirely while the subs menu is open or a substitute is walking on.

It also only ticks when `gameStatePl` is neither `ST_GAME_IN_PROGRESS` nor
`ST_WAITING_ON_PLAYER` (`ST_STOPPED` routes to the half/match-transition
dispatcher instead), **and only once `stoppageEventTimer` has counted down to
zero** — so the reload armed in state 0 is a hard pause of the entire machine,
not just a note on state 0. Without that gate the restart sequence runs ~50
frames early and the goal-celebration pause disappears.

One further gate runs **before** the `stoppageEventTimer` test: while `gameState`
is in `[ST_STARTING_GAME .. ST_GAME_ENDED]`, a press on either team's
`firePressed` (or on `pl1Fire`/`pl2Fire` for a human-coached side, or on either
pad during `ST_RESULT_AFTER_THE_GAME`) runs the end-game transmission handler
*instead*. That is how a human dismisses the half-time and full-time screens, and
it is the consumer of the `firePressed = 1` the CPU-vs-CPU AI sets in §16.2.

The handler only has arms for **four** of the ten in-range states:

```
ST_RESULT_ON_HALFTIME      negateTimers(); SetCameraMovingToShowerState();
                           PrepareForInitialKick()   /* the double step is what
                                                        makes fire skip the walk
                                                        to the tunnel */
ST_RESULT_AFTER_THE_GAME   negateTimers(); playGame = 0
ST_STARTING_GAME           showFansCounter = 0; PrepareForInitialKick()
ST_CAMERA_GOING_TO_SHOWERS PrepareForInitialKick()
```

all four then zero `stoppageEventTimer` and return. For the other six in-range
states the handler does nothing and **falls through** into the stoppage
countdown below (the last `cmpi/bne` arm lands on `game_interrupted`,
SWOS2.s:25267), so holding fire during e.g. `ST_FIRST_HALF_ENDED` changes
nothing — the countdown runs as if fire were up. Do not wire fire to "advance
the transition" for all ten, and do not return after the handler either.
*(Revisions before 2.0 said the dispatcher returns and fire **freezes** the
countdown; that was a port bug — a held button delayed `PlayersLeavingPitch` by
as long as it was held — found by the replay rig.)*

The countdown itself lives at the top of the dispatcher:
`if (stoppageEventTimer) { stoppageEventTimer -= lastFrameTicks;
if (stoppageEventTimer > 0) return; stoppageEventTimer = 0; }` — nothing else
decrements it.

```
state -1 (0xFFFF): freshly stopped. gameStatePl is still ST_STOPPED, so the
         dispatcher routes to the stopped-state handler (§21.4a); for any state
         that is not a half/match transition that handler copies
         gameStatePl = gameState — so gameStatePl now holds ST_CORNER_LEFT,
         ST_THROW_IN_*, ... for the rest of the hand-over, which is why every
         `gameStatePl != ST_GAME_IN_PROGRESS` gate elsewhere sees the restart
         TYPE rather than ST_STOPPED — arms timeVar = 31000 for a keeper's ball
         but only while BOTH inSubstitutesMenu == 0 and
         cameraLeavingSubsTimer == 0, and sets breakCameraMode = 0.

state 0: wait for the ball to stop moving (deltaX == deltaY == 0)
         -> 1, and arm stoppageEventTimer = gsStoppageEventReload (50)
            (75 if this was a goal), except for a keeper's ball

state 1: if this was a goal and auto-highlights/auto-replays are on,
         request the capture/replay now (skipped during a shoot-out AND while
         the substitutes menu is open — the latter stops a highlight being
         captured of a menu overlay)
         currentScorer = 0
         if (playingPenalties && gameState != ST_PENALTIES) STAY in state 1
                 — the machine parks here between shoot-out kicks until
                   NextPenalty() sets gameState = ST_PENALTIES
         place the ball on the restart spot (unless the keeper has it)
         -> 2

state 2: wait for any card animation to finish and the camera to be valid
         goalScored = 0
         FOR A KEEPER'S BALL THE HAND-OVER COMPLETES HERE:
                 gameStatePl = ST_WAITING_ON_PLAYER ; inGameCounter = 0
                 breakState  = gameState
         mark each team's players destReachedState = 1, but only for a team
                 whose resetControls == 0
         (for a keeper's ball, force both keepers to destReachedState = 3)
         -> 4   /* which, for a keeper's ball, never executes: the dispatcher now
                   returns early on ST_WAITING_ON_PLAYER, so states 4/3/5/6/7/8
                   do not run and their keeper's-ball special cases are dead
                   defensive code */

state 4: wait for the subs menu to close -> 3

state 3: wait for the referee to have left (refState == 0) and for the
         injuriesForever flag to be clear, THEN wait until every ON-SCREEN
         player has destReachedState == 3
         (all players, on-screen or not, for kick-offs and penalties)
         runSlower = 0 ; if this was a goal, reset the crowd patterns
         -> 5

state 5: whichCard = 0 ; bookedPlayer = NULL -> 6

state 6: ballOutOfGameTimer = 0 ; arm the "time" HUD (timeVar = 31000) unless
         this is a keeper's ball -> 7

state 7: team.ballInPlay = 1
         ballOutOfGameTimer++
         if (ballOutOfGameTimer >= gsBallOutOfGameTimer (500))
                 -> hand the ball to the keeper (anti-stall) and restart
         if (no controlled player yet) stay here
         give the controlled player its standing/throw-in animation
         team.ballOutOfPlay = 1
         arm the result banner ; -> 8

state 8: blow the whistle (except for a keeper's ball)
         goalCameraMode = 0
         gameStatePl = ST_WAITING_ON_PLAYER
         inGameCounter = 0 ; breakState = gameState
```

`destReachedState` is the handshake: `1` when the restart is armed, `2` once the
player has been ticked by `UpdatePlayers`, `3` once `movePlayer` sees it has
actually stopped on its destination. State 3 waits for all of them, which is why
SWOS restarts wait for the formation to settle.

**Why that cannot deadlock.** The `1 → 2` step happens in exactly one place — the
idle branch (§8.8) — and the controlled player, the pass target and anyone in a
non-`PL_NORMAL` state never reach it. That looks like it should pin them at `1`
forever and stall state 3. It does not, because `writeRestartGlobalsAndStopPlayers`
calls `StopAllPlayers()`, which clears **both** teams' `controlledPlayer` and
`passToPlayerPtr` at the start of every break: during a hand-over every player is
therefore idle and does get ticked, one per team update via the round-robin. A
port that keeps a controlled player alive across a restart *will* deadlock here.

`ST_WAITING_ON_PLAYER` then simply waits for the restart-taker to press fire
(§8.3-A), or for `gsKickoffStoppage` (750 frames) to elapse for a CPU team, at
which point `PrepareForInitialKick` re-arms.

### 18.4 Restart formations

While the ball is dead, idle players are positioned from a per-state table
rather than from the tactics grid (§8.8):

```
table = (this team is the restarting team) ? bottomBallOutOfPlayPositions
                                           : topBallOutOfPlayPositions
positions = table[gameState]                      /* 32 entries, some NULL */
if (!positions) -> fall back to the tactics grid

entry = positions[playerOrdinal - 1]              /* {int16 x, int16 y}    */
if (entry.x == 22222) -> fall back to the tactics grid    /* "not placed"  */

if (team == topTeamData) {   /* the PLAYER's team — NOT which table was picked.
                                The two decisions are orthogonal: the bottom team
                                can be reading the top table and still mirror. */
    if (entry.x <= -1000) { x = entry.x + 1000 + fkOffset ; relative to the ball }
    else if (entry.x >= 1000) { x = entry.x - 1000 + fkOffset ; relative to the ball }
    else { dest = (entry.x + 5 + 81, entry.y + 129) }          /* absolute  */
} else {
    /* mirrored: negate both axes for ball-relative, or
       x = 509 - entry.x - 5, y = 640 - entry.y for absolute */
}
```

So an entry is either an **absolute pitch position** (offset by the pitch origin
`(81,129)`) or, when biased by ±1000, a **ball-relative** offset. `22222` means
"this player is not part of this set piece — use the normal tactics".

**Free-kick walls.** For `ST_FREE_KICK_LEFT2/3`, `_CENTER`, `_RIGHT1/2` (i.e.
not the two outermost), the *defending* team's players with ordinals 2..5 form a
wall: their table index is replaced by a running count of non-sent-off teammates
before them, and an X offset is computed from

```
freeKickFactorsX[7] = { 0, 3, 4, 5, 4, 3, 0 }     /* indexed by gameState - 6 */
fkOffset = factor * 4
for each of the first four outfield players still on the pitch: fkOffset -= factor
```

which spreads the wall symmetrically about the ball and narrows it as players
are sent off.

The full position tables are in `src/swos_data_typed_stubs.c` (42 arrays of 11
`{x,y}` pairs). They are pure data — transcribe them verbatim.

---

## 19. The referee

A single sprite with a six-state machine (`refState` 0..5), driven from
`UpdateReferee()`.

```
0 = idle (invisible)   1 = running on   2 = waiting at the player
3 = about to show the card                4 = booking   5 = leaving
```

**`ActivateReferee` has exactly two call sites**, both in the foul resolver
(§14.2), and both *after* the restart has been set up:

```
issue_yellow:
    if (TryBookingThePlayer(player, team) != 0)   -> foul_no_card
    TestFoulForPenaltyAndFreeKick(victim, team)   /* §14.2a */
    ActivateReferee()
    return
try_red:
    if (TrySendingOffThePlayer(player, team) != 0) -> foul_no_card
    TestFoulForPenaltyAndFreeKick(victim, team)
    ActivateReferee()
    return
foul_no_card:
    TestFoulForPenaltyAndFreeKick(victim, team)   /* NO referee */
```

So a **refused** card (either routine returning non-zero — not a human-managed
team, no injury allowance left, already carded) does *not* fall through from
yellow to red and does *not* summon the referee: it takes the plain restart. This
is the only way `refState` leaves 0, which is why `UpdateReferee` can return
immediately on `refState == 0` and still be correct.

```
ActivateReferee():                   /* called when a foul earns a card */
    dest = (foulX + 28, foulY + 5)
    jitter = Rand() >> 3 ; if (foulX >= 336) jitter = -jitter
    entry.y = (foulY <= 449) ? cameraY + 287 : cameraY - 20   /* off-screen edge */
        /* 287 = viewH + 15 (DOS: 215 at 200 lines); the -20 is measured from
           the top edge and is the same in both releases — §25.4 */
    pos = (dest.x + jitter, entry.y)
    speed = kRefereeSpeed (1024) ; visible = 1
    set the "coming" animation ; refState = 1

UpdateReferee():                     /* every frame */
    if (refState == 0 || !visible) return
    if (on screen) step the animation walker (same command language as §9.3)
    if (refState == 3) { refState = 4 ; select the yellow / red / second-yellow
                         animation by whichCard ; return }
    if (refState == 1) -> move
    if (refState != 5) return       /* states 2 and 4 are INERT here: 2 -> 3 is
                                       driven by the booked player (§8.8), and 4
                                       is ended by UpdateBookedPlayerNumberSprite */
    /* LEAVING: the deactivate test comes BEFORE the move */
    if (off screen) { refState = 0 ; visible = 0 ; rebuild the sprite list ; return }

move:
    recompute deltas toward dest at kRefereeSpeed
    if the 8-way direction changed, restart the "coming" animation
    integrate and clamp per axis (identical to movePlayer)
    if (arrived) { refState = 2 ; direction = 6 ; "waiting" animation }
        /* this tail is SHARED by states 1 and 5, so a LEAVING referee who
           reaches his destination while still on screen is put back into
           "waiting" and never leaves until something re-arms him — a genuine
           quirk, not a bug to fix */
```

The booked player walks to the referee: §8.8 sends him to
`(foulX + 21, foulY)`, and when he arrives and `refState == 2`, the player's own
update sets `refState = 3`, gives him the card-receiving animation, sets
`playerState = PL_BOOKED`, `playerDownTimer = 1` and blows the whistle.

`UpdateBookedPlayerNumberSprite()` then runs the card display from a
time-sliced table:

```
refTimer += lastFrameTicks
entry = (whichCard == CARD_YELLOW ? yellowCardTable
       : whichCard == CARD_RED    ? redCardTable
                                   : secondYellowTable)[refTimer >> 3]

entry == 1 -> show the yellow card sprite above the player   /* UNREACHABLE */
entry == 2 -> show the red card sprite                       /* UNREACHABLE */
entry <  0 -> the sequence is over:
                SetRefereeToLeavingState()
                if (whichCard & 2) SendPlayerAway(bookedPlayer, lastTeamBooked)
                whichCard = 0 ; bookedPlayer = NULL
entry == 0 -> show nothing this slice
otherwise  -> show the player's shirt-number sprite
```

The three tables are `{0,9,0,9,0,9,0,9,0,9,0,9,0,0,…,-1}` — six flashes of the
shirt number at 8-frame resolution, then a pause, then the terminator (index 29
for a yellow/red, index 44 for a second yellow, so a second yellow lingers
longer).

**The first two arms of that decoder are dead, and this overlay never draws a
card.** Every entry in all three tables is `0`, `9` or `-1`, so `entry == 1` and
`entry == 2` can never fire and `SPR_YELLOW_CARD (1178)` / `SPR_RED_CARD (1177)`
are unreachable constants (verified against `SWOS2.s:16629-16641` — the tables are
byte-for-byte as listed in §24.6, and the `9` is just a "draw something" marker,
not the shirt number, which comes from `PlayerInfo.shirtNumber`). Implement the
arms if you want structural fidelity, but do not expect them to run.

The card the player actually *sees* comes from the **referee's own animation**, not
from this table: `refState == 3` selects `refYellowCardAnimTable`,
`refRedCardAnimTable` or `refSecondYellowAnimTable` by `whichCard` before entering
state 4. The overlay sprite driven above is only the flashing shirt number beside
the booked player. Wire the animation selection, or your referee will book people
with empty hands.

`SetRefereeToLeavingState`'s X jitter (`(Rand() >> 2) - 32`, i.e. `-32..+31`) is
added to the referee's **current** `x`, not to `foulX` and not to `destX`. It sends
the referee to `y = 770` (if the foul was in
the top half) or `y = 129`, with a random X jitter of `-32..+31`.

---

## 20. Camera

`MoveCamera()` runs once per frame, before the pitch is drawn. It produces an
integer camera position `(cameraX.s.i, cameraY.s.i)` in world units; the view is
`336 × 272`.

### 20.1 Priority ladder

```
if (showFansCounter)                       return           /* crowd cutaway */
/* EVERY branch below except the tracking camera also zeroes BOTH velocity
   accumulators, every frame it is taken — the ±40 lead does not survive a
   booking, a penalty or the bench camera */
if (whichCard)          target = the booked player's position ; margin = 37
else if (playingPenalties) target = (336, 107)               ; margin = 0
else if (waitForPlayerToGoInTimer) -> bench camera           ; margin = 51
else if (leavingSubsMenu)  target = (211, 449)               ; margin = 37
else if (inSubstitutesMenu) -> bench camera. The margin DEFAULTS to 37; 51 and 0
                               are reachable only while cameraY is in [303,323]
                               (= 449 - viewH/2 ± 10; DOS [339,359] — §25.4):
                               51 if a substitution is walking on, 0 if neither
                               goal sprite is on screen, else 37.
                               (There is no separate "goal camera" target.)
else -> the tracking camera (below)
```

### 20.2 Velocity tracking

The tracking camera does **not** follow the ball directly; it accumulates a
velocity from the *sign* of the ball's motion, which is what gives SWOS its
characteristic lead-and-settle feel.

```
if (!live) {
    /* before a restart, lead in the direction the taker is facing */
    dir = lastTeamPlayedBeforeBreak.controlledPlayer.direction, else cameraDirection
    if (dir >= 0) (dx, dy) = nextCameraDirections[dir]   /* -1, 0 or +1 per axis */
    else          (dx, dy) = (0, 0)   /* no velocity accumulated this frame; the
                                         existing lead is HELD, not reset.
                                         Every half/match transition writes
                                         cameraDirection = -1 (§21.4), so this
                                         arm is common. Keep the guard: the
                                         engine never indexes the table with -1,
                                         and a port that drops the test and does
                                         reads out of bounds. */
} else {
    (dx, dy) = (ball.deltaX, ball.deltaY)
}

if (dx > 0 && cameraXVelocity != +40) cameraXVelocity += 2
if (dx < 0 && cameraXVelocity != -40) cameraXVelocity -= 2
if (dy > 0 && cameraYVelocity != +40) cameraYVelocity += 2
if (dy < 0 && cameraYVelocity != -40) cameraYVelocity -= 2
```

so the lead ramps to ±40 units over 20 frames and holds.

```
nextCameraDirections[8] = { (0,-1), (1,-1), (1,0), (1,1),
                            (0, 1), (-1,1), (-1,0), (-1,-1) }
```

### 20.3 Target and clamping

```
margin = 63                                     /* default */
if (!live && gameState is a CORNER or a THROW_IN) margin = 37

/* fixed targets for the ceremonial states */
if (gameState in [ST_STARTING_GAME .. ST_GAME_ENDED]) {
    ST_STARTING_GAME / ST_CAMERA_GOING_TO_SHOWERS /
    ST_GOING_TO_HALFTIME / ST_PLAYERS_GOING_TO_SHOWER -> (0x24E, 0x1C1) = (590,449) tunnel
    ST_RESULT_AFTER_THE_GAME                          -> (0x150, 0x81)  = (336,129)
    ST_FIRST_HALF_ENDED                               -> the ball
    ST_GAME_ENDED                                     -> the ball, or (336,449)
    anything else                                     -> (336, 449)
    (the three FIXED targets also zero both camera velocities; the two
     ball-tracking cases — ST_FIRST_HALF_ENDED, and ST_GAME_ENDED with
     penaltiesState >= 0 — keep the accumulated velocity lead so the camera
     still leads a ball that is rolling at the whistle)
} else target = the ball position

/* centre the view on the target, then apply the lead */
targetX -= 336/2 ; targetY -= 136
targetX += cameraXVelocity ; targetY += cameraYVelocity

/* keep the view on the pitch */
panRange = 672 - viewWidth                       /* 336 in the original */
clamp targetX to [margin, panRange - margin]
clamp targetY to trainingGame ? [0x50, 0x220] : [0x10, 0x250]
```

### 20.4 Smoothing

```
dx = targetX - cameraX.i ; dy = targetY - cameraY.i
if (dx == 0 && dy == 0) return

fx = 0x4000 * (dx >> 2)                          /* = dx/16 in 16.16       */
fy = 0x4000 * (dy >> 2)
clamp fx, fy to ±0x00050000                      /* max 5 units per frame  */

newX = cameraX.raw + fx ; newY = cameraY.raw + fy
cameraX.frac = low 16 bits of newX               /* keep sub-unit accuracy */
cameraY.frac = low 16 bits of newY
clamp (newX >> 16) to [0, panRange] and (newY >> 16) to [0x10, 0x260]
SetCameraCoordinates(that integer position)
```

`0x4000` is 0.25 in 16.16, so `0x4000 * (dx >> 2)` moves the camera **`dx/16`**
units per frame — a first-order lag with a time constant of ~16 frames (the error
decays by 15/16 each frame), hard-limited to 5 units per frame. The cap engages
at `dx >= 80` — and, because `>>` floors toward −∞, already at `dx <= -77` — so
it is very slightly asymmetric. Both the ramped velocity lead *and* the lagged
follow are needed; either alone feels wrong.

`cameraCoordinatesValid` is cleared here and set again by the scroll routine;
the restart state machine (§18.3, state 2) waits for it so the formation is not
placed while the camera is still catching up.

---

## 21. Match clock, halves, extra time, penalties

### 21.1 The clock

The match clock counts **in-game minutes 0..120** and is stored as three
separate BCD-ish digit bytes inside a 32-bit word (`gameTime`, big-endian:
byte1 = hundreds, byte2 = tens, byte3 = ones).

```
UpdateTime():
    make the time sprite visible

    if (gameSeconds < 0)  -> "last minute in progress" (below)
    if (!live || playingPenalties) -> just rebuild the sprite

    secondsSwitchAccumulator -= timeDelta
    if (secondsSwitchAccumulator >= 0) -> just rebuild the sprite
    secondsSwitchAccumulator += gsFramesPerGameSecond      /* 49 (DOS 70) */
    gameSeconds++
    if (gameSeconds != 60) -> just rebuild the sprite
    gameSeconds = 0
    increment the ones digit, carrying into tens and hundreds
```

The three "just rebuild the sprite" exits above all land on a `copyTimeSprite`
gate: the digit sprite is **recomposed only when `copyTimeSprite` is set** —
otherwise the routine merely repositions the existing sprite. The minute-boundary
arms (and the `EndFirstHalf` / `EndOfGame` / `StartPenalties` arms) jump *past*
the gate and rebuild unconditionally. `copyTimeSprite` is set **once per match**
(by `ResetTime`) and cleared as soon as the digits are composed, so composition
happens on that first frame and thereafter only on a minute boundary — about one
frame in 100 to 330 depending on the match-length setting, not one in five. A
port that recomposes every frame runs the whole sprite-blit path 50×/s for
nothing.

`timeDelta` is chosen once per match from the length setting:

```
gameLenSecondsTable[4] = { 30, 18, 12, 9 }      /* index 0..3 */
```

At 50 Hz with a reload of 49, one in-game second takes `49 / timeDelta` frames,
so a 90-minute match takes:

| Index | `timeDelta` | frames/in-game-second | real length |
|-------|-------------|-----------------------|-------------|
| 0     | 30          | 1.63                  | ≈ 3 minutes  |
| 1     | 18          | 2.72                  | ≈ 5 minutes  |
| 2     | 12          | 4.08                  | ≈ 7 minutes  |
| 3     | 9           | 5.44                  | ≈ 10 minutes |

### 21.2 Injury time

At each of the four boundary minutes the clock does **not** stop immediately;
it enters a "last minute" phase that waits for a natural break. **`gameSeconds`
is the two-pass flag that drives this**, and each boundary arm is entered
*twice* — the first time to arm the wait, the second time to actually end the
half:

```
at minute 45, 90, 105 or 120:
    if (gameSeconds >= 0) {
        /* FIRST pass — arm the wait and return. The half does NOT end. */
        gameSeconds = -1
        endGameCounter = gsEndGameCounter (50)
        -> just rebuild the sprite
    }
    /* SECOND pass — reached only from "last minute in progress" below, i.e.
       only once gameSeconds is already -1. */
    gameSeconds = 0            /* <-- THE ONLY WRITER THAT CLEARS THE -1.
                                  Omit it and the clock is dead from this
                                  boundary on: UpdateTime's very first test
                                  sends every later frame straight back into
                                  "last minute in progress", which re-arms
                                  endGameCounter forever. The match reaches
                                  45:00, whistles, kicks off the second half
                                  correctly — and the clock never ticks again. */
    whistle ; run this boundary's body (§21.3)

"last minute in progress":      /* entered from the top of UpdateTime whenever
                                   gameSeconds < 0 */
    endGameCounter -= lastFrameTicks
    if (endGameCounter < 0)      -> re-run the boundary tests (the SECOND pass)
    if (!live)                   -> re-arm and keep waiting
    if (ball.y <= 216 || ball.y >= 682) -> re-arm  (in a penalty area)
    if (ball is in the upper half && the BOTTOM team last played)  -> re-arm
    if (ball is in the lower half && the TOP team last played)     -> re-arm
    /* otherwise the counter keeps running down and the half ends */
```

So the whistle only goes when the ball is in midfield and the team *defending*
that half last touched it — never with an attack in progress.

Note that while `gameSeconds < 0` the seconds accumulator is **not** ticking and
the minute digits are **not** advancing: the `gameSeconds < 0` test is the *first*
thing `UpdateTime` does, ahead of the `live`/`playingPenalties` gate and ahead of
`secondsSwitchAccumulator`. The displayed clock therefore sits frozen on 45:00 /
90:00 / 105:00 / 120:00 for the whole of injury time. `gameSeconds` is also the
one value in the clock that is read as **signed** while being stored in an
unsigned word — `-1` is `0xFFFF`.

### 21.3 Boundaries

This whole block is reached from exactly two places: falling out of the
minute-boundary increment in §21.1, and the `endGameCounter < 0` exit of "last
minute in progress" in §21.2. It is **not** run every frame.

Each of the four *ending* minutes below therefore runs its body only on the
second of those two entries, because of the two-pass `gameSeconds` gate of §21.2
— `if (gameSeconds >= 0) { arm the wait; return }`, then `gameSeconds = 0` and
proceed. Read every "whistle" line as "on the second pass". Minutes 1 and 46 have
no gate, but they can only be reached by the digits rolling over, so each fires
exactly once per match (and `BumpPlayersLastPlayedHalfAtHalfStart` is idempotent
anyway — it sets `halfPlayed = 1` unless the player is already at `2` or has two
yellows).

```
minute   1 : mark every player as having started this half
minute  46 : same, for the second half
minute  45 : whistle ; EndFirstHalf() ; mark the half as played
minute  90 : whistle ; mark the half as played ; snapshot the score
             if (team1TotalGoals == team2TotalGoals)   /* THIS match, not the
                                                          aggregate — the
                                                          aggregate is applied
                                                          only inside */
             {
                 /* NOT an else-if chain — the away-goals block only DECIDES when
                    the two adjusted totals differ; if they are also equal it
                    FALLS THROUGH to the extra-time test below */
                 if (secondLeg && playing2ndGame == 1) {
                     team1adj = 2*statsTeam1Goals + team1GoalsFirstLeg
                     team2adj = 2*statsTeam2Goals + team2GoalsFirstLeg
                     if (team1adj != team2adj) -> the larger wins ; EndOfGame()
                     /* equal: fall through */
                 }
                 if (extraTimeState)      { extraTimeState = -1 ; StartFirstExtraTime() }
                 else if (penaltiesState) { penaltiesState = -1 ; StartPenalties() }
                 else { winningTeamPtr = NULL ; EndOfGame() }
             }
             else { winningTeamPtr = the leader ; EndOfGame() }
minute 105 : whistle ; EndFirstExtraTime()
minute 120 : whistle ; if (team1TotalGoals == team2TotalGoals) {
                           if (secondLeg && playing2ndGame)   /* NOT `== 1` */
                               the away-goals test, exactly as at minute 90
                           if (penaltiesState) { penaltiesState = -1 ; StartPenalties() }
                           else { winningTeamPtr = NULL ; EndOfGame() }
                       } else { winningTeamPtr = the leader ; EndOfGame() }
             NOT identical to minute 90: there is no extra-time arm, no score
             snapshot into statsTeamNGoalsCopy, and no half-played mark.
```

**The five match-option globals.** These are set up *before* the match by
whatever launched it (a friendly, a league fixture, a cup tie, a DIY round) and
are not written by the in-match code except as noted:

| Global | Meaning | Values |
|--------|---------|--------|
| `extraTimeState` | is extra time available as a tiebreaker? | `0` no · `1` yes, unused · `-1` yes, already consumed |
| `penaltiesState` | is a shoot-out available? | same tri-state |
| `secondLeg` | is this the second leg of a two-legged tie? | `0` / `1` |
| `playing2ndGame` | which leg is being played | `0`, or the competition's "play each team" count; the away-goals rule needs `== 1` at minute 90 |
| `team1GoalsFirstLeg` / `team2GoalsFirstLeg` | the first leg's score, for the aggregate | goals |

`extraTimeState` and `penaltiesState` are **tri-state**, and the two forms of the
test are both correct for their site. The boundary arms use the truthy form
(`if (extraTimeState)`), which accepts both `1` and `-1`; that is safe because the
arm's first act is to *set* the global to `-1`, and each boundary is only reached
once. Code that needs "is the shoot-out actually happening" uses
`penaltiesState < 0` instead — the camera's full-time behaviour is the example
(`src/move_camera.c:245`, `SWOS2.s:14364-14366`: at `ST_GAME_ENDED`, a negative
`penaltiesState` — the shoot-out has been played — **centres** the camera on
`(336, 449)` for the result; otherwise it stays on the ball, §20.3). A port that
collapses the tri-state to a boolean loses that distinction and will centre the
camera at the end of every match in which a shoot-out was merely *available*.
*(Revisions before 2.0 had this sentence the wrong way round; §20.3's table was
right.)*

All four "not enabled" defaults are `0`; a friendly or league match leaves every
one of them zero, so minute 90 goes straight to `winningTeamPtr = NULL` on a draw.

### 21.4 Half and match transitions

Each of these sets `gameStatePl = ST_STOPPED`, `breakCameraMode = -1`,
`cameraDirection = -1`, `lastTeamPlayedBeforeBreak = topTeamData`, zeroes both
stoppage timers and zeroes the camera velocity. Most call `StopAllPlayers()` —
but **`EndFirstHalf` and `EndOfGame` call `ResetBothTeamsPlayerPassingKicking()`
instead**, which clears the same eight team fields (§18.1) *without* snapping
every player's destination to his position. So players are **not** frozen at the
half-time or full-time whistle; they run on to wherever they were headed. They
differ in `gameState` and the arming timer. (`StartFirstExtraTime` and
`EndFirstExtraTime` are the two exceptions to the `lastTeamPlayedBeforeBreak`
rule: they pick the kick-off side with
`lastTeamPlayedBeforeBreak = (teamStarting == teamPlayingUp) ? topTeamData :
bottomTeamData`, the same rule `PrepareForInitialKick` uses. Getting this wrong
wedges the extra-time kick-off.)

| Function                | `gameState`                    | `stoppageEventTimer` |
|-------------------------|--------------------------------|----------------------|
| `StartingMatch`         | `ST_STARTING_GAME`             | 100                  |
| `EndFirstHalf`          | `ST_FIRST_HALF_ENDED`          | 100                  |
| `FirstHalfJustEnded`    | `ST_GOING_TO_HALFTIME`         | 250                  |
| `GoToHalftime`          | `ST_RESULT_ON_HALFTIME`        | 700                  |
| `SetCameraMovingToShowerState` | `ST_CAMERA_GOING_TO_SHOWERS` | 100             |
| `EndOfGame`             | `ST_GAME_ENDED`                | 150                  |
| `PlayersLeavingPitch`   | `ST_PLAYERS_GOING_TO_SHOWER`   | 250                  |
| `GameOver`              | `ST_RESULT_AFTER_THE_GAME`     | 1500                 |
| `StartFirstExtraTime`   | `ST_FIRST_EXTRA_STARTING`      | 100                  |
| `EndFirstExtraTime`     | `ST_FIRST_EXTRA_ENDED`         | 100                  |

Half time (`SetCameraMovingToShowerState`) flips the ends and walks both squads
back through the tunnel:

```
halfNumber     = 2
teamPlayingUp  = 3 - teamPlayingUp
teamStarting   = 3 - teamStarting
SetBallPosition(1672, 449)          /* park the ball off-world */
InitTeamsData()                     /* rebuild both TeamGeneralInfo blocks */
InitPlayersBeforeEnteringPitch()    /* line both squads up in the tunnel   */
```

`EndFirstExtraTime` performs only `halfNumber = 2`, the two `3 - x` flips and
`InitTeamsData()` — it does **not** park the ball and does **not** re-run the
tunnel line-up (doing so would teleport all 22 players off the pitch mid-match).
`StartFirstExtraTime` likewise omits both, and re-tosses `teamPlayingUp` and
`teamStarting` randomly (`(Rand() & 1) + 1`) instead of flipping them.

**Kick-off** (`PrepareForInitialKick`):

```
SetBallPosition(336, 449)
if (teamStarting == teamPlayingUp) { kickoff = topTeamData ; camDir = 4 ; flags = 0x7C }
else                               { kickoff = bottomTeamData ; camDir = 0 ; flags = 0xC7 }
gameState = ST_PLAYERS_TO_INITIAL_POSITIONS ; breakCameraMode = -1
gameStatePl = ST_STOPPED
foul = (336, 449)
lastTeamPlayedBeforeBreak = kickoff
StopAllPlayers()
```

**Tunnel line-up** (`InitPlayersBeforeEnteringPitch`) places all 22 players on a
diagonal outside the pitch using `kTeamsStartingCoordinates` (22 `{x,y}` pairs,
offset by `(591, 449)`) with a `Rand() & 7` X jitter per player, and resets each
sprite's state, direction, animation, and — only at `ST_STARTING_GAME` — its
cards, `sentAway` and `injuryLevel`.

### 21.4a The stopped-state dispatcher

§21.4 lists what each transition function *sets*; this is what decides which one
runs. While `gameStatePl == ST_STOPPED` — and only once `stoppageEventTimer` has
expired, and only if no fire press was consumed first (§18.3) —
`UpdateGameTimersAndCameraBreakMode` dispatches on `gameState`:

| `gameState`                    | action                                             |
|--------------------------------|----------------------------------------------------|
| `ST_FIRST_HALF_ENDED`          | `FirstHalfJustEnded()`                              |
| `ST_GOING_TO_HALFTIME`         | `negateTimers()` then `GoToHalftime()`              |
| `ST_RESULT_ON_HALFTIME`        | `negateTimers()` then `SetCameraMovingToShowerState()` |
| `ST_CAMERA_GOING_TO_SHOWERS`   | `PrepareForInitialKick()`                           |
| `ST_STARTING_GAME`             | `PrepareForInitialKick()`                           |
| `ST_FIRST_EXTRA_STARTING`      | `PrepareForInitialKick()`                           |
| `ST_FIRST_EXTRA_ENDED`         | `PrepareForInitialKick()`                           |
| `ST_GAME_ENDED`                | `PlayersLeavingPitch()`                             |
| `ST_PLAYERS_GOING_TO_SHOWER`   | `GameOver()`                                        |
| `ST_RESULT_AFTER_THE_GAME`     | `negateTimers()` then `playGame = 0` (match over)   |
| anything else (a restart)      | `gameStatePl = gameState` ; `breakCameraMode = 0`, plus `timeVar = 31000` for a keeper's ball — but only while **both** `inSubstitutesMenu == 0` and `cameraLeavingSubsTimer == 0` |

This is the chain that carries the match from minute 45 to the second-half
kick-off and from minute 90 to the result screen. Without it a port reaches a
half boundary, calls `EndFirstHalf()`, and stops.

`negateTimers()` **negates** `resultTimer`, `timeVar` and `statsTimer` rather
than clearing them — §21.4's `ShowResult` reads a negative `resultTimer` as "hide
the banner now" (§20 HUD), so sign is meaningful state, not a flag.

### 21.5 Penalty shoot-out

```
StartPenalties():
    penaltiesState = -1
    save the 90/120-minute score, then zero both scores and both digit pairs
    team1PenaltyGoals = team2PenaltyGoals = 0
    teamPlayingUp = (Rand() & 1) + 1 ; teamStarting = (Rand() & 1) + 1
    team1PenaltyShooterIndex = team2PenaltyShooterIndex = 11
    team1PenaltyAttempts = team2PenaltyAttempts = 0
    playingPenalties = 1 ; dontShowScorers = 1
    clear every player's cards and sentAway on both teams
    NextPenalty()

NextPenalty():
    total = team1PenaltyAttempts + team2PenaltyAttempts
    if (total >= 10) {
        if (attempts are equal && goals differ) -> the shoot-out is over
    } else {
        if (5 - team1Attempts + team1Goals < team2Goals) -> over   /* can't catch up */
        if (5 - team2Attempts + team2Goals < team1Goals) -> over
    }

    /* set up the next kick */
    ball.z = 0
    teamPlayingUp = 3 - teamPlayingUp ; teamStarting = 3 - teamStarting
    InitTeamsData()
    gameState = ST_PENALTIES ; breakCameraMode = -1
    cameraDirection = 0 ; playerTurnFlags = 0x83
    foul = (336, 187)                        /* always the upper spot */
    gameStatePl = ST_STOPPED
    lastTeamPlayedBeforeBreak = bottomTeamData
    StopAllPlayers() ; penaltiesTimer = 0
    shooterIndex--   ; if (shooterIndex == 0) shooterIndex = 10
    penaltyShooterSprite = bottomTeamData.spritesTable[shooterIndex]
    that team's attempt counter++

    over:
        playingPenalties = 0 ; restore the saved 90/120-minute score
        winningTeamPtr = the side with more penalty goals
        EndOfGame()
```

Shooters are taken from index 10 downward, wrapping at 0 → 10, so the eleventh
kick returns to the first taker. `penaltiesTimer` is **held at 0** while
`gameState == ST_PENALTIES` (the set-up phase) and only counts up *after* the
kick has been struck; `NextPenalty` is forced at `gsPenaltyTimer` (100 frames) if
the kick has still not resolved by then.

During penalties the keeper stops running its decision logic once
`penaltiesTimer` passes 50 — i.e. it behaves normally for the first 50 frames
*after* the kick and then freezes, not the other way round. The dive threshold
also changes (§15.3), and the keeper guesses a side from `currentGameTick` bits.

---

## 22. Statistics

`TeamStatisticsData` is seven counters per side:

```c
struct TeamStatisticsData {
    int16_t ballPossession;   /* raw frame count, converted to % on display */
    int16_t cornersWon;
    int16_t foulsConceded;
    int16_t bookings;
    int16_t sendingsOff;
    int16_t goalAttempts;
    int16_t onTarget;
};
```

Where each is incremented:

| Counter          | Incremented by                                                 |
|------------------|----------------------------------------------------------------|
| `ballPossession` | `UpdateAndDrawStatistics`, once per frame while live, for `lastTeamPlayed` |
| `cornersWon`     | `CheckIfBallOutOfPlay` when a corner is awarded                 |
| `foulsConceded`  | `PlayerTacklingTestFoul` on the fouling team                    |
| `bookings`       | `TryBookingThePlayer` (decremented again on a second yellow)    |
| `sendingsOff`    | second yellow or straight red                                   |
| `goalAttempts`   | when a keeper intervenes outside his 6-yard area (§15.5), and by the goal-attempt tracker |
| `onTarget`       | by the goal-attempt tracker when the attempt was on target      |

Possession percentage is `team.ballPossession * 100 / (team1 + team2)`.

The goal-attempt tracker lives inside `UpdateAndDrawStatistics`, which the frame
loop calls **last** (§5) — after `UpdateAndApplyTeamControls` and `UpdateBall`. It
reads `strikeDestX`, which is refreshed only by `UpdateBallVariables` inside the
keeper branch (§15.2), and only for a keeper whose penalty area the ball is in, who
was not the last toucher, and who passed the wrong-half exit. **`strikeDestX` is
therefore often stale by design** — it holds whatever the last qualifying keeper
update produced, possibly several frames earlier and possibly for the other end of
the pitch. Do not add a fresh prediction here; the staleness is part of the
behaviour, and refreshing it changes which shots get counted.

**Nothing stores which goal the attempt was against** — both the arm and the clear
re-derive it from the ball's current Y every frame, and they use *opposite* team
assignments because they ask opposite questions. Run one or the other, never both,
and only while `gameStatePl == ST_GAME_IN_PROGRESS` and `!playingPenalties`:

```
if (isGoalAttempt) {
    /* CLEAR TEST — "has the attack ended?" `team` is the DEFENDER. */
    team = topTeamData ; dY = ball.deltaY
    if (ball.y >= 449) { team = bottomTeamData ; dY = -dY }
    if (team == lastTeamPlayed || dY >= 0) isGoalAttempt = 0
        /* dY >= 0 after the flip == the ball is no longer closing on that goal;
           zero vertical velocity counts as "ended". */
} else if (ballInGoalkeeperArea) {
    /* ARM TEST — "has an attempt started?" `team` is the ATTACKER. */
    team = topTeamData ; dY = ball.deltaY
    if (ball.y > 449) { /* lower half: keep topTeamData, keep dY */ }
    else              { team = bottomTeamData ; dY = -dY }
    if (dY > 0 && team == lastTeamPlayed && !team.playerHasBall
        && strikeDestX >= 240 && strikeDestX <= 431) {
        attacker = (ball.y > 449) ? topTeamData : bottomTeamData
        if (strikeDestX >= 303 && strikeDestX <= 367) attacker.stats.onTarget++
        attacker.stats.goalAttempts++
        isGoalAttempt = 1
    }
}
if (gameStatePl != ST_GAME_IN_PROGRESS) isGoalAttempt = 0
```

Two details to copy exactly. The half test is `ball.y >= 449` on the clear and
`ball.y > 449` on the arm — an off-by-one at the centre line that is in the
original. And the two `team =` assignments are mirrored: in the *lower* half the
arm names `topTeamData` (who is attacking the bottom goal) while the clear names
`bottomTeamData` (who is defending it). Copying one test into the other inverts
the whole tracker.

---

## 23. Match initialisation order

```
StartMatchSequence(topTeam, bottomTeam):
    LoadSfxResources()
    playGame = 1
    SetPitchTypeAndNumber()
    InitGameSaveTeams(topTeam, bottomTeam)
    InitGameRestoreTeams()
    GameLoop()
```

```
InitGameSaveTeams(left, right):
    snapshot both TeamGame blocks (1018 bytes each) so a replay can restore them
    topTeamPtr = left ; bottomTeamPtr = right
    load the pitch, build the kit colours and name sprites
    wait for BOTH fire buttons to be released
    fade out
```

```
InitGameRestoreTeams():
    InitAdvertisements() ; CopyPlayerNameLengths()
    playerCardChance = playerCardChancesPerGameLength[gameLength][(Rand() & 0x1E) >> 1]
    restore both TeamGame blocks from the snapshots
    InitializePlayerSpriteFrameIndices()
    gameRandValue = Rand()
    ApplyTeamTactics(topTeamInGame) ; ApplyTeamTactics(bottomTeamInGame)
    CrowdResetMatchState()
    teamPlayingUp = (Rand() & 1) + 1 ; teamStarting = (Rand() & 1) + 1
    timeDelta = gameLenSecondsTable[gameLength]
    apply the pitch palette, then the PITCH PHYSICS for pitchType:
        pitchBallSpeedFactor  = kPitchBallSpeedInfluence  [pitchType]
        ballSpeedBounceFactor = kBallSpeedBounceFactorTable[pitchType]
        ballBounceFactor      = kBallBounceFactorTable    [pitchType]
    clear: playingPenalties, dontShowScorers, all stats overlays, subs state,
           both scores and digit pairs, aggregate goals (seeded from the first
           leg if secondLeg), sub counts, keeper-replaced flags, all 14 stats
    InitDisplaySprites() ; InitCurrentGoalHighlight() ; ResetTime()
    ResetResultSprites() ; ResetGoalAttempt() ; InitAnimatedPatterns()
    StartingMatch()
```

`InitTeamsData` is **not** a peer of the three boxes above — it runs at the tail
of `InitGameRestoreTeams`, inside `StartingMatch()`:

```
    StartingMatch():
        halfNumber = 1 ; SetBallPosition(1672, 449) ; hideBall = 0
        InitTeamsData()                     /* the box below                 */
        stoppageEventTimer = 100
        gameState       = ST_STARTING_GAME  /* OVERWRITES InitTeamsData's    */
        gameStatePl     = 101               /* ST_GAME_IN_PROGRESS defaults  */
        breakCameraMode = -1 ; cameraDirection = -1
        lastTeamPlayedBeforeBreak = topTeamData
        stoppageTimerTotal = stoppageTimerActive = 0
        StopAllPlayers() ; clear the camera velocity
        cameraMoveToShowerLatch = 1
        InitPlayersBeforeEnteringPitch()
```

so `InitTeamsData` must run *after* `teamPlayingUp` has been randomised (it uses
it to decide which struct is filled first) and *before* `StartingMatch` overwrites
the state it sets. Its other three call sites are the half, extra-time and
penalty transitions (§21.4).

```
InitTeamsData():           /* also called at every half/extra/penalty change */
    ApplyGameStyle()       /* write the 50 Hz or 70 Hz constant set          */
    clear every per-match global: currentScorer, goalScored, runSlower,
        whichCard, bookedPlayer, playerHadBall, lastKeeperPlayed,
        lastTeamPlayed, lastPlayerPlayed, penalty, goalCameraMode, goalOut,
        fireBlocked, lastTeamPlayedBeforeBreak, both stoppage timers,
        stoppageEventTimer, inGameCounter
    gameStatePl = gameState = ST_GAME_IN_PROGRESS
    breakCameraMode = -1
    for each side, fill TeamGeneralInfo:
        opponentsTeam, inGameTeamPtr, teamStatsPtr, spritesTable, teamNumber,
        playerNumber / playerCoachNumber / isPlCoach, tactics
        updatePlayerIndex = 10
        spinTimer = -1 ; ballControllingPlayerDirection = -1
        everything else = 0
    /* if teamPlayingUp != 1 the two destination structs are swapped, which is
       how the same squad plays "up" or "down" in the second half            */
```

`ApplyTeamTactics` maps the 11 starting players onto the formation slots of
`playersPositionsByTactics[tactics]` (an 11-entry slot order per formation) and
sets each `PlayerInfo.position`.

Two side-effects of `InitTeamsData` are easy to miss and both break the match if
omitted: it sets `gameState`/`gameStatePl` to **live**, which
`StartingMatch`/`PrepareForInitialKick` then immediately overrides, and it
resets `updatePlayerIndex` to 10 so the round-robin starts at player 0.

---

## 24. Complete constant tables

All values are the **Amiga** originals. Hex is given where the original source
used hex (usually because the value is a 16.16 fixed-point constant).

### 24.1 Scalars

**Ball physics**

| Name                       | Value      | Meaning                                     |
|----------------------------|------------|---------------------------------------------|
| `kBallGroundConstant`      | 16         | speed lost per frame on the ground          |
| `kBallAirConstant`         | 10         | speed lost per frame while airborne         |
| `kGravityConstant`         | `0x1200` (4608) | deltaZ lost per frame = 0.0703 units/frame² |
| `kBallKickingSpeed`        | `0x08A0` (2208) | base speed of a struck ball            |
| `kBallKickingDeltaZ`       | `0x00014000` (1.25) | base launch rate of a struck ball  |
| `kNormalKickBallSpeed`     | `0x0A00` (2560) | aftertouch "normal" kick speed         |
| `kNormalKickDeltaZ`        | `0x00016000` (1.375) | aftertouch "normal" launch        |
| `kHighKickBallSpeed`       | `0x0A80` (2688) | aftertouch "high" kick speed           |
| `kHighKickDeltaZ`          | `0x00020000` (2.0) | aftertouch "high" launch            |
| `kFreePassReleasingBallSpeed` | 1792    | pass with no target in that direction       |
| `kShotAtGoalMinumumSpeed`  | `0x0200` (512) | keeper treats it as a shot above this   |

**Player speeds**

| Name                            | Value            |
|---------------------------------|------------------|
| `kPlayerGroundConstant`         | `0x0060` (96)    |
| `kPlayerAirConstant`            | `0x0048` (72)    |
| `kPlayerTacklingSpeed`          | `0x0700` (1792)  |
| `kJumpHeaderSpeed`              | `0x0800` (2048)  |
| `kStaticHeaderPlayerSpeed`      | `0x0100` (256)   |
| `kStaticHeaderBaseBallSpeed`    | `0x0700` (1792)  |
| `kSubstitutedPlayerSpeed`       | `0x0600` (1536)  |
| `kInjuryForeverSpeed`           | `0x0300` (768)   |
| `kRefereeSpeed`                 | `0x0400` (1024)  |

**Header launch rates**

| Name                    | Value                     |
|-------------------------|---------------------------|
| `kBallJumpHeaderDeltaZ` | `0x0000A000` (0.625)      |
| `kHeaderLowJumpHeight`  | `0x00020000` (2.0)        |
| `kHeaderHighJumpHeight` | `0x00024000` (2.25)       |

**Goalkeeper**

| Name                                 | Value            |
|--------------------------------------|------------------|
| `kGoalkeeperGameSpeed`               | `0x0400` (1024)  |
| `kGoalkeeperSpeedWhenGameStopped`    | 1024             |
| `kGoalkeeperMoveToBallSpeed`         | `0x0400` (1024)  |
| `kGoalkeeperBurstSpeed`              | 512              |
| `kGoalkeeperCatchSpeed`              | `0x0300` (768)   |
| `kGoalkeeperNearJumpSpeed`           | 1024             |
| `kGoalkeeperFarJumpSpeed`            | `0x0800` (2048)  |
| `kGoalkeeperFarJumpSlowerSpeed`      | `0x0500` (1280)  |
| `kGoalKeeperRecoveryStepBase`        | `0x0070` (112)   |
| `kGoalKeeperRecoveryStepAlt`         | `0x00C0` (192)   |
| `kGoalKeeperRecoveryStepFar`         | `0x0080` (128)   |
| `kKeeperSaveDistance`                | 24               |
| `kKeeperPenaltySaveDistanceFar`      | `0x14` (20)      |
| `kKeeperPenaltySaveDistanceNear`     | 12               |
| `kGoalkeeperStrongDeflectBallSpeed`  | `0x0600` (1536)  |
| `kGoalkeeperMediumDeflectBallSpeed`  | `0x0400` (1024)  |
| `kGoalkeeperWeakDeflectBallSpeed`    | `0x0200` (512)   |
| `kGoalkeeperDeflectDeltaZ`           | `0x0000C000` (0.75) |

**Passing speed by squared distance to the target**

| Name                             | Value | Range (squared)    |
|----------------------------------|-------|--------------------|
| `kPassingSpeedCloserThan2500`    | 1536  | < 2500             |
| `kPassingSpeed_2500_10000`       | 1664  | 2500 … 9999        |
| `kPassingSpeed_10000_22500`      | 1792  | 10000 … 22499      |
| `kPassingSpeed_22500_40000`      | 1877  | 22500 … 39999      |
| `kPassingSpeed_40000_62500`      | 1962  | 40000 … 62499      |
| `kPassingSpeed_62500_90000`      | 2048  | 62500 … 89999      |
| `kPassingSpeed_90000_122500`     | 2133  | 90000 … 122499     |
| `kPassingSpeedFurtherThan122500` | 2218  | ≥ 122500           |

### 24.2 Direction offset tables

```c
/* one (dx, dy) pair per 8-way direction */

/* where the ball sits relative to the player carrying it */
kBallPlOffsets[16]        = { 0,-1,  1,-1,  1,0,  1,1,  0,1, -1,1, -1,0, -1,-1 };
/* the exact inverse: where the player stands relative to the ball */
kPlayerWithBallOffsets[16]= { 0, 1, -1, 1, -1,0, -1,-1, 0,-1, 1,-1,  1,0,  1, 1 };

/* the "1000 units that way" ray used for every aim and every run */
kDefaultDestinations[16]  = { 0,-1000,  1000,-1000,  1000,0,  1000,1000,
                              0, 1000, -1000, 1000, -1000,0, -1000,-1000 };

/* 32-direction lead table used to aim a receiver onto a curling pass;
   indexed by ((angle & 0xFF) >> 3) << 1 */
kBallFriction[64] = {
        0,-1000,  199,-1000,  414,-1000,  668,-1000,
     1000,-1000, 1000, -668, 1000, -414, 1000, -199,
     1000,    0, 1000,  199, 1000,  414, 1000,  668,
     1000, 1000,  668, 1000,  414, 1000,  199, 1000,
        0, 1000, -199, 1000, -414, 1000, -668, 1000,
    -1000, 1000,-1000,  668,-1000,  414,-1000,  199,
    -1000,    0,-1000, -199,-1000, -414,-1000, -668,
    -1000,-1000, -668,-1000, -414,-1000, -199,-1000 };
```

### 24.2a Aftertouch spin tables

Both are 8 directions × 4 entries = 32 signed words. The index is
`direction * 4 + (side >> 1)`, where `side` is `0` for left spin and `4` for
right (§12.5) — so `[+0]` / `[+1]` is the left-spin `(dx, dy)` pair and
`[+2]` / `[+3]` the right-spin pair. The chosen pair is multiplied by
`kSpinMultiplierFactor[spinTimer]` and *added to the ball's destination*.

`kKickSpinFactor` is indexed by `controlledPlDirection` (shots); the weaker
`kPassingSpinFactor` by `ball.direction` (passes).

```c
kKickSpinFactor[32] = {
    -32,   0,   32,   0,     0, -23,  23,   0,
      0, -32,    0,  32,    23,   0,   0,  23,
     32,   0,  -32,   0,     0,  23, -23,   0,
      0,  32,    0, -32,   -23,   0,   0, -23 };

kPassingSpinFactor[32] = {
    -16,   0,   16,   0,     0, -11,  11,   0,
      0, -16,    0,  16,    11,   0,   0,  11,
     16,   0,  -16,   0,     0,  11, -11,   0,
      0,  16,    0, -16,   -11,   0,   0, -11 };
```

### 24.3 Set-piece aim tables

```c
kLeftThrowInBallDestDelta[16]  = {  250,-1000, 1000,-1000, 1000,0, 1000,1000,
                                    250, 1000,-1000, 1000,-1000,0,-1000,-1000 };
kRightThrowInBallDestDelta[16] = { -250,-1000, 1000,-1000, 1000,0, 1000,1000,
                                   -250, 1000,-1000, 1000,-1000,0,-1000,-1000 };
kPenaltyBallDestDelta[16]      = {    0,-1000,  500,-1000, 1000,0,  500,1000,
                                      0, 1000, -500, 1000,-1000,0, -500,-1000 };
kUpperLeftCornerBallDestDelta[16]  = {   0,-1000, 1000,-1000, 1000, 150, 1000, 300,
                                       250, 1000,-1000, 1000,-1000,   0,-1000,-1000 };
kUpperRightCornerBallDestDelta[16] = {   0,-1000, 1000,-1000, 1000,   0, 1000,1000,
                                      -250, 1000,-1000,  350,-1000, 150,-1000,-1000 };
kLowerLeftCornerBallDestDelta[16]  = { 250,-1000, 1000, -350, 1000,-150, 1000,1000,
                                         0, 1000,-1000, 1000,-1000,   0,-1000,-1000 };
kLowerRightCornerBallDestDelta[16] = {-250,-1000, 1000,-1000, 1000,   0, 1000,1000,
                                         0, 1000,-1000, 1000,-1000,-150,-1000, -350 };
```

### 24.4 Quadrant tables

```c
ballXQuadrantLimits[5]  = { 183, 285, 387, 489, 129 };   /* [4] = Y band 0's
                                                             leading bound (§10.8) */
ballYQuadrantLimits[6]  = { 220, 312, 403, 495, 586, 678 };

playerXQuadrantsCoordinates[15] = { 98, 132, 166, 200, 234, 268, 302, 336,
                                   370, 404, 438, 472, 506, 540, 574 };
playerYQuadrantCoordinates[16] = { 149, 189, 229, 269, 309, 349, 389, 429,
                                   469, 509, 549, 589, 629, 669, 709, 749 };

/* editor-space variants, used by the tactics editor rather than the match */
ballXQuadrantCoordinates[5] = { 12, 38, 64, 90, 116 };
ballYQuadrantCoordinates[7] = { 12, 40, 69, 97, 126, 154, 181 };
xQuadrantToCoordinates[15]  = { 3,12,21,29,38,47,55,64,73,81,90,99,107,116,125 };
yQuadrantToCoordinates[16]  = { 5,14,24,36,50,64,79,93,107,121,136,150,164,176,186,195 };
```

### 24.5 Pitch condition

Pitch types are `0..6` = FROZEN, MUDDY, WET, SOFT, NORMAL, DRY, HARD.

```c
/* added to ground friction when nobody is carrying the ball */
kPitchBallSpeedInfluence[7]   = { -2,  2,  3,  0,  0, -1, -1 };   /* DOS: -3, 4, 1, ... */
/* speed lost on each bounce: speed -= (speed * f) >> 8           */
kBallSpeedBounceFactorTable[7]= { 24, 80, 80, 72, 64, 40, 32 };
/* deltaZ damping on each bounce                                  */
kBallBounceFactorTable[7]     = { 88,112,104,104, 96, 88, 80 };
```

Frozen keeps almost all pace and bounce; muddy kills both.

### 24.6 Player skill tables

Most are indexed by a skill value 0..7, but **not all** — several in this block
are indexed by something else entirely, and each such case is noted inline below.
The exceptions:

| Table | Indexed by |
|-------|-----------|
| `kKeeperEaseBallTable[8]` | `(currentGameTick & 0xE) >> 1` — a frame counter |
| `goalScoredChances[16]` | `finishing - goalieSkill + 7` → 0..14 |
| `kInjuriesSpeedHandicap[8]` | injury severity `(inj & 0xE0) >> 5` |
| `kInjuryLevels[7]` / `…AlreadyInjured[7]` | the severity bucket walk, 0..6 |
| `kInjuryRecoveryTime[8]` | `(severity & 0xE0) >> 5` |
| `kTackleInjuryProbability[4]` | match-length setting 0..3 |
| `kSpinMultiplierFactor[10]` | `spinTimer`, 0..9 |
| `cardAnimStates*[5]` | `player.cards + 1` (booking) / `(player.cards ^ 1) + 3` (sending-off) — §14.4 |

```c
/* movement */
kPlayerSpeedsGameInProgress[8] = { 928, 974,1020,1066,1112,1158,1204,1250 };
kPlayerSpeedsGameStopped[8]    = {1136,1152,1168,1184,1200,1216,1232,1248 };
kInjuriesSpeedHandicap[8]      = { 0,-96,-128,-160,-192,-224,-256,-288 };

/* passing */
kBallSpeedPassingIncrease[8]   = { 0, 48, 96,144,192,256,320,384 };
kAIFailedPassChance[8]         = { 6,  4,  3,  2,  1,  0,  0,  0 };  /* out of 16 */

/* shooting */
kBallSpeedKicking[8]           = {-384,-270,-162,-54, 54,162,270,384 };
kBallSpeedFinishing[8]         = {-288,-160, -32, 96,224,352,480,608 };

/* heading */
kPlayerHeaderSpeedIncrease[8]  = {-336,-288,-240,-192,-144,-96,-48,0 };

/* ball control */
kBallSpeedDeltaWhenControlled[8] = { 130,116,102, 88, 74, 60, 46, 32 };
ballControlTurnThresholds[8]     = {   4,  5,  6,  8, 11, 14, 17, 21 };

/* tackling */
kPlayerTacklingDownTime[8]     = { 30, 27, 24, 21, 18, 15, 12,  9 };
kComputerTacklingDownTime[8]   = {  3,  3,  3,  3,  3,  3,  3,  3 };
plAvgTacklingBallControlDiffChance[8] = { 16,17,18,19,20,21,22,23 }; /* out of 32 */

/* goalkeeping */
goalScoredChances[16] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0 };  /* out of 16 */
kGoalkeeperDiveDeltas[8] = { 0x00030000, 0x00038000, 0x00040000, 0x00048000,
                             0x00050000, 0x00058000, 0x00060000, 0x00068000 };
                           /* 3.0 3.5 4.0 4.5 5.0 5.5 6.0 6.5 units/frame */
kKeeperEaseBallTable[8] = { 1, 4, 3, 3, 3, 2, 2, 2 };   /* shift for destY easing */

/* injuries */
kInjuryLevels[7]                        = { 42, 7, 5, 4, 3, 2, 1 };
kInjuryLevelAlreadyInjured[7]           = { 14,15,12, 9, 7, 5, 2 };
kInjuryRecoveryTime[8]                  = { 0, 60, 70, 80, 90,100,110,130 };
kTackleInjuryProbability[4]             = { 48, 28, 20, 14 };  /* by match length */
kTackleInjuryProbabilityAlreadyInjured[4]= { 96, 57, 41, 28 };

/* aftertouch */
kSpinMultiplierFactor[10] = { 5,4,3,2,2,2,2,1,1,1 };

/* keeper hand/ball heights during the dive and claim animations */
goalkeeperDiveBallHeights[28] = { 7,13,11, 8, 5, 2,20, 2,
                                  5, 8,11,13, 7,20, 5,11,
                                  9, 6, 3, 0,20, 0, 3, 6,
                                  9,11, 5,20 };
kGoalKeeperClaimingBallHeight[7] = { 17,17,17,10,8,5,255 };

/* card display slices (§19), 8 frames per entry */
cardAnimStatesNoFlag[5]   = { 0, 1, 3, 3, 3 };
cardAnimStatesWithFlag[5] = { 1, 2, 4, 4, 4 };
yellowCardTable[30]  = {0,9,0,9,0,9,0,9,0,9,0,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1};
redCardTable[30]     = {0,9,0,9,0,9,0,9,0,9,0,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1};
secondYellowTable[46]= {0,9,0,9,0,9,0,9,0,9,0,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,0};

/* ball animation frame lists (image ids; -1 terminates and rewinds) */
ballStaticFrameIndices[3] = { 1068, 1068, -1 };
ballMovingFrameIndices[5] = { 1068, 1067, 1066, 1065, -1 };
```

### 24.7 Goalkeeper tables

Each is 30 words laid out as the `GoalieSkill` struct of §2.4. Rows are
`{ field_0, field_2, field_4, speed_unk, speed_unk2, index[0..15],
   field_2A, field_2C, field_2E, field_30, field_32, field_34, field_36,
   field_38, field_3A }`.

```c
kGoalieSkill0Table[30] = { 7, 0x01A8, -50, 0x0340, 0x00A0,
                           4,5,6,7,7,7,7,7,7,7,7,7,7,7,7,7,
                           3,5,8,5,0x0B,2,6,8,5 };
kGoalieSkill1Table[30] = { 6, 0x024C,  -4, 0x0360, 0x00B0,
                           3,4,5,6,6,6,6,6,6,6,6,6,6,7,7,7,
                           4,5,7,6,0x0A,3,6,7,6 };
kGoalieSkill2Table[30] = { 5, 0x02F0, 0x2A, 0x0380, 0x00C0,
                           2,3,4,5,5,5,5,5,5,5,5,5,5,6,7,7,
                           5,5,6,7,9,4,6,6,7 };
kGoalieSkill3Table[30] = { 4, 0x0394, 0x58, 0x03A0, 0x00D0,
                           1,2,3,4,4,4,4,4,4,4,4,4,4,5,6,7,
                           6,5,5,8,8,5,6,5,8 };
kGoalieSkill4Table[30] = { 3, 0x0438, 0x86, 0x03C0, 0x00E0,
                           0,1,2,3,3,3,3,3,3,3,3,3,3,4,5,6,
                           6,6,4,9,7,6,6,4,9 };
kGoalieSkill5Table[30] = { 2, 0x04DC, 0xB4, 0x03E0, 0x00F0,
                           0,1,1,2,2,2,2,2,2,2,2,2,2,3,4,5,
                           7,6,3,0x0A,6,7,6,3,0x0A };
kGoalieSkill6Table[30] = { 1, 0x0580, 0xE2, 0x0400, 0x0100,
                           0,0,0,1,1,1,1,1,1,1,1,1,1,2,3,4,
                           8,6,2,0x0B,5,8,6,2,0x0B };
kGoalieSkill7Table[30] = { 0x63, 0x0580, 0xE2, 0x0400, 0x0100,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,
                           9,6,1,0x0C,4,9,6,1,0x0C };

/* used when the "keeper" slot is occupied by an outfield player */
teamPlOfs24Table[30]   = { 8, 0x0400, 0x70, 0x0320, 0x0090,
                           7,7,7,3,4,5,6,7,7,7,7,7,7,7,7,7,
                           1,6,9,4,0x0C,1,6,9,4 };

/* read in ONE place only: the diving-contact parry/catch resolution of §15.5,
   when (penalty || playingPenalties). It does NOT replace team.shotChanceTable
   anywhere, so only field_2A / field_2C of this table are ever used — index[]
   and field_30 / field_3A below are dead. */
penaltyGoalieSkillTable[30] = { 1, 0x0400, 0xB0, 0x0400, 0x0100,
                                7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                                2,0,0x0E,0x0D,3,0x0A,5,1,8 };
```

Reading the important columns — in the eight `kGoalieSkill*Table`s, `field_30`
(word 24) and `field_3A` (word 29) hold the same value, running
`5, 6, 7, 8, 9, 10, 11, 12` for skills 0..7. `teamPlOfs24Table` also has them
equal (both `4`). `penaltyGoalieSkillTable` is the **one exception**: `field_30`
is `13` and `field_3A` is `8` — but both of those entries are unreachable (see
the comment above the table), so the asymmetry never has an effect.

* `field_3A` — the "may try a catch this tick" threshold, compared against
  `(currentGameTick & 0xF0) >> 4` (0..15). Skill 0 passes 5 times in 16, skill 7
  twelve times in 16.
* `field_30` — the catch-vs-parry threshold in the `PL_GOALIE_CATCHING_BALL`
  state (§15.4), same comparison. Skill 0 holds on to 5 in 16, skill 7 holds 12
  in 16. The **diving** contact path uses `field_2A` / `field_2C` instead
  (§15.5; skill 7: `9` / `6` against `12`), so do not interchange them.
  `field_0` / `field_2` (words 0 and 1) are not read by gameplay at all — the
  asm's `SPRITE_DIRECTION`/`SPRITE_SPEED` equate names on that path make them
  look as if they were.
* `index[16]` — the dive-reach selector, sampled by `(currentGameTick & 0x3C) >> 2`.
  The distributions are strongly skewed: skill 7 draws index `0` on 13 of the 16
  ticks (and `1, 2, 3` on the rest), while skill 0 draws index `7` on 13 of 16.
  Because the dive test is `framesDive >= framesY` (§15.3), the *smaller* deltas
  that skill 7 draws make it dive **more** readily, not less.

### 24.8 Card chance by match length

```c
playerCardChances3min[16]  = { 4,4,4,4, 5,5,5,5, 6,6,6,7, 7,8,9,10 };
playerCardChances5min[16]  = { 2,2,3,3, 3,3,3,3, 4,4,4,4, 4,5,5,6 };
playerCardChances7min[16]  = { 1,1,2,2, 2,2,2,2, 3,3,3,3, 3,4,4,4 };
playerCardChances10min[16] = { 1,1,1,1, 1,2,2,2, 2,2,2,2, 2,2,3,3 };
playerCardChancesPerGameLength[4] = { 3min, 5min, 7min, 10min };
```

One entry is drawn per match (§23) and becomes `playerCardChance`, compared
against `(currentGameTick & 0x1E) >> 1` (0..15) at every foul.

### 24.9 AI tables

```c
aiRandomRotateTable[2] = { -32, 32 };
aiLeftSpinTable[3]     = { -1, -2, -3 };
aiRotateRightTable[3]  = {  1,  2,  3 };
aiLongKickTable[3]     = {  0, -999, 4 };        /* -999 & 7 == 1 */
aiThrowInDirections[6] = { 3, 6, 12, -127, -64, 96 };  /* direction bitmasks */
AI_maxStoppageTime     = a high-water mark, not a tuning constant

/* ESC-abandon fake scorer weights, walked from index 0 (striker) downward */
kAbortLossScorerWeightTable[12] = { 35,25,20,15,11,7,5,4,3,2,124,4 };
```

### 24.10 DOS (70 Hz) retune

If you target a frame rate other than 50 Hz, this is the shape of the change the
DOS release made. `ApplyGameStyle()` writes one set or the other before every
match.

| Constant                    | Amiga (50 Hz) | DOS (70 Hz) |
|-----------------------------|---------------|-------------|
| `kBallGroundConstant`       | 16            | 13          |
| `kBallAirConstant`          | 10            | 4           |
| `kGravityConstant`          | 4608          | 3291        |
| `kPitchBallSpeedInfluence[0..2]` | -2, 2, 3 | -3, 4, 1    |
| `kKeeperSaveDistance`       | 24            | 16          |
| `kGoalkeeperDiveDeltas[i]`  | `0x30000 + i*0x8000` | that − `0x8000` |
| frames per in-game second   | 49            | 70          |
| `endGameCounter` reload     | 50            | 55          |
| keeper catch z-threshold    | 22            | 27          |
| penalty auto-advance        | 100           | 110         |
| result dismissal (non-half) | 350           | 385         |
| ball-out-of-game timer      | 500           | 550         |
| result dismissal (half/full)| 600           | 660         |
| kick-off stoppage           | 750           | 825         |
| stoppage-event reload       | 50            | 55          |
| result banner, half/full    | 250           | 275         |
| result banner, stoppage     | 150           | 165         |
| movement delta scale        | 1/1           | 41/64       |
| player-down timer, tackle / jump header (`_gsPlayerDownTimer`; `PlayerTackled` stays 50) | 50 | 55 |
| shoot-out keeper free frames (`_gsPenaltiesKeeperFree`) | 50 | 55 |
| throw-in hide-result gate (`_gsThrowInHideResult`) | 50 | 55 |
| bench camera return (`cameraLeavingSubsTimer`) | 50 | 55 |
| subs-menu held-direction repeat (`_gsSubsMovementDelay`) | 2 | 4 |
| `stoppageEventTimer`: shower pan and both extra-time transitions (`_gsStoppageShowerPan`) | 100 | 110 |
| `stoppageEventTimer`: `FirstHalfJustEnded` / `PlayersLeavingPitch` | 250 | 275 |
| `stoppageEventTimer`: `GoToHalftime` | 700 | 770 |
| `stoppageEventTimer`: `GameOver` | 1500 | 1650 |

The last nine rows were measured against DOS recordings by the replay rig
after 1.8 and are applied by the same `ApplyGameStyle()` (`src/dos_gameplay.c`).
Not every literal of a family scales: `StartingMatch`'s and `EndFirstHalf`'s
100 and `EndOfGame`'s 150 are the same in both releases, so the §21.4 table is
the Amiga column and only the rows above change. `ApplyGameStyle()` also
selects the DOS release's retuned 4-4-2, 4-3-3 and Sweep formations. The
remaining DOS differences are not constants but behaviour (the dive-reach index
is `tick & 15` and then one entry lower, not `(tick & 0x3C) >> 2`; no
shoot-out winner and no `ST_GAME_ENDED` after a shoot-out; the stats overlay
does not stop `currentGameTick`; and more) — docs/DOS_GAMEPLAY.md and the
`_gDosGameplay` sites in `src/` hold the list.

The last row is the key one: at 70 Hz every `deltaX`/`deltaY` out of
`CalculateDeltaXAndY` is multiplied by `41/64` so players cover the same ground
per *second*. Ball trajectories are not scaled there — they are retuned through
the physics constants above.

**Do that multiply as a chain of arithmetic shifts, not as `* 41 / 64`:**

```c
d = d - (d >> 2) - (d >> 4) - (d >> 5) - (d >> 6);   /* == d * 41/64 */
```

Shifts round toward −∞ (§25.2); C division rounds toward zero, so the two
disagree by one on every negative delta, and one unit of X error compounds into
a different trajectory within a few frames. Verified against swos-port's
`CalculateDeltaXAndY` vectors: the shift form matches 10/10, the division form
4/10 (`bin/SWOS --replay-selftest`, docs/TESTS.md §5).

The two result-banner rows and the 70 in the clock row were both **measured**
from DOS recordings by the replay rig, not read off a table; the clock value in
particular was 54 in every earlier revision of this document and in
docs/DOS_GAMEPLAY.md. 140 frames per game-minute with a per-frame decrement of
30 forces `2N = 140`.

### 24.11 `kRandomTable`

256 signed bytes plus one pad byte. The 257th entry (`26` = `0x1A`) is the high
byte of the original's `dc.w $1A00` alignment pad and is **never read** — both
generators index with an 8-bit value, so only entries 0..255 matter:

```c
int8_t kRandomTable[257] = {
  124,-102,-110,  70, 101,  -6, -83,  89, 117,  26,  67,  12, -18,-109, -30,  34,
   78, -57,  -3, 107, 125,  87, -86, -48, -68,  72,   5,  51, -32, -10,-111,  32,
  110, -22,  68,  43,  99,  39, -53,  86, -79, -93, -58, -94,-128,  16,  93,  -1,
 -108,  10,  59,  54, -81, -34, -54,  -7, 127, -31, -84, -17,   8, -42,-114, -21,
  -89,  73,  44,   7,-126, 105, 114,  97, -11, -97,  56, -59, -43, -35, -76,-127,
  106,  69, -77,  95,  90,  46, 103, 122, -40,  80,  45,-100,  18,  36,-122, -69,
  -27, -70, -72,-112, 109,-106, -73,-121,  29,  -5, -82,  71, -33, -12, -24,  83,
   81, -92,  25,  79,  62, -56,  65, -75,-124,  55, -15,  21,  -4,  82, -78, 123,
  -37, -29,-105,  76,  91,  28,   6,  -2, 119, -25,  85,  14, -20, -71, 108,  24,
  111,  40,  98,  35, -88, -67,  41,  74, 102, -47,  96,-103, -80,-123,  -9,  50,
   33,  52, 113,-115, -50,  42,   3, -96, -39, -95, -63,  64,-113, -51, -74, -23,
 -117,  37, -19,  84,-125, 121,   4,  22,  20, -62, -13, -87,   0, -55,  48,  61,
    9, -44,-119,  57,   1, 126,-101,  92,  31, -61,  19,  88, -28, 116,  15,  94,
  -65, -46,  47,  60, -38, 115, -16, 118, -98, 104,-118, -85,  13, 120, -45,  27,
  -99, -14,  66, -49,-104,  30,   2,-116, -60,  17, -66,-107,  75, -26,  38,  11,
   77, 100, -64, -36, -52, -90, -91,  53, 112,-120,  23,  63,  49,  -8, -41,  58,
   26 };
```

### 24.12 Where the rest of the data lives

Some tables are too large to reproduce inline and are pure data; transcribe them
from the source files listed:

| Data                                        | File                          |
|---------------------------------------------|-------------------------------|
| `kSineCosineTable[257]`, `kAngleCoeficients[1025]` | `src/math68k.c`        |
| 42 restart position arrays + the two 32-entry pointer tables | `src/swos_data_typed_stubs.c` |
| 12 built-in `TeamTactics` blocks (370 bytes each) | `src/tactics_data.c`     |
| `kTeamsStartingCoordinates[44]` (tunnel line-up)  | `src/match_setup.c`      |
| Every scalar listed in §24.1                | `src/swos_amiga_data_imports.c` |
| Skill / injury / card tables                | `src/swos_data_bulk_imports.c` and `src/swos_amiga_data_imports.c` (e.g. `kInjuriesSpeedHandicap`, `playerCardChance`, `kPlayerTacklingDownTime`) |
| 33 `AnimTable` objects + the 266 frame-index lists they point at | `src/sprite_data.c` |
| Per-image sprite metrics (`centerX`, `centerY`, `pixWidth`, `nlines`) — required for §5's `onScreen` | the sprite-extraction command streams, decoded by `src/image_cmd.c` |
| `playersPositionsByTactics` (formation slot orders, needed by §23's `ApplyTeamTactics`) | `src/tactics_data.c` |
| `kGoalieSkill0..7Table` + `kGoalieSkillTables` | `src/swos_globals.c`         |
| `teamPlOfs24Table`, `penaltyGoalieSkillTable`  | `src/swos_amiga_data_imports.c` |

---

## 25. Porting checklist and known traps

### 25.1 Order of work

A port that follows this order gets a playable ball early and can validate each
layer against the original:

1. **`CalculateDeltaXAndY` + the two tables + the RNG.** Unit-test them against
   the values in §3 and §4 before anything else. Every trajectory in the game
   depends on them.
2. **`Sprite`, `TeamGeneralInfo`, `PlayerInfo` and the 16.16 fixed-point type.**
3. **`UpdateBall` (§10) and `MovePlayers` (§9)** with a hard-coded kick. You
   should get correct bounce, friction, goal-frame collisions and shadows.
4. **Input (§6) and one controlled player (§8.3).** Dribbling, passing and
   shooting all work with no AI and no other players.
5. **Player selection (§7)** and the receiver logic (§8.9).
6. **Out-of-play detection and the restart hand-over (§18).** At this point you
   have a playable one-on-one.
7. **Tactics positioning (§17) and the ball quadrants (§10.8).** The other 20
   players come alive.
8. **Goalkeeper (§15)**, then **tackling/fouls/cards (§14)**, then **the referee
   (§19)**.
9. **The AI (§16).**
10. **Clock, halves, extra time, penalties (§21)** and **statistics (§22)**.

### 25.2 Things that silently break the feel

* **Updating both teams every frame.** `UpdatePlayers` is 25 Hz per side and
  interleaved (§5.1). Running both at 50 Hz doubles every timer's effective
  rate and changes every contested ball.
* **Using `atan2` instead of `kAngleCoeficients`.** The pseudo-angle is a median
  ≈ 2.9° and worst-case ≈ 4.7° off true, and that error is baked into every shot
  the game was balanced around.
* **Rounding differently in `CalculateDeltaXAndY`.** `>> 8` on a signed 16-bit
  sine, `>> 2` on the camera error, `>> 1` on speeds — all are arithmetic shifts
  of signed values, which round toward −∞, not toward zero. The DOS `41/64`
  delta scale (§24.10) is the same trap wearing a different hat: written as
  `* 41 / 64` it rounds toward zero and every negative delta comes out one
  short.
* **Taking the square root of `ballDistance`.** Every threshold in the engine is
  a squared constant.
* **Treating the three ball-proximity flags as cumulative.** They are mutually
  exclusive bands (§8.2); a single-flag gate means a ring, not a disc. This
  silently changes the tackle launch range.
* **Inverting `fullDirection`.** It is the angle from the **ball to the player**
  (§3.3); the engine adds 128 to derive a facing.
* **Treating `fullDirection` comparisons as integers.** They are signed-byte
  subtractions that rely on 8-bit wrap.
* **Following the ball with the camera instead of the velocity ladder (§20.2).**
  The ±40 lead plus the 1/16-error lag is the SWOS camera.
* **Repositioning every idle player every frame.** The `updatePlayerIndex`
  round-robin (§8.1) is what makes off-ball movement look composed.
* **Positioning from the ball instead of from `ballNextX/Y`.** The tactics grid
  is driven by the *prediction* (§10.8/§11.1).
* **Dropping the `|= 1` on `deltaZ`.** The ball settles into a dead float in
  mid-air without it.
* **Skipping `onScreen`.** Auto player-selection deliberately ignores players
  the camera cannot see (§5). Without it you switch to a player off-camera.
* **Clamping a position by writing the whole fixed-point value.** `movePlayer`
  writes only the integer half and keeps the fraction (§9.2).

### 25.3 Endianness and layout

The original is big-endian 68000 and several structures are read at two
granularities:

* `FixedPoint.s.i` must alias the **high** half of `.raw` regardless of host
  endianness.
* Several `int16_t` fields are written as bytes and read as words (and vice
  versa) — notably `TeamGeneralInfo.firePressed`/`fireThisFrame`, which the
  original tests as one 16-bit word to mean "either fire signal is set", and
  `field_46`, which is set with a byte write to the *high* byte (value `0x0100`)
  and cleared with `&= 0xFF`.
* `gameTime` is **three** digit bytes in the low three bytes of one 32-bit word
  (byte 0 is always zero), compared as a whole word against big-endian constants
  (`0x00000405` = minute 45, `0x00000900` = minute 90, `0x00010005` = 105,
  `0x00010200` = 120).

A fresh port is free to use plain fields for all of these — just make sure the
*semantics* above survive.

### 25.4 Values that look like bugs and are not

These are faithfully preserved in this port because gameplay depends on them:

* **`CalculateIfPlayerWinsBall` looks the opponent's player up in the wrong
  squad first** (§12.4). The resulting skill value feeds the 50/50 roll.
* **`kComputerTacklingDownTime` is selected by `tacklingTimer == -1`**, not by
  whether the team is CPU — and since only a *human* team ever negates that
  timer, the "computer" table is in fact used **only** by a human who taps and
  releases fire within two updates, never by the CPU (§14.1).
* **`aiLongKickTable[1] == -999`**, which after `& 7` is `1`.
* **`playerDownTimer` for a tackle is set to `0xFF`** and the slide is actually
  ended by friction, not by the timer.
* **`ballXQuadrantLimits` has a 5th word (129)** that is not an X threshold —
  only entries `[0..3]` are. It is read, though, by the *Y* loop: its `i == 0`
  case reads the word before `ballYQuadrantLimits` via the original's `-4(a1)`
  addressing, and that word is this one. The X loop's `i == 0` case likewise
  reads the pad word before its own array (`81`). The pseudo-code in §10.8
  spells both out. *(Revisions before 2.0 called the 129 "never read".)*
* **The keeper's "beaten" latch is a signed counter** (`±5`), and its sign — not
  its magnitude — is what the parry logic reads.
* **Several globals look load-bearing and are not.** `patternsGoalCounter` is the
  fan-tile animation phase (free-running, wraps at 300); `nobodysBallTimer` only
  keeps the last toucher's name plate on screen for 50 frames; `goalCounter` is a
  crowd-audio timer (§18.2). None is read by any gameplay decision.
  `runSlower`, by contrast, **is** gameplay (§9.1).
* **Dead branches that survive in the source.** Nothing ever writes
  `Sprite.injuryLevel = -1` or `-2`, `forceLeftTeam` (§18.1), `injuriesForever`,
  `cardsDisallowed`, `goalkeeperSkipFlag` (§15.2), or `subsState == 3` — yet all
  are tested. Implement the tests only if you want bit-identical structure; none
  can fire. `goalkeeperSkipFlag` is the one worth keeping in place regardless,
  because its position fixes the order of the `ballNextGroundX = -1` store.
* **The one-on-one duel reads the wrong squad** (§15.3) — the attacker's shirt
  number is resolved inside the *keeper's* team. This shifts the whole
  finishing-vs-goalieSkill distribution and must be reproduced.

**One place where this port was wrong — fixed 2026-07-30 (2e57a8e), kept here
because a port copied from older sources may still carry it.**
`src/update_players.c` used to define the four caught-ball sprite ids as
`0x3C4 / 0x3C6 / 0x3D0 / 0x3D2` (in both the SDL and the Amiga trees, which were
still separate then). The original ids — now what it defines
(`SPR_GOALIE{1,2}_CAUGHT_BALL_*`, `src/update_players.c:203-206`) — are
`0x35E / 0x360 / 0x3E0 / 0x3E2` — the symmetric set `{base+5, base+7, base+19,
base+21}` from goalie 1's dive base `0x359` and goalie 2's base `0x3CD`, which is
the same four offsets `DoGoalkeeperSprites` uses. The wrong values make `0x3D2` a
duplicate of a literal already in the same compare chain and put `0x3C4`/`0x3C6`
outside every goalkeeper dive sequence (`0x359..0x374`, `0x393..0x3AE`,
`0x3CD..0x3E8`, `0x407..0x422`), so a keeper in the caught-ball pose decays at
`kGoalKeeperRecoveryStepBase (112)` instead of
`kGoalKeeperRecoveryStepAlt (192)` and slides roughly 70 % further after the
catch (§15.5). Use the original ids.

**A second place, also fixed — it bit in widescreen.** The
`onScreen` cull in `src/sprites.c` used to key off the *live* render width. On
the Amiga that is always 336, and this port at 4:3 matched; but a widescreen
render can make it 650, and §5's rule that auto player-selection ignores
players the camera cannot see then admitted players the Amiga would have
culled — **playing ultrawide changed which player the CPU selected.** Two
things were wrong and both are corrected:

- the cull window is a **constant 336 wide**, not the view width. Measured
  against the DOS recordings, whose visible screen is 320 wide: a sprite is
  recorded on-screen out to camera-relative x 335 and off from 336, with the
  per-image metrics taken from the DOS banks and each axis conditioned on the
  other being comfortably inside (clean boundary, no overlap, all 1519 frames
  of `zkz-12065730`). Both releases cull to a 336-wide buffer; only the height
  follows the display — on out to y 199 for DOS's 200 lines, and 272 here.
- the routine then ended each iteration with an unconditional
  `sprite->onScreen = 1` after enqueueing, which undid the cull for anything
  the renderer drew. Harmless while "culled for gameplay" and "culled for
  drawing" were the same test; wrong as soon as they are not.

So `onScreen` is now decided by a fixed 336 x `viewH` window while the enqueue
still keys off the render width — cull to a fixed window, draw wider, which is
what §25.5 prescribes. At 4:3 and on Amiga the two coincide and nothing
changes. `_sim_view_width` / `_sim_view_height` (`include/display.h`) carry the
simulation's geometry, and the replay rig sets them to a recording's.

**The camera constants are all functions of that same view**, which is worth
knowing in its own right — it is why §20's and §21's literals are what they
are, and why the DOS release's differ:

| constant | formula | 336x272 (Amiga) | 320x200 (DOS) |
|---|---|---:|---:|
| kick-off centre X | `(672 - viewW) / 2` | 168 | 176 |
| kick-off end Y (bottom) | `664 - (viewH - 200)` | 592 | 664 |
| camera centre Y offset | `viewH / 2` | 136 (`0x88`) | 100 |
| pan range | `672 - viewW` | 336 | 352 |
| camera target Y max | `664 - (viewH - 200)` | 592 (`0x250`) | 664 |
| camera position Y max | `880 - viewH` | 608 (`0x260`) | 680 |
| referee entry below the camera (§19) | `viewH + 15` | 287 | 215 |
| bench-camera Y window (§20.1) | `449 - viewH/2 ± 10` | 303..323 | 339..359 |
| end-of-game / highlights camera X | `(672 - viewW) / 2` | 168 | 176 |

In the first six rows the right-hand column is swos-port's own named constants (`kCenterX`,
`kBottomStartLocationY`, `kPitchMaxX`, `kCameraMaxY`), reached by the same
formulas; the last three were read from the DOS listing (`swos.asm`) by the
replay rig and live in `include/display.h` as `SIM_REFEREE_ENTRY_BELOW`,
`SIM_CAMERA_BENCH_Y_LO/HI` and `SIM_CAMERA_CENTRE_X` — so these are not Amiga magic numbers, they are one model evaluated
at two resolutions.

### 25.5 Determinism

These are the requirements for **simulation determinism** — the same inputs from
the same state producing the same match, which is what a lockstep-netcode or
input-level re-simulation port needs. They are *not* requirements for playing back
a HIL/RPL recording; see the second block below for why:

* `currentGameTick` must increment exactly once per simulation frame and be the
  only source of the dozens of `tick & mask` decisions — **except** that the
  Amiga's VBlank interrupt does not advance it while paused **or while the
  statistics overlay is up** (`showingUserStats`, SWOS2.s:5613-5619), whereas
  `currentTick` always advances. One skipped tick shifts the phase of every
  mask for the rest of the match. (DOS tests only `paused`.)
* **`currentTick` too.** It is a second counter, and it is *also* read by
  gameplay — `currentTick & 2` decides whether the ball-control skill bonus is
  added to a controlled ball's speed (§12.4). On the Amiga both counters are the
  same VBlank interrupt, so this is easy to miss; on a host that drives them
  from a timer thread they drift apart and that decision becomes a coin toss.
* **`lastGameTick` must be reset when the match starts.** `lastFrameTicks` is
  `currentGameTick - lastGameTick`, and §21's `resultTimer`,
  `stoppageEventTimer`, `endGameCounter`, `refTimer` and `inGameCounter` are all
  stepped by it. Left over from the menus, it makes frame 0 step every one of
  them by an arbitrary amount. **Frame 0's delta is 1, like every other
  frame's**: `GameLoop` reads the delta once before the loop and then waits for
  vblank, so one tick falls between that read and the first pass through the
  body. Reset `lastGameTick` to one *behind* `currentGameTick`, not equal to
  it — equal gives frame 0 a delta of 0 and every timer-driven transition in
  the match lands a frame late.
* **Reset the tick's value and the tick's phase separately.** They look like
  one knob and are two: `lastFrameTicks` is a *difference*, while the dozens of
  `currentGameTick & mask` AI decisions read the *absolute* value. Starting the
  tick at 1 fixes the delta and silently rotates every one of those masks by
  one frame. Keep `currentGameTick` equal to the frame number and move
  `lastGameTick` instead.
* `Rand()` and `Rand2()` must stay on separate state, and `Rand2`'s
  save/restore stack must be preserved.
* `frameCount` drives the team alternation, so it must also advance exactly once
  per frame.
* No gameplay decision may read the wall clock, the frame time, or a
  floating-point value.

These are testable rather than aspirational: `scripts/replay_test.sh`
(docs/TESTS.md) replays a recorded match frame by frame and compares 900+ named
globals per frame. The first three bullets above are there because the rig found
each of them violated in this port — two runs of the same recording diverged at
different frames until all three were fixed.

**What replay determinism does and does not require.** Be clear about which kind
of determinism is at stake, because SWOS's replay format makes the distinction
matter:

* **Replays are POSITION recordings, not input recordings.** A HIL/RPL stream is
  a packed list of `(spriteIndex, x, y)` per sprite per frame, with a frame
  marker — the recorded camera and sprite positions, nothing else. Playback draws
  those positions; it does **not** re-run the simulation. So replay fidelity does
  not depend on `onScreen`, on sprite metrics, or on the view width at all.
* **The recorded camera need not be replayed verbatim.** A recording made in a
  336-wide view has only `672 - 336` units of camera travel; played back in a
  wider view the recorded path cannot follow the ball. The fix is to re-derive
  the camera from the recording's own content: **track the ball shadow**
  (`SPR_BALL_SHADOW`, image 1069 — a constant sprite present every frame at the
  ball's ground X, unlike the ball itself, whose image changes with height),
  reconstruct the ball's world X as `recordedCameraX + recordedShadowScreenX`,
  and drive the §20 camera model from it. That plays any recording correctly at
  any width, and it is what this port does.
* **`onScreen` only matters for input-level re-simulation** — lockstep netcode,
  or replaying a stored input sequence. The original never does this, so the
  §5 caveat is not a limit on SWOS replays. If you *do* build input-replay or
  rollback netcode on top of this simulation, then `onScreen` becomes part of
  the state hash, and it requires both the original per-image sprite metrics and
  a fixed cull width — in which case cull against a fixed 336 × 272 window and
  let the renderer draw wider; the cull and the draw need not share a width.
  This port now does both: GGPO rollback netplay (docs/NETPLAY.md; the saved
  state is the registry in `src/sim_state.c`, `onScreen` included) and
  input-level re-simulation of a live match's `.SRP` (`--srp-resim=<file>`,
  docs/REPLAY_FORMAT_V2.md). Cross-machine netplay also found that a declaration of
  `CalculateDeltaXAndY` with the wrong return type (`int16_t` for its
  `uint16_t`, `0xFFFF` = "not moving") made two compilers disagree about the
  sentinel — declare it once, in `include/math68k.h`.

---

## Revision history

| Version | Date       | Sections touched | Change |
|---------|------------|------------------|--------|
| 2.0     | 2026-09-30 | §2.4, §5, §6.3, §7.1, §7.2, §8.6, §8.9, §8.10a, §9.1, §9.4, §10.5, §10.8, §12.2, §12.4, §14.1, §14.2, §15.5, §15.7, §16.4, §16.5, §18.3, §19, §20.1, §21.3, §24.4, §24.7, §24.10, §24.12, §25.4, §25.5, Known gaps | The replay rig kept going after 1.8 and fixed a run of port defects in the Amiga path that this document had faithfully described; each was checked against `data/SWOS2.s` again for this revision. **`CORRECTION:`** §15.5/§2.4/§24.7 — the diving-contact catch/parry thresholds are `GoalieSkill.field_2A`/`field_2C` (byte offsets 42/44, spelled `SPRITE_DIRECTION`/`SPRITE_SPEED` in the asm), not `field_0`/`field_2`; the "four-way is really a two-way" note is withdrawn — it is tick-dependent. §7.1/§7.2/§12.2 — the nearest-player scans visit slots **0 → 10** (ties to the lowest slot), not 10 → 0. §16.4 — the sideways-facing shot gate refuses *inside* the opposing six-yard box (`ball.y >= 740` top / `<= 158` bottom), not outside it; the fire tolerance is `-tol < e <= tol`; the long-ball bucket test is `{0,1}` (the `$FFFF` arm is dead after `& 7`); x = 478 is outside the central spin band. §16.5 — the CPU-vs-CPU random-rotate position test had its y sense inverted. §10.5 — the goalpost block has **three** arms (a weak-Y ball at post height reverses `destY` and adds the jitter to `destY`), not two. §10.8/§24.4/§25.4 — the Y quadrant loop's `i == 0` reference is `129` (`ballXQuadrantLimits[4]`, read by adjacency), not `ballYQuadrantLimits[0]`. §8.6 — the out-of-pitch goalmouth slide is `speed >>= 4` (four `lsr`s; the asm comment's "/32" is wrong), and a released tackle timer of `-2` also folds to `-1`. §8.9/§9.1 — `tst.l` on `longPass` also covers `longSpinPass`. §8.10a — `isThisPlayerOkToSelect` returns a positive offset for a substituted-off player (the cursor stops on him; the fire press refuses him), and the reserve-keeper cursor lock is on the `gameMaxSubstitutes > 5` side. §18.3 — a fire press in the six unhandled end-of-game states falls through into the countdown; it does not freeze it. §21.3 — a negative `penaltiesState` at `ST_GAME_ENDED` *centres* the camera (the sentence was reversed; §20.3 was right). **`ADDED:`** §24.10 — nine more DOS retune rows (player-down, shoot-out keeper, throw-in banner, bench camera, subs repeat, four `stoppageEventTimer` reloads) and the list of behavioural DOS splits; §25.4 — three more view-derived literals (referee entry 287, bench window 303..323, end-of-game camera X); §25.5 — the stats overlay stops `currentGameTick` on the Amiga, and this port now does rollback netplay and `.SRP` re-simulation; §14.2 — the last-man search's tie order; §8.10a — the DOS bench alternation and second fire button. **`CLARIFIED:`** §5 — the `onScreen` width is a constant 336; §9.4/§15.5/§25.4/Known gaps — the caught-ball ids and the RNG-seed writes were fixed in the port on 2026-07-30; source line numbers refreshed. Two edits made without a version bump between 1.8 and this row are recorded here too: §12.4 (2026-09-17, cd9dc97) — the ball contest's `wonTheBallTimer` goes to the side that *lost* the roll; §6.3 (2026-09-13, 478cee2) — the computer team's tackle clearing the human's fire counter is an original bug the port fixes by default (`--fire-hold-lockout=on` / `[gameplay] fire_hold_lockout` restores it). |
| 1.8     | 2026-09-03 | §21.1, §24.10, §25.2, §25.4, §25.5 | First round checked by **measurement** rather than by reading: the recorded-game replay rig (docs/TESTS.md) replays a DOS match frame by frame against the port and compares 900+ named globals per frame, so a wrong constant shows up as a divergence at a specific frame. **`CORRECTION:`** §24.10/§21.1 — **frames per in-game second is 70 on DOS, not 54.** Every earlier revision said 54, taken from a swos-2020 comment. The recording reaches game-minute 1 at frame 702 and minute 2 at frame 842 = 140 frames per game-minute; the accumulator decrements by `gameLenSecondsTable[0] = 30` per frame and reloads by `N`, so 60 seconds take `2N` frames and `N = 70`. 54 would give 108 frames/minute, swos-2020's alternative 69 would give 138. swos-port's independent decompile says 70 as well. §24.10 — the `41/64` delta scale must be the **shift chain** `d - (d>>2) - (d>>4) - (d>>5) - (d>>6)`, not `* 41 / 64`; §25.2 already warned that these shifts round toward −∞ while C division rounds toward zero, but the table's own note said "integer, truncating" and so specified the wrong one. Every negative delta came out one unit short and the trajectory forked within a few frames; against swos-port's reference vectors the shift form scores 10/10 and the division form 4/10. **`ADDED:`** §24.10 — the two DOS result-banner lengths (275 half/full, 165 stoppage, against the Amiga's 250/150), which gate the §21.4a stopped-state dwell and the §16 AI's `resultTimer != 0` test. §25.5 — **`currentTick` is a second gameplay-read tick counter** (`currentTick & 2` gates the ball-control speed bonus), and **`lastGameTick` must be reset when a match starts, to one behind `currentGameTick` rather than equal to it**: `lastFrameTicks` steps `resultTimer`, `stoppageEventTimer`, `endGameCounter`, `refTimer` and `inGameCounter`, so left over from the menus it steps all five by an arbitrary amount, and set equal it makes frame 0's delta 0 and lands every timer-driven transition in the match a frame late. §25.5 also now separates the tick's *value* from its *phase* — `lastFrameTicks` is a difference, the `tick & mask` AI decisions read the absolute value, and fixing the first by starting the tick at 1 silently rotates all of the second. Both were live defects in this port, found because two runs of the same recording diverged at different frames. §25.5 now names the rig as the way to check the section rather than trust it. §25.4 gains a **second place the C ports were wrong**, found the same way and now FIXED: the `onScreen` cull tested the live render width, so a widescreen render admitted players the Amiga would have culled and §5's auto-selection rule then picked a different man — CPU behaviour changed with the window size. Two defects, both corrected: the cull window is a **constant 336 wide** in both releases (measured from the DOS recordings, whose screen is 320 — on-screen out to x 335, off from 336), and the routine ended each iteration with an unconditional `onScreen = 1` that undid the cull for anything the renderer drew. §25.5's own advice (cull to a fixed window and draw wider) is now what the code does, and the port now carries the separation it needs (`_sim_view_width`/`_sim_view_height`, include/display.h); docs/TESTS.md carries the worked example that proves the chain from a recording's own data. §25.4 also gains the **camera-constant derivation**: §20's and §21's literals (168, 592, `0x88`, `0x250`, `0x260`) are not magic numbers but one model — `(672-viewW)/2`, `664-(viewH-200)`, `viewH/2`, `672-viewW`, `880-viewH` — evaluated at 336x272, and the same formulas reach swos-port's DOS constants (`kCenterX` 176, `kBottomStartLocationY` 664, `kPitchMaxX` 352, `kCameraMaxY` 680) at 320x200. |
| 1.7     | 2026-07-29 | §1.3, §1.3a, §2.3, §2.4, §4, §5, §8, §8.10, §8.10a, §11.2, §11.3, §13.2, §14.4, §15.1–§15.5, §15.7 (new), §16.2, §16.4, §16.5, §16.6, §19, §20.2, §21.1, §21.2, §21.3, §21.4, §22, §24.6, §24.7, §25.4, §25.5, Known gaps | Fifth and final round before release. Three agents: a release-gate audit of every v1.6 addition, a precision pass over the goalkeeper (§15), and a **second blind implementation** — a fresh agent, given only this document, which wrote and compiled 3488 lines of C covering §13–§22 and reported every point the text failed it. Between them they found one clock-stopping omission, one empty branch, one no-op arm and eleven elided expressions. **`CORRECTION:`** §21.1/§21.2 — **nothing in the document ever cleared `gameSeconds` back from `-1`.** Each of the four boundary minutes is entered *twice*: the first pass arms the injury-time wait, and only the second pass writes `gameSeconds = 0` and blows the whistle. A port following the old text reached 45:00, whistled, kicked off the second half correctly and then **never advanced the clock again**, because `UpdateTime`'s first test sent every later frame back into "last minute in progress". This was the worst defect in the document. §15.1/§24.7 — `shotChanceTable` is **never** repointed for a penalty; `penaltyGoalieSkillTable` is read at exactly one site (the diving-contact parry), so its `index[]` and `field_30`/`field_3A` are dead and every other penalty decision still uses the keeper's own skill table. §15.2 — "the keeper's `destY` is *always* clamped" was false (four paths skip it), the opening sentence contradicted the body (the *cascade* is gated on `live`, the routine is not), and branch D's far arm has a **second** burst-out test the document showed once. §15.5 — the scattered-parry Y scatter is `((tick & 0x1F) << 4) - 256`, i.e. `-256..+240` in steps of 16, **not** a symmetric ±256, and the second speed subtraction reuses the pre-subtraction value (final speed `×5/8`, not `×9/16`); the four-way parry decision collapses to two, because `d1 - field_2` is always negative. §16.6 — both AI spin arms **do** assign `currentAllowedDirection`; as written they computed a value and discarded it, and since §16.1 has already centred the stick, §12.5 then took the lofted-straight arm — every CPU curled shot became a lofted straight one. §16.6's single "distance to goal" rule for `AI_afterTouchStrength` is wrong: there are six per-call-site rules, on two different partitions of `d6`. §16.5's mid-range gated tree belongs to **§16.4** (it is the "no fire" fallback), not to "nobody near the ball" — it had been filed under the wrong branch since v1.4. §14.4's cards — the sending-off path does **not** increment `player.cards`, and does set `bookedPlayer`/`lastTeamBooked`/`refTimer`, all three of which §19 requires. §19 — the card decoder's `entry == 1` / `entry == 2` arms are **unreachable** given the three tables (which contain only `0`, `9` and `-1`), so `SPR_YELLOW_CARD`/`SPR_RED_CARD` are dead and the visible card comes from the referee's *own* animation; also "a 5-state machine" listing six states. §20.2's `(0,0)` note still ended with an out-of-bounds claim v1.6 had already corrected. **`ADDED:`** §15.2's **midrange positioning body** — an empty branch until now, and the most common save situation in the game; the keeper's wrong-half threshold (`y > 449`, strict on one side only); §15.3's penalty side-guess as two explicit tick masks (and why variant 1's slower dive speed is the "guessed wrong" speed); §15.5's diving-contact overlap windows in full, with `pixWidth` named as asset data; the `kKeeperEaseBallTable` easing expression; §13.2's two-step static-header turn, and that the *stick* rather than the facing decides where the header goes; §11.2's raw initialisers (`d1`/`d2`/`d3`/`d6` are read before the document assigned them) and §11.3's key fact that **the integrators have no ground and no `z` termination** — adding one silently turns dipping shots from "on target" into "off target", which is exactly what it did to the blind implementer; §16.4's "not deep" test (the 6-yard-box Y bounds) and the four leaves of its fallback tree; §16.5's long-ball spin bias in full (three X bands × a 3-bit tick bucket); §16.2's keeper's-ball facing set (`{1,3}` left of centre, `{5,7}` right) and the `aiTurnDirection` oscillator's real mechanics (it reverses only when `playerTurnFlags` blocks the step, and issues no direction on the frame it reverses); §21.3's five match-option globals with `extraTimeState`/`penaltiesState` as a **tri-state**, not a boolean; §22's goal-attempt tracker as code, including that the arm and the clear use *opposite* team assignments and differ by `>` vs `>=` at the centre line; `PlayerInfo.position`'s 0..7 role encoding, without which §8.10a's three-tier selector cannot be written; §8.10's phase `-1` hand-off to the idle-target path, without which the incoming substitute walks *away* from the pitch; `numYellowCards` as a five-value **state** rather than a count; §1.3a rows for `gameSeconds`, `endGameCounter`, `secondsSwitchAccumulator`, `refState`, `refTimer`, `aiCounter`, `aiResumePlayTimer`, `aiTurnDirection`, `field_84` and `isGoalAttempt`; §15.7 — a **worked example** that walks one keeper frame end to end and names, at every step, which unfixed variable decides the next branch. **`CLARIFIED:`** §16.6 — the CPU's stick is consumed **one team update late** and that is correct, because `ApplyBallAfterTouch` sits in `UpdatePlayers`' preamble while the AI writes the stick inside the per-player dispatch; §22 — `strikeDestX` is deliberately stale, so do not refresh it; §15.3 — the one-on-one duel resolves the attacker's shirt number inside the **keeper's** squad (an original-SWOS bug that is nonetheless behaviour, and must be reproduced); §4/§25.5 — the RNG rules are *simulation*-determinism requirements, not replay-playback requirements; §25.4 now records the one place the C ports themselves are wrong. |
| 1.6     | 2026-07-29 | §1.3, §1.3a (new), §4, §8.3, §8.10a, §11.3, §12.6, §14.3, §15.2, §16.2, §18.1, §20.1, §20.2, §21.1, §21.3, §24.2a (new), §24.6, §25.5, Known gaps | Round 4 completed (it had been cut short by a session limit). Three agents: an audit of the round-3 additions, an independent source-first pass over the least-reviewed sections, and a **blind implementation** — an agent allowed to read only this document, which wrote and compiled a 2052-line simulation from it and logged every point the text failed it. **`CORRECTION:`** the v1.5 RNG seeding block invented a third line, `randXorKey = kRandomTable[randXorIndex]`, that no code path performs — `randXorKey` starts at 0 and is written only inside `Rand()`'s own rekey arm, so following it XORed every value before the first wrap with the wrong key; seeding also happens on game-**mode** entry, not per match, and the state does start zeroed. §21.3's minute-90 resolution was written as an if/else-if chain when the away-goals block **falls through** to extra time and penalties if the adjusted totals are also level — as documented, a level second leg could never reach extra time. Minute 120 is not "the same as minute 90" (no extra-time arm, no score snapshot, no half-played mark). §11.3's sample-at-ball paths zero `strikeDest*` rather than setting it to the ball position, and the two `deltaX` arms have an already-past-the-keeper early exit. §20.1's subs-menu margin defaults to 37, not 0; §20.2's `cameraDirection < 0` yields `(0,0)` rather than an out-of-bounds table read; every non-tracking camera branch also zeroes both velocity accumulators. §14.3's severity upgrade is an **unsigned** byte compare (0x20..0xE0 read as -128..-32 signed, which inverts the rule). §24.6's "all indexed 0..7" heading was false for eight of its tables. `forceLeftTeam` has **eight** read sites, not six, and the eighth is disguised as `== ST_GOAL_OUT_LEFT`. **`ADDED:`** §24.2a — `kKickSpinFactor` and `kPassingSpinFactor`, which appeared exactly once in the whole document (at their use site) and in no table section, making aftertouch, chips, long passes and all of §16.6 unimplementable; this was the worst blocker found. §1.3a — a set/stepped/cleared lifecycle table for every load-bearing shared global, which is the class of defect that cost the blind implementer the most time (a missing *writer* presents as your own bug, not as missing text). The `inGameCounter` increment (read by §8.8's freeze gate, previously zeroed twice and stepped nowhere — omit it and no off-ball player repositions again after the first corner). `StopAllPlayers()`'s full eight-field clear, including `ballInPlay`/`ballOutOfPlay`, which is what actually terminates the §18.3 hand-over. A definition for "toggle timers"; the corner-table selection (by foul spot, not by state); the `aiThrowInDirections` rotate semantics; and the squared-distance note on the keeper's rush-out test. **`CLARIFIED:`** §25.5 — the `onScreen` cull also decides which sprites a newly *made* recording contains, even though playback is unaffected. Both previously UNVERIFIED items are resolved: `gameMaxSubstitutes > 5` is unreachable from every menu and all 89 shipped presets, and `substitutedPlX/Y` has no reader at all. |
| 1.5     | 2026-07-29 | §3.2, §4, §5, §18.1, §18.3, §25.5 | **`CORRECTION:`** v1.4 claimed that a port with substitute artwork "cannot reproduce a SWOS replay frame-for-frame" and that this port breaks cross-machine replay determinism. **Both were wrong.** A HIL/RPL recording is a packed `(spriteIndex, x, y)` **position** stream, not an input stream, so playback never re-simulates and replay fidelity does not depend on `onScreen`, on sprite metrics or on the view width. §25.5 now documents the actual technique instead: re-derive the replay camera by tracking the **ball shadow** (image 1069 — constant every frame at the ball's ground X, unlike the ball itself whose image changes with height), reconstruct `ballWorldX = recordedCameraX + recordedShadowScreenX`, and drive the §20 camera model from it — which plays any recording correctly at any width, and is what this port does. The `onScreen` caveat is scoped to input-level re-simulation (lockstep/rollback), which the original never performs. `CORRECTION:` §18.1's throw-in state selection had been reduced to the hand-wave "the matching `ST_THROW_IN_*` for (side, zone, which team throws)" during the v1.2 rewrite; all twelve cases are restored. **`ADDED:`** the RNG seeding (`seed = currentTick >> 8`, `randXorIndex = currentTick & 0xFF`) — v1.4 defined the generators but never their initial state, which is observable on the first call; the `stoppageEventTimer` decrement site, previously armed in nine places and decremented in none; why the `destReachedState` handshake cannot deadlock even though `1 → 2` happens only in the idle branch (`StopAllPlayers` clears both teams' `controlledPlayer` at the start of every break — a port that keeps one alive across a restart *will* deadlock); and a note that the 14 sine-table deviations' signs are not derivable, so the literal table must be transcribed for bit-exactness. Round 4's three agents were cut short by a session limit before filing reports; these items were salvaged from a blind implementation attempt (an agent that could read only this document and tried to build the simulation from it). Its remaining flagged gaps are listed under "Known gaps" below. |
| 1.4     | 2026-07-29 | §2.1, §2.2, §5, §8.10, §8.10a (new), §9.1, §9.4 (new), §10.8, §11.2, §13.1, §13.2, §14.2a, §14.4, §14.5 (new), §15.2, §15.3, §15.4, §16.2, §16.5, §18.1, §18.2, §18.3, §19, §21.4a, §23, §24.12, §25.4, §25.5 | Third round: an audit of the round-2 additions, an independent pass over the least-reviewed sections, and the substitution system. **`CORRECTION:`** §13.1's stick-centred arm was placed after `input` had been reassigned, making it unreachable — as written it silently deleted the flying header, turning every centred-stick jump header into a flat case-0 header; §13.2's static-header steering is **one-shot**, not ±1 per frame (the routine replaces the pose it requires), so total aim authority is ±1 before contact plus ±2 at contact, not a free 180° rotation; §14.2a's penalty arms set `breakCameraMode = -1` themselves and skip the commit block; §15.2's wrong-half exit is tested *before* the three controlled/pass-target re-tests, not after; §14.4's `plgD3Param` is the **competition-match flag**, not the highlight path — so what v1.2 called a "second personality" is the normal case for every league, cup and career match and the LUT path is the friendly/training exception; §19's referee deactivate test precedes the move and its arrival tail is shared by the incoming and leaving states; §23's `InitTeamsData` is not a peer box but runs inside `InitGameRestoreTeams` via `StartingMatch()`; the fire-press end-game handler covers only 4 of 10 in-range states and *stalls* the other 6; `Sprite.injuryLevel` is never written `-1` or `-2` and `forceLeftTeam` is never written at all, so those branches are dead rather than live sentinels as v1.3 implied. **`ADDED:`** §8.10a the bench and substitution decision — the 11-condition gate list, the three-tap trigger, the `subsState` and `substituteInProgress` value tables, every global §20.1 reads, the squad edit and the substitution limit; §9.4 which animation identities gameplay actually tests (five) versus merely assigns; §14.5 `trainingGame` and its exactly three simulation rules; §24.12 rows for the animation tables, the per-image sprite metrics and `playersPositionsByTactics`; `field_46`/`field_84` in the §2.2 struct; the `GoalkeeperJumping` speed variant per call site; the AI restart gates and mid-range decision tree; the ball-animation fall-through that un-hides a stationary ball; `runSlower`'s real lifecycle (a 37.5 % cut on all 22 players for the whole goal reset). **`CLARIFIED:`** §5's `onScreen` is now specified exactly — the per-image `centerX/centerY/pixWidth/nlines` are authored data that change every animation frame, so a port with its own artwork cannot match it exactly. (The determinism conclusion drawn from this in the first draft of v1.4 was wrong and is corrected in v1.5.) |
| 1.3     | 2026-07-29 | §1.3, §5, §6.3, §8.3, §8.6, §8.9, §10.1, §10.7, §10.8, §12.5, §14.2a (new), §15.2–§15.6, §16.2, §16.4, §16.5, §17.1, §18.3, §20.1, §20.4, §21.4a (new), §24.4, §25.2, §25.3 | Second verification round: a correction audit of all 69 v1.2 edits, an independent source-first re-derivation of the five longest control-flow routines, and a whole-document consistency/completeness pass. The v1.2 edits audited **65/69 clean, 0 missed, 0 applied backwards**; the 4 partials and 3 rewording slips introduced by v1.2 are fixed here. **`CORRECTION:`** one instance of the `fullDirection` inversion survived v1.2 — §16.4 still commented `(fullDirection + 128)` as "face away from ball" when it turns the player *toward* it; §18.3 state 2 omitted that a keeper's ball sets `gameStatePl = ST_WAITING_ON_PLAYER` and therefore **completes the hand-over five states early**, making the keeper's-ball special cases in states 4–8 dead code, and its `destReachedState = 1` sweep is gated per team on `resetControls == 0`; §15.2's keeper cascade rewrote `goaliePlayingOrOut`/`ballOutOfPlayOrKeeper` unconditionally (they gate keeper selectability) and always overwrites the stopped-game speed; §15.2's "rush out" box is the penalty area X span with a trimmed Y band, not the 6-yard box; §15.5's recovery step keeps the *base* value once `field_46` is latched; §15.5's save epilogue also clears `penalty`/`playerHadBall` and clears `passingKickingPlayer` only when `goalkeeperSavedCommentTimer >= 0`; §16.2's CPU penalty release is on the *rejected*-direction arm (as written the AI never took a penalty); §12.5's prose contradicted itself on what a centred stick does (NORMAL is a 1.25→1.375 lift, only the HIGH arm chips); §17.1's `positions[5][7]` is incompatible with the flat `q = xq + 5*yq` index (the run is 7 rows of 5); §25.3 said the match clock is four digit bytes (it is three); §10.8's quadrant compares are unsigned, so an off-pitch prediction lands in the *last* quadrant; §20.4's inline comment and the §25.2 bullet still said `dx/4`. **`ADDED:`** §14.2a `TestFoulForPenaltyAndFreeKick` — the producer of every `ST_PENALTY`/`ST_FREE_KICK_*`/`ST_FOUL` state, previously absent although §3.4, §12.6 and §18.4 all consume it, so a port could never enter a free kick; §21.4a the stopped-state dispatcher, without which a match reaches minute 45 and stalls; the keeper's ball-gather block (§15.4), the only path by which a keeper picks up a ball at its feet; the keeper's save destinations `(ballNotHighX, ballNotHighY)` (§15.2), the only consumer of that predictor; the fire-press end-game short-circuit (§18.3); the `CalculateNextBallPosition` call site (§10.8); definitions for `stoppageTimerTotal` vs `stoppageTimerActive` and `showFansCounter` (§1.3); `animBase` (§15.6). **`CLARIFIED:`** §8.3 section A *falls through* into B (as written no restart could be taken); the keeper's `-> idle` exits are frame-gated 2-in-16; `GoalkeeperJumping`'s speed variants per call site; §16.5's mid-range gate tree; §6.3's "per frame" means per team update. |
| 1.2     | 2026-07-29 | §2.1, §2.2, §2.4, §3.1–§3.4, §5, §6.1, §6.3, §7.1–§7.3, §8.2–§8.6, §8.9, §8.11, §9.3, §10, §10.5, §11.1, §11.3, §12.3, §12.5, §13.1, §13.3, §14.1, §14.2, §14.4, §15.3, §15.5, §16.4, §16.5, §17.3, §18.1, §18.3, §18.4, §20.1, §20.3, §20.4, §21.4, §21.5, §22, §24.6, §24.7, §24.11, §24.12, §25.2, §25.4 | Full source re-verification of the whole document (three independent passes, every constant table diffed by script). 35 corrections + 23 clarifications + 11 additions. **`CORRECTION:` the material errors were:** `fullDirection` runs ball→player, not player→ball (§2.1/§3.3 — a 180° inversion); the three ball-proximity flags are mutually exclusive *bands*, not cumulative predicates (§2.2/§8.2 — changes the tackle launch range); §10.5's post test was missing its `bx <= 295 \|\| bx > 372` gate, so as written every ball crossing a goal line hit a post; the last-man foul is an 87.5 % straight red with the `playerCardChance` gate skipped, not a 12.5 % red (§14.2 — the odds were inverted); `kComputerTacklingDownTime` can never be selected by the CPU, only by a human "stab" tackle (§14.1/§8.6 — was stated backwards); §17.3 described a runtime man-marking system that does not exist (`markedPlayer` indexes the team's *own* sprites and is HUD-only; `pairIds` is editor-only); the camera lag is `dx/16`, not `dx/4` (§20.4); a centred stick at `spinTimer == 4` lofts the shot rather than doing nothing (§12.5); a dead-ball long throw returns early from `PlayerKickingBall` with no aftertouch window (§12.3); `penaltiesTimer` semantics were inverted (§21.5); the goal-attempt flag is cleared by the *defending* side (§22); a goal does not blow the whistle (§18.1); the fire classifier reads the global port counter, not `team.fireCounter` (§6.3); `§7.1` ties go to the higher index; `playerSwitchTimer` is ~1 s, not ~0.5 s (§7.3); the pseudo-angle error is median ≈ 2.9° / max ≈ 4.7°, not ≈ 2° (§3.2/§25.2); negative `z` takes the `scale = 8` path (§11.1); `§11.3`'s dispatch is not an else-chain; `§18.4` mirrors on the player's team, not the chosen table; the `0x18` AI mask is 75 %, not 87.5 % (§16.5); `field_30`/`field_3A` are equal in every table *except* `penaltyGoalieSkillTable` (§24.7 — this claim was introduced by 1.1). **`CLARIFIED:`** the two normalisation loops in `CalculateDeltaXAndY` *are* equivalent to a single loop (1.0 wrongly flagged them as load-bearing); `kAngleCoeficients` is a scaled-tangent, not a diamond-angle, approximation; `& 0xBB` applies only to goal kicks and keeper distribution; `kSineCosineTable` deviates at 14 entries and its 257th word is an unread pad, as is `kRandomTable`'s. **`ADDED:`** hidden sprites keep a stale `onScreen`; the `stoppageEventTimer` gate on the whole restart machine; the `refState` gate on state 3; `field_46` clearing in `GoalkeeperJumping`; the `plgD3Param` highlight-recording card path; the goal-celebration `+24` frame offset and the `cycleFramesTimer` reload in the animation walker. All §24 numeric tables were confirmed **byte-for-byte correct** (0 mismatches across the goalkeeper, skill, AI, card, pitch, quadrant, aim and RNG tables); the §1.1 pitch-geometry table and the §3.2 `kAngleCoeficients` generator (0/1024 mismatches) were also confirmed exact. |
| 1.1     | 2026-07-29 | §15.3, §24.7     | `CORRECTION:` the goalkeeper dive-reach explanation had the sense of `kGoalkeeperDiveDeltas` backwards. Was: "a skill-7 keeper mostly rolls indices 0..3 (fast reach)… it needs fewer frames". Now: the test is `framesDive >= framesY`, so a **smaller** delta yields **more** frames and makes a dive **more** likely; skill 7 draws index 0 (3.0) on 13 ticks in 16 and therefore dives more readily than skill 0, which draws index 7 (6.5). The pseudo-code and all constants were already correct — only the prose was wrong. Also expanded §24.7 with the verified `field_30`/`field_3A` progressions (`5..12` for skills 0..7, identical in both columns), re-derived from `src/swos_globals.c`. |
| 1.0     | 2026-07-29 | all              | Initial specification. World model, math core, frame loop, input, player selection, `UpdatePlayers`, movement/animation, ball physics + prediction, possession/passing/shooting/aftertouch, heading, tackling/fouls/cards/injuries, goalkeeper, AI, tactics positioning, restarts, referee, camera, clock/extra time/penalties, statistics, init order, full constant tables, porting checklist. |

### Known gaps

Found by an agent that was allowed to read **only this document** and tried to
implement the simulation from it. These are places where the document is
knowingly not self-sufficient; each is a deliberate deferral to §24.12 rather
than an error, but a porter should know about them up front.

| Gap | Status |
|-----|--------|
| The bulk data tables (§24.12) — the sine table's exact 14 deviations, `kAngleCoeficients` if you prefer literals to the generator, the 12 `TeamTactics` blocks, the 42 restart-position arrays, the animation tables and the per-image sprite metrics | **By design.** Too large to inline; transcribe from the named files. Everything needed to *use* them is specified. |
| §17.2 cannot be exercised without the `TeamTactics` blocks, and §18.4 cannot be exercised without the restart-position arrays | Consequence of the above — the *algorithms* are complete, the *data* is external. |
| The animation frame lists behind the five identity-tested tables (§9.4) | External, but only the identities matter for gameplay; substitute frame data freely. |

Both items previously flagged UNVERIFIED are now resolved:
`gameMaxSubstitutes > 5` is unreachable from every menu and all 89 shipped
presets (only a hand-edited `.HIL` header reaches it), and `substitutedPlX/Y`
has **no reader at all** — it is recorded at the commit press and never read.

**Still open after round 5.** Two small things the sources here cannot settle:

| Open question | Why it is open |
|---------------|----------------|
| `goalkeeperSkipFlag`'s intent (§15.2) | The global exists in `SWOS2.s` and in the port, is read exactly once, is commented in `SWOS2.s:21560` as "when non-zero, goalkeeper processing is bypassed" — and has **no writer anywhere**. Whether the original had one that IDA failed to resolve, or it is vestigial, cannot be determined. Implement the read; its position fixes an ordering (§25.4). |
| Whether the §15.5 contact window is the same width on the DOS sprite bank | The window is `pixWidth + 6` and `pixWidth` is per-image authored data from the loaded bank. The Amiga and DOS banks are different files, so a DOS-asset port has a different contact window by construction. Neither bank was dumped to compare. |

Two source-side defects were found while verifying round 5 and are recorded in
§25.4 / reported separately rather than silently patched: the four caught-ball
sprite ids in `src/update_players.c`, and the little-endian `uint16`
RNG-seed writes in `src/menus.c`, which split `currentTick` across
`seed`/`randXorIndex` the opposite way round from the m68k `move.w` they
replace. **Both were fixed on 2026-07-30 (2e57a8e)**: the ids are now
`0x35E/0x360/0x3E0/0x3E2`, and every seed site writes the two bytes explicitly
(`seed = currentTick >> 8`, `randXorIndex = currentTick & 0xFF`).

### How to update this document

**Bump the version** in the header block at the top of the file and add a row
here for every substantive change. Use `MAJOR.MINOR`:

* **MINOR** (`1.0` → `1.1`) — a correction, a clarification, a newly documented
  routine, or new constants. Anything that does not invalidate work already done
  against the previous version.
* **MAJOR** (`1.x` → `2.0`) — a change that means an existing port is now
  **wrong**: a corrected constant, a corrected formula, a re-described state
  machine, or a reversal of something previously stated as fact.

**Versions 1.0 through 1.7 are the pre-release verification series.** Every one of
1.1–1.7 contains corrections that this rule would otherwise make MAJOR; they were
rolled into MINOR bumps because no port existed to invalidate. **1.7 is the
released baseline**, and from here the rule above applies as written — the next
correction to a constant, a formula or a state machine is `2.0`.

**Rules for entries**

* One row per change, newest at the top. Keep the "Sections touched" column
  filled in with `§` numbers so a reader can diff against their implementation.
* If a value or formula is **corrected**, say so explicitly and give the old and
  new values — someone has already implemented the old one. Prefix the change
  with `CORRECTION:`.
* If new behaviour is documented that was previously missing, prefix with
  `ADDED:`; for rewording with no behavioural change, prefix with `CLARIFIED:`.
* When a constant table changes, also state which source file it was
  re-derived from, so the provenance stays traceable (see §24.12).

**Scope reminders when editing**

* This document describes the **Amiga** behaviour. DOS differences belong in
  §24.10 or as an inline parenthetical, never as the primary value.
* Rendering stays out of scope except for the two feedback points named in the
  header (`Sprite.onScreen`, the animation walker).
* Anything asserted as fact here should be traceable to the port sources; if a
  detail is inferred rather than verified, mark it as such in the text rather
  than stating it flatly.

---

## Credits

The SWOS gameplay documented here was reverse-engineered from the original
binaries. This specification would not exist without:

- [Ben Baker](https://github.com/benbaker76) — Amiga Reverse Engineering and C Port
- [Zlatko Karakaš](https://github.com/zlatkok) — DOS Reverse Engineering

Sensible World of Soccer is © Sensible Software / Codemasters. This document
describes the game's mechanics for interoperability and preservation purposes
and contains no original game assets.
