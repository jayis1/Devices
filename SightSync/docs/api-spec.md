# SightSync — API Specification

## Base URL

```
https://api.sightsync.cloud/v1
```

## Authentication

JWT Bearer token. Obtain via `/auth/login`.

## Endpoints

### Health

```
GET /health
→ 200 { "status": "ok" }
```

### Authentication

```
POST /auth/register
Body: { "email": "...", "password": "...", "name": "..." }
→ 201 { "user_id": "...", "token": "..." }

POST /auth/login
Body: { "email": "...", "password": "..." }
→ 200 { "user_id": "...", "token": "..." }
```

### Fatigue

```
GET /fatigue/current
→ 200 {
  "fatigue_score": 42,
  "alert_level": 1,
  "blink_rate": 12,
  "viewing_distance_mm": 450,
  "ambient_lux": 520,
  "minutes_since_break": 15,
  "timestamp": "2026-07-06T12:00:00Z"
}

GET /fatigue/history?days=7
→ 200 {
  "data": [
    { "timestamp": "...", "fatigue_score": 35, "blink_rate": 14, ... },
    ...
  ]
}
```

### Distance

```
GET /distance/history?days=1
→ 200 {
  "data": [
    { "timestamp": "...", "distance_mm": 420, "near_work_flag": 0 },
    ...
  ],
  "summary": {
    "avg_distance_mm": 480,
    "near_work_minutes": 95,
    "too_close_events": 3
  }
}
```

### Blink

```
GET /blink/history?days=1
→ 200 {
  "data": [ { "timestamp": "...", "bpm": 11, "confidence": 85 }, ... ],
  "summary": {
    "avg_bpm": 9.2,
    "min_bpm": 4,
    "low_blink_events": 7
  }
}
```

### Light Exposure

```
GET /light/history?days=1
→ 200 {
  "data": [ { "timestamp": "...", "lux": 520, "blue_mw": 340, "cct": 4500 }, ... ],
  "summary": {
    "avg_lux": 480,
    "insufficient_light_minutes": 45,
    "blue_dose_mj_cm2": 8.2
  }
}
```

### Myopia Forecast

```
GET /myopia/forecast?child_id=...
→ 200 {
  "risk_30day": 25,
  "risk_90day": 38,
  "refractive_delta_diopter": -0.12,
  "near_work_today_min": 95,
  "outdoor_today_min": 35,
  "avg_distance_mm": 420,
  "recommendation": "more_outdoor",
  "timestamp": "..."
}
```

### Optometrist Report

```
GET /report/optometrist?format=pdf
→ 200 (application/pdf)

GET /report/optometrist?format=json
→ 200 {
  "report_date": "...",
  "patient": { "name": "...", "age": 12 },
  "visual_hygiene_score": 72,
  "daily_fatigue_avg": 38,
  "blink_rate_avg": 9.1,
  "near_work_daily_avg_min": 105,
  "outdoor_light_daily_avg_min": 28,
  "viewing_distance_avg_mm": 440,
  "blue_light_dose_daily_avg": 7.8,
  "forward_head_posture_pct": 42,
  "20_20_20_compliance_pct": 65,
  "dry_eye_risk_avg": 28,
  "myopia_risk_90day": 38,
  "recommendations": [ ... ]
}
```

### Lamp Control

```
GET /lamp/policy
→ 200 {
  "mode": "circadian",
  "schedule": [
    { "hour": 6, "cct": 3000, "brightness": 60 },
    { "hour": 12, "cct": 5500, "brightness": 80 },
    { "hour": 18, "cct": 3500, "brightness": 55 },
    { "hour": 22, "cct": 1800, "brightness": 15 }
  ]
}

POST /lamp/override
Body: { "cct": 4500, "brightness": 70, "duration_min": 30 }
→ 200 { "status": "ok" }
```

### MQTT Webhook (Hub → Cloud)

```
POST /mqtt/inbound
Body: { "topic": "sightsync/hub/fatigue", "payload": { ... } }
→ 200 { "status": "ok" }
```

## Error Responses

```
400 { "error": "bad_request", "message": "..." }
401 { "error": "unauthorized", "message": "..." }
404 { "error": "not_found", "message": "..." }
500 { "error": "internal_error", "message": "..." }
```

## Rate Limits

- 100 requests/min per user (standard)
- 1000 requests/min per user (premium)