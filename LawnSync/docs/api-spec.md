# LawnSync — API Specification

Base URL: `https://api.lawnsync.cloud/api/v1`

## Authentication

### POST /auth/login
Login and receive JWT token.

**Request:**
```json
{ "email": "user@example.com", "password": "secret" }
```

**Response (200):**
```json
{
  "access_token": "eyJ...",
  "token_type": "bearer",
  "expires_in": 86400
}
```

**Response (401):** Invalid credentials

All subsequent requests require header: `Authorization: Bearer <token>`

---

## Devices

### GET /devices
List all devices in the system.

**Response (200):**
```json
[
  {
    "node_id": 0,
    "node_type": "hub",
    "name": "Hub",
    "battery_mv": 0,
    "last_seen": "2026-07-15T12:00:00Z",
    "online": true,
    "firmware_version": "1.0.0"
  }
]
```

### POST /devices/{node_id}/ota
Trigger OTA firmware update for a node.

**Query:** `?version=1.1.0`

**Response (200):**
```json
{ "status": "initiated", "node_id": 1, "version": "1.1.0" }
```

---

## Soil Data

### GET /soil
Get latest soil readings for all nodes.

**Response (200):**
```json
[
  {
    "node_id": 1,
    "timestamp": "2026-07-15T12:00:00Z",
    "moisture_pct": 22.5,
    "temp_c": 18.0,
    "ph": 6.5,
    "nitrogen_mgkg": 35.0,
    "phosphorus_mgkg": 12.0,
    "potassium_mgkg": 80.0,
    "light_lux": 15000,
    "battery_mv": 320
  }
]
```

### GET /soil/history
Get historical soil data for a specific node.

**Query:** `?node_id=1&hours=24`

**Response (200):**
```json
{
  "node_id": 1,
  "data_points": [
    {
      "timestamp": "2026-07-15T11:00:00Z",
      "moisture_pct": 22.5,
      "temp_c": 18.0,
      "ph": 6.5
    }
  ]
}
```

---

## Weather

### GET /weather
Get current weather data from the weather station.

**Response (200):**
```json
{
  "timestamp": "2026-07-15T12:00:00Z",
  "temp_c": 22.3,
  "humidity_pct": 58.0,
  "pressure_hpa": 1013.2,
  "wind_speed_ms": 3.5,
  "wind_dir_deg": 180,
  "rain_mm": 0.0,
  "solar_irr_wm2": 650,
  "uv_index": 4.2
}
```

---

## Irrigation

### GET /irrigation/schedule
Get the current irrigation schedule.

**Response (200):**
```json
{
  "zones": [
    {
      "zone_id": 1,
      "name": "Front Lawn Left",
      "enabled": true,
      "duration_min": 15,
      "days": ["mon", "wed", "fri"],
      "start_time": "06:00",
      "moisture_threshold_pct": 18.0,
      "rain_skip": true,
      "last_run": null,
      "next_run": "2026-07-16T06:00:00Z"
    }
  ],
  "water_saved_liters": 1250,
  "water_saved_pct": 38
}
```

### PUT /irrigation/schedule
Update the irrigation schedule.

**Request:** Same format as GET response

**Response (200):**
```json
{ "status": "updated", "zones": 4 }
```

### POST /irrigation/zone/{zone_id}/run
Manually run a specific irrigation zone.

**Query:** `?duration_min=10`

**Response (200):**
```json
{ "status": "running", "zone": 1, "duration_min": 10 }
```

---

## Scanner Results

### GET /scan/results
Get recent scan results (disease detection, weed mapping, NDVI).

**Query:** `?limit=10`

**Response (200):**
```json
[
  {
    "timestamp": "2026-07-15T06:00:00Z",
    "disease_class": "Brown Patch",
    "confidence": 0.91,
    "avg_ndvi": 0.65,
    "weed_coverage_pct": 3,
    "dominant_weed": "Clover",
    "image_url": "https://lawnsync.cloud/images/scan_0.jpg",
    "gps_lat": 37.7749,
    "gps_lon": -122.4194
  }
]
```

### GET /scan/ndvi
Get the latest NDVI map (64×64 grid).

**Response (200):**
```json
{
  "width": 64,
  "height": 64,
  "ndvi_map": [[0.5, 0.6, ...], ...],
  "avg_ndvi": 0.65
}
```

---

## Alerts

### GET /alerts
List alerts, optionally filtered by acknowledgment status.

**Query:** `?acknowledged=false`

**Response (200):**
```json
[
  {
    "id": "a1",
    "node_id": 1,
    "alert_type": "low_moisture",
    "severity": 2,
    "message": "Zone 1 soil moisture at 14% (below 18% threshold)",
    "timestamp": "2026-07-15T10:00:00Z",
    "acknowledged": false
  }
]
```

### PUT /alerts/{alert_id}/ack
Acknowledge an alert.

**Response (200):**
```json
{ "status": "acknowledged", "alert_id": "a1" }
```

---

## Health Score

### GET /health-score
Get the overall Lawn Health Score (0–100).

**Response (200):**
```json
{
  "score": 78,
  "status": "Good",
  "moisture_score": 72,
  "disease_score": 60,
  "nutrient_score": 85,
  "density_score": 82,
  "recommendations": [
    "Apply fungicide to Backyard zone to treat Brown Patch",
    "Increase irrigation for Zone 1 (moisture below threshold)"
  ]
}
```

---

## Fertilization

### GET /fertilization
Get fertilization recommendation.

**Response (200):**
```json
{
  "recommended": true,
  "days_until_window": 5,
  "npk_ratio": "16-4-8",
  "nitrogen_lb_per_1000sqft": 0.8,
  "phosphorus_lb_per_1000sqft": 0.2,
  "potassium_lb_per_1000sqft": 0.4,
  "notes": "Nitrogen slightly low. Apply when soil temp > 15°C and no rain expected for 48h."
}
```

---

## Water Usage

### GET /water-usage
Get water usage statistics and savings.

**Response (200):**
```json
{
  "today_liters": 95,
  "week_liters": 620,
  "month_liters": 2400,
  "savings_vs_timer_liters": 1250,
  "savings_pct": 38,
  "daily_usage": [
    { "date": "2026-07-15", "liters": 95 }
  ]
}
```

---

## ML Predictions

### GET /ml/predict/disease
Get disease risk prediction based on weather, soil, and scan data.

**Response (200):**
```json
{
  "risk_level": "Moderate",
  "risk_score": 45,
  "likely_diseases": [
    { "name": "Brown Patch", "probability": 0.35 },
    { "name": "Dollar Spot", "probability": 0.18 }
  ],
  "contributing_factors": [
    "High humidity (>70%) for 3+ consecutive days",
    "Soil temperature 22°C (optimal for brown patch)"
  ]
}
```

### GET /ml/predict/soil
Get 14-day soil moisture forecast.

**Query:** `?node_id=1`

**Response (200):**
```json
{
  "forecast_days": 14,
  "daily_moisture": [
    { "date": "2026-07-16", "moisture_pct": 21.2, "confidence": 0.95 }
  ],
  "irrigation_recommended": true,
  "next_irrigation_date": "2026-07-18"
}
```

---

## WebSocket

### WS /ws
Real-time WebSocket connection for live updates.

**Messages (server → client):**
```json
{
  "type": "telemetry",
  "timestamp": "2026-07-15T12:00:00Z",
  "data": { "moisture": 22.5, "temp": 18.0, "health": 78 }
}
```

**Messages (server → client, alert):**
```json
{
  "type": "alert",
  "alert_id": "a4",
  "alert_type": "disease",
  "severity": 3,
  "message": "Brown Patch detected in Backyard"
}
```

---

## Error Responses

All endpoints may return:

| Code | Description |
|------|-------------|
| 400 | Bad request (invalid parameters) |
| 401 | Unauthorized (missing/invalid token) |
| 404 | Resource not found |
| 429 | Rate limit exceeded |
| 500 | Internal server error |

```json
{ "detail": "Error message describing the issue" }
```