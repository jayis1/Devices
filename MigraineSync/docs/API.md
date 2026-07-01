# MigraineSync — API Specification

Base URL: `https://api.migrainesync.io/api/v1`

Authentication: JWT Bearer token (`Authorization: Bearer <token>`)

---

## Endpoints

### GET `/health`
Health check (no auth required).

**Response:**
```json
{"status": "ok", "service": "migrainesync", "version": "1.0.0"}
```

### GET `/risk`
48-hour migraine onset risk forecast.

**Response:**
```json
{
  "risk_score": 72.5,
  "risk_level": "high",
  "confidence": 0.84,
  "forecast_hours": 48,
  "contributing_factors": [
    {"factor": "barometric_pressure_drop", "contribution_pct": 35.2, "value": "-4.2 hPa/3h"},
    {"factor": "hrv_decline", "contribution_pct": 28.1, "value": "RMSSD 18ms (baseline 35ms)"},
    {"factor": "poor_sleep", "contribution_pct": 18.5, "value": "sleep_score 42/100"},
    {"factor": "dehydration", "contribution_pct": 12.3, "value": "intake 800ml (goal 2000ml)"}
  ],
  "trend": "increasing",
  "recommended_action": "Consider taking preventive medication now. Hydrate. Avoid bright light.",
  "last_updated": "2026-07-01T14:30:00Z"
}
```

### GET `/triggers`
Personal trigger attribution (XGBoost SHAP).

**Response:**
```json
[
  {"trigger": "barometric_pressure", "contribution_pct": 32.5, "exposure_level": "high", "recommendation": "Rapid pressure drop detected. Consider preventive medication."},
  {"trigger": "sleep_quality", "contribution_pct": 24.0, "exposure_level": "moderate", "recommendation": "Last night's sleep was poor. Prioritize rest tonight."},
  {"trigger": "hydration", "contribution_pct": 18.2, "exposure_level": "low", "recommendation": "You are 600ml below target. Drink water now."},
  {"trigger": "stress", "contribution_pct": 15.3, "exposure_level": "high", "recommendation": "HRV indicates high stress. Try 10min breathing exercise."},
  {"trigger": "light_exposure", "contribution_pct": 6.0, "exposure_level": "moderate", "recommendation": "Reduce screen brightness. Wear sunglasses outdoors."},
  {"trigger": "noise", "contribution_pct": 4.0, "exposure_level": "low", "recommendation": "No action needed."}
]
```

### GET `/triggers/heatmap`
Trigger co-occurrence heatmap data (last 90 days).

**Response:**
```json
{
  "triggers": ["barometric", "sleep", "hydration", "stress", "light", "noise"],
  "matrix": [[1.0, 0.42, 0.15, 0.38, 0.10, 0.05], ...],
  "migraine_correlation": [0.73, 0.55, 0.30, 0.68, 0.22, 0.12]
}
```

### GET `/hydration`
Hydration summary.

**Response:**
```json
{
  "intake_today_ml": 1400,
  "goal_ml": 2000,
  "pct_of_goal": 70.0,
  "sip_count_today": 12,
  "pattern": "adequate",
  "last_sip": "2026-07-01T13:45:00Z",
  "trend_7d": [2100, 1800, 1500, 1200, 900, 1600, 1400],
  "recommendation": "600ml below target. Drink 2 glasses of water."
}
```

### GET `/events`
Recent events.

**Query params:** `limit` (1-500, default 50)

**Response:**
```json
[
  {"timestamp": "2026-07-01T14:00:00Z", "event_type": "alert", "severity": 3, "message": "High migraine risk (72%)"},
  {"timestamp": "2026-07-01T10:00:00Z", "event_type": "manual", "severity": 1, "message": "User logged: mild headache"},
  {"timestamp": "2026-07-01T08:00:00Z", "event_type": "hydration", "severity": 1, "message": "Hydration reminder sent"}
]
```

### GET `/trends`
Time-series trend data.

**Query params:**
- `metric`: hrv | pressure | light | hydration | sleep_score | skin_temp | activity
- `hours`: 1-720 (default 24)

**Response:**
```json
{
  "metric": "hrv",
  "hours": 24,
  "data": [
    {"timestamp": "2026-07-01T00:00:00Z", "avg": 35.2, "max": 42.1},
    {"timestamp": "2026-07-01T01:00:00Z", "avg": 33.8, "max": 40.5}
  ]
}
```

### GET `/action-plan`
Personalized intervention recommendations.

**Response:**
```json
{
  "zone": "yellow",
  "risk_score": 62.0,
  "steps": [
    "Take your preventive medication (as prescribed by your doctor)",
    "Drink 500ml of water immediately (hydration is low)",
    "Avoid bright screens for the next 2 hours",
    "Do a 10-minute guided breathing exercise",
    "Log any symptoms in the app"
  ],
  "last_updated": "2026-07-01T14:30:00Z"
}
```

### POST `/event`
Log a manual event.

**Body:**
```json
{
  "event_type": "migraine_onset",
  "value": 7,
  "note": "Throbbing right side, aura visible"
}
```

### GET `/report`
Neurologist-ready clinical report.

**Response:**
```json
{
  "report_type": "90-day migraine summary",
  "generated": "2026-07-01T14:30:00Z",
  "period": "90 days",
  "summary": {
    "migraine_count": 12,
    "avg_duration_hours": 18.5,
    "avg_severity": 6.8,
    "top_triggers": [
      {"trigger": "barometric_pressure", "correlation": 0.73},
      {"trigger": "sleep_quality", "correlation": 0.55},
      {"trigger": "stress", "correlation": 0.68}
    ],
    "prediction_accuracy_auc": 0.81,
    "medication_adherence_pct": 87.0
  }
}