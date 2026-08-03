# Bed Mat Schematic

## Overview

The Bed Mat is placed under the mattress and monitors sleep quality and night sweats during menopause. It contains a PVDF piezoelectric strip for ballistocardiography (BCG), an FDC2214 capacitive sensor for mattress moisture detection (night sweats), and a TMP117 for mattress surface temperature.

## SoC: nRF52840 QFAA

- ARM Cortex-M4F @ 64 MHz
- BLE 5.0 (coded PHY for range through mattress)
- 1 MB flash, 256 KB RAM

## Pin Assignments (nRF52840 P0.x)

| Pin | Function | Bus | Notes |
|-----|----------|-----|-------|
| P0.02 | Piezo ADC | ADC | PVDF BCG signal |
| P0.03 | I²C SDA | I²C0 | FDC2214 + TMP117 |
| P0.04 | I²C SCL | I²C0 | Shared I²C |
| P0.05 | FDC2214 INT | GPIO | Moisture data ready |
| P0.06 | TMP117 INT | GPIO | Temp alert |
| P0.07 | LED | GPIO | Status blink |
| P0.08 | Button | GPIO | User button |

## I²C Address Map

| Device | Address | Notes |
|--------|---------|-------|
| TMP117 | 0x48 | Mattress temperature |
| FDC2214 | 0x2A | Capacitive moisture |

## Sensor Details

### PVDF Piezoelectric Strip — Ballistocardiography (BCG)
- 80 cm flexible PVDF piezo film strip
- Placed under mattress at chest level
- Picks up micro-vibrations from heartbeat and breathing
- Cardiac component: 0.5-5 Hz (HR 30-300 bpm)
- Respiratory component: 0.1-0.5 Hz (BR 6-30 breaths/min)
- Signal conditioned via LM358 dual op-amp
- Read via nRF52840 internal 12-bit ADC at 100 Hz
- Sleep staging: awake/light/deep/REM from HR variability + breathing + motion

### FDC2214 — Capacitive Moisture (Night Sweat)
- 28-bit capacitance-to-digital converter
- Capacitive electrode strip on PCB underside
- Detects mattress moisture from night sweats
- Dry baseline calibrated on first use
- Moisture > 35% above baseline → night sweat indicator
- 0.05 Hz sampling (every 20s)

### TMP117 — Mattress Temperature
- ±0.1°C accuracy, medical-grade
- Mattress surface temperature
- Elevated temp (> 36.8°C) + moisture → night sweat confirmation

## Power

- **Battery:** CR2032 220 mAh (replaceable)
- **LDO:** None — direct 3V from CR2032
- **Battery life:** 180 days (1 Hz BCG + 0.05 Hz sweat/temp)

## Block Diagram

```
┌──────────────────────────────────────────────────────────┐
│                BED MAT (nRF52840)                         │
│                                                          │
│  ┌──────────────────┐  ┌──────────┐  ┌──────────┐       │
│  │ PVDF Piezo       │  │ FDC2214  │  │ TMP117   │       │
│  │ Strip 80cm       │  │ Moisture │  │ Mat Temp │       │
│  │ BCG HR/BR/sleep  │  │ 0x2A     │  │ 0x48     │       │
│  │ → LM358 → ADC    │  │ I²C      │  │ I²C      │       │
│  └──────────────────┘  └──────────┘  └──────────┘       │
│                                                          │
│  ┌──────────────────┐  ┌──────────┐                      │
│  │ CR2032           │  │ LED      │                      │
│  │ 220 mAh          │  │ Status   │                      │
│  │ 180-day life     │  │          │                      │
│  └──────────────────┘  └──────────┘                      │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ BLE 5.0 (Coded PHY for range through mattress)   │    │
│  │ PCB Trace Antenna                                 │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ Flexible PVC mat (80×30 cm, waterproof)         │    │
│  │ Placed under mattress at chest level             │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```