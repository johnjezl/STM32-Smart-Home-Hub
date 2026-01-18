# SmartHub REST API

The SmartHub web server provides a RESTful API for device management, room organization, automations, and system monitoring.

## Base URL

```
http://<smarthub-ip>:<port>/api
```

Default port is 8443 (configurable via `web.port` in config.yaml).

## Authentication

The API supports two authentication methods:

### Session-based Authentication
Use the login endpoint to create a session, then include the session cookie in subsequent requests.

### API Token Authentication
Include a Bearer token in the Authorization header:
```
Authorization: Bearer <api-token>
```

### Public Routes
The following routes are accessible without authentication:
- `POST /api/auth/login`
- `GET /api/system/status`

---

## Authentication Endpoints

### POST /api/auth/login

Authenticate and create a session.

**Request Body:**
```json
{
  "username": "admin",
  "password": "password123"
}
```

**Response (200 OK):**
```json
{
  "success": true,
  "username": "admin",
  "role": "admin"
}
```

Sets a `session` cookie for subsequent requests.

**Response (401 Unauthorized):**
```json
{
  "error": "Invalid credentials"
}
```

---

### POST /api/auth/logout

End the current session.

**Response (200 OK):**
```json
{
  "success": true
}
```

---

### GET /api/auth/me

Get current user information.

**Response (200 OK):**
```json
{
  "userId": 1,
  "username": "admin",
  "role": "admin"
}
```

---

## System Endpoints

### GET /api/system/status

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

## Room Endpoints

### GET /api/rooms

Returns a list of all rooms.

**Response:**
```json
[
  {
    "id": "living-room",
    "name": "Living Room",
    "deviceCount": 3,
    "activeDevices": 1
  },
  {
    "id": "kitchen",
    "name": "Kitchen",
    "deviceCount": 2,
    "activeDevices": 0
  }
]
```

| Field | Type | Description |
|-------|------|-------------|
| id | string | Unique room identifier |
| name | string | Room display name |
| deviceCount | integer | Number of devices in room |
| activeDevices | integer | Number of devices currently on |

---

### POST /api/rooms

Create a new room.

**Request Body:**
```json
{
  "name": "Bedroom"
}
```

**Response (200 OK):**
```json
{
  "success": true,
  "id": "bedroom"
}
```

**Response (400 Bad Request):**
```json
{
  "error": "Room name is required"
}
```

---

### PUT /api/rooms/{id}

Update a room's name.

**Parameters:**
- `id` (path) - Room identifier

**Request Body:**
```json
{
  "name": "Master Bedroom"
}
```

**Response (200 OK):**
```json
{
  "success": true
}
```

---

### DELETE /api/rooms/{id}

Delete a room. Devices in this room will be unassigned.

**Parameters:**
- `id` (path) - Room identifier

**Response (200 OK):**
```json
{
  "success": true
}
```

---

## Device Endpoints

### GET /api/devices

Returns a list of all registered devices.

**Response:**
```json
[
  {
    "id": "light1",
    "name": "Living Room Light",
    "type": "dimmer",
    "online": true
  },
  {
    "id": "sensor1",
    "name": "Temperature Sensor",
    "type": "temperature_sensor",
    "online": true
  }
]
```

| Field | Type | Description |
|-------|------|-------------|
| id | string | Unique device identifier |
| name | string | Human-readable device name |
| type | string | Device type |
| online | boolean | Device connectivity status |

---

### GET /api/devices/{id}

Returns details for a specific device.

**Parameters:**
- `id` (path) - Device identifier

**Response (200 OK):**
```json
{
  "id": "light1",
  "name": "Living Room Light",
  "type": "dimmer",
  "room": "Living Room",
  "protocol": "zigbee",
  "online": true,
  "state": {
    "on": true,
    "brightness": 75
  }
}
```

**Response (404 Not Found):**
```json
{
  "error": "Device not found"
}
```

---

### POST /api/devices

Create a new device.

**Request Body:**
```json
{
  "type": "dimmer",
  "name": "Office Light",
  "protocol": "local",
  "room": "Office",
  "config": {}
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| type | string | Yes | Device type (switch, dimmer, color_light, etc.) |
| name | string | Yes | Device display name |
| protocol | string | No | Protocol (local, zigbee, mqtt, http). Default: local |
| room | string | No | Room to assign device to |
| config | object | No | Protocol-specific configuration |

**Response (200 OK):**
```json
{
  "success": true,
  "id": "device-abc123"
}
```

---

### PUT /api/devices/{id}

Update device state.

**Parameters:**
- `id` (path) - Device identifier

**Request Body:**
```json
{
  "on": true,
  "brightness": 75
}
```

Properties vary by device type:

| Device Type | Properties |
|------------|------------|
| switch | on (boolean) |
| dimmer | on (boolean), brightness (0-100) |
| color_light | on, brightness, color_temp, hue, saturation |
| thermostat | target_temp, mode (heat/cool/auto/off) |
| lock | locked (boolean) |

**Response (200 OK):**
```json
{
  "success": true
}
```

---

### PUT /api/devices/{id}/settings

Update device settings (name, room assignment).

**Parameters:**
- `id` (path) - Device identifier

**Request Body:**
```json
{
  "name": "New Device Name",
  "room": "Bedroom"
}
```

**Response (200 OK):**
```json
{
  "success": true
}
```

---

### DELETE /api/devices/{id}

Delete a device.

**Parameters:**
- `id` (path) - Device identifier

**Response (200 OK):**
```json
{
  "success": true
}
```

---

### GET /api/devices/{id}/history

Get sensor history data.

**Parameters:**
- `id` (path) - Device identifier
- `hours` (query) - Number of hours of history (default: 24)

**Response (200 OK):**
```json
{
  "deviceId": "sensor1",
  "property": "value",
  "data": [
    {"timestamp": 1705500000, "value": 72.5},
    {"timestamp": 1705503600, "value": 73.0}
  ]
}
```

---

## Zigbee Pairing Endpoints

### POST /api/zigbee/permit-join

Enable Zigbee pairing mode.

**Request Body:**
```json
{
  "duration": 60
}
```

| Field | Type | Description |
|-------|------|-------------|
| duration | integer | Pairing window in seconds (default: 60) |

**Response (200 OK):**
```json
{
  "success": true,
  "duration": 60
}
```

---

### DELETE /api/zigbee/permit-join

Stop Zigbee pairing mode.

**Response (200 OK):**
```json
{
  "success": true
}
```

---

### GET /api/zigbee/pending-devices

Get devices discovered during pairing.

**Response (200 OK):**
```json
[
  {
    "id": "pending-device-1",
    "name": "Zigbee Light",
    "type": "dimmer",
    "ieeeAddress": "0x00158d0001234567",
    "manufacturer": "IKEA"
  }
]
```

---

### POST /api/zigbee/devices

Confirm and add a discovered Zigbee device.

**Request Body:**
```json
{
  "name": "Kitchen Light",
  "room": "Kitchen"
}
```

**Response (200 OK):**
```json
{
  "success": true,
  "id": "zigbee-00158d0001234567"
}
```

---

## Automation Endpoints

### GET /api/automations

Returns a list of all automations.

**Response:**
```json
[
  {
    "id": "auto-1",
    "name": "Turn on lights at sunset",
    "enabled": true,
    "trigger": {
      "type": "time",
      "hour": 18,
      "minute": 30
    },
    "actions": [
      {
        "deviceId": "light1",
        "property": "on",
        "value": true
      }
    ]
  }
]
```

---

### POST /api/automations

Create a new automation.

**Request Body:**
```json
{
  "name": "Motion Light",
  "description": "Turn on light when motion detected",
  "enabled": true,
  "trigger": {
    "type": "device_state",
    "deviceId": "motion-sensor-1",
    "property": "value",
    "value": true
  },
  "actions": [
    {
      "deviceId": "light1",
      "property": "on",
      "value": true
    }
  ]
}
```

**Trigger Types:**

| Type | Fields | Description |
|------|--------|-------------|
| device_state | deviceId, property, value | Triggers when device state changes |
| time | hour, minute | Triggers at specific time |
| interval | intervalMinutes | Triggers every N minutes |
| sensor_threshold | deviceId, operator, threshold | Triggers when sensor crosses threshold |

**Response (200 OK):**
```json
{
  "success": true,
  "id": "auto-abc123"
}
```

---

### GET /api/automations/{id}

Get details for a specific automation.

**Parameters:**
- `id` (path) - Automation identifier

**Response (200 OK):**
```json
{
  "id": "auto-1",
  "name": "Motion Light",
  "description": "Turn on light when motion detected",
  "enabled": true,
  "trigger": { ... },
  "conditions": [],
  "actions": [ ... ],
  "lastTriggered": 1705500000
}
```

---

### PUT /api/automations/{id}

Update an automation.

**Parameters:**
- `id` (path) - Automation identifier

**Request Body:**
```json
{
  "name": "Updated Name",
  "enabled": true,
  "trigger": { ... },
  "actions": [ ... ]
}
```

**Response (200 OK):**
```json
{
  "success": true
}
```

---

### DELETE /api/automations/{id}

Delete an automation.

**Parameters:**
- `id` (path) - Automation identifier

**Response (200 OK):**
```json
{
  "success": true
}
```

---

### PUT /api/automations/{id}/enable

Enable or disable an automation.

**Parameters:**
- `id` (path) - Automation identifier

**Request Body:**
```json
{
  "enabled": true
}
```

**Response (200 OK):**
```json
{
  "success": true
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
| 400 | Bad request (invalid JSON or missing fields) |
| 401 | Unauthorized (authentication required) |
| 404 | Resource not found |
| 405 | Method not allowed |
| 429 | Too many requests (rate limited) |
| 500 | Internal server error |

---

## Rate Limiting

The API implements rate limiting to prevent abuse. By default, 100 requests per minute per IP are allowed. Exceeding this limit returns a 429 status.

---

## Example Usage

### curl

```bash
# Get all devices
curl http://smarthub.local:8443/api/devices

# Get system status
curl http://smarthub.local:8443/api/system/status

# Create a room
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"name":"Living Room"}' \
  http://smarthub.local:8443/api/rooms

# Turn on a light
curl -X PUT \
  -H "Content-Type: application/json" \
  -d '{"on":true,"brightness":100}' \
  http://smarthub.local:8443/api/devices/light1

# Create an automation
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Night Mode",
    "trigger": {"type": "time", "hour": 22, "minute": 0},
    "actions": [{"deviceId": "light1", "property": "on", "value": false}]
  }' \
  http://smarthub.local:8443/api/automations
```

### JavaScript (Web Dashboard)

```javascript
// Get rooms
const rooms = await fetch('/api/rooms').then(r => r.json());

// Create a device
await fetch('/api/devices', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    type: 'dimmer',
    name: 'New Light',
    protocol: 'local'
  })
});

// Toggle automation
await fetch('/api/automations/auto-1/enable', {
  method: 'PUT',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ enabled: false })
});
```

---

## WebSocket API

Real-time updates are available via WebSocket at `/ws`:

```javascript
const ws = new WebSocket('ws://smarthub.local:8443/ws');

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  // msg.type: 'device.state', 'sensor.data', 'automation.triggered', etc.
};
```

Message types:
- `device.state` - Device state changed
- `device.added` - New device added
- `device.removed` - Device removed
- `sensor.data` - Sensor reading updated
- `automation.triggered` - Automation was executed
