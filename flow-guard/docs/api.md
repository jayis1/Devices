# FlowGuard API Documentation

## Base URL

```
Production: https://api.flowguard.io/v1
Development: http://localhost:8000/api/v1
```

## Authentication

All API endpoints require a JWT token in the `Authorization` header:

```
Authorization: Bearer <token>
```

Tokens are obtained via `POST /auth/login` with email/password.

## Endpoints

### System Status

#### `GET /status`

Get overall system status including valve state, active alerts, and recent readings.

**Response:**
```json
{
  "valve_state": "closed",
  "active_alerts": [
    {
      "id": 42,
      "level": 2,
      "alert_type": 0,
      "source_node_id": 16,
      "message": "Leak detected under kitchen sink",
      "timestamp": "2026-06-15T10:30:00Z",
      "acknowledged": false
    }
  ],
  "recent_readings": [
    {
      "node_id": 16,
      "node_type": "appliance_monitor",
      "name": "Kitchen Sink",
      "flow_rate_ml_min": 0,
      "temperature_cx100": 2350,
      "leak_probe_1": true,
      "timestamp": "2026-06-15T10:29:55Z"
    }
  ],
  "system_healthy": false
}
```

### Water Usage

#### `GET /usage?days=7`

Get daily water usage breakdown by appliance.

**Query Parameters:**
- `days` (int, default 7): Number of days of history

**Response:**
```json
{
  "usage": [
    {
      "date": "2026-06-14",
      "total_gallons": 85.2,
      "toilet_gallons": 18.5,
      "shower_gallons": 32.0,
      "faucet_gallons": 12.3,
      "dishwasher_gallons": 8.4,
      "washing_machine_gallons": 14.0,
      "outdoor_gallons": 0.0,
      "unknown_gallons": 0.0
    }
  ]
}
```

### Sensor History

#### `GET /sensors/{node_id}?hours=24`

Get time-series data for a specific sensor node.

**Path Parameters:**
- `node_id` (int): The node ID to query

**Query Parameters:**
- `hours` (int, default 24): Hours of history to return

**Response:**
```json
{
  "readings": [
    {
      "timestamp": "2026-06-15T10:30:00Z",
      "temperature_cx100": 2345,
      "humidity_cx10": 550,
      "vibration_rms_mgx10": 15,
      "acoustic_anomaly": 10,
      "leak_state": 0,
      "flow_rate_ml_min": 0,
      "battery_mv": 2950
    }
  ]
}
```

### Alerts

#### `GET /alerts?level=2&limit=50`

Get alert history with optional filtering.

**Query Parameters:**
- `level` (int, optional): Minimum alert level (0=info, 1=warning, 2=critical, 3=emergency)
- `limit` (int, default 50): Maximum number of alerts to return

**Response:**
```json
{
  "alerts": [
    {
      "id": 42,
      "level": 2,
      "alert_type": 0,
      "source_node_id": 16,
      "message": "Leak detected under kitchen sink",
      "timestamp": "2026-06-15T10:30:00Z",
      "acknowledged": false
    }
  ]
}
```

#### `POST /alerts/{alert_id}/acknowledge`

Acknowledge an alert.

**Response:**
```json
{
  "status": "acknowledged",
  "alert_id": 42
}
```

### Valve Control

#### `POST /valve/command`

Send a command to the valve controller.

**Request Body:**
```json
{
  "command": "close",
  "auth_token": "F60D",
  "reason": "leak_detected",
  "two_factor_code": null
}
```

**Valve Open requires 2FA:**
```json
{
  "command": "open",
  "auth_token": "F60D",
  "reason": "user_manual",
  "two_factor_code": "123456"
}
```

**Response:**
```json
{
  "status": "command_sent",
  "command": "close",
  "timestamp": "2026-06-15T10:30:05Z"
}
```

### Nodes

#### `GET /nodes`

Get all registered nodes and their status.

**Response:**
```json
{
  "nodes": [
    {
      "id": 1,
      "home_id": 1,
      "node_type": "valve_controller",
      "node_id": 1,
      "name": "Main Water Valve",
      "location": "Utility closet",
      "last_seen": "2026-06-15T10:30:00Z",
      "battery_mv": 3100,
      "firmware_version": "1.0.0"
    },
    {
      "id": 2,
      "home_id": 1,
      "node_type": "pipe_sensor",
      "node_id": 16,
      "name": "Kitchen Pipe",
      "location": "Under kitchen sink",
      "last_seen": "2026-06-15T10:29:55Z",
      "battery_mv": 2950,
      "firmware_version": "1.0.0"
    }
  ]
}
```

## WebSocket

#### `WS /ws`

Real-time streaming of sensor data, alerts, and valve status changes.

**Message Format (server → client):**
```json
{
  "type": "sensor_data",
  "node_type": "pipe_sensor",
  "node_id": 16,
  "data": {
    "temperature_cx100": 2345,
    "humidity_cx10": 550,
    "vibration_rms_mgx10": 15,
    "acoustic_anomaly": 10,
    "leak_state": 0,
    "battery_mv": 2950
  },
  "timestamp": "2026-06-15T10:30:00Z"
}
```

**Alert messages:**
```json
{
  "type": "alert",
  "data": {
    "level": 3,
    "alert_type": 0,
    "source_node_id": 16,
    "message": "EMERGENCY: Burst pipe detected!"
  },
  "timestamp": "2026-06-15T10:30:00Z"
}
```

## Error Responses

All errors follow RFC 7807 format:

```json
{
  "detail": "2FA code required for valve OPEN command",
  "status": 403,
  "title": "Forbidden",
  "type": "https://flowguard.io/errors/2fa-required"
}
```

| Status Code | Meaning |
|------------|---------|
| 400 | Bad request — invalid parameters |
| 401 | Unauthorized — missing or invalid JWT |
| 403 | Forbidden — 2FA required, insufficient permissions |
| 404 | Not found — node or alert doesn't exist |
| 500 | Internal server error |