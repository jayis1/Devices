# FireSync — API Specification

Base URL: `http://localhost:8080/api/v1`

## Authentication

### POST /auth/login
JWT login (returns bearer token).

**Parameters:** `username`, `password` (query or form)

**Response:**
```json
{
  "access_token": "eyJ...",
  "token_type": "bearer"
}
```

## Devices

### GET /devices
List all FireSync devices (hub, sentinels, stove, panel, escape).

**Response:** Array of Device objects:
```json
[
  {
    "device_id": "hub-001",
    "device_type": "hub",
    "name": "FireSync Hub",
    "firmware_version": "1.0.0",
    "online": true
  }
]
```

### POST /devices/{device_id}/ota
Trigger OTA firmware update for a device.

**Parameters:** `version` (query, e.g. "1.1.0")

## Room Sentinels

### GET /sentinels
List all Room Sentinels with battery, room assignment, online status.

### GET /sentinels/{node_id}
Get sentinel detail + latest telemetry (smoke, CO, temp, thermal, FlameNet).

## Stove Guard

### GET /stove
Latest Stove Guard telemetry (thermal, knob positions, valve state, timer).

## Panel Monitor

### GET /panel
Latest Panel Monitor telemetry (current, voltage, power, temps, arc fault).

## Escape Controller

### GET /escape
Latest Escape Controller telemetry (LED zones, speaker, doors, route).

## Fire Events

### GET /fire/events
Fire event history (filterable by limit).

### GET /fire/active
Currently active fire event (if any).

**Response:**
```json
{
  "active": true,
  "room_id": 0,
  "safe_exit": "front",
  "avoid_rooms": [0, 1]
}
```

### POST /fire/{event_id}/ack
Acknowledge a fire event.

### POST /fire/{event_id}/false
Mark fire event as false alarm.

## Alarm Control

### POST /alarm/test
Trigger monthly test alarm.

### POST /alarm/silence
Silence current alarm.

## Occupants

### GET /occupants
Current room occupancy map (which rooms have people).

**Response:**
```json
{
  "Kitchen": "empty",
  "Living Room": "occupied",
  "Bedroom": "occupied"
}
```

## Escape Route

### GET /route
Current escape route (if active).

## Rooms

### GET /rooms
List room configuration (graph + exits).

### POST /rooms
Add or update a room.

**Body:**
```json
{
  "room_id": 4,
  "name": "Garage",
  "adjacent": [5],
  "has_exit": "garage",
  "sentinel_node": 5
}
```

## Risk Forecast

### GET /risk/forecast
7-day fire risk forecast with SHAP attribution.

**Response:**
```json
{
  "score": 28,
  "level": "low",
  "factors": [
    {"feature": "stove_max_temp_7d", "contribution": 12, "value": "195°C"}
  ],
  "recommendation": "Fire risk is LOW..."
}
```

### GET /risk/weekly
Weekly fire risk report.

## Alerts

### GET /alerts
List recent alerts (fire, CO, arc, thermal, battery, sensor).

### PUT /alerts/{alert_id}/ack
Acknowledge an alert.

## Emergency Dispatch

### POST /dispatch/cancel
Cancel 911 dispatch (false alarm).

### GET /dispatch/status
911 dispatch status.

## Suppression

### GET /suppression/status
Suppression system status (stove valve, panel breaker, HVAC, hood).

## ML Endpoints

### GET /ml/flamenet/history
FlameNet classification history.

### GET /ml/thermal/anomalies
Thermal anomaly detection history.

### GET /ml/arcdetect/history
Arc detection history.

## WebSocket

### WS /ws
Real-time WebSocket for telemetry, alerts, fire events, and dispatch updates.

**Client→Server messages:**
```json
{ "type": "silence_alarm" }
{ "type": "test_alarm" }
{ "type": "cancel_dispatch" }
```

**Server→Client messages:**
```json
{ "type": "heartbeat", "ts": "2026-..." }
{ "type": "fire_detected", "room": 0, "class": "flaming_fire" }
{ "type": "alarm_silenced" }
{ "type": "dispatch_cancelled" }
{ "type": "false_alarm", "event_id": 3 }
```