# Smart Insole Node Schematic — SoleGuard

## MCU

**nRF52840** — BLE 5.3 mesh + sensor sampling + local feature extraction. Single chip, minimal footprint.

## Block Diagram

```
              ┌──────────────────────────────────────────────┐
              │            SMART INSOLE NODE                  │
              │   (left & right — identical, node_id differs) │
              │                                              │
  Qi RX ────  │  5W receiver ── MCP73831 ── LiPo 350mAh      │
              │  ── 3.0V LDO (TLV743) ── nRF52840             │
              │                                              │
  nRF52840 ── │  ADC0 ── FSR mux (CD74HC4067) ── 24× FSR-402 │
              │  ADC1 ── Thermistor mux (same 4067) ── 8× NTC │
              │  I2C0 ── LSM6DSO32 (IMU)                     │
              │  GPIO0.17-20 ── mux select lines S0-S3       │
              │  GPIO0.21 ── Qi RX status                    │
              │  GPIO0.22 ── battery voltage divider         │
              │  PCB trace antenna ── BLE mesh               │
              └──────────────────────────────────────────────┘
```

## Sensor Layout (plantar surface, 24 FSRs + 8 thermistors)

```
        TOES (row 0, sensors 16-23)
   ┌───────────┬───────────┐
   │ 16 17 18 19│ 20 21 22 23│  ← hallux | lesser toes
   │  (M1)     │  (M2-5)    │
   ├───────────┼───────────┤
   │ 12 13 14 15│  8  9 10 11│  ← metatarsal heads
   ├───────────┼───────────┤
   │  4  5  6  7│  4  5  6  7│  ← midfoot (thermistors on ch 4-7)
   ├───────────┼───────────┤
   │  0  1  2  3│  0  1  2  3│  ← heel (thermistors on ch 0-3)
   └───────────────────────┘
   LEFT half          RIGHT half
   (for left insole, the matrix is the full plantar area)
```

8 thermistors are interleaved at: heel×2, midfoot×2, metatarsal×2, toes×2.

## Pin Assignments — nRF52840 (QFN73)

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | ADC0/AIN0 | FSR mux output (CD74HC4067 Y pin) |
| P0.03 | ADC0/AIN1 | Thermistor mux output |
| P0.04 | I2C0 SDA | LSM6DSO32 |
| P0.05 | I2C0 SCL | LSM6DSO32 |
| P0.06 | SPI0 SCK | (unused — for programming) |
| P0.08 | SPI0 MOSI| (unused) |
| P0.11 | GPIO out | mux S0 (select bit 0) |
| P0.12 | GPIO out | mux S1 |
| P0.13 | GPIO out | mux S2 |
| P0.14 | GPIO out | mux S3 |
| P0.15 | GPIO in  | Qi RX "charging" status |
| P0.17 | ADC0/AIN7| battery voltage (divider 2:1) |
| P0.19 | GPIO out | IMU CS (if SPI; we use I2C so this is INT1) |
| P0.20 | GPIO in  | IMU INT2 (data-ready interrupt) |
| P0.21 | GPIO out | status LED (tiny SMD, under arch) |
| P0.23 | SWDIO    | programming |
| P0.24 | SWCLK    | programming |
| P0.31 | DEC4     | antenna matching |

## FSR Circuit (×24)

Each FSR (Interlink 402, 12.7mm circular) forms a voltage divider with a 10kΩ pull-down:
```
VDD (3.0V) ── FSR ──┬── 10kΩ ── GND
                    └──→ ADC (via mux)
```
Pressure range: ~0.1N (light touch) to ~100N (full body weight zone). ADC 12-bit.

## Thermistor Circuit (×8)

NTC 10kΩ @25°C, B=3977, with 10kΩ pull-up to VDD:
```
VDD ── 10kΩ ──┬── NTC ── GND
              └──→ ADC (via mux, channel 1)
```

## Power

- LiPo 350mAh, 3.7V nominal, 1.5mm thin-pouch (custom cell)
- MCP73831 charger from Qi 5W receiver (5V → 4.2V CC/CV)
- TLV743-3.0 LDO → 3.0V for nRF52840 + sensors
- Average current: ~3mA (burst sampling + sleep) → 14–18h runtime
- Qi charging: 90 min full

## PCB

- 0.1mm PET flex substrate, 2-layer
- FSR pads are exposed copper with pressure-sensitive conductive film overlay
- MCU + battery housed in arch cavity (thickest part of insole, ~5mm)
- IP54 conformal coating

## KiCad Project

`schematic/insole-node/insole-node.kicad_sch` — schematic
`schematic/insole-node/insole-node.kicad_pcb` — flex PCB