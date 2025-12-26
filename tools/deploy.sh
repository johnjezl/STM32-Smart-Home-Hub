#!/bin/bash
#
# deploy.sh - Deploy SmartHub application to target device
#
# Usage: ./deploy.sh [target-ip]
#

set -e

# Configuration
TARGET_IP="${1:-smarthub.local}"
TARGET_USER="root"
APP_NAME="smarthub"
BUILD_DIR="../build"
REMOTE_DIR="/opt/smarthub"

echo "=== SmartHub Deployment Script ==="
echo "Target: ${TARGET_USER}@${TARGET_IP}"

# Check if build exists
if [ ! -f "${BUILD_DIR}/app/${APP_NAME}" ]; then
    echo "Error: Application not found at ${BUILD_DIR}/app/${APP_NAME}"
    echo "Please build the application first with:"
    echo "  mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

# Create remote directory
echo "Creating remote directory..."
ssh "${TARGET_USER}@${TARGET_IP}" "mkdir -p ${REMOTE_DIR}"

# Stop existing service (if running)
echo "Stopping existing service..."
ssh "${TARGET_USER}@${TARGET_IP}" "systemctl stop smarthub 2>/dev/null || true"

# Copy application
echo "Copying application..."
scp "${BUILD_DIR}/app/${APP_NAME}" "${TARGET_USER}@${TARGET_IP}:${REMOTE_DIR}/"

# Copy assets
if [ -d "../app/assets" ]; then
    echo "Copying assets..."
    scp -r ../app/assets "${TARGET_USER}@${TARGET_IP}:${REMOTE_DIR}/"
fi

# Set permissions
ssh "${TARGET_USER}@${TARGET_IP}" "chmod +x ${REMOTE_DIR}/${APP_NAME}"

# Restart service
echo "Starting service..."
ssh "${TARGET_USER}@${TARGET_IP}" "systemctl start smarthub 2>/dev/null || ${REMOTE_DIR}/${APP_NAME} &"

echo "=== Deployment complete ==="
