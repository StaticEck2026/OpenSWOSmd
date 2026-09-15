# NETPLAY.md — rollback netplay (GGPO-X)

**Status (2026-09-15): a real match plays between two peers.** GGPO-X is vendored and
callable from C, the match state it saves and loads is a registry the rollback rig proves
(docs/ROLLBACK.md), and a match runs as a frame step GGPO can call again. Under GGPO's
synctest, and as two processes over loopback that each know only one player's input and roll
back on every misprediction, recorded matches come out identical to plain runs at every frame
(§5). The inputs still come from recordings; live input is next.

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

### Next

1. **Live input**: a peer's input from its own pad and keyboard instead of a recording, and
   the in-match keys (R, H, P, ESC, S) kept out of a session or sent as input. The pause loop
   waits for input inside a frame, as the statistics overlay did.
2. **Presentation during re-simulation**: sound, and the overlay's one pass, which still calls
   `Platform_VBlank` (it presents, and runs the VBlank interrupt a second time in that frame --
   deterministically, so the match is unaffected).
3. **A session handshake** — the same build, simulation width and registry
   (`SimState_LayoutHash`) on both peers — and pacing from `NETPLAY_EVENT_TIMESYNC`.
4. The lobby.
