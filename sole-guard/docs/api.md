# SoleGuard API Reference

Base URL: `http://<host>:8000/api/v1`

## Endpoints

### GET /health
Health check. → `{"status": "ok"}`

### POST /ingest/pressure
Ingest a pressure/temperature payload from an insole (REST equivalent of MQTT).
```json
{
  "patient_id": 1,
  "node_id": 1,
  "pressure": [24 ints 0-255],
  "temp_centic": [8 ints, centi-degC],
  "pti_centic": 35000,
  "flags": 0
}
```

### GET /patient/{pid}/risk
Current ulcer-risk score + 7-day trend.
```json
{
  "risk_l": 25,
  "risk_r": 72,
  "trend": [{"ts": "...", "risk_l": 20, "risk_r": 60}, ...]
}
```

### GET /patient/{pid}/heatmap
Latest pressure + temperature heat map data.
```json
{
  "ts": "2026-06-19T08:00:00Z",
  "pressure_l": [24 ints],
  "pressure_r": [24 ints],
  "temp_l": [8 ints],
  "temp_r": [8 ints],
  "pti_l": 35000,
  "pti_r": 28000
}
```

### GET /patient/{pid}/gait
30-day gait feature history (cadence, stride, symmetry, double-support, shuffling, steps).

### GET /patient/{pid}/scans
Foot-scan image history + wound classifications.
```json
[{
  "id": 42, "ts": "...", "foot": "L",
  "wound_class": "ulcer", "confidence": 88,
  "weight_kg": 82.3, "image_key": "scans/p1/42.jpg"
}]
```

### GET /patient/{pid}/alerts
Alert history with type, severity, message, acknowledged flag.

### POST /clinician/report/{pid}
Generate a structured wound + risk report for the clinician portal. Returns combined risk, heatmap, scans, and alerts.

### WebSocket /ws/alerts/{patient_id}
Real-time alert stream. Server pushes JSON alert objects when the alert engine fires.

## MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `soleguard/{patient_id}/pressure` | insole→cloud | PressureTemp JSON |
| `soleguard/{patient_id}/gait` | insole/ankle→cloud | Gait JSON |
| `soleguard/{patient_id}/edema` | ankle→cloud | Edema JSON |
| `soleguard/{patient_id}/scan` | scanner→cloud | Scan result JSON |
| `soleguard/{patient_id}/risk` | hub→cloud | Risk score JSON |