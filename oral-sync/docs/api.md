# OralSync — Cloud API

Base URL: `https://api.oralsync.cloud/v1` · Auth: `Authorization: Bearer <jwt>` (per-home) · All responses JSON.

## Auth

### POST /auth/pair
Pair a Hub with a home account. Body: `{ "hub_id": "...", "pair_code": "6-digit" }` → `{ "jwt": "...", "home_id": "..." }`

### POST /auth/refresh → `{ "jwt": "..." }`

## Homes & Users

### GET /homes/{home_id}/users → `[{ "user_id": 1, "name": "Ada", "age": 34, "orthodontic": false }]`
### POST /homes/{home_id}/users → create a household member
### PATCH /users/{user_id} → update profile (orthodontic mode, brushing goal)

## Brushing Sessions

### GET /users/{user_id}/sessions?since=ISO&limit=100
→ `[{ "session_id": 123, "start": "2026-06-26T07:01Z", "duration_s": 122, "technique": "Bass", "coverage": 0.78, "overpressure_events": 2, "missed_surfaces": ["UR-lingual"] }]`

### GET /sessions/{session_id}
→ full session incl. per-sextant coverage map, pressure timeline URI, technique histogram

### POST /sessions (Hub uplink)
Body: `{ "user_id": 1, "device_id": "...", "start": ISO, "duration_s": 122, "coverage_bitmap": "3f3f3f...", "technique": "Bass", "imu_uri": "s3://..." }`
→ `{ "session_id": 124 }`

## Scans

### GET /users/{user_id}/scans?limit=50
→ `[{ "scan_id": 55, "ts": "...", "plaque_pct": 18.2, "lesions": 0, "image_uri": "...", "heatmap_uri": "..." }]`

### POST /scans (Hub uplink)
Body (multipart): `metadata` JSON + `image` file (405 nm), `nir` file, `heatmap` PNG
→ `{ "scan_id": 56, "plaque_pct": 17.4, "gingivitis": { "UR": "mild", ... }, "lesions": [...] }`

### GET /scans/{scan_id}/lesions → per-tooth lesion list with first-seen & change
### GET /scans/{scan_id}/image?band=405 → raw band image

## Saliva

### GET /users/{user_id}/saliva?since=ISO&limit=200
→ `[{ "ts": "...", "ph": 6.8, "nitrite_um": 42, "buffer": 3, "temp_c": 36.4 }]`
### POST /saliva (Hub uplink) → `{ "reading_id": 9001 }`

## Risk

### GET /users/{user_id}/risk?horizon_days=90
→ `[{ "tooth_fdi": 16, "surface": "occlusal", "risk_0_100": 22, "trend": "down", "forecast_delta": -3, "factors": { "plaque": 0.4, "ph": 0.3, "coverage": 0.2, "lesion_history": 0.1 } }]`

### GET /users/{user_id}/risk/timeline?tooth_fdi=16 → weekly risk history

## Reports

### POST /users/{user_id}/report → generates dentist-ready PDF; returns `{ "report_uri": "..." }`
PDF includes: plaque heatmap per sextant, lesion change tracking, pH/nitrite trends, caries risk per tooth, recommended actions.

## WebSocket

`wss://api.oralsync.cloud/v1/stream?jwt=...` — live events:

```
{ "type": "session_start", "user_id": 1, "ts": "..." }
{ "type": "coach_cue", "user_id": 1, "cue": "MOVE_TO_UPPER_LEFT" }
{ "type": "session_end", "user_id": 1, "coverage": 0.78, "duration_s": 122 }
{ "type": "scan_complete", "scan_id": 56, "plaque_pct": 17.4 }
{ "type": "saliva_reading", "user_id": 1, "ph": 6.8 }
{ "type": "risk_update", "user_id": 1, "tooth_fdi": 16, "risk": 22 }
{ "type": "alert", "severity": "warn", "msg": "New white-spot lesion detected on 16-occlusal" }
```

## Consumables

### GET /users/{user_id}/consumables → `{ "ph_tip_uses_left": 18, "nitrite_tip_uses_left": 12 }`
### POST /consumables/reorder → triggers tip reorder (placeholder webhook)

## MQTT Topics (Hub uplink)

```
oralsync/<home_id>/<node_id>/telemetry   — periodic rollups (QoS1)
oralsync/<home_id>/<node_id>/event       — session/scan/saliva events (QoS1)
oralsync/<home_id>/<node_id>/ota/status  — OTA progress
oralsync/<home_id>/cmd                   — cloud→hub commands (QoS1, retained=false)
```

## Error Format

```json
{ "error": "invalid_token", "message": "JWT expired", "code": 401 }
```

## Rate Limits

- 600 req/min per home, 60 req/min per /report
- WebSocket: 1 connection per home
- MQTT: 1000 msgs/day per node (sessions are bundled)