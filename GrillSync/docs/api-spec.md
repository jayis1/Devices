# GrillSync — API Specification

## Base URL

```
https://api.grillsync.local/api/v1
```

## Authentication

Bearer token (JWT). Token obtained via `/api/v1/auth/login`.

## Endpoints

### Cook Sessions

#### POST /cook-sessions
Start a new cook session.

**Request:**
```json
{
  "meat_type": 0,
  "doneness_target": 3,
  "probes": [0, 1],
  "grill_config": {
    "target_grill_temp_c": 230,
    "grill_type": "gas"
  }
}
```

**Response (201):**
```json
{
  "id": "cook_2026_001",
  "start_time": "2026-07-27T18:00:00Z",
  "meat_type": 0,
  "doneness_target": 3,
  "probes": [0, 1],
  "status": "active"
}
```

#### GET /cook-sessions/{id}
Get cook session details.

**Response (200):**
```json
{
  "id": "cook_2026_001",
  "start_time": "2026-07-27T18:00:00Z",
  "end_time": null,
  "meat_type": 0,
  "meat_name": "Beef",
  "doneness_target": 3,
  "doneness_name": "Medium",
  "target_temp_c": 60.0,
  "probes": [
    {
      "probe_id": 0,
      "current_temp_c": 45.2,
      "target_temp_c": 60.0,
      "doneness": 2,
      "eta_seconds": 480,
      "history": [...]
    }
  ],
  "thermal_frames": [...],
  "safety_events": [...],
  "status": "active"
}
```

#### POST /cook-sessions/{id}/end
End cook session.

**Response (200):**
```json
{
  "id": "cook_2026_001",
  "end_time": "2026-07-27T18:45:00Z",
  "duration_minutes": 45,
  "result": "success",
  "food_safety": {
    "usda_temp_reached": true,
    "final_temp_c": 61.2,
    "usda_min_c": 62.8,
    "safe": false
  }
}
```

### Nodes

#### GET /nodes
List all registered nodes.

**Response (200):**
```json
{
  "nodes": [
    {"id": 0, "type": "hub", "name": "Grill Hub", "online": true},
    {"id": 1, "type": "sentinel", "name": "Grill Sentinel", "online": true},
    {"id": 2, "type": "smoke", "name": "Smoke Node", "online": true},
    {"id": 3, "type": "probe", "name": "Meat Probe 0", "online": true, "battery_v": 4.1},
    {"id": 4, "type": "probe", "name": "Meat Probe 1", "online": false, "battery_v": 3.5}
  ]
}
```

#### GET /nodes/{id}/telemetry
Get node telemetry history.

**Query params:** `from`, `to`, `limit`, `sensor`

**Response (200):**
```json
{
  "node_id": 1,
  "telemetry": [
    {
      "timestamp": "2026-07-27T18:00:00Z",
      "surface_max_c": 285.3,
      "surface_avg_c": 180.5,
      "gas_ppm": 50,
      "flareup_risk": 15
    }
  ]
}
```

### Alerts

#### GET /alerts
List alerts, filterable by severity, node, session.

**Query params:** `severity`, `node_id`, `session_id`, `acknowledged`

**Response (200):**
```json
{
  "alerts": [
    {
      "id": "alert_001",
      "timestamp": "2026-07-27T18:15:00Z",
      "node_id": 1,
      "session_id": "cook_2026_001",
      "alert_type": "FLARE_UP_WARNING",
      "severity": "high",
      "data": {"risk": 75, "eta_ms": 8000},
      "acknowledged": false
    }
  ]
}
```

#### POST /alerts/{id}/ack
Acknowledge alert.

**Response (200):**
```json
{"id": "alert_001", "acknowledged": true}
```

### Meat Profiles

#### GET /meat-profiles
List built-in meat type profiles.

**Response (200):**
```json
{
  "profiles": [
    {
      "id": 0,
      "name": "Beef",
      "meat_type": 0,
      "usda_min_temp_c": 62.8,
      "doneness_levels": [
        {"level": 1, "name": "Rare", "temp_c": 52.0},
        {"level": 2, "name": "Medium Rare", "temp_c": 54.0},
        {"level": 3, "name": "Medium", "temp_c": 60.0},
        {"level": 4, "name": "Medium Well", "temp_c": 65.0},
        {"level": 5, "name": "Well Done", "temp_c": 71.0}
      ],
      "rest_time_minutes": 5
    }
  ]
}
```

### ML Predictions

#### POST /ml/doneness-predict
Request cloud doneness prediction (for verification of edge result).

**Request:**
```json
{
  "probe_id": 0,
  "meat_type": 0,
  "temp_history": [...],
  "target_temp_c": 60.0
}
```

**Response (200):**
```json
{
  "doneness": 2,
  "doneness_name": "Medium Rare",
  "confidence": 0.94,
  "eta_seconds": 480,
  "model_version": "doneness_v2.1"
}
```

### Reports

#### GET /reports/cook/{session_id}
Generate cook report PDF.

**Response (200):** `application/pdf`

Report includes:
- Cook session timeline
- Temperature curves (all probes)
- Thermal frame history
- Safety events log
- Food safety validation (USDA compliance)
- Doneness results
- Recommendations

### Thermal Frames

#### GET /thermal-frames/{session_id}
Get thermal frame history for a cook session.

**Query params:** `from`, `to`, `limit`

**Response (200):**
```json
{
  "session_id": "cook_2026_001",
  "frames": [
    {
      "timestamp": "2026-07-27T18:00:00Z",
      "max_temp_c": 285.3,
      "avg_temp_c": 180.5,
      "hot_zones": 3,
      "frame_data": "base64..."
    }
  ]
}
```

### WebSocket

#### WS /ws/realtime
Real-time updates via WebSocket.

**Messages (server → client):**
```json
{"type": "telemetry", "node_id": 1, "data": {...}}
{"type": "alert", "alert_type": "FLARE_UP_WARNING", "severity": "high", "data": {...}}
{"type": "doneness_update", "probe_id": 0, "doneness": 2, "eta_s": 480}
{"type": "thermal_frame", "max_c": 285.3, "avg_c": 180.5, "zones": 3}
{"type": "smoke_quality", "quality": "Clean Blue", "pm25": 2.5}
{"type": "cook_status", "status": "active", "probes_online": 2}
```

### Firmware / OTA

#### GET /firmware/latest
Get latest firmware version for a node type.

**Query params:** `node_type`

**Response (200):**
```json
{
  "version": "2.1.0",
  "node_type": "sentinel",
  "checksum": "sha256:...",
  "size_bytes": 524288,
  "release_notes": "Improved FlareUpNet accuracy"
}
```

#### POST /firmware/ota
Trigger OTA update for specific nodes.

**Request:**
```json
{"node_ids": [1, 2], "version": "2.1.0"}
```

### Safety Events

#### GET /safety-events
Get safety event log.

**Query params:** `session_id`, `from`, `to`, `severity`

**Response (200):**
```json
{
  "events": [
    {
      "id": "safety_001",
      "timestamp": "2026-07-27T18:15:00Z",
      "session_id": "cook_2026_001",
      "event_type": "FLARE_UP_WARNING",
      "severity": "high",
      "description": "Flare-up predicted 8s ahead (risk=75%)",
      "action_taken": "Buzzer alert + LED ring flash"
    }
  ]
}
```

## Error Responses

All errors use standard HTTP status codes with JSON body:

```json
{
  "error": "not_found",
  "message": "Cook session cook_999 not found",
  "status": 404
}
```

## Rate Limiting

- 100 requests/minute per token
- WebSocket: 1 connection per token
- OTA: 1 concurrent update per node