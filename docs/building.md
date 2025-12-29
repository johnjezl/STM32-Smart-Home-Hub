# Building SmartHub

This document describes how to build, flash, and run SmartHub.

---

## Prerequisites

### Required Tools

| Tool | Purpose | Installation |
|------|---------|--------------|
| `git` | Source control | `sudo apt install git` |
| `build-essential` | Compiler toolchain | `sudo apt install build-essential` |
| `libncurses-dev` | menuconfig | `sudo apt install libncurses-dev` |
| `unzip`, `wget`, `curl` | Downloads | `sudo apt install unzip wget curl` |
| `python3` | Build scripts | `sudo apt install python3` |

### Install All Dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y \
    build-essential git cmake wget curl unzip \
    libncurses-dev flex bison libssl-dev bc \
    cpio rsync file gawk libelf-dev \
    python3 python3-pip
```

### Disk Space

Buildroot requires significant disk space:

| Item | Size |
|------|------|
| Buildroot source | ~100 MB |
| Build output | ~5 GB |
| Downloaded packages | ~2 GB |
| **Total recommended** | **10+ GB** |

---

## Repository Setup

### Clone Repositories

```bash
mkdir -p ~/projects/smarthub
cd ~/projects/smarthub

# Clone Buildroot
git clone https://github.com/buildroot/buildroot.git buildroot-src
cd buildroot-src
git checkout 2025.02.5

# Clone Bootlin ST external tree
cd ~/projects/smarthub
git clone https://github.com/bootlin/buildroot-external-st.git
cd buildroot-external-st
git checkout st/2025.02.5

# Clone SmartHub project
cd ~/projects/smarthub
git clone <your-repo-url> STM32-Smart-Home-Hub
```

### Directory Structure

```
~/projects/smarthub/
├── buildroot-src/              # Buildroot source tree
│   ├── output/                 # Build artifacts
│   │   ├── build/              # Package build directories
│   │   ├── images/             # Final images (sdcard.img)
│   │   └── target/             # Root filesystem
│   └── .config                 # Current configuration
│
├── buildroot-external-st/      # Bootlin ST external tree
│   ├── board/stmicroelectronics/
│   ├── configs/                # ST defconfigs
│   └── package/                # ST-specific packages
│
└── STM32-Smart-Home-Hub/       # SmartHub project
    ├── buildroot/              # SmartHub external tree
    │   ├── Config.in
    │   ├── external.desc
    │   ├── external.mk
    │   ├── configs/smarthub_defconfig
    │   └── board/smarthub/
    │       ├── overlay/        # Root filesystem overlay
    │       ├── post_build.sh
    │       └── post_image.sh
    ├── docs/                   # Documentation
    └── tools/                  # Build & lab scripts
```

---

## Build Targets

### Quick Reference

| Command | Description |
|---------|-------------|
| `./tools/build.sh` | Full build (default) |
| `./tools/build.sh config` | Apply smarthub_defconfig |
| `./tools/build.sh menuconfig` | Interactive configuration |
| `./tools/build.sh savedefconfig` | Save current config |
| `./tools/build.sh clean` | Clean build directory |
| `./tools/build.sh rebuild` | Full clean rebuild |
| `./tools/flash.sh` | Flash to SD card |

### First Build

```bash
cd ~/projects/smarthub/STM32-Smart-Home-Hub

# Configure and build
./tools/build.sh

# First build takes 30-60 minutes
# Subsequent builds are much faster (~5 minutes)
```

### Manual Build (without script)

```bash
cd ~/projects/smarthub/buildroot-src

# Set external trees
export BR2_EXTERNAL="/home/$USER/projects/smarthub/buildroot-external-st:/home/$USER/projects/smarthub/STM32-Smart-Home-Hub/buildroot"

# Apply configuration
make smarthub_defconfig

# Optional: modify configuration
make menuconfig

# Build
make -j$(nproc)
```

### Build Output

After successful build:

```bash
ls ~/projects/smarthub/buildroot-src/output/images/
```

| File | Description |
|------|-------------|
| `sdcard.img` | Complete SD card image (flash this) |
| `sdcard.img.gz` | Compressed image |
| `rootfs.ext4` | Root filesystem |
| `zImage` | Linux kernel |
| `stm32mp157f-dk2.dtb` | Device tree |
| `tf-a-stm32mp157f-dk2.stm32` | TF-A bootloader |
| `fip.bin` | Firmware Image Package (U-Boot + OP-TEE) |
| `u-boot-nodtb.bin` | U-Boot binary |

---

## Flashing

### Using flash.sh Script

```bash
# Auto-detect SD card
./tools/flash.sh

# Or specify device
./tools/flash.sh /dev/sdX
```

### Manual Flash

```bash
# Find your SD card
lsblk

# Unmount any mounted partitions
sudo umount /dev/sdX*

# Flash (CAREFUL: verify correct device!)
sudo dd if=~/projects/smarthub/buildroot-src/output/images/sdcard.img \
    of=/dev/sdX bs=4M conv=fsync status=progress

# Sync
sync
```

### SD Card Layout

After flashing, the SD card contains:

| Partition | Type | Size | Contents |
|-----------|------|------|----------|
| fsbl1 | raw | 256K | TF-A first copy |
| fsbl2 | raw | 256K | TF-A second copy |
| fip | raw | 4M | U-Boot + OP-TEE |
| u-boot-env | raw | 512K | U-Boot environment |
| rootfs | ext4 | 200M | Root filesystem |

---

## Configuration

### menuconfig

```bash
./tools/build.sh menuconfig
```

Key locations:
- **Target packages** → Package selection
- **Filesystem images** → Image generation options
- **Bootloaders** → U-Boot, TF-A configuration
- **Kernel** → Linux kernel options

### Save Configuration Changes

After modifying with menuconfig:

```bash
./tools/build.sh savedefconfig
```

This updates `buildroot/configs/smarthub_defconfig`.

### Adding Packages

1. Enable in menuconfig: `Target packages → <category> → <package>`
2. Or add to defconfig: `BR2_PACKAGE_<NAME>=y`
3. Rebuild: `./tools/build.sh`

### Modifying Root Filesystem

Add files to overlay directory:
```
buildroot/board/smarthub/overlay/
```

Files are copied to rootfs during build.

---

## Rebuilding

### Incremental Build

After code changes:
```bash
./tools/build.sh
```

Only changed packages rebuild.

### Package Rebuild

Force rebuild of specific package:
```bash
cd ~/projects/smarthub/buildroot-src
make <package>-rebuild
```

### Full Rebuild

Complete clean rebuild:
```bash
./tools/build.sh rebuild
# or
./tools/build.sh clean && ./tools/build.sh
```

---

## Troubleshooting

### Build Fails

```bash
# Check last error
make 2>&1 | tail -50

# Rebuild failed package
make <package>-dirclean
make <package>-rebuild
```

### Missing Dependencies

```bash
# Buildroot checks for host tools
make host-<tool>
```

### Out of Disk Space

```bash
# Check space
df -h

# Clean downloads (recoverable)
rm -rf ~/projects/smarthub/buildroot-src/dl/*

# Clean build (requires full rebuild)
./tools/build.sh clean
```

### Configuration Issues

```bash
# Reset to default config
./tools/build.sh config

# Check for config errors
make savedefconfig
diff buildroot/configs/smarthub_defconfig .config
```

---

## Development Workflow

### Typical Cycle

1. Make changes to overlay/scripts
2. `./tools/build.sh` (fast, ~2 min)
3. `./tools/flash.sh`
4. Insert SD, power cycle, test
5. Repeat

### Testing Without Reflash

For quick iterations, deploy files via SSH:

```bash
# Copy updated binary
scp myapp root@smarthub.local:/opt/smarthub/bin/

# Or sync overlay
rsync -avz buildroot/board/smarthub/overlay/ root@smarthub.local:/
```

### NFS Root (Advanced)

For fastest iteration, boot from NFS:

```bash
# On host: export rootfs via NFS
# On target: modify U-Boot to boot from NFS
# Changes appear instantly without reflash
```

---

## Build Times

Measured on AMD Ryzen 7 5800X (8 cores):

| Build Type | Time |
|------------|------|
| First build (empty cache) | 45-60 min |
| Rebuild after package change | 5-10 min |
| Rebuild after overlay change | 2-3 min |
| Clean rebuild | 30-45 min |

---

## Cross-Compiling the SmartHub Application

For rapid development, you can cross-compile just the SmartHub C++ application without rebuilding the entire Buildroot image.

### Prerequisites

Install the ARM cross-compiler:

```bash
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

Ensure Buildroot sysroot exists (from a previous full build):
```
~/projects/smarthub/buildroot-src/output/staging/
```

### Cross-Compilation

```bash
cd ~/projects/smarthub/STM32-Smart-Home-Hub/app

# Create ARM build directory
mkdir -p build-arm && cd build-arm

# Configure with toolchain file
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-linux-gnueabihf.cmake \
    -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Verify ARM binary
file smarthub
# Should show: ELF 32-bit LSB pie executable, ARM, EABI5
```

### Deployment to Target

```bash
# Copy binary to board (use -O for legacy SCP if sftp not available)
scp -O build-arm/smarthub root@192.168.4.99:/opt/smarthub/bin/

# Or to /tmp for testing
scp -O build-arm/smarthub root@192.168.4.99:/tmp/
```

### Testing on Target

```bash
# SSH to board
ssh root@192.168.4.99

# Test help
/tmp/smarthub --help

# Run application
/tmp/smarthub

# Check if running
ps aux | grep smarthub
netstat -tlnp | grep smarthub
```

### Optional Dependencies

Some dependencies are optional and the application will work without them:

| Library | Status | Fallback |
|---------|--------|----------|
| yaml-cpp | Optional | Config.cpp uses INI-style parser |
| LVGL | Optional | UI disabled, web interface only |
| nlohmann_json | Required | Bundled in third_party/ |

If a library is found by pkg-config on the host but not in the sysroot, the build will automatically skip linking it.

### Troubleshooting Cross-Compilation

**nlohmann/json.hpp not found:**
```bash
# The header is bundled in third_party/
ls app/third_party/nlohmann/json.hpp
```

**yaml-cpp linking fails:**
The build system only links yaml-cpp if the library file is actually found in the sysroot. Config.cpp has a fallback INI parser.

**Library not found at runtime:**
```bash
# Check library dependencies on target
ssh root@<board-ip> "ldd /tmp/smarthub"
```

---

## References

- [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html)
- [Buildroot Tips](https://buildroot.org/downloads/manual/manual.html#_tips_and_tricks)
- [STM32MP1 Wiki](https://wiki.st.com/stm32mpu)

---

*Created: 28 December 2025*
*Updated: 29 December 2025 - Added cross-compilation section*
