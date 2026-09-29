#!/bin/bash
# Install/restart/remove the swos-matchmaker-admin systemd service - the small,
# unprivileged web gateway (main.py) that only ever drops request files for
# admin-cron-worker.sh (root, via crontab) to actually act on. See main.py's
# and admin-cron-worker.sh's own header comments for the full split rationale.
#
# Usage: ./matchmaker-admin-service.sh {install|restart|remove|status}
#
# This installs the SERVICE (the Python gateway). It does NOT install the root
# crontab entry or the nginx auth_basic proxy - see this directory's README.md
# for those two manual, one-time steps (root crontab and nginx both need a
# human decision - a password file to create, a cron line to add - not
# something this script should silently do for you).
set -euo pipefail

SERVICE_NAME="swos-matchmaker-admin"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
REPO_DIR="/home/redhair/git/OpenSWOS"
RUN_USER="redhair"
TOOL_DIR="${REPO_DIR}/tools/matchmaker-admin"
PYTHON_BIN="$(command -v python3)"

usage() {
    echo "Usage: $0 {install|restart|remove|status}" >&2
    exit 1
}

install_service() {
    echo "Writing ${SERVICE_FILE} ..."
    sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=swos-matchmaker admin gateway (unprivileged - see main.py)
After=network.target

[Service]
Type=simple
User=${RUN_USER}
Group=${RUN_USER}
WorkingDirectory=${TOOL_DIR}
ExecStart=${PYTHON_BIN} ${TOOL_DIR}/main.py
Restart=always
RestartSec=2

# Binds 127.0.0.1:8899 only (see main.py) - this hardening is defense in depth
# on top of that, not a substitute for it. No ProtectHome: pull/build run
# right in this process now (main.py's LOCAL_ACTIONS), as this same user, and
# git itself touches more of $HOME than just the repo (confirmed on the live
# deploy: credential.helper=store's lock file under ~/.git-credentials failed
# under ProtectHome=read-only) - enumerating every dotfile git might lock
# isn't worth it when this already runs as the account's own normal user;
# ProtectSystem=strict is the hardening that actually matters here (the rest
# of the OS stays read-only regardless).
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ReadWritePaths=/srv/swos-matchmaker/admin
# Clearing fetched reports and recordings (main.py CLEAR_ACTIONS, POST /clear).
ReadWritePaths=/srv/swos-matchmaker/reports
ReadWritePaths=/srv/swos-matchmaker/recordings

[Install]
WantedBy=multi-user.target
EOF

    echo "Reloading systemd and enabling service ..."
    sudo systemctl daemon-reload
    sudo systemctl enable "$SERVICE_NAME"
    # restart, not "enable --now": a reinstall is how a changed unit (ReadWritePaths) or
    # main.py goes live, and "--now" leaves an already-running gateway as it was.
    sudo systemctl restart "$SERVICE_NAME"
    echo "Installed and (re)started."
    systemctl status "$SERVICE_NAME" --no-pager || true
}

restart_service() {
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
