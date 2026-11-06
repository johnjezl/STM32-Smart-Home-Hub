#!/bin/bash
#
# flash.sh - Flash SmartHub image to SD card
#
# Usage:
#   ./tools/flash.sh /dev/sdX      # Flash to a specific device
#   ./tools/flash.sh               # Use the one removable device, if there's only one
#
# It only writes to a whole, removable disk that isn't holding this system, and asks for the
# device's name to be typed back. Set SMARTHUB_IMAGE to flash a different image.
#

set -euo pipefail

BUILDROOT_DIR="$HOME/projects/smarthub/buildroot-src"
IMAGE="${SMARTHUB_IMAGE:-$BUILDROOT_DIR/output/images/sdcard.img}"
SYS_BLOCK="${SMARTHUB_SYS_BLOCK:-/sys/block}"

die() {
    echo "Error: $*" >&2
    exit 1
}

show_devices() {
    echo "Available devices:" >&2
    lsblk -d -o NAME,SIZE,RM,TYPE,MODEL,TRAN >&2 || true
}

# Removable by the kernel's account. An SD card in a built-in reader often says it isn't, so that
# counts too. An eMMC is also an mmcblk device, but its type says MMC.
is_removable() {
    local device="$1"
    local name
    name=$(basename "$device")
    [ "$(lsblk -dn -o RM "$device" | tr -d ' ')" = "1" ] && return 0
    [ "$(cat "$SYS_BLOCK/$name/device/type" 2>/dev/null || true)" = "SD" ]
}

# The disks this system is running from, through any partitions, RAID or LVM
system_disks() {
    local mountpoint source
    for mountpoint in / /boot /boot/efi /home /usr /var; do
        source=$(findmnt -n -o SOURCE "$mountpoint" 2>/dev/null || true)
        case "$source" in
            /dev/*) lsblk -nrs -o NAME,TYPE "${source%%\[*}" 2>/dev/null | awk '$2 == "disk" { print $1 }' ;;
        esac
    done
}

refuse_unless_safe() {
    local device="$1"
    local name type size
    name=$(basename "$device")

    [ -b "$device" ] || die "$device is not a block device"

    type=$(lsblk -dn -o TYPE "$device" | tr -d ' ')
    [ "$type" = "disk" ] || die "$device is a $type, not a whole disk"

    if system_disks | grep -qxF "$name"; then
        die "$device is holding this system"
    fi

    is_removable "$device" || die "$device is not removable"

    size=$(lsblk -bdn -o SIZE "$device" | tr -d ' ')
    [ "${size:-0}" -gt 0 ] || die "$device has no media in it"
    [ "$size" -ge "$(stat -L -c %s "$IMAGE")" ] || die "The image doesn't fit on $device"
}

# The one removable disk, if there's exactly one
detect_device() {
    local candidates=() name
    while read -r name; do
        if is_removable "/dev/$name" && [ "$(lsblk -bdn -o SIZE "/dev/$name" | tr -d ' ')" != "0" ]; then
            candidates+=("/dev/$name")
        fi
    done < <(lsblk -dn -o NAME,TYPE | awk '$2 == "disk" { print $1 }')

    if [ "${#candidates[@]}" -eq 0 ]; then
        show_devices
        die "No SD card detected. Usage: $0 /dev/sdX"
    fi
    if [ "${#candidates[@]}" -gt 1 ]; then
        show_devices
        die "More than one removable device (${candidates[*]}). Name the one to use: $0 /dev/sdX"
    fi
    echo "${candidates[0]}"
}

if [ ! -f "$IMAGE" ]; then
    echo "Error: Image not found: $IMAGE" >&2
    echo "Run ./tools/build.sh first" >&2
    exit 1
fi

if [ -n "${1:-}" ]; then
    DEVICE="$1"
else
    DEVICE=$(detect_device)
fi

refuse_unless_safe "$DEVICE"

echo "Target device: $DEVICE"
echo "Size and model: $(lsblk -dn -o SIZE,MODEL "$DEVICE")"
echo "Image: $IMAGE ($(du -h "$IMAGE" | cut -f1))"
echo ""
lsblk "$DEVICE" -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT || true
echo ""

echo "This will ERASE ALL DATA on $DEVICE."
read -r -p "Type the device name ($DEVICE) to continue: " confirm
if [ "$confirm" != "$DEVICE" ]; then
    echo "Aborted"
    exit 1
fi

echo "Unmounting partitions..."
while read -r mountpoint; do
    if [ -n "$mountpoint" ]; then
        sudo umount "$mountpoint"
    fi
done < <(lsblk -nr -o MOUNTPOINT "$DEVICE")

echo "Flashing..."
sudo dd if="$IMAGE" of="$DEVICE" bs=4M conv=fsync status=progress

echo "Syncing..."
sync

echo ""
echo "Flash complete!"
echo "Remove SD card and insert into STM32MP157F-DK2"
