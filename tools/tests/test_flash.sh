#!/bin/bash
#
# Tests for flash.sh, with lsblk, findmnt, sudo and dd replaced, so nothing can be written to a disk.
#
# Run: tools/tests/test_flash.sh
#

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
FLASH="$HERE/../flash.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

MOCKS="$TMP/mocks"
mkdir -p "$MOCKS"

# The script requires a real block device, so use one of this machine's. Nothing is written to it.
BLOCK=$(find /dev -maxdepth 1 -type b 2>/dev/null | sort | head -1)
BLOCK_NAME=$(basename "${BLOCK:-none}")
OTHER_BLOCK=$(find /dev -maxdepth 1 -type b 2>/dev/null | sort | sed -n 2p)

# Set by each test: RM, TYPE, SIZE, ROOT_DISK, MOUNTED
cat > "$MOCKS/lsblk" <<'MOCK'
#!/bin/bash
case "$*" in
    "-dn -o RM "*)            echo "$MOCK_RM" ;;
    "-dn -o TYPE "*)          echo "$MOCK_TYPE" ;;
    "-bdn -o SIZE "*)         echo "$MOCK_SIZE" ;;
    "-dn -o SIZE,MODEL "*)    echo "1G Test Card" ;;
    "-nrs -o NAME,TYPE "*)    [ -z "${MOCK_ROOT_UNKNOWN:-}" ] || exit 1; echo "$MOCK_ROOT_DISK disk" ;;
    "-dn -o NAME,TYPE")       echo "$MOCK_DISKS" | tr ',' '\n' | sed 's/$/ disk/' ;;
    "-nl -o MOUNTPOINTS "*)
        [ -z "${MOCK_NO_MOUNTPOINTS:-}" ] || exit 1
        # Gone once unmounted, unless a test says it stays
        if [ -z "${MOCK_STAYS_MOUNTED:-}" ] && grep -q '^umount ' "$MOCK_LOG"; then exit 0; fi
        printf '%b\n' "$MOCK_MOUNTED" ;;
    *)                        echo "mock lsblk" ;;
esac
MOCK
cat > "$MOCKS/findmnt" <<'MOCK'
#!/bin/bash
case "${!#}" in
    /)    [ -z "${MOCK_FINDMNT_BROKEN:-}" ] || exit 1; echo "${MOCK_ROOT_SOURCE:-/dev/rootpart} ext4" ;;
    /var) [ -n "${MOCK_SEPARATE_VAR:-}" ] && echo "/dev/varpart ext4" ;;
    /home) [ -n "${MOCK_NFS_HOME:-}" ] && echo "server:/home nfs4" ;;
    /boot/efi) [ -n "${MOCK_AUTOMOUNT_EFI:-}" ] && printf 'systemd-1 autofs\n/dev/efipart vfat\n' ;;
esac
exit 0
MOCK
cat > "$MOCKS/sudo" <<'MOCK'
#!/bin/bash
"$@"
MOCK
cat > "$MOCKS/dd" <<'MOCK'
#!/bin/bash
echo "dd $*" >> "$MOCK_LOG"
MOCK
cat > "$MOCKS/umount" <<'MOCK'
#!/bin/bash
echo "umount $*" >> "$MOCK_LOG"
[ -z "${MOCK_UMOUNT_FAILS:-}" ]
MOCK
chmod +x "$MOCKS"/*

IMAGE="$TMP/image"
head -c 1024 /dev/zero > "$IMAGE"
SYS="$TMP/sys"
mkdir -p "$SYS"

passed=0
failed=0

# Runs flash.sh with the default mocks, overridden by NAME=value arguments before --
run() {
    : > "$TMP/log"
    env PATH="$MOCKS:$PATH" MOCK_LOG="$TMP/log" SMARTHUB_IMAGE="$IMAGE" SMARTHUB_SYS_BLOCK="$SYS" \
        MOCK_RM=1 MOCK_TYPE=disk MOCK_SIZE=1000000 MOCK_ROOT_DISK=rootdisk MOCK_DISKS="$BLOCK_NAME" MOCK_MOUNTED="" \
        "$@" > "$TMP/out" 2>&1
}

check() {
    local name="$1" condition="$2"
    if eval "$condition"; then
        passed=$((passed + 1))
        echo "  ok: $name"
    else
        failed=$((failed + 1))
        echo "  FAILED: $name"
        sed 's/^/    /' "$TMP/out"
    fi
}

wrote()   { grep -q '^dd ' "$TMP/log"; }
said()    { grep -qi "$1" "$TMP/out"; }

if [ -z "$BLOCK" ]; then
    echo "No block device on this machine to test with"
    exit 1
fi

# ---- What it refuses ---------------------------------------------------------

touch "$TMP/regular"
echo "$TMP/regular" | run "$FLASH" "$TMP/regular"
check "refuses a regular file" '! wrote && said "not a block device"'

echo "/dev/typo-does-not-exist" | run "$FLASH" /dev/typo-does-not-exist
check "refuses a mistyped path" '! wrote && said "not a block device"'
check "doesn't create a file for a mistyped path" '[ ! -e /dev/typo-does-not-exist ]'

echo /dev/null | run "$FLASH" /dev/null
check "refuses a character device" '! wrote && said "not a block device"'

echo "$BLOCK" | run MOCK_TYPE=part "$FLASH" "$BLOCK"
check "refuses a partition" '! wrote && said "not a whole disk"'

echo "$BLOCK" | run MOCK_RM=0 "$FLASH" "$BLOCK"
check "refuses a disk that isn't removable" '! wrote && said "not removable"'

echo "$BLOCK" | run MOCK_ROOT_DISK="$BLOCK_NAME" "$FLASH" "$BLOCK"
check "refuses the root disk, even if it says it's removable" '! wrote && said "holding this system"'

echo "$BLOCK" | run MOCK_SIZE=0 "$FLASH" "$BLOCK"
check "refuses a reader with no card" '! wrote && said "no media"'

echo "$BLOCK" | run MOCK_SIZE=100 "$FLASH" "$BLOCK"
check "refuses a card the image doesn't fit on" '! wrote && said "fit"'

echo "$BLOCK" | run MOCK_ROOT_DISK="$BLOCK_NAME" MOCK_SEPARATE_VAR=1 "$FLASH" "$BLOCK"
check "still sees the root disk when /var is separate" '! wrote && said "holding this system"'

echo "$BLOCK" | run MOCK_ROOT_SOURCE="rpool/ROOT/x" "$FLASH" "$BLOCK"
check "stops if it can't tell which disk is the root" '! wrote && said "Can.t tell"'

echo "$BLOCK" | run MOCK_FINDMNT_BROKEN=1 "$FLASH" "$BLOCK"
check "stops if findmnt can't see the root" '! wrote && said "Can.t tell"'

echo "$BLOCK" | run MOCK_ROOT_UNKNOWN=1 "$FLASH" "$BLOCK"
check "stops if lsblk can't say what the root is on" '! wrote && said "Can.t tell"'

echo "$BLOCK" | run MOCK_NO_MOUNTPOINTS=1 "$FLASH" "$BLOCK"
check "stops if it can't tell what's mounted" '! wrote && said "whether .* is mounted"'

echo "$BLOCK" | run MOCK_NFS_HOME=1 "$FLASH" "$BLOCK"
check "isn't stopped by a network /home" 'wrote'

echo "$BLOCK" | run MOCK_AUTOMOUNT_EFI=1 "$FLASH" "$BLOCK"
check "isn't stopped by an automount placeholder" 'wrote'

ln -s "$BLOCK" "$TMP/alias"
echo "$BLOCK" | run MOCK_ROOT_DISK="$BLOCK_NAME" "$FLASH" "$TMP/alias"
check "sees through a name like /dev/disk/by-id to the disk" '! wrote && said "holding this system"'

echo "$BLOCK" | run "$FLASH" "$TMP/alias"
check "writes to the real device, and asks for its real name" 'wrote && grep -q "of=$BLOCK " "$TMP/log"'

# ---- Confirmation ------------------------------------------------------------

echo "y" | run "$FLASH" "$BLOCK"
check "y isn't enough" '! wrote && said "aborted"'

echo "" | run "$FLASH" "$BLOCK"
check "nothing typed isn't enough" '! wrote'

run "$FLASH" "$BLOCK" < /dev/null
check "no terminal isn't enough" '! wrote'

echo "$BLOCK" | run "$FLASH" "$BLOCK"
check "writes when the name is typed back" 'wrote && grep -q "of=$BLOCK " "$TMP/log"'
check "shows size and model" 'said "1G Test Card"'

# ---- Built-in readers --------------------------------------------------------

mkdir -p "$SYS/$BLOCK_NAME/device"
echo SD > "$SYS/$BLOCK_NAME/device/type"
echo "$BLOCK" | run MOCK_RM=0 "$FLASH" "$BLOCK"
check "accepts an SD card in a built-in reader" 'wrote'

echo MMC > "$SYS/$BLOCK_NAME/device/type"
echo "$BLOCK" | run MOCK_RM=0 "$FLASH" "$BLOCK"
check "refuses an eMMC" '! wrote && said "not removable"'
rm -rf "${SYS:?}/$BLOCK_NAME"

# ---- Unmounting --------------------------------------------------------------

echo "$BLOCK" | run MOCK_MOUNTED="/media/john/NO NAME\\n/run/media/john/rootfs" "$FLASH" "$BLOCK"
check "unmounts everything on the device, spaces and all" \
    'wrote && grep -q "umount /media/john/NO NAME" "$TMP/log" && grep -q "umount /run/media/john/rootfs" "$TMP/log"'

echo "$BLOCK" | run MOCK_MOUNTED="/media/card" MOCK_UMOUNT_FAILS=1 "$FLASH" "$BLOCK"
check "stops with a reason if it can't unmount" '! wrote && said "unmount /media/card"'

echo "$BLOCK" | run MOCK_MOUNTED="/media/card" MOCK_STAYS_MOUNTED=1 "$FLASH" "$BLOCK"
check "won't write to what's still mounted" '! wrote && said "still mounted"'

echo "$BLOCK" | run MOCK_MOUNTED="[SWAP]" "$FLASH" "$BLOCK"
check "refuses a device being used for swap" '! wrote && said "in use"'

echo "$BLOCK" | run MOCK_MOUNTED="/opt/data" "$FLASH" "$BLOCK"
check "refuses a device mounted somewhere a card wouldn't be" '! wrote && said "in use, at /opt/data"'

mkdir -p "$SYS/$BLOCK_NAME/holders/dm-0"
echo "$BLOCK" | run "$FLASH" "$BLOCK"
check "refuses a device LVM or encryption is using" '! wrote && said "in use by another"'
rm -rf "${SYS:?}/$BLOCK_NAME"
mkdir -p "$SYS/$BLOCK_NAME/part1/holders/dm-0"
echo "$BLOCK" | run "$FLASH" "$BLOCK"
check "refuses one whose partition is in use that way" '! wrote && said "in use by another"'
rm -rf "${SYS:?}/$BLOCK_NAME"

# ---- Choosing a device -------------------------------------------------------

echo "$BLOCK" | run "$FLASH"
check "uses the one removable device" 'wrote && grep -q "of=$BLOCK " "$TMP/log"'

run MOCK_RM=0 "$FLASH" < /dev/null
check "says when there's none" '! wrote && said "No SD card detected"'

if [ -n "$OTHER_BLOCK" ]; then
    run MOCK_DISKS="$BLOCK_NAME,$(basename "$OTHER_BLOCK")" "$FLASH" < /dev/null
    check "won't guess between two" '! wrote && said "More than one"'
fi

# ---- No image ----------------------------------------------------------------

: > "$TMP/log"
echo "$BLOCK" | env PATH="$MOCKS:$PATH" MOCK_LOG="$TMP/log" SMARTHUB_IMAGE="$TMP/missing" "$FLASH" "$BLOCK" > "$TMP/out" 2>&1
check "refuses with no image" '! wrote && said "Image not found"'

echo ""
echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]
