# Smart Cane — Schematic

## MCU: nRF52840 QFAA
- Cortex-M4F 64 MHz, 1 MB flash, 256 KB RAM
- BLE 5.0 (peripheral to hub)

## Power
- LiPo 3.7V 500 mAh (20+ hour autonomy)
- MCP73871 USB-C charger
- AP2112K-3.3 LDO
- Battery voltage on P0.11 (AIN11)

## Peripherals

| Component | Interface | Pins |
|-----------|-----------|------|
| HC-SR04 Ultrasonic | GPIO | P0.08 (TRIG), P0.09 (ECHO) |
| VL53L0X Downward ToF | I²C | P0.02 (SCL), P0.03 (SDA) |
| ICM-42688 IMU | I²C | P0.04 (SCL), P0.05 (SDA) |
| DRV2605L Haptic | I²C | P0.06 (SCL), P0.07 (SDA) |
| Status LED | GPIO | P0.13 |
| Motor Enable | GPIO | P0.15 |

## Physical Design
- Electronics in cane handle (PCB) + sensor pod near tip
- VL53L0X angled 45° downward for ground-level hazard detection
- HC-SR04 forward-facing for obstacle detection
- ERM haptic motor in handle for vibration feedback
- Carbon fiber folding cane shaft (5-section)

## KiCad Project
Open `smart-cane.kicad_pro` in KiCad 7+. Schematic sheets:
1. `cane MCU.sch` — nRF52840 + decoupling
2. `cane Ultrasonic.sch` — HC-SR04 + level shifter
3. `cane ToF.sch` — VL53L0X + I²C
4. `cane IMU.sch` — ICM-42688
5. `cane Haptic.sch` — DRV2605L + ERM motor
6. `cane Power.sch` — MCP73871 + LDO + LiPo