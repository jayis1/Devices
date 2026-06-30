# AsthmaSync — Inhaler Tag Schematic (KiCad)
# nRF52840 + LSM6DSO + CR2032

This folder contains the KiCad schematic project for the AsthmaSync Inhaler Tag.

## Components
- **U1**: nRF52840 QFAA (BLE 5.0 SoC)
- **U2**: LSM6DSO (ST 6-axis IMU, accel + gyro)
- **D1**: Blue LED 0603
- **LS1**: SMD piezo buzzer 10mm
- **SW1**: Tactile button (dose confirm)
- **BT1**: CR2032 battery holder (SMD)

## Power Architecture
- **Battery**: CR2032 (3V, 220mAh) — primary cell
- **No LDO**: nRF52840 runs directly from 3V coin cell (1.7V-3.6V supply range)
- **DCDC**: nRF52840 internal DCDC converter enabled for low-power
- **Battery life target**: 6 months @ 15µA average

## Form Factor
- **PCB**: 18mm diameter, 4-layer, circular FR4
- **Housing**: Injection-molded silicone sleeve that clips onto MDI canister (Ø25mm)

## Low-Power Design
- System OFF mode between actuations
- LSM6DSO wake-up interrupt triggers nRF52840 from sleep
- BLE advertising only after actuation event (not continuous)
- Average current budget:
  - Sleep: 1.5µA (nRF System OFF + LSM6DSO sleep)
  - Active: 8mA for 300ms per actuation
  - BLE TX: 15mA for 10ms per event
  - Estimated: 10-15µA average → 6 months on CR2032