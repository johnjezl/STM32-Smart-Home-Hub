#!/bin/bash
#
# build.sh - SmartHub Buildroot build script
#
# Usage:
#   ./tools/build.sh              # Full build
#   ./tools/build.sh menuconfig   # Configure
#   ./tools/build.sh clean        # Clean build
#   ./tools/build.sh rebuild      # Clean and rebuild
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILDROOT_DIR="${SMARTHUB_BUILDROOT_DIR:-$(dirname "$PROJECT_DIR")/buildroot-src}"
ST_EXTERNAL="${SMARTHUB_ST_EXTERNAL:-$(dirname "$PROJECT_DIR")/buildroot-external-st}"
SMARTHUB_EXTERNAL="$PROJECT_DIR/buildroot"

"$SCRIPT_DIR/setup.sh" --check

# Absolute, as this moves into Buildroot's directory, and they're used from there
BUILDROOT_DIR="$(realpath "$BUILDROOT_DIR")"
ST_EXTERNAL="$(realpath "$ST_EXTERNAL")"

cd "$BUILDROOT_DIR"

# Set up external trees
export BR2_EXTERNAL="$ST_EXTERNAL:$SMARTHUB_EXTERNAL"

case "${1:-build}" in
  config|defconfig)
    echo "Configuring SmartHub..."
    make smarthub_defconfig
    ;;
  menuconfig)
    echo "Opening menuconfig..."
    make menuconfig
    ;;
  savedefconfig)
    echo "Saving defconfig..."
    make savedefconfig BR2_DEFCONFIG="$SMARTHUB_EXTERNAL/configs/smarthub_defconfig"
    ;;
  clean)
    echo "Cleaning build..."
    make clean
    ;;
  rebuild)
    echo "Full rebuild..."
    make clean
    make smarthub_defconfig
    make -j$(nproc)
    ;;
  build|"")
    echo "Building SmartHub..."
    if [ ! -f .config ] || [ "$SMARTHUB_EXTERNAL/configs/smarthub_defconfig" -nt .config ]; then
        echo "No .config, or the defconfig has changed. Running defconfig first..."
        make smarthub_defconfig
    fi
    make -j$(nproc)
    ;;
  *)
    echo "Passing to make: $@"
    make "$@"
    ;;
esac

echo ""
echo "Build complete. Images in: $BUILDROOT_DIR/output/images/"
