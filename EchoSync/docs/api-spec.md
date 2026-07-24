# EchoSync — API Specification

Base URL: `http://localhost:8000/api/v1`

## Authentication

All endpoints require JWT token in `Authorization: Bearer <token>` header.

### Login
```
POST /auth/login
Body: { "username": "string", "password": "string" }
Response: { "token": "string", "user": { "id": 1, "name": "string" } }
```

## Devices

### List Devices
```
GET /devices
Response: [
  { "id": 1, "type": "room-sentinel", "online": true },
  { "id": 4, "type": "wrist-band", "battery_v": 3.7, "worn": true, "sleeping": false }
]
```

## Sound Events

### Get Latest Events
```
GET /sound-events?limit=50&priority=2
Response: [
  {
    "node_id": 1,
    "sound_class": 0,
    "sound_name": "SmokeAlarm",
    "confidence": 95,
    "direction": 180.0,
    "elevation": 0,
    "duration_ms": 2000,
    "temp_c": 22.5,
    "humidity_pct": 45.0,
    "db_spl": 75,
    "priority": 2,
    "event_id": 42,
    "timestamp": "2026-07-24T12:00:00Z"
  }
]
```

### Get Sound History
```
GET /sound-events/history?days=7
Response: {
  "2026-07-24": { "total": 15, "emergency": 1, "important": 3, "info": 11, "classes": {...} }
}
```

## Room Sentinel

### Get Latest Data Per Sentinel
```
GET /room-sentinel
Response: {
  "1": { ...latest event... }
}
```

## Wrist Band

### Get Status
```
GET /wrist-band
Response: {
  "4": { "battery_v": 3.7, "worn": true, "sleeping": false, "alerts_24h": 12 }
}
```

## Door Tag

### Get Events
```
GET /door-tag?limit=50
Response: [
  { "node_id": 5, "event_type": 0, "confidence": 95, "knock_count": 3, "event_id": 7 }
]
```

## Alerts

### Get Alert History
```
GET /alerts
Response: [
  { "type": "emergency", "sound": "SmokeAlarm", "timestamp": "...", "node": 1 }
]
```

## Awareness Score

### Get Sound Awareness Score
```
GET /sound-awareness-score
Response: { "score": 75.0, "classes_detected": 15, "classes_total": 20, "events_today": 12 }
```

## Daily Sound Log

### Get Daily Log
```
GET /daily-sound-log?date=2026-07-24
Response: {
  "date": "2026-07-24",
  "total_events": 15,
  "emergency_count": 1,
  "important_count": 3,
  "info_count": 11,
  "most_common": "DoorKnock",
  "timeline": [...]
}
```

## Weekly Report

### Get Weekly Report
```
GET /weekly-report
Response: {
  "total_events": 85,
  "emergency_events": 2,
  "important_events": 15,
  "class_distribution": { "DoorKnock": 20, "Water": 15, ... },
  "hourly_pattern": { 0: 2, 1: 0, ..., 23: 5 },
  "most_active_hour": 9
}
```

## Custom Sounds

### List Custom Sounds
```
GET /custom-sounds
Response: [{ "node_id": 1, "name": "Front doorbell", "priority": 1, "status": "active" }]
```

### Enroll Custom Sound
```
POST /custom-sounds/enroll
Body: { "node_id": 1, "sound_name": "Front doorbell", "priority": 1, "enrollment_samples": 5 }
Response: { "status": "enrollment_started", "node_id": 1 }
```

## ML Predictions

### Priority Prediction
```
GET /ml/predict/priority?sound_class=0&confidence=95&hour=14
Response: { "priority": 2, "confidence": 95 }
```

### Class Prediction
```
GET /ml/predict/class
Response: { "sound_class": 5, "sound_name": "DoorKnock", "confidence": 88, "direction": 90.0, "priority": 1 }
```

## Reports

### Accessibility Report
```
GET /reports/accessibility
Response: {
  "report_type": "Accessibility Sound Awareness Report",
  "generated": "2026-07-24T12:00:00Z",
  "summary": { "total_events_7d": 85, "emergency_events_7d": 2, ... },
  "events_by_class": { "DoorKnock": 20, ... },
  "recommendations": ["Good coverage", "Emergency sound detection active"]
}
```

## Haptic Configuration

### Set Haptic Config
```
POST /haptic/config
Body: { "intensity": 100, "emergency_pattern": 73, "important_pattern": 47, "info_pattern": 12 }
Response: { "status": "updated" }
```

## Display Configuration

### Set Display Config
```
POST /display/config
Body: { "brightness": 80, "auto_off_seconds": 30 }
Response: { "status": "updated" }
```

## WebSocket

### Real-time Events
```
WS /ws
→ Send "ping" to keep alive
← Receive sound event JSON objects in real-time
```