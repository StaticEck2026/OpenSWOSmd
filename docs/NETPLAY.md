# NETPLAY.md — rollback netplay (GGPO-X)

**Status (2026-09-15): a real match plays through GGPO.** GGPO-X is vendored and callable
from C, the match state it saves and loads is a registry the rollback rig proves
(docs/ROLLBACK.md), and a match runs as a frame step GGPO can call again. Under GGPO's
synctest, which rolls the match back and re-simulates it, recorded matches come out
identical to plain runs at every frame (§5). Both players' inputs still come from a
recording, in one process; two peers are next.

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
| `include/netplay_match.h`, `src/netplay_match.c` | a match played through the frame step and GGPO (§5) |
| `include/match_step.h`, `src/match_step.c` | one frame of the players' input and the frame's clock, shared with the replay rig |
| `include/sim_state.h`, `src/sim_state.c` | the match state GGPO saves and loads (docs/ROLLBACK.md) |
| `scripts/netplay_test.sh` | recorded matches through the step and GGPO's synctest, compared frame by frame with plain runs |
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

It takes the rollback rig's watching options (`--rollback=trace`, `--rollback-trace`,
`--rollback-dump`, `--rollback-save-state`) and refuses the ones that rewind the match.
Without `--rollback` the run is the replay rig's comparison with the recording, played
through GGPO. `scripts/netplay_test.sh` plays recordings plain and in each mode and requires
the traces to be identical at every frame.

Results (2026-09-15), over the recorded corpus — 13 recordings, each with Amiga and with DOS
gameplay: 26 matches, 488,837 frames — every trace compared at every frame with a plain run
of the same build (`scripts/netplay_test.sh --all`, 250 s at 6 jobs):

| mode | result |
|---|---|
| direct | 26 of 26 matches identical to the plain run |
| synctest:1 | 26 of 26 identical; 488,837 frames re-simulated, one load per frame |
| synctest:8 | 26 of 26 identical; 488,768 frames re-simulated from 61,096 loads |

Every re-simulated frame's checksum matched. Compared with the recordings instead, a match
played through synctest gets the same result as the replay rig on its own: zkz-12065730 and
playa-10090613 pass every frame, and zkz-11215003 diverges at the same frame (12,646) on the
same field. A synctest:1 match takes 1.2 to 1.5 times as long as a plain one.

### Next

1. **Two peers over loopback**, headless: each process adds one player's recorded input,
   GGPO predicts the other's and rolls back when the prediction was wrong; both traces must
   equal the plain run's. The match may end only on a confirmed frame, never a predicted one.
2. Sound during re-simulation, the in-match keys, and a session handshake: the same build,
   simulation width and registry (`SimState_LayoutHash`) on both peers.
3. Live input, and the lobby.
