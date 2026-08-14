# PostureSync API Specification

## Base URL
```
https://api.postsync.io/api/v1
```

## Authentication
JWT Bearer token (production). For development, no auth required.

## Endpoints

### GET /health
Health check.

**Response:**
```json
{
  "status": "ok",
  "service": "PostureSync",
  "version": "1.0.0"
}
```

### GET /posture/current
Get current posture score and classification.

**Response:**
```json
{
  "score": 85,
  "class": 0,
  "class_name": "Neutral",
  "spine_angles": {
    "pitch": 5.2,
    "roll": 1.1,
    "yaw": 0.3
  },
  "timestamp": "2026-08-14T12:00:00Z"
}
```

### GET /posture/history?hours=24
Get historical posture readings.

**Query Parameters:**
- `hours` (int): Number of hours to look back (default: 24)

**Response:** Array of posture readings.

### GET /spine/angle
Get current spinal alignment angles (cervical, thoracic, lumbar).

### GET /emg/imbalance
Get current 8-channel EMG data and bilateral asymmetry analysis.

**Response:**
```json
{
  "emg_rms": [120, 125, 80, 85, 30, 32, 50, 48],
  "asymmetry_pct": 15,
  "channels": [
    "L Upper Trap", "R Upper Trap",
    "L Erector", "R Erector",
    "L SCM", "R SCM",
    "L Rectus Abd", "R Rectus Abd"
  ]
}
```

### GET /risk/forecast
Get 90-day spinal health risk forecast.

**Response:**
```json
{
  "risk_score": 35,
  "risk_level": "moderate",
  "forecast_days": 90,
  "factors": ["Low average posture score", "Declining posture trend"]
}
```

### GET /risk/spinal-age
Get biological spinal age estimation.

**Response:**
```json
{
  "spinal_age": 38,
  "chronological_age": 35,
  "delta": 3,
  "interpretation": "healthy"
}
```

### GET /scoliosis/screen
Get scoliosis screening result.

**Response:**
```json
{
  "risk_score": 12,
  "confidence": 0.87,
  "threshold": 10,
  "recommendation": "monitor"
}
```

### POST /correction/trigger
Manually trigger a posture correction haptic.

**Request:**
```json
{
  "haptic_pattern": 2,
  "duration_sec": 2,
  "message": "Sit up straight!"
}
```

### POST /calibration/start
Start calibration sequence for a node.

**Request:**
```json
{
  "node_id": 2,
  "calibration_type": "full"
}
```

### GET /coaching/recommendations
Get personalized ergonomic coaching recommendations.

### GET /reports/weekly
Get weekly spinal health summary report.

### GET /reports/clinical
Get clinical report for healthcare provider (HIPAA-compliant).

### GET /devices
List all registered devices.

### POST /devices/pair
Pair a new device.

**Request:**
```json
{
  "node_id": 2,
  "node_type": "spine_band",
  "firmware_version": "1.0.0",
  "battery": 100
}
```

### WebSocket /ws/realtime
Real-time data stream.

**Message types:**
- `posture_update`: Real-time posture score + angles
- `emg_update`: EMG channel data
- `chair_update`: Chair pad pressure data
- `desk_update`: Desk sentinel data
- `alert`: Posture correction alert

## Error Responses
```json
{
  "detail": "Resource not found"
}
```
HTTP status codes: 200, 400, 404, 500.