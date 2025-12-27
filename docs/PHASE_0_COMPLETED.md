# Phase 0: Development Environment Setup - Completed

**Completion Date**: 2025-12-26
**Duration**: ~4 hours (including troubleshooting)

---

## Summary

Phase 0 established the development infrastructure, verified hardware functionality, and validated the toolchain for the STM32MP157F-DK2 Smart Home Hub project.

---

## Host Machine Setup (Windows)

### Installed Tools

| Tool | Version | Purpose |
|------|---------|---------|
| STM32CubeIDE | Latest | M4 firmware development, includes ARM toolchain |
| STM32CubeProgrammer | Latest | Flashing and ST-Link access |
| balenaEtcher | Latest | SD card flashing |
| WSL2 (Ubuntu) | Ubuntu | Linux tools for image creation |

### WSL Packages Installed
```bash
sudo apt install gdisk
```

---

## SD Card Image Creation

### Challenge
The STM32MP1 Starter Package does not include a pre-built `.raw` image for direct flashing. The standard approach requires USB DFU mode, which was problematic on Windows.

### Solution
Created a custom script to build a flashable SD card image with proper GPT partition table.

### Final Image Creation Script
Location: Created in WSL at the OpenSTLinux images directory

```bash
#!/bin/bash
set -e

OUTPUT="sdcard_gpt.img"
ATF_DIR="arm-trusted-firmware"
FIP_DIR="fip"

# Partition type GUIDs for STM32MP1
FSBL_GUID="8301"
FIP_TYPE="19d5df83-11b0-457b-be2c-7559c13142a5"
METADATA_TYPE="8da63339-0007-60c0-c436-083ac8230908"

echo "Creating 7GB SD card image..."
dd if=/dev/zero of=$OUTPUT bs=1M count=7000 status=progress

echo "Creating GPT with correct type GUIDs..."
sgdisk -a 1 \
  -n 1:34:545      -c 1:fsbl1     -t 1:$FSBL_GUID \
  -n 2:546:1057    -c 2:fsbl2     -t 2:$FSBL_GUID \
  -n 3:1058:1569   -c 3:metadata1 -t 3:$METADATA_TYPE \
  -n 4:1570:2081   -c 4:metadata2 -t 4:$METADATA_TYPE \
  -n 5:2082:10273  -c 5:fip-a     -t 5:$FIP_TYPE \
  -n 6:10274:18465 -c 6:fip-b     -t 6:$FIP_TYPE \
  -n 7:18466:19489 -c 7:u-boot-env \
  -n 8:19490:150561 -c 8:bootfs   -t 8:8300 \
  -n 9:150562:183329 -c 9:vendorfs -t 9:8300 \
  -n 10:183330:0 -c 10:rootfs     -t 10:8300 \
  $OUTPUT

# Set partition UUIDs for fip-a and fip-b (required by metadata)
sgdisk -u 5:4fd84c93-54ef-463f-a7ef-ae25ff887087 $OUTPUT
sgdisk -u 6:09c54952-d5bf-45af-acee-335303766fb3 $OUTPUT

echo "Writing partition data..."
dd if=$ATF_DIR/tf-a-stm32mp157f-dk2-optee-sdcard.stm32 of=$OUTPUT bs=512 seek=34 conv=notrunc
dd if=$ATF_DIR/tf-a-stm32mp157f-dk2-optee-sdcard.stm32 of=$OUTPUT bs=512 seek=546 conv=notrunc
dd if=$ATF_DIR/metadata.bin of=$OUTPUT bs=512 seek=1058 conv=notrunc
dd if=$ATF_DIR/metadata.bin of=$OUTPUT bs=512 seek=1570 conv=notrunc
dd if=$FIP_DIR/fip-stm32mp157f-dk2-optee-sdcard.bin of=$OUTPUT bs=512 seek=2082 conv=notrunc

echo "Writing bootfs..."
dd if=st-image-bootfs-openstlinux-weston-stm32mp1.bootfs.ext4 of=$OUTPUT bs=512 seek=19490 conv=notrunc status=progress

echo "Writing vendorfs..."
dd if=st-image-vendorfs-openstlinux-weston-stm32mp1.vendorfs.ext4 of=$OUTPUT bs=512 seek=150562 conv=notrunc status=progress

echo "Writing rootfs..."
dd if=st-image-weston-openstlinux-weston-stm32mp1.rootfs.ext4 of=$OUTPUT bs=512 seek=183330 conv=notrunc status=progress

echo "Done! Flash with balenaEtcher"
```

### Key Learnings

1. **Partition Type GUIDs are critical**: The STM32MP1 bootloader (TF-A) looks for partitions by type GUID, not just name.
   - FIP type: `19d5df83-11b0-457b-be2c-7559c13142a5`
   - Metadata type: `8da63339-0007-60c0-c436-083ac8230908`

2. **Partition UUIDs matter**: The metadata.bin contains references to specific partition UUIDs for FIP-A and FIP-B.
   - FIP-A UUID: `4fd84c93-54ef-463f-a7ef-ae25ff887087`
   - FIP-B UUID: `09c54952-d5bf-45af-acee-335303766fb3`

3. **Alignment disabled**: Used `sgdisk -a 1` to disable sector alignment since STM32MP1 requires specific non-aligned offsets.

---

## Hardware Configuration

### Boot Switches (SW1)

| Mode | BOOT0 | BOOT2 |
|------|-------|-------|
| **SD Card Boot** | ON (away from BOOT text) | ON (away from BOOT text) |
| DFU/Recovery | OFF | OFF |

### Connections

| Port | Purpose |
|------|---------|
| USB-C (PWR_IN) | Power supply |
| USB-C (ST-LINK) | Serial console + debugging |
| Micro-USB (CN7) | USB DFU flashing (if needed) |

### Serial Console

- **Baud rate**: 115200
- **Settings**: 8N1, no flow control
- **Windows port**: COM8
- **Cygwin device**: /dev/ttyS7

---

## Boot Configuration Fix

### Problem
The stock extlinux configuration uses a hardcoded PARTUUID that doesn't match our generated partition UUIDs.

### Solution
Modified `/boot/mmc0_extlinux/stm32mp157f-dk2_extlinux.conf` to use PARTLABEL instead:

```
# Changed from:
APPEND root=PARTUUID=e91c4e10-16e6-4c0e-bd0e-77becf4a3582 rootwait rw

# Changed to:
APPEND root=PARTLABEL=rootfs rootwait rw
```

### U-Boot Environment
Saved `distro_bootpart=8` to U-Boot environment so it scans the correct partition for boot files.

---

## Network Configuration

### Ethernet
- **Interface**: end0 (not eth0 - new naming convention)
- **Configuration**: DHCP (automatic)
- **IP Address**: 192.168.4.102

### WiFi (Manual Setup Required Each Boot)
```bash
mkdir -p /etc/wpa_supplicant
wpa_passphrase "SSID" "PASSWORD" > /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
udhcpc -i wlan0
```

To make persistent:
```bash
systemctl enable wpa_supplicant@wlan0
```

### SSH
- **Service**: dropbear (lightweight SSH)
- **Access**: `ssh root@192.168.4.102` (no password)

---

## System Information

```
Kernel: 6.6.78
OS: ST OpenSTLinux - Weston 5.0.8 (scarthgap)
Board: STM32MP157F-DK2 (MB1272 Var4.0 Rev.C-03)
CPU: STM32MP157FAC Rev.Z (Dual Cortex-A7 @ 650 MHz)
RAM: 512 MB DDR3
```

---

## Verified Functionality

| Feature | Status | Notes |
|---------|--------|-------|
| SD Card Boot | ✓ | Auto-boots to Linux |
| Serial Console | ✓ | 115200 baud via ST-Link VCP |
| LCD Display | ✓ | Weston desktop visible |
| Touchscreen | ✓ | Input working |
| Ethernet | ✓ | DHCP, SSH accessible |
| WiFi | ✓ | Manual config required |
| Remoteproc (M4) | ✓ | Ready for firmware (offline state) |

---

## Files Modified/Created

### On Host (Windows)
- `CLAUDE.md` - Serial console access notes for Claude Code

### On Target (STM32MP157F-DK2)
- `/boot/mmc0_extlinux/stm32mp157f-dk2_extlinux.conf` - Fixed root= parameter
- `/boot/mmc0_extlinux/extlinux.conf` - Fixed root= parameter
- U-Boot environment - Saved distro_bootpart=8

---

## Troubleshooting Notes

### Issue: USB DFU not detected on Windows
**Cause**: DFU requires micro-USB connection to CN7 port, separate from ST-Link USB-C.
**Solution**: Created custom SD card image instead of using DFU flashing.

### Issue: Boot fails with "No partition with uuid mention"
**Cause**: Missing or incorrect partition type GUIDs.
**Solution**: Used correct GUIDs (FIP: 19d5df83-..., Metadata: 8da63339-...).

### Issue: Kernel can't find root filesystem
**Cause**: extlinux.conf references wrong PARTUUID.
**Solution**: Changed to use PARTLABEL=rootfs instead.

### Issue: U-Boot stops at prompt instead of auto-booting
**Cause**: Bootfs partition missing "bootable" flag; U-Boot couldn't find boot partition.
**Solution**: Set distro_bootpart=8 in U-Boot environment.

---

## Next Steps

Proceed to **Phase 1: Buildroot Base System** to create a minimal, optimized Linux image tailored for the smart home hub.
