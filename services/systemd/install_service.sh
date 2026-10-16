#!/usr/bin/env bash
# ==============================================================================
# MindSCADA Systemd Service Auto-Installer (Linux / Raspberry Pi OS)
# ==============================================================================
set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "[ERROR] This installer must be run as root (use sudo ./install_service.sh)."
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICES_DIR="$(dirname "${SCRIPT_DIR}")"
SERVICE_NAME="mindscada.service"
TARGET_UNIT="/etc/systemd/system/${SERVICE_NAME}"

# Detect running user (if invoked with sudo, use SUDO_USER)
RUN_USER="${SUDO_USER:-$(logname 2>/dev/null || echo pi)}"
PYTHON_PATH="$(which python3 2>/dev/null || echo /usr/bin/python3)"

echo "========================================================"
echo " MindSCADA Systemd Service Auto-Installer"
echo "========================================================"
echo " Services Dir : ${SERVICES_DIR}"
echo " Service User : ${RUN_USER}"
echo " Python Exec  : ${PYTHON_PATH}"
echo " Unit File    : ${TARGET_UNIT}"
echo "========================================================"

cat <<EOF > "${TARGET_UNIT}"
[Unit]
Description=MindSCADA Industrial Engine & Gateway Daemon
After=network.target network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${RUN_USER}
WorkingDirectory=${SERVICES_DIR}
Environment="PYTHONUNBUFFERED=1"
ExecStart=${PYTHON_PATH} ${SERVICES_DIR}/scada_daemon.py run
ExecStop=${PYTHON_PATH} ${SERVICES_DIR}/scada_daemon.py stop
Restart=always
RestartSec=3s
LimitNOFILE=65535
TimeoutStopSec=15s
KillMode=mixed

[Install]
WantedBy=multi-user.target
EOF

chmod 644 "${TARGET_UNIT}"

echo "[INFO] Reloading systemd manager configuration..."
systemctl daemon-reload

echo "[INFO] Enabling ${SERVICE_NAME} for auto-start on boot..."
systemctl enable "${SERVICE_NAME}"

echo "[INFO] Starting ${SERVICE_NAME}..."
systemctl start "${SERVICE_NAME}" || true

echo ""
echo "========================================================"
echo " [SUCCESS] MindSCADA systemd service installed and active!"
echo " Control Commands:"
echo "   sudo systemctl status ${SERVICE_NAME}"
echo "   sudo systemctl restart ${SERVICE_NAME}"
echo "   sudo systemctl stop ${SERVICE_NAME}"
echo "   journalctl -u ${SERVICE_NAME} -f"
echo "========================================================"
