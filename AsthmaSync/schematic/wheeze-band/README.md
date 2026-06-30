# AsthmaSync — Wheeze Band Schematic (KiCad)
# nRF52840 + SPH0645 + MAX30101 + TMP117 + LSM6DSO + TP4056

This folder contains the KiCad schematic project for the AsthmaSync Wheeze Band.

## Components
- **U1**: nRF52840 QFAA (BLE 5.0 SoC)
- **U2**: SPH0645LM4H-B (Knowles I²S MEMS microphone)
- **U3**: MAX30101 (Maxim PPG sensor — HR/HRV/SpO₂)
- **U4**: TMP117 (TI digital temperature sensor)
- **U5**: LSM6DSO (ST 6-axis IMU for activity context)
- **U6**: TP4056 (USB-C Li-ion charger)
- **M1**: LRA haptic motor (vibrator)
- **D1**: Green LED 0603
- **SW1**: Tactile button (mark event / SOS)
- **BAT1**: 200mAh LiPo (302030)

## Power Architecture
- **Battery**: 200mAh LiPo (3.7V, 4.2V full charge)
- **Charging**: USB-C via TP4056 (500mA charge current)
- **3.3V**: nRF52840 internal DCDC (no external LDO needed)
- **Battery life target**: 36 hours (1.5 days)

## Current Budget
| Component | Active Current | Duty Cycle | Avg Current |
|-----------|---------------|------------|-------------|
| nRF52840 (CPU) | 5.5 mA | 30% | 1.65 mA |
| BLE radio | 15 mA | 2% | 0.30 mA |
| SPH0645 mic | 1.4 mA | 100% | 1.40 mA |
| MAX30101 PPG | 1.2 mA | 25% | 0.30 mA |
| LSM6DSO IMU | 0.9 mA | 10% | 0.09 mA |
| TMP117 | 0.0035 mA | 100% | 0.004 mA |
| **Total** | | | **3.74 mA** |
| **Battery life** | 200mAh / 3.74mA | | **53 hours** |

Conservative estimate with overheads: 36-48 hours.

## PCB Layout Notes
- **Form factor**: 30×25mm wristband PCB
- **Mic placement**: Edge of PCB, facing skin side (body-coupled acoustics)
- **PPG placement**: Center of PCB, skin-facing
- **I²S routing**: Keep traces short (< 20mm), series resistors for signal integrity
- **Antenna**: PCB trace antenna on top edge, keep ground plane clear