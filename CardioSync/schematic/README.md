# CardioSync Schematics

All schematics are KiCad 8.0+ projects. Each file is a `.kicad_sch` that can be opened directly in KiCad.

## Files

| File | Node | SoC | Description |
|------|------|-----|-------------|
| `hub/cardiosync_hub.kicad_sch` | Hub | ESP32-S3-WROOM-1-N16R8 | Central coordinator with SIM7000 4G LTE, e-ink, siren, LED ring |
| `ecg-patch/cardiosync_ecg.kicad_sch` | ECG Patch | nRF52840 | 24-bit ECG AFE (ADS1292R), 3 Ag/AgCl electrodes, IMU, flex PCB |
| `bp-cuff/cardiosync_bp.kicad_sch` | BP Cuff | ESP32-C3 | Motorized oscillometric BP cuff with pressure sensor + safety |
| `smart-ring/cardiosync_ring.kicad_sch` | Smart Ring | nRF52833 | Miniaturized PPG ring with MAX30102, TMP117, IMU |

## Block Diagrams

### Hub Block Diagram
```
USB-C 5V ── TPS63020 ── 3.3V
                │
     ESP32-S3-WROOM-1-N16R8
     ┌──────────────────────────────┐
     │  GPIO 4/5/6  → I²S → MAX98357A → Speaker    │
     │  GPIO 8/9   → I²C  → DS3231 + DRV2605L + SHT40 │
     │  GPIO 10-13 → SPI  → CC1101 (Sub-GHz)        │
     │  GPIO 12-16 → SPI  → E-ink 2.9" + microSD    │
     │  GPIO 17/18 → UART2 → SIM7000 4G LTE         │
     │  GPIO 48    → RMT  → SK6812 LED ring (24×)   │
     │  BLE 5.0    → ECG Patch + BP Cuff + Ring     │
     │  Wi-Fi      → Cloud (MQTT)                   │
     └──────────────────────────────┘
                 │
     18650 ×2 ── TP4056 ── MCP16301 ── 5V (UPS backup)
```

### ECG Patch Block Diagram
```
     Ag/AgCl ×3 (RA, LA, RLD)
         │
     ADS1292R (24-bit ECG AFE)
     ┌──────────────────────────────┐
     │  PGA (gain 12) → ΔΣ ADC      │
     │  RLD amplifier → common-mode │
     │  Lead-off detection           │
     └──────────┬───────────────────┘
                │ SPI (2 MHz)
     nRF52840
     ┌──────────────────────────────┐
     │  Pan-Tompkins R-peak detect  │
     │  BLE 5.0 → Hub               │
     │  I²C → LSM6DSO (IMU)         │
     │  I²C → TMP117 (skin temp)    │
     │  ADC → Battery voltage       │
     └──────────────────────────────┘
                │
     245mAh Li-Po → TPS62743 → 3.3V
```

### BP Cuff Block Diagram
```
     MP3V5050GP ── ADC (GPIO 0) ── ESP32-C3
                                         │
     Pump MOSFET (GPIO 2) ←──────────────┤
     Valve MOSFET (GPIO 3) ←─────────────┤
     LM393 comparator (GPIO 6) ──────────┤ (hardware safety: 200 mmHg)
     I²C → LSM6DSO (wrist position) ────┤
                                         │
     BLE 5.0 → Hub                       │
     500mAh Li-Po → TPS63020 → 3.3V      │
                                         │
     Cuff tubing ── Pump ── Valve ── Pressure sensor
```

### Smart Ring Block Diagram
```
     MAX30102 (green+red+IR PPG) ── I²C ── nRF52833
     TMP117 (skin temp)          ── I²C ─┘
     LSM6DSO (activity IMU)      ── I²C ─┘
                                         │
     nRF52833                             │
     ┌──────────────────────────────┐
     │  PPG HR calculation (5 s)    │
     │  SpO₂ calculation (1 min)   │
     │  HRV RMSSD/SDNN (5 min)     │
     │  BLE 5.0 → Hub              │
     └──────────────────────────────┘
                │
     20mAh Li-Po → TPS62743 → 3.3V
     nPM1300 PMIC (charging)
```