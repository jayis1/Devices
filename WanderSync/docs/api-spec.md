# WanderSync — API Specification

## Base URL

```
https://api.wandersync.io/api/v1
```

## Authentication

JWT Bearer token via `/api/v1/auth/login`.

## Endpoints

### Auth

| Method | Path | Description |
|--------|------|-------------|
| POST | `/auth/login` | Login (username + password → JWT) |

### Devices

| Method | Path | Description |
|--------|------|-------------|
| GET | `/devices` | List all devices (hub, band, doors, rooms, voice) |
| POST | `/devices/{id}/ota` | Trigger OTA firmware update |

### Wander Band

| Method | Path | Description |
|--------|------|-------------|
| GET | `/band/location` | Current GPS location (lat, lon, fix, timestamp) |
| GET | `/band/location/history?start=&end=` | GPS history (time range) |
| GET | `/band/health` | Battery, HR, HRV, activity, wander risk, steps |
| GET | `/band/geofence` | Current geofence (center, radius, night radius, schedule) |
| POST | `/band/geofence` | Update geofence configuration |

### Doors

| Method | Path | Description |
|--------|------|-------------|
| GET | `/doors` | List all Door Sentinels (id, state, lock, tamper, battery) |
| GET | `/doors/{id}` | Door detail + history |
| POST | `/doors/{id}/lock` | Lock specific door |
| POST | `/doors/{id}/unlock` | Unlock specific door |
| POST | `/doors/lock-all` | Lock all doors (emergency) |
| POST | `/doors/unlock-all` | Unlock all doors (fire safety) |

### Rooms

| Method | Path | Description |
|--------|------|-------------|
| GET | `/rooms` | List all Room Sentinels (id, presence, activity, battery) |
| GET | `/rooms/{id}` | Room detail + latest activity |
| GET | `/rooms/{id}/timeline` | 24-hour activity timeline |
| GET | `/adl/timeline` | Aggregated ADL timeline (all rooms) |

### Voice Reminders

| Method | Path | Description |
|--------|------|-------------|
| GET | `/voice/reminders` | Reminder schedule (24 hourly slots) |
| POST | `/voice/reminders` | Add/edit reminder |
| DELETE | `/voice/reminders/{id}` | Delete reminder |
| POST | `/voice/reminders/{id}/trigger` | Manually trigger reminder now |
| GET | `/voice/clips` | List voice clips in flash |
| POST | `/voice/clips` | Upload family voice clip (multipart) |

### Cognitive Health

| Method | Path | Description |
|--------|------|-------------|
| GET | `/cognitive/score` | Current cognitive decline score (0-100) + rate of change |
| GET | `/cognitive/trajectory?months=6` | Cognitive trajectory graph data |
| GET | `/cognitive/report` | Neurologist-ready PDF report |

### Anomalies

| Method | Path | Description |
|--------|------|-------------|
| GET | `/anomalies` | Behavioral anomaly history |
| GET | `/anomalies/active` | Currently active anomaly (with SHAP explanation) |

### Sleep

| Method | Path | Description |
|--------|------|-------------|
| GET | `/sleep/summary` | Daily sleep summary (quality, duration, stages) |
| GET | `/sleep/weekly` | Weekly sleep + circadian report |

### Wandering Events

| Method | Path | Description |
|--------|------|-------------|
| GET | `/wander/events` | Wandering event history |
| GET | `/wander/active` | Active wandering event (GPS, duration, route) |
| POST | `/wander/{id}/resolve` | Mark resolved (person found) |
| GET | `/wander/route` | Predicted route (if active wandering) |

### Alerts

| Method | Path | Description |
|--------|------|-------------|
| GET | `/alerts` | List alerts (wander, fall, SOS, door, anomaly, battery) |
| PUT | `/alerts/{id}/ack` | Acknowledge alert |

### Emergency

| Method | Path | Description |
|--------|------|-------------|
| POST | `/dispatch/cancel` | Cancel 911 dispatch (false alarm) |
| GET | `/dispatch/status` | 911 dispatch status |

### Caregivers

| Method | Path | Description |
|--------|------|-------------|
| GET | `/caregivers` | List shared caregivers |
| POST | `/caregivers` | Invite caregiver (email/phone) |
| DELETE | `/caregivers/{id}` | Remove caregiver |

### Risk

| Method | Path | Description |
|--------|------|-------------|
| GET | `/risk/wander` | Current wandering risk score (0-100) |
| GET | `/risk/weekly` | Weekly care summary report |

### WebSocket

| Path | Description |
|------|-------------|
| WS `/ws` | Real-time: location updates, alerts, ADL, anomalies, telemetry |

## Error Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 201 | Created |
| 400 | Bad Request |
| 401 | Unauthorized (invalid/missing JWT) |
| 403 | Forbidden (insufficient permissions) |
| 404 | Not Found |
| 500 | Internal Server Error |