# SmartHub

A custom smart home hub built on the STM32MP157F-DK2 development board featuring local-only operation, touch-screen control, and modular device integration.

## Features

- **Local-only operation** - No internet or cloud required
- **Touch-screen UI** - 4" 480x800 LVGL-based interface
- **Secure web interface** - HTTPS REST API with local authentication
- **Zigbee support** - Via CC2652P coordinator
- **WiFi device control** - Tasmota, Shelly, Tuya (local protocol)
- **Real-time sensors** - M4 core handles time-critical acquisition

## Hardware

- **Board**: STM32MP157F-DK2
- **A7 Cores**: Dual Cortex-A7 @ 800 MHz (Buildroot Linux)
- **M4 Core**: Cortex-M4 @ 209 MHz (bare-metal C++)
- **Connectivity**: WiFi 802.11b/g/n, Gigabit Ethernet, BLE 4.1

## Building

```bash
# A7 application
mkdir build && cd build
cmake ..
make

# M4 firmware (requires arm-none-eabi toolchain)
mkdir build-m4 && cd build-m4
cmake ../m4-firmware
make
```

## Project Structure

```
├── app/            # A7 Linux application (C++17)
├── m4-firmware/    # M4 bare-metal firmware (C++17)
├── buildroot/      # Buildroot external tree
├── web-ui/         # Web interface
├── tools/          # Development utilities
├── tests/          # Unit and integration tests
└── docs/           # Documentation
```

## Documentation

See `docs/MASTER_PLAN.md` for the full project plan and phase breakdown.

## License

TBD
