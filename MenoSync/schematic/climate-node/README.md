# Climate Node Schematic

## Overview

The Climate Node is a room-mounted environmental sensor and actuator that monitors ambient conditions and controls HVAC + smart shades for pre-emptive cooling during menopause hot flashes. It contains a BME280 for ambient sensing, an MLX90640 thermal IR array for radiant temperature mapping, and dual relay outputs for HVAC and shade control.

## SoC: ESP32-C3-WROOM-02

- Single-core RISC-V @ 160 MHz
- Ultra-low-cost, Wi-Fi capable (not used — Sub-GHz only)
- 4 MB flash

## Pin Assignments (ESP32-C3)

| GPIO | Function | Bus | Notes |
|------|----------|-----|-------|
| GPIO2 | Relay HVAC | GPIO | HVAC control relay |
| GPIO3 | Relay Shade | GPIO | Smart shade/curtain relay |
| GPIO4 | I²C SDA | I²C0 | BME280 + MLX90640 |
| GPIO5 | I²C SCL | I²C0 | Shared I²C |
| GPIO6 | MLX INT | GPIO | MLX90640 interrupt |
| GPIO7 | LED | SK6812 | Status |
| GPIO7 | RFM CS | SPI CS | Sub-GHz radio |
| GPIO8 | RFM SCK | SPI | Sub-GHz radio |
| GPIO9 | RFM MOSI | SPI | Sub-GHz radio |
| GPIO10 | RFM MISO | SPI | Sub-GHz radio |
| GPIO11 | RFM RST | GPIO | Sub-GHz radio reset |
| GPIO12 | RFM DIO0 | GPIO Int | Sub-GHz radio interrupt |

## I²C Address Map

| Device | Address | Notes |
|--------|---------|-------|
| BME280 | 0x76 | Ambient temp/humidity/pressure |
| MLX90640 | 0x33 | 32×24 thermal IR array |

## Sensor Details

### BME280 — Ambient Environmental
- Temperature ±1°C
- Humidity ±3% RH
- Pressure ±1 hPa
- 0.1 Hz sampling (every 10s)
- Ambient temp > 26°C is a hot flash trigger risk

### MLX90640 — Thermal IR Array
- 32×24 pixel thermal infrared sensor
- Measures radiant temperature (NOT contact temp)
- Range: -40 to 300°C, accuracy ±1°C
- Refresh rate: 0.5 Hz (reported at 0.02 Hz)
- Use cases:
  - Detect uneven cooling (HVAC effectiveness)
  - Detect drafts (cold spots)
  - Detect sunlight hotspots (shade trigger)
  - Map room thermal environment

### Relay Outputs
- **HVAC relay:** Controls HVAC system (cool/heat/fan via 24V thermostat interface or direct relay)
- **Shade relay:** Controls motorized smart shades/curtains (open/close via time-based relay control)

## Power

- **Input:** USB-C 5V or 5V solar panel (1W)
- **Charger:** TP4056 (USB-C + solar charging management)
- **LDO:** AMS1117-3.3 (5V → 3.3V)
- **Solar option:** 5V 1W mini panel for wire-free placement

## Block Diagram

```
┌──────────────────────────────────────────────────────────┐
│              CLIMATE NODE (ESP32-C3)                      │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │
│  │ BME280   │  │ MLX90640 │  │ RFM69HCW │  │ Relay×2  │ │
│  │ Ambient  │  │ 32×24 IR │  │ 868 MHz  │  │ HVAC +   │ │
│  │ T/RH/P   │  │ Radiant  │  │ Sub-GHz  │  │ Shade    │ │
│  │ I²C 0x76 │  │ I²C 0x33 │  │ SPI      │  │ GPIO     │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ USB-C    │  │ TP4056   │  │ SK6812   │               │
│  │ 5V Power │  │ Charger  │  │ LED      │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                                                          │
│  ┌──────────┐                                             │
│  │ Solar    │  (optional wire-free power)                 │
│  │ 5V 1W    │                                             │
│  └──────────┘                                             │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ 868 MHz wire antenna (500m LOS, wall-penetrating)│    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │ Wall/ceiling mount enclosure (60×40×20mm)        │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```