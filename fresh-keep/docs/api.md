# FreshKeep — API Documentation

## Base URL
```
Production: https://api.freshkeep.local:8000
Local: http://freshkeep.local:8000
```

## Authentication
API key via `X-API-Key` header (for external integrations).
BLE and MQTT connections are authenticated by hub pairing.

---

## Endpoints

### GET /api/status
Get overall system status.

**Response:**
```json
{
  "status": "online",
  "fridge": {
    "spoilage_score": 35,
    "temp_c": 3.8,
    "door": 0,
    "battery_pct": 87
  },
  "pantry": {
    "items_count": 42,
    "temp_c": 22.1,
    "door": 0
  },
  "stove_guard": {
    "alert_level": 0,
    "max_temp_c": 25,
    "gas_valve_open": 1,
    "fire_confidence": 5
  },
  "fire_alerts": 0
}
```

### GET /api/fridge/history?hours=24
Get fridge sensor data history.

**Query Parameters:**
- `hours` (int, default 24, max 168): Hours of history to return

**Response:** Array of readings with timestamps, gas values, temperatures, spoilage scores.

### GET /api/stove/history?hours=24
Get stove guard data history.

**Response:** Array of readings with temperatures, gas levels, flame/smoke status, alert levels.

### GET /api/inventory?location=fridge&fresh_only=true
Get current inventory items.

**Query Parameters:**
- `location` (string, optional): "fridge" or "pantry"
- `fresh_only` (bool, default true): Only show items not yet spoiled

**Response:**
```json
[
  {
    "id": 123,
    "name": "Organic Whole Milk",
    "location": "fridge",
    "barcode": "012345678905",
    "category": "dairy",
    "weight_mg": 1950000,
    "expiry_date": "2026-06-22T00:00:00",
    "days_until_expiry": 6,
    "still_fresh": true
  }
]
```

### POST /api/inventory
Add or update an inventory item (from barcode scan or manual entry).

**Request Body:**
```json
{
  "action": "added",
  "location": "pantry",
  "barcode": "012345678905",
  "name": "Organic Whole Milk",
  "weight_mg": 1950000,
  "expiry_days": 7,
  "category": "dairy"
}
```

### GET /api/shopping-list
Get auto-generated shopping list.

**Response:**
```json
[
  {
    "id": 45,
    "name": "Eggs",
    "category": "protein",
    "quantity": "1 dozen",
    "priority": "high",
    "source": "auto"
  },
  {
    "id": 46,
    "name": "Milk",
    "category": "dairy",
    "quantity": "1 gallon",
    "priority": "urgent",
    "source": "auto"
  }
]
```

### POST /api/shopping-list
Add item to shopping list manually.

**Request Body:**
```json
{
  "name": "Avocados",
  "category": "produce",
  "quantity": "3",
  "priority": "medium",
  "source": "manual"
}
```

### PUT /api/shopping-list/{item_id}/purchased
Mark a shopping list item as purchased.

### GET /api/recipes/suggestions?max_results=10
Suggest recipes based on expiring inventory items.

**Response:**
```json
{
  "suggestions": [
    {
      "priority": "high",
      "item_name": "Chicken Breast",
      "category": "protein",
      "days_until_expiry": 1,
      "suggestion": "Use Chicken Breast soon — expires in 1 day. Try: Chicken Stir-Fry with the peppers and onions you also have."
    }
  ]
}
```

### GET /api/fire-alarms?resolved=false&limit=20
Get fire alarm history.

**Response:**
```json
[
  {
    "id": 3,
    "timestamp": "2026-06-16T14:30:00",
    "max_temp_c": 285,
    "lpg_ppm": 0,
    "smoke_level": 45,
    "flame_detected": false,
    "fire_confidence": 230,
    "source_node": 3,
    "resolved": true,
    "resolution": "False alarm — searing steak at high heat"
  }
]
```

### POST /api/commands/{node}
Send command to a node via MQTT (through hub).

**Path Parameters:**
- `node`: "fridge", "pantry", or "stove-guard"

**Request Body:**
```json
{
  "command": "photo_trigger",
  "params": {"camera": 1}
}
```

**Available Commands:**
| Node | Command | Description |
|------|---------|-------------|
| stove-guard | `gas_shutoff` | Close gas valve (params: {state: 0=close, 1=open}) |
| stove-guard | `suppression_on` | Activate fire suppression |
| stove-guard | `suppression_off` | Deactivate fire suppression |
| fridge | `photo_trigger` | Capture photo from specified camera |
| pantry | `barcode_scan` | Trigger barcode scanner |
| all | `calibrate_weight` | Recalibrate weight sensors (empty state) |
| all | `reset` | Reset node |

### WebSocket /ws
Real-time data streaming to dashboard.

**Messages:**
```json
{
  "topic": "freshkeep/stove/data",
  "data": {
    "max_temp_c": 180,
    "lpg_ppm": 0,
    "fire_confidence": 15,
    "alert_level": 1
  }
}
```

---

## MQTT Topics

| Topic | Direction | Payload | Frequency |
|-------|-----------|---------|-----------|
| `freshkeep/fridge/data` | Hub → Cloud | FridgeData struct (binary) | Every 500ms |
| `freshkeep/pantry/data` | Hub → Cloud | PantryData struct (binary) | Every 500ms |
| `freshkeep/stove/data` | Hub → Cloud | StoveData struct (binary) | Every 500ms |
| `freshkeep/fire/alarm` | Hub → Cloud | FireAlarm struct (binary) | On event |
| `freshkeep/inventory/update` | Hub → Cloud | InventoryUpdate struct | On barcode scan |
| `freshkeep/commands/fridge` | Cloud → Hub | Command struct | On demand |
| `freshkeep/commands/pantry` | Cloud → Hub | Command struct | On demand |
| `freshkeep/commands/stove-guard` | Cloud → Hub | Command struct | On demand |

---

## Error Responses

All endpoints return standard HTTP error codes:

| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Bad request (invalid parameters) |
| 404 | Resource not found |
| 500 | Internal server error |

Error body:
```json
{
  "detail": "Error description"
}
```