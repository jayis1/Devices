# CompostSync — API Specification

## Base URL

```
https://api.compostsync.local
```

## Authentication

JWT Bearer token via `Authorization: Bearer <token>`.

### POST /api/auth/register
Register a new user.
```json
Request: { "email": "user@example.com", "name": "Jane", "password": "secret" }
Response: { "access_token": "...", "token_type": "bearer", "expires_in": 86400 }
```

### POST /api/auth/login
```json
Request: { "email": "user@example.com", "password": "secret" }
Response: { "access_token": "...", "token_type": "bearer", "expires_in": 86400 }
```

## Devices

### POST /api/devices
Register a new device/hub.
```json
Request: { "id": "compost-hub-001", "name": "Backyard Bin", "bin_volume_liters": 200, "compost_type": "hot" }
Response: { "id": "compost-hub-001", "name": "Backyard Bin", "bin_volume_liters": 200, "compost_type": "hot", "last_seen": "2026-06-27T12:00:00Z" }
```

### GET /api/devices/{device_id}
Get device info.

### GET /api/devices
List all devices for the authenticated user.

### DELETE /api/devices/{device_id}
Remove a device.

## Telemetry

### POST /api/telemetry
Ingest a telemetry data point (used by MQTT handler or direct API).
```
Query: device_id=compost-hub-001&node_id=0002
Body: { "uptime": 3600, "batt": 87, "temps": [552, 582, 541], "moisture": [48, 55, 62], "co2": 3200, "ch4": 45, "mass": 12500, "vent": 0, "phase": "thermophilic", "alerts": 0 }
```

### GET /api/telemetry/{device_id}?hours=24
Get telemetry for the last N hours (max 720 = 30 days).
```json
Response: [
  { "timestamp": "2026-06-27T12:00:00Z", "node_id": "0002", "temp_c": [55.2, 58.2, 54.1], "moisture_pct": [48, 55, 62], "co2_ppm": 3200, "methane_ppm": 45, "mass_grams": 12500, "ph": null, "vent_position": 0, "phase": "thermophilic", "alerts": 0 }
]
```

### GET /api/telemetry/{device_id}/latest
Get the most recent reading.

## Compost

### GET /api/compost/{device_id}/status
Get current compost status with ML predictions.
```json
Response: {
  "device_id": "compost-hub-001",
  "phase": "thermophilic",
  "maturity_score": 35.0,
  "cn_ratio": 28.0,
  "days_to_ready": 42,
  "recommendation": "🔥 Thermophilic phase! Temp 58.2°C. Turn in 3-5 days.",
  "mass_kg": 12.5,
  "diverted_kg": 47.3
}
```

### GET /api/compost/{device_id}/timeline?days=30
Get time series for charts.

### GET /api/compost/{device_id}/recipes
Get composting recipe suggestions based on current conditions.
```json
Response: {
  "recipes": [
    { "name": "Add dry browns", "ingredients": ["Shredded cardboard", "Dry leaves"], "reason": "Moisture is 72%", "c_ratio": 60 }
  ]
}
```

## Alerts

### GET /api/alerts/{device_id}?hours=72
Get recent alerts.
```json
Response: [
  { "id": 1, "alert_type": "methane_high", "severity": 2, "message": "Methane 1200 ppm — TURN PILE NOW", "timestamp": "2026-06-27T10:00:00Z", "acknowledged": false }
]
```

### PUT /api/alerts/{alert_id}/acknowledge
Acknowledge an alert.

## MQTT Topics

| Topic | Direction | QoS | Description |
|-------|-----------|-----|-------------|
| `compostsync/{user}/{node}/telemetry` | Node→Cloud | 1 | Sensor data |
| `compostsync/{user}/{node}/status` | Node→Cloud | 1 | Node status |
| `compostsync/{user}/{node}/command` | Cloud→Node | 1 | Commands |
| `compostsync/{user}/alerts` | Cloud→User | 2 | Alerts |
| `compostsync/{user}/ml/forecast` | Cloud→User | 1 | ML predictions |

## WebSocket (future)

`wss://api.compostsync.local/ws/{device_id}` — real-time streaming.