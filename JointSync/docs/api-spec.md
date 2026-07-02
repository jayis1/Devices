# JointSync — API Specification

## Base URL
```
https://api.jointsync.cloud/api/v1
```

## Authentication

All endpoints except `/auth/*` require a Bearer token:
```
Authorization: Bearer <access_token>
```

### POST /auth/register
Register a new patient.

**Request:**
```json
{
  "email": "patient@example.com",
  "name": "Jane Doe",
  "password": "securepass",
  "diagnosis": "ra"
}
```

**Response (201):**
```json
{
  "id": "uuid-string",
  "email": "patient@example.com",
  "name": "Jane Doe",
  "diagnosis": "ra"
}
```

### POST /auth/login
OAuth2 password flow.

**Request (form-encoded):**
```
username=patient@example.com&password=securepass
```

**Response:**
```json
{
  "access_token": "eyJ...",
  "token_type": "bearer"
}
```

---

## Joints

### GET /joints
List all tracked joints for the authenticated patient.

**Response:**
```json
[
  {"id": "uuid", "joint_type": "knee", "side": "left", "tag_id": 1},
  {"id": "uuid", "joint_type": "knee", "side": "right", "tag_id": 2}
]
```

### POST /joints
Add a new joint to tracking.

### GET /joints/{id}/rom?hours=24
Get ROM history.

**Response:**
```json
[
  {"time": "2024-01-15T10:30:00Z", "joint_angle": 95.5},
  {"time": "2024-01-15T10:31:00Z", "joint_angle": 96.0}
]
```

### GET /joints/{id}/temperature?hours=24
Get temperature history.

**Response:**
```json
[
  {"time": "2024-01-15T10:30:00Z", "skin_temp": 32.1, "bilateral_delta": 0.3}
]
```

### GET /joints/{id}/thermal?limit=10
Get recent thermal scans.

### GET /joints/{id}/flare-risk
Get 7-day flare prediction.

**Response:**
```json
{
  "target_date": "2024-01-22T00:00:00Z",
  "risk_score": 0.12,
  "confidence": 0.85,
  "contributing_factors": {
    "rom_decline": 0.35,
    "temp_delta": 0.45,
    "hrv_decline": 0.20
  }
}
```

---

## Therapy

### POST /therapy/sessions
Start a compression therapy session.

### GET /therapy/sessions
List therapy sessions.

---

## Reports

### GET /reports/clinical
Generate a rheumatologist-ready clinical report.

**Response:**
```json
{
  "patient": {"name": "Jane Doe", "diagnosis": "ra"},
  "report_date": "2024-01-15T12:00:00Z",
  "joints": [
    {
      "type": "knee",
      "side": "left",
      "current_rom": 95.0,
      "current_temp": 32.1,
      "bilateral_delta": 0.3,
      "swelling_grade": 0,
      "flare_risk_7day": 0.12
    }
  ]
}
```

---

## WebSocket

### WS /ws/alerts
Real-time alert stream.

**Messages:**
```json
{"type": "inflammation", "joint": "left_knee", "probability": 0.85}
{"type": "flare_warning", "target_date": "2024-01-22", "risk": 0.72}
{"type": "therapy_reminder", "joint": "left_knee", "overdue_hours": 6}
```

---

## Health

### GET /health
```json
{"status": "ok", "service": "jointsync-api", "version": "1.0.0"}
```