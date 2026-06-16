# CradleKeep — API Documentation

## Base URL

```
https://api.cradlekeep.com/v1
```

Local: `http://hub.local:8000`

## Authentication

All API calls require a bearer token obtained during device pairing.

```
Authorization: Bearer <token>
```

## Endpoints

### Dashboard

#### GET /api/dashboard

Get current state of all nodes (latest readings).

**Response:**
```json
{
  "crib": {
    "breath_rate": 32,
    "breath_regularity": 85,
    "movement_score": 12,
    "position": 1,
    "temp_c": 34.5,
    "wetness": false,
    "alert_level": 0
  },
  "nursery": {
    "cry_type": 0,
    "cry_confidence": 0,
    "room_temp_c": 22.3,
    "humidity_pct": 52.0,
    "co2_ppm": 420,
    "light_lux": 2,
    "noise_db": 25
  },
  "feeding": {
    "state": 0,
    "bottle_temp_c": null,
    "volume_consumed_ml": 95,
    "duration_s": 720
  },
  "cry": {
    "type": 0,
    "confidence": 0,
    "timestamp": null
  }
}
```

---

### Crib Data

#### POST /api/crib/data

Submit crib pad sensor data from hub.

**Request:**
```json
{
  "breath_rate": 32,
  "breath_regularity": 85,
  "movement_score": 12,
  "position": 1,
  "temp_c_x10": 345,
  "wetness_flag": 0,
  "wetness_level": 15,
  "breath_apnea_count": 0,
  "alert_level": 0,
  "battery_pct": 92
}
```

#### GET /api/crib/data

Get recent crib readings.

**Query Parameters:**
- `hours` (int, default 24): Hours of data to return
- `limit` (int, default 1000): Max number of readings

#### GET /api/crib/breathing

Get breathing summary statistics.

**Query Parameters:**
- `hours` (int, default 24): Hours of data to analyze

**Response:**
```json
{
  "avg_breath_rate": 33.5,
  "min_breath_rate": 24,
  "max_breath_rate": 48,
  "avg_regularity": 82.3,
  "total_readings": 43200,
  "alert_count": 2
}
```

---

### Nursery Data

#### POST /api/nursery/data

Submit nursery monitor sensor data.

#### GET /api/nursery/environment

Get room environment summary.

**Response:**
```json
{
  "avg_temp_c": 22.3,
  "avg_humidity_pct": 52.0,
  "avg_co2_ppm": 420,
  "avg_voc_index": 120,
  "avg_light_lux": 2,
  "avg_noise_db": 25
}
```

---

### Feeding Data

#### POST /api/feeding/data

Submit feeding station data.

#### GET /api/feeding/history

Get feeding history.

**Query Parameters:**
- `days` (int, default 7): Days of history to return

**Response:**
```json
[
  {
    "timestamp": "2024-01-15T14:30:00Z",
    "volume_ml": 95,
    "duration_s": 720,
    "bottle_temp_c": 37.0
  }
]
```

#### POST /api/feeding/start

Start bottle warming.

**Request:**
```json
{
  "target_temp_c_x10": 370,
  "auto_start": false
}
```

#### POST /api/feeding/stop

Stop bottle warming.

---

### Cry Events

#### POST /api/cry/event

Submit cry classification event.

**Request:**
```json
{
  "cry_type": 1,
  "cry_confidence": 200,
  "cry_intensity": 150,
  "duration_s": 5,
  "preceding_sleep_stage": 2,
  "time_since_feed_min": 135,
  "time_since_sleep_min": 82
}
```

#### GET /api/cry/patterns

Get cry pattern analysis.

**Query Parameters:**
- `days` (int, default 7): Days of data to analyze

**Response:**
```json
[
  {
    "cry_type": 1,
    "count": 23,
    "avg_confidence": 195,
    "avg_duration_s": 45,
    "hour": 3
  }
]
```

---

### Breathing Alerts

#### POST /api/breathing/alert

Submit breathing safety alert.

#### GET /api/breathing/alerts

Get breathing alert history.

**Query Parameters:**
- `hours` (int, default 24): Hours of alerts to return

---

### Sleep Analysis

#### GET /api/sleep/analysis

Get sleep stage analysis.

**Query Parameters:**
- `hours` (int, default 24): Hours of data to analyze

**Response:**
```json
[
  {
    "stage": "deep",
    "episodes": 4,
    "total_duration_s": 10800,
    "avg_confidence": 180
  }
]
```

---

### Sound Control

#### POST /api/sound/play

Play a soothing sound on hub speaker.

**Request:**
```json
{
  "sound_type": 1,
  "duration_s": 1800
}
```

**Sound types:** 1=White noise, 2=Lullaby 1, 3=Lullaby 2, 4=Heartbeat, 5=Rain, 6=Ocean, 7=Shushing

#### POST /api/sound/stop

Stop currently playing sound.

---

### WebSocket

#### WS /ws

Real-time data stream. Receives all sensor data as JSON messages.

**Message format:**
```json
{
  "topic": "cradle-keep/crib/data",
  "data": { ... },
  "timestamp": "2024-01-15T14:30:00.123Z"
}
```

## Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 201 | Created |
| 400 | Bad Request |
| 401 | Unauthorized |
| 404 | Not Found |
| 429 | Rate Limited |
| 500 | Internal Server Error |

## Rate Limits

- Sensor data endpoints: 1 request per second
- Dashboard/dashboard: 1 request per 5 seconds
- Sound commands: 1 request per 3 seconds