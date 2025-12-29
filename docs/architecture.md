# SmartHub Architecture

High-level architecture documentation for the Smart Home Hub system.

**Status:** Phase 3 complete (December 2025)

---

## Overview

SmartHub is a custom Linux-based smart home automation hub running on the STM32MP157F-DK2 development board. The system uses a heterogeneous architecture with a dual-core Cortex-A7 running Linux and a Cortex-M4 coprocessor for real-time tasks.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          SmartHub Architecture                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                      Application Layer                               │   │
│   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐   │   │
│   │  │  SmartHub    │  │   Web UI     │  │    Home Assistant        │   │   │
│   │  │  Daemon      │  │  (Future)    │  │    Integration           │   │   │
│   │  └──────────────┘  └──────────────┘  └──────────────────────────┘   │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │ MQTT                                    │
│                                    ▼                                         │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                      Services Layer                                  │   │
│   │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐    │   │
│   │  │ Mosquitto  │  │   Avahi    │  │  Dropbear  │  │  dhcpcd    │    │   │
│   │  │   MQTT     │  │   mDNS     │  │    SSH     │  │   DHCP     │    │   │
│   │  └────────────┘  └────────────┘  └────────────┘  └────────────┘    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│                                    ▼                                         │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                       Linux Kernel 6.6                               │   │
│   │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐  │   │
│   │  │ brcmfmac│ │remoteproc│ │  DRM  │ │ stmmac │ │  I2C  │ │  SPI  │  │   │
│   │  │  WiFi  │ │   M4   │ │Display│ │Ethernet│ │       │ │       │  │   │
│   │  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘  │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                        │                    │                                │
│            ┌───────────┘                    └────────────┐                   │
│            ▼                                             ▼                   │
│   ┌─────────────────────────┐               ┌─────────────────────────┐     │
│   │    Cortex-A7 (x2)       │               │      Cortex-M4          │     │
│   │    @ 650 MHz            │◄── OpenAMP ──►│      @ 209 MHz          │     │
│   │    Linux + Apps         │    RPMsg      │    Real-time Tasks      │     │
│   └─────────────────────────┘               └─────────────────────────┘     │
│                                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                         Hardware (STM32MP157F-DK2)                           │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐         │
│  │  DDR3  │ │ eMMC/  │ │  WiFi  │ │  ETH   │ │  LCD   │ │  GPIO  │         │
│  │ 512MB  │ │   SD   │ │BCM43430│ │RTL8211F│ │  4"    │ │Arduino │         │
│  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘         │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Platform

### STM32MP157F-DK2 Discovery Board

| Component | Specification |
|-----------|---------------|
| **MPU** | STM32MP157FAC - Dual Cortex-A7 @ 650 MHz |
| **MCU** | Cortex-M4 @ 209 MHz |
| **RAM** | 512 MB DDR3L |
| **Storage** | microSD card (8GB+) |
| **Display** | 4" 480x800 LCD with capacitive touch |
| **WiFi/BT** | Murata 1DX (BCM43430) |
| **Ethernet** | Gigabit (RTL8211F PHY) |
| **Debug** | ST-Link V2.1 (USB-C) |

### Boot Chain

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│  ROM     │───►│  TF-A    │───►│  OP-TEE  │───►│  U-Boot  │───►│  Linux   │
│ (fixed)  │    │  BL2     │    │  BL32    │    │  BL33    │    │  Kernel  │
└──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘
     │               │               │               │               │
   1.5s            0.5s            0.5s            2s              4s
                                                              ─────────────
                                                              ~8s to login
```

| Stage | Component | Source |
|-------|-----------|--------|
| FSBL | TF-A (ARM Trusted Firmware) | STMicroelectronics v2.10 |
| BL32 | OP-TEE (Secure OS) | STMicroelectronics 4.0 |
| BL33 | U-Boot | STMicroelectronics 2023.10 |
| Kernel | Linux | STMicroelectronics 6.6 |

---

## Software Stack

### Build System

SmartHub uses **Buildroot 2025.02.5** with two external trees:

1. **buildroot-external-st** (Bootlin) — Kernel, bootloader, device tree
2. **buildroot/smarthub** (custom) — SmartHub-specific packages and config

```
~/projects/smarthub/
├── buildroot-src/           # Buildroot source
├── buildroot-external-st/   # Bootlin ST external tree
└── STM32-Smart-Home-Hub/
    └── buildroot/           # SmartHub external tree
        ├── Config.in
        ├── external.desc
        ├── external.mk
        ├── configs/smarthub_defconfig
        └── board/smarthub/
            ├── overlay/     # Root filesystem overlay
            ├── post_build.sh
            └── post_image.sh
```

### Installed Packages

| Package | Purpose | Port/Interface |
|---------|---------|----------------|
| Mosquitto | MQTT broker | 1883/tcp |
| Dropbear | SSH server | 22/tcp |
| Avahi | mDNS/DNS-SD | 5353/udp |
| wpa_supplicant | WiFi authentication | — |
| dhcpcd | DHCP client | — |
| SQLite | Local database | — |
| OpenAMP | A7↔M4 communication | — |

### Init System

SmartHub uses **BusyBox init** for fast boot times:

| Script | Service |
|--------|---------|
| S01syslogd | System logging |
| S02klogd | Kernel logging |
| S10mdev | Device manager |
| S30tee-supplicant | OP-TEE client |
| S40network | Ethernet + WiFi |
| S50dropbear | SSH server |
| S50mosquitto | MQTT broker |
| S80remoteproc | M4 firmware loader |

---

## Communication Architecture

### MQTT Message Bus

All SmartHub components communicate via MQTT:

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Home Asst.    │     │    SmartHub     │     │   M4 Firmware   │
│   (external)    │     │    Daemon       │     │   (via RPMsg)   │
└────────┬────────┘     └────────┬────────┘     └────────┬────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   Mosquitto Broker      │
                    │   localhost:1883        │
                    └─────────────────────────┘
```

Topic hierarchy (planned):
```
smarthub/
├── status              # Hub status
├── sensors/            # Sensor readings
│   ├── temperature
│   └── humidity
├── actuators/          # Control outputs
│   ├── relay/1
│   └── relay/2
└── m4/                 # M4 coprocessor
    ├── status
    └── data
```

### A7 ↔ M4 Communication

The Cortex-M4 coprocessor communicates with Linux via **OpenAMP/RPMsg**:

```
┌──────────────────────────────────────────────────────────────────┐
│                     Linux (Cortex-A7)                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐   │
│  │ SmartHub    │    │ /dev/       │    │ remoteproc driver   │   │
│  │ Daemon      │───►│ ttyRPMSG0   │───►│ /sys/class/         │   │
│  │             │    │             │    │ remoteproc/         │   │
│  └─────────────┘    └─────────────┘    └──────────┬──────────┘   │
│                                                    │              │
├────────────────────────────────────────────────────┼──────────────┤
│                      Shared Memory                 │              │
│  ┌─────────────────────────────────────────────────┼──────────────┤
│  │              VirtIO Buffers                     │              │
│  └─────────────────────────────────────────────────┼──────────────┤
│                                                    │              │
├────────────────────────────────────────────────────┼──────────────┤
│                   M4 Firmware (Cortex-M4)          │              │
│  ┌─────────────┐    ┌─────────────┐    ┌──────────▼──────────┐   │
│  │ Sensor      │    │  OpenAMP    │    │    RPMSG endpoint   │   │
│  │ Handler     │───►│  Library    │───►│                     │   │
│  └─────────────┘    └─────────────┘    └─────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

---

## Network Architecture

### Interfaces

| Interface | Purpose | IP Assignment |
|-----------|---------|---------------|
| `eth0` | Wired Ethernet | DHCP |
| `wlan0` | WiFi (2.4 GHz) | DHCP |
| `lo` | Loopback | 127.0.0.1 |

### Service Discovery

SmartHub advertises itself via mDNS:

| Name | Type | Port |
|------|------|------|
| `smarthub.local` | A | — |
| `_mqtt._tcp.local` | SRV | 1883 |
| `_ssh._tcp.local` | SRV | 22 |

---

## Security Considerations

### Current State (Development)

- Root login via SSH with key authentication
- MQTT broker allows anonymous local connections
- No TLS/SSL on MQTT (planned for Phase 4)

### Planned (Production)

- MQTT with TLS and password authentication
- OP-TEE secure storage for credentials
- Signed firmware updates

---

## Memory Budget

| Component | Usage |
|-----------|-------|
| Kernel + modules | ~10 MB |
| System services | ~8 MB |
| Network stack | ~4 MB |
| **Idle total** | **~24 MB** |
| Available for apps | ~360 MB |

---

## Application Architecture (Phase 2-3)

The SmartHub C++ application is structured in layers:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SmartHub Application                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        Web/API Layer                                 │   │
│   │  ┌──────────────────┐  ┌──────────────────────────────────────┐    │   │
│   │  │   WebServer      │  │           REST API                   │    │   │
│   │  │   (Mongoose)     │  │  /api/devices, /api/system/status    │    │   │
│   │  └──────────────────┘  └──────────────────────────────────────┘    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                      Device Layer (Phase 3)                          │   │
│   │  ┌──────────────────┐  ┌──────────────┐  ┌────────────────────┐    │   │
│   │  │  DeviceManager   │  │ Device Types │  │  DeviceTypeRegistry│    │   │
│   │  │                  │  │ Switch/Dimmer│  │                    │    │   │
│   │  │                  │  │ ColorLight   │  │                    │    │   │
│   │  │                  │  │ Sensors      │  │                    │    │   │
│   │  └──────────────────┘  └──────────────┘  └────────────────────┘    │   │
│   │          │                                                          │   │
│   │  ┌───────▼──────────────────────────────────────────────────────┐   │   │
│   │  │                   Protocol Handlers                           │   │   │
│   │  │  ┌────────────┐  ┌──────────────┐  ┌────────────────────┐   │   │   │
│   │  │  │   MQTT     │  │  ProtocolFactory│ │ IProtocolHandler  │   │   │   │
│   │  │  │  Handler   │  │               │  │    Interface      │   │   │   │
│   │  │  └────────────┘  └──────────────┘  └────────────────────┘   │   │   │
│   │  └──────────────────────────────────────────────────────────────┘   │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                      Core Layer (Phase 2)                            │   │
│   │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐    │   │
│   │  │  EventBus  │  │   Logger   │  │   Config   │  │  Database  │    │   │
│   │  │  (pub/sub) │  │            │  │   (YAML)   │  │  (SQLite)  │    │   │
│   │  └────────────┘  └────────────┘  └────────────┘  └────────────┘    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                   Communication Layer                                │   │
│   │  ┌────────────────────────┐  ┌────────────────────────────────┐    │   │
│   │  │      MQTT Client       │  │        RPMsg Client            │    │   │
│   │  │   (libmosquitto)       │  │      (A7 ↔ M4 comm)            │    │   │
│   │  └────────────────────────┘  └────────────────────────────────┘    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Core Components (Phase 2)

| Component | Description | Source |
|-----------|-------------|--------|
| EventBus | Publish/subscribe event system | `src/core/EventBus.cpp` |
| Logger | Leveled logging (debug→error) | `src/core/Logger.cpp` |
| Config | YAML configuration loading | `src/core/Config.cpp` |
| Database | SQLite persistence layer | `src/database/Database.cpp` |
| MqttClient | MQTT communication | `src/protocols/mqtt/MqttClient.cpp` |
| RpmsgClient | M4 coprocessor communication | `src/rpmsg/RpmsgClient.cpp` |
| WebServer | HTTP REST API | `src/web/WebServer.cpp` |

### Device Layer (Phase 3)

| Component | Description | Source |
|-----------|-------------|--------|
| IDevice | Device interface | `include/smarthub/devices/IDevice.hpp` |
| Device | Base device class | `src/devices/Device.cpp` |
| SwitchDevice | On/off switch | `src/devices/types/SwitchDevice.cpp` |
| DimmerDevice | Dimmable light | `src/devices/types/DimmerDevice.cpp` |
| ColorLightDevice | Color-capable light | `src/devices/types/ColorLightDevice.cpp` |
| TemperatureSensor | Temperature sensor | `src/devices/types/TemperatureSensor.cpp` |
| MotionSensor | Motion sensor | `src/devices/types/MotionSensor.cpp` |
| DeviceManager | Device lifecycle management | `src/devices/DeviceManager.cpp` |
| DeviceTypeRegistry | Device factory | `src/devices/DeviceTypeRegistry.cpp` |
| Room | Room organization | `src/devices/Room.cpp` |
| DeviceGroup | Group multiple devices | `src/devices/DeviceGroup.cpp` |

### Protocol Layer (Phase 3)

| Component | Description | Source |
|-----------|-------------|--------|
| IProtocolHandler | Protocol interface | `include/smarthub/protocols/IProtocolHandler.hpp` |
| ProtocolFactory | Protocol creation | `src/protocols/ProtocolFactory.cpp` |

### Data Flow

```
                    External Device (Zigbee/MQTT)
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Protocol Handler (MQTT)                       │
│  - Receives messages from zigbee2mqtt                           │
│  - Parses device state                                          │
│  - Calls DeviceManager callbacks                                │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
     Device Discovered   State Changed   Availability Changed
              │               │               │
              ▼               ▼               ▼
┌─────────────────────────────────────────────────────────────────┐
│                       DeviceManager                              │
│  - Adds/updates devices                                         │
│  - Persists to database                                         │
│  - Publishes events to EventBus                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
          EventBus        Database         WebServer
              │               │               │
              ▼               ▼               ▼
         Subscribers      Persistence     REST API
```

---

## References

- [STM32MP157F-DK2 Wiki](https://wiki.st.com/stm32mpu/wiki/STM32MP157F-DK2)
- [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html)
- [OpenAMP Documentation](https://github.com/OpenAMP/open-amp)
- [Device Abstraction Layer](devices.md)
- [Protocol Handlers](protocols.md)

---

*Created: 28 December 2025*
*Updated: 29 December 2025 (Phase 3)*
