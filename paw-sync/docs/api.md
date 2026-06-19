# PawSync API Specification

Base URL: `https://api.pawsync.local/api/v1`

## Health Check

### GET /health
Returns system health status.

**Response:** `{"status": "ok"}`

---

## Data Ingestion (Hub → Cloud)

### POST /ingest/vitals
Ingest vital signs from the collar tag (via hub).

**Request:**
```json
{
  "pet_id": 1,
  "hr_bpm": 80,
  "hrv_rmssd_ms": 42.5,
  "skin_temp_c": 38.5,
  "activity_class": 1,
  "gait_symmetry": 45,
  "gait_stride_cm": 55,
  "battery_pct": 85,
  "flags": 0
}
```

**Response:** `{"status": "stored", "alerts": [...]}`

### POST /ingest/behavior
Ingest behavior classification event from the camera.

**Request:**
```json
{
  "pet_id": 1,
  "behavior_class": 1,
  "vocalization": 2,
  "confidence": 85,
  "duration_s": 0,
  "is_anxiety_episode": true,
  "clip_ref": ""
}
```

### POST /ingest/feeding
Ingest feeding event from the smart feeder.

**Request:**
```json
{
  "pet_id": 1,
  "dispensed_g": 50,
  "consumed_g": 45,
  "water_ml": 120,
  "hopper_pct": 80,
  "appetite_loss": false
}
```

---

## Pet Data APIs (Mobile App + Vet Portal)

### GET /pet/{pet_id}/wellness
Current wellness score + 7-day trend.

**Response:**
```json
{
  "wellness": 85,
  "illness_risk": 15,
  "anxiety_level": 10,
  "trend": [
    {"ts": "2024-01-01T00:00:00", "wellness": 85, "illness_risk": 15, "anxiety": 10}
  ]
}
```

### GET /pet/{pet_id}/activity
24-hour activity timeline.

**Response:**
```json
[
  {"ts": "2024-01-01T12:00:00", "activity": "walking", "hr": 95, "hrv": 38.2}
]
```

### GET /pet/{pet_id}/vitals
Current vitals + baseline + 24h trend.

**Response:**
```json
{
  "current": {"hr": 80, "hrv_ms": 42.5, "temp_c": 38.5, "activity": 0},
  "baseline": {"hr": 78, "hrv_ms": 45.0, "established": true},
  "trend": [...]
}
```

### GET /pet/{pet_id}/feeding
Feeding log + intake trends.

**Response:**
```json
[
  {"ts": "2024-01-01T08:00:00", "dispensed_g": 50, "consumed_g": 48,
   "water_ml": 120, "hopper_pct": 80, "appetite_loss": false}
]
```

### GET /pet/{pet_id}/anxiety
Separation anxiety episodes.

**Response:**
```json
[
  {"ts": "2024-01-01T10:00:00", "duration_s": 600,
   "behavior": "pacing", "vocalization": 2}
]
```

### GET /pet/{pet_id}/alerts
Alert history.

**Response:**
```json
[
  {"id": 1, "ts": "2024-01-01T10:00:00", "type": "hrv_decline",
   "severity": "high", "message": "HRV declined 25% below baseline",
   "acknowledged": false}
]
```

### POST /vet/report/{pet_id}
Generate a structured vet report.

**Response:**
```json
{
  "pet_id": 1,
  "generated_at": "2024-01-01T12:00:00",
  "baseline": {"established": true, "baseline_hr": 78, "baseline_hrv_ms": 45.0},
  "current_vitals": {"hr": 80, "hrv_ms": 42.5, "temp_c": 38.5, "activity": 0},
  "wellness": {"wellness": 85, "illness_risk": 15, "anxiety_level": 10, "trend": [...]},
  "feeding_summary": {"recent_meals": [...], "appetite_loss_count": 0},
  "anxiety_episodes": [...],
  "recent_alerts": [...]
}
```

---

## WebSocket

### WS /ws/alerts/{pet_id}
Real-time alert push to the mobile app.

**Messages:**
```json
{
  "type": "hrv_decline",
  "severity": "high",
  "message": "HRV declined 25% below baseline — possible pain or illness"
}
```

---

## MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `pawsync/{pet_id}/vitals` | Hub → Cloud | JSON vitals |
| `pawsync/{pet_id}/behavior` | Camera → Cloud | JSON behavior event |
| `pawsync/{pet_id}/feeding` | Feeder → Cloud | JSON feeding event |
| `pawsync/{pet_id}/wellness` | Hub → Cloud | JSON wellness score |
| `pawsync/{pet_id}/alert` | Hub → Cloud | JSON alert |