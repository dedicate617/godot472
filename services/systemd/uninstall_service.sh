#!/usr/bin/env bash
# ==============================================================================
# MindSCADA Systemd Service Uninstaller (Linux / Raspberry Pi OS)
# ==============================================================================
set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "[ERROR] This uninstaller must be run as root (use sudo ./uninstall_service.sh)."
    exit 1
fi

SERVICE_NAME="mindscada.service"
TARGET_UNIT="/etc/systemd/system/${SERVICE_NAME}"

echo "Stopping and disabling ${SERVICE_NAME}..."
systemctl stop "${SERVICE_NAME}" 2>/dev/null || true
systemctl disable "${SERVICE_NAME}" 2>/dev/null || true

if [ -f "${TARGET_UNIT}" ]; then
    rm -f "${TARGET_UNIT}"
    echo "Removed ${TARGET_UNIT}"
fi

systemctl daemon-reload
echo "[SUCCESS] MindSCADA systemd service uninstalled."
