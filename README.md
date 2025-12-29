# SmartHub

A custom smart home hub built on the STM32MP157F-DK2 development board featuring local-only operation, touch-screen control, and modular device integration.

[![CI](https://github.com/johnjezl/STM32-Smart-Home-Hub/actions/workflows/ci.yml/badge.svg)](https://github.com/johnjezl/STM32-Smart-Home-Hub/actions/workflows/ci.yml)

## Features

- **Local-only operation** - No internet or cloud required
- **Dual-core architecture** - A7 Linux + M4 real-time firmware
- **Touch-screen UI** - 4" 480x800 LVGL-based interface
- **Secure web interface** - HTTPS REST API with local authentication
- **MQTT broker** - Built-in Mosquitto for device communication
- **Zigbee support** - Via CC2652P coordinator (planned)
- **WiFi device control** - Tasmota, Shelly, Tuya local protocols (planned)

## Project Status

| Phase | Name | Status |
|-------|------|--------|
| 0 | Development Environment Setup | ✅ Complete |
| 1 | Buildroot Base System | ✅ Complete |
| 2 | Core Application Framework | ✅ Complete |
| 3 | Device Integration Framework | ✅ Complete |
| 4 | Zigbee Integration | 🔄 In Progress |
| 5 | WiFi Device Integration | ⏳ Planned |
| 6 | M4 Bare-Metal Firmware | ⏳ Planned |
| 7 | Security Hardening | ⏳ Planned |
| 8 | UI Refinement & Polish | ⏳ Planned |

## Hardware

| Component | Specification |
|-----------|---------------|
| **Board** | STM32MP157F-DK2 |
| **A7 Cores** | Dual Cortex-A7 @ 800 MHz (Buildroot Linux) |
| **M4 Core** | Cortex-M4 @ 209 MHz (bare-metal C++) |
| **Display** | 4" 480×800 MIPI DSI touchscreen |
| **WiFi** | BCM43430 802.11b/g/n |
| **Ethernet** | Gigabit via RTL8211F PHY |
| **Bluetooth** | BLE 4.1 |

## Technology Stack

| Component | Technology |
|-----------|------------|
| A7 Operating System | Buildroot 2025.02.5 minimal Linux |
| A7 Application | C++17 |
| M4 Firmware | C++17 (bare-metal) |
| GUI Framework | LVGL 9.x |
| Web Server | Mongoose |
| Database | SQLite 3 |
| MQTT | Mosquitto (broker) + libmosquitto (client) |
| IPC | OpenAMP/RPMsg |
| Build System | CMake + Buildroot |
| Testing | Google Test |
| CI/CD | GitHub Actions |

## Project Structure

```
├── app/                    # A7 Linux application (C++17)
│   ├── include/smarthub/  # Public headers
│   ├── src/               # Implementation
│   ├── tests/             # Unit and integration tests
│   └── CMakeLists.txt     # Build configuration
├── m4-firmware/           # M4 bare-metal firmware (C++17)
├── buildroot/             # Buildroot external tree
│   ├── configs/           # defconfig files
│   └── board/smarthub/    # Board-specific files
├── tools/                 # Development utilities
│   ├── build.sh           # Buildroot wrapper
│   ├── flash.sh           # SD card flashing
│   └── test.sh            # Test runner
├── docs/                  # Documentation
│   ├── MASTER_PLAN.md     # Project roadmap
│   ├── architecture.md    # System design
│   ├── building.md        # Build instructions
│   └── testing.md         # Testing guide
└── .github/workflows/     # CI/CD pipelines
```

## Quick Start

### Prerequisites

- Ubuntu 22.04+ or similar Linux distribution
- CMake 3.16+
- GCC 11+ with C++17 support
- SQLite3, OpenSSL, yaml-cpp, nlohmann-json development packages

### Building the Application (Native)

```bash
cd app
cmake -B build -DSMARTHUB_BUILD_TESTS=ON
cmake --build build

# Run the application
./build/smarthub
```

### Running Tests

```bash
# Using the convenience script
./tools/test.sh

# Or directly with CMake
cd app/build && ctest --output-on-failure
```

### Building the Buildroot Image

```bash
# Full build (first time takes ~1 hour)
./tools/build.sh

# Flash to SD card
./tools/flash.sh /dev/sdX
```

See [docs/building.md](docs/building.md) for detailed instructions.

## Performance Metrics (Phase 1)

| Metric | Value | Target |
|--------|-------|--------|
| Boot time | ~8 seconds | <10 seconds |
| RAM usage | 24.7 MB | <64 MB |
| Rootfs size | 48.7 MB | <100 MB |

## Documentation

- [Master Plan](docs/MASTER_PLAN.md) - Full project roadmap and phases
- [Architecture](docs/architecture.md) - System design and diagrams
- [Building](docs/building.md) - Build and flash instructions
- [Buildroot](docs/buildroot.md) - Linux image configuration
- [Networking](docs/networking.md) - WiFi, Ethernet, mDNS setup
- [REST API](docs/api.md) - Web server REST API reference
- [MQTT Client](docs/mqtt.md) - MQTT protocol integration
- [RPMsg/M4](docs/rpmsg.md) - Cortex-M4 communication
- [Testing](docs/testing.md) - Testing infrastructure guide
- [Lab Operations](docs/lab-operations.md) - Hardware interaction guide

## Contributing

This is a personal project, but suggestions and feedback are welcome via GitHub issues.

## License

TBD
