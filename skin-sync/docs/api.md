# SkinSync API Reference

Base URL: `https://api.skinsync.local/api/v1`

## Health

### GET /health
Returns service status.

## Users

### POST /users
Create a new user with Fitzpatrick skin type.

**Parameters:**
- `name` (string): User name
- `fitz_type` (int, 1-6): Fitzpatrick skin type

**Response:**
```json
{
  "id": 1,
  "name": "Alice",
  "fitz_type": 2,
  "personal_med": 250
}
```

## UV Exposure

### GET /users/{user_id}/uv
Get UV exposure history.

**Parameters:**
- `hours` (int, default 24): Hours of history

**Response:**
```json
[
  {
    "ts": "2026-06-22T14:30:00",
    "uva": 4500.0,
    "uvb": 500.0,
    "med_frac": 35,
    "uv_idx": 8.5,
    "temp_c": 33.2,
    "uv_status": 1
  }
]
```

## Skin Scans

### GET /users/{user_id}/scans
Get skin scan history.

**Response:**
```json
[
  {
    "ts": "2026-06-22T10:00:00",
    "condition": "acne_inflammatory",
    "conf": 87,
    "abcde": 0,
    "skin_age": 32,
    "lesion_id": 0,
    "image_url": "cloud://scans/1/20260622_100000.jpg"
  }
]
```

### POST /users/{user_id}/scan/upload
Upload multispectral scan images.

**Parameters:**
- `body_location` (int): Body location code (see protocol)
- `lesion_id` (int, optional): Tracked lesion ID
- `files` (multipart): 4 images (white, UV, NIR, polarized)

## Lesions

### GET /users/{user_id}/lesions
Get tracked lesions/moles.

**Response:**
```json
[
  {
    "lesion_id": 1,
    "location": 1,
    "first_seen": "2026-06-01T10:00:00",
    "last_scanned": "2026-06-22T10:00:00",
    "abcde": 45,
    "status": "changing"
  }
]
```

## Dispensing

### GET /users/{user_id}/dispense
Get dispensing history.

### POST /users/{user_id}/dispense
Trigger manual dispensing.

**Parameters:**
- `slot` (int): Product slot (0-3)
- `amount_mg` (int): Amount in milligrams

## Risk Scores

### GET /users/{user_id}/risk
Get current + trend risk scores.

**Response:**
```json
{
  "current": {
    "uv_burn_risk": 35,
    "skin_cancer_risk": 22,
    "skin_status": 0,
    "skin_age": 32,
    "routine_score": 78
  },
  "trend": [...]
}
```

## Alerts

### GET /users/{user_id}/alerts
Get alert history.

## Inventory

### GET /users/{user_id}/inventory
Get current product inventory.

## Dermatologist Report

### GET /users/{user_id}/derm-report
Generate a clinical report for dermatologist visits.

**Response:**
```json
{
  "patient": { "name": "Alice", "fitz_type": 2, "personal_med": 250 },
  "uv_exposure": {
    "today_uva_jm2": 4500,
    "today_uvb_jm2": 500,
    "annual_estimated_uvb_jm2": 180000,
    "med_fraction_today": 35
  },
  "skin_conditions": [...],
  "lesions": [...],
  "risk_assessment": { "skin_cancer_risk": 22, "skin_age": 32 },
  "recommendation": "Annual skin check recommended"
}
```

## WebSocket

### WS /ws/alerts/{user_id}
Real-time alert stream.

**Messages:**
```json
{
  "type": "uv_warning",
  "severity": "high",
  "message": "UV at 75% MED — seek shade or reapply sunscreen"
}
```