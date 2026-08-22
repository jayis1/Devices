# WasteSort API Specification

## REST endpoints

### `GET /api/v1/health`
Returns backend and MQTT bridge health.

### `GET /api/v1/overview`
Returns household diversion score, current fill, next pickups, open alerts.

### `GET /api/v1/bins`
Returns all configured bins with latest telemetry.

### `POST /api/v1/sort/resolve`
Resolve an item classification request.

Request:
```json
{
  "barcode": "012345678905",
  "rgb_features": [0.22, 0.18, 0.11],
  "spectral": [151, 180, 98, 77, 32, 20, 19, 12],
  "municipality": "us-ca-oakland"
}
```

Response:
```json
{
  "stream": "recycle",
  "material": "pet_1_clear",
  "confidence": 0.94,
  "instructions": "Rinse lightly and replace cap before recycling."
}
```

### `POST /api/v1/command/bin/{bin_id}`
Send commands like `deodorize`, `refresh_label`, `set_stream`, `recalibrate`.

### `GET /api/v1/pickups/forecast`
Returns the next scheduled pickup plus overflow and miss risk.

### `GET /api/v1/recommendations`
Returns HabitCoach actions prioritized for the household.

## WebSocket

### `GET /ws/live`
Pushes event envelopes:
- `overview_update`
- `sort_event`
- `bin_update`
- `pickup_update`
- `alert`

## MQTT payload shapes

See `docs/protocol_spec.md` and `software/dashboard/models.py`.
