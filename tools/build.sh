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
BUILDROOT_DIR="$HOME/projects/smarthub/buildroot-src"
ST_EXTERNAL="$HOME/projects/smarthub/buildroot-external-st"
SMARTHUB_EXTERNAL="$PROJECT_DIR/buildroot"

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
    if [ ! -f .config ]; then
        echo "No .config found, running defconfig first..."
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
