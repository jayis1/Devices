# UV Patch Schematic — SkinSync

## MCU Architecture

Single-MCU: **nRF52832** (Sub-GHz mesh + sensor sampling + 14-day coin-cell life) + **SX1262** (Sub-GHz radio).

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │              UV PATCH NODE                   │
                    │              (wrist/shoulder wearable)        │
                    │                                              │
  CR2477 ─────────  │  3.0V coin cell ── nRF52832 (3.0V native)   │
  coin cell          │                                              │
                    │  nRF52832 ─── GPIO/SPI/I2C/ADC              │
                    │  ├── I2C  ── VEML6075 (UVA + UVB sensor)     │
                    │  ├── I2C  ── TMP117 (skin temperature)       │
                    │  ├── I2C  ── LTR390 (UV index + ambient)     │
                    │  ├── I2C  ── DRV2605L (haptic driver)        │
                    │  ├── SPI  ── SX1262 (Sub-GHz radio)          │
                    │  ├── ADC  ── battery voltage divider          │
                    │  ├── GPIO ── LED (pairing + low batt)        │
                    │  └── GPIO ── button (pairing + force TX)     │
                    │                                              │
  SX1262 ─────────  │  Sub-GHz to mirror hub                       │
  + PCB antenna     │  868/915 MHz, +10 dBm, 50m indoor            │
                    │                                              │
  DRV2605L ───────  │  LRA haptic motor (sunburn warning buzz)     │
  + LRA             │                                              │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — nRF52832

| Pin | Function | Notes |
|-----|----------|-------|
| P0.00 | ADC0     | Battery voltage (1/2 divider) |
| P0.01 | GPIO     | LED (pairing indicator) |
| P0.02 | GPIO     | Button (pairing / force TX) |
| P0.03 | I2C SDA  | VEML6075 + TMP117 + LTR390 + DRV2605L |
| P0.04 | I2C SCL  | shared I2C bus |
| P0.05 | GPIO     | DRV2605L GPIO trigger (alternative to I2C) |
| P0.08 | SPI CS   | SX1262 CS |
| P0.09 | SPI CLK  | SX1262 SCK |
| P0.10 | SPI MOSI | SX1262 MOSI |
| P0.11 | SPI MISO | SX1262 MISO |
| P0.12 | GPIO     | SX1262 RST |
| P0.13 | GPIO     | SX1262 BUSY |
| P0.14 | GPIO IRQ | SX1262 DIO1 (TX/RX done) |
| P0.15 | GPIO     | SX1262 antenna switch |

## Power

- CR2477 coin cell (3.0V, 1000 mAh) → nRF52832 runs natively at 3.0V (no LDO needed)
- Active: ~25 mA for 200ms (sample + TX)
- Sleep: ~14 µA (RTC + wake on timer)
- Estimated life: 14 days at 1-min active sampling + 5-min Sub-GHz TX
- Power-gate UV sensors between samples (MOSFET on I2C VCC)

## Sensor Details

### VEML6075 (UVA + UVB)
- I2C address: 0x10
- UVA sensitivity: 320-400nm
- UVB sensitivity: 280-320nm
- Raw counts → UVA/UVB irradiance (W/m²) via calibration coefficients
- Integration time: 50ms (configurable)

### TMP117 (Skin Temperature)
- I2C address: 0x48
- ±0.1°C accuracy, medical-grade
- Mounted on skin-facing side of PCB
- Used for flush/burn detection (>2°C rise from baseline)

### LTR390 (UV Index + Ambient Light)
- I2C address: 0x53
- UV index (0-11+) + ambient lux
- Used for indoor/outdoor detection (lux < 100 = indoor → reduce sampling rate)

## Physical Design

- PCB: 35×25mm 2-layer FR4 (flexible PCB option for wristband integration)
- Enclosure: 35×25×8mm silicone wristband or clip-on
- UV sensors on top face (facing up/out when worn)
- TMP117 on bottom face (skin contact)
- CR2477 in rear compartment (tool-less replacement)
- IP65 rated (splash-proof, not for swimming)
- Hypoallergenic silicone band