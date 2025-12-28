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

# Set SSH directory permissions
mkdir -p "${TARGET_DIR}/root/.ssh"
chmod 700 "${TARGET_DIR}/root/.ssh"
if [ -f "${TARGET_DIR}/root/.ssh/authorized_keys" ]; then
    chmod 600 "${TARGET_DIR}/root/.ssh/authorized_keys"
fi

# Create WiFi firmware symlink for STM32MP157F-DK2 (Murata 1DX / BCM43430)
if [ -d "${TARGET_DIR}/lib/firmware/brcm" ]; then
    ln -sf brcmfmac43430-sdio.MUR1DX.txt \
        "${TARGET_DIR}/lib/firmware/brcm/brcmfmac43430-sdio.st,stm32mp157f-dk2.txt"
    echo "Created WiFi firmware symlink for STM32MP157F-DK2"
fi

echo "SmartHub post-build script complete."
