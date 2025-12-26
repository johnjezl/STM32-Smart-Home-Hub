# STM32MP157F-DK2 Smart Home Hub - Master Project Plan

## Project Overview

A custom smart home hub built on the STM32MP157F-DK2 development board, featuring local-only operation (no internet required), touch-screen control panel, secure web interface, and modular device integration architecture.

### Target Hardware
- **Board**: STM32MP157F-DK2
- **A7 Cores**: Dual Cortex-A7 @ 800 MHz running Buildroot Linux
- **M4 Core**: Cortex-M4 @ 209 MHz running bare-metal C++ firmware
- **Display**: 4" 480×800 MIPI DSI touchscreen
- **Connectivity**: WiFi 802.11b/g/n, Gigabit Ethernet, BLE 4.1
- **External Modules**: CC2652P Zigbee coordinator, 433 MHz RF TX/RX

### Technology Stack
| Component | Technology |
|-----------|------------|
| A7 Operating System | Buildroot minimal Linux |
| A7 Language | C++17 |
| M4 Language | C++17 (bare-metal) |
| GUI Framework | LVGL 9.x |
| Web Server | Mongoose |
| Database | SQLite 3 |
| MQTT | Mosquitto (broker) + libmosquitto (client) |
| IPC | OpenAMP/RPMsg |
| Build System | CMake + Buildroot |

---

## Phase Summary

| Phase | Name | Duration | Dependencies |
|-------|------|----------|--------------|
| 0 | Development Environment Setup | 1 week | None |
| 1 | Buildroot Base System | 1-2 weeks | Phase 0 |
| 2 | Core Application Framework | 2-3 weeks | Phase 1 |
| 3 | Device Integration Framework | 2 weeks | Phase 2 |
| 4 | Zigbee Integration | 2-3 weeks | Phase 3 |
| 5 | WiFi Device Integration | 2 weeks | Phase 3 |
| 6 | M4 Bare-Metal Firmware | 2-3 weeks | Phase 2 |
| 7 | Security Hardening | 2 weeks | Phase 2, 6 |
| 8 | UI Refinement & Polish | 2-3 weeks | Phase 4, 5, 6 |

**Estimated Total Duration**: 4-6 months

---

## Phase 0: Development Environment Setup

**Objective**: Establish development infrastructure, verify hardware functionality, and validate toolchain.

### Key Deliverables
- Working STM32MP157F-DK2 with stock OpenSTLinux
- Host development machine configured with all tools
- Verified WiFi, Ethernet, display, and touchscreen functionality
- ST-Link debugging operational
- Serial console access established

### Success Criteria
- Can SSH into board over WiFi and Ethernet
- Touchscreen responds to input
- Can deploy and debug M4 firmware via ST-Link
- Serial console shows boot messages

---

## Phase 1: Buildroot Base System

**Objective**: Create minimal, optimized Linux image tailored for the smart home hub.

### Key Deliverables
- Buildroot environment configured for STM32MP157F-DK2
- Custom defconfig with minimal footprint
- Boot time under 10 seconds
- WiFi and Ethernet networking functional
- Framebuffer/DRM display output working
- OpenAMP/remoteproc configured for M4 communication

### Success Criteria
- System boots to shell in <10 seconds
- WiFi connects to WPA2 network via wpa_supplicant
- RAM usage <64 MB at idle
- Rootfs size <100 MB
- M4 firmware can be loaded via remoteproc

---

## Phase 2: Core Application Framework

**Objective**: Implement the foundational application architecture that all features build upon.

### Key Deliverables
- CMake-based C++ project structure
- LVGL integration with touchscreen input
- Mongoose HTTP/HTTPS server with REST API skeleton
- SQLite database with schema for devices, state, and configuration
- Mosquitto MQTT broker running as service
- Event bus for internal component communication
- Configuration management system (YAML-based)
- Logging framework

### Success Criteria
- LVGL demo UI renders on display with touch response
- REST API returns system status over HTTPS
- MQTT broker accepts connections
- Database stores and retrieves test records
- Events propagate between components

---

## Phase 3: Device Integration Framework

**Objective**: Create the modular plugin architecture for device types and protocols.

### Key Deliverables
- Abstract device interface (IDevice)
- Protocol handler interface (IProtocolHandler)
- Device registry with discovery/pairing workflow
- State management for all device types
- Plugin loader/manager
- Device type definitions (switches, dimmers, sensors, etc.)

### Success Criteria
- Can register mock devices through the framework
- Device state changes propagate to UI and REST API
- Plugin architecture supports runtime loading
- Device configuration persists across reboots

---

## Phase 4: Zigbee Integration

**Objective**: Enable control and monitoring of Zigbee devices via CC2652P coordinator.

### Key Deliverables
- CC2652P hardware integration (UART connection)
- Z-Stack ZNP protocol implementation
- Zigbee device database (manufacturer signatures, clusters)
- Pairing mode UI and workflow
- Support for common device types:
  - On/Off switches and outlets
  - Dimmable lights
  - Color temperature / RGB lights
  - Temperature/humidity sensors
  - Contact sensors
  - Motion sensors

### Success Criteria
- Can pair Zigbee devices through UI
- Control commands execute within 200ms
- Sensor updates appear in real-time
- Devices reconnect automatically after power cycle
- Mesh network forms with multiple devices

---

## Phase 5: WiFi Device Integration

**Objective**: Enable control of WiFi-based smart devices over local network.

### Key Deliverables
- MQTT device protocol (Tasmota/ESPHome compatible)
- HTTP REST device protocol (Shelly)
- Tuya local protocol implementation
- Device discovery (mDNS, MQTT autodiscovery)
- Device templates for common products

### Success Criteria
- Tasmota devices controllable via MQTT
- Shelly devices controllable via REST API
- Tuya devices controllable locally (no cloud)
- Device discovery finds devices automatically
- State synchronization within 500ms

---

## Phase 6: M4 Bare-Metal Firmware

**Objective**: Implement real-time sensor acquisition and time-critical functions on the M4 core.

### Key Deliverables
- Bare-metal C++ firmware structure
- RPMsg communication with Linux A7
- I2C sensor driver framework
- Supported sensors:
  - SHT31 (temperature/humidity)
  - BME280 (temperature/humidity/pressure)
  - BH1750 (light level)
  - SCD40 (CO2)
- UART driver for future Zigbee offloading
- ADC driver for analog sensors
- Watchdog and error handling

### Success Criteria
- M4 boots and establishes RPMsg channel
- Sensor data streams to A7 at configurable intervals
- Sensor polling jitter <1ms
- M4 firmware survives A7 reboot
- Resource usage reported to A7

---

## Phase 7: Security Hardening

**Objective**: Implement security measures appropriate for a local network device.

### Key Deliverables
- TLS certificate generation and management
- Secure boot chain (optional, evaluated)
- Web UI authentication (local accounts)
- API token authentication
- Encrypted device credential storage
- Secure WiFi credential storage
- Firewall rules (iptables)

### Success Criteria
- All HTTP traffic redirects to HTTPS
- API requires valid authentication token
- Credentials not stored in plaintext
- Failed login attempts are rate-limited
- Security scan shows no critical vulnerabilities

---

## Phase 8: UI Refinement & Polish

**Objective**: Create a polished, user-friendly touch interface.

### Key Deliverables
- Dashboard home screen with device summary
- Room/zone organization
- Device control screens by type
- Sensor history graphs
- WiFi configuration wizard (SSID scan, password entry)
- Settings and configuration screens
- Virtual keyboard integration
- Theme system (light/dark modes)
- Screen saver / display sleep

### Success Criteria
- All functions accessible via touch (no keyboard needed)
- UI responsive (<100ms touch feedback)
- Can configure WiFi from first boot
- Intuitive navigation (user testing)
- Consistent visual design throughout

---

## Project Structure

```
smarthub/
├── docs/                      # Documentation
│   ├── MASTER_PLAN.md
│   ├── PHASE_X_TODO.md
│   └── api/                   # API documentation
├── buildroot/                 # Buildroot external tree
│   ├── board/
│   │   └── smarthub/
│   │       ├── overlay/       # Root filesystem overlay
│   │       ├── post_build.sh
│   │       └── post_image.sh
│   ├── configs/
│   │   └── smarthub_defconfig
│   └── package/               # Custom packages
│       └── smarthub/
├── app/                       # Main A7 application
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp
│   │   ├── core/              # Event bus, config, logging
│   │   ├── database/          # SQLite wrapper
│   │   ├── devices/           # Device framework
│   │   ├── protocols/         # Protocol handlers
│   │   │   ├── zigbee/
│   │   │   ├── mqtt/
│   │   │   ├── http/
│   │   │   └── tuya/
│   │   ├── ui/                # LVGL screens and widgets
│   │   ├── web/               # REST API and web server
│   │   └── rpmsg/             # M4 communication
│   ├── include/
│   └── assets/                # UI assets, fonts, images
├── m4-firmware/               # M4 bare-metal firmware
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp
│   │   ├── drivers/           # HAL wrappers
│   │   ├── sensors/           # Sensor drivers
│   │   └── rpmsg/             # Communication with A7
│   ├── include/
│   └── linker/
│       └── stm32mp15xx_m4.ld
├── web-ui/                    # Web interface (optional SPA)
│   ├── src/
│   └── dist/                  # Built static files
├── tools/                     # Development utilities
│   ├── provision.sh           # Initial device setup
│   └── deploy.sh              # Deployment script
└── tests/                     # Unit and integration tests
    ├── unit/
    └── integration/
```

---

## Hardware Shopping List

### Required for Phase 0-2
| Item | Purpose | Est. Cost |
|------|---------|-----------|
| STM32MP157F-DK2 | Main board (already owned) | - |
| MicroSD card (32GB+) | Boot media | $10 |
| USB-C power supply (5V/3A) | Board power | $15 |
| USB-UART adapter | Serial console | $10 |
| Ethernet cable | Initial setup | $5 |

### Required for Phase 4 (Zigbee)
| Item | Purpose | Est. Cost |
|------|---------|-----------|
| CC2652P module (SONOFF Zigbee 3.0 USB or equivalent) | Zigbee coordinator | $15-25 |
| Dupont jumper wires | UART connection | $5 |
| Test Zigbee devices (bulb, sensor, outlet) | Testing | $30-50 |

### Required for Phase 6 (Sensors)
| Item | Purpose | Est. Cost |
|------|---------|-----------|
| SHT31 breakout | Temperature/humidity | $10 |
| BME280 breakout | Temp/humidity/pressure | $8 |
| BH1750 breakout | Light level | $5 |
| Breadboard + wires | Prototyping | $10 |

### Optional
| Item | Purpose | Est. Cost |
|------|---------|-----------|
| 433 MHz TX/RX modules | Legacy RF devices | $5 |
| SCD40 breakout | CO2 sensing | $50 |
| LD2410C mmWave radar | Presence detection | $8 |

---

## Risk Register

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| WiFi driver issues with custom kernel | High | Medium | Start with ST's known-working kernel config |
| LVGL performance on framebuffer | Medium | Low | Use DRM backend with GPU acceleration |
| Zigbee device compatibility | Medium | Medium | Focus on well-documented devices first |
| RPMsg communication reliability | High | Low | Implement heartbeat and reconnection logic |
| Boot time exceeds target | Low | Medium | Profile and optimize incrementally |
| Scope creep | High | High | Strict phase gating, defer nice-to-haves |

---

## References

- [STM32MP157F-DK2 Product Page](https://www.st.com/en/evaluation-tools/stm32mp157f-dk2.html)
- [OpenSTLinux Documentation](https://wiki.st.com/stm32mpu)
- [Bootlin STM32MP1 Buildroot](https://github.com/bootlin/buildroot-external-st)
- [LVGL Documentation](https://docs.lvgl.io/)
- [Mongoose Library](https://mongoose.ws/)
- [Z-Stack ZNP Protocol](https://dev.ti.com/tirex/explore/node?node=A__ADnKkj5GYAWqT22F9nNYuQ__com.ti.SIMPLELINK_CC13XX_CC26XX_SDK__BSEc4rl__LATEST)
- [OpenAMP Documentation](https://github.com/OpenAMP/open-amp)

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | Initial | - | Initial plan creation |
