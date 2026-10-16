#!/usr/bin/env bash
# ==============================================================================
# MindSCADA Systemd Service Health Check (Linux / Raspberry Pi OS)
# ==============================================================================
set -e

SERVICE_NAME="mindscada.service"

GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
NC="\033[0m"

echo "========================================================"
echo " MindSCADA Systemd Service Health Check"
echo "========================================================"

# 1. Systemd Status
if systemctl is-active --quiet "${SERVICE_NAME}"; then
    echo -e " Service Status   : ${GREEN}ACTIVE (Running)${NC}"
else
    echo -e " Service Status   : ${RED}INACTIVE or FAILED${NC}"
fi

if systemctl is-enabled --quiet "${SERVICE_NAME}" 2>/dev/null; then
    echo -e " Auto-start Boot  : ${GREEN}ENABLED${NC}"
else
    echo -e " Auto-start Boot  : ${YELLOW}DISABLED${NC}"
fi

echo "--------------------------------------------------------"
echo " Port Verification:"

check_port() {
    local name="$1"
    local port="$2"
    if nc -z 127.0.0.1 "$port" 2>/dev/null || nc -z ::1 "$port" 2>/dev/null || (exec 3<>/dev/tcp/127.0.0.1/"$port") 2>/dev/null; then
        echo -e "   [${GREEN}PASS${NC}] ${name} on port ${port}"
    else
        echo -e "   [${RED}FAIL${NC}] ${name} on port ${port} (not listening)"
    fi
}

check_port "OPC-UA Server" 4840
check_port "NanoMQ Broker" 1883
check_port "SCADA Tunnel" 4841
check_port "WS Gateway" 8088
check_port "Web Server" 8090
check_port "Stream Gateway" 8095

echo "========================================================"
echo " Quick Logs (Last 10 lines):"
journalctl -u "${SERVICE_NAME}" -n 10 --no-pager 2>/dev/null || true
echo "========================================================"
