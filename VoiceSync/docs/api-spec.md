# VoiceSync — API Specification

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
List all VoiceSync devices.

**Response:**
```json
[
  { "device_id": "hub-001", "device_type": "hub", "online": true }
]
```

### POST /devices/{device_id}/ota
Trigger OTA firmware update.

## Vocal Band

### GET /vocal-band
Latest 20 vocal band readings (F0, jitter, shimmer, HNR, phonation, etc.).

**Response:**
```json
[
  {
    "node_id": 1,
    "timestamp": "2026-07-22T12:00:00Z",
    "f0_hz": 140.5,
    "jitter_pct": 0.52,
    "shimmer_pct": 2.10,
    "hnr_db": 22.3,
    "phonation_pct": 15,
    "skin_temp_c": 35.2,
    "heart_rate": 72,
    "hrv_rmssd": 45,
    "stress_level": 20
  }
]
```

### GET /vocal-band/history?hours=24
Historical vocal metrics.

## Room Sentinel

### GET /room-sentinel
Latest room sentinel data (voice quality, environment, talking detection).

### GET /room-sentinel/history?hours=24
Historical voice quality data.

## Hydration

### GET /hydration
Current hydration status (water mass, sips, intake).

### GET /hydration/history?hours=24
Historical water intake.

## Humidity

### GET /humidity
Current room humidity + humidifier status.

### POST /humidifier/control?action=on|off
Control smart humidifier.

## Alerts

### GET /alerts?limit=50
List recent alerts.

### PUT /alerts/{alert_id}/ack
Acknowledge an alert.

## Vocal Health

### GET /vocal-health
Vocal Health Score (0–100) + recommendations.

**Response:**
```json
{
  "score": 85,
  "level": "Good",
  "f0_hz": 140.5,
  "jitter_pct": 0.52,
  "shimmer_pct": 2.10,
  "hnr_db": 22.3,
  "phonation_pct": 15,
  "recommendations": ["Your vocal health is good. Keep up the hydration!"]
}
```

### GET /voice-disorder-risk
7-day voice disorder risk + per-disorder breakdown.

**Response:**
```json
{
  "score": 15,
  "level": "Low",
  "nodules_risk": 0.05,
  "reflux_risk": 0.02,
  "fatigue_risk": 0.08,
  "contributing_factors": { "phonation": 0.3 }
}
```

### GET /vocal-load
Today's cumulative vocal dose (NCVS safe dose).

### GET /voice-quality?hours=24
Voice quality classification history.

### GET /reflux-risk
Laryngopharyngeal reflux (LPR) damage assessment.

## ML Predictions

### GET /ml/predict/risk
7-day voice disorder risk forecast (LSTM).

**Response:**
```json
{
  "timestamps": ["2026-07-22T12:00:00Z", "..."],
  "risk_index": [0.15, 0.20, "..."],
  "confidence_low": [0.12, "..."],
  "confidence_high": [0.18, "..."]
}
```

### GET /ml/predict/voice
Latest voice quality prediction from VoiceNet.

## Clinical Reports

### GET /reports/clinical
Generate speech-pathologist-ready clinical report (PDF in production).

**Response includes:**
- Vocal health score + level
- Disorder risk score + level
- Acoustic features (F0, jitter, shimmer, HNR)
- Clinical thresholds reference
- Vocal load analysis (NCVS)
- Reflux assessment
- Recommendations
- Notes for SLP

## WebSocket

### WS /ws
Real-time stream of telemetry, alerts, and voice status events.

**Events:**
- `heartbeat` — periodic system status
- `humidifier_command` — humidifier control
- `vocal_health_update` — vocal health score change
- `voice_alert` — voice quality alert
- `hoarseness_detected` — elevated jitter
- `high_risk` — high disorder risk