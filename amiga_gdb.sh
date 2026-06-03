#!/usr/bin/env bash
#
# amiga_gdb.sh — reliable launch + attach orchestration for the SWOS Amiga
# build under FS-UAE's remote-debugger stub.
#
# The FS-UAE remote_debugger port is ONE-SHOT per emulator session: every
# gdb connect->disconnect burns it, and a connect issued before the SWOS2
# entry trigger has settled yields a DEGRADED attach (gdb says "not being
# run" / "No registers"; only ELF-static reads work). That made the manual
# launch+attach dance flaky. This script makes it deterministic by:
#
#   1. tearing down any stale FS-UAE / gdb / daemon (clean slate),
#   2. launching FS-UAE,
#   3. waiting for the "Breakpoint 20 triggered" SWOS2-entry line in the log
#      plus a short settle,
#   4. starting the gdb_proxy daemon,
#   5. HEALTH-CHECKING the attach (target stopped at SIGTRAP, PC readable),
#   6. and if the attach is degraded, doing a FULL restart and retrying
#      (up to ATTACH_RETRIES times) — the only real recovery for the
#      one-shot port.
#
# Usage:
#   amiga_gdb.sh up        # clean + launch + attach, retry until healthy
#   amiga_gdb.sh down      # tear everything down
#   amiga_gdb.sh status    # health verdict of the current attach
#   amiga_gdb.sh restart   # down + up
#
# After `up` returns SUCCESS, drive gdb with:
#   python3 <DIR>/gdb_proxy.py cmd "info registers"   (etc.)
#
# Config via env (sane defaults below).

set -u

# ---- config ---------------------------------------------------------------
SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GDB_PROXY="${GDB_PROXY:-$SELF_DIR/gdb_proxy.py}"

FSUAE_DIR="${FSUAE_DIR:-/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/linux/fs-uae}"
FSUAE_BIN="${FSUAE_BIN:-$FSUAE_DIR/fs-uae}"
DH0="${DH0:-/mnt/c/Users/headk/.vscode/extensions/bartmanabyss.amiga-debug-1.8.2/bin/dh0}"
DH1="${DH1:-/mnt/c/Projects/AI/SWOS/SWOS_C/uae/dh0/data}"
KICKSTART="${KICKSTART:-/mnt/c/Projects/Amiga/Kickstarts/KICK31_A1200.rom}"
PORT="${PORT:-2345}"
TRIGGER="${TRIGGER:-:SWOS2}"
ENTRY_PC="${ENTRY_PC:-0x219b10}"     # SWOS2 _start/main; PC here == healthy stop

FSUAE_LOG="${FSUAE_LOG:-/tmp/fs-uae.log}"
DAEMON_LOG="${DAEMON_LOG:-/tmp/swos_gdb_daemon.log}"
SOCK="${SOCK:-/tmp/swos_gdb.sock}"

ATTACH_RETRIES="${ATTACH_RETRIES:-4}"
BOOT_TIMEOUT="${BOOT_TIMEOUT:-60}"   # max secs to wait for the entry trigger
SETTLE="${SETTLE:-6}"                # secs after trigger before attaching

log() { echo "[amiga_gdb] $*" >&2; }

# ---- teardown -------------------------------------------------------------
# NOTE: this is a SCRIPT FILE, so its own argv is "bash amiga_gdb.sh" — it does
# NOT contain the FS-UAE arg strings, so pkill -f patterns here can't self-match
# (the inline-command self-kill trap doesn't apply inside a script).
down() {
    pkill -9 -f 'm68k-amiga-elf-gdb' 2>/dev/null
    pkill -9 -f 'gdb_proxy.py daemon' 2>/dev/null
    pkill -9 -f "fs-uae .*remote_debugger_port=${PORT}" 2>/dev/null
    rm -f "$SOCK" 2>/dev/null
    # give the OS a moment to release the port / pids
    for _ in 1 2 3 4 5; do
        ss -ltn 2>/dev/null | grep -q ":${PORT} " || break
        sleep 0.5
    done
}

# ---- launch FS-UAE (detached) --------------------------------------------
launch_fsuae() {
    ( cd "$FSUAE_DIR" && \
      setsid env LD_LIBRARY_PATH=. nohup "$FSUAE_BIN" \
        --amiga_model=a1200 --automatic_input_grab=0 \
        --hard_drive_0="$DH0" --hard_drive_1="$DH1" \
        --remote_debugger=20 --remote_debugger_port="$PORT" \
        --remote_debugger_trigger="$TRIGGER" \
        --ntsc_mode=0 --kickstart_file="$KICKSTART" \
        --chip_memory=2048 --fast_memory=8192 \
        > "$FSUAE_LOG" 2>&1 < /dev/null & disown )
}

# ---- wait for the SWOS2 entry trigger ------------------------------------
wait_for_trigger() {
    local end=$(( SECONDS + BOOT_TIMEOUT ))
    while (( SECONDS < end )); do
        if grep -q 'Breakpoint 20 triggered' "$FSUAE_LOG" 2>/dev/null; then
            return 0
        fi
        sleep 1
    done
    return 1
}

# ---- start the proxy daemon ----------------------------------------------
start_daemon() {
    setsid nohup python3 "$GDB_PROXY" daemon --mode amiga --host 127.0.0.1 \
        > "$DAEMON_LOG" 2>&1 < /dev/null & disown
    local end=$(( SECONDS + 15 ))
    while (( SECONDS < end )); do
        if [ -S "$SOCK" ] && python3 "$GDB_PROXY" ping 2>/dev/null | grep -qi pong; then
            return 0
        fi
        sleep 1
    done
    return 1
}

# ---- health check: target stopped (SIGTRAP) with a readable PC ------------
# Returns 0 healthy, 1 degraded. Drains the attach banner first (the daemon's
# first cmd returns the buffered "target remote" banner, not the real state).
is_healthy() {
    python3 "$GDB_PROXY" ping 2>/dev/null | grep -qi pong || return 1
    python3 "$GDB_PROXY" cmd "print 1" --timeout 5 >/dev/null 2>&1     # drain banner
    local ip pc
    ip="$(python3 "$GDB_PROXY" cmd "info program" --timeout 6 2>&1)"
    echo "$ip" | grep -qi "SIGTRAP" || return 1
    pc="$(python3 "$GDB_PROXY" cmd "print/x \$pc" --timeout 5 2>&1)"
    echo "$pc" | grep -qiE '0x[0-9a-f]+' || return 1
    # PC should be the SWOS2 entry (or at least a real code address, not 0).
    echo "$pc" | grep -qi "0x0\b" && return 1
    return 0
}

# ---- one full attach attempt (clean -> launch -> wait -> daemon -> verify) -
attach_once() {
    down
    log "launching FS-UAE..."
    launch_fsuae
    log "waiting for SWOS2 entry trigger (<= ${BOOT_TIMEOUT}s)..."
    if ! wait_for_trigger; then
        log "  trigger never appeared in $FSUAE_LOG"
        return 1
    fi
    log "  trigger hit; settling ${SETTLE}s..."
    sleep "$SETTLE"
    log "starting gdb daemon..."
    if ! start_daemon; then
        log "  daemon never came up (no pong)"
        return 1
    fi
    log "health check..."
    if is_healthy; then
        return 0
    fi
    log "  attach DEGRADED (target not cleanly stopped)"
    return 1
}

up() {
    local i
    for (( i = 1; i <= ATTACH_RETRIES; i++ )); do
        log "=== attach attempt $i/$ATTACH_RETRIES ==="
        if attach_once; then
            local pc
            pc="$(python3 "$GDB_PROXY" cmd "print/x \$pc" --timeout 5 2>&1 | grep -oiE '0x[0-9a-f]+' | head -1)"
            log "ATTACHED + HEALTHY (PC=$pc). Drive with: python3 $GDB_PROXY cmd \"...\""
            echo "OK"
            return 0
        fi
        log "attempt $i failed; retrying with a full restart..."
        sleep 2
    done
    log "FAILED to get a healthy attach after $ATTACH_RETRIES attempts."
    log "Check $FSUAE_LOG and $DAEMON_LOG. Is the ELF built (uae/dh0/data/SWOS2.elf)?"
    echo "FAIL"
    return 1
}

status() {
    if [ ! -S "$SOCK" ]; then echo "DOWN (no socket)"; return 1; fi
    if is_healthy; then
        local pc; pc="$(python3 "$GDB_PROXY" cmd "print/x \$pc" --timeout 5 2>&1 | grep -oiE '0x[0-9a-f]+' | head -1)"
        echo "HEALTHY (PC=$pc)"; return 0
    fi
    echo "DEGRADED"; return 1
}

case "${1:-up}" in
    up)      up ;;
    down)    down; log "torn down."; echo "DOWN" ;;
    restart) down; up ;;
    status)  status ;;
    *) echo "usage: $0 {up|down|restart|status}" >&2; exit 2 ;;
esac
