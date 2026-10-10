#!/usr/bin/env bash
# ==============================================================================
# MindSCADA Native HMI Client Launcher for Raspberry Pi / Linux ARM
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}"

# 1. Architecture detection
ARCH="$(uname -m)"
echo "========================================================"
echo " MindSCADA Native Client Launcher (Raspberry Pi / Linux)"
echo "========================================================"
echo " Detected Architecture : ${ARCH}"
echo " Working Directory     : ${SCRIPT_DIR}"
echo "========================================================"

RUNNER_BIN=""
PCK_FILE=""

# Candidate directories to search
SEARCH_DIRS=(
    "${SCRIPT_DIR}"
    "${SCRIPT_DIR}/bin"
    "${SCRIPT_DIR}/build"
    "${ROOT_DIR}/../godot.4.2.1/bin"
    "${ROOT_DIR}/../godot.4.7.2/bin"
    "/opt/mindscada"
    "/opt/mindscada/bin"
)

# 2. Locate Engine Executable
if [[ "${ARCH}" == "aarch64"* || "${ARCH}" == "arm64"* ]]; then
    CAND_RUNNERS=(
        "MindSCADA.linuxbsd.template_release.arm64"
        "MindSCADA.linuxbsd.template_debug.arm64"
        "godot.linuxbsd.template_release.arm64"
        "linux_release_arm64"
        "MindSCADA.arm64"
    )
else
    # 32-bit ARM (armv7l, armv6l, etc.)
    CAND_RUNNERS=(
        "MindSCADA.linuxbsd.template_release.arm32"
        "MindSCADA.linuxbsd.template_debug.arm32"
        "MindSCADA.linuxbsd.editor.arm32"
        "godot.linuxbsd.template_release.arm32"
        "linux_release_arm32"
        "MindSCADA.arm32"
    )
fi

for d in "${SEARCH_DIRS[@]}"; do
    for r in "${CAND_RUNNERS[@]}"; do
        if [ -f "${d}/${r}" ]; then
            RUNNER_BIN="${d}/${r}"
            break 2
        fi
    done
done

# 3. Locate PCK Package
CAND_PCKS=(
    "client.pck"
    "client_arm32.pck"
    "client_arm64.pck"
    "client.pck.arm32"
    "client.pck.arm64"
    "mindscada.pck"
    "luxiangyi4.pck"
    "build/client_arm32.pck"
    "build/client_arm64.pck"
    "../scada4/build/client_arm32.pck"
    "../scada4/build/client_arm64.pck"
)

for d in "${SEARCH_DIRS[@]}"; do
    for p in "${CAND_PCKS[@]}"; do
        if [ -f "${d}/${p}" ]; then
            PCK_FILE="${d}/${p}"
            break 2
        fi
    done
done

if [ -z "${RUNNER_BIN}" ]; then
    echo "[ERROR] No compatible MindSCADA Linux executable found for ${ARCH}!"
    echo "Please place MindSCADA.linuxbsd.template_release.arm32 / arm64 in bin/ or install via package."
    exit 1
fi

chmod +x "${RUNNER_BIN}" 2>/dev/null || true
echo " [OK] Engine Binary : ${RUNNER_BIN}"

CMD=("${RUNNER_BIN}")
if [ -n "${PCK_FILE}" ]; then
    echo " [OK] Project Pack  : ${PCK_FILE}"
    CMD+=("--main-pack" "${PCK_FILE}")
else
    echo " [WARN] No standalone .pck file found, launching engine directly."
fi

# Set Display if not set
if [ -z "${DISPLAY}" ] && [ -z "${WAYLAND_DISPLAY}" ]; then
    export DISPLAY=:0
fi

echo "========================================================"
echo " Launching MindSCADA HMI..."
echo "========================================================"

exec "${CMD[@]}" "$@"
