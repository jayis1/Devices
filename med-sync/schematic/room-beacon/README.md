# MedSync - Room Beacon Schematic

## Overview

The room beacon is a compact, coin-cell powered BLE mesh node that detects room occupancy and provides proximity medication reminders.

## Block Diagram

```
┌────────────────────────────────────┐
│        ROOM BEACON PCB             │
│                                     │
│  ┌──────────────┐                  │
│  │  nRF52832     │                  │
│  │  (BLE Mesh    │                  │
│  │   Router +    │                  │
│  │   Sensors)    │                  │
│  │               │                  │
│  │  I2C ──────►│ SHT40 Temp/RH    │
│  │  I2C ──────►│ VEML7700 Light   │
│  │  I2S ──────►│ SPH0645 Mic      │
│  │  GPIO ◄──────│ AM312 PIR       │
│  │  GPIO ──────►│ WS2812B LED     │
│  │  GPIO ──────►│ Piezo Buzzer    │
│  │  GPIO ◄──────│ Push Button     │
│  └──────────────┘                  │
│                                     │
│  Power: CR2477 coin cell (3V)      │
│         direct to nRF52832         │
│         (1.7-3.6V input range)     │
│                                     │
│  Antenna: PCB trace 2.4GHz         │
│                                     │
└────────────────────────────────────┘
```

## Key Design Notes

1. **Ultra-Low Power**: Average 15µA in normal operation. CR2477 coin cell (1000mAh) provides 7.5+ years of battery life.

2. **PIR-Triggered Reminders**: AM312 PIR detects person walking past (5m, ±110°). If medication is due, the LED pulses and buzzer beeps once.

3. **Sound Level**: MEMS microphone samples sound level (dB) to adjust reminder strategy — louder reminders in noisy rooms.

4. **Mesh Routing**: Functions as BLE mesh router, extending network range throughout the home.

## KiCad Project Files

See `room-beacon.kicad_pro`, `room-beacon.kicad_sch`, `room-beacon.kicad_pcb` in this directory.

Note: KiCad project files are not yet generated. Use the pin assignments and BOM from the main README to create the schematic.