#!/bin/bash
#
# setup.sh - Fetch the sources the image is built from, at the versions it's known to build with
#
# Usage:
#   ./tools/setup.sh
#
# They go next to this repository. Set SMARTHUB_BUILDROOT_DIR and SMARTHUB_ST_EXTERNAL to put
# them elsewhere. Running it again checks the versions, and moves to them if they've changed.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILDROOT_DIR="${SMARTHUB_BUILDROOT_DIR:-$(dirname "$PROJECT_DIR")/buildroot-src}"
ST_EXTERNAL="${SMARTHUB_ST_EXTERNAL:-$(dirname "$PROJECT_DIR")/buildroot-external-st}"

# Commits rather than names: st/2025.02.5 is a branch that keeps moving
BUILDROOT_URL="https://github.com/buildroot/buildroot.git"
BUILDROOT_COMMIT="745791cf6e"                 # Tag 2025.02.5
ST_EXTERNAL_URL="https://github.com/bootlin/buildroot-external-st.git"
ST_EXTERNAL_COMMIT="20f486a"                  # On branch st/2025.02.5

fetch_at() {
    local url="$1" dir="$2" commit="$3"
    if [ ! -d "$dir/.git" ]; then
        echo "Cloning $url into $dir"
        git clone "$url" "$dir"
    fi
    if [ "$(git -C "$dir" rev-parse HEAD)" != "$(git -C "$dir" rev-parse "$commit^{commit}" 2>/dev/null || true)" ]; then
        if [ -n "$(git -C "$dir" status --porcelain --untracked-files=no)" ]; then
            echo "Error: $dir has local changes, so it wasn't moved to $commit" >&2
            exit 1
        fi
        git -C "$dir" fetch origin
        git -C "$dir" checkout --detach "$commit"
    fi
    echo "$dir is at $(git -C "$dir" rev-parse --short=10 HEAD)"
}

fetch_at "$BUILDROOT_URL" "$BUILDROOT_DIR" "$BUILDROOT_COMMIT"
fetch_at "$ST_EXTERNAL_URL" "$ST_EXTERNAL" "$ST_EXTERNAL_COMMIT"

echo ""
echo "Ready. Build with ./tools/build.sh"
