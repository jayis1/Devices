# WanderSync — Schematics

Each hardware node has its own KiCad project in a subfolder:

| Node | MCU | Key Components |
|------|-----|----------------|
| [care-hub](care-hub/) | ESP32-S3-WROOM-1-N16R8 | SX1262 868 MHz, SIM7000A 4G LTE, BME280, DS3231, microSD, LiPo 2000 mAh |
| [wander-band](wander-band/) | nRF52840 QFAA | SX1262 868 MHz, L80-R GPS, LSM6DSL IMU, MAX30101 PPG, DRV2605L haptic, LiPo 300 mAh |
| [door-sentinel](door-sentinel/) | ESP32-C3-WROOM-02-N4 | SX1262 868 MHz, reed switch, motorized deadbolt, H-bridge, 2× CR123A |
| [room-sentinel](room-sentinel/) | ESP32-S3-WROOM-1-N8R2 | SX1262 868 MHz, HLK-LD2410 mmWave, AM612 PIR, LiPo 1200 mAh |
| [voice-node](voice-node/) | ESP32-S3-WROOM-1-N8R2 | SX1262 868 MHz, INMP441 I²S mic, MAX98357A amp, W25Q128 flash, LiPo 1500 mAh |

Open the `.kicad_pro` files in KiCad 7+. Each schematic is split into sheets:
1. MCU + decoupling + flash
2. Power (USB-C, charger, LDO, battery)
3. Sub-GHz radio (SX1262 + matching network + antenna)
4. Peripherals (sensors, actuators, LEDs, buzzer)

Refer to the README.md in each subfolder for pin assignments and design notes.