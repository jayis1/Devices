# SeizureSync — Cloud API Reference

Base URL: `https://api.seizuresync.com`

## Authentication
JWT Bearer token in `Authorization` header.
```
Authorization: Bearer <token>
```

## Endpoints

### Patients

#### GET /patients
List all patients (for caregiver mode).

**Response 200:**
```json
[{"id":"uuid","name":"Jane Doe","epilepsy_type":"focal",
  "seizure_frequency":3.2,"sudep_risk_score":0.42}]
```

#### POST /patients
Register a new patient.

**Body:**
```json
{"name":"Jane Doe","birth_date":"1990-01-15",
 "epilepsy_type":"focal","seizure_frequency":3.2}
```

#### GET /patients/{id}
Get patient details.

#### GET /patients/{id}/events
List seizure events.

**Query params:** `limit` (default 50), `since` (ISO datetime)

**Response 200:**
```json
[{"id":1,"patient_id":"uuid","onset":"2024-01-15T10:30:00Z",
  "duration_s":45,"semiology":"fbtcs","severity":2,
  "confidence":95,"recovery_state":"recovered",
  "triggers":["sleep_deprivation","missed_medication"]}]
```

#### POST /patients/{id}/events
Create a seizure event (manual entry or device-triggered).

**Body:**
```json
{"onset":"2024-01-15T10:30:00Z","duration_s":45,
 "semiology":"fbtcs","severity":2,"confidence":95,
 "triggers":["sleep_deprivation"]}
```

### Risk

#### GET /patients/{id}/risk
Get current 24-hour and 7-day risk forecasts.

**Response 200:**
```json
{"patient_id":"uuid","risk_24h":12.5,"risk_7d":45.0,
 "timestamp":"2024-01-15T12:00:00Z"}
```

### SUDEP

#### GET /patients/{id}/sudep
Get SUDEP risk assessment.

**Response 200:**
```json
{"patient_id":"uuid","annual_risk_pct":0.42,
 "apnea_density":2.1,"prone_episodes":3,
 "timestamp":"2024-01-15T12:00:00Z"}
```

### Reports

#### POST /patients/{id}/reports
Generate a neurologist PDF report.

**Response 200:** PDF file download.

#### GET /patients/{id}/reports/{report_id}
Download a previously generated report.

### Alerts

#### GET /patients/{id}/alerts
List alert history.

#### POST /patients/{id}/alerts/{alert_id}/ack
Acknowledge an alert.

### WebSocket

#### WS /ws/{patient_id}
Real-time alert stream for mobile app.

**Messages (server → client):**
```json
{"type":"seizure","severity":2,"semiology":"fbtcs","onset":"..."}
{"type":"aura","probability":73,"lead_time_s":360}
{"type":"sudep","apnea_state":3,"spo2":82}
{"type":"risk_update","risk_24h":15.2}
```

### Webhooks

#### POST /webhooks/twilio
Twilio status callback for emergency dispatch calls.