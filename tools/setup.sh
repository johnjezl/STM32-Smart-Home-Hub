#!/bin/bash
#
# setup.sh - Fetch the sources the image is built from, at the versions it's known to build with
#
# Usage:
#   ./tools/setup.sh           # Fetch them, or move them to the right versions
#   ./tools/setup.sh --check   # Only check them, which build.sh does
#
# They go next to this repository. Set SMARTHUB_BUILDROOT_DIR and SMARTHUB_ST_EXTERNAL to put
# them elsewhere.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILDROOT_DIR="${SMARTHUB_BUILDROOT_DIR:-$(dirname "$PROJECT_DIR")/buildroot-src}"
ST_EXTERNAL="${SMARTHUB_ST_EXTERNAL:-$(dirname "$PROJECT_DIR")/buildroot-external-st}"

# Commits rather than names: st/2025.02.5 is a branch that keeps moving
BUILDROOT_URL="https://github.com/buildroot/buildroot.git"
BUILDROOT_COMMIT="745791cf6e117f7230f3e7d6bd759711f295358f"     # Tag 2025.02.5
ST_EXTERNAL_URL="https://github.com/bootlin/buildroot-external-st.git"
ST_EXTERNAL_COMMIT="20f486afdbcd35d2c27fac8ee535c3a9ee29e9b4"   # On branch st/2025.02.5

die() {
    echo "Error: $*" >&2
    exit 1
}

# Tracked files only: Buildroot's output and downloads are ignored, and it's edits that matter
has_local_changes() {
    [ -n "$(git -C "$1" status --porcelain --untracked-files=no)" ]
}

check() {
    local dir="$1" commit="$2"
    [ -d "$dir/.git" ] || die "$dir isn't there. Run ./tools/setup.sh first."
    [ "$(git -C "$dir" rev-parse HEAD)" = "$commit" ] ||
        die "$dir isn't at the version this builds with. Run ./tools/setup.sh."
    if has_local_changes "$dir"; then
        die "$dir has local changes, so what's built wouldn't be the pinned version"
    fi
}

fetch_at() {
    local url="$1" dir="$2" commit="$3"
    if [ ! -d "$dir/.git" ]; then
        echo "Cloning $url into $dir"
        git clone "$url" "$dir"
    fi
    if has_local_changes "$dir"; then
        die "$dir has local changes, so it wasn't checked against $commit"
    fi
    if [ "$(git -C "$dir" rev-parse HEAD)" != "$commit" ]; then
        if ! git -C "$dir" cat-file -e "$commit^{commit}" 2>/dev/null; then
            git -C "$dir" fetch origin
        fi
        git -C "$dir" checkout --detach "$commit"
        if [ -d "$dir/output" ]; then
            echo "Warning: $dir changed version, and has old build output. Run ./tools/build.sh rebuild." >&2
        fi
    fi
    echo "$dir is at $(git -C "$dir" rev-parse --short=10 HEAD)"
}

if [ "${1:-}" = "--check" ]; then
    check "$BUILDROOT_DIR" "$BUILDROOT_COMMIT"
    check "$ST_EXTERNAL" "$ST_EXTERNAL_COMMIT"
    exit 0
fi

fetch_at "$BUILDROOT_URL" "$BUILDROOT_DIR" "$BUILDROOT_COMMIT"
fetch_at "$ST_EXTERNAL_URL" "$ST_EXTERNAL" "$ST_EXTERNAL_COMMIT"

echo ""
echo "Ready. Build with ./tools/build.sh"
