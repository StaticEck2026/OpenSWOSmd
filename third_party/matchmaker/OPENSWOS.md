# SimpleMatchmaker in OpenSWOS

The matchmaking server SWOS 2020's online play talks to. Vendored so the whole
online flow can be run locally, with no public server involved:

```bash
scripts/run_matchmaker.sh          # build + run on UDP 19602
```

| | |
|---|---|
| upstream | https://github.com/thomashenry79/SimpleMatchmaker |
| taken from | `swos-2020/simple_matchmaker/SimpleMatchmaker-main` (the copy SWOS 2020 ships) |
| licence | MIT (`LICENSE`), Copyright (c) 2022 thomashenry79 |
| files taken | `Server/`, `common/`, `ServerConnection/`, `P2PConnection/`, `LICENSE`, `README.upstream.md` — CRLF -> LF |

## What is built, and what is only reference

**Built:** `Server/` + `common/`, by `CMakeLists.txt`, into
`build/matchmaker/swos-matchmaker`.

**Built as a TEST PEER:** `ServerConnection/` + `tools/mm_refpeer`, into
`build/matchmaker/mm-refpeer`. See below.

**Not built, kept to read:** `P2PConnection/` — upstream's peer link. Ours is
`src/p2p_link.c` and speaks its own handshake, so this one is reference only.

Our client is `src/mm_client.c`, in C, because it has to build for every SDL
target; the upstream one is C++ and Windows-leaning. It is vendored because it
is the **authority on the protocol**: when the server does something our client
does not expect, `ServerConnection.cpp` says what the reference client would
have done.

## The reference client as a test peer

`scripts/matchmaker_pair_test.sh` runs two copies of OUR client, so it can only
prove that our client agrees with itself — two implementations of one protocol
can share a misreading and nothing notices. `tools/mm_refpeer` drives
`ServerConnection` (the code SWOS 2020 ships) through the same host/join
sequence, and `scripts/matchmaker_interop_test.sh` pairs one of each **both ways
round**, so every message each side sends is parsed by the other implementation:

```bash
scripts/matchmaker_interop_test.sh    # 3 rounds, server started and stopped for you
```

It found one real incompatibility on its first run — the CREATE data blob below
— and confirmed the rest of the handshake is identical, including that the
server does not care that the two clients send different `VERSION:` tags
(`SWOS_MatchMaker` vs `OPENSWOS`): `Connections::VerifyVersion` is an empty
`// TODO` that returns true.

Two differences that are NOT faults, worth knowing before reading a capture:

- The reference binds a **fixed local port** (the first free of 6000–6009) and
  reports it as its P2P port; ours takes an ephemeral one
  (`enet_host_create(NULL, ...)`). Both work; a predictable port is friendlier to
  some NATs.
- The reference **disconnects from the server the moment `START:` arrives** and
  delivers its StartP2P callback from the disconnect event.

## Changes from upstream

Keep this list short and justified — it is a fork, and every entry is drift.

- **`Server/main.cpp`: the port argument works.** Upstream's README says "run it
  with no arguments and it will run on default port 19601. Pass a different port
  as an optional argument", but `main.cpp` hardcoded **19602** and ignored
  `argv`. We honour the argument and kept **19602** as the default, so a stock
  swos-2020 client still finds us. Also flushes the startup line, so a harness
  reading its stdout sees "running on port N" immediately rather than when the
  buffer happens to fill.
- **`Server/User.cpp`: `CREATE:` accepts the open-game data blob.** Upstream
  demanded exactly `<min>,<max>` and **disconnects** a user whose CREATE has
  anything else — while upstream's own `ServerConnection::CreateGame` sends
  `"<min>,<max>," + payload`. The two halves of the same release disagree, so
  the client SWOS 2020 ships could not host a game against this server at all
  (found by round 3 of the interop test). We accept `>= 2` fields and ignore the
  payload: nothing here stores it, so `GAMESOPEN:` keeps the old bare
  `name,name,` format that both clients parse. Storing it and emitting the
  newer `<count>:<name>:<len>:<data>` list is a feature neither client needs
  from us yet — `src/mm_client.c` only parses the old format.
- **`ServerConnection/ServerConnection.cpp`: a portable `Sleep()`.**
  `IPConnection::DoConnection` polls with the Win32 one; four lines of
  `nanosleep` under `#ifndef _WIN32` are what let the file build as the test
  peer. Nothing else in it is touched.
- **Builds against `third_party/enet` (1.3.18)** instead of its own bundled ENET
  (1.3.17). See `third_party/enet/OPENSWOS.md` for why one copy.
- **The relay: `Server/Relay.cpp` + `Server/include/Relay.h` (new), hooked into
  `Connections::StartGame` and `main.cpp`.** `--relay` makes OpenSWOS pairs play
  THROUGH the server: at START it makes a session with a random token, sends each
  player `RELAY:<token>,<n>` just before START, and forwards every datagram between
  the two addresses that register with that token. It shares the matchmaker's UDP
  port through ENet's `intercept` hook, so it needs no port of its own. Only between
  two clients whose `VERSION:` tag is `OPENSWOS` — the SWOS 2020 client's parser
  throws on a header it does not know (`ServerConnection.cpp:432` has no catch), and
  it could not use a relay anyway. Off by default, so a server run without flags is
  byte-for-byte the matchmaker it was. Protocol and reasons:
  `include/relay_proto.h` in the game tree; docs/NETPLAY.md §7, "The relay".
  - `--netem <profile|cycle>` puts the game's own network emulator
    (`include/netem_core.h` + `src/netem_core.c`, compiled in from the game tree so
    the two cannot drift) in each direction of every relayed match.
  - `--ledger <file>` appends one JSON line per finished session.
  - Its clock is `steady_clock`, NOT `enet_time_get`: that is `gettimeofday`, and a
    wall-clock step under WSL2 made a session in the middle of a match look a minute
    idle and closed it.
- **The recorder: `Server/Recorder.cpp` + `Server/include/Recorder.h` (new).**
  `--record` listens on TCP (the same port number) for players streaming a live match
  (`include/record_proto.h` in the game tree): both players' frame hashes are paired by
  match id and compared as they arrive, and a desync is reported the moment it is seen.
  A full recording also streams the match's `.SRP` frames, which the recorder writes
  to `--recordings DIR` with the game's own writer (`src/srp_writer.c` + miniz via
  `src/miniz_impl.c`, compiled in) — the same file as the host's `LATEST.SRP`,
  including what it takes to play the match again (the RESUME message and each
  frame's input: setup, kick-off state, inputs, hashes; `--srp-resim` in the game).
  Independent of the relay — a match played peer to peer can be recorded too. POSIX
  only; a Windows build of the server says so and carries on without it.
- **The WebSocket gate: `Server/WebGate.cpp` + `Server/include/WebGate.h` (new).**
  `--websocket`: browsers (the game's web build) send their datagrams as WebSocket
  messages over TCP on the server's port number; each browser socket gets a UDP socket
  of its own here, talking to the server's own UDP port, so everything behind the gate
  sees an ordinary client. `TcpGate` owns the TCP listener and sniffs each connection:
  `GET ` is a WebSocket, anything else goes to the recorder (`Recorder::Adopt` -- the
  recorder no longer listens itself). The relay serves any match with a browser in it
  (`RelayOptions::web`, `VERSION:OPENSWOS-WEB`). SHA-1 and base64 for the handshake
  are in the file. POSIX only, like the recorder. The game side is docs/NETPLAY.md §8.
- **`wss://` in the gate: `--tls-cert FILE --tls-key FILE`.** A page served over https
  may only open `wss://`, and Emscripten dials it on the datagram's own port, so TLS is
  terminated in the gate rather than by a proxy: `TcpGate` sends a connection opening
  with a TLS record (`0x16 0x03`) to `WebGate::Adopt(..., tls)`, which runs OpenSSL
  non-blocking (`SSL_accept`, then `SSL_read`/`SSL_write` in place of recv/send) and
  is a plain WebSocket after that. The pair is re-read within a minute of changing
  (a certbot renewal needs no restart). OpenSSL is the system's (`libssl-dev`,
  `find_package(OpenSSL)` in CMakeLists.txt); without it the server builds `ws://`
  only. SIGPIPE is ignored, since OpenSSL writes to the socket without MSG_NOSIGNAL.
  Deploying it: docs/NETPLAY_SETUP.md §10.
- **`Server/include/Ledger.h` (new):** the `--ledger` file the relay and the recorder
  share (one JSON object per line: `relay`, `match`, `desync`), and the clocks they use.
- **`Server/main.cpp`: options, and a clean stop.** The port is still the first bare
  number. SIGINT/SIGTERM end the loop (upstream's never ended), so the relay can
  close its sessions and write their ledger lines.

## The protocol

Text messages over ENET, `HEADER:content`. Defined by `common/Message.cpp`'s
`headers` map; `src/mm_client.c` mirrors it.

Client -> server, on connect, in this order:

```
VERSION:<gameID>
INFO:<ip:port,ip:port,...>     public + local candidates, for NAT traversal
LOGIN:<userName>:<userData>
```

then `CREATE:<blob>`, `JOIN:<owner>`, `APPROVE:<player>`, `EJECT:<player>`,
`LEAVE:`, `START:`.

Server -> client: `LOGIN:` `CREATE:` `LEAVE:` `EJECT:` as acks,
`PLAYERSACTIVE:<payload>` and `GAMESOPEN:<payload>` as list updates,
`GAMEINFO:<owner/joined/requested>`, `JOIN:<name>` (yourself = your request was
accepted; anyone else = they want to join **your** game), `APPROVE:<name>`, and
`START:<playerNumber>,<peerName>,<addr>,<addr>,...`.

**It is a matchmaker, not a game server** — unless it is run with `--relay`.
`START:` hands each side the other's candidate addresses and the server is then out
of the loop — the match runs peer-to-peer over GGPO (docs/NETPLAY.md). Killing the
server mid-match does not end the match; it only stops new ones being made. A
RELAYED match is the exception: its packets go through the server, so it ends with
it.

`RELAY:<token as 16 hex digits>,<playerNumber>` (server -> client, ours, only to
`OPENSWOS` clients, just before `START:`) says the match is to be played through
the server; the client then registers there with that token (a 16-byte `SWRL`
datagram, `include/relay_proto.h`) from the socket it plays on.

To update: re-copy from upstream, re-apply the changes listed above, rebuild
(`scripts/run_matchmaker.sh --rebuild`), and re-run
`bin/SWOS.x86_64 --matchmaker-selftest` against it.
