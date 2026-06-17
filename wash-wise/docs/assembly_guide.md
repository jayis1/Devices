# WashWise — Assembly Guide

## Overview

WashWise consists of 4 nodes. **Install the dryer node first** — it provides
fire safety monitoring that could save your home.

## Parts List

| Node | Key Components | Est. Cost |
|------|---------------|-----------|
| Hub | RP2040 + ESP32-C6, SX1262, ILI9341 TFT, LiPo 2000mAh | ~$22 |
| Washer | ESP32-S3, SX1261, ADXL313, YF-S201, HX711, peristaltic pump | ~$18 |
| Dryer | ESP32-S3, SX1261, MPXV7002DP, MAX6675, K-type, SHT40 | ~$16 |
| Scanner | ESP32-S3, SX261, OV2640, UV/IR LEDs, ST7789, LiPo 1000mAh | ~$14 |

## Step 1: Hub Node

### Assembly
1. Solder all components per schematic (`schematic/hub-node/`)
2. Flash firmware: `firmware/hub-node/hub_main.c` (RP2040) + ESP32-C6 WiFi bridge
3. Connect 868MHz antenna to SMA connector
4. Insert LiPo battery (backup power)
5. Connect USB-C power

### Placement
- Laundry room, central location
- Wall-mount or shelf (within 10m of washer + dryer)
- Connect to WiFi via mobile app (BLE pairing)

### Verification
- TFT displays "WashWise Hub v1.0"
- Status LED: solid green
- Display shows 0 active nodes (until others are installed)

---

## Step 2: Dryer Node (INSTALL FIRST — Fire Safety!)

### Assembly
1. Solder all components per schematic (`schematic/dryer-node/`)
2. Flash firmware: `firmware/dryer-node/dryer_main.c`
3. Connect 868MHz antenna
4. Insert LiPo 500mAh battery (backup — critical for fire safety)
5. Connect USB-C power

### Installation (Non-Invasive)
1. **Differential pressure taps:**
   - Drill two 4mm holes in exhaust duct (one before lint trap, one after)
   - Insert silicone tubing fittings (rated to 200°C)
   - Connect to MPXV7002DP pressure sensor ports
   - Seal with high-temp silicone

2. **K-type thermocouple:**
   - Tape junction to exterior of exhaust duct (near outlet)
   - Use high-temp foil tape (rated to 260°C)
   - Route thermocouple wire away from hot surfaces

3. **Current sensor (ACS712):**
   - Clamp onto dryer power cord (non-invasive split-core)
   - Position away from motor/electronics

4. **Humidity sensor (SHT40):**
   - Mount in exhaust stream (protected from direct heat)
   - Use small perforated enclosure

5. **Vibration sensor (ADXL313):**
   - Magnetic mount on dryer side panel

6. **USB-C power:** Route cable to outlet

### Verification
- Node appears on hub TFT (1 active node)
- App shows dryer node: exhaust temp, pressure, humidity
- Fire risk gauge shows "OK" (green)

### ⚠️ Safety Notes
- **Never** modify the dryer's internal wiring
- **Never** place electronics inside the dryer
- **Always** use heat-rated materials near exhaust
- Test the fire alarm: briefly block exhaust → pressure rises → alarm triggers

---

## Step 3: Washer Node

### Assembly
1. Solder all components per schematic (`schematic/washer-node/`)
2. Flash firmware: `firmware/washer-node/washer_main.c`
3. Connect 868MHz antenna
4. Connect 12V DC (pump power) and USB-C (MCU power)

### Installation
1. **Vibration sensor (ADXL313):** Magnetic mount on washer cabinet
2. **Current sensor (ACS712):** Clamp on washer power cord
3. **Flow sensor (YF-S201):** Splice into cold water fill hose
   - Use quick-connect fittings
   - Or use external non-invasive ultrasonic flow sensor
4. **Water temperature (DS18B20):** Tape to fill hose exterior
5. **Detergent reservoir + load cell:** Place on platform beside washer
6. **Peristaltic pump:** Output tube → washer dispenser drawer
7. **Humidity sensor (SHT40):** Mount under washer (leak detection)

### Detergent Reservoir Setup
1. Fill reservoir with liquid detergent
2. Place on load cell platform
3. Calibrate load cell (see `scripts/calibrate_sensors.py`)
4. Connect pump output to washer dispenser drawer with tubing
5. Prime pump (run `scripts/prime_pump.py`)

### Verification
- Node appears on hub TFT (2 active nodes)
- Run a wash cycle → verify cycle phase detection
- Check detergent dosing works (manual dose via app)

---

## Step 4: Stain Scanner

### Assembly
1. Solder all components per schematic (`schematic/scanner-node/`)
2. Flash firmware: `firmware/scanner-node/scanner_main.c`
3. Charge via USB-C (full charge ~2 hours)
4. Load TFLite models (fabric + stain classifiers) to flash

### Usage
1. Power on (press scan button)
2. Point at garment, 10-15cm distance
3. Press scan button → LEDs flash (white/UV/IR)
4. Wait 2-3 seconds for inference
5. Read result on display: fabric type, stain type, recommendation
6. Result sent to hub → app + cloud

### Verification
- Node appears on hub TFT when scanning (3 active nodes)
- App receives scan results
- Test with known fabrics (cotton shirt, wool sweater)

---

## Step 5: Calibration

Run the calibration script after all nodes are installed:

```bash
cd scripts
python3 calibrate_sensors.py --node all
```

This calibrates:
- Dryer differential pressure (zero offset)
- Dryer thermocouple (ice bath reference)
- Washer load cell (tare + known weight)
- Washer flow sensor (measured volume)
- Scanner camera white balance

## Step 6: First Wash Cycle

1. Scan a garment with the scanner
2. Load washer
3. App shows recommended cycle + detergent amount
4. Approve (or override) in app
5. Start washer
6. Watch cycle progress in app
7. Auto-dose happens during fill phase
8. Cycle completion notification
9. Move to dryer
10. Dryer node monitors for fire safety + dryness
11. "Dry" notification when done (saves energy!)

## Troubleshooting

| Issue | Solution |
|-------|---------|
| Node not appearing on hub | Check antenna connection, power, distance <30m |
| Dryer temp reads wrong | Check thermocouple polarity, cold junction |
| Pressure always high | Check tubing for kinks, re-zero calibrate |
| Detergent not dosing | Prime pump, check tubing for air locks |
| Scanner battery dies fast | Check deep sleep config, UV LED current |
| Fire alarm false trigger | Re-zero pressure sensor, check thermocouple contact |