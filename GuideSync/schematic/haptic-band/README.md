# Haptic Band — Schematic

## MCU: nRF52840 QFAA
- Cortex-M4F 64 MHz, BLE 5.0
- Ultra-low power (48+ hour battery life)

## Power
- LiPo 3.7V 300 mAh (48+ hour autonomy)
- MCP73871 USB-C charger
- AP2112K-3.3 LDO
- Battery voltage on P0.09 (AIN9)

## Peripherals

| Component | Interface | Pins |
|-----------|-----------|------|
| ICM-42688 IMU (200 Hz) | I²C | P0.02 (SCL), P0.03 (SDA) |
| DRV2605L Haptic | I²C | P0.04 (SCL), P0.05 (SDA) |
| SOS Button | GPIO (IRQ) | P0.06 |
| IMU INT1 | GPIO (IRQ) | P0.07 |
| IMU INT2 | GPIO (IRQ) | P0.08 |
| Motor Enable | GPIO | P0.10 |
| Status LED | GPIO | P0.11 |

## Physical Design
- 4-layer PCB 35×25 mm
- IP67 silicone wristband enclosure (<25 g)
- Large textured SOS button (6mm sealed, tactile-distinguishable)
- ERM motor on inner wrist surface

## KiCad Project
Open `haptic-band.kicad_pro` in KiCad 7+. Schematic sheets:
1. `band MCU.sch` — nRF52840 + decoupling
2. `band IMU.sch` — ICM-42688 (200 Hz accel for fall detection)
3. `band Haptic.sch` — DRV2605L + ERM motor
4. `band SOS.sch` — Tactile button + debounce
5. `band Power.sch` — MCP73871 + LDO + LiPo