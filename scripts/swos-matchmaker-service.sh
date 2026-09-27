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
    sudo mkdir -p "${DEPLOY_DIR}/tls" "${DEPLOY_DIR}/bin" "${DEPLOY_DIR}/recordings" "${DEPLOY_DIR}/reports"
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
    sudo cp "$BUILT_BINARY" "$BINARY"
    sudo chown "${RUN_USER}:${RUN_USER}" "$BINARY"
    sudo chmod 755 "$BINARY"
}

install_service() {
    ensure_srv_layout
    deploy_binary

    echo "Writing ${SERVICE_FILE} ..."
    sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=OpenSWOS matchmaking / relay server
After=network-online.target
Wants=network-online.target

[Service]
User=${RUN_USER}
WorkingDirectory=${DEPLOY_DIR}
ExecStart=${BINARY} ${PORT} --websocket --tls-cert /srv/swos-matchmaker/tls/fullchain.pem --tls-key /srv/swos-matchmaker/tls/privkey.pem --record --recordings /srv/swos-matchmaker/recordings --reports /srv/swos-matchmaker/reports --ledger /srv/swos-matchmaker/matches.jsonl
ReadWritePaths=/srv/swos-matchmaker/recordings /srv/swos-matchmaker/reports /srv/swos-matchmaker/matches.jsonl
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

    echo "Reloading systemd and enabling service ..."
    sudo systemctl daemon-reload
    sudo systemctl enable --now "$SERVICE_NAME"
    echo "Installed and started."
    systemctl status "$SERVICE_NAME" --no-pager || true
}

restart_service() {
    deploy_binary
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
