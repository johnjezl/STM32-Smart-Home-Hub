# SmartHub Networking

This document describes the network configuration for SmartHub.

---

## Overview

SmartHub supports both wired (Ethernet) and wireless (WiFi) networking:

| Interface | Hardware | Driver | Speed |
|-----------|----------|--------|-------|
| eth0 | RTL8211F PHY | stmmac | 1 Gbps |
| wlan0 | BCM43430 (Murata 1DX) | brcmfmac | 72 Mbps (802.11n) |

Both interfaces obtain IP addresses via DHCP and can operate simultaneously.

---

## Network Initialization

### Boot Sequence

The `S40network` init script handles network setup:

1. Bring up loopback (`lo`)
2. Bring up Ethernet (`eth0`) + DHCP
3. Start wpa_supplicant for WiFi
4. Bring up WiFi (`wlan0`) + DHCP

### Init Script Location

```
/etc/init.d/S40network
```

Source in repository:
```
buildroot/board/smarthub/overlay/etc/init.d/S40network
```

---

## Ethernet Configuration

### Automatic Setup

Ethernet starts automatically on boot:

```bash
# Bring up interface
ip link set eth0 up

# Get IP via DHCP
udhcpc -i eth0 -b -q      # BusyBox DHCP (fast)
dhcpcd -b eth0            # Full-featured DHCP
```

### Manual Configuration

```bash
# Static IP (temporary)
ip addr add 192.168.1.100/24 dev eth0
ip route add default via 192.168.1.1

# Verify
ip addr show eth0
```

### Hardware Details

| Property | Value |
|----------|-------|
| PHY | Realtek RTL8211F |
| Speed | 10/100/1000 Mbps |
| Interface | RGMII |
| MAC | Assigned from OTP |

---

## WiFi Configuration

### Overview

WiFi uses the Broadcom BCM43430 chip (Murata 1DX module):

| Property | Value |
|----------|-------|
| Chip | BCM43430 |
| Module | Murata 1DX (Type 1DX) |
| Interface | SDIO |
| Bands | 2.4 GHz only |
| Protocols | 802.11 b/g/n |
| Security | WPA/WPA2/WPA3 |

### Firmware Requirements

The brcmfmac driver requires firmware and NVRAM configuration:

| File | Purpose |
|------|---------|
| `brcmfmac43430-sdio.bin` | Main firmware |
| `brcmfmac43430-sdio.clm_blob` | Country/regulatory data |
| `brcmfmac43430-sdio.st,stm32mp157f-dk2.txt` | Board-specific NVRAM |

**Important:** The NVRAM file must exist for WiFi to work. SmartHub creates a symlink to the Murata 1DX configuration:

```bash
# In post_build.sh:
ln -sf brcmfmac43430-sdio.MUR1DX.txt \
    brcmfmac43430-sdio.st,stm32mp157f-dk2.txt
```

### wpa_supplicant Configuration

WiFi credentials are stored in:
```
/etc/wpa_supplicant/wpa_supplicant.conf
```

Format:
```
ctrl_interface=/var/run/wpa_supplicant
ctrl_interface_group=0
update_config=1
country=US

network={
    ssid="NetworkName"
    psk="Password"
    key_mgmt=WPA-PSK
}
```

### Manual WiFi Setup

```bash
# Start wpa_supplicant
wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf

# Scan for networks
wpa_cli -i wlan0 scan
wpa_cli -i wlan0 scan_results

# Add network interactively
wpa_cli -i wlan0
> add_network
0
> set_network 0 ssid "MyNetwork"
> set_network 0 psk "MyPassword"
> enable_network 0
> save_config
> quit

# Get IP
udhcpc -i wlan0
```

### Troubleshooting WiFi

**No wlan0 interface:**
```bash
# Check driver loaded
lsmod | grep brcmfmac

# Check firmware
ls /lib/firmware/brcm/brcmfmac43430*

# Check dmesg for errors
dmesg | grep -i "brcm\|wlan\|firmware"
```

**Driver loads but no scan results:**
- Verify NVRAM symlink exists
- Check antenna is connected
- Try power cycle

**Association fails:**
- Verify SSID/password correct
- Check `wpa_cli status` for errors
- Try WPA2 instead of WPA3

---

## mDNS / DNS-SD

### Overview

SmartHub uses Avahi for multicast DNS:

- **mDNS**: Resolves `smarthub.local` to IP address
- **DNS-SD**: Advertises services (MQTT, SSH)

### How It Works

```
┌─────────────────────────────────────────────────────────────────┐
│                     Local Network                                │
│                                                                  │
│  ┌──────────────┐           ┌──────────────┐                    │
│  │   Laptop     │           │   SmartHub   │                    │
│  │              │           │              │                    │
│  │ "Who is      │──mDNS───►│ "I am        │                    │
│  │  smarthub    │  query    │  smarthub    │                    │
│  │  .local?"    │           │  .local at   │                    │
│  │              │◄──────────│  192.168.4.99"                    │
│  └──────────────┘  response └──────────────┘                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Avahi Configuration

Avahi runs automatically via init:
```
/etc/init.d/S50avahi-daemon
```

Service advertisement config:
```
/etc/avahi/services/*.service
```

### Discovering SmartHub

From any machine on the network:

```bash
# Ping by name
ping smarthub.local

# Discover services (Linux)
avahi-browse -a

# Discover services (macOS)
dns-sd -B _mqtt._tcp

# SSH by name
ssh root@smarthub.local
```

### Windows mDNS Support

Windows 10+ supports mDNS natively. For older Windows, install Bonjour Print Services.

---

## MQTT Networking

### Mosquitto Configuration

MQTT broker listens on:

| Port | Protocol | Access |
|------|----------|--------|
| 1883 | TCP | Local network |
| 9001 | WebSocket | Future (disabled) |

Config file: `/etc/mosquitto/mosquitto.conf`

### Firewall (if enabled)

```bash
# Allow MQTT
iptables -A INPUT -p tcp --dport 1883 -j ACCEPT

# Allow mDNS
iptables -A INPUT -p udp --dport 5353 -j ACCEPT
```

### Testing MQTT

```bash
# From SmartHub
mosquitto_pub -h localhost -t test -m "hello"
mosquitto_sub -h localhost -t test

# From remote machine
mosquitto_pub -h smarthub.local -t test -m "hello"
```

---

## Multiple Interface Handling

### Routing

When both Ethernet and WiFi are connected:

```bash
# View routing table
ip route

# Default route uses first interface that gets DHCP
# Typically eth0 (faster link)
```

### Binding to Specific Interface

Services can bind to specific interfaces:

```bash
# MQTT on all interfaces (default)
# listener 1883

# MQTT only on localhost
# listener 1883 127.0.0.1
```

---

## Network Debugging

### Useful Commands

```bash
# Interface status
ip -br link
ip -br addr

# Routing
ip route

# DNS resolution
nslookup google.com

# Connection test
ping -c 3 8.8.8.8

# WiFi status
wpa_cli -i wlan0 status
iw dev wlan0 link

# Open ports
netstat -tlnp
```

### Log Locations

```bash
# System log
cat /var/log/messages

# Kernel messages
dmesg | grep -i "eth\|wlan\|brcm\|net"

# DHCP
cat /var/log/messages | grep dhcp
```

---

## Security Considerations

### Current State (Development)

| Service | Binding | Auth |
|---------|---------|------|
| SSH | 0.0.0.0:22 | Key only |
| MQTT | 0.0.0.0:1883 | Anonymous |
| mDNS | 0.0.0.0:5353 | None |

### Production Recommendations

1. **SSH**: Disable password auth (done), use key only
2. **MQTT**: Enable TLS, require password
3. **Firewall**: Block unused ports
4. **WiFi**: Use WPA3 if available

---

## References

- [brcmfmac Driver](https://wireless.wiki.kernel.org/en/users/drivers/brcm80211)
- [wpa_supplicant Documentation](https://w1.fi/wpa_supplicant/)
- [Avahi Documentation](https://avahi.org/)
- [Mosquitto Documentation](https://mosquitto.org/documentation/)

---

*Created: 28 December 2025*
