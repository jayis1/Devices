# PestSync — Assembly Guide

## Overview

This guide covers assembling all 4 PestSync node types from components.

## Tools Required

- Soldering iron (fine tip, 0.5mm)
- Solder wire (0.6mm, lead-free or leaded)
- Flux
- Multimeter
- Tweezers
- 3D printer (PETG filament)
- M2/M3 hardware kit
- USB-C cable
- Computer with ESP-IDF v5.1+ installed

## 1. Hub Assembly

### Components
See `hardware/bom/hub_bom.csv`

### Steps
1. **Order PCB**: Send `schematic/hub/hub.kicad_pcb` gerbers to JLCPCB or PCBWay
2. **Solder components** (order: lowest profile first):
   - Solder ESP32-WROOM-32E module
   - Solder SX1262 module (SPI)
   - Solder AP2112 LDO, TP4056, DW01
   - Solder SSD1306 OLED (I2C)
   - Solder BME280 breakout
   - Solder microSD socket
   - Solder WS2812B LED, USB-C, buttons, headers
   - Solder 18650 battery holder
3. **Print enclosure**: `hardware/hub_enclosure.stl` (PETG)
4. **Flash firmware**: `scripts/flash_all.sh hub`
5. **Test**: Power via USB-C, verify OLED displays "PestSync Hub"

## 2. Pest Sentinel Assembly

### Components
See `hardware/bom/pest-sentinel_bom.csv`

### Steps
1. **Order PCB**: 4-layer (camera bus impedance control). Send gerbers to JLCPCB (4-layer option)
2. **Solder components**:
   - Solder ESP32-S3-N8R2 module
   - Solder OV2640 camera connector (FPC)
   - Solder MLX90640 thermal array (careful: I2C at 400 kHz)
   - Solder SX1262 module
   - Solder AM312 PIR sensor
   - Solder 2× 850nm IR LEDs + MOSFET driver
   - Solder TP4056 + DW01 + 18650 holder
   - Solder USB-C, WS2812B, button
3. **Print enclosure**: `hardware/sentinel_enclosure.stl` with IR-transparent window
4. **Flash firmware**: `scripts/flash_all.sh sentinel`
5. **Test**: Cover PIR → camera should capture → LED flash → Sub-GHz TX to Hub

## 3. Smart Trap Assembly

### Components
See `hardware/bom/smart-trap_bom.csv`

### Steps
1. **Order PCB**: 2-layer FR4
2. **Solder components**:
   - Solder ESP32-C3 module
   - Solder SX1262 module
   - Solder HX711 + load cell connector
   - Solder ADXL362 (SPI, careful with QFN package)
   - Solder reed switch + magnet
   - Solder capacitive bait sensor probe
   - Solder TPS61099 boost converter
   - Solder AA battery holder, LED, USB-C
3. **Print enclosure**: `hardware/trap_adapter.stl` — retrofits onto snap trap
4. **Flash firmware**: `scripts/flash_all.sh trap`
5. **Calibrate load cell**: `scripts/calibrate_trap.py`
6. **Test**: Snap trap trigger → LED red → Hub receives trap event

## 4. Deterrent Node Assembly

### Components
See `hardware/bom/deterrent-node_bom.csv`

### Steps
1. **Order PCB**: 2-layer FR4
2. **Solder components**:
   - Solder ESP32-C3 module
   - Solder SX1262 module
   - Solder 2× piezo ultrasonic transducers + MOSFET drivers
   - Solder strobe LED + MOSFET
   - Solder piezo atomizer disc + reservoir
   - Solder capacitive oil level sensor
   - Solder TP4056 + DW01 + 18650 holder
   - Solder USB-C, WS2812B, button
3. **Print enclosure**: `hardware/deterrent_enclosure.stl` with oil refill port
4. **Fill reservoir**: Add peppermint essential oil (~5 mL)
5. **Flash firmware**: `scripts/flash_all.sh deterrent`
6. **Test**: Button press → ultrasonic emission → Hub receives status

## Network Setup

1. **Hub**: Power on, connect to WiFi via mobile app (BLE commissioning)
2. **Pest Sentinels**: Power on, join Sub-GHz mesh automatically (pre-paired to Hub)
3. **Smart Traps**: Power on, join Sub-GHz mesh, LED green = armed
4. **Deterrent Nodes**: Power on, join Sub-GHz mesh, default mode = adaptive

## Placement Guide

| Location | Node Type | Notes |
|----------|-----------|-------|
| Behind stove/fridge | Pest Sentinel | Cockroach pathway, low to ground |
| Under sink | Pest Sentinel + Trap | Rodent entry point |
| Garage threshold | Pest Sentinel | Rodent entry, spiders |
| Pantry corner | Pest Sentinel + Deterrent | Food pest detection |
| Along walls | Smart Traps | Pests travel along edges |
| Attic/basement | Deterrent Node | Ultrasonic needs enclosed space |
| Central location | Hub | WiFi range, USB-C power |

## Calibration

### Load Cell (Smart Trap)
```bash
python scripts/calibrate_trap.py --port /dev/ttyUSB0
# Place known weight (e.g., 20g) on trap
# Script records HX711 reading and calculates scale factor
```

### Moisture/Bait Sensor
```bash
python scripts/calibrate_trap.py --bait --port /dev/ttyUSB0
# Empty: record ADC value (air)
# Full bait: record ADC value
# Script sets calibration constants
```

### Camera (Pest Sentinel)
```bash
python scripts/calibrate_camera.py --port /dev/ttyUSB0
# Adjusts OV2640 exposure, gain, white balance
# Captures test frames for ML model verification
```