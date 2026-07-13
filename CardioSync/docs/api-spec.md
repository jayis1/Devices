# CardioSync — API Specification

Base URL: `http://localhost:8000/api/v1`

## Authentication

All endpoints (except `/auth/*`) require a JWT Bearer token:
```
Authorization: Bearer <token>
```

### POST /auth/register
Register a new user.

**Request:**
```json
{
  "username": "string",
  "email": "string",
  "password": "string",
  "full_name": "string (optional)"
}
```

**Response:** `200 OK`
```json
{ "status": "registered", "username": "string" }
```

### POST /auth/login
Login and receive JWT token.

**Request (form-encoded):**
```
username=string&password=string
```

**Response:** `200 OK`
```json
{ "access_token": "string", "token_type": "bearer" }
```

---

## ECG Endpoints

### GET /ecg/events
List arrhythmia events.

**Query:** `limit` (int, default 50), `offset` (int, default 0)

**Response:**
```json
[
  {
    "id": 1,
    "event_type": "AFib",
    "confidence": 0.97,
    "heart_rate": 142,
    "timestamp": "2026-07-13T10:30:00Z"
  }
]
```

### GET /ecg/events/{event_id}
Get a specific ECG event with ECG strip data.

**Response:**
```json
{
  "id": 1,
  "event_type": "AFib",
  "confidence": 0.97,
  "heart_rate": 142,
  "ecg_strip": [12, 15, 18, ...],
  "timestamp": "2026-07-13T10:30:00Z"
}
```

### WebSocket /ecg/stream
Real-time ECG stream (WebSocket).

**Connection:** `ws://localhost:8000/api/v1/ecg/stream?user_id=1`

**Messages:** ECG sample packets as JSON.

---

## Blood Pressure Endpoints

### GET /bp/records
List BP records.

**Query:** `limit` (int, default 50), `offset` (int, default 0)

### GET /bp/trends
BP trend analysis.

**Query:** `days` (int: 7, 30, or 90)

**Response:**
```json
{
  "period_days": 7,
  "count": 14,
  "avg_systolic": 128.5,
  "avg_diastolic": 82.3,
  "sys_trend_slope": 0.2,
  "dia_trend_slope": 0.1,
  "latest_category": "Hypertension Stage 1",
  "latest_systolic": 135,
  "latest_diastolic": 88,
  "trend": [...]
}
```

---

## HRV Endpoints

### GET /hrv/trends
HRV trends (RMSSD, SDNN).

**Query:** `days` (int, default 7)

**Response:**
```json
{
  "period_days": 7,
  "count": 2016,
  "avg_rmssd": 38.5,
  "avg_sdnn": 45.2,
  "hrv_trend": "stable",
  "records": [...]
}
```

---

## Risk Endpoints

### GET /risk/stroke
30-day stroke risk forecast.

**Response:**
```json
{
  "stroke_risk_30d": 12.5,
  "afib_burden_pct": 15.0,
  "bp_category": "135/88",
  "hrv_trend": "normal",
  "risk_factors": {
    "afib_events_24h": 3,
    "latest_bp": "135/88",
    "latest_rmssd": 35,
    "chads_vasc_score": 2
  }
}
```

---

## Report Endpoints

### GET /reports/monthly
Generate cardiologist-ready monthly report.

**Response:**
```json
{
  "report_id": "report_1_202607",
  "period": "2026-06-13 to 2026-07-13",
  "summary": {
    "total_ecg_events": 12,
    "afib_events": 8,
    "afib_burden_pct": 16.0,
    "bp_readings": 60,
    "avg_systolic": 130,
    "avg_diastolic": 85
  },
  "events": [...],
  "download_url": "/api/v1/reports/report_1_202607/pdf"
}
```

---

## Alert Endpoints

### GET /alerts/contacts
Get emergency contacts.

### POST /alerts/contacts
Set emergency contacts.

**Request:**
```json
{
  "contact_1": "+1234567890",
  "contact_2": "+1987654321"
}
```

---

## Configuration Endpoints

### POST /config/bp-schedule
Set BP measurement schedule (sent to Hub via MQTT).

**Request:**
```json
{
  "am_hour": 7,
  "pm_hour": 19,
  "post_activity": true
}
```

---

## Other

### GET /health
Health check.

### GET /dashboard
Dashboard summary (all key metrics).