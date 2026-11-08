# SmartHub REST API

The SmartHub web server provides a RESTful API for device management, room organization, automations, and system monitoring.

## Base URL

```
https://<smarthub-ip>/api
```

The hub serves HTTPS on port 443 by default (`web.port` in the configuration). With no certificate configured it creates a self-signed one at first boot, so tools need to trust it, or skip the check with `curl -k` on your own network.

## Authentication

Authentication is required on every network, including the local one. Requests without valid credentials get `401`.

The API supports two authentication methods:

### Session-based Authentication
Use the login endpoint to create a session, then include the session cookie in subsequent requests. A session ends after 60 minutes without requests, on logout, or when the hub restarts. An open dashboard keeps making requests, so it stays logged in.

### API Token Authentication
Include a Bearer token in the Authorization header:
```
Authorization: Bearer <api-token>
```

There's no endpoint for creating tokens yet ([#36](https://github.com/johnjezl/STM32-Smart-Home-Hub/issues/36)).

### Roles
A `user` can read everything except pending Zigbee devices, and can control devices with `PUT /api/devices/{id}`. All other requests that change something, and the Zigbee pairing routes, need an `admin` and otherwise get `403`.

### Public Routes
The following routes are accessible without authentication:
- `POST /api/auth/login`
- `POST /api/auth/logout`
- `GET /api/system/status`
- `GET /api/setup/status`
- `POST /api/setup`, until the first account exists

---

## Authentication Endpoints

### POST /api/auth/login

Authenticate and create a session.

**Request Body:**
```json
{
  "username": "admin",
  "password": "Your-Password-1"
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

The request must have the `Content-Type: application/json` header, or it gets `415`.

**Response (400 Bad Request):** the username or password is missing.

**Response (403 Forbidden):** the request was addressed to a public DNS name. Use the hub's IP address or local name.

**Response (401 Unauthorized):** the same answer for a wrong password, an unknown username and a disabled account.
```json
{
  "error": "Invalid username or password"
}
```

**Response (429 Too Many Requests):** too many failed logins from this IP address. The `Retry-After` header gives the wait in seconds.

**Response (503 Service Unavailable):** a login from this IP address is already being checked, or several are from others. Passwords take over a second to check on the board. `Retry-After` says when to try again.

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
  "role": "admin",
  "isApiToken": false
}
```

---

### GET /api/setup/status

Report whether the hub still needs its first account. No authentication required.

**Response (200 OK):**
```json
{
  "setupRequired": true
}
```

---

### POST /api/setup

Create the administrator on a new hub and log in. No authentication required, and refused once any account exists.

**Request Body:**
```json
{
  "username": "admin",
  "password": "Your-Password-1"
}
```

Usernames are 3 to 32 characters, start with a letter, and contain only letters, digits, `_` and `-`. Passwords are 8 to 128 characters, with an uppercase letter, a lowercase letter and a digit.

The request must have the `Content-Type: application/json` header, or it gets `415`.

**Response (201 Created):** same body as login, and sets the `session` cookie.

**Response (400 Bad Request):** the `error` field says what's wrong with the username or password.

**Response (403 Forbidden):** the request was addressed to a public DNS name. Use the hub's IP address or local name.

**Response (409 Conflict):** an account already exists.

---

## Account Endpoints

Accounts are only for administrators, even to list. Each user manages their own password and API tokens. See [Security](security.md#managing-accounts) for what each change does to sessions and tokens.

Every request with a body must have `Content-Type: application/json`.

### POST /api/auth/password

Change your own password. Not with an API token: sign in.

```json
{ "currentPassword": "Old-Password-1", "newPassword": "New-Password-1" }
```

Your other sessions end and your API tokens are revoked. This session carries on.

**Response (200 OK):** `{"success": true, "revokedTokens": 1}`

**Response (400):** a field is missing, or the new password breaks the [rules](#post-apisetup). **403:** the current password is wrong, which counts as a failed login, or this was sent with an API token. Not 401, which means you aren't signed in. **429:** too many failed logins from this IP.

### GET /api/users

All accounts, without their passwords. **Admin only.**

```json
[{ "id": 1, "username": "owner", "role": "admin", "enabled": true, "createdAt": 1762470000, "lastLogin": 1762480000 }]
```

### POST /api/users

Add an account. **Admin only.** `role` is `admin` or `user`, and defaults to `user`.

```json
{ "username": "guest", "password": "Guest-Password-1", "role": "user" }
```

**Response (201 Created):** the account, as in the list. **400:** a bad username, password or role. **409:** the username is taken.

### PUT /api/users/{username}

Change a role, or disable or enable an account. **Admin only.** Give either or both.

```json
{ "role": "admin", "enabled": true }
```

**Response (200 OK):** the account. **400:** neither given, or a bad value. **404:** no such account. **409:** this would leave no enabled administrator.

### PUT /api/users/{username}/password

Set someone's password. **Admin only**, and not with an API token. Their sessions end, apart from yours if it's your own account, and their API tokens are revoked.

```json
{ "password": "New-Password-1" }
```

**Response (200 OK):** as for `POST /api/auth/password`. **400**, **404** as above. **403:** sent with an API token.

### DELETE /api/users/{username}

Remove an account, its sessions and its API tokens. **Admin only.**

**Response (200 OK)**. **404:** no such account. **409:** it's the last enabled administrator.

### GET /api/tokens

Your API tokens. The token itself is never shown again after it's made.

```json
[{ "id": 3, "name": "Home Assistant", "prefix": "a1b2c3d4...", "createdAt": 1762470000, "lastUsed": 0, "expiresAt": 1770246000, "enabled": true }]
```

### POST /api/tokens

Make an API token. Not with an API token: sign in. `expiryDays` is 0 (never, the default) to 3650.

```json
{ "name": "Home Assistant", "expiryDays": 90 }
```

**Response (201 Created):** `{"token": "...", "name": "Home Assistant", "prefix": "a1b2c3d4...", "expiryDays": 90}`, with `Cache-Control: no-store`. Copy the token now. **400:** no name, a name over 64 characters or with control characters, or a bad `expiryDays`. **403:** sent with an API token. **409:** you already have 20.

### DELETE /api/tokens/{id}

Revoke one of your API tokens. Not with an API token.

**Response (200 OK)**. **404:** you have no token with that id. **403:** sent with an API token.

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
  "room": "living_room",
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
  "room": "office",
  "config": {}
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| type | string | Yes | Device type (switch, dimmer, color_light, etc.) |
| name | string | Yes | Device display name |
| protocol | string | No | Protocol (local, zigbee, mqtt, http). Default: local |
| room | string | No | The room's id, or its name. Stored as the id, so renaming the room keeps the device |
| config | object | No | Protocol-specific configuration |

**Response (201 Created):**
```json
{
  "success": true,
  "id": "device-abc123"
}
```

**Response (400 Bad Request):** a field is missing, or `room` names no room. A name shared by several rooms is refused too: give the id.

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
| color_light | on, brightness, colorTemp (1000-10000 kelvin). On `local` devices also color_h (0-360), color_s and color_v (0-100), color_r, color_g and color_b (0-255) |

**Response (200 OK):**
```json
{
  "success": true
}
```

**Errors:** `502` if a property couldn't be set, for example when the device's radio doesn't accept the command, the device's protocol isn't loaded, or the value has the wrong type. State is only updated for properties that were set. `400` if the body isn't a JSON object, or a property is one this device can't be told to change or its value is out of range. Nothing is set in that case. Sensors report their state and can't be written.

---

### PUT /api/devices/{id}/settings

Update device settings (name, room assignment). `room` works as for `POST /api/devices`, and `""` takes the device out of its room. A refused request changes nothing.

**Parameters:**
- `id` (path) - Device identifier

**Request Body:**
```json
{
  "name": "New Device Name",
  "room": "bedroom"
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

Get a sensor's recorded readings, oldest first, averaged into at most 240 time slots.

Readings are recorded at most once a minute per sensor and kept for 7 days. The newest 5 minutes may not be included yet. Temperature is in Celsius, as sensors report it.

**Parameters:**
- `id` (path) - Device identifier
- `hours` (query) - Number of hours of history, 1 to 168 (default: 24)
- `property` (query) - `temperature` or `humidity` (default: the device's main reading)

**Response (200 OK):**
```json
{
  "deviceId": "sensor1",
  "property": "temperature",
  "data": [
    {"timestamp": 1705500000, "value": 22.5},
    {"timestamp": 1705503600, "value": 23.0}
  ]
}
```

`property` is empty and `data` has nothing for a device without a recorded reading, like a motion sensor.

**Errors:** 400 for an invalid `hours` or `property`, 404 for an unknown device.

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

### DELETE /api/zigbee/pending-devices

Let go of the device the wizard found, when the wizard closes. **Admin only.** If it wasn't added, the hub asks it to leave the network, as only the wizard adds devices. The dashboard calls this when its Add Device dialog closes after finding a device. If nothing calls it, the hub lets the device go 10 minutes after discovery stops.

**Response (200 OK):** `{"success": true}`

---

### POST /api/zigbee/devices

Confirm and add a discovered Zigbee device.

**Request Body:**
```json
{
  "name": "Kitchen Light",
  "room": "kitchen"
}
```

`room` works as for `POST /api/devices`.

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
    "property": "motion",
    "to_value": true
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
| device_state | deviceId, property, from_value (optional), to_value (optional) | Triggers when the property changes, matching the old and new values if given |
| time | hour, minute, days_of_week (optional) | Triggers at a specific time |
| time_interval | interval_minutes | Triggers every N minutes |
| sensor_threshold | deviceId, property, compare_op, threshold | Triggers on each change where the value meets the threshold |

`compare_op` is one of `eq`, `neq`, `gt`, `gte`, `lt` or `lte`. In a `time` trigger, an `hour` or `minute` of -1 means any. An automation with several triggers uses a `triggers` array instead of `trigger`.

Actions run shortly after the trigger, on their own thread, one automation's actions at a time. A new trigger restarts an automation that's waiting in a delay, so "light on, delay 60 s, light off" stays on until 60 seconds after the last trigger. A trigger that fires more often than the delay never reaches the actions after it. A condition is checked when the trigger fires, and if it doesn't hold, the trigger is ignored and a waiting run carries on. Disabling or deleting an automation drops its remaining actions.

An action's property has to be one a client can set on that device, such as `on` or `brightness`. An action can also be `{"type": "delay", "delayMs": 500}`. Delays can total at most 60 seconds.

An interval's countdown starts again when the hub restarts or the automation is changed or re-enabled, so an interval longer than the hub stays up won't fire.

`condition` is optional, and must hold for the actions to run:

```json
{"leaf": {"device_id": "light1", "property": "on", "op": "eq", "value": true}}
```

To combine conditions, use `{"is_group": true, "group_op": "and", "children": [ ... ]}`, where `group_op` is `and` or `or`.

**Response (201 Created):**
```json
{
  "success": true,
  "id": "auto_3f9a1c2d"
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
  "condition": { ... },
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

`name`, the trigger and `actions` are required. Leaving out `enabled`, `description` or `condition` keeps the current value.

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

By default, each IP address can make 600 requests a minute, and 10 failed logins in 5 minutes. Exceeding either limit returns a 429 status. See [Security](security.md#rate-limiting) for the configuration.

---

## Example Usage

### curl

```bash
# -k accepts the self-signed certificate the hub makes at first boot
# Log in and save the session cookie
curl -k -c cookies.txt -X POST \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"Your-Password-1"}' \
  https://smarthub.local/api/auth/login

# Get all devices
curl -k -b cookies.txt https://smarthub.local/api/devices

# Get system status
curl -k https://smarthub.local/api/system/status

# Create a room
curl -k -b cookies.txt -X POST \
  -H "Content-Type: application/json" \
  -d '{"name":"Living Room"}' \
  https://smarthub.local/api/rooms

# Turn on a light
curl -k -b cookies.txt -X PUT \
  -H "Content-Type: application/json" \
  -d '{"on":true,"brightness":100}' \
  https://smarthub.local/api/devices/light1

# Create an automation
curl -k -b cookies.txt -X POST \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Night Mode",
    "trigger": {"type": "time", "hour": 22, "minute": 0},
    "actions": [{"deviceId": "light1", "property": "on", "value": false}]
  }' \
  https://smarthub.local/api/automations
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

There's no WebSocket API yet: `/ws` isn't served. The dashboard polls the REST API every 5 seconds for changes.
