# MedSync - Wearable Tag Schematic

## Overview

The wearable tag is a wrist-worn sensor module with pulse oximetry, fall detection, activity tracking, and haptic medication reminders.

## Block Diagram

```
┌──────────────────────────────────────┐
│        WEARABLE TAG PCB              │
│  (40×30mm, fits in silicone band)    │
│                                        │
│  ┌──────────────┐                     │
│  │  nRF52833     │                     │
│  │  (BLE 5.0 +  │                     │
│  │   Proprietary)│                     │
│  │               │                     │
│  │  I2C ──────►│ MAX30101 Pulse Ox  │
│  │  SPI ──────►│ ADXL362 Accel      │
│  │  I2C ──────►│ BMI160 IMU         │
│  │  I2C ──────►│ DRV2605L Haptic    │
│  │  GPIO ──────│ RGB LED             │
│  │  GPIO ◄──────│ User Button        │
│  │  GPIO ──────│ ERM Vibration Motor │
│  └──────────────┘                     │
│                                        │
│  Power: CR2032 coin cell (3V)          │
│         direct to nRF52833             │
│         (1.7-3.6V input range)         │
│                                        │
│  Antenna: PCB trace 2.4GHz            │
│  (meander antenna, -2dBm to +4dBm)    │
│                                        │
│  Optional: NTAG I2C NFC for            │
│  tap-to-confirm on wristband          │
│                                        │
└──────────────────────────────────────┘
```

## Key Design Notes

1. **Ultra-Low Power**: ADXL362 always-on at 1.8µA. BMI160 at 8µA. MAX30101 pulsed every 5 min. Average 30µA total → ~9 months on CR2032.

2. **Fall Detection**: Two-stage approach — ADXL362 hardware activity interrupt wakes MCU, then BMI160 6-axis window is captured for TFLite Micro classification.

3. **Pulse Oximetry**: MAX30101 with red+IR LEDs measures heart rate and SpO2. PPG signal processing runs on nRF52833.

4. **Haptic Feedback**: DRV2605L drives an ERM vibration motor with pre-programmed haptic waveforms for medication reminders.

5. **Form Factor**: 40×30mm PCB fits inside silicone wristband. Battery door on back for easy CR2032 replacement.

## KiCad Project Files

See `wearable-tag.kicad_pro`, `wearable-tag.kicad_sch`, `wearable-tag.kicad_pcb` in this directory.

Note: KiCad project files are not yet generated. Use the pin assignments and BOM from the main README to create the schematic.