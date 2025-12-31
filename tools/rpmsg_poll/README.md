# rpmsg_poll - VirtIO Vring Polling Daemon

Userspace tool for receiving RPMsg messages from the M4 coprocessor by directly polling the VirtIO vring shared memory.

## Purpose

This tool is used as a workaround when TrustZone security configuration blocks M4 access to the IPCC (Inter-Processor Communication Controller). Instead of relying on hardware interrupts, it polls the VirtIO used ring to detect new messages from the M4.

## Requirements

- Root access (requires `/dev/mem` access)
- M4 firmware must be loaded and running via remoteproc
- STM32MP157 device tree must define the vring memory regions

## Building

### Native Build (on STM32MP157)
```bash
cd tools/rpmsg_poll
make
```

### Cross-Compile (with Buildroot toolchain)
```bash
cd tools/rpmsg_poll
make buildroot
```

## Usage

```bash
# Dump current vring state (useful for debugging)
./rpmsg_poll --dump

# Poll continuously with default 100ms interval
./rpmsg_poll

# Poll with custom interval
./rpmsg_poll --interval 50

# Read one message and exit
./rpmsg_poll --once

# Verbose output (shows memory mappings)
./rpmsg_poll --verbose
```

### Options

| Option | Short | Description |
|--------|-------|-------------|
| `--interval MS` | `-i` | Poll interval in milliseconds (default: 100) |
| `--verbose` | `-v` | Show verbose output including memory mappings |
| `--once` | `-1` | Read messages once and exit |
| `--dump` | `-d` | Dump vring state and exit |
| `--help` | `-h` | Show help message |

## Memory Map

The tool accesses the following physical memory regions defined in the STM32MP157 device tree:

| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| vdev0vring0 | 0x10040000 | 4KB | TX vring (M4 → A7) - **polled** |
| vdev0vring1 | 0x10041000 | 4KB | RX vring (A7 → M4) |
| vdev0buffer | 0x10042000 | 16KB | Shared message buffers |

## VirtIO Vring Layout

```
Offset    Contents
0x000     Descriptor table (16 entries × 16 bytes)
0x080     Available ring (flags, idx, ring[16])
0x0A0     Used ring (flags, idx, ring[16] × 8 bytes)
```

The tool monitors `used.idx` to detect when the M4 has added new messages.

## Output Format

Messages are displayed with their RPMsg header fields:

```
RPMsg: src=0x1234 dst=0x0400 len=32
  Data: rpmsg-smarthub-m4
```

Binary data is displayed as hex dump when not printable.

## Integration with SmartHub

The main SmartHub application can use rpmsg_poll in several ways:

1. **Subprocess**: Run rpmsg_poll and parse its stdout
2. **Library**: Link against the parsing functions directly
3. **M4CommManager**: Use the C++ wrapper class (see `app/include/smarthub/m4/M4CommManager.hpp`)

Example subprocess usage:
```cpp
FILE* pipe = popen("rpmsg_poll --once", "r");
char buffer[256];
while (fgets(buffer, sizeof(buffer), pipe)) {
    // Parse RPMsg output
}
pclose(pipe);
```

## Troubleshooting

### "open /dev/mem (need root)"
Run with sudo or as root. The tool requires physical memory access.

### "mmap vring0: Operation not permitted"
Ensure `/dev/mem` is available and not restricted by kernel config.

### No messages appearing
1. Verify M4 firmware is running: `cat /sys/class/remoteproc/remoteproc0/state`
2. Check vring state with `--dump` to see if used.idx > 0
3. Verify M4 trace output: `cat /sys/kernel/debug/remoteproc/remoteproc0/trace0`

### Messages show garbled data
- Buffer address mismatch between M4 and device tree
- M4 firmware using different vring configuration
- Memory alignment issues

## Why Polling Instead of Interrupts?

On the STM32MP157F-DK2 with the default TF-A + OP-TEE configuration, the M4 core cannot access the IPCC peripheral due to TrustZone security restrictions. When the M4 attempts to write to IPCC registers (0x4C001000), it triggers a bus fault.

The VirtIO vring protocol still works - the M4 can write messages to the shared memory vring - but it cannot notify the A7 via IPCC interrupts. This tool polls the vring to detect new messages as a workaround.

See `docs/PHASE_6_TODO.md` for details on the TrustZone issue and potential solutions.

## Source Files

| File | Description |
|------|-------------|
| rpmsg_poll.c | Main polling daemon |
| Makefile | Build rules for native and cross-compile |
