# MedSync - API Reference

## Base URL

```
http://localhost:8000/api/v1
```

## Authentication

All endpoints require JWT Bearer token in `Authorization` header.

## Endpoints

### System Status

```
GET /status
```

Returns overall system status including today's adherence, active alerts, latest vitals, and next scheduled dose.

**Response:**
```json
{
  "adherence_today": {
    "date": "2026-06-15",
    "doses_scheduled": 3,
    "doses_taken": 2,
    "doses_missed": 0,
    "adherence_pct": 66.7
  },
  "active_alerts": [...],
  "latest_vitals": {
    "heart_rate_bpm": 72,
    "spo2_percent": 98,
    "activity_level": 0,
    "steps_count": 3421,
    "timestamp": "2026-06-15T14:30:00Z"
  },
  "next_dose": {
    "medication_name": "Metformin",
    "time": "18:00",
    "bin_id": 2
  },
  "system_healthy": true
}
```

### Schedule Management

```
GET /schedule?patient_id=1
POST /schedule
```

**Create Schedule Entry:**
```json
{
  "patient_id": 1,
  "medication_id": 5,
  "bin_id": 2,
  "hour": 8,
  "minute": 0,
  "frequency": "daily",
  "days_of_week": null
}
```

### Vitals History

```
GET /vitals?patient_id=1&hours=24
```

Returns time-series vitals data (heart rate, SpO2, activity, steps).

### Dose History

```
GET /doses?patient_id=1&days=7
POST /dose/confirm?patient_id=1&medication_id=5&bin_id=2&method=app
```

### Adherence Tracking

```
GET /adherence?patient_id=1&days=30
```

Returns daily adherence percentages for the past N days.

### Alerts

```
GET /alerts?patient_id=1&level=2&limit=50
POST /alerts/{alert_id}/acknowledge
```

### Caregiver Management

```
GET /caregivers?patient_id=1
POST /caregivers
```

**Add Caregiver:**
```json
{
  "name": "Jane Smith",
  "phone": "+1-555-123-4567",
  "email": "jane@example.com",
  "relationship": "daughter",
  "notify_dose_missed": true,
  "notify_fall": true,
  "notify_vitals_abnormal": true,
  "notify_refill": true
}
```

### WebSocket

```
WS /ws/v1/live
```

Real-time event stream. Messages include:
- `vitals`: Heart rate, SpO2, activity updates
- `dose_event`: Dose taken, missed, confirmed
- `alert`: Fall detected, dose overdue, abnormal vitals