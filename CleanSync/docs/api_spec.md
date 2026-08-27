# CleanSync API Specification

## Base URL
- Local: `http://cleansync-hub.local:8000`
- Cloud proxy: `https://api.cleansync.example`

## REST Endpoints

### `GET /api/v1/health`
Returns backend health and current timestamp.

### `GET /api/v1/overview`
Returns:
- household cleanliness score
- per-room summary
- active alerts
- supply forecast
- recommended jobs

### `POST /api/v1/telemetry/dirt`
Ingest Dirt Sentinel telemetry.

Request body:
```json
{
  "node_id": "dirt-kitchen-1",
  "room": "kitchen",
  "dust_index": 61,
  "humidity_pct": 67.2,
  "temp_c": 24.5,
  "traffic_score": 82,
  "wet_floor_probability": 0.72,
  "odor_index": 18,
  "battery_mv": 2960
}
```

### `POST /api/v1/dock/state`
Ingest dock controller state and consumables.

### `POST /api/v1/wand/scan`
Submit a handheld scan classification event.

### `GET /api/v1/recommendations`
Returns prioritized cleaning recommendations.

### `POST /api/v1/schedule/optimize`
Input household preferences and current room state, output ranked cleaning window recommendations.

## WebSocket
### `GET /ws/live`
Server emits events:
- `dirt_update`
- `dock_update`
- `scan_update`
- `alert`

## MQTT Topics
- `cleansync/dirt/+/telemetry`
- `cleansync/dock/+/state`
- `cleansync/wand/+/scan`
- `cleansync/hub/health`
- `cleansync/alerts`
