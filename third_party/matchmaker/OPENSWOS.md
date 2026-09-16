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

**It is a matchmaker, not a game server.** `START:` hands each side the other's
candidate addresses and the server is then out of the loop — the match runs
peer-to-peer over GGPO (docs/NETPLAY.md). Killing the server mid-match does not
end the match; it only stops new ones being made.

To update: re-copy from upstream, re-apply the changes listed above, rebuild
(`scripts/run_matchmaker.sh --rebuild`), and re-run
`bin/SWOS.x86_64 --matchmaker-selftest` against it.
