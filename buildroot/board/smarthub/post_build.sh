#!/bin/bash
#
# post_build.sh - Buildroot post-build script
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

# Set up default configuration
if [ -f "${BR2_EXTERNAL_SMARTHUB_PATH}/board/smarthub/config/smarthub.yaml" ]; then
    cp "${BR2_EXTERNAL_SMARTHUB_PATH}/board/smarthub/config/smarthub.yaml" \
       "${TARGET_DIR}/opt/smarthub/config/"
fi

# Install systemd service
if [ -d "${TARGET_DIR}/etc/systemd/system" ]; then
    cat > "${TARGET_DIR}/etc/systemd/system/smarthub.service" << 'EOF'
[Unit]
Description=SmartHub Home Automation
After=network.target

[Service]
Type=simple
ExecStart=/opt/smarthub/bin/smarthub
WorkingDirectory=/opt/smarthub
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

    # Enable service
    mkdir -p "${TARGET_DIR}/etc/systemd/system/multi-user.target.wants"
    ln -sf ../smarthub.service \
       "${TARGET_DIR}/etc/systemd/system/multi-user.target.wants/smarthub.service"
fi

echo "SmartHub post-build script complete."
