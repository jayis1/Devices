# PestSync — API Specification

## Base URL

```
Production: https://api.pestsync.com
Local:     http://localhost:8000
```

## Authentication

All endpoints (except `/api/auth/*`) require a Bearer JWT token:
```
Authorization: Bearer <token>
```

## Endpoints

### Auth

#### POST /api/auth/login
```json
// Request
{ "email": "user@example.com", "password": "secret" }

// Response 200
{ "access_token": "eyJ...", "token_type": "bearer", "expires_in": 604800 }
```

#### POST /api/auth/register
```json
// Request
{ "email": "user@example.com", "password": "secret" }

// Response 201
{ "id": 1, "email": "user@example.com", "display_name": "PestSync User" }
```

### Devices

#### GET /api/devices/
List all registered devices.

#### POST /api/devices/
Register a new device.
```json
{ "id": "0x0010", "name": "Kitchen Sentinel", "node_type": "sentinel", "firmware_version": "1.0.0" }
```

#### GET /api/devices/{device_id}
Get device details.

#### DELETE /api/devices/{device_id}
Remove a device.

### Detections

#### GET /api/detections/
List detection events with filters.
- Query params: `device_id`, `pest_class`, `start`, `end`, `limit`

#### POST /api/detections/
Create a detection event.

#### GET /api/detections/heatmap
Get 24-hour activity heatmap.
- Query params: `device_id`

#### GET /api/detections/stats
Get detection statistics.
- Query params: `days` (default 30)

### Traps

#### GET /api/traps/
List all traps.

#### GET /api/traps/{device_id}
Get trap status.

#### POST /api/traps/{device_id}/reset
Send reset/rearm command.

#### GET /api/traps/{device_id}/history
Get catch history.

### Deterrents

#### GET /api/deterrents/
List all deterrent nodes.

#### GET /api/deterrents/{device_id}
Get deterrent status.

#### POST /api/deterrents/{device_id}/command
Send mode/band/duration command.
```json
{ "mode": "adaptive", "band": "both", "duration_s": 300 }
```

#### POST /api/deterrents/{device_id}/strobe
Trigger immediate strobe burst.

#### POST /api/deterrents/{device_id}/diffuse
Trigger immediate essential oil dose.

#### GET /api/deterrents/{device_id}/effectiveness
Get effectiveness metrics.
- Query params: `days` (default 7)

### Alerts

#### GET /api/alerts/
List alerts.
- Query params: `unread_only`, `severity`, `limit`

#### POST /api/alerts/{alert_id}/read
Mark alert as read.

#### POST /api/alerts/read-all
Mark all alerts as read.

### Telemetry

#### GET /api/telemetry/{device_id}
Get time-series telemetry.
- Query params: `start`, `end`, `interval` (1m, 5m, 1h, 1d)

## MQTT Topics

```
pestsync/{user_id}/{node_id}/telemetry     # QoS 1
pestsync/{user_id}/{node_id}/detection      # QoS 1
pestsync/{user_id}/{node_id}/trap           # QoS 1
pestsync/{user_id}/{node_id}/deterrent      # QoS 1
pestsync/{user_id}/{node_id}/command        # QoS 1 (downlink)
pestsync/{user_id}/alerts                  # QoS 2
pestsync/{user_id}/ml/forecast              # QoS 1
```

## Error Responses

```json
{ "detail": "Error message" }
```

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 201 | Created |
| 400 | Bad Request |
| 401 | Unauthorized |
| 404 | Not Found |
| 422 | Validation Error |
| 500 | Internal Server Error |