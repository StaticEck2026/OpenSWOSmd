# NETPLAY.md — rollback netplay (GGPO-X)

**Status (2026-09-15): a real match plays between two peers.** GGPO-X is vendored and
callable from C, the match state it saves and loads is a registry the rollback rig proves
(docs/ROLLBACK.md), and a match runs as a frame step GGPO can call again. Under GGPO's
synctest, and as two processes over loopback that each know only one player's input and roll
back on every misprediction, recorded matches come out identical to plain runs at every frame
(§5). The inputs still come from recordings; live input is next.

The main menu's **MULTIPLAYER** button connects two machines directly, by IP address, and
checks they would play the same match (§6). Setting a match up over that connection, and
playing it, is what is left.

OpenSWOS continues SWOS 2020, whose online play runs on
[GGPO-X](https://github.com/thomashenry79/ggpo-x). We use the same library at the same
commit, so the protocol and the timing behaviour are the ones SWOS 2020 players know.
The lobby, login and matchmaking are SWOS 2020 menus, imported separately
(docs/MENUS.md §10).

## 1. What is where

| Path | Role |
|---|---|
| `third_party/ggpo-x/` | the library (C++), upstream layout; `OPENSWOS.md` = provenance + patch list |
| `include/netplay.h`, `src/netplay.c` | OpenSWOS's netplay module — the only code that talks to GGPO |
| `include/netplay_match.h`, `src/netplay_match.c` | a match played through the frame step and GGPO: directly, under synctest, or as one of two peers (§5) |
| `include/match_step.h`, `src/match_step.c` | one frame of the players' input and the frame's clock, shared with the replay rig |
| `include/sim_state.h`, `src/sim_state.c` | the match state GGPO saves and loads (docs/ROLLBACK.md) |
| `scripts/netplay_test.sh` | recorded matches through the step, GGPO's synctest and two peers over loopback, compared frame by frame with plain runs |
| `include/p2p_link.h`, `src/p2p_link.c` | the UDP handshake two players make before a match (§6) |
| `include/p2p_menu.h`, `src/p2p_menu.c` | the MULTIPLAYER menu over it, JSON-only (§6, docs/MENUS.md §10) |
| `src/online_menus.c` | the imported online menus' handlers (docs/MENUS.md §10) |

## 2. Building

`NETPLAY=1` compiles `third_party/ggpo-x` with `$(CXX)`, defines `SWOS_NETPLAY` for
the C code, and links with `$(CXX)`.

| Target | Default | Notes |
|---|---|---|
| Linux x86_64 (`make X86_64=1`) | **on** | built and self-tested |
| Cygwin64 (`bin/SWOS.exe`) | **on** | built; `cygcheck` shows the same DLLs as before |
| Linux x86 (`make`, -m32) | off | needs g++-multilib (no 32-bit libstdc++ installed here) |
| macOS, ARM Linux | off | `NETPLAY=1` should work — the POSIX platform layer is shared — not yet tried |
| WASM | never | a browser has no UDP sockets |
| Switch | off | `Makefile.switch` does not build it; sockets there are `nn::socket` |
| Amiga | — | `netplay.c` is in `SDL_ONLY_SRCS` |

The C++ runtime is linked **statically** (`-static-libstdc++ -static-libgcc`, not on
macOS), so no DLL or `.so` is added: the Windows drop's four-DLL list and the Linux
`lib/<arch>` bundle in CLAUDE.md are unchanged. Check with `cygcheck bin/SWOS.exe` and
`ldd bin/SWOS.x86_64` after touching the link.

Without `SWOS_NETPLAY`, `netplay.c` compiles to stubs: `Netplay_Available()` is 0 and
every call returns `NETPLAY_ERR_UNAVAILABLE`. Toggling `NETPLAY` needs a `clean`.

## 3. The API (`include/netplay.h`)

A `NetplaySession` wraps one GGPO session. The game provides `NetplayCallbacks` — save
state, load state, free state, advance one frame, events — and each frame calls:

```c
Netplay_Idle(s);
if (Netplay_AddLocalInput(s, localPlayer, &input) == NETPLAY_OK &&
    Netplay_SyncInput(s, inputs, &disconnectFlags) == NETPLAY_OK) {
    /* step the game with inputs[0], inputs[1] */
    Netplay_AdvanceFrame(s, checksum);
}
```

On a rollback GGPO calls `loadState`, then `advanceFrame` once per frame to re-simulate;
`advanceFrame` does the same SyncInput / step / AdvanceFrame triple. `checksum` feeds
GGPO-X's desync detection (a `NETPLAY_EVENT_DESYNC` names the frame).

`Netplay_StartP2P` takes local port, local player (1/2), remote IPv4 and port, input
size, frame delay, max prediction (≤ 8) and the disconnect timers.
`Netplay_StartSyncTest` runs both players locally and re-simulates every frame to
check that save/load/advance are deterministic — the tool to point at the real game
first.

### Self-test: `bin/SWOS.x86_64 --netplay-selftest`

Drives the library for real with a toy deterministic game (its state is a frame counter
and a hash of every input consumed):

1. **Two peers over loopback** (UDP 47901/47902, one process), 600 frames. Each peer's
   input changes every few frames, so the other's "repeat last input" prediction keeps
   failing. Passes when both reach frame 600 with identical state at **every** frame,
   rollbacks actually happened, and there were no desync or disconnect events.
   A typical run: ~1400 ticks, ~200 rollbacks covering ~1400 re-simulated frames.
2. **Synctest**, 600 frames, check distance 1: 1201 saves, 600 re-simulations.
   A checksum mismatch aborts inside GGPO.

Don't run it from the Cygwin build on a desktop you are not watching: binding a UDP
port makes Windows Firewall ask.

## 4. Integrating with a match — the design

### What SWOS 2020 does (`swos-2020/src/mpgame.cpp`)

- **State:** `save_game_state` is one `memcpy` of the whole game data segment,
  `data_begin`..`data_end` in its converted asm, plus a copy of the sprite save list
  (`pushSavedSprites`). `load_game_state` copies it back.
- **Input:** `GGPO_Input { byte control; }` per player — the joypad control word.
- **Session:** `ggpo_start_session(..., "swos2020_mp_game", 2, sizeof(INPUT_TYPE),
  localport, max_rollback)`, max rollback `min(8, frameDelay + 3)`, default frame
  delay 2, ports 47885/47886, disconnect timeout 5000 ms / notify 1000 ms; the remote
  address comes from the matchmaking server.
- **Frame:** `ggpo_advance_frame(ggpo, checksum)` after every simulated frame.

### Why that does not carry over as is

SWOS 2020's data segment is one contiguous block, like the Amiga BSS. Ours is not:
every global is its own object at a linker-chosen address (CLAUDE.md, "Adjacent-globals
BSS pattern"), interleaved with state that must **not** roll back — SDL, audio, menus,
the renderer's record of what is on screen, the netplay session itself.

### Decided: the match state registry (`include/sim_state.h`, docs/ROLLBACK.md)

An explicit table of named variables, chosen and proved by the rollback rig rather than
by a linker section. It keeps the snapshot small and names every byte a resync sends.

| call | use |
|---|---|
| `SimState_Save/Load(buf, SIM_TIER_ALL)` | GGPO `save_game_state` / `load_game_state`: raw copies, this process only |
| `SimState_Hash(SIM_TIER_SIM)` | the checksum GGPO compares between peers: computed over the portable encoding, so it is equal on every platform for the same match |
| `StateWire_Encode/Decode` (`include/state_wire.h`) | a resync or a mid-match join: SIM (or SIM+VIEW) in the portable encoding, as a validated, deflated delta against a base both peers hold — about 1.3–1.9 KB against the kick-off state, a few hundred bytes against a recent agreed frame |
| `SimState_Size(tiers)` | buffer size |

What crosses the network during a match is only the inputs (two bytes per player per
frame, §5) and the SIM checksum; GGPO's save and load stay on the machine. State is sent
only to resync, in the binary packet described in docs/ROLLBACK.md, "The wire format". Its
state is pointer-free and big-endian — pointers travel as identities, like saves — so a
32-bit, a 64-bit and a Windows build exchange it (`scripts/rollback_xbuild.sh`).

Two tiers. `SIM_TIER_SIM` (16.8 KB raw, ~2.6 KB compressed) is everything the simulation
reads and everything that decides anything it reads — closed: no SIM byte depends on a
VIEW byte, which the rig proves by restoring SIM alone. `SIM_TIER_VIEW` (19 KB) is the
highlight ring and its pointers: restore it on a local rollback so the instant replay does
not show a rolled-back goal; a resync can skip it, as long as the session does not play
in-match replays (their playback reads the ring).

Synctest is the acceptance test before two machines are tried: `Netplay_StartSyncTest`
on a real match must run without a checksum mismatch. It does (§5).

### Still to do

- **One simulation width on both peers.** The simulation still reads the render width
  in a few places (the fan-tile column in `DrawAnimatedPatterns`, the camera clamp in
  `SetCameraCoordinates`), so a 16:9 peer and a 4:3 peer diverge from frame 0. Until
  that is removed, a session must pin both to the same width.
- **Sound during re-simulation.** A re-simulated frame is never presented (§5), but the
  VBlank interrupt's work, which it runs, includes the audio channel update — a match with
  sound would play a re-simulated frame's effects again. The tests run with audio off.
- **Instant replays and pause** leave the match loop, and so do the keys that start them
  (`MainKeysCheck`: R, H, P, ESC, and S for the statistics); in a session they have to be
  either disabled or driven by both peers' inputs. A detour into a replay ends a test run.
  Pause waits for input inside a frame; the statistics overlay did too, and now makes one
  pass per frame (§5, "Two peers").
- **Determinism:** the simulation must not read wall-clock state. The rollback rig's
  determinism check runs two processes loaded at different addresses and requires
  identical per-frame hashes.
- **Frame pacing:** `NETPLAY_EVENT_TIMESYNC` asks the peer that is ahead to slow down.
- **The lobby:** the matchmaking server hands out the peer address; until that is
  ported, a direct-IP connect is enough to test two machines.

## 5. Playing a match through GGPO (`src/netplay_match.c`)

GGPO re-simulates from inside its own calls — `Netplay_AdvanceFrame` loads an older state
and calls `advanceFrame` for every frame since — so a netplay match cannot be a loop with
the frame boundary at its top. Each frame is a **step**, called once as the frame is first
played and again for every re-simulation of it:

| part | what | where |
|---|---|---|
| input | both players' `MatchPlayerInput` into the joystick registers, the fire bits, the bench calls and the key buffer | `MatchStep_ApplyInput` (`src/match_step.c`) |
| the frame | the body of `GameLoop`'s loop, **drawing included** | `GameLoop_Frame` (`src/game_loop.c`) |
| the clock | `_currentTick`, and `_currentGameTick` behind the interrupt's own gates | `MatchStep_AdvanceClock` |
| the swap | swap the buffers and set the camera | `GameLoop_EndFrame(0)` |
| the VBlank interrupt | its per-frame work: fade ticks, audio channels, key repeat | `_Int3TimerHandler` |

Only the frame GGPO ends on is shown: `Platform_Present`, which is `Platform_VBlank`
without the interrupt. `GameLoop` itself is now the same parts in a loop, so the replay
rig and a plain match run exactly what a netplay step runs.

Three things decide that shape:

- **Drawing is part of the frame.** The render calls write match state: `DrawSprites`
  sets `onScreen`, which the match reads, and `DrawAnimatedPatterns` calls `Rand`. A
  re-simulation that skipped drawing would play a different match.
- **The VBlank interrupt is part of the frame.** On the Amiga it runs once after every
  frame. On SDL, `Platform_VBlank` ran it between frames, outside anything GGPO calls, and
  the first synctest failed re-simulating frame 1; with the interrupt in the step, every
  frame matches. (It steps the fade, and a match starts with a fade-in.) A netplay match
  also quiesces the timer thread that would run it too, as the rollback rig does.
- **An input's edges are match state.** The bench call and a key press act on a button's
  rising edge, so the previous frame's buttons are remembered — in `g_matchInputLatch`, a
  6-byte registry entry, so a rollback puts them back.

### The input

`MATCH_INPUT_SIZE` is 2 bytes per player per frame, the same on every platform:

| byte | bits |
|---|---|
| `controls` | up 0x01, down 0x02, left 0x04, right 0x08, fire 0x20, bench 0x40 |
| `key` | the key held this frame, as its `keycodeLut` value (0: none) |

The stick and fire bits are the recordings' own. A player's bench button calls the bench of
the team that player controls — its coach, or with none its player — which is also the
guard `MainKeysCheck` applies. A recording's bench button is per team, so the replay rig
hands each press to the player controlling that team (docs/TESTS.md §6b).

### The callbacks

| callback | does |
|---|---|
| `saveState` | `SimState_Save(SIM_TIER_ALL)`, 35,654 bytes on x86_64; its checksum is `SimState_Hash` of both tiers — over the portable encoding — folded to 32 bits |
| `loadState` | `SimState_Load(SIM_TIER_ALL)` |
| `advanceFrame` | `Netplay_SyncInput`, the step, `Netplay_AdvanceFrame` |
| a frame's checksum | `SimState_Hash(SIM_TIER_SIM)` folded to 16 bits: what a peer compares |

Under synctest the match also keeps every frame's state as first played. GGPO compares only
checksums, and aborts; before it does, the match prints which registry entries differ.

### Testing it

`--netplay-match=<mode>` with `--replay-test=<file.swr>` plays that recording's match through
the step, with both players' inputs from the recording:

| mode | |
|---|---|
| `direct` | the step, without GGPO |
| `synctest[:N]` | GGPO's synctest: every N frames (1–8, default 1) it loads the state of N frames back, plays them again and compares their checksums with the first run's |
| `p2p` | one of two peers, each a process with one player's input (below) |

It takes the rollback rig's watching options (`--rollback=trace`, `--rollback-trace`,
`--rollback-dump`, `--rollback-save-state`) and refuses the ones that rewind the match.
Without `--rollback` the run is the replay rig's comparison with the recording, played
through GGPO. `scripts/netplay_test.sh` plays recordings plain and in each mode and requires
the traces to be identical at every frame.

Results (2026-09-15), over the recorded corpus — 13 recordings, each with Amiga and with DOS
gameplay: 26 matches, 488,837 frames — every trace compared at every frame with a plain run
of the same build (`scripts/netplay_test.sh --all --modes "direct synctest:1 synctest:8 p2p
p2p:2" --stall 8`, 652 s at 4 jobs):

| mode | result |
|---|---|
| direct | 26 of 26 matches identical to the plain run |
| synctest:1 | 26 of 26 identical; 488,837 frames re-simulated, one load per frame |
| synctest:8 | 26 of 26 identical; 488,768 frames re-simulated from 61,096 loads |
| p2p | 26 of 26, both peers identical; 8,481 rollbacks re-simulating 35,343 frames, up to 590 in one match |
| p2p:2 | 22 of 22, both peers identical; 6,745 rollbacks re-simulating 28,416 frames. 4 skipped: zkz-11122841 and zkz-12065730 have input in the 2 frames the delay drops |

Every re-simulated frame's checksum matched. Compared with the recordings instead, a match
played through synctest gets the same result as the replay rig on its own: zkz-12065730 and
playa-10090613 pass every frame, and zkz-11215003 diverges at the same frame (12,646) on the
same field. A synctest:1 match takes 1.2 to 1.5 times as long as a plain one.

### Two peers (`--netplay-match=p2p`)

Each peer is a process of its own with one player's recorded input. GGPO predicts the other
player's input — the input it last received — and when the real input arrives and differs,
loads the state from before the mispredicted frame and plays forward again.

```
bin/SWOS.x86_64 --replay-test=<file.swr> --rollback=trace --netplay-match=p2p \
    --netplay-player=1 --netplay-port=7001 --netplay-peer=127.0.0.1:7002 --netplay-trace=p1.trace
```

What makes a peer's run comparable with a plain one:

- **Its trace is the match as finally played.** A peer's state at a frame boundary may still
  rest on a prediction, so it is not hashed there. Every save GGPO asks for records that
  frame's hashes instead, and a later save of the same frame overwrites them: once every
  frame is confirmed, each entry is the frame as it stands.
- **It ends on a confirmed frame.** `ggpo_get_confirmed_frame`, added to GGPO-X
  (`third_party/ggpo-x/OPENSWOS.md`), is the last frame whose input from both players is
  known. A frame is final once every input before it is confirmed; the match ends at the
  first final frame where it is over (`_playGame == 0`), or at the recording's end. A frame
  played past the end — predicted, or re-simulated after a correction ended the match sooner
  — changes nothing.
- **It waits for the other peer before leaving**, until that peer has acknowledged every input
  (GGPO's send queue is empty) and so can reach the same end — 3 seconds at most.
- **A loop waiting for input inside a frame makes one pass.** The statistics overlay
  (`KeepStatisticsOnScreen`) spun inside a single frame until a player pressed fire; a peer
  holding only a prediction of the input that closes it spun for ever, and the first two-peer
  run of zkz-12065730 hung there. While input arrives a frame at a time (`_gFrameStepInput`,
  set by the replay rig and by netplay) the loop makes one pass per frame and the overlay
  stays up into the next frame (`GAME_FRAME_HELD`). Both recordings that open it close it
  within that pass, and their plain traces are unchanged.
- **Input delay drops input.** With a delay of D frames, the first D frames play with no input
  from that player. A recording with input there cannot equal the plain run; the peer says
  so, and `scripts/netplay_test.sh` reports the match as skipped.
- **`--netplay-stall=<ms>`** makes a peer pause now and then, as a busy machine does, so the
  other plays ahead on predicted input and rolls back far more than on an idle loopback.
  Its own generator decides when — never the game's.
- GGPO-X compares the peers' checksums of confirmed frames (`frameChecksum`, the SIM hash
  folded to 16 bits); a `DESYNC` event fails the run.

In most recordings one team is human, so the other player's input rarely changes: the peer
predicting it is almost always right and seldom rolls back, while the peer predicting the
human's input rolls back hundreds of times a match (in zkz-12080203 both do). Both peers'
traces are compared.

### The network between them (`src/netplay_netem.c`)

Over loopback nothing goes wrong, so nothing is proved: every prediction is correct, and
the first two-peer run measured 1200 frames with **zero** rollbacks. The emulator puts the
misbehaviour inside the process, on the SEND side, through GGPO's generic transport filter
(`Udp::SetTransportFilter`, `third_party/ggpo-x/OPENSWOS.md`). Send-side because a
one-way delay applied by both peers is the same thing as delay on the path between them,
and because it lets each peer be given a link of its own — which is how one player on a
bad line is reproduced. Its randomness is its own and never the game's: `Rand()` is match
state both peers must agree on, and a harness that perturbed it would desync the match it
is testing.

The conditions are the set clumsy (jagt.github.io/clumsy) offers, which is the list of
what a real link does to a packet:

| clumsy | here | what it does |
|---|---|---|
| lag | `SWOS_NETEM_DELAY_MS`, `_JITTER_MS` | hold every packet, by a per-packet amount |
| drop | `SWOS_NETEM_LOSS_PCT` | the packet never leaves |
| out of order | `SWOS_NETEM_REORDER_PCT`, `_REORDER_MS` | this packet is held back behind the ones sent after it |
| duplicate | `SWOS_NETEM_DUP_PCT`, `_DUP_COUNT` | the same packet arrives more than once |
| throttle | `SWOS_NETEM_THROTTLE_PCT`, `_THROTTLE_MS`, `_THROTTLE_DROP` | nothing is sent for a window, then the lot goes out at once — or is thrown away, which is the link simply being gone |
| tamper | `SWOS_NETEM_TAMPER_PCT`, `_TAMPER_BITS` | bits flipped in the packet |
| set TCP RST | — | the transport is UDP. What it models, the connection cut from under you, is `_THROTTLE_DROP` |

All of it is one queue of whole packets with a due time, and one rule — send what is due.
The queue is in send order and the pump scans it, which is what lets packets overtake each
other. `SWOS_NETEM_PROFILE` names a whole link (`lan adsl wifi mobile satellite awful`);
any knob set alongside it overrides it. A packet that needs none of this is not touched at
all, so "no delay" really is no delay rather than "one poll later".

```bash
scripts/netplay_test.sh --modes p2p --netem wifi              # the corpus, over a wifi link
scripts/netplay_test.sh --modes p2p --netem delay=80,jitter=30,loss=5,reorder=5
PROFILE=wifi scripts/netplay_live_test.sh                     # the live pair test
JOIN_PROFILE=awful scripts/netplay_live_test.sh               # one peer on a bad line
```

**The result must not change.** Repairing exactly this is what rollback is for, so both
peers must still finish identical to the plain run, however the link behaves; the seed is
each peer's port, so a failure repeats. Measured on zkz-12065730 (DOS gameplay, 1520
frames), every row identical to the plain run:

| conditions | what the link did to player 1's packets (about 1.6k of them) | rollbacks p1 / p2 |
|---|---|---|
| none | — | 6 / 8 |
| `lan` | 1568 delayed by 2 ms | 8 / 11 |
| `adsl` | 19 dropped, 10 reordered | 9 / 16 |
| `wifi` | 23 dropped, 19 reordered, 357 burst in 30 throttle windows | 8 / 12 |
| `mobile` | 37 dropped, 26 reordered, 22 duplicates, 799 burst in 58 windows | 7 / 12 |
| `satellite` | 1969 delayed by 300 ms, 18 dropped | 9 / 17 |
| `awful` | 168 dropped, 62 reordered, 38 duplicates, 919 burst in 75 windows | 7 / 11 |
| `dup=50` | 760 duplicates | 6 / 2 |
| `reorder=20,reorder_ms=40` | 283 reordered, nothing else | 6 / 3 |
| `throttle=3,throttle_ms=400,throttle_drop=1` | 565 packets lost in 35 blackouts of 400 ms — 14 s with no link at all | 6 / 7 |

Over the whole corpus (`--all --modes p2p --gameplay dos --netem wifi`): 13 matches,
248,493 frames, **every peer identical to the plain run** — 6,819 rollbacks re-simulating
33,619 frames, while the link dropped 2,585 of player 1's packets and stalled 5,615 times.

The live pair test takes the same conditions, and this is where a peer gets a link of its
own: `PROFILE=wifi JOIN_PROFILE=awful scripts/netplay_live_test.sh` (1200 frames) ends both
peers on the same state hash, having played very different matches — the wifi peer dropped
13 packets and rolled back 76 times, the awful one dropped 122 and rolled back 141,
re-simulating 765 frames.

Each run prints what the link actually **did**, not what it was asked for
(`[netplay] network emulation did: ...`), because a condition set at 2% may never have
fired and a run that tested nothing should say so.

**Tamper is a fuzz knob, not a link, and it fails the run.** UDP carries a checksum and
the hardware drops what fails it, so a flipped bit reaching the game is rare — what it
tests is whether a corrupt packet is refused rather than acted on, and the answer today is
that it is acted on. `--netem tamper=2` kills the session within a second of kick-off,
before a frame is played, and both peers report the other as disconnected. Traced: a
tampered input packet arrived with `start_frame` 4194339 (bit 22 of frame 35) against a
last-received frame of 42, GGPO took it, and that peer processed no further packet. GGPO
trusts every field of an unauthenticated UDP packet — `start_frame`, the per-player
`last_frame`, the `disconnect_requested` bit that ends the match on its own — and the only
thing it checks is a 16-bit magic number, which a flipped bit elsewhere leaves intact. A
fix is a packet-level integrity check in GGPO-X, or bounding the frame numbers a message
may claim; neither is done. **This is why no profile turns tamper on** and why the corpus
suite does not run it.

### Next

1. **Live input**: a peer's input from its own pad and keyboard instead of a recording, and
   the in-match keys (R, H, P, ESC, S) kept out of a session or sent as input. The pause loop
   waits for input inside a frame, as the statistics overlay did.
2. **Presentation during re-simulation**: sound, and the overlay's one pass, which still calls
   `Platform_VBlank` (it presents, and runs the VBlank interrupt a second time in that frame --
   deterministically, so the match is unaffected).
3. **The match over the link.** The handshake and the connection are §6; what is left is
   the host sending the match setup, and opening the GGPO session over it. Pacing from
   `NETPLAY_EVENT_TIMESYNC` too.
4. The lobby.

----

## 6. The MULTIPLAYER menu (`src/p2p_menu.c`, `src/p2p_link.c`)

The main menu's MULTIPLAYER button opens a menu of OpenSWOS's own, `p2p/p2pMenu`
(JSON-only, docs/MENUS.md §10): one player hosts on a UDP port, the other joins by the
host's IPv4 address. No server, no lobby — SWOS 2020's matchmaking menus are a separate
import (`src/online_menus.c`) and need its servers.

| Row | What it is |
|---|---|
| GAME | host a game, or join one |
| HOST ADDRESS | the host's IPv4 address, typed in; the host itself needs none |
| UDP PORT | 47885 by default, SWOS 2020's `GGPO_PORT1`. The host binds exactly it, so it is the port a player forwards; a joiner takes the next free one above it, so two peers can share a machine |
| INPUT DELAY | frames of local input delay for the match to come, 0..6 |

The status panel follows the link every frame — the menu is flagged `dos_redraw`, so its
`onDraw` polls — and shows the round trip once measured, which is the number to choose an
input delay from.

**The link is the handshake, not the match.** Both sides must agree on everything the
simulation depends on before a session starts, because a disagreement is a desync on the
first frame rather than an error:

- the protocol version,
- `SimState_LayoutHash` — the shape of the state a rollback saves, so two builds whose
  match state differs refuse each other,
- the gameplay style (DOS or Amiga, arcade, the original fire-hold lockout),
- the screen width, until the simulation stops reading it (§4, "Still to do").

A joiner that differs is refused with the reason, and the host is told who bounced off it.

Six packets, all UDP, each either repeated until it is answered or harmless to lose:
`HELLO`, `WELCOME` and `REFUSE` for the handshake, `PING`/`PONG` for the round trip, `BYE`
on leaving. Every one starts with `"SWUN"`, the protocol version and the type, so a peer
of another version is recognised and told so rather than ignored; fields are big-endian,
like the save format. The socket is non-blocking and polled from the menu's draw, and it
closes before GGPO opens its own on the same local port — which is also what keeps a NAT
mapping open for the session that follows.

```
bin/SWOS.x86_64 --netplay-selftest     # GGPO's tests, then the link over loopback
bin/SWOS.x86_64 --menu=p2p/p2pMenu     # open the menu straight away
bin/SWOS.x86_64 --menu-selftest        # it unpacks the way its JSON says
```

The self-test runs the whole protocol over loopback: two links connect and measure the
round trip, a second joiner is turned away, a peer leaving is noticed, a build that would
play a different game is refused with the reason, and a join with nobody listening gives
up instead of hanging.

### The link also settles two things the match needs

**The input delay, agreed rather than chosen twice.** Each peer proposes one from the
round trip it measured (`propose_delay`), they exchange proposals, and both take the
**maximum** — the same two numbers on both sides, so the same answer — fixed for the
match. `P2PLink_AgreedDelay()`.

This is deliberate, and it is the bug that makes one player feel they are **chasing** the
other. SWOS 2020 derives a delay on each machine from that machine's own ping, jitter and
`1000/getMPFps()`, and applies it to its own player only; two machines do not measure the
same jitter, and if one runs slower they do not even agree on the frame time, so a match
that began "AUTO 3" can end up 1 against 5. It announces the number to the other player
(`sendFrameDelayInfo`) — as English prose, into the chat, which nothing parses. Ours takes
its frame time from the **gameplay mode**, which the handshake already forces to be equal,
rather than from anything either machine measures.

It is very probably most of the reported **ghosting** too: in a shoulder-to-shoulder
contest for the ball, four extra frames decides it before the game's own skill roll is
reached, and the rollback that follows drags the losing player visibly back *through* the
opponent. (A second candidate, not confirmed: `CalculateIfPlayerWinsBall` gates on
`oppTeam->wonTheBallTimer` and the winner sets it to 12, so whichever team is evaluated
first in a frame gets to contest and the other is locked out. Against the AI that bias is
invisible; with two humans one of them is always the team evaluated first.)

**The match setup**, as numbered chunks with a bitmap ack: `P2PLink_SendSetup()` /
`P2PLink_ReceivedSetup()`. The payload is a `state_wire` packet (docs/ROLLBACK.md, "The
wire format"), which already encodes portably — typed fields big-endian, pointers as
identities — so a 32-bit peer and a 64-bit one read the same bytes. All the link adds is
getting ~35 KB across UDP intact.

> The read buffer is sized by the largest packet, not the typical one. It was 64 bytes —
> ample for the control packets and 16 short of a setup chunk's header. `recvfrom`
> truncates silently, so every chunk arrived with all but its first 48 bytes missing while
> the transfer still completed and acked: right length, full bitmap, wrong contents. The
> self-test sends 40 KB and compares every byte, which is how that was found.

**What is missing is the match itself**: opening the GGPO session over the link. The live
input is done (`InputSDL_LocalMatchControls`, `NetplayMatch_StartLive`) and the corpus
p2p tests still pass with it in; what is left is starting a match on both peers, loading
the host's state onto the joiner and handing over to the session.

---

## 7. Matchmaking (`src/mm_client.c`, `third_party/matchmaker`)

§6's menu connects two players who already know each other's IP address. That is a
useful test and a bad way to find an opponent, so online play goes the way SWOS 2020's
does instead: a **matchmaking server** introduces two players, and the match itself is
still peer-to-peer.

**The server only introduces you.** Its `START:` message hands each side the other's
candidate addresses and it is then out of the loop. Killing it mid-match does not end
the match; it only stops new ones being made.

### Running one locally

```bash
scripts/run_matchmaker.sh                  # build + run on UDP 19602
bin/SWOS.x86_64 --matchmaker-selftest      # one client: log in, lobby, create, leave
scripts/matchmaker_pair_test.sh            # two clients: join, approve, START
```

The server is SimpleMatchmaker, vendored at `third_party/matchmaker` — the same
project SWOS 2020 ships. Its `OPENSWOS.md` records what was changed and why. Both it
and the client are built on the one vendored ENET (`third_party/enet`).

### The protocol

Text over ENET, one message per packet, `HEADER:content`. On connect, in this order:

```
VERSION:<gameID>            a compatibility tag; a different one is refused
INFO:<addr,addr,...>        our candidate addresses
LOGIN:<userName>:<userData>
```

then `CREATE:<min>,<max>`, `JOIN:<owner>`, `APPROVE:<player>`, `EJECT:<player>`,
`LEAVE:`, `START:`. Back come `LOGIN:`/`CREATE:`/`LEAVE:`/`EJECT:` as acks,
`PLAYERSACTIVE:` and `GAMESOPEN:` as list updates, `GAMEINFO:`, `JOIN:<name>` and
`START:<playerNumber>,<peerName>,<addr>,...`.

**Three things about this protocol are not guessable, and each one fails silently:**

- **`CREATE:` takes `"<minPlayers>,<maxPlayers>"` and nothing else.** The server splits
  on `,`, requires exactly two parts and `stoi`s both; anything else throws and the game
  is simply not created, with **no reply at all**. Sending a descriptive name (the
  obvious guess) looks exactly like the server ignoring you.
- **The two list messages are encoded differently**, though both are "a list of names":
  `PLAYERSACTIVE:` is `<count>:<name>:<dataLen>:<data>...` and `GAMESOPEN:` is a bare
  `name,name,` with a trailing comma and no count. Parsing the second like the first
  yields zero games, which reads as "nobody is hosting".
- **Addresses are a decimal uint32, not a dotted quad** — `ToString` is
  `std::to_string(addr.host)`, the four octets in network byte order printed as one
  number (127.0.0.1 is 16777343). Sending dotted text does not fail: the server `atoi`s
  it to `127` and later tells your opponent to connect there. The first pair test
  reported peers at 127, 10, 192 and 169 — the first octets of the addresses it meant.

`GAMEINFO:` only arrives from the server's `SendInfoToAll()`, i.e. once somebody asks to
join — so an empty game info straight after `CREATE:` is correct, not a dropped message.

### The client

`src/mm_client.c`, in C, against the vendored ENET. It keeps the lobby in plain arrays
you read whenever you like (`MmClient_Users`, `MmClient_OpenGames`, `MmClient_GameInfo`)
rather than delivering callbacks, because menus redraw from state every frame; the only
one-shot events are a player asking to join and being thrown out. Call `MmClient_Poll()`
once a frame.

The upstream C++ client (`ServerConnection/`, `P2PConnection/`) is vendored unbuilt, as
the authority on what the reference implementation does.

### The menus

**MULTIPLAYER now opens SWOS 2020's online flow**, not §6's direct-IP menu: log in →
matchmaking → the lobby. The handlers are in `src/online_menus.c`, against the menus
imported from SWOS 2020 (docs/MENUS.md §10). The direct-IP menu is still there and still
works — it is the way to test a link across a real router with no server in the middle —
but it is reached with `--menu=p2p/p2pMenu` now.

The matchmaking menu's `OnDraw` is also where the client is pumped: it runs every frame
(the menu is flagged `dos_redraw`) and there is no other per-frame hook while a menu is
up. The lobby is redrawn from `MmClient`'s state each frame rather than from callbacks.

```bash
bin/SWOS.x86_64 --matchmaker-menu=NAME   # log in and open the lobby at startup
SWOS_MM_HOST=... SWOS_MM_PORT=...        # point it at a server other than localhost
```

**The gdb harness cannot hold a lobby connection.** A process stopped at a breakpoint
sends no ENET keepalives, so the server times it out — the menu goes on saying ONLINE
while the server has already dropped it. Anything needing a live connection has to run
inside ONE continuously-primed harness session, with the opponent spawned from inside it.
Splitting it across two script runs lets the connection die in the gap.

### Status

Working and tested end to end against a local server:

- connect, log in, the user list, open games, create, join, approve
- `START:` on both sides with the peer's name, the right player number and usable
  addresses (`scripts/matchmaker_pair_test.sh`)
- MULTIPLAYER → login → lobby, with a real opponent appearing in it: `2 ONLINE`, the
  opponent's game in the host rows, both names in the user rows
- joining → the host approving → `START:` → the handoff to `p2p_link`, which puts
  `mpWaitingMenu` up and reports the link's state

**Still to do — the match itself.** `p2p_link` connects two peers and pings; opening the
GGPO session over it (the host sending the match setup, live pad input, running the
session) does not exist on EITHER entry point — see §6, "What is missing". Matchmaking
therefore ends exactly where the direct-IP menu ended. Two further pieces belong with it:

- **NAT traversal.** The server sends several candidate addresses precisely so a peer
  behind a router can be reached; the handoff currently takes the first one that parses
  and meets on a fixed port, which is fine on one machine and on a LAN and is not real
  hole punching. `third_party/matchmaker/P2PConnection/` is the reference.
- **The join prompt.** SWOS 2020 asks the host to accept or refuse (`hostGameMessageMenu`);
  we auto-approve. The event is already surfaced one-shot (`MmClient_TakeJoinRequest`),
  so wiring the prompt is small.
