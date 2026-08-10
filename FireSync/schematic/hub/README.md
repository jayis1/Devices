# FireSync Hub — Schematic

## MCU: ESP32-S3-WROOM-1-N16R8

- 16 MB flash, 8 MB PSRAM, dual-core 240 MHz
- Wi-Fi 2.4 GHz (cloud connectivity)
- Vector instructions for Dijkstra route computation

## Sub-GHz Radio: SX1262

- 868 MHz LoRa modulation, +22 dBm, SF7, BW 125 kHz
- TDMA mesh coordinator (17 slots × 250 ms)
- SPI interface (MOSI=GPIO12, MISO=GPIO13, SCK=GPIO14, NSS=GPIO15)
- DIO1 interrupt (GPIO16), RST (GPIO17), BUSY (GPIO18)
- External SMA paddle antenna

## Peripherals

| Component | Interface | Pins |
|-----------|-----------|------|
| SIM7000A 4G LTE | UART2 | GPIO22 (TX), GPIO23 (RX), GPIO24 (PWRKEY) |
| BME280 | I²C | GPIO4 (SDA), GPIO5 (SCL) |
| DS3231 RTC | I²C (shared) | GPIO6 (SDA), GPIO7 (SCL) |
| microSD | SPI | GPIO8-11 (MOSI/MISO/SCK/CS) |
| SK6812 RGB LEDs ×3 | RMT | GPIO19 |
| Buzzer 105 dB | PWM (LEDC) | GPIO20 |
| Strobe LED | GPIO | GPIO21 |

## Power

- USB-C 5V input → TPS25940 eFuse → AP2112K-3.3 LDO
- MCP73871 LiPo charger (2000 mAh, 18-hour backup)
- Battery voltage on GPIO25 (ADC)
- Wall power detect on GPIO26

## Antennas

- PCB trace antenna for Wi-Fi/BLE (internal)
- SMA paddle antenna for 868 MHz Sub-GHz (external)
- SMA paddle antenna for 4G LTE (external)

## KiCad Project

Open `hub.kicad_pro` in KiCad 7+. Schematic sheets:
1. `hub MCU.sch` — ESP32-S3 + decoupling + flash
2. `hub Power.sch` — USB-C, MCP73871, TPS25940, LDO, LiPo
3. `hub SubGHz.sch` — SX1262 + SMA + matching network
4. `hub Cellular.sch` — SIM7000A + SIM + SMA
5. `hub Peripherals.sch` — BME280, DS3231, microSD, LEDs, buzzer, strobe