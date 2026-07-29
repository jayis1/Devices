# Pressure Mat Schematic

## Overview

The Pressure Mat is a floor mat with a 16×16 (256 sensor) FSR array that measures plantar pressure distribution for balance and weight-bearing exercises. It scans the array at 30 Hz, computes center-of-pressure and asymmetry, and transmits to the Hub via SX1262 Sub-GHz 868 MHz.

## SoC: ESP32-S3-WROOM-1-N8

- 8 MB flash, 512 KB SRAM
- Dual-core Xtensa LX7 @ 240 MHz

## Pin Assignments

| GPIO | Function | Bus | Notes |
|------|----------|-----|-------|
| GPIO4 | SX1262 DIO1 | Radio IRQ | Interrupt |
| GPIO5 | SX1262 BUSY | Radio | Busy |
| GPIO6 | SX1262 NSS | SPI2 CS | Radio CS |
| GPIO7 | SX1262 RST | GPIO | Radio reset |
| GPIO8 | SX1262 SCK | SPI2 | Radio clock |
| GPIO9 | SX1262 MISO | SPI2 | Radio MISO |
| GPIO10 | SX1262 MOSI | SPI2 | Radio MOSI |
| GPIO11 | I²C SDA | I²C0 | ADS1115 |
| GPIO12 | I²C SCL | I²C0 | ADS1115 |
| GPIO13 | MUX Row S0 | GPIO | Row mux select bit 0 |
| GPIO14 | MUX Row S1 | GPIO | Row mux select bit 1 |
| GPIO15 | MUX Row S2 | GPIO | Row mux select bit 2 |
| GPIO16 | MUX Row S3 | GPIO | Row mux select bit 3 |
| GPIO17 | MUX Col S0 | GPIO | Col mux select bit 0 |
| GPIO18 | MUX Col S1 | GPIO | Col mux select bit 1 |
| GPIO19 | MUX Col S2 | GPIO | Col mux select bit 2 |
| GPIO20 | MUX Col S3 | GPIO | Col mux select bit 3 |
| GPIO40 | ADC Alert | GPIO | ADS1115 alert |
| GPIO41 | IMU INT1 | GPIO | LSM6DSO interrupt |
| GPIO42 | Status LED | SK6812 | RGB status |

## FSR Matrix Design

- **Sensors:** 256× Interlink FSR 400 series (0.2-2 N range)
- **Matrix:** 16 rows × 16 columns
- **Row MUX:** 4× CD74HC4067 16:1 analog multiplexer (4 rows read in parallel)
- **Column MUX:** 1× CD74HC4067 16:1 (grounds one column at a time)
- **ADC:** ADS1115 16-bit, 4-channel, I²C, 860 SPS
- **Scan sequence:** 16 columns × 4 ADC channels (4 rows in parallel) = 64 reads per frame
- **Frame rate:** ~13 Hz with ADS1115 (30 Hz achievable with ESP32 ADC for fast mode)

## Power

- **Input:** USB-C 5V (always powered — mat sits on floor)
- **LDO:** AMS1117-3.3 (5V → 3.3V)
- **No battery** — USB-C powered

## Block Diagram

```
┌────────────────────────────────────────────────────────────┐
│               PRESSURE MAT (ESP32-S3)                      │
│                                                            │
│  ┌──────────────────────────────────────────────────┐      │
│  │  16×16 FSR Matrix (256 sensors)                 │      │
│  │  Interlink FSR 400 series                       │      │
│  │  600mm × 400mm active area                      │      │
│  └──────────────────────────────────────────────────┘      │
│         │ Row signals        │ Column signals               │
│  ┌──────┴──────────┐  ┌─────┴──────────┐                   │
│  │ 4× CD74HC4067   │  │ 1× CD74HC4067  │                   │
│  │ 16:1 Row MUX    │  │ 16:1 Col MUX   │                   │
│  │ (parallel read) │  │ (col ground)   │                   │
│  └────────┬────────┘  └────────────────┘                   │
│           │ 4 analog channels                                │
│  ┌────────┴────────┐                                       │
│  │ ADS1115         │                                       │
│  │ 16-bit ADC      │                                       │
│  │ I²C, 860 SPS   │                                       │
│  └────────┬────────┘                                       │
│           │ I²C                                              │
│  ┌────────┴────────────────────────────────────────┐       │
│  │           ESP32-S3-WROOM-1-N8                   │       │
│  │  Dual-core 240 MHz, 8MB flash                  │       │
│  └────────┬──────────────────────┬────────────────┘       │
│           │ SPI2                  │ GPIO                    │
│  ┌────────┴────────┐    ┌────────┴────────┐               │
│  │ SX1262          │    │ MUX Select Logic│               │
│  │ Sub-GHz 868 MHz │    │ GPIO13-GPIO20  │               │
│  └────────┬────────┘    └────────────────┘               │
│           │                                                 │
│  ┌────────┴────────┐                                       │
│  │ 868 MHz Antenna │                                       │
│  └─────────────────┘                                       │
│                                                            │
│  Power: USB-C 5V → AMS1117-3.3 → 3.3V                     │
│  Enclosure: 600mm × 400mm × 8mm rigid mat                 │
└────────────────────────────────────────────────────────────┘
```