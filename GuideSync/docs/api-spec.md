# GuideSync — API Specification

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
List all GuideSync devices (hub, glasses, cane, band, beacons).

**Response:** Array of Device objects:
```json
[
  {
    "device_id": "hub-001",
    "device_type": "hub",
    "name": "Vision Hub",
    "firmware_version": "1.0.0",
    "online": true
  }
]
```

### POST /devices/{device_id}/ota
Trigger OTA firmware update for a device.

**Parameters:** `version` (query, e.g. "1.1.0")

## Smart Glasses

### GET /glasses
Latest glasses telemetry (battery, obstacle info, crosswalk, ToF, IMU).

### GET /glasses/scene
Latest scene descriptions (objects, distances, directions, text).

## Smart Cane

### GET /cane
Latest cane telemetry (ultrasonic, ToF downward, dropoff, stairs, IMU).

## Haptic Band

### GET /band
Latest band telemetry (battery, steps, falls, nav direction, SOS status).

## Nav Beacons

### GET /beacons
List all registered nav beacons with landmark names and positions.

### POST /beacons
Register a new beacon.

**Body:**
```json
{
  "beacon_id": 4,
  "uuid_short": 4,
  "landmark_name": "Living Room",
  "x": 7.5,
  "y": 4.0,
  "floor": 1,
  "battery_v": 300
}
```

### PUT /beacons/{beacon_id}
Update beacon landmark name or position.

## Navigation

### GET /navigation/route
Get current navigation route (if active).

### POST /navigation/destination
Set navigation destination and compute route.

**Body:**
```json
{ "destination": "Kitchen" }
```

### GET /navigation/status
Current navigation status (active, destination, step, ETA).

### POST /navigation/stop
Stop active navigation.

## OCR

### POST /ocr/request
Request text recognition from an image.

**Body:**
```json
{ "image_base64": "..." }
```

**Response:**
```json
{
  "text": "EXIT →",
  "confidence": 0.91
}
```

## Alerts

### GET /alerts
List recent alerts (fall, SOS, obstacle, crosswalk, battery, anomaly).

### PUT /alerts/{alert_id}/ack
Acknowledge an alert.

## Emergency

### POST /sos/cancel
Cancel an active SOS (false alarm).

### GET /emergency/contacts
List emergency contacts.

### POST /emergency/contacts
Add an emergency contact.

## Faces (Privacy-Controlled)

### GET /faces
List familiar faces (encrypted). Face recognition is opt-in.

### POST /faces
Add a familiar face (name + encrypted embedding).

## Location

### GET /location
Current GPS + indoor position (NavNet).

## ML Endpoints

### GET /ml/scene/history
Scene detection history (filterable by hours).

### GET /ml/nav/position
Latest NavNet indoor position estimate.

### GET /ml/fall/history
Fall event history (filterable by days).

## WebSocket

### WS /ws
Real-time WebSocket for telemetry, alerts, scene descriptions, and
navigation updates.

**Client→Server messages:**
```json
{ "type": "set_destination", "destination": "Kitchen" }
{ "type": "stop_navigation" }
{ "type": "cancel_sos" }
```

**Server→Client messages:**
```json
{ "type": "heartbeat", "ts": "2024-..." }
{ "type": "nav_started", "destination": "Kitchen", "steps": 4 }
{ "type": "nav_stopped" }
{ "type": "sos_cancelled" }
{ "type": "ocr_result", "text": "EXIT" }
{ "type": "alert", "alert_type": "fall", "severity": "emergency" }
```