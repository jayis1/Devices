# PoolSync API Specification

## Base URL
```
https://api.poolsync.io/v1
```

## Authentication
All endpoints require Bearer token (JWT) in Authorization header.
```
Authorization: Bearer <jwt_token>
```

## Endpoints

### Pool Health

#### GET /health-score
Get overall pool health score (0-100).

**Response:**
```json
{
  "overall": 82,
  "chemistry": 85,
  "clarity": 90,
  "safety": 95,
  "energy": 70
}
```

### Chemistry

#### GET /chemistry
Get recent chemistry readings.

**Query Parameters:**
| Param | Type | Default | Description |
|-------|------|---------|-------------|
| limit | int | 100 | Number of readings (max 1000) |
| probe_id | int | null | Filter by probe (0-2) |
| since | datetime | null | ISO 8601 timestamp |

**Response:**
```json
[
  {
    "probe_id": 0,
    "ph": 7.42,
    "orp_mv": 735,
    "free_cl_ppm": 2.8,
    "temperature_c": 27.5,
    "conductivity_us": 1250,
    "turbidity_ntu": 0.3,
    "timestamp": "2026-06-26T14:30:00Z"
  }
]
```

#### POST /chemistry
Submit new chemistry reading from probe.

**Request Body:**
```json
{
  "probe_id": 0,
  "ph": 7.42,
  "orp_mv": 735,
  "free_cl_ppm": 2.8,
  "temperature_c": 27.5,
  "conductivity_us": 1250,
  "turbidity_ntu": 0.3,
  "timestamp": "2026-06-26T14:30:00Z"
}
```

#### GET /chemistry/ideal
Get ideal chemistry ranges.

**Response:**
```json
{
  "ph": {"min": 7.2, "max": 7.6, "ideal": 7.4, "unit": "pH"},
  "free_cl_ppm": {"min": 2.0, "max": 4.0, "ideal": 3.0, "unit": "ppm"},
  "orp_mv": {"min": 650, "max": 800, "ideal": 750, "unit": "mV"},
  "temperature_c": {"min": 26, "max": 30, "ideal": 28, "unit": "°C"},
  "conductivity_us": {"min": 800, "max": 2000, "ideal": 1200, "unit": "µS/cm"},
  "turbidity_ntu": {"min": 0, "max": 0.5, "ideal": 0.2, "unit": "NTU"}
}
```

### Clarity

#### GET /clarity
Get recent clarity readings.

#### POST /clarity
Submit new clarity reading from camera.

**Request Body:**
```json
{
  "clarity_score": 0.85,
  "green_channel": 0.32,
  "turbidity_ntu": 0.3,
  "algae_risk": 0,
  "image_hash": "abc123def456",
  "timestamp": "2026-06-26T14:30:00Z"
}
```

#### GET /clarity/image/{image_hash}
Retrieve full-resolution image by hash.

### Algae Forecast

#### GET /algae-forecast
Get 3-day algae outbreak forecast.

**Response:**
```json
{
  "risk_level": "low",
  "confidence": 0.85,
  "forecast_24h": 0.12,
  "forecast_48h": 0.18,
  "forecast_72h": 0.25,
  "contributing_factors": [
    "Free chlorine slightly below ideal (2.8 ppm)",
    "Warm water (27.5°C) accelerates growth"
  ]
}
```

### Equipment

#### GET /equipment
Get current equipment status.

#### POST /equipment/command
Send command to equipment controller.

**Request Body:**
```json
{
  "device_id": 0,
  "command": 1,
  "parameter": null,
  "duration_s": null
}
```

| device_id | Name |
|-----------|------|
| 0 | Pump |
| 1 | Heater |
| 2 | Pool Light |
| 3 | Spa Light |
| 4 | Valve 1 |
| 5 | Valve 2 |
| 6 | Blower |
| 7 | Spare |

| command | Action |
|---------|--------|
| 0 | Off |
| 1 | On |
| 2 | Toggle |
| 3 | Set speed/parameter |

#### POST /equipment/dose
Send chemical dosing command.

**Request Body:**
```json
{
  "pump_id": 0,
  "volume_ml": 50.0,
  "duration_s": 120
}
```

| pump_id | Chemical |
|---------|----------|
| 0 | Muriatic acid |
| 1 | Liquid chlorine |
| 2 | Clarifier |

### Energy

#### GET /energy/schedule
Get optimal pump/heater schedule.

#### GET /energy/usage?days=7
Get energy usage analytics.

### Alarms

#### GET /alarms
Get recent alarms.

**Query Parameters:**
| Param | Type | Default | Description |
|-------|------|---------|-------------|
| limit | int | 50 | Number of alarms |
| severity | int | null | Min severity (0-3) |

**Alarm Types:**
| Type | Severity | Description |
|------|----------|-------------|
| entrapment | 3 (emergency) | Suction entrapment detected |
| gfci_fault | 3 (emergency) | Ground fault detected |
| unauth_access | 2 (warning) | Unsupervised pool access |
| chem_outside | 2 (warning) | Chemistry out of safe range |
| freeze | 1 (info) | Freeze protection activated |
| equip_fault | 2 (warning) | Equipment malfunction |
| low_battery | 1 (info) | Probe battery low |
| dose_fail | 2 (warning) | Dosing verification failed |

### WebSocket

#### WS /ws
Real-time event stream.

**Message Types:**
```json
{"type": "subscribe", "channels": ["chemistry", "alarms", "equipment"]}
{"type": "chemistry", "data": {...}}
{"type": "clarity", "data": {...}}
{"type": "equipment", "data": {...}}
{"type": "alarm", "data": {...}}
```

### Professional

#### GET /professional/pools
List pools managed by service professional.

#### GET /professional/report/{pool_id}
Generate pool service report.

## Error Responses

All errors follow RFC 7807 Problem Details:
```json
{
  "type": "https://api.poolsync.io/errors/dose-limit-exceeded",
  "title": "Dose Limit Exceeded",
  "status": 400,
  "detail": "Requested acid dose of 300mL exceeds safety limit of 200mL per cycle",
  "instance": "/v1/equipment/dose"
}
```