# Phase 1: Buildroot Base System - Detailed TODO

## Overview
**Duration**: 1-2 weeks  
**Objective**: Create minimal, optimized Linux image tailored for the smart home hub.  
**Prerequisites**: Phase 0 complete

---

## 1.1 Buildroot Environment Setup

### 1.1.1 Clone Buildroot
```bash
cd ~/projects/smarthub
git clone https://github.com/buildroot/buildroot.git buildroot-src
cd buildroot-src
git checkout 2025.02.5  # Use LTS release
```

### 1.1.2 Clone STM32MP1 External Tree
```bash
cd ~/projects/smarthub
git clone https://github.com/bootlin/buildroot-external-st.git
cd buildroot-external-st
git checkout st/2025.02.5  # Match Buildroot version
```

### 1.1.3 Initial Configuration
- ✅ Set up external tree:
```bash
cd ~/projects/smarthub/buildroot-src
make BR2_EXTERNAL=../buildroot-external-st st_stm32mp157f_dk2_defconfig
```
- ✅ Verify configuration: — *Used menuconfig to review and customize*
```bash
make menuconfig
# Browse to see default settings - DO NOT change yet
# Exit without saving
```

### 1.1.4 Directory Structure for Custom External Tree
- ✅ Create project-specific external tree: — *Created in STM32-Smart-Home-Hub/buildroot/*
```bash
mkdir -p ~/projects/smarthub/buildroot
cd ~/projects/smarthub/buildroot

mkdir -p board/smarthub/stm32mp157f-dk2
mkdir -p configs
mkdir -p package/smarthub
mkdir -p patches

# Create external.desc
cat > external.desc << 'EOF'
name: SMARTHUB
desc: Smart Home Hub customizations for STM32MP157F-DK2
EOF

# Create external.mk
cat > external.mk << 'EOF'
include $(sort $(wildcard $(BR2_EXTERNAL_SMARTHUB_PATH)/package/*/*.mk))
EOF

# Create Config.in
cat > Config.in << 'EOF'
source "$BR2_EXTERNAL_SMARTHUB_PATH/package/smarthub/Config.in"
EOF
```

---

## 1.2 Kernel Configuration

### 1.2.1 Base Kernel Config
- ✅ Start with ST's proven configuration — *Using stock ST multi_v7 defconfig*
- ⏸️ Create kernel config fragment: — *Deferred, using ST defaults*
```bash
mkdir -p ~/projects/smarthub/buildroot/board/smarthub/stm32mp157f-dk2

cat > board/smarthub/stm32mp157f-dk2/linux.fragment << 'EOF'
# WiFi support (required)
CONFIG_WLAN=y
CONFIG_CFG80211=y
CONFIG_MAC80211=y
CONFIG_BRCMUTIL=y
CONFIG_BRCMFMAC=y
CONFIG_BRCMFMAC_SDIO=y

# Bluetooth support
CONFIG_BT=y
CONFIG_BT_HCIUART=y
CONFIG_BT_HCIUART_BCM=y

# OpenAMP / Remoteproc for M4 communication
CONFIG_REMOTEPROC=y
CONFIG_STM32_RPROC=y
CONFIG_RPMSG_CHAR=y
CONFIG_RPMSG_CTRL=y
CONFIG_RPMSG_TTY=y

# Display support
CONFIG_DRM=y
CONFIG_DRM_STM=y
CONFIG_DRM_STM_DSI=y
CONFIG_DRM_PANEL_ORISETECH_OTM8009A=y

# Touchscreen
CONFIG_INPUT_TOUCHSCREEN=y
CONFIG_TOUCHSCREEN_EDT_FT5X06=y

# I2C for sensors
CONFIG_I2C=y
CONFIG_I2C_CHARDEV=y
CONFIG_I2C_STM32F7=y

# SPI
CONFIG_SPI=y
CONFIG_SPI_STM32=y
CONFIG_SPI_SPIDEV=y

# UART
CONFIG_SERIAL_STM32=y
CONFIG_SERIAL_STM32_CONSOLE=y

# ADC
CONFIG_IIO=y
CONFIG_STM32_ADC_CORE=y
CONFIG_STM32_ADC=y

# GPIO
CONFIG_GPIO_SYSFS=y
CONFIG_GPIO_STM32=y

# Crypto hardware acceleration
CONFIG_CRYPTO_DEV_STM32_CRYP=y
CONFIG_CRYPTO_DEV_STM32_HASH=y
CONFIG_HW_RANDOM_STM32=y

# Watchdog
CONFIG_WATCHDOG=y
CONFIG_STM32_WATCHDOG=y

# Remove unused (save space)
# CONFIG_USB_STORAGE is not set
# CONFIG_SOUND is not set
# CONFIG_SND is not set
# CONFIG_VIDEO_DEV is not set
EOF
```

### 1.2.2 Kernel Optimization
- ⏸️ Create boot optimization fragment: — *Deferred to Phase 6 (optimization)*
```bash
cat > board/smarthub/stm32mp157f-dk2/linux-fastboot.fragment << 'EOF'
# Faster boot
CONFIG_PRINTK_TIME=y
# CONFIG_DEBUG_KERNEL is not set
# CONFIG_FTRACE is not set
# CONFIG_DYNAMIC_DEBUG is not set
CONFIG_CC_OPTIMIZE_FOR_SIZE=y
CONFIG_SLOB=y
EOF
```

---

## 1.3 Buildroot Custom Configuration

### 1.3.1 Create SmartHub defconfig
```bash
cat > ~/projects/smarthub/buildroot/configs/smarthub_defconfig << 'EOF'
# Architecture
BR2_arm=y
BR2_cortex_a7=y
BR2_ARM_FPU_NEON_VFPV4=y
BR2_ARM_INSTRUCTIONS_THUMB2=y

# Toolchain
BR2_TOOLCHAIN_EXTERNAL=y
BR2_TOOLCHAIN_EXTERNAL_BOOTLIN=y
BR2_TOOLCHAIN_EXTERNAL_BOOTLIN_ARMV7_EABIHF_GLIBC_STABLE=y

# Use musl for smaller footprint (alternative)
# BR2_TOOLCHAIN_EXTERNAL_CUSTOM=y
# BR2_TOOLCHAIN_EXTERNAL_CUSTOM_MUSL=y

# System
BR2_SYSTEM_DHCP="eth0"
BR2_TARGET_GENERIC_HOSTNAME="smarthub"
BR2_TARGET_GENERIC_ISSUE="Smart Home Hub"
BR2_ROOTFS_OVERLAY="$(BR2_EXTERNAL_SMARTHUB_PATH)/board/smarthub/stm32mp157f-dk2/overlay"
BR2_ROOTFS_POST_BUILD_SCRIPT="$(BR2_EXTERNAL_SMARTHUB_PATH)/board/smarthub/stm32mp157f-dk2/post_build.sh"
BR2_ROOTFS_POST_IMAGE_SCRIPT="$(BR2_EXTERNAL_SMARTHUB_PATH)/board/smarthub/stm32mp157f-dk2/post_image.sh"

# Kernel
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_CUSTOM_VERSION=y
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="6.6.20"
BR2_LINUX_KERNEL_USE_CUSTOM_CONFIG=y
BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="$(BR2_EXTERNAL_SMARTHUB_PATH)/board/smarthub/stm32mp157f-dk2/linux.config"
BR2_LINUX_KERNEL_CONFIG_FRAGMENT_FILES="$(BR2_EXTERNAL_SMARTHUB_PATH)/board/smarthub/stm32mp157f-dk2/linux.fragment"
BR2_LINUX_KERNEL_DTS_SUPPORT=y
BR2_LINUX_KERNEL_INTREE_DTS_NAME="stm32mp157f-dk2"
BR2_LINUX_KERNEL_INSTALL_TARGET=y

# Bootloader
BR2_TARGET_ARM_TRUSTED_FIRMWARE=y
BR2_TARGET_ARM_TRUSTED_FIRMWARE_CUSTOM_VERSION=y
BR2_TARGET_ARM_TRUSTED_FIRMWARE_CUSTOM_VERSION_VALUE="v2.10"
BR2_TARGET_ARM_TRUSTED_FIRMWARE_PLATFORM="stm32mp1"
BR2_TARGET_ARM_TRUSTED_FIRMWARE_FIP=y
BR2_TARGET_ARM_TRUSTED_FIRMWARE_BL32_OPTEE=y

BR2_TARGET_OPTEE_OS=y
BR2_TARGET_OPTEE_OS_CUSTOM_VERSION=y
BR2_TARGET_OPTEE_OS_CUSTOM_VERSION_VALUE="4.1.0"
BR2_TARGET_OPTEE_OS_PLATFORM="stm32mp1"

BR2_TARGET_UBOOT=y
BR2_TARGET_UBOOT_BUILD_SYSTEM_KCONFIG=y
BR2_TARGET_UBOOT_CUSTOM_VERSION=y
BR2_TARGET_UBOOT_CUSTOM_VERSION_VALUE="2024.01"
BR2_TARGET_UBOOT_BOARD_DEFCONFIG="stm32mp15_trusted"
BR2_TARGET_UBOOT_NEEDS_DTC=y
BR2_TARGET_UBOOT_NEEDS_OPENSSL=y
BR2_TARGET_UBOOT_FORMAT_DTB=y
BR2_TARGET_UBOOT_FORMAT_CUSTOM=y
BR2_TARGET_UBOOT_FORMAT_CUSTOM_NAME="u-boot-nodtb.bin"

# Filesystem
BR2_TARGET_ROOTFS_EXT2=y
BR2_TARGET_ROOTFS_EXT2_4=y
BR2_TARGET_ROOTFS_EXT2_SIZE="512M"

# Init system - BusyBox init for fast boot
BR2_INIT_BUSYBOX=y

# Core packages
BR2_PACKAGE_BUSYBOX_SHOW_OTHERS=y
BR2_PACKAGE_BUSYBOX_WATCHDOG=y

# Networking
BR2_PACKAGE_WPA_SUPPLICANT=y
BR2_PACKAGE_WPA_SUPPLICANT_NL80211=y
BR2_PACKAGE_WPA_SUPPLICANT_AP_SUPPORT=y
BR2_PACKAGE_WPA_SUPPLICANT_PASSPHRASE=y
BR2_PACKAGE_DHCPCD=y
BR2_PACKAGE_IPROUTE2=y
BR2_PACKAGE_IPTABLES=y
BR2_PACKAGE_AVAHI=y
BR2_PACKAGE_AVAHI_DAEMON=y
BR2_PACKAGE_AVAHI_LIBDNSSD_COMPATIBILITY=y

# C++ support
BR2_PACKAGE_LIBCXX=y

# Database
BR2_PACKAGE_SQLITE=y
BR2_PACKAGE_SQLITE_ENABLE_COLUMN_METADATA=y

# Crypto / TLS
BR2_PACKAGE_OPENSSL=y
BR2_PACKAGE_CA_CERTIFICATES=y

# MQTT
BR2_PACKAGE_MOSQUITTO=y
BR2_PACKAGE_MOSQUITTO_BROKER=y

# JSON
BR2_PACKAGE_NLOHMANN_JSON=y

# YAML
BR2_PACKAGE_LIBYAML=y

# Graphics
BR2_PACKAGE_LIBDRM=y
BR2_PACKAGE_LIBDRM_INSTALL_TESTS=y

# Debugging (can disable for production)
BR2_PACKAGE_STRACE=y
BR2_PACKAGE_GDB=y
BR2_PACKAGE_GDB_DEBUGGER=y

# Firmware
BR2_PACKAGE_LINUX_FIRMWARE=y
BR2_PACKAGE_LINUX_FIRMWARE_BRCM_BCM43XX=y

# OpenAMP for M4 communication
BR2_PACKAGE_OPENAMP=y
BR2_PACKAGE_LIBMETAL=y
EOF
```

### 1.3.2 Create Root Filesystem Overlay
```bash
mkdir -p ~/projects/smarthub/buildroot/board/smarthub/stm32mp157f-dk2/overlay

# Create directory structure
cd ~/projects/smarthub/buildroot/board/smarthub/stm32mp157f-dk2/overlay
mkdir -p etc/init.d
mkdir -p lib/firmware
mkdir -p opt/smarthub
mkdir -p var/lib/smarthub
```

### 1.3.3 BusyBox Init Scripts
```bash
# Network initialization
cat > overlay/etc/init.d/S40network << 'EOF'
#!/bin/sh

case "$1" in
  start)
    echo "Starting network..."
    
    # Bring up loopback
    ip link set lo up
    
    # Bring up ethernet
    ip link set eth0 up
    dhcpcd -b eth0
    
    # Start wpa_supplicant for WiFi
    if [ -f /etc/wpa_supplicant/wpa_supplicant.conf ]; then
        wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
        sleep 2
        dhcpcd -b wlan0
    fi
    ;;
  stop)
    echo "Stopping network..."
    killall wpa_supplicant 2>/dev/null
    killall dhcpcd 2>/dev/null
    ip link set wlan0 down
    ip link set eth0 down
    ;;
  *)
    echo "Usage: $0 {start|stop}"
    exit 1
esac

exit 0
EOF
chmod +x overlay/etc/init.d/S40network

# MQTT broker
cat > overlay/etc/init.d/S50mosquitto << 'EOF'
#!/bin/sh

DAEMON=/usr/sbin/mosquitto
CONF=/etc/mosquitto/mosquitto.conf
PIDFILE=/var/run/mosquitto.pid

case "$1" in
  start)
    echo "Starting Mosquitto MQTT broker..."
    start-stop-daemon -S -q -p $PIDFILE --exec $DAEMON -- -d -c $CONF
    ;;
  stop)
    echo "Stopping Mosquitto..."
    start-stop-daemon -K -q -p $PIDFILE
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
esac
EOF
chmod +x overlay/etc/init.d/S50mosquitto

# M4 firmware loader
cat > overlay/etc/init.d/S60remoteproc << 'EOF'
#!/bin/sh

M4_FIRMWARE="smarthub_m4.elf"
RPROC_PATH="/sys/class/remoteproc/remoteproc0"

case "$1" in
  start)
    echo "Loading M4 firmware..."
    if [ -f "/lib/firmware/$M4_FIRMWARE" ]; then
        echo "$M4_FIRMWARE" > $RPROC_PATH/firmware
        echo start > $RPROC_PATH/state
        echo "M4 firmware started"
    else
        echo "M4 firmware not found, skipping"
    fi
    ;;
  stop)
    echo "Stopping M4 firmware..."
    echo stop > $RPROC_PATH/state 2>/dev/null
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  status)
    cat $RPROC_PATH/state
    ;;
  *)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac
EOF
chmod +x overlay/etc/init.d/S60remoteproc

# Main application
cat > overlay/etc/init.d/S90smarthub << 'EOF'
#!/bin/sh

DAEMON=/opt/smarthub/smarthub
PIDFILE=/var/run/smarthub.pid
LOGFILE=/var/log/smarthub.log

case "$1" in
  start)
    echo "Starting SmartHub..."
    if [ -x $DAEMON ]; then
        start-stop-daemon -S -b -m -p $PIDFILE --exec $DAEMON -- >> $LOGFILE 2>&1
    else
        echo "SmartHub not installed"
    fi
    ;;
  stop)
    echo "Stopping SmartHub..."
    start-stop-daemon -K -q -p $PIDFILE
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
esac
EOF
chmod +x overlay/etc/init.d/S90smarthub
```

### 1.3.4 Mosquitto Configuration
```bash
mkdir -p overlay/etc/mosquitto
cat > overlay/etc/mosquitto/mosquitto.conf << 'EOF'
# SmartHub MQTT Broker Configuration

# Persistence
persistence true
persistence_location /var/lib/mosquitto/

# Logging
log_dest stderr
log_type error
log_type warning
log_type information

# Local listener (no auth for internal use)
listener 1883 127.0.0.1
allow_anonymous true

# Network listener (with auth)
listener 1883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/passwd

# WebSocket support (for web UI)
listener 9001
protocol websockets
allow_anonymous false
password_file /etc/mosquitto/passwd
EOF
```

### 1.3.5 WPA Supplicant Template
```bash
mkdir -p overlay/etc/wpa_supplicant
cat > overlay/etc/wpa_supplicant/wpa_supplicant.conf << 'EOF'
ctrl_interface=/var/run/wpa_supplicant
ctrl_interface_group=0
update_config=1
country=US

# Networks will be added at runtime
EOF
```

---

## 1.4 Build Scripts

### 1.4.1 Post-Build Script
```bash
cat > ~/projects/smarthub/buildroot/board/smarthub/stm32mp157f-dk2/post_build.sh << 'EOF'
#!/bin/bash
# Post-build script for SmartHub

TARGET_DIR=$1

# Create necessary directories
mkdir -p $TARGET_DIR/var/lib/smarthub
mkdir -p $TARGET_DIR/var/lib/mosquitto
mkdir -p $TARGET_DIR/var/log

# Set permissions
chmod 755 $TARGET_DIR/etc/init.d/*

# Create empty passwd file for mosquitto
touch $TARGET_DIR/etc/mosquitto/passwd

echo "Post-build complete"
EOF
chmod +x ~/projects/smarthub/buildroot/board/smarthub/stm32mp157f-dk2/post_build.sh
```

### 1.4.2 Post-Image Script
```bash
cat > ~/projects/smarthub/buildroot/board/smarthub/stm32mp157f-dk2/post_image.sh << 'EOF'
#!/bin/bash
# Post-image script - creates SD card image

BOARD_DIR="$(dirname $0)"
GENIMAGE_CFG="${BOARD_DIR}/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

rm -rf "${GENIMAGE_TMP}"

genimage \
    --rootpath "${TARGET_DIR}" \
    --tmppath "${GENIMAGE_TMP}" \
    --inputpath "${BINARIES_DIR}" \
    --outputpath "${BINARIES_DIR}" \
    --config "${GENIMAGE_CFG}"

echo "SD card image created: ${BINARIES_DIR}/sdcard.img"
EOF
chmod +x ~/projects/smarthub/buildroot/board/smarthub/stm32mp157f-dk2/post_image.sh
```

### 1.4.3 Genimage Configuration
```bash
cat > ~/projects/smarthub/buildroot/board/smarthub/stm32mp157f-dk2/genimage.cfg << 'EOF'
image boot.vfat {
    vfat {
        files = {
            "zImage",
            "stm32mp157f-dk2.dtb"
        }
    }
    size = 64M
}

image sdcard.img {
    hdimage {
        partition-table-type = "gpt"
    }

    partition fsbl1 {
        image = "tf-a-stm32mp157f-dk2.stm32"
        offset = 34K
        size = 256K
    }

    partition fsbl2 {
        image = "tf-a-stm32mp157f-dk2.stm32"
        size = 256K
    }

    partition fip {
        image = "fip.bin"
        size = 4M
    }

    partition boot {
        image = "boot.vfat"
        partition-type-uuid = C12A7328-F81F-11D2-BA4B-00A0C93EC93B
        bootable = true
        size = 64M
    }

    partition rootfs {
        image = "rootfs.ext4"
        partition-type-uuid = 0FC63DAF-8483-4772-8E79-3D69D8477DE4
        size = 512M
    }

    partition data {
        partition-type-uuid = 0FC63DAF-8483-4772-8E79-3D69D8477DE4
        size = 512M
    }
}
EOF
```

---

## 1.5 Build and Test

### 1.5.1 Initial Build (Stock ST Configuration)

First, verify the toolchain works with the stock ST defconfig:

```bash
cd ~/projects/smarthub/buildroot-src

# Use stock ST defconfig for initial testing
make BR2_EXTERNAL=../buildroot-external-st st_stm32mp157f_dk2_defconfig

# Build (takes 30-60 minutes first time)
make -j$(nproc)

# Output will be in output/images/
ls output/images/
```

### 1.5.2 Custom SmartHub Build (After Custom External Tree Created)

Once the custom external tree is set up (section 1.1.4 - 1.4):

```bash
cd ~/projects/smarthub/buildroot-src

# Configure with both external trees
make BR2_EXTERNAL="../buildroot-external-st:../buildroot" smarthub_defconfig

# Optional: Review/modify configuration
make menuconfig

# Build
make -j$(nproc)
```

### 1.5.2 Flash to SD Card
```bash
cd ~/projects/smarthub/buildroot-src/output/images

# CAREFUL: Verify correct device!
lsblk

# Flash
sudo dd if=sdcard.img of=/dev/sdX bs=8M conv=fsync status=progress
sync
```

### 1.5.3 First Boot Verification
- ✅ Insert SD card and boot board
- ✅ Connect serial console — */dev/ttyACM0 at 115200 baud*
- ✅ Measure boot time (target: <10 seconds) — *Actual: ~8 seconds*
- ✅ Verify login works — *Root login via serial, SSH with key auth*
- ✅ Check networking: — *eth0: 192.168.4.102, wlan0: 192.168.4.99*
```bash
ip addr
ping -c 3 google.com  # If internet available
```
- ✅ Check Mosquitto running: — *Running on port 1883, pub/sub tested*
```bash
ps aux | grep mosquitto
mosquitto_pub -h localhost -t test -m "hello"
```
- ✅ Verify remoteproc available: — */sys/class/remoteproc/remoteproc0 present*
```bash
ls /sys/class/remoteproc/
```

### 1.5.4 Memory Usage Check
```bash
free -h
# Target: <64 MB used at idle
```

### 1.5.5 Boot Time Measurement
```bash
# Add to kernel cmdline for detailed timing
# In U-Boot: setenv bootargs "${bootargs} printk.time=1 quiet"

# Or measure from serial console timestamp
dmesg | head -50
dmesg | tail -10
```

---

## 1.6 Boot Optimization

> **Note:** Boot optimization deferred to Phase 6. Current boot time (~8s) meets target.

### 1.6.1 Kernel Command Line
- ⏸️ Add `quiet` to reduce boot messages — *Deferred to Phase 6*
- ⏸️ Add `loglevel=3` to suppress non-critical messages — *Deferred to Phase 6*
- ⏸️ Consider `rootwait` timeout adjustment — *Deferred to Phase 6*

### 1.6.2 Init Script Optimization
- ⏸️ Parallelize network startup — *Deferred to Phase 6*
- ⏸️ Defer non-critical services — *Deferred to Phase 6*
- ⏸️ Use background initialization where possible — *Deferred to Phase 6*

### 1.6.3 Filesystem Optimization
- ⏸️ Consider SquashFS + overlayfs for read-only root — *Deferred to Phase 6*
- ⏸️ Move logs to tmpfs — *Deferred to Phase 6*
```bash
# Add to fstab
tmpfs /var/log tmpfs defaults,noatime,nosuid,mode=0755,size=8m 0 0
```

---

## 1.7 Development Conveniences

### 1.7.1 SSH Access
- ✅ Dropbear SSH enabled with key authentication — *Working via `ssh root@smarthub.local`*

### 1.7.2 NFS Root (for Development)
- ⏸️ Configure NFS server on host — *Deferred, not needed for current workflow*
- ⏸️ Add NFS root kernel config — *Deferred*
- ⏸️ Create NFS boot configuration — *Deferred*

### 1.7.3 Build Convenience Script
- ✅ Created `tools/build.sh` — *Wrapper script for Buildroot with external trees*
- ✅ Created `tools/flash.sh` — *SD card flashing script*
- ✅ Created `tools/stm32-power.py` — *Kasa smart plug control for remote power cycling*

```bash
cat > ~/projects/smarthub/tools/build.sh << 'EOF'
#!/bin/bash
set -e

cd ~/projects/smarthub/buildroot-src
make BR2_EXTERNAL="../buildroot-external-st:../buildroot" "$@"
EOF
chmod +x ~/projects/smarthub/tools/build.sh
```

---

## 1.8 Validation Checklist

Before proceeding to Phase 2, verify:

| Item | Status | Notes |
|------|--------|-------|
| Buildroot builds successfully | ✅ | SmartHub custom defconfig with ST external tree |
| SD card image boots | ✅ | TF-A → OP-TEE → U-Boot → Linux 6.6.78 |
| Boot time <10 seconds | ✅ | ~8 seconds to login prompt |
| WiFi connects | ✅ | BCM43430/Murata 1DX, requires NVRAM symlink |
| Ethernet works | ✅ | 1Gbps RTL8211F, DHCP via udhcpc/dhcpcd |
| SSH access works | ✅ | Dropbear with key authentication |
| Display shows console/splash | ⏸️ | Deferred — no fbcon/GUI in minimal build |
| Mosquitto running | ✅ | MQTT broker on port 1883 |
| remoteproc available | ✅ | /sys/class/remoteproc/remoteproc0 present |
| RAM usage <64 MB idle | ✅ | Actual: **24.7 MB** |
| Rootfs size <100 MB | ✅ | Actual: **48.7 MB** |

### WiFi Configuration Notes

The BCM43430 (Murata 1DX module) requires a board-specific NVRAM config file. The driver looks for:
1. `brcm/brcmfmac43430-sdio.st,stm32mp157f-dk2.txt` (board-specific)
2. `brcm/brcmfmac43430-sdio.txt` (generic fallback)

**Solution:** Create symlink in post_build.sh:
```bash
ln -sf brcmfmac43430-sdio.MUR1DX.txt brcmfmac43430-sdio.st,stm32mp157f-dk2.txt
```

### Network Configuration

- **Ethernet**: `eth0` via DHCP (udhcpc for quick boot, dhcpcd for full features)
- **WiFi**: `wlan0` via wpa_supplicant + DHCP
- **mDNS**: Avahi daemon advertises `smarthub.local`

---

## Troubleshooting

### Build fails with package error
```bash
# Clean specific package
make <package>-dirclean
make <package>-rebuild

# Full rebuild
make clean
make
```

### Kernel panic on boot
- Check kernel config matches hardware
- Verify device tree is correct
- Check rootfs mount options

### WiFi firmware not loading
- Verify firmware files in /lib/firmware/brcm/
- Check kernel log: `dmesg | grep brcm`
- Ensure brcmfmac module is built-in or loaded

### Boot too slow
- Profile with `grabserial` or `bootchart`
- Check for service timeouts
- Verify no hung filesystem checks

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 1.1 Environment Setup | 2-3 hours |
| 1.2 Kernel Configuration | 2-3 hours |
| 1.3 Buildroot Configuration | 4-6 hours |
| 1.4 Build Scripts | 2 hours |
| 1.5 Build and Test | 4-6 hours (includes build time) |
| 1.6 Boot Optimization | 4-8 hours |
| 1.7 Dev Conveniences | 2 hours |
| 1.8 Validation | 2 hours |
| **Total** | **22-32 hours** |

---

## References

- [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html)
- [Bootlin STM32MP1 Training](https://bootlin.com/doc/training/buildroot-stm32/)
- [STM32MP1 Wiki](https://wiki.st.com/stm32mpu/wiki/STM32MP15_Discovery_kits_-_Starter_Package)