#!/bin/bash
# Root cron worker for matchmaker-admin: processes queued request files dropped
# by the unprivileged web gateway (main.py) into $REQUESTS_DIR and actually runs
# the privileged action - this script is the ONLY thing in the whole setup that
# runs as root. The gateway itself never does, and never can (see main.py's own
# header comment for the full split rationale).
#
# Only install/remove/restart ever get queued here - pull/build/status need no
# elevated privilege at all, so main.py runs those itself, immediately, and
# this script never even sees them (see main.py's own LOCAL_ACTIONS/
# QUEUED_ACTIONS split).
#
# Install: root's crontab, once a minute -
#   * * * * * /home/redhair/git/OpenSWOS/tools/matchmaker-admin/admin-cron-worker.sh >/dev/null 2>&1
#
# A request file's NAME is the whole request (empty contents, never read/eval'd)
# - `<unixnano>-<action>.req`, action checked against the fixed case statement
# below. Anything that doesn't match is left alone (not deleted) so a mismatched
# gateway/worker version pair fails loudly (a stuck .req file) instead of
# silently dropping requests.
set -uo pipefail

REPO_DIR="/home/redhair/git/OpenSWOS"
ADMIN_DIR="/srv/swos-matchmaker/admin"
REQUESTS_DIR="$ADMIN_DIR/requests"
PROCESSED_DIR="$ADMIN_DIR/processed"
LATEST_LOG="$PROCESSED_DIR/latest.log"
LOCK_FILE="$ADMIN_DIR/worker.lock"

RUN_USER="redhair"

# Whichever of this (root, once a minute) or main.py (redhair, on startup)
# happens to create these dirs FIRST owns them - if this runs first, they'd
# end up root-owned and main.py could never write into them again (confirmed:
# exactly this happened on first deploy). Root can always chown regardless of
# current ownership, so just re-assert it every tick - self-healing either way.
mkdir -p "$REQUESTS_DIR" "$PROCESSED_DIR"
chown "$RUN_USER:$RUN_USER" "$ADMIN_DIR" "$REQUESTS_DIR" "$PROCESSED_DIR"

# install/restart aren't as slow as a rebuild, but the lock still protects
# against two clicks in the same minute overlapping (e.g. install then remove).
exec 9>"$LOCK_FILE"
flock -n 9 || exit 0

shopt -s nullglob
for req in "$REQUESTS_DIR"/*.req; do
    base="$(basename "$req" .req)"
    action="${base#*-}"

    # Validated here, in the actual loop - NOT inside the piped block below,
    # since that runs in a subshell where `continue` can't reach this for loop.
    case "$action" in
        install|remove|restart) ;;
        *)
            echo "admin-cron-worker: unrecognized action '$action' in $(basename "$req"), leaving it in place" >&2
            continue
            ;;
    esac

    ts="$(date '+%Y-%m-%d %H:%M:%S')"
    logfile="$PROCESSED_DIR/${base}.log"

    {
        echo "=== $ts  action=$action ==="
        if "$REPO_DIR/scripts/swos-matchmaker-service.sh" "$action" 2>&1; then
            echo "--- OK ---"
        else
            echo "--- FAILED (exit $?) ---"
        fi
    } | tee "$logfile" > "$LATEST_LOG"

    rm -f "$req"
done
