# Phase 1: Buildroot Base System - Completed

**Completion Date**: 2025-12-28
**Duration**: ~6 hours (including troubleshooting and documentation)

---

## Summary

Phase 1 established a custom Buildroot-based Linux system optimized for the SmartHub application. The system boots in under 10 seconds, uses minimal RAM, and includes all required networking and communication infrastructure.

---

## Build System Setup

### Buildroot Configuration

| Component | Version | Source |
|-----------|---------|--------|
| Buildroot | 2025.02.5 | github.com/buildroot/buildroot |
| External Tree (ST) | st/2025.02.5 | github.com/bootlin/buildroot-external-st |
| Linux Kernel | 6.6-stm32mp-r2 | STMicroelectronics fork |
| U-Boot | ST variant | Via Bootlin external tree |
| TF-A | ST variant | Via Bootlin external tree |

### Custom External Tree

Created `STM32-Smart-Home-Hub/buildroot/` with:

```
buildroot/
├── Config.in               # Package menu entries
├── external.desc           # External tree descriptor
├── external.mk             # Package includes
├── configs/
│   └── smarthub_defconfig  # Main configuration
└── board/smarthub/
    ├── overlay/            # Root filesystem overlay
    │   ├── etc/init.d/     # Init scripts
    │   ├── etc/wpa_supplicant/
    │   └── root/.ssh/
    ├── post_build.sh       # Post-build customization
    └── post_image.sh       # Image generation
```

---

## Key Packages Enabled

### Core Services

| Package | Purpose |
|---------|---------|
| Dropbear | Lightweight SSH server |
| Mosquitto | MQTT broker |
| Avahi | mDNS/DNS-SD |

### Networking

| Package | Purpose |
|---------|---------|
| wpa_supplicant | WiFi WPA/WPA2 authentication |
| dhcpcd | DHCP client |
| iproute2 | Network utilities |

### Libraries

| Package | Purpose |
|---------|---------|
| OpenSSL | TLS/crypto |
| SQLite | Database |
| nlohmann-json | JSON parsing |
| OpenAMP | A7↔M4 communication |

### Firmware

| Package | Purpose |
|---------|---------|
| linux-firmware-brcm-bcm43xxx | WiFi/BT firmware for BCM43430 |

---

## WiFi Configuration Fix

### Problem

The BCM43430 WiFi chip (Murata 1DX module) requires board-specific NVRAM configuration. The kernel driver looks for:

```
/lib/firmware/brcm/brcmfmac43430-sdio.st,stm32mp157f-dk2.txt
```

This file was missing, causing WiFi initialization to fail silently.

### Solution

Created symlink in `post_build.sh`:

```bash
ln -sf brcmfmac43430-sdio.MUR1DX.txt \
    "${TARGET_DIR}/lib/firmware/brcm/brcmfmac43430-sdio.st,stm32mp157f-dk2.txt"
```

The Murata 1DX NVRAM file (`MUR1DX.txt`) is included in the linux-firmware package and contains the correct configuration for this module.

### Verification

```bash
# Check firmware loaded
dmesg | grep brcm
# brcmfmac: brcmf_fw_alloc_request: using brcm/brcmfmac43430-sdio for chip BCM43430/1

# Check interface
ip link show wlan0
# wlan0: <BROADCAST,MULTICAST,UP,LOWER_UP> ...

# Verify connection
wpa_cli -i wlan0 status
# wpa_state=COMPLETED
```

---

## Init Scripts

### S40network

Handles network initialization on boot:

1. Brings up loopback interface
2. Brings up Ethernet with DHCP
3. Starts wpa_supplicant for WiFi
4. Brings up WiFi with DHCP

### S80remoteproc

Placeholder for M4 firmware loading (Phase 3):

1. Checks for firmware at `/lib/firmware/smarthub_m4.elf`
2. Loads firmware via remoteproc if present
3. Gracefully skips if firmware not found

---

## Build Tools Created

### tools/build.sh

Wrapper script for Buildroot with dual external trees:

```bash
./tools/build.sh              # Full build
./tools/build.sh menuconfig   # Interactive config
./tools/build.sh savedefconfig # Save changes
./tools/build.sh clean        # Clean build
```

### tools/flash.sh

SD card flashing script:

```bash
./tools/flash.sh          # Auto-detect SD card
./tools/flash.sh /dev/sde # Specify device
```

### tools/stm32-power.py

Kasa smart plug control for remote power cycling:

```bash
./venv/bin/python tools/stm32-power.py status
./venv/bin/python tools/stm32-power.py on
./venv/bin/python tools/stm32-power.py off
./venv/bin/python tools/stm32-power.py cycle
```

---

## Performance Metrics

### Boot Time

| Phase | Time |
|-------|------|
| TF-A | ~0.5s |
| OP-TEE | ~0.5s |
| U-Boot | ~2s |
| Linux + Init | ~5s |
| **Total** | **~8s** |

Target was <10 seconds.

### Memory Usage

```
              total        used        free      shared  buff/cache   available
Mem:         491.8M       24.7M      430.4M        0.0K       36.8M      449.3M
```

**Used: 24.7 MB** (target was <64 MB)

### Filesystem Size

```
Filesystem      Size  Used Avail Use% Mounted on
/dev/root       194M   49M  136M  27% /
```

**Used: 48.7 MB** (target was <100 MB)

---

## Network Configuration

### Interfaces

| Interface | IP Address | Type |
|-----------|------------|------|
| eth0 | 192.168.4.102 | DHCP (Ethernet) |
| wlan0 | 192.168.4.99 | DHCP (WiFi) |
| mDNS | smarthub.local | Avahi |

### Services

| Service | Port | Status |
|---------|------|--------|
| SSH (Dropbear) | 22 | Running, key auth only |
| MQTT (Mosquitto) | 1883 | Running, anonymous local |
| mDNS (Avahi) | 5353 | Running |

---

## Verified Functionality

| Feature | Status | Notes |
|---------|--------|-------|
| SD Card Boot | ✓ | TF-A → OP-TEE → U-Boot → Linux |
| Boot Time | ✓ | ~8 seconds (target: <10s) |
| Serial Console | ✓ | /dev/ttyACM0 @ 115200 baud |
| Ethernet | ✓ | 1Gbps via RTL8211F PHY |
| WiFi | ✓ | BCM43430 with NVRAM fix |
| SSH Access | ✓ | Key authentication via Dropbear |
| MQTT Broker | ✓ | Mosquitto on port 1883 |
| mDNS | ✓ | smarthub.local via Avahi |
| remoteproc | ✓ | /sys/class/remoteproc/remoteproc0 present |
| RAM Usage | ✓ | 24.7 MB (target: <64 MB) |
| Rootfs Size | ✓ | 48.7 MB (target: <100 MB) |

---

## Files Created/Modified

### New Files

| File | Purpose |
|------|---------|
| `buildroot/external.desc` | External tree descriptor |
| `buildroot/external.mk` | Package includes |
| `buildroot/Config.in` | Package menu |
| `buildroot/configs/smarthub_defconfig` | Main Buildroot config |
| `buildroot/board/smarthub/post_build.sh` | Post-build customization |
| `buildroot/board/smarthub/post_image.sh` | Image generation |
| `buildroot/board/smarthub/overlay/*` | Root filesystem overlay |
| `tools/build.sh` | Build wrapper script |
| `tools/flash.sh` | SD card flash script |
| `tools/stm32-power.py` | Power control script |
| `docs/architecture.md` | System architecture |
| `docs/building.md` | Build instructions |
| `docs/buildroot.md` | Buildroot configuration |
| `docs/networking.md` | Network setup |
| `docs/lab-operations.md` | Hardware operations guide |

### Modified Files

| File | Changes |
|------|---------|
| `docs/PHASE_1_TODO.md` | Updated validation status |
| `.gitignore` | Allow .claude/settings.json |

---

## Troubleshooting Notes

### Issue: WiFi interface not appearing

**Cause**: Missing board-specific NVRAM configuration file.

**Solution**: Create symlink to Murata 1DX NVRAM config in post_build.sh.

### Issue: SSH permission denied

**Cause**: No SSH key in authorized_keys.

**Solution**: Added public key to overlay at `root/.ssh/authorized_keys`.

### Issue: Init scripts not executable

**Cause**: Git doesn't preserve execute permissions on overlay files.

**Solution**: Added `chmod 755` in post_build.sh.

---

## Deferred Items

| Item | Deferred To | Reason |
|------|-------------|--------|
| Kernel config fragments | Phase 6 | ST defaults work well |
| Boot optimization (quiet, loglevel) | Phase 6 | Current boot time acceptable |
| NFS root for development | N/A | SSH + rsync workflow sufficient |
| Display/fbcon | Phase 3 | Requires LVGL integration |

---

## Documentation Created

Comprehensive documentation following SLM-OS style:

1. **architecture.md** - System design with ASCII diagrams
2. **building.md** - Build and flash instructions
3. **buildroot.md** - Detailed Buildroot configuration
4. **networking.md** - WiFi, Ethernet, mDNS setup
5. **lab-operations.md** - Hardware interaction guide for Claude Code

---

## Next Steps

Proceed to **Phase 2: Core Application Framework** to implement:

- C++ application structure with CMake
- LVGL touchscreen UI
- SQLite database layer
- MQTT client integration
- Web server (REST API)
- RPMsg communication foundation

---

*Completed: 28 December 2025*
