# SmartHub Buildroot Configuration

This document describes the Buildroot configuration for SmartHub.

---

## Overview

SmartHub uses Buildroot 2025.02.5 with two external trees:

| External Tree | Purpose |
|---------------|---------|
| buildroot-external-st | Kernel, bootloader, device tree (Bootlin) |
| buildroot/smarthub | SmartHub customizations |

The configuration is defined in `smarthub_defconfig` which extends the ST board support.

---

## External Tree Structure

### SmartHub External Tree

```
buildroot/
├── Config.in               # Custom package menu entries
├── external.desc           # External tree descriptor
├── external.mk             # Include custom packages
├── configs/
│   └── smarthub_defconfig  # Main configuration
├── board/smarthub/
│   ├── overlay/            # Root filesystem overlay
│   │   ├── etc/
│   │   │   ├── init.d/
│   │   │   │   ├── S40network
│   │   │   │   └── S80remoteproc
│   │   │   └── wpa_supplicant/
│   │   │       └── wpa_supplicant.conf
│   │   └── root/.ssh/
│   │       └── authorized_keys
│   ├── post_build.sh       # Post-build customization
│   └── post_image.sh       # Post-image generation
└── package/                # Custom packages (future)
```

### External Tree Descriptor

`external.desc`:
```
name: SMARTHUB
desc: Smart Home Hub customizations for STM32MP157F-DK2
```

This creates the variable `BR2_EXTERNAL_SMARTHUB_PATH` for use in defconfig.

---

## Defconfig Breakdown

### Architecture

```makefile
BR2_arm=y
BR2_cortex_a7=y
```

Target: ARM Cortex-A7 (STM32MP157)

### Toolchain

```makefile
BR2_TOOLCHAIN_EXTERNAL=y
BR2_TOOLCHAIN_EXTERNAL_BOOTLIN=y
BR2_TOOLCHAIN_EXTERNAL_BOOTLIN_ARMV7_EABIHF_GLIBC_STABLE=y
```

Uses Bootlin's pre-built ARM toolchain with glibc.

### System Configuration

```makefile
BR2_TARGET_GENERIC_HOSTNAME="smarthub"
BR2_TARGET_GENERIC_ISSUE="SmartHub Home Automation System"
BR2_SYSTEM_DHCP="eth0"
BR2_TARGET_GENERIC_ROOT_PASSWD=""
BR2_SYSTEM_BIN_SH_BUSYBOX=y
```

- Hostname: `smarthub`
- Empty root password (key auth only)
- BusyBox shell

### Kernel

```makefile
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_CUSTOM_TARBALL=y
BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION="$(call github,STMicroelectronics,linux)v6.6-stm32mp-r2.tar.gz"
BR2_LINUX_KERNEL_DEFCONFIG="multi_v7"
BR2_LINUX_KERNEL_DTS_SUPPORT=y
BR2_LINUX_KERNEL_INTREE_DTS_NAME="st/stm32mp157f-dk2"
```

Uses STMicroelectronics' Linux 6.6 kernel with ST device tree.

### Bootloaders

```makefile
# TF-A (ARM Trusted Firmware)
BR2_TARGET_ARM_TRUSTED_FIRMWARE=y
BR2_TARGET_ARM_TRUSTED_FIRMWARE_PLATFORM="stm32mp1"

# OP-TEE
BR2_TARGET_OPTEE_OS=y
BR2_TARGET_OPTEE_OS_PLATFORM="stm32mp1"
BR2_TARGET_OPTEE_OS_PLATFORM_FLAVOR="157F_DK2"

# U-Boot
BR2_TARGET_UBOOT=y
BR2_TARGET_UBOOT_BOARD_DEFCONFIG="stm32mp15"
```

Complete secure boot chain: TF-A → OP-TEE → U-Boot

### Filesystem

```makefile
BR2_TARGET_ROOTFS_EXT2=y
BR2_TARGET_ROOTFS_EXT2_4=y
BR2_TARGET_ROOTFS_EXT2_SIZE="200M"
```

200MB ext4 root filesystem.

### Overlay and Scripts

```makefile
BR2_ROOTFS_OVERLAY="$(BR2_EXTERNAL_ST_PATH)/board/stmicroelectronics/stm32mp1/overlay/ $(BR2_EXTERNAL_SMARTHUB_PATH)/board/smarthub/overlay/"
BR2_ROOTFS_POST_BUILD_SCRIPT="$(BR2_EXTERNAL_ST_PATH)/board/stmicroelectronics/common/post-build.sh $(BR2_EXTERNAL_SMARTHUB_PATH)/board/smarthub/post_build.sh"
```

Multiple overlays and post-build scripts are applied in order.

---

## Packages

### Core Services

| Package | Config | Purpose |
|---------|--------|---------|
| Dropbear | `BR2_PACKAGE_DROPBEAR=y` | SSH server |
| Mosquitto | `BR2_PACKAGE_MOSQUITTO=y` | MQTT broker |
| Avahi | `BR2_PACKAGE_AVAHI=y` | mDNS/DNS-SD |

### Networking

| Package | Config | Purpose |
|---------|--------|---------|
| wpa_supplicant | `BR2_PACKAGE_WPA_SUPPLICANT=y` | WiFi auth |
| dhcpcd | `BR2_PACKAGE_DHCPCD=y` | DHCP client |
| iproute2 | `BR2_PACKAGE_IPROUTE2=y` | Network tools |

### WiFi Firmware

| Package | Config | Purpose |
|---------|--------|---------|
| linux-firmware | `BR2_PACKAGE_LINUX_FIRMWARE=y` | Firmware blobs |
| BCM43xxx | `BR2_PACKAGE_LINUX_FIRMWARE_BRCM_BCM43XXX=y` | WiFi/BT firmware |

### Libraries

| Package | Config | Purpose |
|---------|--------|---------|
| OpenSSL | `BR2_PACKAGE_OPENSSL=y` | TLS/crypto |
| SQLite | `BR2_PACKAGE_SQLITE=y` | Database |
| nlohmann-json | `BR2_PACKAGE_NLOHMANN_JSON=y` | JSON parsing |
| OpenAMP | `BR2_PACKAGE_OPENAMP=y` | A7↔M4 comm |

### Debug (Development)

| Package | Config | Purpose |
|---------|--------|---------|
| strace | `BR2_PACKAGE_STRACE=y` | System call tracer |

---

## Post-Build Script

`board/smarthub/post_build.sh` runs after filesystem is created:

```bash
#!/bin/bash
TARGET_DIR="$1"

# Create SmartHub directories
mkdir -p "${TARGET_DIR}/opt/smarthub"/{bin,config,data,logs}
mkdir -p "${TARGET_DIR}/var/lib/smarthub"
mkdir -p "${TARGET_DIR}/var/lib/mosquitto"

# Make init scripts executable
chmod 755 "${TARGET_DIR}/etc/init.d/S"*

# Set SSH permissions
chmod 700 "${TARGET_DIR}/root/.ssh"
chmod 600 "${TARGET_DIR}/root/.ssh/authorized_keys"

# Create WiFi firmware symlink
ln -sf brcmfmac43430-sdio.MUR1DX.txt \
    "${TARGET_DIR}/lib/firmware/brcm/brcmfmac43430-sdio.st,stm32mp157f-dk2.txt"
```

---

## Overlay Structure

Files in the overlay are copied directly to the root filesystem:

```
overlay/
├── etc/
│   ├── init.d/
│   │   ├── S40network         # Network initialization
│   │   └── S80remoteproc      # M4 firmware loader
│   └── wpa_supplicant/
│       └── wpa_supplicant.conf # WiFi credentials
└── root/
    └── .ssh/
        └── authorized_keys     # SSH public key
```

---

## Image Generation

### SD Card Image

The final `sdcard.img` is created by genimage using:

```
buildroot-external-st/board/stmicroelectronics/stm32mp1/genimage.cfg
```

Partition layout:

| Partition | Offset | Size | Contents |
|-----------|--------|------|----------|
| fsbl1 | 17KB | 256KB | TF-A (copy 1) |
| fsbl2 | 273KB | 256KB | TF-A (copy 2) |
| fip | 529KB | 4MB | U-Boot + OP-TEE |
| u-boot-env | 4.5MB | 512KB | Environment |
| rootfs | 5MB | 200MB | Root filesystem |

### Output Files

After build, find images in:
```
~/projects/smarthub/buildroot-src/output/images/
```

| File | Description |
|------|-------------|
| `sdcard.img` | Flashable SD card image |
| `sdcard.img.gz` | Compressed image |
| `rootfs.ext4` | Root filesystem only |
| `zImage` | Kernel image |
| `stm32mp157f-dk2.dtb` | Device tree blob |

---

## Customization

### Adding Packages

1. Find package in menuconfig:
   ```bash
   ./tools/build.sh menuconfig
   # Navigate: Target packages → <category> → <package>
   ```

2. Save to defconfig:
   ```bash
   ./tools/build.sh savedefconfig
   ```

3. Or edit defconfig directly:
   ```makefile
   BR2_PACKAGE_<NAME>=y
   ```

### Modifying Kernel Config

```bash
cd ~/projects/smarthub/buildroot-src
make linux-menuconfig
make linux-savedefconfig

# Copy fragment to external tree if needed
```

### Adding Files to Rootfs

Add to overlay directory:
```bash
mkdir -p buildroot/board/smarthub/overlay/path/to/dir
echo "content" > buildroot/board/smarthub/overlay/path/to/file
```

### Custom Init Script

Create in overlay:
```
overlay/etc/init.d/S<priority><name>
```

Priority determines execution order (S01 runs first, S99 runs last).

---

## Build System Integration

### Environment Variables

During build, these variables are available:

| Variable | Value |
|----------|-------|
| `BR2_EXTERNAL_SMARTHUB_PATH` | Path to SmartHub external tree |
| `BR2_EXTERNAL_ST_PATH` | Path to ST external tree |
| `TARGET_DIR` | Path to target filesystem |
| `BINARIES_DIR` | Path to output images |

### Multiple External Trees

The build uses both external trees:

```bash
export BR2_EXTERNAL="$HOME/projects/smarthub/buildroot-external-st:$HOME/projects/smarthub/STM32-Smart-Home-Hub/buildroot"
```

Order matters: ST tree provides base, SmartHub tree provides customizations.

---

## Debugging Build Issues

### Package Build Failure

```bash
# Rebuild single package
make <package>-rebuild

# Clean and rebuild
make <package>-dirclean
make <package>

# Check build log
cat output/build/<package>-<version>/.stamp_*
```

### Configuration Issues

```bash
# Check actual config
grep "CONFIG_NAME" output/.config

# Compare with defconfig
make savedefconfig
diff configs/smarthub_defconfig .defconfig
```

### Rootfs Issues

```bash
# Examine target filesystem
ls -la output/target/

# Check overlay was applied
ls output/target/etc/init.d/S40network
```

---

## References

- [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html)
- [External Tree Documentation](https://buildroot.org/downloads/manual/manual.html#outside-br-custom)
- [Bootlin ST External Tree](https://github.com/bootlin/buildroot-external-st)

---

*Created: 28 December 2025*
