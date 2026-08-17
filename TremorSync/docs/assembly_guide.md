# TremorSync Assembly Guide

## Prerequisites
- Soldering iron (fine tip, temperature controlled)
- Hot air rework station (for QFN packages)
- Magnification (microscope or 10x loupe)
- Multimeter
- KiCad 7+ (for PCB fabrication files)

## Node 1: Hub

### PCB Fabrication
1. Open `schematic/hub/hub.sch` in KiCad
2. Run DRC, generate Gerbers
3. Order 4-layer FR4 PCB (80×60mm) from JLCPCB
4. Minimum trace width: 6 mil, minimum via: 0.3mm

### Component Placement
1. **U1 (ESP32-S3-WROOM-1)** — Solder with hot air (QFN footprint, reflow profile)
2. **U2 (SX1262)** — QFN-24, use stencil + reflow
3. **U3 (E-ink display)** — FPC connector, solder by hand
4. **U4 (ADXL362)** — LCC package, reflow
5. **U5-U7 (sensors/drivers)** — Small QFN/DFN, reflow
6. **U8 (SIM7600G)** — LGA package, reflow with stencil
7. **U9 (TP4056)** — SOP-8, hand solder
8. **Passives** — 0402/0603, reflow with stencil
9. **Battery holder** — Keystone 1042, through-hole solder
10. **USB-C connector** — SMD, hand solder with flux

### Antenna
- 868 MHz PCB antenna (matching network per SX1262 datasheet)
- Keep antenna area clear of ground plane

### Testing
1. Power on via USB-C — verify 3.3V rail
2. Flash firmware via USB (ESP32-S3 ROM bootloader)
3. Verify Wi-Fi connects to AP
4. Verify BLE advertises "TremorSync Hub"
5. Verify SX1262 transmits beacon (check with SDR)
6. Verify e-ink displays text
7. Verify SIM7600G registers on network (AT commands via UART)

## Node 2: Tremor Band

### PCB
- 4-layer flex PCB (40×25mm) — flex for wristband form factor
- Stiffener under nRF52840 and connectors

### Assembly
1. **U1 (nRF52840)** — QFN-73, reflow with stencil
2. **U2 (ICM-42688-P)** — LGA-14, reflow
3. **U3 (MAX30102)** — Small QFN, reflow (optical window alignment critical)
4. **U4 (TMP117)** — DSBGA-4, reflow (requires microscope)
5. **U5 (DRV2605L)** — DFN-10, reflow
6. **L1 (LRA)** — Solder to flex with conductive adhesive
7. **Battery (402030 LiPo)** — Wire to pads, secure with adhesive
8. **Enclosure** — Silicone wristband with PC window for PPG

### Testing
1. Flash firmware via SWD (nRF52840)
2. Verify BLE advertises
3. Verify IMU reads at 200 Hz (check via Nordic UART)
4. Verify PPG reads heart rate
5. Verify haptic triggers

## Node 3: Gait Pod

### PCB
- 4-layer flex PCB (30×20mm) — fits under shoe laces

### Assembly
1. **U1 (nRF52840)** — QFN-73
2. **U2 (SX1262)** — QFN-24
3. **U3 (ICM-42688-P)** — LGA-14
4. **F1, F2 (FSR)** — External, wire to PCB (heel and toe positions)
5. **CR2477 holder** — Through-hole
6. **Enclosure** — IP67 shoe clip

### Testing
1. Flash via SWD
2. Verify Sub-GHz TX to Hub
3. Verify IMU reads at 100 Hz
4. Verify FSR ADC reads (press sensor, check values)

## Node 4: Voice Node

### PCB
- 4-layer flex PCB (45×20mm) — worn as neck band

### Assembly
1. **U1 (ESP32-S3-MINI-1)** — Castellated module, reflow
2. **U2 (ICS-43434)** — I²S MEMS mic, reflow (sound port alignment)
3. **U3 (NAU88C22)** — LQFP-48, reflow
4. **U4 (ADS1292R)** — TQFP-32, reflow
5. **MK1 (throat mic)** — External, wire to NAU88C22 input
6. **Electrodes** — Ag/AgCl textile with snap connectors
7. **Enclosure** — Silicone neck band

### Testing
1. Flash via USB
2. Verify I²S mic reads audio (check via serial)
3. Verify throat mic reads via NAU88C22
4. Verify ADS1292R reads EMG

## Node 5: Med Station

### PCB
- 4-layer FR4 PCB (100×80mm)

### Assembly
1. **U1 (ESP32-S3-MINI-1)** — Castellated module
2. **U2 (SX1262)** — QFN-24
3. **U3 (ULN2003)** — DIP-16, through-hole
4. **M1 (28BYJ-48 stepper)** — Wire to ULN2003 outputs
5. **U4 (HX711)** — SOP-16
6. **U5 (AS5600)** — SOIC-8, positioned near carousel magnet
7. **U6 (SSD1306 OLED)** — I²C, 4-pin header
8. **Carousel** — 3D printed (28 compartments), mounted on stepper shaft
9. **Load cell** — Mounted under carousel drop point
10. **Enclosure** — ABS, LED ring visible, large dome buttons

### Testing
1. Flash via USB
2. Verify stepper rotates to correct compartments (AS5600 feedback)
3. Verify load cell reads pill weight
4. Verify IR beam detects pill drop
5. Verify OLED displays
6. Verify Sub-GHz to Hub