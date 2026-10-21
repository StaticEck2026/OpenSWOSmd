#!/bin/bash
# prune_recordings.sh -- delete the server's match recordings once they are old.
#
#   scripts/prune_recordings.sh                 # delete recordings unchanged for 30 days
#   scripts/prune_recordings.sh --days 14       # ... for 14
#   scripts/prune_recordings.sh --dry-run       # say what would go, delete nothing
#   sudo scripts/prune_recordings.sh --install  # a daily systemd timer runs it (as the
#                                               # checkout's owner); --remove takes it away
#
# The recorder keeps every match the matchmaker arranges (about 1 MB each; release builds
# record by default), and nothing else ever deletes them except a fetch
# (scripts/fetch_recordings.py). Each recording is checked for a desync WHILE it is
# recorded, so an old one is only of use to look at: after DAYS it goes, with its match
# card (scripts/recordings_index.py drops the cards of recordings that are gone and
# rewrites index.json for /playback/). A recording still being written is new by
# definition, so it is never touched. Each run appends what it deleted to
# DEPLOY_DIR/prune.log.
set -uo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_USER="${RUN_USER:-$(stat -c %U "$REPO_DIR")}"
DEPLOY_DIR="${DEPLOY_DIR:-/srv/swos-matchmaker}"
RECORDINGS="$DEPLOY_DIR/recordings"
LOG="$DEPLOY_DIR/prune.log"
DAYS=30
DRY=0
UNIT=swos-prune-recordings

install_timer() {
    cat > "/etc/systemd/system/$UNIT.service" <<EOF
[Unit]
Description=Delete OpenSWOS match recordings older than $DAYS days (scripts/prune_recordings.sh)

[Service]
Type=oneshot
User=$RUN_USER
Group=$RUN_USER
ExecStart=$REPO_DIR/scripts/prune_recordings.sh --days $DAYS
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ReadWritePaths=$RECORDINGS $LOG
EOF
    cat > "/etc/systemd/system/$UNIT.timer" <<EOF
[Unit]
Description=Daily: delete OpenSWOS match recordings older than $DAYS days

[Timer]
OnCalendar=*-*-* 04:30:00
RandomizedDelaySec=15min
Persistent=true

[Install]
WantedBy=timers.target
EOF
    install -o "$RUN_USER" -g "$RUN_USER" -m 0644 /dev/null "$LOG.new" 2>/dev/null
    [ -e "$LOG" ] || mv "$LOG.new" "$LOG"
    rm -f "$LOG.new"
    systemctl daemon-reload
    systemctl enable --now "$UNIT.timer"
    systemctl list-timers --no-pager "$UNIT.timer" | sed -n 1,2p
}

while [ $# -gt 0 ]; do
    case "$1" in
        --days)    DAYS="${2:?--days needs a number}"; shift ;;
        --dry-run) DRY=1 ;;
        --install) INSTALL=1 ;;
        --remove)  systemctl disable --now "$UNIT.timer" 2>/dev/null
                   rm -f "/etc/systemd/system/$UNIT.timer" "/etc/systemd/system/$UNIT.service"
                   systemctl daemon-reload; echo "removed $UNIT"; exit 0 ;;
        -h|--help) sed -n '2,19p' "$0"; exit 0 ;;
        *) echo "prune_recordings: unknown option '$1' (try --help)" >&2; exit 2 ;;
    esac
    shift
done
case "$DAYS" in ''|*[!0-9]*) echo "prune_recordings: --days must be a number" >&2; exit 2 ;; esac
[ "${INSTALL:-0}" = 1 ] && { install_timer; exit $?; }

[ -d "$RECORDINGS" ] || { echo "prune_recordings: no $RECORDINGS" >&2; exit 1; }
mapfile -t OLD < <(find "$RECORDINGS" -maxdepth 1 -type f -name '*.SRP' -mtime +"$((DAYS - 1))" | sort)
KB=0
for f in "${OLD[@]}"; do KB=$(( KB + $(stat -c %s "$f") / 1024 )); done
STAMP="$(date -u '+%Y-%m-%d %H:%M UTC')"
if [ "$DRY" = 1 ]; then
    printf '%s\n' "${OLD[@]##*/}"
    echo "prune_recordings: would delete ${#OLD[@]} recording(s) older than $DAYS days, $((KB / 1024)) MB"
    exit 0
fi
for f in "${OLD[@]}"; do rm -f -- "$f"; done
# The cards of the recordings that went, and index.json for /playback/.
python3 "$REPO_DIR/scripts/recordings_index.py" --recordings "$RECORDINGS" \
    --ledger "$DEPLOY_DIR/matches.jsonl" >/dev/null 2>&1 ||
    echo "prune_recordings: recordings_index.py failed (the list on /playback/ may be stale)" >&2
LEFT=$(find "$RECORDINGS" -maxdepth 1 -type f -name '*.SRP' | wc -l)
LINE="$STAMP  deleted ${#OLD[@]} recording(s) older than $DAYS days ($((KB / 1024)) MB); $LEFT left"
echo "$LINE" >> "$LOG" 2>/dev/null || true
echo "$LINE"
