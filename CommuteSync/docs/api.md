# CommuteSync API

## Overview

Base path: `/api/v1`

## Endpoints

### `GET /health`
Returns service status, UTC timestamp, and DB path.

### `GET /nodes`
Returns discovered node IDs grouped by node type.

### `GET /overview`
Returns the latest computed summaries:

- readiness risk
- lateness risk
- exposure score
- theft risk
- recommended actions

### `POST /telemetry/entry`
```json
{
  "node_id": "entry-1",
  "required_items": 5,
  "confirmed_items": 4,
  "bag_present": true,
  "badge_seen": false,
  "keys_seen": true,
  "departure_in_minutes": 12,
  "ts": "2026-09-01T06:45:00Z"
}
```

### `POST /telemetry/bag`
```json
{
  "node_id": "bag-1",
  "tamper_score": 0.18,
  "separation_m": 0.6,
  "motion_state": "idle",
  "battery_mv": 2920,
  "ts": "2026-09-01T06:45:15Z"
}
```

### `POST /telemetry/mobility`
```json
{
  "node_id": "mobility-1",
  "route_minutes": 33,
  "eta_delta_minutes": 7,
  "pm25_ug_m3": 24.5,
  "voc_index": 118,
  "vibration_rms": 0.82,
  "crash_flag": false,
  "ts": "2026-09-01T07:02:00Z"
}
```

### `POST /telemetry/desk`
```json
{
  "node_id": "desk-1",
  "arrival_confirmed": true,
  "items_left_behind": 0,
  "bag_present": true,
  "laptop_present": true,
  "ts": "2026-09-01T07:26:00Z"
}
```

### `POST /actions/checkin`
Records a user check-in such as `late`, `route_changed`, `bike_locked`, or `forgot_item`.

## Response contract

All telemetry ingestion endpoints return:

```json
{ "stored": true, "node_id": "..." }
```

Check-in endpoint returns:

```json
{ "recorded": true, "event": "late" }
```
