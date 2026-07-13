# CardioSync — Architecture

## System Overview

CardioSync is a multi-node cardiovascular health monitoring system consisting of 4 node types:

1. **Hub** (ESP32-S3) — Central coordinator, AFib CNN edge inference, alert dispatch
2. **ECG Chest Patch** (nRF52840) — Continuous single-lead ECG with R-peak detection
3. **BP Wrist Cuff** (ESP32-C3) — Automated oscillometric blood pressure measurement
4. **Smart Ring** (nRF52833) — PPG heart rate, HRV, SpO₂, skin temperature

## Communication Topology

```
        ┌──────────┐
        │  Cloud   │ (FastAPI + MQTT + TimescaleDB)
        └────▲─────┘
             │ Wi-Fi / 4G LTE (SIM7000)
        ┌────┴─────┐
        │   Hub    │ (ESP32-S3)
        └──┬───┬──┘
           │   │     BLE 5.0
     ┌─────┘   └─────┐
     ▼         ▼     ▼
  ECG Patch  BP Cuff  Smart Ring
  (nRF52840) (ESP32-C3) (nRF52833)
```

- **Hub ↔ Cloud**: Wi-Fi (primary) + SIM7000 4G LTE (backup for emergencies)
- **Hub ↔ Nodes**: BLE 5.0 (secure connections, ECDH)
- **Hub ↔ Mobile**: BLE 5.0 (pairing) + Cloud WebSocket (real-time data)

## ECG Signal Pipeline

```
Ag/AgCl electrodes → ADS1292R (24-bit, gain 12, 250 SPS)
   → SPI → nRF52840
   → Pan-Tompkins R-peak detection (real-time)
   → BLE 5.0 → Hub (250 Hz ECG + HR + R-R)
   → Hub: 30 s window → tflite-micro AFib CNN (200 ms inference)
   → Classify: Normal / AFib / PVC / VT / Bradycardia
   → AFib → Yellow LED + haptic + push notification
   → VT or Brady <30 bpm → Red LED + siren + emergency contacts
   → Cloud: ECG event storage + cardiologist PDF report
```

## ECG Lead Configuration

CardioSync uses **Lead I** (Einthoven):
- **Positive electrode**: Left arm (LA) — left subclavicular area
- **Negative electrode**: Right arm (RA) — right subclavicular area
- **RLD electrode**: Right leg drive — lower right chest

This measures the potential difference between the left and right arms, which captures atrial activity (P-waves) and ventricular activity (QRS complexes, T-waves) — sufficient for AFib, PVC, VT, and bradycardia detection.

## Blood Pressure Measurement

Oscillometric method:
1. Cuff inflates to 180 mmHg (occludes brachial artery)
2. Controlled deflation at 3 mmHg/s
3. Pressure sensor captures oscillometric envelope
4. MAP = pressure at maximum oscillation amplitude
5. Systolic = pressure where envelope rises to 0.55 × max (ratio method)
6. Diastolic = pressure where envelope falls to 0.70 × max

Safety interlocks:
- LM393 hardware comparator: instant valve open at 200 mmHg
- Software timeout: 60 s max measurement duration
- IMU position check: wrist must be at heart level

## ML Pipeline Overview

| # | Model | Location | Input | Output |
|---|-------|----------|-------|--------|
| 1 | AFib CNN | Edge (Hub, tflite-micro) | 30 s ECG (250 Hz) | 5-class arrhythmia |
| 2 | PVC/VT Classifier | Cloud | 10 s ECG (250 Hz) | PVC/VT/Normal |
| 3 | BP Trend LSTM | Cloud | 30-day BP history | BP trend + stage |
| 4 | HRV Analysis | Cloud | R-R intervals (5 min) | RMSSD, SDNN, pNN50 |
| 5 | Stroke Risk XGBoost | Cloud | AFib burden + BP + HRV + CHA₂DS₂-VASc | 30-day risk (%) |
| 6 | Sleep Apnea LSTM | Cloud | Overnight SpO₂ + HR | Apnea risk score |
| 7 | POTS Detector | Cloud | HR + BP on standing | POTS positive/negative |

## Data Flow Summary

```
ECG Patch (250 Hz ECG) ──→ Hub ──→ AFib CNN (edge) ──→ Events
                                              │
Smart Ring (PPG HR/HRV/SpO₂) ──→ Hub ──→ Cross-validate HR
                                    │     │
BP Cuff (scheduled BP) ──→ Hub ──→ Hypertension classification
                                    │
                                    ▼
                              Cloud (MQTT)
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
              BP Trend LSTM   Stroke Risk XGB   PDF Reports
              Sleep Apnea      POTS Detector     Cardiologist
              HRV Analysis                       Emergency Alerts
```

## Security

- BLE 5.0 LE Secure Connections (ECDH P-256)
- TLS 1.3 for cloud communication
- AES-256 at rest (TimescaleDB)
- Signed firmware OTA (Ed25519)
- HIPAA-ready architecture (audit logging, RBAC, minimum necessary data)