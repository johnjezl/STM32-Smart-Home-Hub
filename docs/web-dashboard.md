# SmartHub Web Dashboard

The SmartHub Web Dashboard provides a browser-based interface for monitoring and controlling smart home devices. It complements the native LVGL touchscreen UI with remote access capabilities.

## Features

- **Real-time Device Control**: Toggle lights, switches, and adjust brightness
- **Sensor Monitoring**: View temperature, humidity, motion, and contact sensor readings
- **Room Organization**: Devices grouped by room with quick overview cards
- **Responsive Design**: Works on desktop, tablet, and mobile browsers
- **Dark Theme**: Consistent with the native UI aesthetic
- **Auto-refresh**: Dashboard updates every 5 seconds

## Accessing the Dashboard

The web dashboard is served over HTTPS:

```
https://<device-ip>/
```

For the STM32MP157F-DK2 development board:
```
https://192.168.4.102/
```

> **Note**: The dashboard uses a self-signed TLS certificate. You may need to accept a browser security warning on first access.

## Dashboard Tabs

### Dashboard Tab

The main overview showing:

| Component | Description |
|-----------|-------------|
| Stats Cards | Total devices, active devices, sensors, rooms |
| Room Cards | Per-room summary with device counts and temperature |
| Quick Status | Visual indicators for device states |

### Devices Tab

Full device list with controls:

- **Toggle Switch**: Turn devices on/off
- **Brightness Slider**: Adjust dimmer/light brightness (0-100%)
- **Room Filter**: Filter devices by room
- **Status Icons**: Visual feedback for device state

### Sensors Tab

Sensor readings display:

| Sensor Type | Display Format | Color |
|-------------|----------------|-------|
| Temperature | 72.5°F | Orange |
| Humidity | 65% | Blue |
| Motion | Motion/Clear | Green |
| Contact | Open/Closed | Default |

## REST API

The dashboard communicates via REST API:

### Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/devices` | List all devices with state |
| GET | `/api/devices/:id` | Get single device details |
| PUT | `/api/devices/:id` | Update device state |
| GET | `/api/system/status` | System status info |

### Device Response Format

```json
{
  "id": "light-living",
  "name": "Living Room Light",
  "type": "dimmer",
  "room": "Living Room",
  "online": true,
  "state": {
    "on": true,
    "brightness": 80
  }
}
```

### Updating Device State

```bash
curl -k -X PUT https://192.168.4.102/api/devices/light-living \
  -H "Content-Type: application/json" \
  -d '{"on": false, "brightness": 50}'
```

### Device Types

| Type | State Properties | Controls |
|------|------------------|----------|
| `switch` | on | Toggle |
| `dimmer` | on, brightness | Toggle, Slider |
| `color_light` | on, brightness, color_temp | Toggle, Slider |
| `temperature_sensor` | value | Read-only |
| `humidity_sensor` | value | Read-only |
| `motion_sensor` | value | Read-only |
| `contact_sensor` | value | Read-only |

## File Structure

```
web/
├── index.html      # Main dashboard HTML
├── styles.css      # CSS styles (dark theme)
├── app.js          # JavaScript application logic
└── tests/
    ├── test_app.js       # Frontend unit tests
    └── test_runner.html  # Browser test runner
```

## Configuration

The web server is configured in `/etc/smarthub/config.yaml`:

```yaml
web:
  port: 443
  root: /opt/smarthub/www
  tls:
    cert: /etc/smarthub/certs/server.crt
    key: /etc/smarthub/certs/server.key
```

### Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `port` | HTTPS port | 443 |
| `root` | Web files directory | /opt/smarthub/www |
| `tls.cert` | TLS certificate path | Required |
| `tls.key` | TLS private key path | Required |

## Deployment

### Manual Deployment

1. Copy web files to device:
```bash
scp -O web/* root@192.168.4.102:/opt/smarthub/www/
```

2. Restart SmartHub:
```bash
ssh root@192.168.4.102 "killall smarthub; /root/smarthub &"
```

### Buildroot Integration

Add to your Buildroot package:

```makefile
define SMARTHUB_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0644 $(@D)/web/index.html $(TARGET_DIR)/opt/smarthub/www/index.html
    $(INSTALL) -D -m 0644 $(@D)/web/styles.css $(TARGET_DIR)/opt/smarthub/www/styles.css
    $(INSTALL) -D -m 0644 $(@D)/web/app.js $(TARGET_DIR)/opt/smarthub/www/app.js
endef
```

## Testing

### Backend API Tests

Run C++ tests:
```bash
cd app/build
ctest --output-on-failure -R web_api
```

### Frontend Tests

1. **Browser Testing**: Open `web/tests/test_runner.html` in a browser
2. **Node.js Testing**:
```bash
cd web/tests
node test_app.js
```

### Test Coverage

| Category | Tests |
|----------|-------|
| Device Classification | 3 |
| Room Grouping | 2 |
| Device State | 5 |
| Icon Mapping | 2 |
| Sensor Formatting | 5 |
| Statistics | 4 |
| Device Filtering | 2 |
| API Integration | 4 |
| Tab Navigation | 2 |
| Brightness Control | 3 |

## Browser Compatibility

| Browser | Version | Status |
|---------|---------|--------|
| Chrome | 90+ | ✅ Supported |
| Firefox | 88+ | ✅ Supported |
| Safari | 14+ | ✅ Supported |
| Edge | 90+ | ✅ Supported |
| Mobile Chrome | 90+ | ✅ Supported |
| Mobile Safari | 14+ | ✅ Supported |

## Security Considerations

1. **TLS Encryption**: All traffic encrypted via HTTPS
2. **Authentication**: Session-based auth (disabled in demo mode)
3. **CORS**: Same-origin policy enforced
4. **Headers**: Security headers added (X-Frame-Options, CSP, etc.)

### Enabling Authentication

To require login (production mode):

1. Configure SessionManager in Application.cpp
2. Remove `/api/devices` from public routes in WebServer.cpp
3. Update app.js to use login flow

## Troubleshooting

### Dashboard Shows Empty

1. Check devices are in database:
```bash
sqlite3 /var/lib/smarthub/smarthub.db "SELECT COUNT(*) FROM devices;"
```

2. Check SmartHub logs:
```bash
tail -f /tmp/smarthub.log
```

3. Test API directly:
```bash
curl -k https://192.168.4.102/api/devices
```

### Connection Refused

1. Verify SmartHub is running:
```bash
ps aux | grep smarthub
```

2. Check port is listening:
```bash
netstat -tlnp | grep 443
```

### Certificate Warnings

Expected with self-signed certificates. For production:
1. Use Let's Encrypt or other CA
2. Or add certificate exception in browser

## Architecture

```
┌─────────────────┐     HTTPS      ┌──────────────────┐
│  Web Browser    │◄──────────────►│   Web Server     │
│  (Dashboard)    │                │   (Mongoose)     │
└─────────────────┘                └────────┬─────────┘
                                            │
                                            ▼
                                   ┌──────────────────┐
                                   │  Device Manager  │
                                   │                  │
                                   └────────┬─────────┘
                                            │
                              ┌─────────────┼─────────────┐
                              ▼             ▼             ▼
                        ┌─────────┐   ┌─────────┐   ┌─────────┐
                        │ Lights  │   │ Sensors │   │ Switches│
                        └─────────┘   └─────────┘   └─────────┘
```

## Future Enhancements

- [ ] WebSocket support for real-time updates
- [ ] Device grouping and scenes
- [ ] Historical sensor data charts
- [ ] User management interface
- [ ] Push notifications
- [ ] PWA support for mobile installation
