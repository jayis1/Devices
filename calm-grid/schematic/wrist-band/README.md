# Wrist Band Schematic — CalmGrid

## MCU Architecture

Single SoC: **nRF52840** — handles BLE 5.3 mesh, sensor sampling, and on-device TFLite Micro activity classification.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │           WRIST BAND NODE                   │
                    │           (38×22×10mm, 8g)                  │
                    │                                            │
  LiPo 220mAh ───►  │  RT9013 3.3V LDO ──── all logic            │
                    │                                            │
  nRF52840 ───────  │  I2C0 ── MAX30101 PPG (HR + HRV)          │
                    │  I2C0 ── TMP117 (skin temperature)         │
                    │  I2C1 ── LSM6DSO32 IMU (activity + motion) │
                    │  SPI  ── AD5940 EDA (skin conductance)      │
                    │  ADC  ── battery voltage                    │
                    │  GPIO ── LED green/red                     │
                    │  BLE  ── mesh to hub                       │
                    │                                            │
  EDA electrodes ─► │  Ag/AgCl × 2 → AD5940 TIA → conductance    │
                    │  (excitation on wrist underside)            │
                    │                                            │
  Qi RX coil ────►  │  BQ51013B ── LiPo charger                 │
                    │  (5W wireless, no band removal)             │
                    │                                            │
  Flex PCB ───────  │  0.1mm PET substrate routes sensor traces   │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — nRF52840

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | I2C SDA | MAX30101 PPG + TMP117 |
| P0.03 | I2C SCL | MAX30101 PPG + TMP117 |
| P0.04 | IMU SDA | LSM6DSO32 (separate I2C bus) |
| P0.05 | IMU SCL | LSM6DSO32 |
| P0.06 | SPI CS  | AD5940 EDA |
| P0.07 | SPI SCK | AD5940 EDA |
| P0.08 | SPI MOSI| AD5940 EDA |
| P0.09 | SPI MISO| AD5940 EDA |
| P0.10 | LED green | charging ok |
| P0.11 | LED red | low battery / stress alert |
| P0.12 | Qi RX EN | wireless charging enable |
| P0.13 | VBAT sense | battery voltage divider |
| P0.15 | PPG INT | MAX30101 interrupt |
| P0.20 | IMU INT1 | LSM6DSO32 activity interrupt (wake) |
| P0.28 | EDA excitation | H-tied excitation electrode drive |
| P0.29 | EDA sense+ | TIA positive input (Ag/AgCl) |
| P0.30 | EDA sense− | TIA negative input (Ag/AgCl) |

## EDA Electrode Placement

```
  ┌─────────────────────────────┐
  │    Wrist Band (top view)     │
  │                              │
  │  ┌─────┐    ┌─────┐         │
  │  │ MCU │    │ PPG │         │
  │  └─────┘    └─────┘         │
  │  ┌─────┐    ┌─────┐         │
  │  │ IMU │    │ EDA │         │
  │  └─────┘    └─────┘         │
  │                              │
  │  ═══════════════════════     │  ← strap
  │                              │
  │  ●  ●           ●  ●        │  ← Ag/AgCl electrodes (underside)
  │  E1 E2          (charging   │
  │  (EDA)           coil)       │
  └─────────────────────────────┘
```

Two Ag/AgCl electrodes (E1, E2) on the wrist underside measure skin
conductance. The AD5940 applies a 100mV AC excitation at 0.1Hz and
measures the resulting current via the TIA.

## Power

- 220mAh LiPo (3.7V) → RT9013 3.3V LDO
- Duty cycle: PPG 20s/min @ 100Hz, EDA continuous @ 4Hz, IMU @ 50Hz, BLE TX 1/min
- Deep sleep ~0.3mA; active draw ~9mA (EDA + PPG + IMU)
- Qi 5W wireless charging (BQ51013B) — charges in ~50 min on hub pad
- Low-battery alert at 15%; power-save mode at 5% (IMU-only, 10Hz)

## Physical Design

- PCB: 0.1mm PET flex substrate, 32×18mm active area
- Enclosure: PC/ABS 38×22×10mm, IP67 waterproof, 8g total
- Wrist strap: soft silicone, 20mm wide, adjustable
- PPG sensor: optically isolated against skin using soft silicone pad
- EDA electrodes: Ag/AgCl, 6mm diameter, on underside