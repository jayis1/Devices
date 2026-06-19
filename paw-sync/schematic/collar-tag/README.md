# Collar Tag Schematic — PawSync

## MCU Architecture

Single SoC: **nRF52840** — handles BLE 5.3 mesh, sensor sampling, and on-device TFLite Micro activity classification.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │           COLLAR TAG NODE                   │
                    │           (28×22×10mm, 6g)                  │
                    │                                            │
  LiPo 180mAh ───►  │  RT9013 3.3V LDO ──── all logic            │
                    │                                            │
  nRF52840 ───────  │  I2C0 ── MAX30101 PPG (HR + HRV)           │
                    │  I2C1 ── LSM6DSO32 IMU (activity + gait)   │
                    │  ADC  ── NTC thermistor (skin temp)        │
                    │  GPIO ── LED green/red                     │
                    │  BLE  ── mesh to hub                       │
                    │                                            │
  Qi RX coil ────►  │  BQ51013B ── LiPo charger                 │
                    │  (5W wireless, no collar removal)           │
                    │                                            │
  Flex PCB ───────  │  0.1mm PET substrate routes sensor traces   │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — nRF52840

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | I2C SDA | MAX30101 PPG + thermistor ADC |
| P0.03 | I2C SCL | MAX30101 PPG |
| P0.04 | IMU SDA | LSM6DSO32 (separate I2C bus) |
| P0.05 | IMU SCL | LSM6DSO32 |
| P0.06 | UART RX | ← programming |
| P0.08 | UART TX | → programming |
| P0.09 | BLE antenna | internal |
| P0.10 | LED green | charging ok |
| P0.11 | LED red | low battery / alert |
| P0.12 | Qi RX EN | wireless charging enable |
| P0.13 | VBAT sense | battery voltage divider |
| P0.15 | PPG INT | MAX30101 interrupt |
| P0.20 | IMU INT1 | LSM6DSO32 activity interrupt (wake) |

## Power

- 180mAh LiPo (3.7V) → RT9013 3.3V LDO
- Duty cycle: PPG 20s/min @ 100Hz, IMU continuous @ 50Hz, BLE TX 1/min
- Deep sleep ~0.3mA; active draw ~8mA
- Qi 5W wireless charging (BQ51013B) — charges in ~45 min on hub pad
- Low-battery alert at 15%; power-save mode at 5% (IMU-only, 10Hz)

## Physical Design

- PCB: 0.1mm PET flex substrate, 24×18mm active area
- Enclosure: PC/ABS 28×22×10mm, IP67 waterproof, 6g total
- Collar clip: spring-loaded, fits collars 10-25mm wide
- PPG sensor: optically isolated against fur using soft silicone pad