#!/bin/bash
#
# flash.sh - Flash SmartHub image to SD card
#
# Usage:
#   ./tools/flash.sh /dev/sdX      # Flash to specific device
#   ./tools/flash.sh               # Auto-detect SD card
#

set -e

BUILDROOT_DIR="$HOME/projects/smarthub/buildroot-src"
IMAGE="$BUILDROOT_DIR/output/images/sdcard.img"

if [ ! -f "$IMAGE" ]; then
    echo "Error: Image not found: $IMAGE"
    echo "Run ./tools/build.sh first"
    exit 1
fi

# Determine target device
if [ -n "$1" ]; then
    DEVICE="$1"
else
    # Try to auto-detect SD card
    DEVICE=$(lsblk -d -o NAME,SIZE,MODEL,TRAN | grep -E "SD/MMC|Micro SD" | awk '{print "/dev/"$1}' | head -1)
    if [ -z "$DEVICE" ]; then
        echo "Error: No SD card detected"
        echo "Usage: $0 /dev/sdX"
        echo ""
        echo "Available devices:"
        lsblk -d -o NAME,SIZE,MODEL,TRAN
        exit 1
    fi
fi

echo "Target device: $DEVICE"
echo "Image: $IMAGE ($(ls -lh "$IMAGE" | awk '{print $5}'))"
echo ""
lsblk "$DEVICE" -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT 2>/dev/null || true
echo ""

read -p "Flash image to $DEVICE? This will ERASE ALL DATA! [y/N] " confirm
if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
    echo "Aborted"
    exit 1
fi

echo "Unmounting partitions..."
sudo umount ${DEVICE}* 2>/dev/null || true

echo "Flashing..."
sudo dd if="$IMAGE" of="$DEVICE" bs=4M conv=fsync status=progress

echo "Syncing..."
sync

echo ""
echo "Flash complete!"
echo "Remove SD card and insert into STM32MP157F-DK2"
