# CalmGrid API Specification

Base URL: `https://api.calmgrid.local/api/v1`

## Health Check

### GET /health
Returns system health status.

**Response:** `{"status": "ok"}`

---

## Data Ingestion (Hub → Cloud)

### POST /ingest/vitals
Ingest vital signs from the wrist band (via hub).

**Request:**
```json
{
  "user_id": 1,
  "hr": 72,
  "hrv_ms": 45.2,
  "eda_scl": 8.5,
  "eda_scr": 3.2,
  "temp_c": 33.1,
  "activity": 5,
  "steps": 8420,
  "battery": 85,
  "flags": 0
}
```

**Response:** `{"status": "stored"}`

### POST /ingest/prosody
Ingest prosody stress classification from room sentinel.

**Request:**
```json
{
  "user_id": 1,
  "prosody_class": 2,
  "confidence": 78,
  "speech_min": 12,
  "f0_dev": 150
}
```

---

## User Data APIs

### GET /user/{uid}/stress
Get current + 14-day stress score trend.

**Response:**
```json
{
  "current": { "stress": 45, "burnout_risk": 30, "recovery": 65 },
  "trend": [
    { "ts": "2026-06-19T10:00:00", "stress": 35, "burnout": 28, "recovery": 70 },
    { "ts": "2026-06-19T10:15:00", "stress": 45, "burnout": 30, "recovery": 65 }
  ]
}
```

### GET /user/{uid}/vitals?hours=24
Get 24h of vital sign data.

### GET /user/{uid}/episodes?days=7
Get acute stress episode timeline.

**Response:**
```json
[
  { "ts": "2026-06-19T09:30:00", "hr": 95, "hrv_ms": 22.1, "eda_scr": 8.5, "activity": 5 }
]
```

### GET /user/{uid}/burnout
Get 14-day burnout risk forecast.

**Response:**
```json
{
  "risk": 35,
  "current_stress": 45,
  "avg_stress_30d": 42,
  "trend": [{ "ts": "...", "burnout": 30 }]
}
```

### GET /user/{uid}/interventions?days=7
Get intervention history + efficacy.

**Response:**
```json
[
  { "ts": "...", "type": 0, "duration_s": 300, "efficacy": 72, "hrv_delta": 8.5 }
]
```

### GET /user/{uid}/alerts?days=7
Get alert history.

### POST /therapist/report/{uid}
Generate a structured therapist report.

**Response:**
```json
{
  "user_id": 1,
  "generated_at": "2026-06-19T12:00:00",
  "summary": {
    "avg_hrv_24h": 42.5,
    "avg_stress_30d": 44,
    "burnout_risk": 35,
    "acute_stress_episodes": 7
  },
  "vitals_trend": [...],
  "stress_trend": [...],
  "interventions": [...],
  "recent_alerts": [...]
}
```

---

## WebSocket

### WS /ws/alerts/{uid}
Real-time alert push to mobile app.

**Messages:**
```json
{ "type": "high_stress", "message": "High stress - intervene now" }
{ "type": "burnout", "message": "Burnout risk rising" }
{ "type": "acute_stress", "message": "Acute stress episode detected" }
```