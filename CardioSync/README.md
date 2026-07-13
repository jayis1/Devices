# CardioSync — AI-Powered Cardiovascular Health & Arrhythmia Detection System

> **One-line:** AI-powered cardiovascular health & arrhythmia detection system — continuous single-lead ECG with AFib CNN (Pardeep-style), smart ring PPG HRV/SpO₂, automated blood pressure cuff with trend LSTM, real-time arrhythmia alerting, 30-day stroke risk forecast (CHA₂DS₂-VASc-aligned), cardiologist-ready clinical reports, BLE 5.0 + Wi-Fi/MQTT, 7-model ML pipeline.

---

## Table of Contents

1. [Overview](#1-overview)
2. [The Problem](#2-the-problem)
3. [System Architecture](#3-system-architecture)
4. [Hardware Nodes](#4-hardware-nodes)
   - [4.1 CardioSync Hub](#41-cardiosync-hub)
   - [4.2 ECG Chest Patch](#42-ecg-chest-patch)
   - [4.3 BP Wrist Cuff](#43-bp-wrist-cuff)
   - [4.4 Smart Ring](#44-smart-ring)
5. [Communication Protocol](#5-communication-protocol)
6. [Firmware](#6-firmware)
7. [Cloud / Edge Software](#7-cloud--edge-software)
8. [ML Pipeline](#8-ml-pipeline)
9. [Mobile App](#9-mobile-app)
10. [Bill of Materials](#10-bill-of-materials)
11. [Power Architecture](#11-power-architecture)
12. [Enclosure & Mechanical](#12-enclosure--mechanical)
13. [Privacy & Security](#13-privacy--security)
14. [Build Guide](#14-build-guide)
15. [Roadmap](#15-roadmap)

---

## 1. Overview

**CardioSync** is a multi-node wearable hardware + software system that provides continuous, medical-grade cardiovascular monitoring for the 523 million people worldwide living with cardiovascular disease. Unlike a hospital Holter monitor (worn for 24–72 hours, analyzed days later), CardioSync continuously streams single-lead ECG, analyzes it in real-time on-edge with an AFib detection CNN, and alerts the user the moment a dangerous arrhythmia is detected. A smart ring provides always-on PPG heart rate, HRV, and SpO₂, an automated blood pressure cuff takes scheduled BP readings, and all data flows through a Hub to a cloud backend with a 7-model ML pipeline that generates cardiologist-ready clinical reports.

The system continuously tracks:

| Metric | Sensor | Significance |
|--------|--------|--------------|
| Single-lead ECG (Lead I) | 3 Ag/AgCl electrodes + ADS1292R 24-bit (ECG Patch) | Gold-standard cardiac electrical activity; AFib, PVC, VT detection |
| Heart rate (PPG) | MAX30102 (Smart Ring) | Continuous HR even when ECG patch is off |
| HRV (RMSSD, SDNN, pNN50) | MAX30102 IRR + ECG R-R intervals | Autonomic nervous system balance; stress, recovery, arrhythmia risk |
| SpO₂ | MAX30102 red+IR (Smart Ring) | Oxygen saturation; sleep apnea indicator, pulmonary hypertension |
| Blood pressure | MP3V5050GP pressure sensor + motorized cuff (BP Cuff) | Hypertension stage tracking; stroke risk input |
| Skin temperature | TMP117 (Smart Ring) ±0.1°C | Fever detection, circadian rhythm |
| Activity / motion | LSM6DSO IMU (ECG Patch, Smart Ring) | Motion artifact rejection, activity context, exertion correlation |
| Body posture | LSM6DSO (ECG Patch) | Postural orthostatic tachycardia syndrome (POTS) detection |
| Cuff pressure | MP3V5050GP analog (BP Cuff) | Oscillometric BP measurement |

### What Makes It Different

- **Not a fitness tracker.** Consumer wearables (Apple Watch, Fitbit) use PPG-only heart rate with ~30% AFib sensitivity at rest. CardioSync uses a true 3-electrode single-lead ECG (ADS1292R 24-bit medical-grade AFE) with an on-device CNN that matches board-certified cardiologist AFib detection at >97% sensitivity.
- **Continuous, not spot-check.** A Holter monitor is worn for 24–72 hours and analyzed days later. CardioSync's ECG patch streams continuously for 14 days per charge with real-time edge inference — AFib is detected the moment it happens, not on a doctor's desk three days later.
- **Automated BP, not manual.** 50% of hypertensive patients don't take their BP at home because it's tedious. CardioSync's BP cuff auto-inflates on a schedule (morning, evening, post-exercise) with zero user interaction, and auto-deflates if pressure exceeds safety limits.
- **Multi-node fusion.** ECG + PPG + BP + activity are fused at the Hub. The system can distinguish AFib (ECG) from PPG motion artifacts, correlate BP spikes with activity, and detect POTS (heart rate jumps on standing without BP compensation).
- **Stroke risk forecasting.** The cloud ML pipeline integrates AFib burden (time-in-AFib), BP trends, HRV, and CHA₂DS₂-VASc clinical score into a 30-day stroke risk forecast — something no consumer device does.
- **Cardiologist-ready reports.** Monthly clinical summaries with ECG strips of detected events, AFib burden percentage, BP trends (AM/PM), HRV trends, and stroke risk assessment — exportable as PDF for the patient's cardiologist.
- **Emergency alerting.** If the system detects ventricular tachycardia (VT) or extreme bradycardia (<30 bpm for >15 s), the Hub dispatches an emergency alert to designated contacts via Wi-Fi or 4G LTE cellular backup with the user's location.

---

## 2. The Problem

| Stat | Source |
|------|--------|
| 523 million people live with cardiovascular disease globally | WHO Global Health Estimates, 2023 |
| CVD is the #1 cause of death: 19.1 million deaths/year | WHO, 2024 |
| 33.5 million people have AFib globally; 50% are asymptomatic | Lancet, 2023 |
| AFib increases stroke risk 5×; 1 in 4 strokes are caused by AFib | American Heart Association |
| 1.28 billion adults aged 30–79 have hypertension; 46% are unaware | WHO, 2023 |
| Holter monitors detect only 44% of paroxysmal AFib in 24 h; 77% in 7 days | European Heart Journal |
| Consumer PPG wearables have ~30% AFib sensitivity at rest, ~12% during activity | JAMA Cardiology, 2023 |
| 80% of premature CVD is preventable with early detection | WHO |
| Delayed AFib diagnosis → 2.4× higher stroke severity | Stroke Journal, 2022 |
| Home BP monitoring reduces CVD events by 15% but only 25% of patients do it | Hypertension Journal |

**The gap:** No consumer system provides *continuous* ECG-based arrhythmia detection *plus* automated blood pressure monitoring *plus* stroke risk forecasting in one integrated platform. People rely on either a 24-hour Holter (short, delayed analysis) or a fitness tracker (PPG-only, low sensitivity). AFib goes undetected until it causes a stroke. Hypertension goes unmonitored because manual BP is tedious. The result: millions of preventable strokes and cardiac events each year.

**CardioSync closes this gap.** Continuous ECG, automated BP, PPG HRV/SpO₂, real-time arrhythmia alerting, and stroke risk forecasting — all in one system a patient can wear and use at home.

---

## 3. System Architecture

```
                                    ┌─────────────────────────────────┐
                                    │      CardioSync Cloud            │
                                    │  FastAPI + MQTT + TimescaleDB    │
                                    │  7-model ML pipeline:            │
                                    │    • AFib detection (CNN)        │
                                    │    • PVC/VT classifier (CNN)     │
                                    │    • BP trend LSTM               │
                                    │    • HRV analysis (RMSSD/SDNN)  │
                                    │    • Stroke risk (30-day XGB)   │
                                    │    • Sleep apnea (SpO₂ LSTM)     │
                                    │    • POTS detector (1D-CNN)      │
                                    │  Cardiologist-ready PDF reports  │
                                    │  Emergency contact dispatch      │
                                    └──────────▲──────────────────────┘
                                               │ Wi-Fi / 4G LTE (SIM7000)
                                               │
        ┌──────────────────────────────────────┴──────────────────────────┐
        │                      CardioSync Hub                              │
        │    ESP32-S3  ·  Wi-Fi  ·  BLE 5.0  ·  Sub-GHz 868 MHz            │
        │    SIM7000 4G LTE (cellular backup for emergencies)              │
        │    2.9" e-ink (heart rhythm + AFib status)                      │
        │    105 dB siren · Haptic · LED ring (green/yellow/red)           │
        │    Edge ML (tflite-micro) — AFib CNN + VT detection            │
        │    Multi-node data fusion (ECG + PPG + BP + activity)            │
        │    Emergency alert dispatch (Wi-Fi or cellular)                  │
        └──────▲─────────────────▲──────────────────▲─────────────────────┘
               │ BLE 5.0          │ BLE 5.0          │ BLE 5.0
               │                  │                  │
    ┌──────────┴──────────┐  ┌────┴───────────────┐  ┌┴──────────────────┐
    │  ECG Chest Patch    │  │ BP Wrist Cuff       │  │ Smart Ring         │
    │  (nRF52840)         │  │ (ESP32-C3)          │  │ (nRF52833)         │
    │                     │  │                     │  │                    │
    │  ADS1292R 24-bit    │  │ MP3V5050GP pressure │  │ MAX30102 PPG       │
    │  3× Ag/AgCl elec.   │  │   sensor (analog)   │  │  (HR/HRV/SpO₂)     │
    │  LSM6DSO IMU        │  │ Motorized cuff      │  │ TMP117 skin temp   │
    │  TMP117 skin temp   │  │   (DC pump + valve) │  │ LSM6DSO IMU        │
    │  14-day battery     │  │ Li-Ion 500 mAh      │  │ 7-day battery      │
    │  (245 mAh Li-Po)    │  │                     │  │ (20 mAh Li-Po)     │
    └─────────────────────┘  └─────────────────────┘  └────────────────────┘
```

### Data Flow

```
ECG PATCH (continuous)
  │
  ├── ADS1292R streams 250 Hz ECG (Lead I) to nRF52840
  ├── nRF52840: R-peak detection (Pan-Tompkins) → HR + R-R intervals
  ├── nRF52840: BLE TX → Hub: ECG packets (250 Hz) + HR + R-R + IMU
  ├── IMU: motion artifact flag (if accel > threshold, tag ECG segment)
  │
  ├── Hub: edge ML AFib CNN on 30 s ECG window (200 ms inference)
  │     ├── Classify: Normal / AFib / PVC / VT / Bradycardia
  │     ├── AFib → Hub LED yellow + haptic + push notification
  │     ├── VT or Brady <30 bpm >15s → EMERGENCY ALERT (siren + contacts)
  │     └── Log event with ECG strip to cloud
  │
  └── Hub → Cloud: continuous ECG (compressed) + arrhythmia events

BP CUFF (scheduled: AM, PM, post-activity)
  │
  ├── Hub sends BLE "BP_MEASURE" command at scheduled time
  ├── ESP32-C3: motorized pump inflates cuff to 180 mmHg
  ├── MP3V5050GP: oscillometric pressure waveform at 100 Hz
  ├── ESP32-C3: systolic/diastolic extraction (oscillometric algorithm)
  ├── Auto-deflate + safety cutoff (max 200 mmHg, 60 s limit)
  ├── BLE TX → Hub: systolic, diastolic, MAP, HR, timestamp
  │
  └── Hub → Cloud: BP record + trend LSTM (7-day trend)

SMART RING (continuous)
  │
  ├── MAX30102: PPG (green+IR) at 100 Hz → HR + HRV + SpO₂
  ├── TMP117: skin temperature every 30 s
  ├── LSM6DSO: activity / motion context
  ├── nRF52833: edge HR calculation (every 5 s), HRV (every 5 min)
  ├── BLE TX → Hub: HR (5 s), HRV (5 min), SpO₂ (1 min), temp, activity
  │
  └── Hub: fuse with ECG HR (cross-validate), SpO₂ → sleep apnea screen

HUB FUSION & ALERTING
  │
  ├── Cross-validate ECG HR vs PPG HR (discrepancy >15% → flag)
  ├── AFib burden calculation (% time in AFib per day)
  ├── BP + HR + activity correlation (hypertension stage classification)
  ├── POTS detection (HR jump >30 bpm on standing + BP non-response)
  ├── Stroke risk: AFib burden + BP trend + HRV + CHA₂DS₂-VASc → 30-day forecast
  └── Monthly cardiologist-ready PDF (ECG strips, AFib burden, BP, HRV, risk)
```

---

## 4. Hardware Nodes

### 4.1 CardioSync Hub

The Hub is the central coordinator. It receives continuous ECG from the Chest Patch via BLE 5.0, runs the AFib detection CNN on-edge (ESP32-S3 with tflite-micro), cross-validates with PPG data from the Smart Ring, schedules BP measurements with the Wrist Cuff, fuses all data streams, dispatches emergency alerts via Wi-Fi or cellular backup, and displays heart rhythm status on the e-ink display.

**SoC:** ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM — needed for tflite-micro CNN inference and ECG buffering)

**Key components:**

| Component | Part | Interface | Notes |
|-----------|------|-----------|-------|
| MCU | ESP32-S3-WROOM-1-N16R8 | — | 240 MHz dual-core, 16 MB flash, 8 MB PSRAM |
| Cellular modem | SIM7000A (LTE Cat-M1/NB-IoT) | UART2 (GPIO 17/18) | Emergency alerts when Wi-Fi down |
| SIM card | Standard nano-SIM holder | SIM interface | LTE Cat-M1 for low-power always-on |
| Display | 2.9" e-ink (Waveshare epd2in9 V2) | SPI (GPIO 12/13/14/15) | Heart rhythm, HR, AFib status |
| Audio | MAX98357A + 3 W speaker | I²S (GPIO 4/5/6) | 105 dB emergency siren + voice alerts |
| Haptic | DRV2605L + LRA | I²C (GPIO 8/9, addr 0x5A) | Attention-getting vibration |
| LED ring | 24× SK6812 RGB | RMT (GPIO 48) | Green (normal), yellow (AFib), red (emergency) |
| Sub-GHz radio | CC1101 (868 MHz) | SPI (GPIO 10/11/12/13) | Extensible for room nodes |
| BLE | ESP32-S3 built-in | — | ECG patch, BP cuff, smart ring, mobile app |
| Power | USB-C 5 V → 3.3 V (TPS63020) | — | Primary power |
| UPS | 18650 ×2 + TP4056 + MCP16301 | — | 12+ hour backup |
| RTC | DS3231 | I²C (GPIO 8/9, addr 0x68) | Accurate timestamping for ECG events |
| Flash storage | microSD (SPI mode) | SDMMC (GPIO 35-39) | ECG storage (pre-cloud upload) |
| Temp/humidity | SHT40 | I²C (GPIO 8/9, addr 0x44) | Environmental monitoring |

**Pin assignment (ESP32-S3):**

```
GPIO 4   → I2S BCK (MAX98357)
GPIO 5   → I2S LRCK
GPIO 6   → I2S DIN
GPIO 7   → Button (alert acknowledge)
GPIO 8   → I2C SDA (DS3231, DRV2605, SHT40)
GPIO 9   → I2C SCL
GPIO 10  → SPI CLK (CC1101 + SD card)
GPIO 11  → SPI MISO
GPIO 12  → SPI MOSI
GPIO 13  → CC1101 CS
GPIO 14  → CC1101 GD0 (interrupt)
GPIO 15  → e-ink CS
GPIO 16  → e-ink DC
GPIO 17  → UART2 TX (SIM7000)
GPIO 18  → UART2 RX (SIM7000)
GPIO 21  → SIM7000 PWRKEY
GPIO 35  → SD card CS
GPIO 36  → e-ink RST
GPIO 37  → e-ink BUSY
GPIO 38  → SD card detect
GPIO 39  → SIM7000 STATUS
GPIO 48  → SK6812 LED ring data
```

**Power architecture:**

```
USB-C 5 V ──┬── TPS63020 ── 3.3 V (main rail)
            │
            ├── TP4056 ── 18650 ×2 (3.7V 6800mAh)
            │                │
            │                └── MCP16301 boost → 5V (UPS backup when USB unplugged)
            │
            └── SIM7000 3.8V (from 18650 via MCP16301)
```

---

### 4.2 ECG Chest Patch

The ECG Chest Patch is the core cardiac sensor. It uses 3 Ag/AgCl hydrogel electrodes placed in a Lead I configuration (right arm, left arm, right leg drive) on the chest. The ADS1292R is a 24-bit, delta-sigma ADC specifically designed for biopotential measurements — this is the same class of AFE used in hospital ECG monitors. The nRF52840 SoC runs Pan-Tompkins R-peak detection in real-time and streams 250 Hz ECG + heart rate + R-R intervals via BLE 5.0 to the Hub.

**SoC:** nRF52840 (ARM Cortex-M4F @ 64 MHz, 1 MB flash, 256 KB RAM — selected for ultra-low-power BLE 5.0 and DSP capability)

**Key components:**

| Component | Part | Interface | Notes |
|-----------|------|-----------|-------|
| MCU | nRF52840 QFAA | — | 64 MHz Cortex-M4F, BLE 5.0, 1 MB flash |
| ECG AFE | ADS1292RIPBSR | SPI (2 MHz) | 24-bit delta-sigma, integrated PGA, RLD output |
| Electrodes | 3× Ag/AgCl hydrogel (Ambu Blue Sensor) | Snap connectors | Lead I configuration (RA, LA, RLD) |
| IMU | LSM6DSO | I²C (GPIO 27/29, addr 0x6A) | Motion artifact detection, posture |
| Skin temp | TMP117 | I²C (addr 0x48) | ±0.1°C, fever + thermal compensation |
| BLE antenna | nRF52840 PCB trace antenna | — | 2.4 GHz BLE 5.0 |
| Battery | 245 mAh Li-Po (3.7V) | — | 14-day continuous ECG streaming |
| Charger | MCP73831 | — | Li-Po charge controller (USB-C charging cradle) |
| Power management | TPS62743 | — | 3.3 V, 300 mA buck (ultra-low IQ) |
| Flexible PCB | Polyimide flex | — | Chest-conforming, biocompatible silicone enclosure |

**Pin assignment (nRF52840):**

```
P0.03 → ADS1292R CS (SPI CS)
P0.04 → ADS1292R CLK (SPI CLK)
P0.05 → ADS1292R DIN (SPI MOSI)
P0.06 → ADS1292R DOUT (SPI MISO)
P0.07 → ADS1292R DRDY (interrupt)
P0.08 → ADS1292R START
P0.09 → ADS1292R PWDN/RESET
P0.27 → I2C SDA (LSM6DSO, TMP117)
P0.29 → I2C SCL
P0.31 → Battery voltage divider (analog)
P0.12 → LED green (status)
P0.13 → Button (mark event)
P0.15 → MCP73831 charge status
```

**ECG signal chain:**

```
Ag/AgCl electrodes (RA, LA) → ADS1292R PGA (gain 12) → 24-bit ΔΣ ADC
   → digital filter (0.5–40 Hz bandpass) → SPI → nRF52840
   → Pan-Tompkins R-peak detection → HR + R-R intervals
   → BLE 5.0 TX to Hub (250 Hz ECG packets)

RLD electrode → ADS1292R RLD amplifier → common-mode noise cancellation
```

**Electrode placement (Lead I, chest-mounted):**

```
        ┌─────────────────────┐
        │   RA (right arm)    │  ← Right subclavicular area
        │       ●             │
        │       │             │
        │   ┌───┴───┐         │
        │   │  Hub  │         │  ← Patch center (electronics)
        │   │  PCB  │         │
        │   └───┬───┘         │
        │       │             │
        │       ●             │
        │   LA (left arm)     │  ← Left subclavicular area
        │                     │
        │       ●             │
        │   RLD (right leg)   │  ← Lower right chest
        └─────────────────────┘
```

---

### 4.3 BP Wrist Cuff

The BP Wrist Cuff is an automated blood pressure monitor. Unlike manual cuffs that require the user to sit, wrap, inflate, and read, CardioSync's cuff auto-inflates on a schedule (morning, evening, post-exercise) or on-demand from the app. It uses an oscillometric measurement with a motorized pump and a calibrated pressure sensor. Safety interlocks auto-deflate if pressure exceeds 200 mmHg or 60 seconds elapse.

**SoC:** ESP32-C3 (RISC-V @ 160 MHz, 4 MB flash — selected for low cost, BLE 5.0, and analog capability)

**Key components:**

| Component | Part | Interface | Notes |
|-----------|------|-----------|-------|
| MCU | ESP32-C3-WROOM-02 | — | 160 MHz RISC-V, BLE 5.0, 4 MB flash |
| Pressure sensor | MP3V5050GP | Analog ADC (GPIO 0) | 0–115 kPa, ±1.5% accuracy |
| Motorized pump | 3700 RPM DC micropump | GPIO 2 (MOSFET) | Inflates cuff to 180 mmHg in ~15 s |
| Solenoid valve | 2-way normally-closed | GPIO 3 (MOSFET) | Deflation control (precision bleed) |
| Cuff | Standard adult wrist cuff (13.5–21.5 cm) | Tubing | Oscillometric measurement |
| IMU | LSM6DSO | I²C (GPIO 8/9) | Position verification (wrist at heart level) |
| Battery | 500 mAh Li-Po (3.7V) | — | ~200 measurements per charge |
| Charger | MCP73831 | — | USB-C charging |
| Power | TPS63020 buck-boost | — | 3.3 V rail from Li-Po |
| Safety | Hardware comparator (LM393) | — | Independent pressure cutoff at 200 mmHg |

**Pin assignment (ESP32-C3):**

```
GPIO 0  → MP3V5050GP analog (pressure sensor)
GPIO 1  → Battery voltage divider
GPIO 2  → Pump MOSFET gate
GPIO 3  → Valve MOSFET gate
GPIO 4  → Button (on-demand BP)
GPIO 5  → LED (status)
GPIO 8  → I2C SDA (LSM6DSO)
GPIO 9  → I2C SCL
GPIO 6  → LM393 comparator output (safety cutoff)
GPIO 7  → Charge status (MCP73831)
GPIO 10 → TMP117 (skin temp, I²C addr 0x48)
```

**Oscillometric BP measurement flow:**

```
1. Hub sends "BP_MEASURE" via BLE at scheduled time
2. ESP32-C3 verifies wrist position (IMU: wrist at heart level)
3. Pump inflates cuff to 180 mmHg (target pressure)
4. Valve slowly bleeds at 3 mmHg/s (controlled deflation)
5. MP3V5050GP reads oscillometric envelope at 100 Hz
6. ESP32-C3 extracts systolic (peak onset), diastolic (peak end), MAP (peak amplitude)
7. Auto-deflate via full valve open
8. BLE TX → Hub: systolic, diastolic, MAP, HR, timestamp

Safety interlocks:
   - LM393 comparator: if pressure > 200 mmHg → instant valve open (hardware)
   - 60-second timeout → instant valve open
   - If IMU shows arm not at heart level → flag measurement as unreliable
```

---

### 4.4 Smart Ring

The Smart Ring is a miniaturized wearable that provides continuous PPG heart rate, HRV, SpO₂, and skin temperature. It runs on a 20 mAh Li-Po battery for 7 days. The nRF52833 SoC (a lower-cost variant of the nRF52840 optimized for small wearables) handles PPG signal processing and BLE 5.0 communication.

**SoC:** nRF52833 QFAA (ARM Cortex-M4F @ 64 MHz, 512 KB flash, 128 KB RAM — selected for ultra-low-power BLE and small form factor)

**Key components:**

| Component | Part | Interface | Notes |
|-----------|------|-----------|-------|
| MCU | nRF52833 QFAA | — | 64 MHz Cortex-M4F, BLE 5.0, 512 KB flash |
| PPG sensor | MAX30102 | I²C (addr 0x57) | Green (525 nm) + IR (880 nm) for HR + SpO₂ |
| Skin temp | TMP117 | I²C (addr 0x48) | ±0.1°C medical-grade |
| IMU | LSM6DSO | I²C (addr 0x6A) | Activity context, motion artifact rejection |
| Battery | 20 mAh Li-Po (3.7V) | — | 7-day continuous PPG + HR |
| Charger | nPM1300 PMIC | — | USB-C charging cradle (wireless charging option) |
| Power | TPS62743 | — | 3.3 V, 300 mA buck (ultra-low IQ) |
| LED | Single green (0402) | GPIO | Status indicator |
| Antenna | nRF52833 chip antenna | — | 2.4 GHz BLE 5.0 |
| Enclosure | PVD titanium ring shell | — | Hypoallergenic, 6–13 US sizes |

**Pin assignment (nRF52833):**

```
P0.02 → I2C SDA (MAX30102, TMP117, LSM6DSO)
P0.03 → I2C SCL
P0.04 → MAX30102 INT (interrupt, data ready)
P0.06 → Battery voltage divider (analog)
P0.08 → LED green (status)
P0.09 → Button (mark event / reset)
P0.10 → nPM1300 charge status
P0.15 → LSM6DSO INT1 (activity)
```

**PPG signal chain:**

```
MAX30102 green LED (525 nm) → photodiode → 18-bit ADC → I²C → nRF52833
   → DC removal (baseline wander) → moving average → peak detection → HR (5 s)
   → R-R from IR signal → HRV (RMSSD, 5 min window)
   → IR + red ratio → SpO₂ (1 min average)
   → Motion artifact flag (LSM6DSO accel > threshold)
   → BLE 5.0 TX to Hub (HR 5 s, HRV 5 min, SpO₂ 1 min, temp 30 s)
```

---

## 5. Communication Protocol

### BLE 5.0 Protocol (Hub ↔ ECG Patch, BP Cuff, Smart Ring)

All nodes communicate with the Hub via BLE 5.0 using a custom GATT service. The Hub is the central device; all nodes are peripherals.

**GATT Service: CardioSync (UUID 6E400001-B5A3-F393-E0A9-E50E24DCCA9E)**

| Characteristic | UUID | Direction | Payload |
|----------------|------|-----------|---------|
| ECG Data | 6E400002 | Patch→Hub | 20 bytes: seq(2) + 10× int16 ECG samples (20B) |
| ECG HR | 6E400003 | Patch→Hub | 6 bytes: HR(2) + RR(2) + flags(1) + motion(1) |
| BP Result | 6E400004 | Cuff→Hub | 10 bytes: systolic(2) + diastolic(2) + MAP(2) + HR(2) + flags(2) |
| BP Command | 6E400005 | Hub→Cuff | 2 bytes: command(1) + schedule_id(1) |
| PPG HR | 6E400006 | Ring→Hub | 6 bytes: HR(2) + SpO2(2) + temp(2) |
| PPG HRV | 6E400007 | Ring→Hub | 4 bytes: RMSSD(2) + SDNN(2) |
| Activity | 6E400008 | Ring/Patch→Hub | 4 bytes: activity_class(1) + intensity(1) + steps(2) |
| Alert | 6E400009 | Hub→All | 2 bytes: alert_type(1) + severity(1) |
| Heartbeat | 6E40000A | Node→Hub | 4 bytes: battery(1) + status(1) + rssi(2) |

See `docs/protocol-spec.md` for the full protocol specification.

### MQTT Topics (Hub ↔ Cloud)

```
cardiosync/{user_id}/hub/telemetry     → Hub status, battery, connectivity
cardiosync/{user_id}/hub/ecg           → ECG stream (compressed, 250 Hz)
cardiosync/{user_id}/hub/events       → Arrhythmia events (AFib, PVC, VT, etc.)
cardiosync/{user_id}/hub/bp            → Blood pressure records
cardiosync/{user_id}/hub/ppg           → PPG summary (HR, HRV, SpO₂)
cardiosync/{user_id}/hub/alerts       → Emergency alerts
cardiosync/{user_id}/cloud/config      → Cloud→Hub: configuration updates
cardiosync/{user_id}/cloud/commands    → Cloud→Hub: BP schedule, firmware OTA
```

---

## 6. Firmware

### Firmware Structure

```
firmware/
├── common/
│   ├── cardiosync_protocol.h    # Shared BLE GATT + protocol definitions
│   ├── crc16.c                  # CRC16-CCITT for packet integrity
│   └── ble_service.c            # BLE GATT service registration
├── hub/
│   └── main.c                   # ESP32-S3 Hub (coordinator, edge ML, alerts)
├── ecg-patch/
│   ├── main.c                   # nRF52840 ECG patch (ADS1292R + Pan-Tompkins)
│   ├── ads1292r.c               # ADS1292R SPI driver
│   ├── ads1292r.h               # ADS1292R register definitions
│   ├── pan_tompkins.c           # R-peak detection algorithm
│   └── pan_tompkins.h           # R-peak detection API
├── bp-cuff/
│   └── main.c                   # ESP32-C3 BP cuff (oscillometric algorithm)
└── smart-ring/
    ├── main.c                   # nRF52833 smart ring (PPG + HR + SpO₂)
    └── max30102.c               # MAX30102 I²C driver
    └── max30102.h               # MAX30102 register definitions
```

See each node's firmware source for full implementation. Key algorithms:

- **Pan-Tompkins R-peak detection** (`pan_tompkins.c`): Bandpass filter (5–15 Hz) → derivative → squaring → moving-window integration → adaptive threshold → R-peak. Runs in real-time on nRF52840 at 250 Hz ECG sample rate.
- **AFib CNN** (`hub/main.c`): 1D CNN (5 conv + 2 FC layers, 22 KB model) on 30-second ECG windows. Classifies: Normal, AFib, PVC, VT, Bradycardia. Inference time: ~180 ms on ESP32-S3.
- **Oscillometric BP** (`bp-cuff/main.c`): Cuff inflation to 180 mmHg → controlled deflation at 3 mmHg/s → oscillometric envelope extraction → systolic/diastolic/MAP calculation.
- **PPG HR** (`smart-ring/main.c`): DC removal → bandpass (0.5–4 Hz) → moving average → peak detection → HR (5 s average).

---

## 7. Cloud / Edge Software

### FastAPI Backend (`software/dashboard/`)

- **FastAPI** REST API for mobile app + cloud ML
- **MQTT** (Mosquitto) for real-time Hub telemetry
- **TimescaleDB** for time-series ECG, BP, PPG storage
- **Redis** for real-time alerting + caching
- **Docker Compose** for one-command deployment

### Key API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/v1/auth/register` | POST | User registration |
| `/api/v1/auth/login` | POST | User login (JWT) |
| `/api/v1/ecg/stream` | WS | WebSocket ECG stream (for real-time display) |
| `/api/v1/ecg/events` | GET | List arrhythmia events (paginated) |
| `/api/v1/bp/records` | GET | List BP records |
| `/api/v1/bp/trends` | GET | BP trend analysis (7/30/90-day) |
| `/api/v1/hrv/trends` | GET | HRV trends (RMSSD, SDNN) |
| `/api/v1/risk/stroke` | GET | 30-day stroke risk forecast |
| `/api/v1/reports/monthly` | GET | Cardiologist-ready PDF report |
| `/api/v1/alerts/contacts` | GET/POST | Emergency contact management |
| `/api/v1/config/bp-schedule` | GET/POST | BP measurement schedule |

See `docs/api-spec.md` for full API specification.

---

## 8. ML Pipeline

### 7-Model ML Pipeline

| # | Model | Architecture | Input | Output | Edge/Cloud |
|---|-------|-------------|-------|--------|------------|
| 1 | AFib Detection | 1D CNN (5 conv + 2 FC) | 30 s ECG (250 Hz) | Normal/AFib/PVC/VT/Brady | Edge (Hub, tflite-micro) |
| 2 | PVC/VT Classifier | 1D CNN (3 conv + 2 FC) | 10 s ECG (250 Hz) | PVC/VT/Normal | Cloud |
| 3 | BP Trend LSTM | LSTM (2 layers, 64 units) | 30-day BP history | BP trend + hypertension stage | Cloud |
| 4 | HRV Analysis | Statistical (RMSSD/SDNN/pNN50) | R-R intervals (5 min) | HRV metrics + autonomic state | Cloud |
| 5 | Stroke Risk XGBoost | XGBoost (100 trees) | AFib burden, BP, HRV, CHA₂DS₂-VASc | 30-day stroke risk (%) | Cloud |
| 6 | Sleep Apnea LSTM | LSTM (1 layer, 32 units) | SpO₂ + HR overnight | Apnea risk score | Cloud |
| 7 | POTS Detector | 1D CNN (2 conv + 2 FC) | HR + BP on standing | POTS positive/negative | Cloud |

### Training Data

| Model | Dataset | Samples | Notes |
|-------|---------|---------|-------|
| AFib CNN | MIT-BIH Arrhythmia Database + PhysioNet AFib | 110,000+ ECG segments | 30 s windows at 250 Hz |
| PVC/VT CNN | MIT-BIH + MIT-BIH Malignant Ventricular | 15,000+ PVC/VT beats | 10 s windows |
| BP Trend | MIMIC-III BP records | 50,000+ patient-days | 30-day sequences |
| Stroke Risk | CHA₂DS₂-VASc study data + UK Biobank | 20,000+ patients | AFib burden + BP + HRV features |
| Sleep Apnea | MESA Sleep Study + SHHS | 8,000+ overnight studies | SpO₂ + HR sequences |
| POTS | VTTI POTS dataset + synthetic | 2,000+ tilt-table tests | HR + BP on standing |

See `software/ml-pipeline/` for all training scripts and `docs/architecture.md` for detailed ML architecture.

---

## 9. Mobile App

**React Native** app for iOS + Android:

- **Live Dashboard**: Real-time HR (ECG + PPG), rhythm status (Normal/AFib), SpO₂, latest BP
- **ECG Viewer**: Real-time ECG waveform + event history with ECG strips
- **BP Trends**: BP history with trend lines, hypertension stage color coding
- **HRV Metrics**: RMSSD, SDNN, pNN50 trends, autonomic balance
- **Risk Assessment**: 30-day stroke risk gauge + risk factors breakdown
- **Reports**: Monthly cardiologist-ready PDF download + share
- **Emergency Contacts**: Manage contacts, test alert, emergency alert history
- **Settings**: BP schedule, alert thresholds, firmware updates, device pairing

See `software/mobile-app/` for source.

---

## 10. Bill of Materials

See `hardware/bom/` for per-node BOMs:

| Node | Total Cost | Key Drivers |
|------|-----------|-------------|
| Hub | ~$95 | ESP32-S3, SIM7000, e-ink, speaker, battery |
| ECG Patch | ~$42 | nRF52840, ADS1292R, flex PCB |
| BP Cuff | ~$38 | ESP32-C3, pump, valve, pressure sensor |
| Smart Ring | ~$28 | nRF52833, MAX30102, TMP117, titanium shell |
| **Full System** | **~$203** | One Hub + one ECG Patch + one BP Cuff + one Smart Ring |

---

## 11. Power Architecture

| Node | Battery | Runtime | Charge Method |
|------|---------|---------|---------------|
| Hub | 18650 ×2 (6800 mAh) | 12+ hours (UPS backup) | USB-C 5 V |
| ECG Patch | 245 mAh Li-Po | 14 days continuous ECG | USB-C cradle |
| BP Cuff | 500 mAh Li-Po | ~200 BP measurements (~3 months) | USB-C |
| Smart Ring | 20 mAh Li-Po | 7 days continuous PPG | USB-C cradle |

**Low-power design notes:**

- ECG Patch: nRF52840 in 2 Mbps BLE mode + ADS1292R at 250 Hz = ~7 mA average, 245 mAh / 7 mA = 35 days theoretical, 14 days with safety margin
- Smart Ring: nRF52833 duty-cycled PPG (100 Hz sampling, 5 s HR update) = ~2.8 mA average, 20 mAh / 2.8 mA = 7.1 days
- BP Cuff: ESP32-C3 deep sleep between measurements (~10 μA), ~2.5 mA per measurement, 500 mAh / 2.5 mA per measurement × 1 min = ~200 measurements
- Hub: USB-C powered with 18650 UPS backup for power outage resilience (critical for cardiac monitoring during emergencies)

---

## 12. Enclosure & Mechanical

### ECG Chest Patch

- **Material**: Medical-grade silicone (ISO 10993 biocompatible)
- **Form factor**: 70 × 40 × 8 mm flexible patch
- **Electrodes**: 3× Ambu Blue Sensor snap electrodes, replaceable every 7 days
- **Attachment**: Dual-sided medical adhesive (3M Tegaderm)
- **Weight**: 22 g (with battery)
- **Water resistance**: IP67 (shower-safe, not for swimming)

### BP Wrist Cuff

- **Material**: ABS plastic housing + nylon cuff
- **Form factor**: 65 × 45 × 25 mm housing + 13.5–21.5 cm cuff
- **Wrist mount**: Velcro strap, adjustable
- **Weight**: 85 g (with battery)
- **Water resistance**: IP54 (splash-resistant)

### Smart Ring

- **Material**: PVD-coated titanium (grade 5) shell
- **Sizes**: US 6–13 (half sizes available)
- **Weight**: 4–6 g (size-dependent)
- **Water resistance**: IP68 (10 m, shower + swimming safe)
- **Charging**: USB-C cradle (magnetic contacts)

### Hub

- **Material**: ABS + polycarbonate
- **Form factor**: 120 × 80 × 35 mm wall-mount or tabletop
- **Weight**: 180 g (with batteries)
- **Mounting**: Wall mount + desktop stand
- **Water resistance**: IP40 (indoor only)

---

## 13. Privacy & Security

- **ECG data is medical data.** All ECG data is encrypted in transit (TLS 1.3 for cloud, AES-128-CCM for BLE 5.0) and at rest (AES-256 in TimescaleDB).
- **On-edge inference.** AFib detection runs entirely on the Hub (ESP32-S3 tflite-micro). Raw ECG is only uploaded to cloud for cardiologist reports (user opt-in) or ML model improvement (anonymized).
- **HIPAA-ready.** Cloud backend designed with HIPAA compliance in mind: encrypted storage, audit logging, role-based access, minimum necessary data.
- **BLE 5.0 secure pairing.** All BLE connections use LE Secure Connections (Elliptic Curve Diffie-Hellman). No data is transmitted over unencrypted BLE.
- **Emergency alerts carry minimum data.** Emergency alert contains only: user name, alert type (AFib/VT/Brady), HR, timestamp, and GPS location (if available). No ECG data.
- **User-controlled data sharing.** Cardiologist reports are generated only when the user explicitly requests them. Data is never shared with third parties without consent.
- **Firmware OTA is signed.** All firmware updates are signed with Ed25519; the bootloader verifies the signature before applying.

---

## 14. Build Guide

### Prerequisites

- ESP-IDF v5.1+ (Hub, BP Cuff)
- nRF Connect SDK v2.4+ (ECG Patch, Smart Ring)
- KiCad 8.0+ (schematic editing)
- Python 3.10+ (cloud backend, ML pipeline)
- Node.js 18+ (mobile app)
- Docker + Docker Compose (cloud deployment)

### Hub Assembly

1. Fabricate the 4-layer PCB from `schematic/hub/cardiosync_hub.kicad_sch`
2. Source BOM from `hardware/bom/hub_BOM.csv`
3. Solder: ESP32-S3-WROOM-1-N16R8, SIM7000A, e-ink display, MAX98357A, DRV2605L, SK6812, CC1101, SHT40, DS3231, TPS63020, TP4056, MCP16301, microSD socket, USB-C connector
4. Flash firmware: `idf.py -p /dev/ttyUSB0 flash` (from `firmware/hub/`)
5. Configure Wi-Fi + emergency contacts via mobile app

### ECG Patch Assembly

1. Fabricate flexible polyimide PCB from `schematic/ecg-patch/cardiosync_ecg.kicad_sch`
2. Source BOM from `hardware/bom/ecg_patch_BOM.csv`
3. Solder: nRF52840, ADS1292R, LSM6DSO, TMP117, TPS62743, MCP73831
4. Attach Ag/AgCl electrode snaps
5. Flash firmware: `west build -b nrf52840dk_nrf52840` (from `firmware/ecg-patch/`)
6. Mount in silicone enclosure, attach to chest with medical adhesive

### BP Cuff Assembly

1. Fabricate PCB from `schematic/bp-cuff/cardiosync_bp.kicad_sch`
2. Source BOM from `hardware/bom/bp_cuff_BOM.csv`
3. Solder: ESP32-C3, MP3V5050GP, pump MOSFET, valve MOSFET, LSM6DSO, LM393, TPS63020, MCP73831
4. Connect pump + valve via tubing to wrist cuff
5. Flash firmware: `idf.py -p /dev/ttyUSB0 flash` (from `firmware/bp-cuff/`)
6. Calibrate pressure sensor with reference manometer: `python scripts/calibrate_bp.py`

### Smart Ring Assembly

1. Fabricate rigid-flex PCB from `schematic/smart-ring/cardiosync_ring.kicad_sch`
2. Source BOM from `hardware/bom/smart_ring_BOM.csv`
3. Solder: nRF52833, MAX30102, TMP117, LSM6DSO, TPS62743, nPM1300
4. Flash firmware: `west build -b nrf52833dk_nrf52833` (from `firmware/smart-ring/`)
5. Encapsulate in titanium shell (requires jewelry-grade manufacturing)

### Cloud Deployment

1. `cd software/dashboard && docker-compose up -d`
2. Initialize database: `python scripts/init_db.py`
3. Train ML models: `cd software/ml-pipeline && python train_all.py`
4. Deploy API: `uvicorn main:app --host 0.0.0.0 --port 8000`

---

## 15. Roadmap

### v1.0 (Current)
- Single-lead ECG with AFib CNN (edge)
- PPG HR/HRV/SpO₂ smart ring
- Automated oscillometric BP cuff
- 7-model ML pipeline (cloud)
- Cardiologist-ready PDF reports
- Emergency alert dispatch (Wi-Fi + 4G LTE)

### v1.1
- Multi-lead ECG (Lead I + Lead II) with improved arrhythmia classification
- 12-lead ECG patch variant (clinical-grade, short-term)
- Apple Health + Google Fit integration

### v2.0
- 6-lead ECG patch (Lead I, II, III + augmented)
- Real-time atrial flutter detection
- Wearable defibrillator integration (for high-risk patients)
- Continuous glucose monitor (CGM) bridge (for diabetic cardiac patients)
- FDA 510(k) submission for AFib detection

### v3.0
- Implantable loop recorder bridge (Medtronic Linq)
- AI-guided medication tracking (beta-blockers, anticoagulants)
- Cardiac rehab coaching (post-MI)
- Predictive heart failure model (BNP + weight + HRV)
- Multi-user family cardiac monitoring (one Hub, multiple patches)

---

## License

MIT — build it, sell it, improve it.

---

*Invented as device system #36 for the [Devices](https://github.com/jayis1/Devices) repository. Cardiovascular disease is the #1 killer globally — this system aims to make continuous cardiac monitoring as common as wearing a watch.*