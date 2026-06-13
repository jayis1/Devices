# HearthKeep API Specification

## Base URL

```
Production: https://api.hearethkeep.com/v1
Self-hosted: http://localhost:8000/api
```

## Authentication

All API endpoints require a Bearer token:

```
Authorization: Bearer <token>
```

Tokens are obtained via the `/api/auth/login` endpoint.

## Endpoints

### Authentication

#### POST /api/auth/login
Login with email and password. Returns JWT token.

**Request:**
```json
{
  "email": "caregiver@example.com",
  "password": "password123"
}
```

**Response:**
```json
{
  "access_token": "eyJ...",
  "token_type": "bearer",
  "expires_in": 3600,
  "home_id": 1
}
```

### Homes

#### POST /api/homes
Create a new home.

**Request:**
```json
{
  "name": "Mom's House",
  "hub_id": "HK-HUB-001",
  "address": "123 Main St, Anytown, USA",
  "timezone": "America/New_York",
  "emergency_contacts": [
    {
      "name": "Sarah Johnson",
      "phone": "+1-555-0101",
      "relationship": "daughter",
      "priority": 1
    }
  ],
  "alert_preferences": {
    "fall_detection": true,
    "routine_change": true,
    "health_trend": true,
    "low_battery": true,
    "escalation_delay_seconds": 30
  }
}
```

#### GET /api/homes/{home_id}
Get home details and current status.

**Response:**
```json
{
  "id": 1,
  "name": "Mom's House",
  "hub_id": "HK-HUB-001",
  "status": "normal",
  "nodes": [
    {
      "node_id": "01",
      "type": "room_monitor",
      "room_name": "Living Room",
      "online": true,
      "battery_pct": 100,
      "last_seen": "2024-06-13T10:30:00Z"
    }
  ],
  "emergency_contacts": [...],
  "created_at": "2024-01-01T00:00:00Z"
}
```

### Nodes

#### POST /api/nodes
Register a new node (room monitor, bed mat, or wearable tag).

**Request:**
```json
{
  "home_id": 1,
  "node_id": "01",
  "node_type": "room_monitor",
  "room_name": "Living Room"
}
```

#### GET /api/homes/{home_id}/nodes
List all nodes in a home.

#### PUT /api/nodes/{node_id}
Update node configuration (room name, sensitivity, etc.).

### Sensor Data

#### POST /api/sensors/radar/{node_id}
Store radar data from a room monitor.

**Request:**
```json
{
  "room": "01",
  "presence_count": 1,
  "position_class": "sitting",
  "fall_probability": 0.02,
  "movement_index": 0.35,
  "distance_m": 1.8,
  "velocity_ms": 0.1,
  "temp": 22.5,
  "humidity": 45.0,
  "iaq": 85.0,
  "lux": 320.0
}
```

#### POST /api/sensors/vitals/{node_id}
Store bed vitals data.

**Request:**
```json
{
  "heart_rate_bpm": 68.0,
  "breathing_rate": 14.0,
  "movement_index": 0.05,
  "in_bed": true,
  "sleep_phase": "deep",
  "hr_confidence": 0.92,
  "br_confidence": 0.88,
  "temp": 29.2
}
```

#### GET /api/sensors/radar/{node_id}/history
Get historical radar data.

**Query Parameters:**
- `hours` (int, default 24): Number of hours of history
- `resolution` (string, default "1min"): Data resolution ("1s", "1min", "5min", "1h")

#### GET /api/sensors/vitals/{node_id}/history
Get historical vitals data.

### Alerts

#### GET /api/alerts
List alerts for a home.

**Query Parameters:**
- `home_id` (int, required): Home ID
- `severity` (string, optional): Filter by severity ("info", "warning", "urgent", "emergency")
- `acknowledged` (bool, optional): Filter by acknowledgment status
- `limit` (int, default 50): Maximum results

**Response:**
```json
[
  {
    "id": 1,
    "home_id": 1,
    "node_id": "02",
    "alert_type": "fall",
    "severity": "emergency",
    "message": "Fall detected in Bathroom (probability: 97%, position: fallen)",
    "acknowledged": false,
    "resolved": false,
    "created_at": "2024-06-13T10:15:30Z",
    "metadata": {
      "fall_probability": 0.97,
      "position_class": "fallen",
      "impact_velocity": 2.1
    }
  }
]
```

#### PUT /api/alerts/{alert_id}/acknowledge
Acknowledge an alert.

#### PUT /api/alerts/{alert_id}/resolve
Resolve an alert.

### Wellness

#### GET /api/wellness/{home_id}
Get current wellness summary.

**Response:**
```json
{
  "home_id": 1,
  "timestamp": "2024-06-13T10:30:00Z",
  "overall_status": "normal",
  "rooms": [
    {
      "node_id": "01",
      "room_name": "Living Room",
      "online": true,
      "battery_pct": 100,
      "presence": {
        "occupied": true,
        "position_class": "sitting",
        "fall_probability": 0.02,
        "movement_index": 0.35
      }
    }
  ],
  "bed": {
    "node_id": "0A",
    "online": true,
    "battery_pct": 78,
    "vitals": {
      "heart_rate_bpm": 68.0,
      "breathing_rate": 14.0,
      "in_bed": true,
      "sleep_phase": "deep",
      "hr_confidence": 0.92
    }
  },
  "alerts": 0
}
```

#### GET /api/wellness/{home_id}/trends
Get wellness trends over time.

**Query Parameters:**
- `period` (string, default "7d"): Period ("1d", "7d", "30d", "90d")
- `metrics` (string[], optional): Specific metrics to include

### Activity

#### GET /api/activity/{home_id}/timeline
Get daily activity timeline.

**Query Parameters:**
- `date` (string, optional): Date in ISO format (default: today)
- `resolution` (string, default "1h"): Timeline resolution ("5min", "15min", "1h")

**Response:**
```json
[
  {
    "timestamp": "2024-06-13T07:00:00Z",
    "activity": "morning_routine",
    "room": "Bathroom",
    "duration_s": 1800,
    "confidence": 0.85
  },
  {
    "timestamp": "2024-06-13T08:00:00Z",
    "activity": "cooking",
    "room": "Kitchen",
    "duration_s": 2400,
    "confidence": 0.92
  }
]
```

### OTA Updates

#### POST /api/ota/upload
Upload new firmware for a node type.

**Request:** Multipart form with firmware binary.

#### POST /api/ota/deploy
Deploy firmware update to specific nodes.

**Request:**
```json
{
  "node_type": "room_monitor",
  "version": "1.2.0",
  "node_ids": ["01", "02", "03"]
}
```

### WebSocket

#### WS /ws
Real-time event stream.

**Messages:**
```json
{
  "type": "sensor_update",
  "node_id": "01",
  "data": { "presence_count": 1, "position_class": "sitting", "fall_probability": 0.02 }
}
```

```json
{
  "type": "alert",
  "alert_id": 1,
  "severity": "emergency",
  "message": "Fall detected in Bathroom"
}
```

```json
{
  "type": "vitals_update",
  "node_id": "0A",
  "data": { "heart_rate_bpm": 68, "breathing_rate": 14, "in_bed": true }
}
```

## Error Responses

All errors follow this format:

```json
{
  "detail": "Error message",
  "status_code": 404
}
```

| Status | Meaning |
|--------|---------|
| 400 | Bad Request — invalid parameters |
| 401 | Unauthorized — missing or invalid token |
| 403 | Forbidden — not authorized for this home |
| 404 | Not Found — resource doesn't exist |
| 429 | Too Many Requests — rate limited |
| 500 | Internal Server Error |