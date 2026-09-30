#!/bin/bash
# Update the recordings player at /playback/ (docs/PLAYBACK.md) -- the admin page's
# "Update recordings player" button (tools/matchmaker-admin), or by hand on the server:
#
#   scripts/update_playback.sh
#
# Builds it (scripts/build_playback.sh), copies it into DEPLOY_DIR/playback, and brings
# the recordings' cards and index.json up to date (scripts/recordings_index.py). No root
# and no restart: the player is static files the service account owns, so a match in
# progress is never touched. nginx is set up once, by swos-matchmaker-service.sh install.
#
# Exit 3 when emsdk is missing (build_playback.sh's "skipped"), 1 on a real failure.
set -euo pipefail
cd "$(dirname "$0")/.."

DEPLOY_DIR="${DEPLOY_DIR:-/srv/swos-matchmaker}"
DEST="$DEPLOY_DIR/playback"
FILES="index.html SWOS.html SWOS.js SWOS.wasm SWOS.data"

set +e
scripts/build_playback.sh
rc=$?
set -e
[ "$rc" = 0 ] || exit "$rc"

if [ ! -d "$DEST" ] || [ ! -w "$DEST" ]; then
    echo "update_playback: $DEST is missing or not writable by $(id -un) -- run" >&2
    echo "  scripts/swos-matchmaker-service.sh install    (once, as the admin)" >&2
    exit 1
fi
# Each file beside its old self, then renamed over it: a page loading while this runs
# gets the old file or the new one, never half of one.
for f in $FILES; do
    cp -f "dist/playback/$f" "$DEST/.$f.new"
    mv -f "$DEST/.$f.new" "$DEST/$f"
done
echo "update_playback: copied $FILES -> $DEST"

if [ -d "$DEPLOY_DIR/recordings" ]; then
    python3 scripts/recordings_index.py --recordings "$DEPLOY_DIR/recordings" \
                                        --ledger "$DEPLOY_DIR/matches.jsonl" || true
fi
echo "update_playback: the player is live at /playback/ (reload the page)"
