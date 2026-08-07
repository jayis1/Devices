# GuideSync — Schematics

KiCad schematic projects for each GuideSync hardware node.

## Nodes

| Schematic | MCU | Key Components |
|-----------|-----|----------------|
| `hub/` | ESP32-S3-WROOM-1-N16R8 | SIM7000A 4G LTE, BME280, DS3231, microSD, USB-C/PoE |
| `smart-glasses/` | ESP32-S3-WROOM-1-N8R2 | OV5640 camera, VL53L5CX 8×8 ToF, ICM-42688, MAX98357A, ICS-43434, bone conduction |
| `smart-cane/` | nRF52840 QFAA | HC-SR04 ultrasonic, VL53L0X ToF, ICM-42688, DRV2605L |
| `haptic-band/` | nRF52840 QFAA | ICM-42688 (200 Hz), DRV2605L, SOS button |
| `nav-beacon/` | nRF52840 QFAA | CR2032, reed switch, BLE advertising only |

## Design Notes

- **Hub:** 4-layer PCB, USB-C or PoE power input, MCP73871 LiPo charger for portable operation, TPS25940 eFuse for overcurrent protection. SIM7000A on UART2 with SMA antenna.

- **Smart Glasses:** 6-layer flex-rigid PCB conforming to glasses frame. OV5640 on DVP parallel bus (16 pins). VL53L5CX + ICM-42688 on separate I²C buses (I2C_NUM_1) to avoid address conflicts. MAX98357A I²S amplifier drives bone conduction transducers on temple arms. 800 mAh LiPo in temple.

- **Smart Cane:** 4-layer PCB in cane handle. HC-SR04 ultrasonic on GPIO (trigger/echo). VL53L0X angled 45° downward on I²C. DRV2605L haptic driver + ERM motor in handle. 500 mAh LiPo, USB-C charging.

- **Haptic Band:** 4-layer PCB 35×25 mm. ICM-42688 at 200 Hz for fall detection. DRV2605L + ERM motor for nav haptics. Large tactile SOS button (6mm sealed). 300 mAh LiPo. IP67 silicone wristband.

- **Nav Beacon:** 4-layer PCB 35×35 mm disc. nRF52840 in BLE advertising-only mode. CR2032 coin cell (12-18 month life). Reed switch for magnetic config mode. Adhesive wall mount.

## Power Architecture

All nodes use MCP73871 for USB-C LiPo charging (except beacon which uses direct CR2032). AP2112K-3.3 LDO for 3.3V logic. Battery voltage monitored via ADC on all nodes.