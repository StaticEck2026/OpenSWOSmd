# NETPLAY_SETUP.md — running a server and playing online

How to deploy the matchmaking/relay server, point games at it, and get two players into
a match — natively and in a web browser. This is the operator's runbook.
**docs/NETPLAY.md is the design**: why each piece works the way it does, and what every
test proves. Where this file says *why* in one line, NETPLAY.md has the section.

## 1. What talks to what

The server is SimpleMatchmaker — the same project SWOS 2020 ships — vendored at
`third_party/matchmaker`, with OpenSWOS's additions recorded in its `OPENSWOS.md`.
It does up to four jobs, each switched on by a flag:

| job | flag | what it does |
|---|---|---|
| **introduce** | *(always)* | players log in, see each other, host and join. At `START:` it hands each side the other's addresses and **drops out** — the match is peer to peer |
| **relay** | `--relay` | plays the match *through* the server instead. For players whose NATs will not punch, and for testing |
| **browser gate** | `--websocket` | lets the web build connect. A browser has no UDP, so every browser match is relayed, `--relay` or not |
| **recorder** | `--record` | players stream their matches to it; it compares both sides frame by frame, names the first desync, and keeps the match as a `.SRP` |

```
  native player A ──UDP 19602──┐                     ┌──UDP 19602── native player B
                               ├── swos-matchmaker ──┤
  browser player ──TCP 19602───┘  (introduce/relay)  └── ...
     (WebSocket)

  introduce only : A ◄──────────── peer to peer ────────────► B   (server not involved)
  --relay        : A ◄──► server ◄──► B                          (every packet via server)
```

**Killing the server does not end a peer-to-peer match** — it only stops new ones being
made. A relayed match does end with it.

## 2. Ports

Everything is on **one port number, 19602**, split by protocol:

| port | protocol | needed when | carries |
|---|---|---|---|
| 19602 | **UDP** | **always** | matchmaking (ENet), the relay, relayed native matches |
| 19602 | **TCP** | only with `--websocket` or `--record` | browsers' WebSockets, and players' recorder streams |

The relay shares UDP 19602 with the matchmaker through ENet's `intercept` hook, so relaying
needs no second port. The TCP side is sniffed per connection: one opening with `GET ` is a
browser's WebSocket, one opening with `SWRC` is a recorder stream.

**TCP and UDP cannot be given different numbers.** The server takes one `port` argument and
binds both to it, because the web build sends its "datagrams" (WebSocket messages) to the
same port number a native build would send UDP to. This is what makes `wss://` hard — §10.

## 3. Build the server

```bash
scripts/run_matchmaker.sh --build-only      # -> build/matchmaker/swos-matchmaker
scripts/run_matchmaker.sh --rebuild         # wipe build/matchmaker and reconfigure first
```

Needs `cmake` and a C++ compiler. ENet is vendored (`third_party/enet`), shared with the
client, so there is nothing else to install. It builds and runs on Linux, including WSL2.

`run_matchmaker.sh` without `--build-only` builds if needed and runs a plain server on UDP
19602 — fine for local testing. It takes only `--port`, `--rebuild` and `--build-only`; it
does **not** pass on `--relay`, `--websocket` or `--record`. For a real deployment, build
with the script and **run the binary directly** (§4).

## 4. Run it

The configuration for a public server that serves everyone, browsers included:

```bash
cd /srv/swos-matchmaker        # the working directory matters: see --recordings
build/matchmaker/swos-matchmaker 19602 \
    --relay --websocket --record \
    --ledger matches.jsonl --recordings recordings/
```

| flag | use it for |
|---|---|
| `19602` | the port, UDP and TCP. First bare argument; default 19602 |
| `--relay` | relay every OpenSWOS pair. Without it, only browser matches are relayed |
| `--websocket` | accept browsers. Opens TCP on the port |
| `--record` | accept match streams from players who set `[netplay] record = server`. Opens TCP on the port |
| `--ledger FILE` | append one JSON line per relay session, recorded match and desync |
| `--recordings DIR` | where recorded `.SRP`s go. **Default `recordings/`, relative to the working directory** |
| `--netem P` | **testing only.** Puts a misbehaving network (`wifi`, `awful`, `cycle`...) in the middle of every relayed match. Never on a server real players use |

Drop `--relay` if you would rather players connect directly and only lean on the server to
find each other: it costs the server nothing, and a match survives the server restarting.
Keep it if your players are likely to be behind NATs that will not punch (many home
routers, most mobile connections).

### Keep it running

For a quick session, `tmux new -s mm '<the command above>'`. For a permanent server, a
systemd unit. This is a template — adjust the user and paths:

```ini
# /etc/systemd/system/swos-matchmaker.service
[Unit]
Description=OpenSWOS matchmaking / relay server
After=network-online.target
Wants=network-online.target

[Service]
User=swos
WorkingDirectory=/srv/swos-matchmaker
ExecStart=/srv/swos-matchmaker/build/matchmaker/swos-matchmaker 19602 --relay --websocket --record --ledger matches.jsonl --recordings recordings/
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now swos-matchmaker
journalctl -u swos-matchmaker -f       # every [relay] line is timed to the millisecond
```

The server keeps no state worth losing: it introduces players and forgets them, so a
restart interrupts only relayed matches and matches being introduced at that moment.

## 5. Open the ports

A player has to reach the server's UDP 19602 (and TCP 19602 for browsers and recording)
from the internet. There are up to three layers, and **each one has to be opened**.

**The machine's own firewall.**

```bash
# Linux, ufw
sudo ufw allow 19602/udp
sudo ufw allow 19602/tcp                        # only for --websocket / --record

# Linux, firewalld
sudo firewall-cmd --permanent --add-port=19602/udp
sudo firewall-cmd --permanent --add-port=19602/tcp
sudo firewall-cmd --reload
```

**Windows running the server in WSL2.** With WSL2 in **mirrored** networking mode
(`networkingMode=mirrored` in `%USERPROFILE%\.wslconfig`) a WSL listener shares the Windows
host's addresses — no `netsh portproxy` (which is TCP-only and could not carry UDP anyway).
Mirrored mode sits behind the **Hyper-V firewall**, so the rules go there, from an elevated
PowerShell, once:

```powershell
New-NetFirewallHyperVRule -Name SWOS-MM -DisplayName "SWOS matchmaker (UDP)" `
    -Direction Inbound -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' `
    -Protocol UDP -LocalPorts 19602 -Action Allow

# Only for --websocket / --record. docs/NETPLAY.md's rule is UDP-only; browsers need this too.
New-NetFirewallHyperVRule -Name SWOS-MM-TCP -DisplayName "SWOS matchmaker (TCP)" `
    -Direction Inbound -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' `
    -Protocol TCP -LocalPorts 19602 -Action Allow
```

**The router**, for a server at home: forward **UDP 19602** — and **TCP 19602** for
browsers and recording — to the server machine's LAN address.

**A cloud VPS**: the provider's security group / network firewall, same two rules.

### The address to hand out

Players need the server's **public** address or a DNS name for it:

- A VPS has a fixed public IP; give it a DNS name if you can (and you need one for §10).
- A home connection has a public IP that can change. Use a **dynamic-DNS** name pointed at
  it, or hand out the current IP and expect to update it.
- **`baker76.com` is the wrong name.** It resolves to the GoDaddy shared host the downloads
  live on, not to whatever machine runs the server.

## 6. Prove it before handing it out

From **another machine**, against the **public** address:

```bash
SWOS_MM_HOST=<public address> bin/SWOS.x86_64 --matchmaker-selftest
```

That is a real protocol handshake — it logs in, lists the lobby and leaves — not a ping.
Two things that look like failures and are not:

- **Test the public address even from inside the same house.** Measured on this setup: a
  machine on the same LAN reached the server through the public address and the port
  forward, but **not** through the server's LAN address — same host, same port, same rule.
  So a failure against the LAN address does not mean it is broken, and a success against it
  would not have proved the forward. The public address is what a player uses.
- **There is nothing TCP to test on the UDP side.** A TCP tool reporting "connection
  refused" against UDP 19602 means nothing. (TCP 19602 is only there with `--websocket` or
  `--record`, and speaks WebSocket or `SWRC`, not anything a port scanner understands.)

**The same check from the game**, which is what a player will see: on another machine,
**MULTIPLAYER**, type the public address in **SERVER**, **LOGIN**. The matchmaking screen
should go from **CONNECTING TO SERVER** to showing who is online. **SERVER NOT CONNECTED**
means it was not reached.

Then the full rehearsal — one machine hosts, another joins, both through the public address:

```bash
SWOS_MM_HOST=<public> bin/SWOS.x86_64 --matchmaker-pair=host    # machine 1
SWOS_MM_HOST=<public> bin/SWOS.x86_64 --matchmaker-pair=join    # machine 2
```

Each side should end with `START:`, the right player number, the other's name, and a list
of candidate addresses. Two machines behind the **same** router still reach the server by
hairpin, so this proves the forward and the server — not yet that the internet side accepts
traffic. The first real outside player proves that.

## 7. Point the games at the server

**Players do this in the game, once.** Out of the box a game looks for a server on its own
machine. To point it at yours:

1. Main menu → **MULTIPLAYER**. This opens the **PLAY ONLINE** screen.
2. **NAME** — the name other players will see.
3. **SERVER** — type the server's address: `your.server.name`, or `your.server.name:port`
   if it is not on 19602. Until something is typed it reads **THIS MACHINE**.
4. **LOGIN**.

The SERVER row is **saved to `SWOS.ini` as soon as it is typed**, so a player enters it once
and never again — the next time they open MULTIPLAYER it is already filled in. Editing it
starts from the address already in force, so changing one digit is one digit.

That is the only thing a player has to be told: *"MULTIPLAYER, then type
`your.server.name` in the SERVER row."*

After LOGIN the matchmaking screen shows how the connection is going — **CONNECTING TO
SERVER**, then the players and games online, or **SERVER NOT CONNECTED** if it cannot reach
it (§11).

**Recording matches** is in the menu too. After LOGIN, open **SETTINGS** on the matchmaking
screen: **MULTIPLAYER SETTINGS → RECORD MATCHES**, OFF or ON. ON streams every match to the
server's recorder (§4's `--record`), which keeps it and compares both players frame by frame
to catch a desync. Like the SERVER row it is saved as soon as it is changed, and it applies
from the very next match — no restart.

ON means the server the player logged in to. A player who names a recorder somewhere else
in `SWOS.ini` (`[netplay] record = host[:port]`) sees that address in the row instead of ON,
and switching the row off and back on restores it rather than losing it.

The other rows on MULTIPLAYER SETTINGS — SOUND FX, COMMENTARY, the chat options and the
buttons under them — are SWOS 2020's and do nothing in OpenSWOS yet.

> **For operators and scripts only** — never something to ask a player to do. The SERVER
> row *is* `[netplay] matchmaker` in `SWOS.ini`: typing one writes the other. The only thing
> that overrides it is the environment, `SWOS_MM_HOST` / `SWOS_MM_PORT`, which the repo's
> tests use. **That override is invisible in the menu**: the SERVER row still shows the
> `SWOS.ini` address while LOGIN connects to the environment's. So a player who "typed the
> right server and still can't connect" may simply have one of those left set.

## 8. Two players, one match

1. Both players open **MULTIPLAYER**, set the same **SERVER** (§7) and **LOGIN**.
2. One **hosts** a game. It appears in the other player's lobby.
3. The other **joins**. The host is asked to **accept**.
4. The server sends `START:`. With `--relay` the players both connect through the server;
   otherwise they punch straight to each other.
5. The two games **handshake**, agree an input delay from the ping they both measure, and
   go to the pre-match lobby. From there it is a normal match, and after it a rematch goes
   back through the same lobby without logging in again.

**The handshake refuses to start a match the two games would play differently** — and
this is the most common reason a match will not start. Both players need:

- **the same build** — same commit. The version on the main menu's bottom line, the window
  title and `--version` must match;
- **the same rules** — a **tester build** (`SWOS_TEST_NO_INJURIES_CARDS=1`, "test build: no
  injuries or cards" on its version line) cannot play a **release** build;
- **the same gameplay style** — AMIGA or DOS, in OPTIONS;
- **the same screen width.**

What each refusal means is in §11.

## 9. Playing in a web browser

The web build can play online — against another browser, or against a native Linux,
Windows or macOS player. It is proven bit for bit: the same match ends on the same state
hash in a browser as natively.

**1. Build the web build with netplay.** Netplay is **opt-in** for WASM:

```bash
make WASM=1 NETPLAY=1                                   # -> dist/web/SWOS.html (+ .js .wasm .data)
make WASM=1 NETPLAY=1 SWOS_TEST_NO_INJURIES_CARDS=1     # to play the TESTER natives (§8 rules)
```

Match the native builds' rules, or the handshake refuses them (§8).

**2. Run the server with `--websocket`.** A browser can only play through the server, and a
server with `--websocket` relays any match with a browser in it, whether or not it has
`--relay`. Open **TCP** 19602 as well as UDP (§5).

**3. Serve the web build over plain `http://`.** This is the one that bites:

```bash
cd dist/web && python3 -m http.server 8080      # then http://<host>:8080/SWOS.html
```

The browser connects to the server with `ws://` from an `http://` page. **From an
`https://` page it can only use `wss://`**, and the server does not speak `wss://` yet —
§10. Any static web server will do; it does not have to be the machine the matchmaker runs
on, because the browser dials the matchmaker directly.

**4. Point it at the server** — exactly as natively: **MULTIPLAYER**, type the server in
the **SERVER** row, **LOGIN** (§7). The browser keeps it in its own storage, so it is typed
once per browser.

**5. Keep the tab in front.** A browser throttles a background tab's timers to about once a
second, and a live match does not survive that.

**The public build at baker76.com cannot play online today.** It is served over `https://`,
so it could only reach a `wss://` server, and it is built without `NETPLAY` for exactly that
reason — its MULTIPLAYER button would lead nowhere.

## 10. Certificates and `wss://` — **not done yet**

This is the missing piece for playing online from a public `https://` page. Here is what is
needed and why it is not a configuration change.

**Why a certificate at all.** A page served over `https://` may only open `wss://` (TLS)
WebSockets — browsers block `ws://` from it as mixed content. So the server's WebSocket
side needs TLS, with a certificate for **the name players connect to**. That means the
server needs a **DNS name** (certificates are issued for names; a Let's Encrypt certificate
from `certbot` is the obvious source).

**Why it cannot simply be put behind a TLS proxy**, the usual answer:

1. **The browser dials the same port the game would.** `wasm/pre.js` sets
   `Module.websocket.url` to the bare prefix `wss://` on an https page, and Emscripten
   completes it with the address *and port* each datagram is for — so the browser opens
   `wss://<server>:19602`.
2. **The server already owns TCP 19602, in plain text.** It takes one port and binds TCP on
   it (§2). A TLS proxy terminating on 19602 cannot share that port with it on the same
   machine.
3. **The server tells browsers apart by their address.** Each browser socket becomes a
   tunnel keyed by *the browser's address and a port it names*. Behind a proxy every browser
   arrives from the proxy's address. The web build picks a random port per socket, so they
   would rarely collide — but the server would no longer see real player addresses, which
   the relay and the ledger both report.

**The recommended fix is TLS inside the gate** (`TcpGate`/`WebGate` in
`third_party/matchmaker/Server`): it keeps the prefix URL working unchanged, keeps real
browser addresses, and stays one process on one port. It needs the certificate and key
paths as server arguments, and a reload when `certbot` renews.

**A quicker experiment, untested:** give `pre.js` a *full* URL instead of the prefix
(`wss://your.server.name/`, which Emscripten then uses for every socket) and put an ordinary
TLS proxy (Caddy, nginx) on 443 in front of the gate. The gate routes by the port a browser
*names* in its first message rather than the URL's, and a browser only ever talks to the
server, so this may work — but point 3 above still applies, and nothing has proved it.

Until one of those lands, browser multiplayer means an `http://` page (§9).

## 11. Troubleshooting

| what the player sees | what it means | fix |
|---|---|---|
| **SERVER NOT CONNECTED** (matchmaking screen) | the server was not reached | check the **SERVER** row (§7); the server's firewall and router forward (§5) |
| stuck on **CONNECTING TO SERVER** | nothing answering at that address | wrong address or port in **SERVER**, or UDP 19602 blocked (§5) |
| **THE HOST DID NOT ANSWER** / CHECK THE ADDRESS, PORT AND FIREWALL | the other player was never reached | usually NATs that will not punch: run the server with `--relay` (§4). Otherwise the firewall and forward (§5) |
| **THE OTHER PLAYER HAS ANOTHER VERSION** / BOTH OF YOU NEED THE SAME BUILD | different commits | both download the same build; compare the version line |
| **YOUR BUILDS PLAY BY DIFFERENT RULES** / ONE OF YOU HAS A TEST BUILD | tester build v release build | both use tester, or both release (§8) |
| **YOUR GAMEPLAY STYLES ARE DIFFERENT** / SET THE SAME ONE IN OPTIONS | AMIGA v DOS | OPTIONS → GAMEPLAY, the same on both |
| **YOUR SCREEN WIDTHS ARE DIFFERENT** / BOTH OF YOU NEED THE SAME ONE | different widths | set the same `[video]` width |
| **THE HOST ALREADY HAS A PLAYER** | someone else joined first | host another game |
| **THE SERVER'S RELAY HAS CLOSED** / FIND EACH OTHER AGAIN | a relay session closes after about a minute with no traffic | go back to the lobby and join again |
| **THIS SERVER CANNOT RELAY A BROWSER GAME** | a browser is in the match and the server lacks `--websocket` | restart the server with `--websocket` (§4) |
| **THE CONNECTION WENT QUIET** | the link stopped. In a browser, most often the tab went to the background | keep the tab in front (§9); otherwise the network dropped |
| two natives never connect, but the server is fine | their NATs will not punch | run the server with `--relay` (§4) |
| a browser cannot connect at all | an `https://` page trying `ws://` | serve the web build over `http://` (§9, §10) |
| selftest fails against the LAN address, works against the public one | expected — see §6 | use the public address |
| a TCP tool says "connection refused" on 19602 | meaningless — the service is UDP | use `--matchmaker-selftest` (§6) |

On the server, the console (or `journalctl`) shows every login, game, `START:` and relay
session, each `[relay]` line timed to the millisecond. With `--ledger`, each relay session,
recorded match and **desync** is a JSON line: a desync line names the exact frame, both
builds and both platforms. A recorded match can be played again anywhere with
`bin/SWOS.x86_64 --srp-resim=recordings/<match>.SRP`, which names the first frame that parts.

## 12. Testing a deployment with the repo's own tests

These drive the real menus with real binaries; run them after changing anything in the
path. NETPLAY.md says what each one proves.

```bash
scripts/netplay_lobby_test.sh                     # two instances, lobby, match, rematch
scripts/relay_test.sh                             # two players through the relay
NETEM=awful scripts/relay_test.sh                 #   ... under the worst emulated link
RECORD=1 scripts/relay_test.sh                    #   ... recorded; the .SRP must match byte for byte
scripts/wasm_netplay_test.sh                      # browser v browser, headless Chrome
PAIR=web-native scripts/wasm_netplay_test.sh      # browser v the native build
PAIR=mac-web scripts/wasm_netplay_test.sh         # the Mac's native build v a browser
```

## 13. Related

- **docs/NETPLAY.md** — the design: §6 the MULTIPLAYER menu and the handshake, §7
  matchmaking, the relay and the recorder, §8 the browser.
- **docs/REPLAY_FORMAT_V2.md** — the `.SRP` a recorder keeps, and `--srp-resim`.
- **`third_party/matchmaker/OPENSWOS.md`** — what OpenSWOS changed in the server.
- **`include/relay_proto.h`, `include/record_proto.h`** — the relay and recorder wire formats.
