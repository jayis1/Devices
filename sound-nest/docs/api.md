# SoundNest API Specification

## Base URL

```
http://soundnest-hub.local:8000/api/v1
```

## Authentication

All API endpoints require a Bearer token in the Authorization header:

```
Authorization: Bearer <token>
```

Tokens are obtained via `/auth/login` and refreshed via `/auth/refresh`.

---

## Sound Events

### List Events

```
GET /events?limit=100&offset=0&sound_class=0x20&room_id=1&min_confidence=50&start=2024-01-01T00:00:00Z&end=2024-01-31T23:59:59Z
```

**Response:**
```json
[
  {
    "id": "uuid",
    "hub_id": "0001",
    "node_id": "0002",
    "sound_class": 32,
    "sound_class_name": "Speech",
    "confidence": 85.0,
    "direction_deg": 15.0,
    "spl_dba": 62.3,
    "spl_dbc": 64.1,
    "spl_dbz": 65.2,
    "peak_spl": 68.5,
    "duration_ms": 2000,
    "room_id": 1,
    "occupancy": true,
    "temp_c": 22.1,
    "humidity_pct": 45,
    "timestamp": "2024-01-15T14:30:00Z"
  }
]
```

### Get Event Details

```
GET /events/{event_id}
```

### Event Statistics

```
GET /events/stats/summary
GET /events/stats/by-class
GET /events/stats/by-hour
```

---

## SPL Readings

### List SPL Readings

```
GET /spl?limit=100&offset=0&room_id=1
```

### Live SPL

```
GET /spl/live
```

**Response:**
```json
{
  "rooms": {
    "0002": { "dba": 45.2, "dbc": 47.1, "dbz": 48.3, "timestamp": "..." },
    "0003": { "dba": 38.7, "dbc": 40.2, "dbz": 41.5, "timestamp": "..." }
  }
}
```

### SPL History & Statistics

```
GET /spl/history?room_id=1&hours=24
GET /spl/stats?room_id=1
```

---

## Sound Dose

### Today's Dose

```
GET /dose/today?person_id=person1
```

**Response:**
```json
{
  "person_id": "person1",
  "daily_dose_pct": 42.5,
  "twa_dba": 72.3,
  "peak_dba": 95.1,
  "exposure_min": 240,
  "current_spl_dba": 48.2,
  "timestamp": "2024-01-15T14:30:00Z"
}
```

### Dose History & Alerts

```
GET /dose/history?person_id=person1&days=7
GET /dose/alerts
```

### Tinnitus Profile

```
POST /dose/tinnitus-profile
```

**Request:**
```json
{
  "person_id": "person1",
  "frequency_hz": 6000,
  "bandwidth": 0.5,
  "volume_pct": 40,
  "mask_type": "narrowband",
  "sleep_fade_min": 30
}
```

---

## Masking Control

### Start Masking

```
POST /masking/start
```

**Request:**
```json
{
  "room_id": 1,
  "mode": 2,
  "volume": 50,
  "stereo_balance": 50,
  "fade_in_ms": 30,
  "fade_out_ms": 50,
  "duration_min": 60,
  "adaptive": true
}
```

### Stop Masking

```
POST /masking/stop?room_id=1
```

### Update Settings

```
PUT /masking/settings
```

### Get Status

```
GET /masking/status
```

---

## Node Management

```
GET    /nodes
GET    /nodes/{node_id}
PUT    /nodes/{node_id}/config
POST   /nodes/{node_id}/ota
DELETE /nodes/{node_id}
```

---

## Configuration

```
GET /config
PUT /config
GET /config/alert-rules
PUT /config/alert-rules
GET /config/masking-profiles
```

---

## WebSocket Channels

### Real-time Events

```
ws://soundnest-hub.local:8000/ws/events
```

Messages:
```json
{"type": "event", "data": { "sound_class": 32, "confidence": 85, ... }}
```

### Real-time SPL

```
ws://soundnest-hub.local:8000/ws/spl
```

Messages (1 Hz):
```json
{"dba": 45.2, "dbc": 47.1, "dbz": 48.3, "timestamp": "..."}
```

### Real-time Dose

```
ws://soundnest-hub.local:8000/ws/dose
```

Messages (every 5s):
```json
{"dose_pct": 42.5, "twa_dba": 72.3, "peak_dba": 95.1, "timestamp": "..."}
```

### Real-time Alerts

```
ws://soundnest-hub.local:8000/ws/alerts
```