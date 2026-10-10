#!/usr/bin/env bash
# ==============================================================================
# MindSCADA Backend Services Stopper (Linux / Raspberry Pi)
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_DIR="${SCRIPT_DIR}/.pids"

# Colors
C_GREEN="\033[92m"
C_YELLOW="\033[93m"
C_RED="\033[91m"
C_BOLD="\033[1m"
C_RESET="\033[0m"

echo "========================================================"
echo " Stopping MindSCADA Backend Services (Linux/Raspberry Pi)"
echo "========================================================"
echo ""

# 1. Stop tracked processes from PID files
if [ -d "${PID_DIR}" ]; then
    for pid_file in "${PID_DIR}"/*.pid; do
        if [ -f "${pid_file}" ]; then
            sname="$(basename "${pid_file}" .pid)"
            pid="$(cat "${pid_file}" 2>/dev/null || true)"
            if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then
                echo "Stopping ${sname} (PID: ${pid})..."
                kill "${pid}" 2>/dev/null || true
                sleep 0.2
                if kill -0 "${pid}" 2>/dev/null; then
                    kill -9 "${pid}" 2>/dev/null || true
                fi
            fi
            rm -f "${pid_file}"
        fi
    done
fi

# 2. Stop daemon if running
if [ -f "${SCRIPT_DIR}/.scada_daemon.pid" ]; then
    dpid="$(cat "${SCRIPT_DIR}/.scada_daemon.pid" 2>/dev/null || true)"
    if [ -n "${dpid}" ] && kill -0 "${dpid}" 2>/dev/null; then
        echo "Stopping supervisor daemon (PID: ${dpid})..."
        kill "${dpid}" 2>/dev/null || true
        sleep 0.5
        kill -9 "${dpid}" 2>/dev/null || true
    fi
    rm -f "${SCRIPT_DIR}/.scada_daemon.pid"
fi

# 3. Clean up listening ports (4840, 1883, 4841, 8088, 8090, 8095)
PORTS=(4840 1883 4841 8088 8090 8095)
for p in "${PORTS[@]}"; do
    if command -v fuser >/dev/null 2>&1; then
        fuser -k -n tcp "${p}" >/dev/null 2>&1 || true
    fi
done

echo ""
echo "========================================================"
echo -e "${C_GREEN}[SUCCESS] All services stopped successfully.${C_RESET}"
echo "========================================================"
