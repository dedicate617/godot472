#!/usr/bin/env bash
# ==============================================================================
# MindSCADA / MindStudio - One-Click Multi-Platform Build Script
#
# Usage:
#   ./build_all.sh [options]
#   ./build_all.sh --windows [options]   (Build Windows Editor & Templates)
#   ./build_all.sh --web [options]       (Build Web WASM32 Debug & Release Templates)
#   ./build_all.sh --all [options]       (Build both Windows and Web distributions)
#
# Common Options:
#   --dry-run                           (Simulate build without compiling)
#   -j N                                (Set concurrency jobs, default: CPU cores)
#   --no-cache                          (Disable SCons compilation cache)
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

BUILD_PLATFORM="windows"
EXTRA_ARGS=()

show_help() {
    echo "============================================================================"
    echo "MindSCADA One-Click Build Script"
    echo "============================================================================"
    echo "Usage:"
    echo "  $0 [--windows | --web | --all] [options...]"
    echo ""
    echo "Platform Targets:"
    echo "  --windows          Build Windows x86_64 Editor & Export Templates (default)"
    echo "  --web              Build Web WASM32 Debug & Release Export Templates"
    echo "  --all              Build both Windows and Web distributions"
    echo ""
    echo "Common Options:"
    echo "  --dry-run          Simulate build commands without running SCons"
    echo "  -j <jobs>          Set number of concurrent compilation jobs"
    echo "  --no-cache         Disable SCons compilation cache"
    echo "  --help, -h         Show this help message"
    echo "============================================================================"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --web|web)
            BUILD_PLATFORM="web"
            shift
            ;;
        --windows|windows)
            BUILD_PLATFORM="windows"
            shift
            ;;
        --all|all)
            BUILD_PLATFORM="all"
            shift
            ;;
        --help|-h)
            show_help
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

echo "============================================================================"
echo "[MindSCADA] Starting One-Click Distribution Build"
echo "Working Directory : ${SCRIPT_DIR}"
echo "Cache Directory   : ${SCRIPT_DIR}/.cache/scons"
echo "Target Scope      : ${BUILD_PLATFORM}"
if [[ ${#EXTRA_ARGS[@]} -gt 0 ]]; then
    echo "Extra Arguments   : ${EXTRA_ARGS[*]}"
fi
echo "============================================================================"

PACKAGE_EXTRA_ARGS=()
for arg in "${EXTRA_ARGS[@]}"; do
    if [[ "$arg" == "--dry-run" ]]; then
        PACKAGE_EXTRA_ARGS+=("--dry-run")
    fi
done

# 1. Windows build
if [[ "${BUILD_PLATFORM}" == "windows" || "${BUILD_PLATFORM}" == "all" ]]; then
    echo ""
    echo "===> Compiling Windows x86_64 Targets (Editor Release, Template Debug & Release)..."
    python scripts/build.py --platform windows --arch x86_64 --dist "${EXTRA_ARGS[@]}"

    if [[ "${BUILD_PLATFORM}" == "windows" ]]; then
        echo ""
        echo "============================================================================"
        echo "[SUCCESS] Windows compilation completed!"
        echo "Packaging Windows distribution archives into dist/..."
        echo "============================================================================"
        python scripts/package.py --platform windows "${PACKAGE_EXTRA_ARGS[@]}"
    fi
fi

# 2. Web build
if [[ "${BUILD_PLATFORM}" == "web" || "${BUILD_PLATFORM}" == "all" ]]; then
    echo ""
    echo "===> Compiling Web WASM32 Targets (Template Debug & Release)..."
    python scripts/build.py --platform web --arch wasm32 --dist "${EXTRA_ARGS[@]}"

    if [[ "${BUILD_PLATFORM}" == "web" ]]; then
        echo ""
        echo "============================================================================"
        echo "[SUCCESS] Web compilation completed!"
        echo "Packaging Web distribution archives into dist/..."
        echo "============================================================================"
        python scripts/package.py --platform web "${PACKAGE_EXTRA_ARGS[@]}"
    fi
fi

# 3. Package all
if [[ "${BUILD_PLATFORM}" == "all" ]]; then
    echo ""
    echo "============================================================================"
    echo "[SUCCESS] All target compilations completed!"
    echo "Packaging all distribution archives into dist/..."
    echo "============================================================================"
    python scripts/package.py --platform all "${PACKAGE_EXTRA_ARGS[@]}"
fi

echo ""
echo "============================================================================"
echo "[SUCCESS] One-Click Build & Packaging Completed Successfully!"
echo "Artifacts and checksums available in: ${SCRIPT_DIR}/dist/"
echo "============================================================================"
