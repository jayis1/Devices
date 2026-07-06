# SightSync Schematics

## Overview

Each hardware node has a KiCad schematic file (`.kicad_sch`). These are
text-based KiCad 7+ schematic files containing the netlist, component
placements, and pin assignments.

## Nodes

| Node | File | SoC |
|------|------|-----|
| Vision Hub | `hub/sightsync_hub.kicad_sch` | ESP32-S3 |
| Desk Sentinel | `desk-sentinel/sightsync_desk.kicad_sch` | ESP32-S3 |
| Wearable Eye Tag | `eye-tag/sightsync_eyetag.kicad_sch` | nRF52840 |
| Smart Lamp Node | `lamp-node/sightsync_lamp.kicad_sch` | RP2040 |

## Key Design Notes

- **Vision Hub**: ESP32-S3 with dual SPI buses (SPI2 for CC1101, SPI3 for e-ink).
  I²C for BMP390 + DRV2605L. I²S for MAX98357A audio. GPIO for SK6812 LED ring.
  TP4056 charging circuit for 18650 backup battery.

- **Desk Sentinel**: ESP32-S3 with I²C bus for VL53L1X, VEML7700, TCS34725,
  APDS9306, and SSD1306 OLED. Single SPI bus for CC1101. USB-C power input.

- **Wearable Eye Tag**: nRF52840 with I²C for TMP117 + APDS9306, SPI for LSM6DSO.
  GPIO PWM for IR LED (940 nm). ADC for photodiode. Dual CR2032 with Schottky
  ideal-diode OR-ing for extended battery life.

- **Smart Lamp Node**: RP2040 with SPI for TLC5971 (LED driver) and CC1101.
  I²C for VEML7700 ambient light sensor. GPIO for rotary encoder + button.
  MP1584EN buck converter for 12V-to-3.3V logic power. LEDs driven directly
  from 12V via TLC5971 constant-current channels.

## Opening

Open each `.kicad_sch` file in KiCad 7 or later. The schematics use standard
KiCad symbol libraries plus custom symbols for CC1101, VL53L1X, and TLC5971.