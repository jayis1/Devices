# AsthmaSync — API Reference

## Base URL
```
https://api.asthmasync.io
```

## Authentication
All endpoints require a Bearer token:
```
Authorization: Bearer <token>
```

## Endpoints

### GET /api/v1/health
Health check.

**Response:**
```json
{
  "status": "ok",
  "service": "asthmasync",
  "version": "1.0.0"
}
```

---

### GET /api/v1/risk
7-day asthma exacerbation risk forecast.

**Response:**
```json
{
  "risk_score": 42.5,
  "risk_level": "moderate",
  "confidence": 0.78,
  "forecast_days": 7,
  "contributing_factors": [
    {
      "factor": "rescue_inhaler_use",
      "value": 3,
      "weight": 24
    },
    {
      "factor": "wheeze_frequency",
      "value": 8,
      "weight": 25
    }
  ],
  "trend": "declining"
}
```

| Field | Type | Description |
|-------|------|-------------|
| risk_score | float | 0-100 risk score |
| risk_level | string | "low" (<30), "moderate" (30-60), "high" (>60) |
| confidence | float | Model confidence 0-1 |
| contributing_factors | array | Factors driving the risk score |
| trend | string | "improving", "stable", or "declining" |

---

### GET /api/v1/triggers
Personal trigger attribution (XGBoost + SHAP).

**Response:**
```json
[
  {
    "trigger": "PM2.5 (fine particles)",
    "contribution_pct": 32.5,
    "exposure_level": "high",
    "recommendation": "Use HEPA air purifier, close windows during high pollution"
  },
  {
    "trigger": "VOCs (volatile compounds)",
    "contribution_pct": 18.2,
    "exposure_level": "moderate",
    "recommendation": "Increase ventilation, check for new furniture/paint off-gassing"
  }
]
```

---

### GET /api/v1/adherence
Medication adherence summary.

**Response:**
```json
{
  "rescue_count_7d": 3,
  "rescue_count_30d": 11,
  "controller_adherence_pct": 85.0,
  "last_rescue": "2024-06-30T14:23:00Z",
  "gina_controlled": false
}
```

**GINA Control Assessment:**
- `gina_controlled: true` → rescue use ≤ 2/week
- `gina_controlled: false` → rescue use > 2/week (partly controlled)

---

### GET /api/v1/events
Recent events (wheeze, actuation, alerts).

**Query Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| limit | int | 50 | Max events to return (1-500) |

**Response:**
```json
[
  {
    "timestamp": "2024-06-30T14:23:00Z",
    "event_type": "actuation",
    "severity": 1,
    "message": "Rescue inhaler used"
  },
  {
    "timestamp": "2024-06-30T13:45:00Z",
    "event_type": "wheeze",
    "severity": 1,
    "message": "Wheeze detected"
  }
]
```

---

### GET /api/v1/trends
Time-series trend data for a specific metric.

**Query Parameters:**
| Parameter | Type | Default | Options |
|-----------|------|---------|---------|
| metric | string | "pm25" | pm25, co2, voc, hr, spo2, hrv, wheeze_prob |
| hours | int | 24 | 1-168 |

**Response:**
```json
{
  "metric": "pm25",
  "hours": 24,
  "data": [
    {
      "timestamp": "2024-06-30T00:00:00Z",
      "avg": 12.5,
      "max": 28.3
    }
  ]
}
```

---

### GET /api/v1/action-plan
GINA-aligned action plan based on current data.

**Response:**
```json
{
  "zone": "yellow",
  "rescue_use_7d": 3,
  "last_spo2": 96,
  "steps": [
    "Review trigger exposure (check app for air quality alerts)",
    "Ensure controller medication adherence",
    "Consider doubling controller dose per your action plan",
    "Monitor for worsening symptoms"
  ],
  "last_updated": "2024-06-30T15:00:00Z"
}
```

**Zone Logic:**
| Zone | Condition | Action |
|------|-----------|--------|
| Green | rescue ≤ 2/week, SpO₂ ≥ 95% | Continue current regimen |
| Yellow | rescue 3-4/week or SpO₂ 92-94% | Step up controller, review triggers |
| Red | rescue > 4/week or SpO₂ < 92% | Seek medical help, consider ER |

---

### POST /api/v1/event
Log a manual event (symptom, peak flow reading, etc.).

**Request Body:**
```json
{
  "event_type": "peak_flow",
  "value": 450,
  "note": "Morning reading, felt tight"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| event_type | string | Yes | symptom, peak_flow, exercise, medication |
| value | float | No | Numeric value (e.g., L/min for peak flow) |
| note | string | No | Free-text note |

---

### GET /api/v1/report
Generate 30-day clinical summary report.

**Response:**
```json
{
  "report_type": "30-day asthma summary",
  "generated": "2024-06-30T15:00:00Z",
  "period": "30 days",
  "summary": {
    "rescue_inhaler_uses": 11,
    "wheeze_events": 24,
    "avg_pm25_exposure": 18.5,
    "avg_spo2": 96.2,
    "alert_count": 3,
    "gina_controlled": true
  }
}
```