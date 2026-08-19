# CycleGuard API Specification

Base URL: `https://api.cycleguard.cloud`

## Authentication
All endpoints require `Authorization: Bearer *** header.

## Endpoints

### Ride

#### GET `/api/v1/ride/current`
Returns current ride metrics.

**Response:**
```json
{
  "speed_kmh": 22.5,
  "cadence_rpm": 85,
  "tire_pressure": 95.0,
  "gps_lat": 34.0522,
  "gps_lon": -118.2437,
  "heading": 180.0,
  "battery": 85,
  "timestamp": "2026-08-19T10:30:00Z"
}
```

#### GET `/api/v1/ride/history?hours=24`
Returns time-series ride data.

### Helmet

#### GET `/api/v1/helmet/current`
Returns current helmet crash detection status.

**Response:**
```json
{
  "crash_class": 0,
  "impact_g": 0.15,
  "rot_velocity": 25.0,
  "horn_detected": false,
  "siren_detected": false,
  "battery": 72
}
```

| crash_class | Description |
|---|---|
| 0 | Normal riding |
| 1 | Pothole/curb |
| 2 | Near-miss |
| 3 | Crash |

### Lock

#### GET `/api/v1/lock/status`
Returns smart lock status and GPS location.

**Response:**
```json
{
  "lock_state": 1,
  "gps_lat": 34.0522,
  "gps_lon": -118.2437,
  "tamper_count": 0,
  "load_cell_kg": 0,
  "battery": 95
}
```

| lock_state | Description |
|---|---|
| 0 | Disarmed |
| 1 | Armed |
| 2 | Tamper |
| 3 | Alarm |
| 4 | Tracking |

#### POST `/api/v1/lock/arm`
Arm the smart lock.

#### POST `/api/v1/lock/disarm`
Disarm the smart lock.

### Theft

#### GET `/api/v1/theft/alerts`
Returns theft alert history.

#### GET `/api/v1/theft/trail`
Returns GPS trail of stolen bike (recent lock_data during ALARM/TRACKING).

### Safety

#### POST `/api/v1/safety/route`
Get safe route recommendation.

**Request:**
```json
{
  "start_lat": 34.0522,
  "start_lon": -118.2437,
  "end_lat": 34.0700,
  "end_lon": -118.2500
}
```

**Response:**
```json
{
  "segments": [...],
  "avg_safety_score": 85,
  "total_distance_km": 5.2,
  "estimated_time_min": 18
}
```

#### GET `/api/v1/safety/forecast`
Returns 48-hour crash risk forecast.

### Crash

#### GET `/api/v1/crash/reports`
Returns crash incident reports (for insurance/EMT).

### Reports

#### GET `/api/v1/reports/ride/{ride_id}`
Returns detailed ride report (distance, speed, safety score, calories).

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