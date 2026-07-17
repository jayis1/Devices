# Hub / Gateway Schematic

## Overview

The Hub is the central coordinator of the StormSync system. It bridges the Sub-GHz mesh network to the cloud via Wi-Fi/MQTT, with 4G LTE cellular backup for critical flood alerts during internet outages.

## SoC: ESP32-S3-WROOM-1-N16R8

- Dual-core Xtensa LX7 @ 240 MHz
- 16 MB Quad SPI flash
- 8 MB Octal PSRAM
- Wi-Fi 4 (2.4 GHz 802.11 b/g/n)
- BLE 5.0
- 45 GPIOs, rich peripheral set

## Sub-GHz Radio: SX1262IMLTRT

- LoRa modulation, 868 MHz
- +22 dBm max output power
- SPI interface
- DIO1 interrupt line

## Cellular Backup: SIM7000A

- 4G LTE Cat-M1 (low power, long range)
- UART AT command interface
- Embedded nano-SIM holder
- Used for SMS alerts and MQTT when Wi-Fi fails

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3-WROOM-1                         │
│                                                             │
│  GPIO4  ─── SX1262 DIO1 (Radio IRQ)                        │
│  GPIO5  ─── SX1262 BUSY                                    │
│  GPIO6  ─── SX1262 NSS (SPI CS)                            │
│  GPIO7  ─── SX1262 RST                                     │
│  GPIO8  ─── SX1262 SCK (SPI CLK)                           │
│  GPIO9  ─── SX1262 MISO                                    │
│  GPIO10 ─── SX1262 MOSI                                    │
│  GPIO11 ─── BME280 SDA (I²C)                               │
│  GPIO12 ─── BME280 SCL (I²C)                               │
│  GPIO13 ─── DS3231 SDA (I²C shared)                        │
│  GPIO14 ─── DS3231 SCL (I²C shared)                        │
│  GPIO15 ─── SD MOSI                                        │
│  GPIO16 ─── SD MISO                                        │
│  GPIO17 ─── SD SCK                                         │
│  GPIO18 ─── SD CS                                          │
│  GPIO19 ─── SK6812 LED Data                                │
│  GPIO20 ─── Buzzer (PWM)                                   │
│  GPIO21 ─── SIM7000 TX (UART2)                             │
│  GPIO22 ─── SIM7000 RX (UART2)                             │
│  GPIO23 ─── SIM7000 PWRKEY                                  │
│  GPIO43 ─── UART0 TX (USB debug)                           │
│  GPIO44 ─── UART0 RX (USB debug)                           │
│                                                             │
│  3V3  ─── Decoupling: 10µF + 0.1µF on each VDD pin         │
│  EN   ─── 10kΩ pullup + 1µF to GND                         │
│  IO0  ─── 10kΩ pullup + button to GND (boot)               │
│  IO46 ─── 10kΩ pulldown (normal boot from flash)            │
└─────────────────────────────────────────────────────────────┘
         │
         ├── SX1262 (SPI, DIO1, BUSY, RST) + 868 MHz antenna (SMA)
         ├── BME280 (I²C 0x76) — temp/humidity/pressure
         ├── DS3231SN (I²C 0x68) — RTC + CR2032 backup
         ├── SIM7000A (UART2, PWRKEY) + 4G LTE antenna (SMA)
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
- AP2112K-3.3: 5V → 3.3V (500 mA)
- Decoupling: 22 µF bulk + 0.1 µF per IC

## Antennas

- 868 MHz whip antenna, SMA connector
- 4G LTE paddle antenna, SMA connector
- π-network matching per datasheet
- Antenna placement: >5 mm from PCB edge, ground plane clearance

## PCB Notes

- 4-layer PCB: signal / GND / 3V3 / signal
- SX1262 ground plane directly below radio
- Keep SPI traces short (<50 mm)
- Route I²C away from high-speed signals
- SIM7000 UART traces away from radio SPI
- Place decoupling caps close to VDD pins