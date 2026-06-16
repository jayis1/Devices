# CradleKeep — Assembly Guide

## Safety Warnings

> **⚠️ CRADLEKEEP IS NOT A MEDICAL DEVICE.** It does not diagnose, treat, or prevent SIDS or any medical condition. Always follow AAP safe sleep guidelines. Never rely solely on any monitor for your baby's safety.

> **⚠️ CHOKING HAZARD:** Keep all electronics, batteries, and small parts away from babies and children. The crib pad contains no small parts and should remain sealed under the mattress.

> **⚠️ BATTERY SAFETY:** The crib pad uses a CR2450 coin cell. If swallowed, coin cell batteries can cause fatal internal burns. Keep the battery compartment sealed and locked at all times.

## Assembly Overview

### Hub Node
1. Assemble the RP2040 + ESP32-C6 hub PCB according to the schematic
2. Solder all components, paying attention to orientation marks
3. Connect the 2.8" TFT display via the SPI header
4. Connect the PCM5102A DAC + 3W speaker via I2S header
5. Connect the SPH0645 MEMS microphone
6. Connect the SX1262 Sub-GHz radio module
7. Insert the 3000mAh LiPo battery
8. Flash the RP2040 firmware via USB-C
9. Flash the ESP32-C6 firmware via UART
10. Test: Power on, verify TFT display shows "CradleKeep Hub"
11. Enclose in 3D-printed case (STL files in `hardware/enclosures/`)

### Crib Pad Node
1. Assemble the ultra-thin flex PCB (0.8mm, 200×300mm)
2. Affix 4× FSR-402 force-sensitive resistors at the marked positions:
   - FSR1 (head zone): 40mm from top edge, centered
   - FSR2 (chest zone): 130mm from top edge, centered
   - FSR3 (left hip): 200mm from top, 60mm from left edge
   - FSR4 (right hip): 200mm from top, 60mm from right edge
3. Solder the LIS3DH accelerometer (position sensor)
4. Solder the SHT40 temperature sensor
5. Affix the conductive wetness traces in the diaper zone (bottom center)
6. Solder the STM32L476 and supporting passives on the rigid tail section
7. Solder the SX1261 radio module
8. Install the CR2450 coin cell battery holder
9. Flash the STM32L476 firmware via SWD (SWDIO/SWCLK pads on rigid section)
10. Slide the pad under the crib mattress, rigid tail section hanging over the edge
11. Test: Place a weight on the pad, verify breathing detection via hub display

### Nursery Monitor Node
1. Assemble the ESP32-S3 nursery monitor PCB
2. Solder the OV5640 camera module connector
3. Solder the dual SPH0645 MEMS microphone modules (5mm separation)
4. Solder the SHT40, SCD30, SGP40, and VEML7700 sensors on I2C bus
5. Solder the SX1261 radio module
6. Solder the 4× 940nm IR LEDs and SFH309FA phototransistor
7. Solder the IR cut filter relay driver
8. Insert the 1200mAh LiPo battery
9. Flash the ESP32-S3 firmware via USB-C
10. Mount on wall 1.5m above crib, angled 30° downward
11. Connect USB-C power
12. Test: Verify night vision (IR LEDs), audio classification, environment readings

### Feeding Station Node
1. Assemble the nRF52840 feeding station PCB
2. Solder the dual HX711 ADCs and supporting passives
3. Solder the DS18B20 waterproof temperature probe connector
4. Solder the OLED SH1106 display
5. Solder the PTC heater driver MOSFET and supporting components
6. Solder the SG90 servo connector
7. Solder the VCSEL + photodiode turbidity sensor
8. Solder the 3× tactile buttons and piezo buzzer
9. Solder the SX1261 radio module
10. Install the 2000mAh LiPo battery
11. Flash the nRF52840 firmware via USB-C
12. Mount the load cells on the warming plate assembly
13. Connect the DS18B20 probe (waterproof cable)
14. Place on countertop, plug in USB-C
15. Test: Place a bottle on the scale, verify weight reading and temperature control

## Pairing and Setup

1. Download the CradleKeep mobile app
2. Power on the hub and connect to WiFi via the app
3. The hub will automatically discover nodes via Sub-GHz mesh
4. Follow the in-app setup wizard to:
   - Pair the crib pad (place on mattress, follow calibration)
   - Pair the nursery monitor (mount on wall, verify camera)
   - Pair the feeding station (calibrate scale with empty bottle)
5. The system calibrates automatically over the first 24 hours
6. No manual tuning required after initial setup

## Calibration

### Crib Pad
- Auto-calibrates FSR baseline when no weight is detected (first 5 minutes)
- Calibrates position detection based on first 30 minutes of data
- Run manual calibration: `python calibrate_crib.py --device /dev/ttyUSB0`

### Feeding Station
- Auto-calibrates scale on first boot (zero tare)
- Calibrate temperature probe: `python calibrate_crib.py --station feeding --target 37.0`
- Formula scoop calibration: Use app settings to adjust scoop size

### Nursery Monitor
- Auto-calibrates IR LED brightness based on ambient light
- Cry classification model improves over first 2 weeks (on-device learning)
- CO2 sensor auto-calibrates (SCD30 has built-in ASC)

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Hub display blank | Check USB-C power, try different cable. Verify 3.3V rail. |
| Crib pad not detected | Check CR2450 battery. Re-pair via app. Verify SX1261 antenna connection. |
| Breathing rate shows 0 | Check FSR connections. Re-calibrate pad. Ensure pad is flat under mattress. |
| Wetness false alarms | Clean mattress. Reduce sensitivity in app settings. |
| Cry classification poor | Allow 2 weeks for on-device learning. Check microphone is unobstructed. |
| Feeding station scale drift | Re-calibrate scale. Ensure flat surface. Check for vibrations. |
| Bottle not warming | Check PTC heater MOSFET. Verify DS18B20 probe is in warming well. |
| Mesh range too short | Move hub closer to nursery. Check 868MHz antenna is vertical. |
| App not connecting | Check WiFi. Restart hub. Re-pair BLE. |

## Maintenance

- **Crib pad**: Replace CR2450 battery every 12-18 months
- **Hub**: Charge LiPo monthly if on battery backup
- **Nursery monitor**: Clean IR LEDs monthly. Check camera lens.
- **Feeding station**: Descale warming well monthly. Calibrate scale quarterly.
- **Firmware updates**: OTA via app. Check for updates monthly.