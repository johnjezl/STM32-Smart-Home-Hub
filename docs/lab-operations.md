# Lab Operations Guide

This document describes how to interact with the STM32MP157F-DK2 lab hardware. Hardware management is handled through **labctl**, a centralized lab management system. The primary interface is the **labctl MCP server**, configured globally in `~/.claude/settings.json`. Use MCP tools directly rather than shelling out to the CLI.

The SBC is registered as **`stm32mp157f-dk2`** in labctl.

---

## Quick Reference (MCP Tools)

| Operation | MCP Tool | Parameters |
|-----------|----------|------------|
| Power on | `power_on` | `sbc_name: "stm32mp157f-dk2"` |
| Power off | `power_off` | `sbc_name: "stm32mp157f-dk2"` |
| Power cycle | `power_cycle` | `sbc_name: "stm32mp157f-dk2"` |
| Health check | `run_health_check` | `sbc_name: "stm32mp157f-dk2"` |
| SBC details | Read resource `lab://sbcs/stm32mp157f-dk2` | — |
| Power state | Read resource `lab://power/stm32mp157f-dk2` | — |
| Lab overview | Read resource `lab://status` | — |

---

## Network Access

### SSH Connection

The SmartHub is accessible via SSH over both Ethernet and WiFi:

```bash
# Via mDNS (preferred)
ssh root@smarthub.local

# Via Ethernet IP
ssh root@192.168.4.99

# Via WiFi IP
ssh root@192.168.4.67
```

Authentication is via SSH key (no password).

### IP Addresses

| Interface | IP | MAC | Notes |
|-----------|-----|-----|-------|
| eth0 (Ethernet) | 192.168.4.99 | — | DHCP, 1Gbps |
| wlan0 (WiFi) | 192.168.4.67 | 48:EB:62:D0:75:D9 | DHCP, 2.4GHz |
| mDNS | smarthub.local | — | Works on both interfaces |

---

## Power Control

### Power Management

The STM32MP157F-DK2 is powered via a Kasa HS300 smart plug strip at `192.168.4.140` (outlet index 5), managed through labctl.

Use the MCP tools directly:

- **`power_on`** / **`power_off`** / **`power_cycle`** with `sbc_name: "stm32mp157f-dk2"`
- **Read `lab://power/stm32mp157f-dk2`** to check current power state
- **`run_health_check`** with `sbc_name: "stm32mp157f-dk2"` for a full ping + power check

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

Serial console is **not currently assigned** for this SBC in labctl. If serial access becomes necessary (e.g., for debugging boot issues, kernel panics, or U-Boot interaction), assign a serial port via labctl:

```bash
# Discover connected USB-serial adapters
labctl serial discover

# Assign a serial port to the SBC
labctl port assign stm32mp157f-dk2 console <device> --alias stm32-console --baud 115200

# Connect to serial console
labctl console stm32mp157f-dk2
```

The ST-Link debug port on the board provides a USB serial interface at 115200 baud (8N1, no flow control).

---

## Flashing SD Cards

SDWire is **not currently assigned** for this SBC in labctl. SD card flashing is done manually.

### Using flash.sh Script

```bash
cd ~/projects/smarthub/STM32-Smart-Home-Hub

# Flash (auto-detects SD card in card reader)
./tools/flash.sh

# Or specify device
./tools/flash.sh /dev/sdX
```

### Manual Flash

```bash
# Identify SD card
lsblk

# Unmount partitions
sudo umount /dev/sdX*

# Flash image
sudo dd if=~/projects/smarthub/buildroot-src/output/images/sdcard.img \
    of=/dev/sdX bs=4M conv=fsync status=progress
sync
```

### Flash Procedure

1. Build image: `./tools/build.sh`
2. Remove SD card from board (if inserted)
3. Insert SD card into card reader
4. Flash: `./tools/flash.sh`
5. Remove SD card from reader
6. Insert SD card into board (slot on underside)
7. Power on via MCP tool: `power_on(sbc_name="stm32mp157f-dk2")`

---

## Boot Process

### Normal Boot Sequence

1. **Power on** — via `power_on` MCP tool or physical button
2. **TF-A** — ARM Trusted Firmware loads (~0.5s)
3. **OP-TEE** — Secure OS initializes (~0.5s)
4. **U-Boot** — Bootloader, loads kernel (~2s)
5. **Linux** — Kernel boots, init runs (~5s)
6. **Login prompt** — Total ~8s from power-on

### Interrupting Boot

To stop at U-Boot prompt:

1. Connect serial console (requires serial port assignment in labctl)
2. Power cycle via `power_cycle` MCP tool
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

### Deploy SmartHub Binary

```bash
# Transfer and restart
cat app/build-arm/smarthub | ssh root@smarthub.local "cat > /usr/bin/smarthub && chmod +x /usr/bin/smarthub"
ssh root@smarthub.local "killall smarthub; nohup smarthub > /var/log/smarthub.log 2>&1 &"
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

1. Read `lab://power/stm32mp157f-dk2` to check power state
2. Use `run_health_check(sbc_name="stm32mp157f-dk2")` for a full diagnostic
3. Try ping: `ping smarthub.local`
4. Use `power_cycle(sbc_name="stm32mp157f-dk2")` to reboot

### Boot Hangs

If serial console is assigned, check for last messages:
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

## labctl Reference

The labctl MCP server is configured globally in `~/.claude/settings.json` and is the preferred interface for all lab operations. The CLI (`/opt/labctl/venv/bin/labctl`) is available as a fallback but should not be needed in normal workflows.

For full documentation, see the [Embedded Lab Control repository](https://github.com/johnjezl/Embedded-Lab-Control).

---

*Created: 28 December 2025*
*Updated: 3 April 2026 - Migrated to labctl for lab hardware management*
