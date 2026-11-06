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
    "-nrs -o NAME,TYPE "*)    echo "$MOCK_ROOT_DISK disk" ;;
    "-dn -o NAME,TYPE")       echo "$MOCK_DISKS" | tr ',' '\n' | sed 's/$/ disk/' ;;
    "-nr -o MOUNTPOINT "*)    echo "$MOCK_MOUNTED" ;;
    *)                        echo "mock lsblk" ;;
esac
MOCK
cat > "$MOCKS/findmnt" <<'MOCK'
#!/bin/bash
[ "$4" = "/" ] && echo "/dev/rootpart"
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

echo "$BLOCK" | run MOCK_MOUNTED="/media/card boot" "$FLASH" "$BLOCK"
check "unmounts what's on the device, by mount point" 'grep -q "umount /media/card boot" "$TMP/log"'

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

echo "$BLOCK" | env PATH="$MOCKS:$PATH" MOCK_LOG="$TMP/log" SMARTHUB_IMAGE="$TMP/missing" "$FLASH" "$BLOCK" > "$TMP/out" 2>&1
check "refuses with no image" 'said "Image not found"'

echo ""
echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]
