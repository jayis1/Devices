# DriveSync — API Specification

## Base URL

```
https://api.drivesync.cloud/api/v1
```

## Authentication

All endpoints (except `/auth/*`) require a JWT Bearer token:
```
Authorization: Bearer <token>
```

## Endpoints

### Auth

| Method | Path | Description |
|--------|------|-------------|
| POST | `/auth/register` | Register new user |
| POST | `/auth/login` | Login (OAuth2 form) |

### Trips

| Method | Path | Description |
|--------|------|-------------|
| GET | `/trips` | List recent trips |
| GET | `/trips/{id}` | Get trip details |
| GET | `/trips/{id}/events` | Get risk events for trip |
| GET | `/trips/{id}/timeline` | Get full risk timeline |

### Coaching

| Method | Path | Description |
|--------|------|-------------|
| GET | `/coaching/weekly` | Get weekly coaching report |

### Devices

| Method | Path | Description |
|--------|------|-------------|
| POST | `/devices/pair` | Pair a device set (hub + nodes) |

### WebSocket

| Path | Description |
|------|-------------|
| `/ws/alerts` | Real-time alert stream (critical drowsiness events) |

### Health

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Service health check |

## Trip Object

```json
{
  "id": "uuid",
  "start_time": "2024-01-15T10:00:00Z",
  "end_time": "2024-01-15T10:40:00Z",
  "duration_sec": 2400,
  "distance_km": 35.2,
  "safety_score": 85.5,
  "avg_risk": 15.2,
  "max_risk": 65.0,
  "drowsiness_events": 1,
  "distraction_events": 0
}
```

## Risk Event Object

```json
{
  "id": "uuid",
  "time": "2024-01-15T10:15:00Z",
  "event_type": "drowsiness",
  "risk_score": 72,
  "alert_level": 3,
  "source": "fusion",
  "speed_kmh": 85.0
}
```

## Coaching Report Object

```json
{
  "week_start": "2024-01-08T00:00:00Z",
  "week_end": "2024-01-15T00:00:00Z",
  "total_trips": 12,
  "total_distance_km": 285.4,
  "avg_safety_score": 78.3,
  "total_drowsiness_events": 4,
  "total_distraction_events": 7,
  "riskiest_time_of_day": "14:00",
  "recommendations": [
    "You experienced multiple drowsiness events..."
  ]
}
```

## MQTT Topics

| Topic | Direction | Content |
|-------|-----------|---------|
| `drivesync/data/fusion` | Hub → Cloud | Risk score + sub-scores (JSON, 5s interval) |
| `drivesync/alerts/critical` | Hub → Cloud | Critical drowsiness event (JSON) |
| `drivesync/status/hub` | Hub → Cloud | Hub heartbeat + status (30s) |
| `drivesync/cmd/mode` | Cloud → Hub | Mode change command |
| `drivesync/cmd/end_trip` | Cloud → Hub | End current trip |