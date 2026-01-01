# ZNP Test Utilities

Standalone C programs for testing Z-Stack ZNP (Zigbee Network Processor) communication with the CC2652P USB dongle.

## Building

Cross-compile for ARM:
```bash
arm-linux-gnueabihf-gcc -o znp_init znp_init.c -static
arm-linux-gnueabihf-gcc -o znp_pair znp_pair.c -static
arm-linux-gnueabihf-gcc -o znp_control znp_control.c -static
arm-linux-gnueabihf-gcc -o znp_listen znp_listen.c -static
arm-linux-gnueabihf-gcc -o znp_list znp_list.c -static
```

## Utilities

### znp_init
Initialize the CC2652P as a Zigbee coordinator. Configures NV items:
- ZCD_NV_STARTUP_OPTION = 0x03 (clear config + state)
- ZCD_NV_LOGICAL_TYPE = 0x00 (Coordinator)
- ZCD_NV_ZDO_DIRECT_CB = 0x01 (Enable callbacks)
- ZCD_NV_CHANLIST = 0x07FFF800 (Channels 11-26)

```bash
./znp_init [port]    # Default: /dev/ttyUSB0
```

### znp_pair
Enable permit join and listen for device announcements.

```bash
./znp_pair [port]    # Ctrl+C to exit
```

### znp_control
Send ZCL On/Off commands to devices.

```bash
./znp_control <addr_hex> <on|off|toggle> [endpoint]
./znp_control 805e toggle 1
```

### znp_listen
Listen for incoming messages (e.g., from sensors).

```bash
./znp_listen [port]  # Ctrl+C to exit
```

### znp_list
Query the coordinator's neighbor table to list paired devices.

```bash
./znp_list
```

## Hardware Tested

- CC2652P USB Dongle (Amazon B0FCYLX8FT) with Z-Stack 3.x.0 ZNP firmware
- ThirdReality Zigbee Smart Outlet
- ThirdReality Garage Door Tilt Sensor (IAS Zone cluster 0x0500)

## ZNP Protocol Reference

Frame format:
```
+------+--------+------+--------+---------+-----+
| SOF  | Length | Cmd0 | Cmd1   | Payload | FCS |
| 0xFE | 1 byte | 1    | 1      | N bytes | 1   |
+------+--------+------+--------+---------+-----+
```

Message types:
- SREQ (0x20): Synchronous request
- AREQ (0x40): Asynchronous indication
- SRSP (0x60): Synchronous response

Common subsystems:
- SYS (0x01): System commands
- AF (0x04): Application Framework
- ZDO (0x05): Zigbee Device Object
- UTIL (0x07): Utility commands
