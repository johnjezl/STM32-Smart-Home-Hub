#!/bin/bash
#
# provision.sh - Initial device provisioning for SmartHub
#
# This script sets up a fresh STM32MP157F-DK2 board with the
# necessary configuration for SmartHub development.
#
# Usage: ./provision.sh [target-ip]
#

set -e

TARGET_IP="${1:-smarthub.local}"
TARGET_USER="root"

echo "=== SmartHub Device Provisioning ==="
echo "Target: ${TARGET_USER}@${TARGET_IP}"
echo ""
echo "This script will:"
echo "  1. Create smarthub directories"
echo "  2. Install systemd service"
echo "  3. Configure basic networking"
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
fi

# Create directories
echo "Creating directories..."
ssh "${TARGET_USER}@${TARGET_IP}" "mkdir -p /opt/smarthub/{bin,config,data,logs}"

# Create systemd service
echo "Installing systemd service..."
ssh "${TARGET_USER}@${TARGET_IP}" 'cat > /etc/systemd/system/smarthub.service << EOF
[Unit]
Description=SmartHub Home Automation
After=network.target

[Service]
Type=simple
ExecStart=/opt/smarthub/smarthub
WorkingDirectory=/opt/smarthub
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF'

# Enable service
ssh "${TARGET_USER}@${TARGET_IP}" "systemctl daemon-reload"
ssh "${TARGET_USER}@${TARGET_IP}" "systemctl enable smarthub"

# Set hostname
echo "Setting hostname..."
ssh "${TARGET_USER}@${TARGET_IP}" "hostnamectl set-hostname smarthub"

echo ""
echo "=== Provisioning complete ==="
echo ""
echo "Next steps:"
echo "  1. Build the application: cd build && cmake .. && make"
echo "  2. Deploy: ./tools/deploy.sh ${TARGET_IP}"
echo "  3. Check logs: ssh ${TARGET_USER}@${TARGET_IP} journalctl -u smarthub -f"
