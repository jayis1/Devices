# Vision Hub — Schematic

## MCU: ESP32-S3-WROOM-1-N16R8

- 16 MB flash, 8 MB PSRAM, dual-core 240 MHz
- BLE 5.0 (central role) + Wi-Fi 2.4 GHz
- Vector instructions for OCR inference

## Power
- USB-C 5V input → TPS25940 eFuse → AP2112K-3.3 LDO
- MCP73871 LiPo charger (2000 mAh, portable operation)
- Battery voltage on GPIO17 (ADC)

## Peripherals

| Component | Interface | Pins |
|-----------|-----------|------|
| SIM7000A 4G LTE | UART2 | GPIO14 (TX), GPIO15 (RX), GPIO16 (PWRKEY) |
| BME280 | I²C | GPIO4 (SDA), GPIO5 (SCL) |
| DS3231 RTC | I²C (shared) | GPIO6 (SDA), GPIO7 (SCL) |
| microSD | SPI | GPIO8-11 (MOSI/MISO/SCK/CS) |
| SK6812 RGB LEDs ×3 | RMT | GPIO12 |
| Buzzer | PWM | GPIO13 |

## Antennas
- PCB trace antenna for BLE + Wi-Fi (internal)
- SMA paddle antenna for 4G LTE (external)

## KiCad Project
Open `hub.kicad_pro` in KiCad 7+. Schematic sheets:
1. `hub MCU.sch` — ESP32-S3 + decoupling + flash
2. `hub Power.sch` — USB-C, MCP73871, TPS25940, LDO
3. `hub Cellular.sch` — SIM7000A + SIM + SMA
4. `hub Peripherals.sch` — BME280, DS3231, microSD, LEDs, buzzer