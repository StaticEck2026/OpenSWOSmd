#!/bin/bash
# Root cron worker for matchmaker-admin: processes queued request files dropped
# by the unprivileged web gateway (main.py) into $REQUESTS_DIR and actually runs
# the privileged action - this script is the ONLY thing in the whole setup that
# runs as root. The gateway itself never does, and never can (see main.py's own
# header comment for the full split rationale).
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
RUN_USER="redhair"
ADMIN_DIR="/srv/swos-matchmaker/admin"
REQUESTS_DIR="$ADMIN_DIR/requests"
PROCESSED_DIR="$ADMIN_DIR/processed"
LATEST_LOG="$PROCESSED_DIR/latest.log"
LOCK_FILE="$ADMIN_DIR/worker.lock"

mkdir -p "$REQUESTS_DIR" "$PROCESSED_DIR"

# --rebuild --build-only alone can run well past a minute - if it's still going
# when cron fires again, skip that tick entirely rather than starting a second
# build in the same build/ directory. The next tick (or the one after) picks up
# whatever's still queued once the lock is free.
exec 9>"$LOCK_FILE"
flock -n 9 || exit 0

# git pull / the build keep the repo's files owned by redhair, not root - a
# root-owned file appearing in a redhair-owned checkout is exactly the kind of
# thing that breaks the NEXT normal (non-admin) build in confusing ways.
run_as_repo_user() {
    sudo -u "$RUN_USER" -H bash -c "cd '$REPO_DIR' && $1"
}

run_action() {
    local action="$1"
    case "$action" in
        pull)  run_as_repo_user "git pull --ff-only" ;;
        build) run_as_repo_user "scripts/run_matchmaker.sh --rebuild --build-only" ;;
        # The service script needs real root (systemctl/tee into /etc/systemd) -
        # runs directly, not through run_as_repo_user, unlike pull/build above.
        install|remove|restart|status) "$REPO_DIR/scripts/swos-matchmaker-service.sh" "$action" ;;
    esac
}

shopt -s nullglob
for req in "$REQUESTS_DIR"/*.req; do
    base="$(basename "$req" .req)"
    action="${base#*-}"

    # Validated here, in the actual loop - NOT inside the piped block below,
    # since that runs in a subshell where `continue` can't reach this for loop.
    case "$action" in
        pull|build|install|remove|restart|status) ;;
        *)
            echo "admin-cron-worker: unrecognized action '$action' in $(basename "$req"), leaving it in place" >&2
            continue
            ;;
    esac

    ts="$(date '+%Y-%m-%d %H:%M:%S')"
    logfile="$PROCESSED_DIR/${base}.log"

    {
        echo "=== $ts  action=$action ==="
        if run_action "$action" 2>&1; then
            echo "--- OK ---"
        else
            echo "--- FAILED (exit $?) ---"
        fi
    } | tee "$logfile" > "$LATEST_LOG"

    rm -f "$req"
done
