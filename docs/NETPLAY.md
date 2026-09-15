# NETPLAY.md — rollback netplay (GGPO-X)

**Status (2026-09-14): library in, game not yet.** GGPO-X is vendored, builds into
the SDL binary, is callable from C, and passes a two-peer loopback test and a synctest.
Nothing in a match uses it yet; the next step is the state snapshot (§4).

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

## 4. Integrating with a match — the open design

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
| `SimState_Hash(SIM_TIER_SIM)` | the checksum GGPO compares between peers: pointers hashed as offsets from the executable, so it is equal on every machine running the same build |
| `StateWire_Encode/Decode` (`include/state_wire.h`) | a resync or a mid-match join: SIM (or SIM+VIEW) as a validated, deflated delta against a base both peers hold — 1.3–1.8 KB against the kick-off state, a few hundred bytes against a recent agreed frame |
| `SimState_Size(tiers)` | buffer size |

What crosses the network during a match is only the inputs (one control word per player
per frame) and the SIM checksum; GGPO's save and load stay on the machine. State is sent
only to resync, in the binary packet described in docs/ROLLBACK.md, "The wire format" —
valid between identical executables until pointers travel as semantic identities.

Two tiers. `SIM_TIER_SIM` (16.8 KB raw, ~2.6 KB compressed) is everything the simulation
reads and everything that decides anything it reads — closed: no SIM byte depends on a
VIEW byte, which the rig proves by restoring SIM alone. `SIM_TIER_VIEW` (19 KB) is the
highlight ring and its pointers: restore it on a local rollback so the instant replay does
not show a rolled-back goal; a resync can skip it, as long as the session does not play
in-match replays (their playback reads the ring).

Synctest remains the acceptance test: `Netplay_StartSyncTest` on a real match must run
without a checksum mismatch before two machines are tried. The rollback rig already
does the equivalent headless, over the recorded corpus.

### Other prerequisites

- **One simulation width on both peers.** The simulation still reads the render width
  in a few places (the fan-tile column in `DrawAnimatedPatterns`, the camera clamp in
  `SetCameraCoordinates`), so a 16:9 peer and a 4:3 peer diverge from frame 0. Until
  that is removed, a session must pin both to the same width.
- **A callable single-frame step.** GGPO's `advance_frame` re-simulates frames on demand;
  `GameLoop` is one long loop with the frame boundary at its top
  (`ReplayTest_FrameStart`). The rig rewinds in place there; netplay needs the body
  factored into a step.
- **Presentation during re-simulation.** Re-simulated frames must not trigger sound
  effects twice or be presented; the renderer keeps its own record of what it drew
  (scroll offsets, both frontbuffers), which a rollback deliberately leaves alone.
- **Instant replays and pause** leave the match loop; in a session they have to be
  either disabled or driven by both peers' inputs.
- **Determinism:** the simulation must not read wall-clock state. The rollback rig's
  determinism check runs two processes loaded at different addresses and requires
  identical per-frame hashes.
- **Input:** what one frame of a player's input is — the replay format already records
  it per frame (docs/REPLAY_FORMAT_V2.md).
- **Frame pacing:** `NETPLAY_EVENT_TIMESYNC` asks the peer that is ahead to slow down.
- **The lobby:** the matchmaking server hands out the peer address; until that is
  ported, a direct-IP connect is enough to test two machines.
