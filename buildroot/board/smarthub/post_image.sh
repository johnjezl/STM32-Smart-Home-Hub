#!/bin/bash
#
# post_image.sh - Buildroot post-image script
#
# This script runs after Buildroot creates the final filesystem images.
# Use it for creating SD card images, signing, etc.
#
# Arguments:
#   $1: Path to images directory (BINARIES_DIR)
#

set -e

BINARIES_DIR="$1"

echo "SmartHub post-image script running..."

# Create combined SD card image (if needed)
# genimage --config "${BR2_EXTERNAL_SMARTHUB_PATH}/board/smarthub/genimage.cfg" \
#          --inputpath "${BINARIES_DIR}" \
#          --outputpath "${BINARIES_DIR}"

echo "SmartHub post-image script complete."
echo "Images available in: ${BINARIES_DIR}"
