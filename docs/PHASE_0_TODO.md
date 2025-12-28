# Phase 0: Development Environment Setup - Detailed TODO

## Overview
**Duration**: 1 week
**Objective**: Establish development infrastructure, verify hardware functionality, and validate toolchain.

> **Note**: This TODO was written for Linux development. Actual implementation was done on Windows with WSL2 for Linux-specific tools. See `PHASE_0_COMPLETED.md` for the actual steps taken.

---

## 0.1 Host Machine Setup

### 0.1.1 Operating System Requirements
- ✅ Install Ubuntu 22.04 LTS (recommended) or compatible Linux distribution — *Used Windows + WSL2*
- ✅ Ensure at least 100 GB free disk space for Buildroot builds
- ✅ Minimum 16 GB RAM recommended (8 GB minimum)

### 0.1.2 Essential Development Packages
```bash
# Run on host machine
sudo apt update
sudo apt install -y \
    build-essential \
    git \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    python3-venv \
    libncurses-dev \
    flex \
    bison \
    libssl-dev \
    bc \
    u-boot-tools \
    device-tree-compiler \
    swig \
    libpython3-dev \
    cpio \
    rsync \
    unzip \
    wget \
    curl \
    screen \
    minicom \
    picocom \
    libusb-1.0-0-dev \
    libftdi-dev \
    pkg-config \
    gawk \
    file \
    patch \
    gperf \
    gettext \
    texinfo \
    help2man \
    libtool-bin \
    autoconf \
    automake
```

### 0.1.3 ARM Toolchain Installation
- ✅ Download ARM GNU Toolchain (arm-none-eabi for M4, arm-linux-gnueabihf for A7) — *Included in STM32CubeIDE*
```bash
# For M4 bare-metal development
wget https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz
sudo tar xf arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz -C /opt/

# Add to PATH in ~/.bashrc
echo 'export PATH="/opt/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify installation
arm-none-eabi-gcc --version
```

### 0.1.4 STM32CubeIDE Installation (Optional but Recommended)
- ✅ Download STM32CubeIDE from ST website (requires free account)
- ✅ Install:
```bash
chmod +x st-stm32cubeide_*.sh
sudo ./st-stm32cubeide_*.sh
```
- ✅ Launch and install STM32MP1 software package when prompted

### 0.1.5 STM32CubeProgrammer Installation
- ✅ Download STM32CubeProgrammer from ST website
- ✅ Install:
```bash
chmod +x SetupSTM32CubeProgrammer-*.linux
sudo ./SetupSTM32CubeProgrammer-*.linux
```
- ✅ Add udev rules for ST-Link: — *Windows drivers installed automatically*
```bash
sudo cp /opt/st/stm32cubeide_*/plugins/com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.linux64_*/tools/bin/config/udev/rules.d/*.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### 0.1.6 VS Code Setup (For A7 App and Web UI)
- ✅ Install VS Code
- ✅ Install extensions:
  - ✅ C/C++ (Microsoft) — *A7 app development*
  - ✅ CMake Tools — *build system*
  - ✅ Remote - SSH — *edit on target*
  - ✅ GitLens — *git integration*
  - ✅ ESLint + Prettier — *web UI*

---

## 0.2 Hardware Setup and Verification

### 0.2.1 Unbox and Inspect
- ✅ Verify all components present:
  - ✅ STM32MP157F-DK2 board
  - ✅ 4" LCD touchscreen (may be pre-attached)
  - ✅ WiFi/BLE antenna
  - ✅ Quick start guide
- ✅ Attach WiFi antenna to U.FL connector (important for WiFi range)
- ✅ Attach LCD if not pre-attached

### 0.2.2 MicroSD Card Preparation
- ✅ Obtain quality microSD card (32GB minimum, Class 10 or better) — *Used 8GB card*
- ✅ Download OpenSTLinux Starter Package:
```bash
mkdir -p ~/stm32mp1-images
cd ~/stm32mp1-images
# Download from ST website - requires free account
# https://www.st.com/en/embedded-software/stm32mp1starter.html
```
- ✅ Extract and flash image: — *Custom GPT image created, see PHASE_0_COMPLETED.md*
```bash
unzip en.stm32mp1-openstlinux-*.zip
cd stm32mp1-openstlinux-*/images/stm32mp1/

# Find your SD card device (CAREFUL - verify correct device!)
lsblk

# Flash (replace sdX with your device)
sudo dd if=flashlayout_st-image-weston/stm32mp157f-dk2/FlashLayout_sdcard_stm32mp157f-dk2-optee.raw of=/dev/sdX bs=8M conv=fsync status=progress
sync
```

### 0.2.3 Initial Boot
- ✅ Insert microSD card into board (slot on underside)
- ✅ Connect USB-C power supply
- ✅ Verify boot switches are set correctly (SW1: **BOOT0=ON, BOOT2=ON** for SD boot)
  - **Note**: Both switches should be ON (away from "BOOT" text) for SD card boot
- ✅ Connect USB-C (ST-Link) to host for serial console
- ✅ Open serial terminal: — *COM8 / /dev/ttyS7 via picocom*
```bash
# Find the serial device
ls /dev/ttyACM*

# Connect (typically ttyACM0)
picocom -b 115200 /dev/ttyACM0
# Or
screen /dev/ttyACM0 115200
```
- ✅ Power on and observe boot messages
- ✅ Login with default credentials (root, no password on starter image)

### 0.2.4 Display Verification
- ✅ Verify Weston desktop appears on LCD
- ✅ Test touch input - tap and drag should work
- ✅ Note any display issues (flickering, incorrect orientation) — *None observed*

### 0.2.5 Network Setup - Ethernet
- ✅ Connect Ethernet cable
- ✅ Verify IP address assigned: — *end0 interface, 192.168.4.102*
```bash
ip addr show eth0
```
- ✅ Test connectivity:
```bash
ping -c 3 8.8.8.8  # External (if internet available)
ping -c 3 <your-host-ip>  # Host machine
```
- ✅ SSH from host: — *dropbear SSH server*
```bash
ssh root@<board-ip>
```

### 0.2.6 Network Setup - WiFi
- ✅ Scan for networks:
```bash
iw dev wlan0 scan | grep SSID
```
- ✅ Connect to WiFi: — *Manual wpa_supplicant config required each boot*
```bash
# Using wpa_supplicant
wpa_passphrase "YourSSID" "YourPassword" >> /etc/wpa_supplicant/wpa_supplicant-wlan0.conf

# Or create file manually
cat > /etc/wpa_supplicant/wpa_supplicant-wlan0.conf << EOF
ctrl_interface=/var/run/wpa_supplicant
ctrl_interface_group=0
update_config=1

network={
    ssid="YourSSID"
    psk="YourPassword"
    key_mgmt=WPA-PSK
}
EOF

# Start wpa_supplicant
systemctl restart wpa_supplicant@wlan0
```
- ✅ Verify WiFi IP address:
```bash
ip addr show wlan0
```
- ✅ Test SSH over WiFi

---

## 0.3 Development Workflow Verification

### 0.3.1 ST-Link Debugging Setup
- ✅ Verify ST-Link is detected: — *VCP working for serial console*
```bash
lsusb | grep ST-Link
# Should show: STMicroelectronics ST-LINK/V2.1
```
- ✅ Test connection with STM32CubeProgrammer:
```bash
STM32_Programmer_CLI -c port=SWD
```

### 0.3.2 M4 Firmware Test
- ✅ Create simple M4 test project in STM32CubeIDE: — *Imported OpenAMP_TTY_echo from STM32CubeMP1 v1.7.0*
  - ✅ File > Import > Existing Projects > STM32MP157C-DK2 example
  - ✅ Selected OpenAMP_TTY_echo_CM4 project
- ✅ Build project
- ✅ Copy firmware to board:
```bash
scp Debug/*.elf stm32mp1:/lib/firmware/
```
- ✅ Load and run on M4: — *OpenAMP_TTY_echo tested successfully, echo works via /dev/ttyRPMSG0*
```bash
# On the board
echo "your_firmware.elf" > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state

# Check status
cat /sys/class/remoteproc/remoteproc0/state

# View M4 debug output (if using UART)
cat /dev/ttyRPMSG0
```
- ✅ Stop M4:
```bash
echo stop > /sys/class/remoteproc/remoteproc0/state
```

### 0.3.3 Remote Development Setup
- ✅ Configure VS Code Remote SSH to board — *Host "stm32mp1" configured*
- ⏸️ Test editing files directly on board — *VS Code Remote SSH doesn't support ARM32; use SSH + nano/vi or SFTP instead*
- ✅ Set up SSH key authentication: — *~/.ssh/stm32mp1 key installed*
```bash
# On host
ssh-keygen -t ed25519 -f ~/.ssh/stm32mp1
ssh-copy-id -i ~/.ssh/stm32mp1.pub root@<board-ip>

# Add to ~/.ssh/config
cat >> ~/.ssh/config << EOF
Host stm32mp1
    HostName <board-ip>
    User root
    IdentityFile ~/.ssh/stm32mp1
EOF

# Test
ssh stm32mp1
```

---

## 0.4 System Exploration

### 0.4.1 Understand the OpenSTLinux Environment
- ✅ Explore filesystem structure: — *rootfs 6.4G, bootfs 55M, vendorfs 14M*
```bash
df -h                    # Partition layout
ls /                     # Root contents
cat /etc/os-release      # Distribution info
uname -a                 # Kernel version
```
- ✅ Check running services: — *25 services (weston, avahi, bluetooth, dropbear, iiod, netdata, etc.)*
```bash
systemctl list-units --type=service --state=running
```
- ⏸️ Review device tree: — *deferred, will explore as needed*
```bash
ls /proc/device-tree/
dtc -I fs /proc/device-tree | less
```

### 0.4.2 Hardware Peripheral Survey
- ✅ Identify available interfaces: — *3x I2C, 10x GPIO chips, 2x ADC, 1x UART*
```bash
# I2C buses
ls /dev/i2c-*
i2cdetect -l

# SPI buses  
ls /dev/spidev*

# UARTs
ls /dev/ttySTM*

# GPIO
ls /sys/class/gpio/

# ADC
ls /sys/bus/iio/devices/
```
- ⏸️ Document which interfaces map to which headers (Arduino, RPi) — *deferred to Phase 1*
- ✅ Test I2C bus scan: — *Devices found at 0x28, 0x33, 0x38, 0x39, 0x4a, 0x60*
```bash
i2cdetect -y 1  # Try different bus numbers
```

### 0.4.3 Performance Baseline
- ✅ Measure boot time: — *58s total (17s kernel + 41s userspace)*
```bash
systemd-analyze
systemd-analyze blame
```
- ✅ Check memory usage: — *386MB total, 120MB used, 265MB available*
```bash
free -h
cat /proc/meminfo
```
- ✅ Check CPU info: — *Dual Cortex-A7, ARMv7l, NEON/VFPv4*
```bash
cat /proc/cpuinfo
lscpu
```

---

## 0.5 Project Repository Setup

### 0.5.1 Create Git Repository
```bash
mkdir -p ~/projects/smarthub
cd ~/projects/smarthub
git init

# Create initial structure
mkdir -p docs buildroot app m4-firmware web-ui tools tests
mkdir -p app/src app/include
mkdir -p m4-firmware/src m4-firmware/include

# Create .gitignore
cat > .gitignore << 'EOF'
# Build directories
build/
out/
*.o
*.elf
*.bin
*.hex
*.map

# IDE
.vscode/
.idea/
*.swp
*~

# Buildroot
buildroot/dl/
buildroot/output/

# Secrets
*.pem
*.key
secrets/

# OS
.DS_Store
Thumbs.db
EOF

git add .
git commit -m "Initial project structure"
```

### 0.5.2 Documentation Setup
- ✅ Copy planning documents to docs/
- ✅ Create README.md with project overview
- ✅ Set up documentation structure

---

## 0.6 Validation Checklist

**PHASE 0 COMPLETED: 2025-12-26**

See `PHASE_0_COMPLETED.md` for detailed completion notes.

| Item | Status | Notes |
|------|--------|-------|
| Host toolchain installed | ✅ | STM32CubeIDE on Windows (includes ARM toolchain) |
| Board boots OpenSTLinux | ✅ | Custom SD image with GPT partition table |
| Serial console works | ✅ | 115200 baud, COM8 / /dev/ttyS7 |
| Display shows Weston | ✅ | 480x800 LCD working |
| Touch input works | ✅ | |
| Ethernet works | ✅ | end0 interface, DHCP, SSH accessible |
| WiFi works | ✅ | wlan0 interface, manual config required |
| ST-Link detected | ✅ | VCP working for serial console |
| M4 firmware test passes | ✅ | remoteproc0 available, ready for firmware |
| Git repository created | ✅ | github.com/johnjezl/STM32-Smart-Home-Hub |

---

## Troubleshooting

### Board doesn't boot
- Check boot switch positions (SW1)
- Verify SD card is properly seated
- Try re-flashing SD card
- Check serial console for error messages

### No display output
- Verify LCD ribbon cable is properly connected
- Check for correct display device tree overlay
- Verify boot completes (check serial console)

### WiFi not working
- Ensure antenna is connected
- Check kernel has brcmfmac driver loaded: `lsmod | grep brcmfmac`
- Verify firmware files exist: `ls /lib/firmware/brcm/`
- Check dmesg for WiFi errors: `dmesg | grep brcm`

### ST-Link not detected
- Check USB cable (must be data cable, not charge-only)
- Install udev rules
- Try unplugging/replugging
- Check with `lsusb`

### M4 firmware won't load
- Check firmware path is correct
- Verify ELF is built for Cortex-M4
- Check `/sys/class/remoteproc/remoteproc0/` exists
- Review dmesg: `dmesg | grep remoteproc`

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 0.1 Host Machine Setup | 2-3 hours |
| 0.2 Hardware Setup | 2-3 hours |
| 0.3 Development Workflow | 2-3 hours |
| 0.4 System Exploration | 1-2 hours |
| 0.5 Repository Setup | 30 minutes |
| 0.6 Validation | 1 hour |
| **Total** | **8-12 hours** |

---

## Notes

- Keep the serial console connected during all testing - it's your primary debugging tool
- Document any deviations from expected behavior
- Take screenshots of working display for reference
- Save working configurations before making changes
