#!/usr/bin/env bash
# ==============================================================================
# Start MindSCADA Supervisor Daemon (Linux / Raspberry Pi)
# ==============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="$(which python3 2>/dev/null || which python 2>/dev/null || echo python3)"

if [ "$1" = "--foreground" ] || [ "$1" = "-f" ] || [ "$1" = "run" ]; then
    exec "${PYTHON_BIN}" "${SCRIPT_DIR}/scada_daemon.py" run
else
    exec "${PYTHON_BIN}" "${SCRIPT_DIR}/scada_daemon.py" start
fi
