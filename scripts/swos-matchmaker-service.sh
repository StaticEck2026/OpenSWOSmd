#!/bin/bash
# Install/restart/remove the swos-matchmaker systemd service.
# Usage: ./swos-matchmaker-service.sh {install|restart|remove|status}
set -euo pipefail

SERVICE_NAME="swos-matchmaker"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
REPO_DIR="/home/redhair/git/OpenSWOS"
RUN_USER="redhair"
BUILT_BINARY="${REPO_DIR}/build/matchmaker/swos-matchmaker"
DEPLOY_DIR="/srv/swos-matchmaker"
BINARY="${DEPLOY_DIR}/bin/swos-matchmaker"
PORT=19602

usage() {
    echo "Usage: $0 {install|restart|remove|status}" >&2
    exit 1
}

ensure_srv_layout() {
    echo "Ensuring ${DEPLOY_DIR} directories/files exist (ReadWritePaths requires them to already be there, systemd won't create them) ..."
    sudo mkdir -p "${DEPLOY_DIR}/tls" "${DEPLOY_DIR}/bin" "${DEPLOY_DIR}/recordings" "${DEPLOY_DIR}/reports" \
                  "${DEPLOY_DIR}/playback"
    sudo touch "${DEPLOY_DIR}/matches.jsonl"
    sudo chown -R "${RUN_USER}:${RUN_USER}" "${DEPLOY_DIR}"
}

deploy_binary() {
    if [ ! -x "$BUILT_BINARY" ]; then
        echo "Error: built binary not found or not executable: $BUILT_BINARY" >&2
        echo "Build it first, then re-run." >&2
        exit 1
    fi
    echo "Copying ${BUILT_BINARY} -> ${BINARY} ..."
    # Copy beside it, then rename over it. Copying straight onto the binary the running
    # server is executing fails with "Text file busy"; a rename is allowed, and the
    # running server keeps the old file until the restart below replaces the process.
    sudo cp "$BUILT_BINARY" "${BINARY}.new"
    sudo chown "${RUN_USER}:${RUN_USER}" "${BINARY}.new"
    sudo chmod 755 "${BINARY}.new"
    sudo mv -f "${BINARY}.new" "$BINARY"
}

# The recordings player (docs/PLAYBACK.md): static files nginx serves at /playback/,
# built beforehand as the unprivileged user (scripts/build_playback.sh, or the admin
# page's Rebuild) -- never here, which can run as root.
deploy_playback() {
    local built="${REPO_DIR}/dist/playback"
    if [ ! -s "${built}/SWOS.wasm" ] || [ ! -s "${built}/index.html" ]; then
        echo "Recordings player not built (${built}); skipping it. Build it with"
        echo "  ${REPO_DIR}/scripts/build_playback.sh   (needs emsdk)"
        return 0
    fi
    echo "Copying the recordings player -> ${DEPLOY_DIR}/playback ..."
    sudo mkdir -p "${DEPLOY_DIR}/playback"
    sudo cp -f "${built}"/index.html "${built}"/SWOS.html "${built}"/SWOS.js "${built}"/SWOS.wasm \
               "${built}"/SWOS.data "${DEPLOY_DIR}/playback/"
    sudo chown -R "${RUN_USER}:${RUN_USER}" "${DEPLOY_DIR}/playback"
    if ! grep -rqs "location /playback/" /etc/nginx/ 2>/dev/null; then
        echo "NOTE: nginx has no 'location /playback/' yet -- see docs/PLAYBACK.md:"
        echo "  location /playback/ { alias ${DEPLOY_DIR}/playback/; }"
    fi
}

install_service() {
    ensure_srv_layout
    deploy_binary
    deploy_playback

    echo "Writing ${SERVICE_FILE} ..."
    # One argument per line. In this unquoted heredoc "\\" writes the single backslash
    # systemd reads as a line continuation (a lone "\" would join the lines here).
    sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=OpenSWOS matchmaking / relay server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${RUN_USER}
Group=${RUN_USER}
WorkingDirectory=${DEPLOY_DIR}
ExecStart=${BINARY} ${PORT} \\
    --websocket \\
    --tls-cert ${DEPLOY_DIR}/tls/fullchain.pem \\
    --tls-key ${DEPLOY_DIR}/tls/privkey.pem \\
    --record \\
    --recordings ${DEPLOY_DIR}/recordings \\
    --reports ${DEPLOY_DIR}/reports \\
    --ledger ${DEPLOY_DIR}/matches.jsonl
Restart=always
RestartSec=2
# SIGTERM is a clean stop: relay sessions close and write their ledger lines.
KillSignal=SIGTERM
TimeoutStopSec=10

# Hardening. It reads its certificate and writes only these three paths, which
# must exist before it starts (ensure_srv_layout): a missing one is 226/NAMESPACE.
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=${DEPLOY_DIR}/recordings
ReadWritePaths=${DEPLOY_DIR}/reports
ReadWritePaths=${DEPLOY_DIR}/matches.jsonl

[Install]
WantedBy=multi-user.target
EOF

    echo "Reloading systemd and enabling service ..."
    sudo systemctl daemon-reload
    sudo systemctl enable "$SERVICE_NAME"
    # restart, not start: install is also how a changed unit or a new binary goes live,
    # and "enable --now" leaves an already-running server as it was. restart starts a
    # stopped one too, so install works whether or not the service exists yet.
    sudo systemctl restart "$SERVICE_NAME"
    echo "Installed and (re)started."
    systemctl status "$SERVICE_NAME" --no-pager || true
}

restart_service() {
    deploy_binary
    deploy_playback
    echo "Restarting ${SERVICE_NAME} ..."
    sudo systemctl restart "$SERVICE_NAME"
    systemctl status "$SERVICE_NAME" --no-pager || true
}

remove_service() {
    echo "Stopping and removing ${SERVICE_NAME} ..."
    sudo systemctl stop "$SERVICE_NAME" 2>/dev/null || true
    sudo systemctl disable "$SERVICE_NAME" 2>/dev/null || true
    sudo rm -f "$SERVICE_FILE"
    sudo systemctl daemon-reload
    echo "Removed."
}

status_service() {
    systemctl status "$SERVICE_NAME" --no-pager
}

case "${1:-}" in
    install) install_service ;;
    restart) restart_service ;;
    remove)  remove_service ;;
    status)  status_service ;;
    *) usage ;;
esac
