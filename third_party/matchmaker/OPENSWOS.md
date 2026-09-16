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

**Not built, kept to read:** `ServerConnection/` and `P2PConnection/` are the
upstream C++ client. Our client is `src/mm_client.c`, in C, because it has to
build for every SDL target; the upstream one is C++ and Windows-leaning
(`Sleep()`, `conio.h` — it does not compile on Linux as shipped). They are
vendored anyway because they are the **authority on the protocol**: when the
server does something our client does not expect, `ServerConnection.cpp` says
what the reference client would have done.

## Changes from upstream

Keep this list short and justified — it is a fork, and every entry is drift.

- **`Server/main.cpp`: the port argument works.** Upstream's README says "run it
  with no arguments and it will run on default port 19601. Pass a different port
  as an optional argument", but `main.cpp` hardcoded **19602** and ignored
  `argv`. We honour the argument and kept **19602** as the default, so a stock
  swos-2020 client still finds us. Also flushes the startup line, so a harness
  reading its stdout sees "running on port N" immediately rather than when the
  buffer happens to fill.
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
