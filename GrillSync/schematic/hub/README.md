# Grill Hub Schematic

## Overview

The Grill Hub is the central gateway of the GrillSync system. It coordinates
the Sub-GHz mesh network, bridges to the cloud via Wi-Fi/MQTT, runs local
edge DonenessNet inference, drives the TFT display and LED ring, and
triggers the gas shutoff relay on leak/fire detection.

## SoC: ESP32-S3-WROOM-1-N16R8

- 16 MB flash, 8 MB PSRAM
- Dual-core Xtensa LX7 @ 240 MHz
- Vector instructions for CNN acceleration
- Wi-Fi 2.4 GHz + BLE 5.0

## Pin Assignments

| GPIO | Function | Bus | Notes |
|------|----------|-----|-------|
| GPIO4 | SX1262 DIO1 | Radio IRQ | Interrupt |
| GPIO5 | SX1262 BUSY | Radio | Busy signal |
| GPIO6 | SX1262 NSS | SPI2 CS | Radio CS |
| GPIO7 | SX1262 RST | GPIO | Radio reset |
| GPIO8 | SX1262 SCK | SPI2 | Radio clock |
| GPIO9 | SX1262 MISO | SPI2 | Radio MISO |
| GPIO10 | SX1262 MOSI | SPI2 | Radio MOSI |
| GPIO11 | BME280 SDA | I²C0 | Ambient sensor |
| GPIO12 | BME280 SCL | I²C0 | Ambient sensor |
| GPIO13 | DS3231 SDA | I²C0 | RTC (shared bus) |
| GPIO14 | DS3231 SCL | I²C0 | RTC (shared bus) |
| GPIO15 | SD card MOSI | SPI3 | Logging |
| GPIO16 | SD card MISO | SPI3 | Logging |
| GPIO17 | SD card SCK | SPI3 | Logging |
| GPIO18 | SD card CS | SPI3 CS | Logging |
| GPIO19 | TFT SCK | SPI3 | Display |
| GPIO20 | TFT MOSI | SPI3 | Display |
| GPIO21 | TFT CS | SPI3 CS | Display |
| GPIO35 | TFT DC | GPIO | Display data/command |
| GPIO36 | TFT RST | GPIO | Display reset |
| GPIO37 | TFT BL | PWM | Backlight |
| GPIO38 | LED Ring | WS2812B | 24-LED ring |
| GPIO39 | Gas Relay | GPIO | Gas shutoff (active high) |
| GPIO40 | Buzzer | PWM | Audio alert |
| GPIO41 | Status LEDs | SK6812 | 3× RGB status |
| GPIO43 | USB TX | UART0 | Debug |
| GPIO44 | USB RX | UART0 | Debug |

## Power

- **Input:** USB-C 5V or 12V barrel jack
- **eFuse:** TPS25940 (overcurrent protection)
- **Buck:** MP1584EN (12V → 5V, 3A)
- **LDO:** AMS1117-3.3 (5V → 3.3V)
- **12V direct:** Gas shutoff relay + motorized ball valve

## Block Diagram

```
┌─────────────────────────────────────────────┐
│              ESP32-S3-WROOM-1               │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐       │
│  │ CPU0 │ │ CPU1 │ │ Wi-Fi│ │ BLE  │       │
│  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘       │
│     │        │        │        │            │
│  ┌──┴────────┴────────┴────────┴──┐        │
│  │       GPIO / SPI / I²C          │        │
│  └──┬─────┬─────┬─────┬─────┬─────┘        │
└─────┼─────┼─────┼─────┼─────┼──────────────┘
      │     │     │     │     │
   SPI2   I²C0  SPI3  GPIO  GPIO
      │     │     │     │     │
  ┌───┴──┐ ┌┴──┐ ┌┴──┐ ┌┴──┐ ┌┴──────┐
  │SX1262│ │BME│ │TFT│ │LED│ │Gas    │
  │Radio │ │280│ │LCD│ │Ring│ │Relay  │
  └──────┘ └───┘ └───┘ └───┘ └───────┘
                ┌───┐
                │SD │
                │Card│
                └───┘
```

## Key Design Notes

1. **Gas shutoff relay:** The relay controls a 12V motorized ball valve
   on the propane line. The valve is fail-closed (spring-return), so power
   loss = valve closes = gas off.

2. **LED ring:** 24× WS2812B LEDs arranged in a ring, visible from a
   distance. Color indicates doneness (red→orange→yellow→green) or
   alerts (flashing red = critical).

3. **TFT display:** 2.4" ILI9341 320×240. Shows current temps for all
   probes, doneness countdown, grill surface temp, and safety status.

4. **SD card:** Local cook log buffering during Wi-Fi outage. 7-day
   capacity at 2 Hz telemetry rate.

5. **I²C bus:** BME280 and DS3231 share the same I²C bus at 100 kHz.