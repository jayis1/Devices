# BreathHome — API Reference

## Base URL

```
http://breathhome.local:8000/api
```

## Authentication

All endpoints require API key header:
```
Authorization: Bearer <api_key>
```

## Endpoints

### Rooms

#### GET /rooms
List all rooms with current AQI.

**Response:**
```json
[
  {
    "room_id": 1,
    "room_name": "Living Room",
    "aqi_score": 42,
    "aqi_category": 0,
    "pm2_5": 8.7,
    "co2": 520,
    "voc_index": 85,
    "hcho": 0.023,
    "temperature": 22.5,
    "humidity": 45,
    "mold_risk_pct": 12,
    "radon_bq_m3": null,
    "timestamp": "2026-06-14T10:30:00Z"
  }
]
```

#### GET /rooms/{room_id}?hours=24
Get detailed air quality history for a room.

**Parameters:**
- `hours` (int, default 24, max 168): Time range in hours

**Response:** Array of sensor readings at 30-second intervals.

#### GET /rooms/{room_id}/latest
Get the most recent reading for a room.

#### GET /rooms/{room_id}/trends?hours=168
Get hourly aggregated trends for a room.

**Response:**
```json
[
  {
    "hour": "2026-06-14T10:00:00Z",
    "avg_pm25": 8.5,
    "avg_co2": 510,
    "avg_voc": 82,
    "avg_temp": 22.3,
    "avg_humidity": 44,
    "avg_aqi": 40,
    "max_aqi": 55,
    "avg_mold_risk": 11
  }
]
```

### Exposure

#### GET /exposure/{tag_id}?hours=24
Get personal exposure history for a wearable tag.

**Response:**
```json
[
  {
    "tag_id": 1,
    "timestamp": "2026-06-14T10:30:00Z",
    "eco2": 450,
    "tvoc": 80,
    "temperature": 22.1,
    "humidity": 44,
    "activity": 1,
    "personal_aqi": 38,
    "battery_pct": 85,
    "symptom_flag": 0
  }
]
```

### Alerts

#### GET /alerts?limit=50&category=danger&acknowledged=false
Get recent alert events.

**Parameters:**
- `limit` (int, default 50, max 500): Number of alerts to return
- `category` (string, optional): Filter by category (info, warning, danger, critical)
- `acknowledged` (bool, optional): Filter by acknowledgment status

#### PUT /alerts/{alert_id}/acknowledge
Mark an alert as acknowledged.

### HVAC Control

#### GET /hvac/status
Get current HVAC controller state.

**Response:**
```json
{
  "vent_positions": [70, 60, 80, 50, 30, 50, 50, 50],
  "purifier_speed": 2,
  "filter_health_pct": 68.5,
  "duct_pressure_pa": 135.2,
  "supply_air_temp_c": 23.1,
  "blower_current_ma": 3.2,
  "relay_states": 2,
  "timestamp": "2026-06-14T10:30:00Z"
}
```

#### POST /hvac/command
Send a command to the HVAC controller.

**Request Body:**
```json
{
  "vent_positions": [80, 70, 90, 60, 30, 50, 50, 50],
  "purifier_speed": 3,
  "fan_override": true,
  "range_hood": false,
  "bathroom_exhaust": true
}
```

### Analytics

#### GET /analytics/summary
Get overall system analytics summary.

**Response:**
```json
{
  "current_aqi": [...],
  "avg_aqi_24h": [...],
  "alert_counts_24h": {"warning": 3, "danger": 1, "critical": 0},
  "filter_health": {"filter_health_pct": 68.5}
}
```

## WebSocket

#### ws://breathhome.local:8000/ws/realtime
Real-time sensor data streaming.

**Messages:**
```json
{
  "type": "sensor_update",
  "room_id": 1,
  "pm2_5": 8.7,
  "co2": 520,
  "voc_index": 85,
  "aqi_score": 42,
  "timestamp": "2026-06-14T10:30:00Z"
}
```

```json
{
  "type": "alert",
  "room_id": 2,
  "parameter": "co2",
  "value": 1850,
  "category": "danger",
  "message": "DANGER: CO2 = 1850.0 in room 2"
}
```

## MQTT Topics

| Topic | Direction | Purpose |
|-------|-----------|---------|
| `breathhome/sensors/{hub_id}/{room_id}` | Hub → Cloud | Sensor readings |
| `breathhome/hvac/{hub_id}/status` | Hub → Cloud | HVAC state |
| `breathhome/hvac/{hub_id}/cmd` | Cloud → Hub | HVAC commands |
| `breathhome/exposure/{hub_id}/{tag_id}` | Hub → Cloud | Wearable tag data |
| `breathhome/alerts/{hub_id}/{room_id}` | Hub → Cloud | Alert events |
| `breathhome/ota/{node_type}/{node_id}` | Cloud → Hub | OTA firmware updates |