#!/usr/bin/env bash
# ==============================================================================
# MindSCADA Engine Extension - One-Click Dual-Platform Release Publisher
#
# Simultaneously publishes releases to both GitHub and Gitee:
#   1. Validates local repository state
#   2. Configures/verifies GitHub (origin) & Gitee (gitee) remotes
#   3. Creates version tag (e.g. v4.7.2-YYYYMMDD)
#   4. Pushes commits & tag to GitHub (triggers CI/CD multi-arch build matrix)
#   5. Pushes commits & tag to Gitee (syncs repository and release tags)
#   6. Optionally uploads local packaged assets from dist/ to Gitee Release
#
# Usage:
#   ./publish.sh [tag_name] [options]
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

GITHUB_REMOTE_URL="https://github.com/dedicate617/godot472.git"
GITEE_REMOTE_URL="https://gitee.com/bosmutus/mind-scada-release.git"

TARGET_TAG=""
IS_DRY_RUN=false
FORCE_PUSH=false

show_help() {
    echo "============================================================================"
    echo "MindSCADA One-Click Dual-Platform Release Publisher"
    echo "============================================================================"
    echo "Usage:"
    echo "  $0 [tag_name] [options]"
    echo ""
    echo "Arguments:"
    echo "  tag_name           Target release tag (e.g. v4.7.2, v4.7.2-$(date +%Y%m%d))"
    echo "                     If omitted, defaults to v4.7.2-YYYYMMDD."
    echo ""
    echo "Options:"
    echo "  --dry-run          Simulate tag creation and pushes without remote changes"
    echo "  --force            Force push tag updates to remotes"
    echo "  --help, -h         Show this help message"
    echo "============================================================================"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h)
            show_help
            ;;
        --dry-run)
            IS_DRY_RUN=true
            shift
            ;;
        --force)
            FORCE_PUSH=true
            shift
            ;;
        -*)
            echo "Unknown option: $1"
            show_help
            ;;
        *)
            if [[ -z "${TARGET_TAG}" ]]; then
                TARGET_TAG="$1"
            fi
            shift
            ;;
    esac
done

# 1. Resolve Tag Name
if [[ -z "${TARGET_TAG}" ]]; then
    TODAY_STR=$(date +%Y%m%d)
    TARGET_TAG="v4.7.2-${TODAY_STR}"
fi

if [[ "${TARGET_TAG}" != v* ]]; then
    TARGET_TAG="v${TARGET_TAG}"
fi

echo "============================================================================"
echo "[MindSCADA] One-Click Dual-Platform Release Publisher"
if [ "${IS_DRY_RUN}" = true ]; then
    echo "           *** DRY-RUN SIMULATION MODE ***"
fi
echo "============================================================================"
echo "Working Directory : ${SCRIPT_DIR}"
echo "Target Tag        : ${TARGET_TAG}"
echo "GitHub Remote     : ${GITHUB_REMOTE_URL}"
echo "Gitee Remote      : ${GITEE_REMOTE_URL}"
echo "============================================================================"

# 2. Check Git status
if [[ -n $(git status --porcelain) ]]; then
    echo "[WARNING] Uncommitted changes detected in working directory:"
    git status -s
    echo ""
    if [ "${IS_DRY_RUN}" = false ]; then
        echo "Please commit or stash local changes before publishing a release."
        exit 1
    fi
fi

# 3. Configure remotes
echo ""
echo "===> [Step 1/5] Verifying Remote Configurations..."
if ! git remote get-url origin >/dev/null 2>&1; then
    echo "Adding GitHub remote 'origin'..."
    git remote add origin "${GITHUB_REMOTE_URL}"
fi

if ! git remote get-url gitee >/dev/null 2>&1; then
    echo "Adding Gitee remote 'gitee'..."
    git remote add gitee "${GITEE_REMOTE_URL}"
else
    git remote set-url gitee "${GITEE_REMOTE_URL}"
fi
echo "[OK] Remotes configured: origin (GitHub) & gitee (Gitee)"

# 4. Create Git Tag
echo ""
echo "===> [Step 2/5] Creating Git Release Tag '${TARGET_TAG}'..."
if git rev-parse "${TARGET_TAG}" >/dev/null 2>&1; then
    echo "Tag '${TARGET_TAG}' already exists locally."
else
    if [ "${IS_DRY_RUN}" = true ]; then
        echo "[DRY-RUN] Would create tag: git tag -a ${TARGET_TAG} -m \"Release ${TARGET_TAG}\""
    else
        git tag -a "${TARGET_TAG}" -m "Release ${TARGET_TAG}"
        echo "[OK] Created git tag ${TARGET_TAG}"
    fi
fi

# 5. Push to GitHub
echo ""
echo "===> [Step 3/5] Pushing to GitHub (origin master & ${TARGET_TAG})..."
if [ "${IS_DRY_RUN}" = true ]; then
    echo "[DRY-RUN] Would execute: git push origin master"
    echo "[DRY-RUN] Would execute: git push origin ${TARGET_TAG}"
else
    git push origin master
    git push origin "${TARGET_TAG}"
    echo "[OK] Successfully pushed to GitHub! CI/CD workflow triggered."
fi

# 6. Push to Gitee
echo ""
echo "===> [Step 4/5] Pushing to Gitee (gitee master & ${TARGET_TAG})..."
if [ "${IS_DRY_RUN}" = true ]; then
    echo "[DRY-RUN] Would execute: git push gitee master --force"
    echo "[DRY-RUN] Would execute: git push gitee ${TARGET_TAG} --force"
else
    git push gitee master --force
    git push gitee "${TARGET_TAG}" --force
    echo "[OK] Successfully pushed to Gitee!"
fi

# 7. Check for local dist assets to upload to Gitee
echo ""
echo "===> [Step 5/5] Checking for Local Dist Assets to Upload to Gitee..."
if ls dist/*.tpz >/dev/null 2>&1; then
    if [[ -n "${GITEE_TOKEN}" ]]; then
        echo "Found local dist/ artifacts and GITEE_TOKEN is set."
        EXTRA_ARGS=()
        if [ "${IS_DRY_RUN}" = true ]; then
            EXTRA_ARGS+=("--dry-run")
        fi
        python scripts/gitee_release.py --tag "${TARGET_TAG}" --dist-dir dist "${EXTRA_ARGS[@]}"
    else
        echo "[INFO] Local dist/ packages found, but GITEE_TOKEN is not set in environment."
        echo "[INFO] GitHub Actions CI/CD will build all platforms and upload release assets."
        echo "[TIP]  To upload local dist/ assets to Gitee directly, run:"
        echo "       GITEE_TOKEN=your_token python scripts/gitee_release.py --tag ${TARGET_TAG}"
    fi
else
    echo "[INFO] No local dist/ packages found. GitHub Actions CI/CD will compile all 4 platforms."
fi

echo ""
echo "============================================================================"
echo "[SUCCESS] Release '${TARGET_TAG}' published to both GitHub and Gitee!"
echo "============================================================================"
echo "* GitHub Repository : ${GITHUB_REMOTE_URL}"
echo "* GitHub Actions CI : https://github.com/dedicate617/godot472/actions"
echo "* GitHub Releases   : https://github.com/dedicate617/godot472/releases/tag/${TARGET_TAG}"
echo "* Gitee Repository  : ${GITEE_REMOTE_URL}"
echo "* Gitee Releases    : https://gitee.com/bosmutus/mind-scada-release/releases"
echo "============================================================================"
