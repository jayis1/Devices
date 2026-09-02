# SickDaySync API

## GET /health
Returns service liveness.

## POST /telemetry
Accepts normalized node telemetry.

Example payload:
```json
{
  "patient_id": "adult-1",
  "node": "recovery-band",
  "room_id": "guest-room",
  "fever_c": 38.1,
  "spo2": 96.0,
  "resting_hr": 98.0,
  "coughs_per_hour": 24,
  "co2_ppm": 1100,
  "humidity_pct": 41.0,
  "hydration_ml": 480
}
```

## GET /risk/summary?patient_id=<id>
Returns combined spread, hydration, and fever risk.

## GET /patients/{patient_id}/timeline
Returns stored telemetry events.

## GET /recommendations?patient_id=<id>
Returns plain-language intervention suggestions.
