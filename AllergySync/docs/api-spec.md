# AllergySync — API Specification

Base URL: `https://api.allergysync.io/api/v1`

## Endpoints

### GET /exposure/current
Get current pollen levels and personal allergen risk.

**Response:**
```json
{
  "pm2_5": 12.3,
  "pm10": 25.7,
  "co2_ppm": 650,
  "pollen_class": 1,
  "pollen_name": "birch",
  "pollen_confidence": 78,
  "risk_level": "high",
  "timestamp": "2025-07-10T14:30:00Z"
}
```

### GET /exposure/forecast
24-hour pollen concentration forecast.

**Response:**
```json
{
  "forecast": [
    {"hour": 0, "pollen_class": 1, "concentration": 45, "confidence": 82},
    {"hour": 1, "pollen_class": 1, "concentration": 38, "confidence": 80},
    ...
  ],
  "generated_at": "2025-07-10T14:00:00Z",
  "source": "lstm"
}
```

### GET /exposure/history?hours=24
Historical exposure data.

**Query params:**
- `hours` (int, default 24): Hours of history to retrieve

**Response:**
```json
{
  "history": [
    {"timestamp": "2025-07-10T14:00:00Z", "pm2_5": 12.3, "pm10": 25.7, "pollen_class": 1, "pollen_confidence": 78},
    ...
  ],
  "count": 288
}
```

### POST /symptoms
Log a symptom entry.

**Request:**
```json
{
  "sneezing": 3,
  "itchy_eyes": 4,
  "congestion": 2,
  "runny_nose": 3,
  "headache": 1,
  "fatigue": 2,
  "notes": "after morning walk",
  "timestamp": "2025-07-10T14:30:00Z"
}
```

**Response:**
```json
{
  "status": "logged",
  "total_severity": 2.5
}
```

### GET /symptoms?days=30
Retrieve symptom journal.

**Response:**
```json
{
  "symptoms": [
    {"id": 1, "timestamp": "...", "sneezing": 3, "itchy_eyes": 4, ...},
    ...
  ],
  "count": 30
}
```

### POST /medication
Log a medication dose.

**Request:**
```json
{
  "medication": "cetirizine",
  "dose_mg": 10,
  "taken": true,
  "timestamp": "2025-07-10T08:00:00Z"
}
```

### GET /profile
Get the user's allergy profile.

**Response:**
```json
{
  "birch": 4,
  "grass": 3,
  "ragweed": 2,
  "oak": 3,
  "pine": 1,
  "mold": 2,
  "dust_mites": 3,
  "pet_dander": 1,
  "immunotherapy": false,
  "updated_at": "2025-07-01T10:00:00Z"
}
```

### PUT /profile
Update the allergy profile.

**Request:**
```json
{
  "birch": 4,
  "grass": 3,
  "ragweed": 2,
  "oak": 3,
  "pine": 1,
  "mold": 2,
  "dust_mites": 3,
  "pet_dander": 1,
  "skin_prick_results": {"birch": 8, "grass": 6},
  "immunotherapy": false
}
```

### GET /nodes
List all registered nodes.

**Response:**
```json
{
  "nodes": [
    {"node_id": 1, "node_type": "sentinel", "serial": "AS-S-001", "firmware_version": "1.0.0"},
    {"node_id": 2, "node_type": "window", "serial": "AS-W-001", "firmware_version": "1.0.0"},
    ...
  ]
}
```

### POST /nodes/pair
Pair a new node.

**Request:**
```json
{
  "node_type": "sentinel",
  "serial": "AS-S-002",
  "pubkey": "base64-encoded-ecdh-pubkey"
}
```

**Response:**
```json
{
  "status": "paired",
  "node_id": 5
}
```

### POST /nodes/{node_id}/ota?version=latest
Trigger OTA firmware update for a specific node.

### GET /insights?period=weekly
Get weekly or monthly insights.

**Response:**
```json
{
  "period": "weekly",
  "days": 7,
  "avg_symptom_severity": 2.3,
  "symptom_entries": 15,
  "medication_doses": [
    {"medication": "cetirizine", "doses": 7}
  ],
  "last_reading": "2025-07-10T14:30:00Z",
  "tip": "Your symptoms correlate most with birch pollen exposure."
}
```

### WebSocket /ws
Real-time updates from all nodes.

**Client → Server:**
```
"ping"
```

**Server → Client:**
```json
{
  "type": "pong"
}
```
```json
{
  "node_id": 1,
  "data": "hex-encoded-telemetry",
  "timestamp": "2025-07-10T14:30:00Z"
}
```

## Error Responses

All errors follow this format:
```json
{
  "detail": "Error message describing what went wrong"
}
```

| Status | Meaning |
|--------|---------|
| 200 | Success |
| 400 | Bad request (invalid parameters) |
| 404 | Resource not found |
| 500 | Internal server error |