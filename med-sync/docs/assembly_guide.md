# MedSync - Assembly Guide

## Tools Required

- Soldering iron (temperature-controlled, fine tip)
- Solder (63/37 leaded, 0.5mm)
- Flux pen
- Tweezers (ESD-safe, fine tip)
- Multimeter
- magnifying glass or microscope
- Hex screwdrivers (1.5mm, 2mm)
- USB-C cable
- ST-Link v2 programmer (for STM32F407)
- nRF Connect programmer (for nRF52 chips)
- CR2477 coin cells (×3 for room beacons)
- CR2032 coin cell (×1 for wearable)
- 18650 LiPo batteries (×2 for hub and pill station)

## Hub Node Assembly

### 1. PCB Assembly

1. Solder all surface-mount components (0402 passives first, then ICs)
2. nRF52840 (aQFN-73): Use hot air reflow or solder paste stencil
3. ESP32-S3-MINI-1: Solder paste stencil, reflow
4. PCM5102A (TSSOP-20): Fine-pitch soldering
5. SPH0645LM4H MEMS mic: Pre-mounted module, through-hole
6. ILI9488 TFT display: FPC connector, careful alignment
7. PN532 NFC module: Solder headers, mount on top surface
8. DS3231 RTC: SOIC-16, hand-solder
9. WS2812B LED ring: Solder each LED individually around display
10. MicroSD card slot: SMD, careful of bent pins

### 2. Power Supply

1. Solder MCP73831 charge controller + passives
2. Solder AP2112-3.3V regulator
3. Solder AP6212-1.8V regulator
4. Insert 18650 battery in holder
5. Verify 3.3V rail with multimeter before connecting nRF52840

### 3. Programming

```bash
# Flash nRF52840
cd firmware/hub-node
west build -b nrf52840dk_nrf52840
west flash

# Flash ESP32-S3
cd firmware/hub-node/esp32-bridge
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

### 4. Enclosure

1. 3D print or injection mold the 120×80×30mm ABS enclosure
2. Mount PCB with 4× M2 standoffs
3. Install TFT display in front window (adhesive gasket)
4. Install speaker behind grille
5. Mount NFC antenna on top surface
6. Install USB-C port on bottom

## Pill Station Assembly

### 1. PCB Assembly

1. Solder STM32F407VGT6 (LQFP-100): Solder paste stencil, reflow
2. Solder nRF52832 (QFN-48): Hot air reflow
3. Solder 8× A4988 stepper driver modules (through-hole headers)
4. Solder 8× HX711 load cell amplifier ICs
5. Solder SSD1306 OLED display (I2C connector)
6. Solder PN532 NFC module
7. Solder DS3231 RTC
8. Solder 8× IR emitter/detector pairs
9. Solder magnetic reed switch
10. Solder piezo buzzer

### 2. Mechanical Assembly

1. 3D print the carousel mechanism (8 compartments, 40×30mm each)
2. Mount main carousel stepper motor (28BYJ-48) centrally
3. Install 8 per-bin stepper motors on carousel
4. Route motor wires through slip ring (allows unlimited rotation)
5. Mount 8 load cells (one per compartment, on bin base)
6. Install IR emitter/detector pairs (one per bin, at bin opening)
7. Mount reed switch on cover hinge
8. Install 8× WS2812B LEDs (one per bin, at bin entrance)

### 3. Programming

```bash
# Flash STM32F407
cd firmware/pill-station
# Using OpenOCD with ST-Link
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/pill_station.elf verify reset exit"

# Flash nRF52832
cd firmware/pill-station/nrf52-bridge
west build -b nrf52dk_nrf52832
west flash
```

### 4. Calibration

1. Power on and home the carousel to position 0
2. Calibrate each load cell:
   - Place known weight (1g calibration weight) on each bin
   - Run calibration command via BLE
   - Repeat for empty bin (tare)
3. Test each IR beam-break sensor
4. Verify motor rotation and direction for each bin
5. Test cover switch

## Room Beacon Assembly

### 1. PCB Assembly

1. Solder nRF52832 (QFN-48): Hot air reflow
2. Solder SHT40 (DFN-4): Hot air
3. Solder VEML7700 (LGA-4): Hot air
4. Solder SPH0645LM4H MEMS mic module: Through-hole
5. Solder AM312 PIR sensor module: Through-hole
6. Solder WS2812B single LED: Hand-solder
7. Solder piezo buzzer: Hand-solder
8. Solder tactile push button: Hand-solder
9. Install CR2477 battery holder: Hand-solder

### 2. Programming

```bash
cd firmware/room-beacon
west build -b nrf52dk_nrf52832
west flash
```

### 3. Mounting

1. Install CR2477 coin cell
2. Adhere to wall using 3M VHB pad or screws
3. Position PIR sensor to cover room entry/exit
4. Pair with hub via NFC tap or mobile app

## Wearable Tag Assembly

### 1. PCB Assembly

1. Solder nRF52833 (QFN-48): Hot air reflow
2. Solder MAX30101 (TQFN-24): Hot air
3. Solder ADXL362 (3×3 LGA): Hot air
4. Solder BMI160 (LGA-14): Hot air
5. Solder DRV2605L (VQFN-10): Hot air
6. Solder RGB LED (0603): Hand-solder
7. Solder tactile side button: Hand-solder
8. Solder CR2032 battery holder: Hand-solder
9. Solder ERM vibration motor wires: Hand-solder

### 2. Programming

```bash
cd firmware/wearable-tag
west build -b nrf52833dk_nrf52833
west flash
```

### 3. Enclosure

1. Insert PCB into silicone wristband module
2. Route ERM motor to vibration pad inside band
3. Install CR2032 coin cell
4. Snap on IP54 top cover
5. Pair with hub via NFC tap or mobile app

## System Setup

1. Power on hub node (USB-C)
2. Hub creates BLE mesh network (provisioner)
3. Power on pill station (USB-C) — it joins mesh automatically
4. Install room beacon batteries — they join mesh automatically
5. Install wearable tag battery — it joins mesh automatically
6. Open MedSync mobile app
7. App discovers hub via BLE
8. NFC tap phone on hub to pair
9. App walks through medication setup:
   - Add each medication (name, dose, frequency)
   - Assign each to a bin (load pills into carousel)
   - Set schedule times
10. Configure caregiver contact information
11. Verify each node is reporting data on dashboard
12. Test fall detection (gentle test — do not actually fall)
13. Test dose verification (take a scheduled dose, verify weight/IR detection)
14. System is now operational!