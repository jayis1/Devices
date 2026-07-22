# Vocal Band — Schematic

## Block Diagram

```
┌─────────────────────────────────────────────┐
│              nRF52840 QFAA                   │
│        Cortex-M4F 64MHz · BLE 5.0            │
│                                             │
│  P0.02 ── I²S SDA (NAU88C22 audio data)     │
│  P0.03 ── I²S BCLK (audio bit clock)        │
│  P0.04 ── I²S LRCLK (audio word select)     │
│  P0.05 ── Codec I²C SDA (NAU88C22 config)   │
│  P0.06 ── Codec I²C SCL                     │
│  P0.07 ── IMU SDA (LSM6DS3TR-C)             │
│  P0.08 ── IMU SCL                            │
│  P0.09 ── TMP117 SDA (skin temp)            │
│  P0.10 ── TMP117 SCL                          │
│  P0.11 ── PPG INT (MAX30102)                │
│  P0.12 ── PPG SDA                             │
│  P0.13 ── PPG SCL                             │
│  P0.14 ── SK6812 LED                         │
│  P0.15 ── Battery voltage (ADC)              │
│  P0.16 ── USB detect                          │
│  P0.17 ── Codec enable (power gate)         │
│  P0.18 ── Mic enable (power gate)            │
│  P0.19 ── Button (manual rest mark)          │
│  P0.20 ── Status LED                          │
│  P0.21 ── Charger status (MCP73831)         │
└─────────────────────────────────────────────┘
```

## Subcircuits

### Audio Path (Contact Microphone → Codec → MCU)
- **Knowles BU-27135-000** throat/contact microphone
  - Picks up vocal fold vibrations through tissue conduction
  - Frequency response: 20 Hz – 16 kHz
  - Connected to NAU88C22 microphone input (MIC1P/MIC1N)
- **NAU88C22** I²S 24-bit audio codec
  - I²C config: address 0x1A (P0.05/P0.06)
  - I²S data: P0.02 (SDA), P0.03 (BCLK), P0.04 (LRCLK)
  - PGA gain: 0–30 dB (configured via I²C)
  - Sample rate: 16 kHz, 16-bit
  - Power gated via P0.17 (codec enable)
- Mic power gated via P0.18 (mic enable MOSFET)

### IMU (Neck Posture)
- **LSM6DS3TR-C** 6-axis IMU
  - I²C address: 0x6A (P0.07/P0.08)
  - Accelerometer: ±2g, 12.5 Hz low-power mode
  - Gyroscope: ±250 dps
  - Used for neck angle: atan2(ay, sqrt(ax²+az²))

### Skin Temperature
- **TMP117** ±0.1°C digital temperature sensor
  - I²C address: 0x48 (P0.09/P0.10)
  - Vocal cord inflammation proxy
  - Normal vocal cord skin temp: 34.5–35.5°C
  - Elevated >36°C suggests inflammation

### PPG (Heart Rate/HRV)
- **MAX30102** reflective PPG sensor
  - I²C address: 0x57 (P0.12/P0.13)
  - Interrupt: P0.11
  - Red+IR LEDs for heart rate and SpO₂
  - HRV (RMSSD) computed from 30s RR intervals
  - Stress level derived from HRV (low HRV = high stress)

### Power Management
- **MCP73831** USB-C LiPo charger
  - 100 mA charge current
  - STAT pin → P0.21 (charge status)
  - USB power detect: P0.16
- **LiPo 3.7V 250 mAh**
  - 48-hour battery life
  - 30-minute charge time
  - Battery voltage via ADC on P0.15

### Status LED
- SK6812 RGB on P0.14
- Green: normal, Yellow: vocal rest, Red: high risk
- Green LED on P0.20 for charge status

## PCB Layout Notes
- 4-layer board (30×40 mm)
- Audio path: shielded, away from digital noise
- PPG sensor: edge of band, skin contact side
- Contact mic: center of band, skin contact side
- TMP117: adjacent to contact mic for vocal cord temp
- BLE antenna: top edge, keep clear of ground pour
- Flexible PCB section for throat band curvature