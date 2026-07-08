# QuakeGuard — API Specification

Base URL: `https://api.quakeguard.io`

## Authentication

All endpoints require `Authorization: Bearer <token>` header (JWT).
Tokens obtained via `/api/auth/login` (future implementation).

## REST Endpoints

### GET /api/health
Health check.
```json
{"status": "ok", "service": "quakeguard-cloud"}
```

### GET /api/events
List seismic events.

**Query params:**
- `hub_id` (optional) — filter by hub
- `limit` (default: 50, max: 500)

**Response:**
```json
[{
  "id": 1,
  "hub_id": "QG-001234",
  "event_id": 12345,
  "timestamp": "2026-07-08T12:00:00Z",
  "severity": 3,
  "magnitude": 5.2,
  "epicenter_dist_km": 50,
  "actions_taken": 63,
  "node_count": 4
}]
```

### GET /api/events/{event_id}
Get event detail with waveforms, structural assessment, and family responses.

**Response:**
```json
{
  "event": { ... },
  "waveforms": [{ "node_addr": 16, "data": "base64..." }],
  "structural_assessment": [{
    "strain_max_micro": 150,
    "resonance_shift_hz": 2,
    "anomaly_score": 45
  }],
  "family_responses": [{
    "user_id": "user_1",
    "status": "safe"
  }]
}
```

### GET /api/nodes
List all nodes.

**Query params:** `hub_id` (optional)

**Response:**
```json
[{
  "hub_id": "QG-001234",
  "node_addr": 16,
  "node_type": "floor",
  "battery_pct": 85,
  "temperature_c": 23.5,
  "status": "online",
  "last_seen": "2026-07-08T12:00:00Z"
}]
```

### GET /api/structural
List structural health reports.

**Query params:** `hub_id`, `limit`

### POST /api/family/response
Submit family safety check-in response.

**Body:**
```json
{
  "hub_id": "QG-001234",
  "event_id": 12345,
  "user_id": "user_1",
  "status": "safe"
}
```

**Response:**
```json
{"status": "recorded"}
```

### GET /api/reports/structural
Generate civil-engineer-ready structural health report (30-day summary).

**Query params:** `hub_id`

**Response:**
```json
{
  "hub_id": "QG-001234",
  "report_period": "30 days",
  "total_events": 2,
  "structural_samples": 8640,
  "max_strain": 150,
  "max_anomaly": 45,
  "avg_resonance_shift": 0.3,
  "recommendation": "No structural anomalies detected.",
  "report_date": "2026-07-08T12:00:00Z"
}
```

## WebSocket

### WS /ws
Real-time push channel for alerts and status updates.

**Server → Client messages:**
```json
{"type": "event", "hub_id": "...", "event_id": 1, "severity": 3, "magnitude": 5.2}
{"type": "family_checkin", "hub_id": "...", "event_id": 1, "question": "Are you safe?"}
{"type": "gas_leak", "hub_id": "...", "h2_ppm": 150, "ch4_ppm": 200}
{"type": "family_response", "hub_id": "...", "user_id": "...", "status": "safe"}
```

## MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `quakeguard/{hub_id}/event` | Hub→Cloud | Event confirmation |
| `quakeguard/{hub_id}/node/{addr}/status` | Node→Cloud | Heartbeat |
| `quakeguard/{hub_id}/structural/{addr}` | Tag→Cloud | Structural report |
| `quakeguard/{hub_id}/family/{user_id}` | App→Cloud | Family response |
| `quakeguard/{hub_id}/gas` | Shutoff→Cloud | Gas readings |