# MosquitoSync — API Specification

Base URL: `http://localhost:8080/api/v1`

## Authentication

### POST /auth/login
Returns JWT token.

**Request:**
```json
{ "username": "demo", "password": "demo" }
```

**Response:**
```json
{ "access_token": "eyJ...", "token_type": "bearer" }
```

## Devices

### GET /devices
List all MosquitoSync devices.

**Response:**
```json
[
  { "device_id": "hub-001", "device_type": "hub", "online": true }
]
```

### POST /devices/{device_id}/ota
Trigger OTA firmware update.

**Query:** `version=1.1.0`

## Acoustic Sentinel

### GET /acoustic
Latest 20 acoustic sentinel readings.

**Response:**
```json
[
  {
    "node_id": 1,
    "timestamp": "2026-07-20T12:00:00Z",
    "mosquito_detected": true,
    "species_class": 0,
    "species_name": "Aedes aegypti",
    "confidence_pct": 87.5,
    "wingbeat_freq_hz": 483.2,
    "detections_24h": 12,
    "temp_c": 26,
    "humidity_pct": 65
  }
]
```

### GET /acoustic/history?hours=24
Historical acoustic detection data.

## CO2 Trap

### GET /trap
Latest trap readings (temp, humidity, IR breaks, captures, propane level).

### GET /trap/images?limit=10
Recent trap camera capture images (for CaptureCount CNN).

## Window Barriers

### GET /barrier/status
Per-barrier status (open/closed, battery, cycles).

### POST /barrier/close
Close all window barriers immediately.

### POST /barrier/open
Open all window barriers.

## Weather

### GET /weather
Current weather conditions (temp, humidity, pressure, wind, rain).

## Alerts

### GET /alerts?limit=50
List recent alerts (species detected, disease risk, trap full, etc.).

### PUT /alerts/{alert_id}/ack
Acknowledge an alert.

## Risk Scores

### GET /bite-risk
Personal bite risk score (0–100) + recommendations.

**Response:**
```json
{
  "score": 45,
  "level": "Moderate",
  "dominant_species": "Aedes aegypti",
  "recommendations": ["Apply repellent before going outside"]
}
```

### GET /disease-risk
Disease risk score + per-disease breakdown (dengue, West Nile, malaria).

**Response:**
```json
{
  "score": 30,
  "level": "Moderate",
  "dengue_risk": 0.15,
  "west_nile_risk": 0.08,
  "malaria_risk": 0.02,
  "contributing_factors": { "temperature": 0.3, "rain_14d": 0.2 }
}
```

### GET /activity-forecast
72-hour mosquito activity forecast (LSTM).

**Response:**
```json
{
  "timestamps": ["2026-07-20T12:00:00Z", "..."],
  "activity_index": [0.3, 0.5, "..."],
  "confidence_low": [0.25, "..."],
  "confidence_high": [0.35, "..."]
}
```

## Statistics

### GET /species?period=24h
Species detection breakdown (24h/7d/30d).

### GET /trap-count?days=7
Daily capture count history.

## ML Predictions

### GET /ml/predict/activity
72-hour activity forecast prediction.

### GET /ml/predict/disease
Disease risk prediction (dengue, West Nile, malaria).

### GET /ml/predict/bite
Personal bite risk prediction.

## WebSocket

### WS /ws
Real-time stream of telemetry, alerts, and species detection events.

**Events:**
- `heartbeat` — periodic system status
- `barrier_command` — barrier close/open command
- `species_detected` — mosquito species detection
- `disease_alert` — disease risk alert
- `trap_full` — CO2 trap bag full