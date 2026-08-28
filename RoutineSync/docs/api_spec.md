# RoutineSync API Specification

## Base URL
- Local edge: `http://routinesync.local:8000`
- Cloud relay: `https://api.routinesync.example`

## Endpoints

### `GET /api/v1/health`
Returns service health.

### `GET /api/v1/overview`
Returns current household snapshot.

### `GET /api/v1/routines`
Returns configured routines and compliance metrics.

### `POST /api/v1/departure/evaluate`
Evaluates a departure event.

#### Request
```json
{
  "routine": "workday",
  "missing_items": 1,
  "tray_mass_delta": 142.0,
  "door_open": true,
  "minutes_to_deadline": 11,
  "focus_fragmentation": 0.32
}
```

#### Response
```json
{
  "miss_risk": 0.74,
  "likely_missing_item": "laptop",
  "nudge": "tag_chirp",
  "detail": "Laptop last seen in office",
  "confidence": 0.83
}
```

### `POST /api/v1/find-item`
Ranks likely rooms for an item.

### `POST /api/v1/focus/evaluate`
Returns focus-state classification and suggested intervention.

### `GET /api/v1/live/ws`
WebSocket feed for real-time dashboard and mobile app.
