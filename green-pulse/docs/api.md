# GreenPulse API Specification

Base URL: `https://api.greenpulse.local/api/v1`

## Health

### GET /health
```json
{ "status": "ok", "mqtt": true }
```

## Plants

### POST /plants
Create a new plant (paired to a tag).

**Query params:** `user_id`, `tag_id`, `name`, `species_name`, `profile_id`, `location`, `auto_water`

**Response:**
```json
{ "id": 1, "tag_id": 3, "name": "Big Mike" }
```

### GET /plants/{user_id}
List all plants for a user with latest telemetry + risk scores.

**Response:**
```json
[
  {
    "id": 1, "tag_id": 3, "name": "Big Mike",
    "species": "Monstera deliciosa", "location": "Living Room",
    "auto_water": true,
    "soil_moisture": 45, "battery_pct": 82,
    "status": 0, "disease_risk": 15, "water_risk": 30,
    "hours_to_water": 72
  }
]
```

### GET /plant/{plant_id}/telemetry
Per-plant time-series telemetry.

**Query:** `hours` (default 24)

**Response:**
```json
[
  { "ts": "2025-06-20T10:00:00", "soil": 45, "lux": 1200.5,
    "temp_c": 22.3, "humidity": 45.2, "batt": 82 }
]
```

### GET /plant/{plant_id}/watering
Watering event log.

**Query:** `days` (default 30)

**Response:**
```json
[
  { "ts": "2025-06-19T14:00:00", "source": "auto",
    "ml": 250, "duration_s": 30, "status": 0, "pre_moisture": 25 }
]
```

### GET /plant/{plant_id}/scans
Leaf scan results.

**Response:**
```json
[
  { "ts": "2025-06-18T09:00:00", "disease": "powdery mildew",
    "disease_conf": 87, "pests": 0,
    "image_url": "https://storage.../scan_001.jpg" }
]
```

### GET /plant/{plant_id}/risk
Per-plant risk scores + 7-day trend.

**Response:**
```json
{
  "current": {
    "disease_risk": 15, "water_risk": 30, "light_risk": 5,
    "status": 0, "hours_to_water": 72
  },
  "trend": [
    { "ts": "...", "disease": 10, "water": 25, "light": 5 }
  ]
}
```

### POST /plant/{plant_id}/water
Trigger manual watering.

**Response:**
```json
{ "status": "watering_command_sent", "tag_id": 3 }
```

## Alerts

### GET /user/{user_id}/alerts
**Query:** `days` (default 7)

**Response:**
```json
[
  { "id": 1, "ts": "...", "type": "low_moisture",
    "severity": "high", "message": "Soil moisture low: 18% — water now",
    "plant_id": 3, "acknowledged": false }
]
```

## Scans

### POST /scan/upload
Upload a multispectral leaf image (multipart form).

**Params:** `tag_id`, `file` (image)

**Response:**
```json
{ "status": "processing", "tag_id": 3, "bytes_received": 1048576 }
```

## Species

### GET /species/{species_id}
Get species care profile from the plant database.

**Response:**
```json
{
  "name": "Monstera deliciosa",
  "light": "bright indirect",
  "water": "when top 50% dry",
  "humidity": "40-60%",
  "min_moisture": 35, "max_moisture": 80
}
```

## WebSocket

### WS /ws/alerts/{user_id}
Real-time alert push.

```json
{ "type": "disease", "severity": "high",
  "message": "Spider mites detected on your Calathea", "plant_id": 5 }
```