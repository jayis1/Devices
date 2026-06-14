# SleepSync — API Specification

## Base URL

```
http://{hub_ip}:8000/api
```

## Endpoints

### Sleep Data

#### GET /sleep/latest
Get latest sleep data from the sleep strip.

**Response:**
```json
{
  "heart_rate": 580,
  "hrv": 55,
  "resp_rate": 150,
  "rrv": 22,
  "movement": 12,
  "snoring": 5,
  "sleep_stage": 2,
  "sleep_stage_name": "DEEP",
  "stage_confidence": 185,
  "battery_pct": 87
}
```

#### GET /sleep/history?hours=24&device_id=default
Get historical sleep data.

**Response:** Array of sleep data records with timestamps.

#### GET /sleep/score?device_id=default
Get computed sleep score for the current/last night.

**Response:**
```json
{
  "score": 82.5,
  "total_samples": 5760,
  "stages": {
    "deep_pct": 18.2,
    "rem_pct": 22.5,
    "light_pct": 52.1,
    "wake_pct": 7.2
  }
}
```

### Environment

#### GET /env/latest
Get latest room environment data.

**Response:**
```json
{
  "temperature": 1967,
  "humidity": 4320,
  "co2_ppm": 650,
  "hvac_state": 0,
  "heater_state": 2,
  "humidifier_state": 0,
  "errors": 0
}
```

#### GET /env/history?hours=24
Get historical environment data.

#### GET /env/recommendations
Get population + personalized environment recommendations.

**Response:**
```json
{
  "population_optimal": {
    "temperature": {"min": 18.3, "max": 20.0, "unit": "°C"},
    "humidity": {"min": 40.0, "max": 50.0, "unit": "%RH"},
    "co2": {"min": 0, "max": 800, "unit": "ppm"},
    "light": {"min": 0, "max": 1, "unit": "lux"},
    "noise": {"min": 0, "max": 30, "unit": "dBA"}
  },
  "personalized": null,
  "note": "Personalized recommendations appear after 7 nights of data"
}
```

### Controls

#### POST /climate/setpoint
Set climate target setpoints.

**Body:**
```json
{
  "temperature": 19.0,
  "humidity": 45.0
}
```

#### POST /shade/position
Set shade position.

**Body:**
```json
{
  "position": 0
}
```

`position`: 0-100 (0=closed, 100=open)

#### POST /alarm
Configure smart alarm.

**Body:**
```json
{
  "window_start": "06:30",
  "window_end": "07:00",
  "enabled": true
}
```

#### POST /sound
Configure soundscape.

**Body:**
```json
{
  "sound_id": 1,
  "volume": 80
}
```

`sound_id`: 0=off, 1=white, 2=pink, 3=brown, 4=rain, 5=ocean, 6=forest, 7=campfire
`volume`: 0-255

### Reports

#### GET /report/daily?date=2026-06-13
Get daily sleep report.

**Response:**
```json
{
  "device_id": "default",
  "date": "2026-06-13",
  "sleep_score": 82.5,
  "total_sleep_min": 450,
  "deep_sleep_pct": 18.2,
  "rem_sleep_pct": 22.5,
  "light_sleep_pct": 52.1,
  "wake_pct": 7.2,
  "sleep_latency_min": 12,
  "waso_count": 2,
  "snoring_min": 15,
  "apnea_events": 0,
  "avg_temp": 19.1,
  "avg_humidity": 44.5,
  "avg_co2": 620,
  "avg_noise": 25,
  "recommendations": "Your deep sleep improved with the cooler temperature..."
}
```

### Health

#### GET /health/apnea_risk
Get 7-day apnea risk assessment.

**Response:**
```json
{
  "risk": "low",
  "ahi_estimate": 2.1,
  "avg_snoring_intensity": 15.3,
  "snoring_events_7d": 42,
  "recommendation": "No significant apnea indicators detected"
}
```

### WebSocket

#### WS /ws/live
Real-time data stream. Server pushes JSON every 5 seconds:

```json
{
  "sleep": {"default": {...}},
  "env": {"default": {...}},
  "shade": {"default": {...}}
}
```

## Error Responses

All errors return:
```json
{
  "detail": "Error description"
}
```

Common HTTP status codes:
- 200: Success
- 400: Bad request (invalid parameters)
- 404: Resource not found
- 500: Internal server error