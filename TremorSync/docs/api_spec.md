# TremorSync API Specification

Base URL: `https://api.tremorsync.cloud`

## Authentication
All endpoints require `Authorization: Bearer <JWT>` header.

## Endpoints

### Tremor

#### GET `/api/v1/tremor/current`
Returns current tremor classification and amplitude.

**Response:**
```json
{
  "tremor_class": 1,
  "tremor_amplitude": 0.234,
  "bradykinesia": 45.2,
  "onoff_state": 1,
  "hr": 72,
  "timestamp": "2026-08-17T10:30:00Z"
}
```

| tremor_class | Description |
|---|---|
| 0 | No tremor |
| 1 | Resting tremor |
| 2 | Postural tremor |
| 3 | Action tremor |

#### GET `/api/v1/tremor/history?hours=24`
Returns time-series tremor data.

### Gait

#### GET `/api/v1/gait/current`
Returns current gait metrics and FOG status.

**Response:**
```json
{
  "stride_length": 0.62,
  "cadence": 98.0,
  "freeze_index": 0.12,
  "fog_detected": false,
  "festination": false,
  "battery": 72,
  "timestamp": "2026-08-17T10:30:00Z"
}
```

### Voice

#### GET `/api/v1/voice/current`
Returns current speech quality and swallow metrics.

### Medication

#### GET `/api/v1/med/onoff`
Returns ON/OFF state and dose prediction.

**Response:**
```json
{
  "current_state": 1,
  "minutes_since_dose": 142,
  "predicted_off_in_min": 68,
  "next_dose_recommended": false
}
```

#### GET `/api/v1/med/history`
Returns dose history with ON/OFF timeline.

#### POST `/api/v1/med/dose`
Logs a manual dose (non-station).

### Risk

#### GET `/api/v1/risk/fall`
Returns 30-day fall risk forecast.

**Response:**
```json
{
  "fall_risk_score": 45.0,
  "threshold": 60,
  "recommendation": "Monitor"
}
```

#### GET `/api/v1/risk/progression`
Returns MDS-UPDRS-aligned disease progression estimate.

### Reports

#### GET `/api/v1/reports/daily`
Generates daily PD summary (PDF).

#### GET `/api/v1/reports/clinical`
Generates neurologist-ready MDS-UPDRS-aligned clinical report (PDF).

#### GET `/api/v1/reports/weekly`
Generates weekly trend report (PDF).

### Real-time

#### WS `/ws/realtime`
WebSocket stream of real-time sensor data.

### Device Management

#### GET `/api/v1/devices`
Lists registered devices.

#### POST `/api/v1/devices/pair`
Pairs a new device.

## Error Codes

| Code | Description |
|------|-------------|
| 200 | Success |
| 400 | Bad request |
| 401 | Unauthorized |
| 404 | Not found |
| 500 | Server error |