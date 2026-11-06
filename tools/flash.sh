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

# The disks this system is running from, through any partitions, RAID or LVM. Dies if it can't tell.
system_disks() {
    local mountpoint source fstype ancestors
    for mountpoint in / /boot /boot/efi /home /usr /var; do
        while read -r source fstype; do
            case "$fstype" in
                tmpfs | autofs | overlay | squashfs | nfs | nfs4 | cifs | smb3 | fuse.*) continue ;;
            esac
            case "$source" in
                /dev/*)
                    ancestors=$(lsblk -nrs -o NAME,TYPE "${source%%\[*}" | awk '$2 == "disk" { print $1 }') ||
                        die "Can't tell which disk holds $mountpoint ($source)"
                    [ -n "$ancestors" ] || die "Can't tell which disk holds $mountpoint ($source)"
                    echo "$ancestors"
                    ;;
                *) die "Can't tell which disk holds $mountpoint ($source)" ;;
            esac
        done < <(findmnt -nr -o SOURCE,FSTYPE "$mountpoint" || true)
    done
}

# Where the device and its partitions are mounted, one to a line. List mode, as raw mode escapes
# spaces, and a card labelled "NO NAME" is common. Fails rather than say nothing is mounted, if
# this lsblk doesn't know the column.
mounts_of() {
    local mounts
    mounts=$(lsblk -nl -o MOUNTPOINTS "$1") || return 1
    sed '/^[[:space:]]*$/d' <<< "$mounts"
}

refuse_unless_safe() {
    local device="$1"
    local name type size disks mountpoint mounts
    name=$(basename "$device")

    [ -b "$device" ] || die "$device is not a block device"

    type=$(lsblk -dn -o TYPE "$device" | tr -d ' ')
    [ "$type" = "disk" ] || die "$device is a $type, not a whole disk"

    # Captured first: with pipefail, grep stopping at a match could read as there being no match
    disks=$(system_disks)
    if grep -qxF "$name" <<< "$disks"; then
        die "$device is holding this system"
    fi

    is_removable "$device" || die "$device is not removable"

    # Something's using it, like LVM or encryption
    if compgen -G "$SYS_BLOCK/$name/holders/*" > /dev/null ||
       compgen -G "$SYS_BLOCK/$name/*/holders/*" > /dev/null; then
        die "$device is in use by another device, such as LVM or encryption"
    fi

    # Mounted where a desktop mounts a card is fine, as it gets unmounted. Anywhere else, it's
    # probably something this system is using, however removable it is.
    mounts=$(mounts_of "$device") || die "Can't tell whether $device is mounted"
    while read -r mountpoint; do
        case "$mountpoint" in
            /media/* | /run/media/*) ;;
            "") ;;
            *) die "$device is in use, at $mountpoint" ;;
        esac
    done <<< "$mounts"

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

[ -f "$IMAGE" ] || die "Image not found: $IMAGE. Run ./tools/build.sh first"

if [ -n "${1:-}" ]; then
    DEVICE="$1"
else
    DEVICE=$(detect_device)
fi

# So that a name like /dev/disk/by-id/... is checked as the disk it really is
if [ -e "$DEVICE" ]; then
    DEVICE=$(readlink -f "$DEVICE")
fi

refuse_unless_safe "$DEVICE"

echo "Target device: $DEVICE"
echo "Size and model: $(lsblk -dn -o SIZE,MODEL "$DEVICE")"
echo "Image: $IMAGE ($(du -h --apparent-size "$IMAGE" | cut -f1))"
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
MOUNT_LIST=$(mounts_of "$DEVICE") || die "Can't tell whether $DEVICE is mounted"
mapfile -t MOUNTS <<< "$MOUNT_LIST"
for mountpoint in "${MOUNTS[@]}"; do
    if [ -n "$mountpoint" ]; then
        sudo umount "$mountpoint" || die "Couldn't unmount $mountpoint"
    fi
done
MOUNT_LIST=$(mounts_of "$DEVICE") || die "Can't tell whether $DEVICE is mounted"
[ -z "$MOUNT_LIST" ] || die "$DEVICE is still mounted"

echo "Flashing..."
sudo dd if="$IMAGE" of="$DEVICE" bs=4M conv=fsync status=progress

echo "Syncing..."
sync

echo ""
echo "Flash complete!"
echo "Remove SD card and insert into STM32MP157F-DK2"
