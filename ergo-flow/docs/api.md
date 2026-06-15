# ErgoFlow — API Specification

## Base URL

- Development: `http://localhost:8000`
- Production: `https://ergoflow.local:8000`

## Authentication

All endpoints require JWT Bearer token in `Authorization` header.

```
Authorization: Bearer <token>
```

Tokens obtained via BLE pairing with hub node.

## Endpoints

### System

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | Health check |
| GET | `/api/v1/status` | System status (nodes, batteries) |

### Posture

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/posture/current` | Current posture score & class |
| GET | `/api/v1/posture/history?minutes=60` | Posture time series |
| GET | `/api/v1/posture/history?hours=24` | 24-hour posture history |
| GET | `/api/v1/posture/history?days=7` | 7-day posture summary |

### RSI Risk

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/rsi-risk` | Current RSI risk assessment |
| GET | `/api/v1/rsi-risk/trends?days=7` | RSI risk trend over time |

### Activity

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/activity/current` | Current detected activity |
| GET | `/api/v1/activity/stats?period=today` | Activity breakdown |
| GET | `/api/v1/activity/stats?period=week` | Weekly activity summary |

### Focus

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/focus/current` | Current focus level |
| GET | `/api/v1/focus/sessions` | Focus session history |

### Breaks

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/breaks` | Break history & compliance |
| POST | `/api/v1/breaks/dismiss` | Dismiss current break |
| POST | `/api/v1/breaks/complete` | Mark break as completed |

### Desk Control

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/v1/desk/height` | Set desk height (mm) |
| POST | `/api/v1/desk/preset` | Set desk to preset |
| POST | `/api/v1/desk/stop` | Emergency stop |
| GET | `/api/v1/desk/status` | Current desk status |

### Lighting

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/v1/lighting` | Set RGBW + brightness + mode |
| GET | `/api/v1/lighting` | Current lighting state |

### Monitor Tilt

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/v1/monitor/tilt` | Set monitor tilt (-15 to +15°) |
| GET | `/api/v1/monitor/tilt` | Current monitor tilt |

### Environment

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/environment` | Current lux, temp, humidity |

### Analytics

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/analytics/weekly` | Weekly health report |
| GET | `/api/v1/analytics/monthly` | Monthly trends |

### Management

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/nodes` | List all mesh nodes |
| POST | `/api/v1/calibrate` | Start calibration |
| POST | `/api/v1/ota/start` | Initiate OTA update |
| POST | `/api/v1/factory-reset` | Factory reset a node |

### WebSocket

| Endpoint | Description |
|----------|-------------|
| `ws://host:8000/ws/v1/realtime` | Real-time posture/activity stream |

**WebSocket messages (server → client):**
```json
{"type": "posture", "data": {"timestamp": "...", "score": 85, "posture_class": "good", "risk_level": 0, "duration_seconds": 300}}
{"type": "activity", "data": {"timestamp": "...", "activity": "typing", "confidence": 0.82}}
{"type": "break", "data": {"type": "stretch", "duration_seconds": 300}}
{"type": "desk", "data": {"height_mm": 750, "motor_state": "idle"}}
```

## Error Responses

All errors follow this format:
```json
{
  "detail": "Error message describing what went wrong",
  "status_code": 400
}
```

Common error codes:
- `400` — Invalid request parameters
- `401` — Missing or invalid authentication token
- `404` — Resource not found
- `429` — Rate limit exceeded
- `500` — Internal server error