# Lab Operations Guide

This document describes how to interact with the STM32MP157F-DK2 lab hardware. It is primarily intended for Claude Code to reference when performing lab operations.

Power control and health checks go through the **labctl MCP tools**, with the SBC name `stm32mp157f-dk2`. Never run the `labctl` CLI. See `CLAUDE.md`.

---

## Quick Reference

| Operation | Command |
|-----------|---------|
| SSH to board | `ssh root@smarthub.local` or `ssh root@192.168.4.102` |
| Power off | `power_off` MCP tool |
| Power on | `power_on` MCP tool |
| Power cycle | `power_cycle` MCP tool |
| Health check | `run_health_check` MCP tool |
| Serial console | `sudo picocom -b 115200 /dev/ttyACM0` |
| Flash SD card | `tools/flash.sh` |

---

## Network Access

### SSH Connection

The SmartHub is accessible via SSH over both Ethernet and WiFi:

```bash
# Via mDNS, if this machine resolves .local names
ssh root@smarthub.local

# Via WiFi IP
ssh root@192.168.4.99

# Via Ethernet IP
ssh root@192.168.4.102
```

Authentication is via SSH key (no password).

### IP Addresses

| Interface | IP | Notes |
|-----------|-----|-------|
| eth0 (Ethernet) | 192.168.4.102 | DHCP, 1Gbps |
| wlan0 (WiFi) | 192.168.4.99 | DHCP, 2.4GHz |
| mDNS | smarthub.local | Works on both interfaces |

---

## Power Control

### Kasa Smart Plug

The STM32MP157F-DK2 is powered through a Kasa HS300 smart plug strip at `192.168.4.140` (outlet index 5), managed by labctl.

Use the `power_on`, `power_off`, `power_cycle` and `run_health_check` MCP tools with `sbc_name: "stm32mp157f-dk2"`.

### When to Use Power Control

**Prefer software reboot when possible:**
```bash
ssh root@smarthub.local reboot
```

**Use power cycle only when:**
- SSH not responding
- System hung/crashed
- Need cold boot

---

## Serial Console Access

### Connection

The ST-Link provides a USB serial port for console access. No serial port is assigned to this board in labctl, so `/dev/ttyACM0` exists only while the ST-Link USB cable is plugged into the machine you're using. Ask the maintainer if it's missing.

```bash
# Check for device
ls /dev/ttyACM0

# Connect with picocom
sudo picocom -b 115200 /dev/ttyACM0
```

**To exit picocom:** Press `Ctrl-A` then `Ctrl-X`

### When to Use Serial

- Watching boot messages
- Debugging when network is unavailable
- U-Boot interaction (interrupt boot)
- Kernel panic debugging

### Serial Port Permissions

To avoid needing sudo:

```bash
# Add user to dialout group
sudo usermod -aG dialout $USER

# Log out and back in for change to take effect
```

### Capturing Boot Output

```bash
# Capture boot messages to file
sudo timeout 60 cat /dev/ttyACM0 > boot_log.txt &

# Then power cycle with the power_cycle MCP tool to capture the full boot

# Wait for boot, then check log
cat boot_log.txt
```

**Never use a bare `cat` on a tty device.** It waits for an EOF that never comes. Always wrap it in `timeout`, as above.

---

## Flashing SD Cards

### Using flash.sh

```bash
cd ~/projects/smarthub/STM32-Smart-Home-Hub

# Flash (uses the removable device, if there's only one)
./tools/flash.sh

# Or specify device
./tools/flash.sh /dev/sde
```

It only writes to a whole, removable disk that isn't holding this system, and asks for the device's name to be typed back.

### Manual Flash

```bash
# Identify SD card (look for ~8GB device)
lsblk

# Unmount partitions
sudo umount /dev/sde*

# Flash image
sudo dd if=~/projects/smarthub/buildroot-src/output/images/sdcard.img \
    of=/dev/sde bs=4M conv=fsync status=progress
sync
```

### Flash Procedure

1. Build image: `./tools/build.sh`
2. Remove SD card from board (if inserted)
3. Insert SD card into card reader
4. Flash: `./tools/flash.sh`
5. Remove SD card from reader
6. Insert SD card into board (slot on underside)
7. Power on

---

## Boot Process

### Normal Boot Sequence

1. **Power on** — via smart plug or physical button
2. **TF-A** — ARM Trusted Firmware loads (~0.5s)
3. **OP-TEE** — Secure OS initializes (~0.5s)
4. **U-Boot** — Bootloader, loads kernel (~2s)
5. **Linux** — Kernel boots, init runs (~5s)
6. **Login prompt** — Total ~8s from power-on

### Interrupting Boot

To stop at U-Boot prompt:

1. Connect serial console
2. Power cycle
3. Press any key during "Hit any key to stop autoboot"

### U-Boot Commands

```
# Boot normally
boot

# Check environment
printenv

# Network boot (if configured)
dhcp; tftp ${loadaddr} zImage; bootz ${loadaddr}

# Reset
reset
```

---

## Deploying to Running System

### Quick File Copy

```bash
# Copy single file
scp myfile root@smarthub.local:/opt/smarthub/

# Copy directory
scp -r mydir root@smarthub.local:/opt/smarthub/
```

### Sync Overlay

```bash
# Sync entire overlay to running system
rsync -avz buildroot/board/smarthub/overlay/ root@smarthub.local:/
```

### M4 Firmware Deployment

```bash
# Copy firmware
scp m4_firmware.elf root@smarthub.local:/lib/firmware/smarthub_m4.elf

# Restart M4 (on target)
ssh root@smarthub.local "echo stop > /sys/class/remoteproc/remoteproc0/state; \
    echo smarthub_m4.elf > /sys/class/remoteproc/remoteproc0/firmware; \
    echo start > /sys/class/remoteproc/remoteproc0/state"
```

---

## Troubleshooting

### Board Not Responding to SSH

1. Check power: `run_health_check` MCP tool
2. Try ping: `ping smarthub.local`
3. Check serial console for errors
4. Power cycle: `power_cycle` MCP tool

### No Serial Output

1. Check USB cable connected to ST-Link port (not power)
2. Check device exists: `ls /dev/ttyACM0`
3. Check no other program using port: `sudo fuser /dev/ttyACM0`
4. Kill blocking process: `sudo fuser -k /dev/ttyACM0`

### Boot Hangs

Check serial console for last messages:
- **Hangs at TF-A** — SD card not detected or corrupted
- **Hangs at U-Boot** — Check boot environment
- **Hangs at kernel** — Missing driver or hardware issue
- **Hangs at init** — Check init scripts

### WiFi Not Connecting

```bash
# Check interface exists
ssh root@smarthub.local "ip link show wlan0"

# Check wpa_supplicant running
ssh root@smarthub.local "ps | grep wpa"

# Check firmware loaded
ssh root@smarthub.local "dmesg | grep brcm | tail -10"

# Manually start WiFi
ssh root@smarthub.local "/etc/init.d/S40network restart"
```

---

## Hardware Reference

### Board Layout

```
┌────────────────────────────────────────────────┐
│  ┌──────┐                         ┌──────────┐ │
│  │ST-Link│ USB-C                  │  SD Card │ │
│  │ Debug │ (console)              │  (under) │ │
│  └──────┘                         └──────────┘ │
│                                                │
│  ┌──────────────────────────────────────────┐  │
│  │                                          │  │
│  │              STM32MP157                  │  │
│  │                                          │  │
│  └──────────────────────────────────────────┘  │
│                                                │
│  ┌──────┐  ┌──────┐  ┌──────────┐  ┌───────┐  │
│  │ USB-C│  │  ETH │  │  HDMI    │  │ Audio │  │
│  │ Power│  │      │  │ (unused) │  │       │  │
│  └──────┘  └──────┘  └──────────┘  └───────┘  │
│                                                │
│  ┌──────────────────────────────────────────┐  │
│  │         4" LCD with Touch                │  │
│  │           (attached)                     │  │
│  └──────────────────────────────────────────┘  │
└────────────────────────────────────────────────┘
```

### Boot Switches (BOOT0/BOOT2)

Located on top of board:

| BOOT0 | BOOT2 | Boot Source |
|-------|-------|-------------|
| ON | ON | SD Card (normal) |
| OFF | ON | eMMC (if present) |
| ON | OFF | USB DFU mode |

**Normal operation:** Both switches ON (away from "BOOT" text)

### Connectors

| Connector | Purpose |
|-----------|---------|
| USB-C (top) | ST-Link debug/console |
| USB-C (bottom) | Power input (5V/3A) |
| Ethernet | Gigabit network |
| microSD | Boot/root storage |
| Arduino headers | GPIO, I2C, SPI |
| 40-pin RPi header | Additional GPIO |

---

*Created: 28 December 2025*
*For: Claude Code reference during lab operations*
