# Hub / Gateway Schematic

## Overview

The Hub is the central coordinator of the LawnSync system. It bridges the Sub-GHz mesh network (soil nodes, weather station, sprinkler, scanner) to the cloud via Wi-Fi/MQTT.

## SoC: ESP32-S3-WROOM-1-N16R8

- Dual-core Xtensa LX7 @ 240 MHz
- 16 MB Quad SPI flash
- 8 MB Octal PSRAM
- Wi-Fi 4 (2.4 GHz 802.11 b/g/n)
- BLE 5.0
- 45 GPIOs, rich peripheral set

## Sub-GHz Radio: SX1276IMLTRT

- LoRa modulation, 868 MHz
- +20 dBm max output power
- SPI interface
- DIO0/DIO1/DIO2 interrupt lines

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3-WROOM-1                         │
│                                                             │
│  GPIO4  ─── SX1276 DIO0 (LoRa IRQ)                         │
│  GPIO5  ─── SX1276 DIO1 (CAD IRQ)                          │
│  GPIO6  ─── SX1276 DIO2 (FIFO Full)                        │
│  GPIO7  ─── SX1276 NSS (SPI CS)                            │
│  GPIO8  ─── SX1276 RST                                     │
│  GPIO9  ─── SX1276 SCK (SPI CLK)                           │
│  GPIO10 ─── SX1276 MISO                                    │
│  GPIO11 ─── SX1276 MOSI                                    │
│  GPIO12 ─── BME280 SDA (I²C)                               │
│  GPIO13 ─── BME280 SCL (I²C)                               │
│  GPIO14 ─── DS3231 SDA (I²C shared)                       │
│  GPIO15 ─── DS3231 SCL (I²C shared)                        │
│  GPIO16 ─── SD MOSI                                        │
│  GPIO17 ─── SD MISO                                        │
│  GPIO18 ─── SD SCK                                         │
│  GPIO19 ─── SD CS                                          │
│  GPIO20 ─── SK6812 LED Data                                │
│  GPIO21 ─── Buzzer (PWM)                                   │
│  GPIO43 ─── UART0 TX (USB debug)                           │
│  GPIO44 ─── UART0 RX (USB debug)                           │
│                                                             │
│  3V3  ─── Decoupling: 10µF + 0.1µF on each VDD pin         │
│  EN   ─── 10kΩ pullup + 1µF to GND                         │
│  IO0  ─── 10kΩ pullup + button to GND (boot)               │
│  IO46 ─── 10kΩ pulldown (normal boot from flash)            │
└─────────────────────────────────────────────────────────────┘
         │
         ├── SX1276 (SPI, DIO0-2, RST) + 868 MHz antenna (SMA)
         ├── BME280 (I²C 0x76) — temp/humidity/pressure
         ├── DS3231SN (I²C 0x68) — RTC + CR2032 backup
         ├── MicroSD slot (SPI, pullup on CS)
         ├── SK6812 RGB LED ×3 (status indicators)
         ├── CMT-8543S-SMT buzzer (PWM via NPN transistor)
         ├── USB-C connector (5V power + UART debug)
         ├── TPS25940 eFuse (overcurrent protection)
         └── AP2112K-3.3 LDO (3.3V regulation)
```

## Power Supply

- Input: USB-C 5V (2A) or PoE (IEEE 802.3af via separate PoE splitter)
- TPS25940 eFuse: 5V → overcurrent protection (2.5A limit)
- AP2112K-3.3: 5V → 3.3V (500 mA, sufficient for ESP32-S3 + peripherals)
- Decoupling: 22 µF bulk + 0.1 µF per IC

## Antenna

- 868 MHz whip antenna, SMA connector
- π-network matching (per SX1276 datasheet)
- Antenna placement: >5 mm from PCB edge, ground plane clearance underneath

## PCB Notes

- 4-layer PCB: signal / GND / 3V3 / signal
- SX1276 ground plane directly below radio
- Keep SPI traces short (<50 mm)
- Route I²C away from high-speed signals
- Place decoupling caps close to VDD pins