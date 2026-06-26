# TrailSync — API Specification

## Base URL

```
http://localhost:8023/api/v1
```

## Runner Telemetry

### POST /runners/{runner_id}/telemetry

Receive telemetry from a wrist unit via hub.

**Request:**
```json
{
  "runner_id": "runner_001",
  "lat": 40.01500,
  "lon": -105.27055,
  "altitude_m": 2400,
  "speed_cm_s": 267,
  "distance_m": 8500,
  "hr": 145,
  "spo2": 96,
  "hrv_rmssd_ms": 45.2,
  "skin_temp_c": 32.1,
  "pressure_hpa": 1008.5,
  "battery_pct": 85,
  "num_satellites": 8,
  "flags": 0
}
```

**Response:**
```json
{
  "status": "ok",
  "alerts": [
    {"type": "altitude_sickness", "severity": "warning", "message": "SpO2 94% — altitude sickness risk"}
  ]
}
```

### GET /runners/{runner_id}
### GET /runners
### POST /runners/{runner_id}/gait

## Injury Risk

### GET /runners/{runner_id}/injury_risk?days=7

**Response:**
```json
{
  "runner_id": "runner_001",
  "date": "2026-06-26",
  "injuries": {
    "IT band syndrome": 15,
    "plantar fasciitis": 10,
    "Achilles tendinopathy": 8,
    "stress fracture": 5,
    "shin splints": 12,
    "runner's knee": 20,
    "ankle sprain": 7,
    "hamstring strain": 3,
    "hip flexor strain": 5,
    "calf strain": 4,
    "IT band friction": 8,
    "patellar tendinopathy": 6
  },
  "training_load": 42.0,
  "acute_chronic_ratio": 1.1,
  "recommendations": [
    "Your acute:chronic workload ratio is 1.1 — within safe range",
    "Left/right gait asymmetry trending up — monitor over next 3 runs"
  ]
}
```

## Beacon Conditions

### POST /beacons/{beacon_id}/conditions
### GET /beacons
### GET /beacons/{beacon_id}
### PUT /beacons/{beacon_id}/trail_conditions

## SOS

### POST /sos

**Request:**
```json
{
  "runner_id": "runner_001",
  "sos_type": "fall_auto",
  "severity": "serious",
  "lat": 40.01500,
  "lon": -105.27055,
  "altitude_m": 2800,
  "hr": 110,
  "spo2": 92,
  "hrv_rmssd_ms": 25.0,
  "injury_class": "ankle_sprain",
  "num_people": 1
}
```

**Response:**
```json
{
  "status": "ok",
  "sos_id": 42
}
```

### GET /sos
### PUT /sos/{sos_id}/ack

## Training Sessions

### POST /runners/{runner_id}/sessions
### GET /runners/{runner_id}/sessions?limit=20

## WebSocket

### WS /ws/v1/live

Real-time telemetry, SOS alerts, and group tracking.

**Events:**
- `telemetry` — runner position/vitals update
- `beacon` — trail beacon conditions update
- `sos` — emergency SOS alert
- `injury_alert` — injury risk threshold crossed
- `storm_alert` — storm prediction warning