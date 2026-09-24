#!/usr/bin/env bash
#
# wasm_netplay_test.sh — the BROWSER build plays live matches: through the matchmaker's
# WebSocket gate and its relay, against another browser or against a native build.
#
#   scripts/wasm_netplay_test.sh                  # browser v browser, 1 match of 300 frames
#   PAIR=web-native scripts/wasm_netplay_test.sh  # the host in a browser, the joiner native
#   PAIR=native-web scripts/wasm_netplay_test.sh  # ... and the other way round
#   MATCHES=2 FRAMES=600 scripts/wasm_netplay_test.sh    # a rematch too
#   NETEM=wifi scripts/wasm_netplay_test.sh       # the server disturbs the relayed link
#   PAIR=web-mac scripts/wasm_netplay_test.sh     # the joiner is the MAC's native build
#   PAIR=mac-web ...                              # (MAC_HOST, MAC_PATH; over the LAN)
#   PAIR=macweb-web ...                           # a browser ON THE MAC v one here
#   MM_HOST=us.swos.eu scripts/wasm_netplay_test.sh   # through an EXISTING server
#
# MM_HOST=<name> [MM_PORT=19602] plays through a server someone else runs (it must run
# --websocket, and relays any match with a browser in it). No server is started, and the
# two checks that read the server's own log -- that it forwarded, and one address per
# browser tunnel -- are replaced by what the players can see: each registered with the
# relay. The players get names of their own and SWOS_MM_AUTO_PEER, so on a public server
# they meet only each other. WEBDIR can point at a netplay build kept apart from the
# release one: WEBDIR=dist/web-netplay after `make WASM=1 NETPLAY=1 OUTDIR=dist/web-netplay`.
#
# PAIR is <host>-<join>, each one of: web (headless Chrome here), native (bin/$BIN here),
# mac (the Mac's native build), macweb (headless Chrome on the Mac: MAC_CHROME, a Chrome
# for Testing chrome-headless-shell, serving a copy of the web build from its own
# localhost and reaching this machine's gate over the LAN -- TCP and UDP 19602).
#
# A browser has no UDP, so the web build's datagrams travel as WebSocket messages to
# the server (Emscripten's socket layer), and the server's gate hands each browser
# socket a UDP address of its own on the server machine (third_party/matchmaker/Server/
# WebGate.cpp, docs/NETPLAY.md §8). This goes through what a player does -- login,
# create, join, START, the lobby, the match -- in headless Chrome, driven by the web
# build's query-string hooks (wasm/pre.js) instead of a command line, and checks:
#
#   * both players finished MATCHES matches, on the same final SIM-tier hash;
#   * the server relayed them (a browser can reach nothing else);
#   * each browser's match link and then its GGPO socket, bound to one port, reached
#     the relay from ONE address -- the tunnel outlived the link's WebSocket;
#   * with a native player too: that the two builds play one match, bit for bit,
#     across wasm32 and x86-64.
#
# Needs the web build WITH netplay (`make WASM=1 NETPLAY=1`, into dist/web), built by
# the same rules as the native binary when one plays: a test build (SWOS_TEST_NO_
# INJURIES_CARDS=1) and a plain one refuse each other at the handshake, which is the
# handshake working, not this test. Headless Chrome: CHROME=<path>, else Playwright's
# chrome-headless-shell, else chromium / google-chrome on PATH.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1
PAIR="${PAIR:-web-web}"
BIN="${BIN:-SWOS.x86_64}"
WEBDIR="${WEBDIR:-dist/web}"
# A Mac player reaches this machine's server over the LAN, and UDP 19602 is the port
# open here (docs/NETPLAY.md); a local pair can use any.
case "$PAIR" in *mac*) PORT="${PORT:-19602}" ;; *) PORT="${PORT:-19613}" ;; esac
MAC_HOST="${MAC_HOST:-bbaker@192.168.1.37}"
MAC_PATH="${MAC_PATH:-Programming/OpenSWOS}"
MAC_BIN="${MAC_BIN:-SWOS.macos}"
MAC_CHROME="${MAC_CHROME:-/tmp/swos-chrome/chrome-headless-shell-mac-arm64/chrome-headless-shell}"
MAC_WEB="${MAC_WEB:-/tmp/swos-web}"
MAC_HTTP_PORT="${MAC_HTTP_PORT:-18124}"
# The address the Mac reaches us on: whichever the route to it leaves from. (The first
# private address on an interface is wrong under WSL's mirrored networking, which adds
# a 10.255.255.254 of its own.)
LAN_IP="${LAN_IP:-$(ip -4 route get "${MAC_HOST#*@}" 2>/dev/null | grep -oE 'src [0-9.]+' | cut -d' ' -f2)}"
HTTP_PORT="${HTTP_PORT:-18123}"
MATCHES="${MATCHES:-1}"
FRAMES="${FRAMES:-300}"
WAIT="${WAIT:-$((150 + MATCHES * 90))}"
NETEM="${NETEM:-}"
SERVER="${SERVER:-build/matchmaker/swos-matchmaker}"
MM_HOST="${MM_HOST:-}"                  # an existing server to play through instead of our own
MM_PORT="${MM_PORT:-19602}"
REMOTE=0; [ -n "$MM_HOST" ] && REMOTE=1
OUT="${OUT:-${TMPDIR:-/tmp}/swos-wasm-netplay.$$}"
mkdir -p "$OUT" && OUT="$(cd "$OUT" && pwd)"

HOST_KIND=${PAIR%-*}
JOIN_KIND=${PAIR#*-}
for k in "$HOST_KIND" "$JOIN_KIND"; do
    case "$k" in web|native|mac|macweb) ;;
        *) echo "wasm-netplay-test: PAIR is <host>-<join>, each web, native, mac or macweb" >&2; exit 1 ;;
    esac
done
case "$PAIR" in *web*) ;;
    *) echo "wasm-netplay-test: PAIR needs a browser in it (web or macweb)" >&2; exit 1 ;;
esac

if [ -z "${CHROME:-}" ]; then
    for c in ~/.cache/ms-playwright/chromium_headless_shell-*/chrome-headless-shell-linux64/chrome-headless-shell \
             "$(command -v chromium 2>/dev/null)" "$(command -v chromium-browser 2>/dev/null)" \
             "$(command -v google-chrome 2>/dev/null)"; do
        [ -n "$c" ] && [ -x "$c" ] && { CHROME="$c"; break; }
    done
fi
[ -n "${CHROME:-}" ] || { echo "wasm-netplay-test: no headless Chrome (set CHROME=)" >&2; exit 1; }
[ -f "$WEBDIR/SWOS.wasm" ] || { echo "wasm-netplay-test: no web build in $WEBDIR (make WASM=1 NETPLAY=1)" >&2; exit 1; }
grep -aq "OPENSWOS-WEB" "$WEBDIR/SWOS.wasm" ||
    { echo "wasm-netplay-test: $WEBDIR was built without netplay (make WASM=1 NETPLAY=1)" >&2; exit 1; }
[ "$REMOTE" = 1 ] || [ -x "$SERVER" ] || { echo "wasm-netplay-test: $SERVER not built" >&2; exit 1; }
HN=HOST; JN=JOIN
if [ "$REMOTE" = 1 ]; then
    PLAY_HOST=$MM_HOST; PLAY_PORT=$MM_PORT
    MAC_MM_HOST=$MM_HOST                  # the Mac goes to the same server, not to us
    n=$(( $$ % 10000 )); HN="WHOST$n"; JN="WJOIN$n"   # never two runs' players mixed
    [ -n "$NETEM" ] && echo "wasm-netplay-test: NETEM is the server's to choose; ignored for $MM_HOST" >&2
    echo "wasm-netplay-test: through $MM_HOST:$MM_PORT as $HN and $JN"
else
    PLAY_HOST=127.0.0.1; PLAY_PORT=$PORT
    MAC_MM_HOST=$LAN_IP
fi
case "$PAIR" in *native*)
    [ -x "bin/$BIN" ] || { echo "wasm-netplay-test: bin/$BIN not built" >&2; exit 1; } ;;
esac
case "$PAIR" in *mac*)
    [ -n "$LAN_IP" ] || { echo "wasm-netplay-test: no LAN address for the Mac to reach (set LAN_IP=)" >&2; exit 1; }
    ssh -o BatchMode=yes -o ConnectTimeout=10 "$MAC_HOST" true 2>/dev/null ||
        { echo "wasm-netplay-test: cannot ssh to $MAC_HOST" >&2; exit 1; } ;;
esac
case " $HOST_KIND $JOIN_KIND " in *" mac "*)
    echo "mac: $(ssh "$MAC_HOST" "cd $MAC_PATH/bin && ./$MAC_BIN --version 2>&1 | tail -1"), to $LAN_IP:$PORT" ;;
esac
case " $HOST_KIND $JOIN_KIND " in *" macweb "*)
    ssh "$MAC_HOST" "test -x $MAC_CHROME" || {
        echo "wasm-netplay-test: no headless Chrome on the Mac at $MAC_CHROME -- Chrome for Testing's" >&2
        echo "  chrome-headless-shell for mac-arm64, unzipped (see docs/NETPLAY.md §8)" >&2; exit 1; }
    # The web build, served from the Mac's own localhost: only the gate's port has to
    # be open on this machine. Copied only when it changed.
    ssh "$MAC_HOST" "mkdir -p $MAC_WEB"
    for f in SWOS.html SWOS.js SWOS.wasm SWOS.data; do
        here=$(md5sum "$WEBDIR/$f" | cut -d' ' -f1)
        there=$(ssh "$MAC_HOST" "md5 -q $MAC_WEB/$f 2>/dev/null")
        [ "$here" = "$there" ] || scp -q "$WEBDIR/$f" "$MAC_HOST:$MAC_WEB/$f" ||
            { echo "wasm-netplay-test: could not copy $f to the Mac" >&2; exit 1; }
    done
    MAC_HTTP_PID=$(ssh "$MAC_HOST" "cd $MAC_WEB && nohup python3 -m http.server $MAC_HTTP_PORT --bind 127.0.0.1 \
        > /tmp/swos-web-http.log 2>&1 & echo \$!")
    echo "macweb: $(ssh "$MAC_HOST" "$MAC_CHROME --version"), serving $MAC_WEB, to $LAN_IP:$PORT" ;;
esac

PIDS=()
cleanup() {
    for p in "${PIDS[@]}"; do kill -9 "$p" 2>/dev/null; done
    [ -n "${SERVER_PID:-}" ] && kill -INT "$SERVER_PID" 2>/dev/null
    [ -n "${HTTP_PID:-}" ] && kill "$HTTP_PID" 2>/dev/null
    if [ -n "${MAC_HTTP_PID:-}" ]; then
        # [e]: a pattern that cannot match the shell running it
        ssh "$MAC_HOST" "kill $MAC_HTTP_PID 2>/dev/null; pkill -f 'user-data-dir=/tmp/swos-wasm-test-chrom[e]-'" 2>/dev/null
    fi
    wait 2>/dev/null
}
trap cleanup EXIT

python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 --directory "$WEBDIR" > "$OUT/http.log" 2>&1 &
HTTP_PID=$!
if [ "$REMOTE" = 0 ]; then
    SERVER_ARGS=(--websocket)
    [ -n "$NETEM" ] && SERVER_ARGS+=(--relay --netem "$NETEM")
    "$SERVER" "$PORT" "${SERVER_ARGS[@]}" --ledger "$OUT/ledger.jsonl" > "$OUT/server.log" 2>&1 &
    SERVER_PID=$!
fi
sleep 1

# What both players are told, as environment: the server, the lobby's choices, the
# match's length. Each side's input comes from its own generator, so both predict
# wrong and roll back, and the relay carries real rollback traffic.
common_env=(SWOS_MM_HOST="$PLAY_HOST" SWOS_MM_PORT="$PLAY_PORT" SWOS_P2P_LOG=1 SWOS_NO_LOG=1
            SWOS_LOBBY_READY=1 SWOS_LOBBY_MATCHES="$MATCHES" SWOS_LIVE_FRAMES="$FRAMES")

# A browser player: the page, with its command line and environment in the query
# string (wasm/pre.js). Its console is Chrome's log; <name>.log is just the game's lines.
web_player() {   # name auto env...
    local name=$1 auto=$2; shift 2
    local url="http://127.0.0.1:$HTTP_PORT/SWOS.html?arg=--matchmaker-menu=$name&env=SWOS_MM_AUTO=$auto"
    local kv
    for kv in "${common_env[@]}" "$@"; do url="$url&env=$kv"; done
    "$CHROME" --no-sandbox --enable-logging=stderr --v=0 --user-data-dir="$OUT/chrome-$name" \
        --autoplay-policy=no-user-gesture-required --disable-background-timer-throttling \
        --disable-renderer-backgrounding --disable-backgrounding-occluded-windows \
        "$url" > "$OUT/$name.raw" 2>&1 &
    PIDS+=($!)
    disown $!           # killed at the end: no "Killed" job notice
}
native_player() {   # name auto env...
    local name=$1 auto=$2; shift 2
    ( cd bin && env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy HOME="$OUT/home-$name" \
        SWOS_MM_AUTO="$auto" "${common_env[@]}" "$@" \
        timeout -s KILL "$WAIT" ./"$BIN" --matchmaker-menu="$name" > "$OUT/$name.log" 2>&1 ) &
    PIDS+=($!)
    disown $!           # killed at the end: no "Killed" job notice
}
# The Mac's native build, over ssh, pointed at this machine's LAN address. A watchdog
# instead of timeout(1), which macOS does not have; a HOME of its own, so the test
# leaves the Mac's own saves and SWOS.ini alone.
mac_player() {   # name auto env...
    local name=$1 auto=$2; shift 2
    local kv envs=""
    for kv in "${common_env[@]}" "$@"; do
        case "$kv" in SWOS_MM_HOST=*) kv="SWOS_MM_HOST=$MAC_MM_HOST" ;; esac
        envs="$envs $(printf %q "$kv")"
    done
    ssh "$MAC_HOST" "cd $MAC_PATH/bin && rm -rf /tmp/swos-wasm-test-$name && mkdir -p /tmp/swos-wasm-test-$name &&
        env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy HOME=/tmp/swos-wasm-test-$name SWOS_MM_AUTO=$auto $envs \
        ./$MAC_BIN --matchmaker-menu=$name 2>&1 & p=\$!; ( sleep $WAIT; kill -9 \$p 2>/dev/null ) & w=\$!;
        wait \$p; kill \$w 2>/dev/null" > "$OUT/$name.log" 2>&1 &
    PIDS+=($!)
    disown $!           # killed at the end: no "Killed" job notice
}
# A browser on the Mac: headless Chrome there, the page from its own localhost, the
# gate at this machine's LAN address. Its console comes back over ssh.
macweb_player() {   # name auto env...
    local name=$1 auto=$2; shift 2
    local url="http://127.0.0.1:$MAC_HTTP_PORT/SWOS.html?arg=--matchmaker-menu=$name&env=SWOS_MM_AUTO=$auto"
    local kv
    for kv in "${common_env[@]}" "$@"; do
        case "$kv" in SWOS_MM_HOST=*) kv="SWOS_MM_HOST=$MAC_MM_HOST" ;; esac
        url="$url&env=$kv"
    done
    ssh "$MAC_HOST" "rm -rf /tmp/swos-wasm-test-chrome-$name;
        $MAC_CHROME --enable-logging=stderr --v=0 --user-data-dir=/tmp/swos-wasm-test-chrome-$name \
        --autoplay-policy=no-user-gesture-required --disable-background-timer-throttling \
        --disable-renderer-backgrounding --disable-backgrounding-occluded-windows $(printf %q "$url") 2>&1 &
        p=\$!; ( sleep $WAIT; kill -9 \$p 2>/dev/null ) & w=\$!; wait \$p; kill \$w 2>/dev/null" > "$OUT/$name.raw" 2>&1 &
    PIDS+=($!)
    disown $!           # killed at the end: no "Killed" job notice
}
player() {   # kind name auto env...
    local kind=$1; shift
    case "$kind" in
        web)    web_player "$@" ;;
        macweb) macweb_player "$@" ;;
        mac)    mac_player "$@" ;;
        *)      native_player "$@" ;;
    esac
}
log_of() {   # name kind -> the game's lines
    if [ "$2" = web ] || [ "$2" = macweb ]; then
        grep -oE 'CONSOLE[^"]*"[^"]*"' "$OUT/$1.raw" 2>/dev/null | sed -E 's/^CONSOLE:[0-9]+\] "//; s/"$//'
    else
        cat "$OUT/$1.log" 2>/dev/null
    fi
}

mkdir -p "$OUT/home-$HN" "$OUT/home-$JN"
player "$HOST_KIND" "$HN" host SWOS_MM_AUTO_PEER="$JN" SWOS_LOBBY_TEAM=50:0B SWOS_LOBBY_START=1 SWOS_LIVE_RANDOM_INPUT=0xA5A5F00D
sleep 3
player "$JOIN_KIND" "$JN" join SWOS_MM_AUTO_PEER="$HN" SWOS_LOBBY_TEAM=52:02 SWOS_LIVE_RANDOM_INPUT=0x5A5A0FF0

# A browser does not end: the game leaves its lobby after MATCHES matches and the page
# stays. So wait for that line (or a crash) from each, then close the browsers.
done_line='\[lobby\] [0-9]+ match(es)? played; leaving'
crash='Aborted\(|RuntimeError|program exited \(with status: [1-9]'
deadline=$((SECONDS + WAIT))
while [ $SECONDS -lt $deadline ]; do
    h=$(log_of "$HN" "$HOST_KIND" | grep -cE "$done_line|$crash")
    j=$(log_of "$JN" "$JOIN_KIND" | grep -cE "$done_line|$crash")
    [ "$h" -gt 0 ] && [ "$j" -gt 0 ] && break
    sleep 2
done
sleep 2
for s in "$HN:$HOST_KIND" "$JN:$JOIN_KIND"; do log_of "${s%%:*}" "${s#*:}" > "$OUT/${s%%:*}.game.log"; done

fail=0
for s in "$HN" "$JN"; do
    grep -qE "$done_line" "$OUT/$s.game.log" || { echo "!! $s never left its lobby"; fail=1; }
    grep -E "$crash|DIFFERENT RULES|link failed" "$OUT/$s.game.log" | head -3 | sed "s/^/!! $s: /"
done
hh=$(grep -oE "final sim hash [0-9a-f]+" "$OUT/$HN.game.log" | awk '{print $4}' | paste -sd' ')
jh=$(grep -oE "final sim hash [0-9a-f]+" "$OUT/$JN.game.log" | awk '{print $4}' | paste -sd' ')
hn=$(grep -c "live match finished" "$OUT/$HN.game.log")
jn=$(grep -c "live match finished" "$OUT/$JN.game.log")
echo "host ($HOST_KIND): $hn finished, hashes $hh"
echo "join ($JOIN_KIND): $jn finished, hashes $jh"
[ "$hn" = "$MATCHES" ] && [ "$jn" = "$MATCHES" ] && [ -n "$hh" ] && [ "$hh" = "$jh" ] || fail=1

if [ "$REMOTE" = 1 ]; then
    # The server's log is on its machine. What the players can show instead: each one
    # registered with the relay -- which a browser must, since it can reach nothing else.
    for s in "$HN" "$JN"; do
        if grep -q "^\[p2p\] relay: registered" "$OUT/$s.game.log"; then
            echo "$s: $(grep -m1 '^\[p2p\] relay: registered' "$OUT/$s.game.log")"
        else
            echo "!! $s never registered with the relay"; fail=1
        fi
    done
    echo "(the server's forwarding and tunnel checks are on $MM_HOST; not checked from here)"
else
# Relayed: one session (a rematch stays in it), forwarding between the two.
fw=$(grep -c "both players here: forwarding" "$OUT/server.log")
[ "$fw" -ge 1 ] || { echo "!! the relay never forwarded between the players"; fail=1; }
# One address per browser socket: every WebSocket that named a port got the same
# tunnel as the last one that named it (the link's, then GGPO's).
if grep -q "browser socket :" "$OUT/server.log"; then
    python3 - "$OUT/server.log" <<'PYEOF' || fail=1
import re, sys
seen, bad, reused = {}, 0, 0
for line in open(sys.argv[1]):
    m = re.search(r"\[web\] ([\d.]+):\d+: browser socket :(\d+) is ([\d.]+:\d+)", line)
    if not m:
        continue
    key = (m.group(1), m.group(2))
    if key in seen:
        reused += 1
        if seen[key] != m.group(3):
            bad += 1
            print("!! browser socket :%s was %s, then %s" % (m.group(2), seen[key], m.group(3)))
    seen[key] = m.group(3)
print("tunnels: %d browser port(s), %d reused by the socket that followed, %d moved" % (len(seen), reused, bad))
sys.exit(1 if bad or not reused else 0)
PYEOF
fi
fi

what="$MATCHES match(es) $HOST_KIND v $JOIN_KIND through the WebSocket gate${NETEM:+ ($NETEM)}"
[ "$REMOTE" = 1 ] && what="$MATCHES match(es) $HOST_KIND v $JOIN_KIND through $MM_HOST's gate and relay"
if [ "$fail" = 0 ]; then
    echo "wasm-netplay-test: PASSED — $what, both players identical (logs in $OUT)"
    exit 0
fi
echo "wasm-netplay-test: FAILED — $what (logs in $OUT)"
exit 1
