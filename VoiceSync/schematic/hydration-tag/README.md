# Hydration Tag — Schematic

## Block Diagram

```
┌─────────────────────────────────────────────┐
│              nRF52840 QFAA                   │
│        Cortex-M4F 64MHz · BLE 5.0            │
│                                             │
│  P0.02 ── HX711 DOUT (load cell data)      │
│  P0.03 ── HX711 SCK (load cell clock)       │
│  P0.04 ── IMU SDA (LIS2DW12)                │
│  P0.05 ── IMU SCL                            │
│  P0.06 ── IMU INT1 (motion interrupt)       │
│  P0.07 ── SK6812 LED                        │
│  P0.08 ── Battery voltage (ADC)             │
│  P0.09 ── Button (manual sip mark)          │
│  P0.10 ── Status LED                         │
│  P0.11 ── HX711 RATE                         │
│  P0.12 ── HX711 GAIN                         │
└─────────────────────────────────────────────┘
```

## Subcircuits

### Load Cell + HX711 (Water Mass)
- **1 kg bar load cell** (strain gauge)
  - Mounted in bottle base ring
  - Measures total mass (bottle + water)
  - ±0.1 g resolution after calibration
  - Tare offset subtracts empty bottle weight
- **HX711** 24-bit load cell ADC
  - DOUT → P0.02, SCK → P0.03
  - RATE → P0.11 (10 Hz / 80 Hz select)
  - GAIN → P0.12 (channel/gain select)
  - Programmable gain: 128 (Channel A)
  - VCC: 3.0V from CR2032 (HX711 supports 2.7V-5.5V)
  - Excitation voltage: AVDD ≈ 4.2V (internal boost)

### IMU (Sip Detection)
- **LIS2DW12** 3-axis accelerometer
  - I²C address: 0x19 (P0.04/P0.05)
  - ±2g range, 12.5 Hz low-power mode
  - INT1 → P0.06 (wake-on-motion interrupt)
  - Detects bottle lift (acceleration > 0.5g)
  - Detects tilt > 30° (drinking position)
  - Ultra-low power: 1 µA in low-power mode

### Power
- **CR2032** 3V lithium coin cell
  - 6-month battery life
  - Ultra-low duty cycle: sample 10s, TX 15 min
  - Battery voltage via ADC on P0.08
  - nRF52840 in System OFF mode between samples

### Status LED
- SK6812 RGB on P0.07
- Green: good hydration, Yellow: needs water, Red: dehydrated
- Green LED on P0.10 for BLE status

## PCB Layout Notes
- 2-layer board (30×30 mm)
- Load cell mounted at center of bottle ring
- IMU at edge for best motion sensitivity
- CR2032 holder on top side
- BLE antenna: keep clear of ground pour
- Conformal coating for water resistance