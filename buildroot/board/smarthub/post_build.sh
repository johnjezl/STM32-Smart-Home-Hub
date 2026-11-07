#!/bin/bash
#
# post_build.sh - SmartHub Buildroot post-build script
#
# This script runs after Buildroot builds the target filesystem
# but before the final image is created.
#
# Arguments:
#   $1: Path to target filesystem (TARGET_DIR)
#

set -e

TARGET_DIR="$1"

echo "SmartHub post-build script running..."

# Create SmartHub directories
mkdir -p "${TARGET_DIR}/opt/smarthub"/{bin,config,data,logs}
mkdir -p "${TARGET_DIR}/var/lib/smarthub"
mkdir -p "${TARGET_DIR}/var/lib/mosquitto"
mkdir -p "${TARGET_DIR}/lib/firmware"

# Make init scripts executable
if [ -d "${TARGET_DIR}/etc/init.d" ]; then
    chmod 755 "${TARGET_DIR}/etc/init.d/S"* 2>/dev/null || true
fi

# Create mosquitto password file (empty - will be configured at runtime)
if [ -d "${TARGET_DIR}/etc/mosquitto" ]; then
    touch "${TARGET_DIR}/etc/mosquitto/passwd"
fi

# Logging in. Nothing is committed: a key and a password hash come from the person building, and
# without them the image still works, but has no login. See "Logging In" in docs/building.md.
mkdir -p "${TARGET_DIR}/root/.ssh"
chmod 700 "${TARGET_DIR}/root/.ssh"
# Decided on every build, as the target directory keeps what an earlier build put there
KEYS="${SMARTHUB_AUTHORIZED_KEYS:-$(dirname "$0")/overlay/root/.ssh/authorized_keys}"
if [ -f "$KEYS" ]; then
    cp "$KEYS" "${TARGET_DIR}/root/.ssh/authorized_keys"
    chmod 600 "${TARGET_DIR}/root/.ssh/authorized_keys"
else
    rm -f "${TARGET_DIR}/root/.ssh/authorized_keys"
    echo "WARNING: no SSH key in this image, so it can't be logged into over the network"
fi
# The defconfig's empty password would let anyone in at the console
sed -i "s|^root:[^:]*:|root:${SMARTHUB_ROOT_PASSWORD_HASH:-*}:|" "${TARGET_DIR}/etc/shadow"
if [ -z "${SMARTHUB_ROOT_PASSWORD_HASH:-}" ]; then
    echo "WARNING: no root password in this image, so it can't be logged into at the console"
fi

# Create WiFi firmware symlink for STM32MP157F-DK2 (Murata 1DX / BCM43430)
if [ -d "${TARGET_DIR}/lib/firmware/brcm" ]; then
    ln -sf brcmfmac43430-sdio.MUR1DX.txt \
        "${TARGET_DIR}/lib/firmware/brcm/brcmfmac43430-sdio.st,stm32mp157f-dk2.txt"
    echo "Created WiFi firmware symlink for STM32MP157F-DK2"
fi

echo "SmartHub post-build script complete."
