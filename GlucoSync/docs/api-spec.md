# GlucoSync — API Specification

## Base URL

```
http://localhost:8000/api
```

## Authentication

JWT Bearer token. Obtain via `/register` or `/login` (future).

```
Authorization: Bearer <token>
```

## Endpoints

### Health
- `GET /health` — service health check

### Users
- `POST /register` — register new user
  - Body: `{ email, password, diabetes_type, weight_kg, target_glucose, hypo_threshold, hyper_threshold }`
  - Returns: `{ user_id, status }`

### Glucose
- `POST /glucose` — ingest glucose reading
  - Body: `{ user_id, glucose_mgdl, trend_mgdl_min, sensor_state, confidence, timestamp }`
- `GET /glucose/{user_id}?hours=24` — get glucose history
  - Returns: `[{ id, user_id, glucose_mgdl, trend, created_at, ... }]`

### Meals
- `POST /meals` — ingest meal scan
  - Body: `{ user_id, food_class_id, food_confidence, carb_grams, portion_grams, glycemic_index, spectral_bands, timestamp }`
- `GET /meals/{user_id}?hours=24` — get meal history

### Insulin
- `POST /insulin` — ingest insulin event
  - Body: `{ user_id, pen_type, pen_id, estimated_units, confidence, injection_dur_ms, timestamp }`
- `GET /insulin/{user_id}?hours=24` — get insulin history

### Activity
- `POST /activity` — ingest activity data
  - Body: `{ user_id, hr, hrv_rmssd, activity_class, intensity, confidence, timestamp }`

### Analytics
- `GET /analytics/tir/{user_id}?days=14` — time-in-range
  - Returns: `{ tir_pct, below_pct, above_pct, avg_glucose, gmi, readings, days }`
- `GET /analytics/agp/{user_id}?days=14` — ambulatory glucose profile
  - Returns: `{ agp_data: [{ time_bucket, median, p10, p25, p75, p90, count }], days, total_readings }`
- `GET /analytics/sensitivity/{user_id}` — insulin sensitivity
  - Returns: `{ ic_ratio, isf, tdd_avg, method, note }`

### Emergency Contacts
- `POST /contacts/{user_id}` — add emergency contact
  - Body: `{ name, phone, relationship }`
- `GET /contacts/{user_id}` — list contacts

### WebSocket
- `WS /ws/{user_id}` — real-time glucose updates (1/min)

## Error Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 201 | Created |
| 400 | Bad request |
| 401 | Unauthorized |
| 404 | Not found |
| 500 | Internal error |