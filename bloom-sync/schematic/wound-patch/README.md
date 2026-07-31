# Wound Patch Schematic

## Overview

The Wound Patch is a medical-grade adhesive patch placed over a C-section incision or perineal tear. It monitors wound healing for early infection detection using a TMP117 temperature sensor (inflammation), FDC2214 capacitive moisture sensor (exudate), and LMP91200 pH analog front-end (bacterial growth indicator).

## SoC: nRF52840 QFAA

- ARM Cortex-M4F @ 64 MHz
- BLE 5.0, 1 MB flash, 256 KB RAM

## Pin Assignments (nRF52840 P0.x)

| Pin | Function | Bus | Notes |
|-----|----------|-----|-------|
| P0.02 | TMP117 INT | GPIO | Wound temp alert |
| P0.03 | I²C SDA | I²C0 | TMP117 + FDC2214 |
| P0.04 | I²C SCL | I²C0 | Shared I²C |
| P0.05 | FDC2214 INT | GPIO | Moisture data ready |
| P0.06 | LMP91200 Alert | GPIO | pH threshold alert |
| P0.07 | pH ADC | ADC | LMP91200 analog output |
| P0.08 | LED | GPIO | Status blink |
| P0.09 | Button | GPIO | User button |

## I²C Address Map

| Device | Address | Notes |
|--------|---------|-------|
| TMP117 | 0x48 | Wound temperature |
| FDC2214 | 0x2A | Capacitive moisture |

## Power

- **Battery:** CR2032 220 mAh (replaceable)
- **LDO:** None — direct 3V from CR2032
- **Battery life:** 21 days (covers full wound healing window)

## Sensor Details

### TMP117 — Wound Temperature
- ±0.1°C accuracy, medical-grade
- Digital I²C, 16-bit resolution
- Resolution: 0.0078°C/LSB
- Measures local inflammation at wound site

### FDC2214 — Capacitive Moisture
- 28-bit capacitance-to-digital converter
- Measures wound exudate moisture level
- Capacitive sensor electrode on PCB underside
- Dry baseline calibrated on first use
- Moisture > 80% indicates excessive exudate

### LMP91200 — pH Sensor
- Analog pH front-end for glass electrode
- pH range: 0-14, resolution 0.1 pH
- Normal wound pH: 5.5-6.8
- Elevated pH > 7.5 indicates bacterial growth
- Miniature glass combination electrode

## Block Diagram

```
┌──────────────────────────────────────────────────────────┐
│               WOUND PATCH (nRF52840)                      │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ TMP117   │  │ FDC2214  │  │ LMP91200 │               │
│  │ Temp     │  │ Moisture │  │ pH       │               │
│  │ 0x48     │  │ 0x2A     │  │ Analog   │               │
│  │ I²C      │  │ I²C      │  │ ADC      │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                                                          │
│  ┌──────────────────┐  ┌──────────┐  ┌──────────┐       │
│  │ pH Glass         │  │ CR2032   │  │ LED      │       │
│  │ Electrode        │  │ 220 mAh  │  │ Status   │       │
│  └──────────────────┘  └──────────┘  └──────────┘       │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ BLE 5.0 (Coded PHY for range through clothing)   │    │
│  │ PCB Trace Antenna                                 │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ Sterile adhesive patch (waterproof outer layer)  │    │
│  │ Medical-grade skin contact (21-day wear)         │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```