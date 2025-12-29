# SmartHub REST API

The SmartHub web server provides a RESTful API for device management and system monitoring.

## Base URL

```
http://<smarthub-ip>:<port>/api
```

Default port is 8443 (configurable via `web.port` in config.yaml).

## Authentication

Currently, the API operates without authentication for local network access. Authentication will be added in Phase 7 (Security Hardening).

## Endpoints

### System Status

#### GET /api/system/status

Returns system information and overall status.

**Response:**
```json
{
  "version": "0.1.0",
  "uptime": 3600,
  "devices": 5
}
```

| Field | Type | Description |
|-------|------|-------------|
| version | string | SmartHub version |
| uptime | integer | Uptime in seconds |
| devices | integer | Number of registered devices |

---

### Devices

#### GET /api/devices

Returns a list of all registered devices.

**Response:**
```json
[
  {
    "id": "light1",
    "name": "Living Room Light",
    "type": "light",
    "online": true
  },
  {
    "id": "sensor1",
    "name": "Temperature Sensor",
    "type": "sensor",
    "online": true
  }
]
```

| Field | Type | Description |
|-------|------|-------------|
| id | string | Unique device identifier |
| name | string | Human-readable device name |
| type | string | Device type (light, sensor, thermostat, lock, switch, unknown) |
| online | boolean | Device connectivity status |

---

#### GET /api/devices/{id}

Returns details for a specific device.

**Parameters:**
- `id` (path) - Device identifier

**Response (200 OK):**
```json
{
  "id": "light1",
  "name": "Living Room Light",
  "type": "light",
  "room": "Living Room",
  "online": true
}
```

**Response (404 Not Found):**
```json
{
  "error": "Device not found"
}
```

---

#### PUT /api/devices/{id}

Update device state.

**Parameters:**
- `id` (path) - Device identifier

**Request Body:**
```json
{
  "power": "on",
  "brightness": 75
}
```

Properties vary by device type:

| Device Type | Properties |
|------------|------------|
| light | power (on/off), brightness (0-100), color |
| switch | power (on/off) |
| thermostat | target_temp, mode (heat/cool/auto/off) |
| lock | locked (true/false) |
| sensor | (read-only) |

**Response (200 OK):**
```json
{
  "success": true
}
```

**Response (404 Not Found):**
```json
{
  "error": "Device not found"
}
```

---

## Error Responses

All error responses follow this format:

```json
{
  "error": "Error message description"
}
```

| HTTP Status | Meaning |
|-------------|---------|
| 200 | Success |
| 400 | Bad request (invalid JSON) |
| 404 | Resource not found |
| 500 | Internal server error |

---

## Future Endpoints (Planned)

### Phase 3: Device Integration
- `POST /api/devices` - Register new device
- `DELETE /api/devices/{id}` - Remove device
- `GET /api/devices/{id}/history` - Sensor history data

### Phase 4: Zigbee
- `GET /api/zigbee/devices` - List Zigbee devices
- `POST /api/zigbee/permit` - Enable pairing mode

### Phase 5: WiFi Devices
- `GET /api/wifi/scan` - Discover devices
- `POST /api/wifi/devices` - Add WiFi device

### Phase 7: Security
- `POST /api/auth/login` - Authenticate
- `POST /api/auth/logout` - End session
- `GET /api/users` - List users (admin only)

---

## WebSocket API (Planned)

Real-time updates will be available via WebSocket at `/ws`:

```javascript
const ws = new WebSocket('ws://smarthub.local:8443/ws');

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  // msg.type: 'device.state', 'sensor.data', etc.
};
```

---

## Example Usage

### curl

```bash
# Get all devices
curl http://smarthub.local:8443/api/devices

# Get system status
curl http://smarthub.local:8443/api/system/status

# Turn on a light
curl -X PUT \
  -H "Content-Type: application/json" \
  -d '{"power":"on","brightness":100}' \
  http://smarthub.local:8443/api/devices/light1
```

### Python

```python
import requests

# Get devices
response = requests.get('http://smarthub.local:8443/api/devices')
devices = response.json()

# Set device state
requests.put(
    'http://smarthub.local:8443/api/devices/light1',
    json={'power': 'on', 'brightness': 75}
)
```

### JavaScript

```javascript
// Get system status
const response = await fetch('http://smarthub.local:8443/api/system/status');
const status = await response.json();

// Update device
await fetch('http://smarthub.local:8443/api/devices/light1', {
  method: 'PUT',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ power: 'on' })
});
```
