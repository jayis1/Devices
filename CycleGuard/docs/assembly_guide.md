# CycleGuard Assembly Guide

## Prerequisites
- Soldering iron (fine tip, temperature controlled)
- Hot air rework station (for QFN packages)
- Magnification (microscope or 10x loupe)
- Multimeter
- KiCad 7+ (for PCB fabrication files)
- 3D printer (for custom enclosures)

## Node 1: Hub (Handlebar Computer)

### PCB Fabrication
1. Open `schematic/hub/hub.sch` in KiCad
2. Run DRC, generate Gerbers
3. Order 4-layer FR4 PCB (90×70mm) from JLCPCB
4. Minimum trace width: 6 mil, minimum via: 0.3mm

### Component Placement
1. **U1 (ESP32-S3-WROOM-1)** — Solder with hot air (QFN footprint, reflow profile)
2. **U2 (SX1262)** — QFN-24, use stencil + reflow
3. **U3 (ILI9341 TFT)** — FPC connector, solder by hand
4. **U4 (OV5640 camera)** — LGA package, reflow with stencil
5. **U5 (NEO-M9N GPS)** — LCC package, reflow
6. **U6 (SIM7600G)** — LGA package, reflow with stencil
7. **U7-U10 (sensors/drivers)** — Small QFN/DFN/SOIC, reflow
8. **Passives** — 0402/0603, reflow with stencil
9. **Battery holder** — Keystone 1042, through-hole solder
10. **USB-C connector** — SMD, hand solder with flux

### Antenna
- 868 MHz PCB antenna (matching network per SX1262 datasheet)
- GPS antenna: keep area clear of ground plane (chip antenna on NEO-M9N)
- Camera: ensure lens is front-facing in enclosure

### Testing
1. Power on via USB-C — verify 3.3V rail
2. Flash firmware via USB (ESP32-S3 ROM bootloader)
3. Verify Wi-Fi connects to AP
4. Verify BLE advertises "CycleGuard Hub"
5. Verify SX1262 transmits beacon (check with SDR)
6. Verify TFT displays text
7. Verify GPS gets fix (LED indicator)
8. Verify SIM7600G registers on network (AT commands via UART)
9. Mount on handlebar (GoPro-style mount)

## Node 2: Smart Helmet

### PCB
- 4-layer flex PCB (45×30mm) — flex for helmet integration
- Stiffener under nRF52840 and connectors

### Assembly
1. **U1 (nRF52840)** — QFN-73, reflow with stencil
2. **U2 (ICM-42688-P)** — LGA-14, reflow
3. **U3 (NAU88C22)** — LQFP-48, reflow
4. **U4 (MAX98357A)** — SOIC-8, reflow
5. **U5 (DRV2605L)** — DFN-10, reflow
6. **Bone conductor (BOCO P-D4010)** — Solder to flex, mount against helmet strap
7. **Throat mic (SPQ2820WP3-1)** — External, wire to NAU88C22 input
8. **Battery (402030 LiPo)** — Wire to pads, secure with adhesive
9. **Integrate into EPS helmet** — route wiring through helmet shell

### Testing
1. Flash firmware via SWD (nRF52840)
2. Verify BLE advertises
3. Verify IMU reads at 500 Hz
4. Verify bone conduction audio output
5. Verify horn/siren detection (play car horn near mic)
6. Tap helmet to simulate impact — verify crash detection LED

## Node 3: Smart Light Set

### PCB
- Front: 4-layer FR4 PCB (60×40mm) in aluminum housing
- Rear: flexible PCB strip (8×WS2812B) in silicone sleeve

### Assembly
1. **U1 (ESP32-C6-MINI-1)** — Castellated module, reflow
2. **U2 (ICM-42688-P)** — LGA-14, reflow
3. **U3 (APDS9301)** — Small QFN, reflow (align light sensor window)
4. **U4 (PT4115)** — SOP-8, reflow
5. **D1 (Luxeon LED)** — Large pad, reflow with thermal management
6. **WS2812B strip** — Solder to rear PCB
7. **Asymmetric lens** — Mount over front LED (StVZO compliance)

### Testing
1. Flash via USB
2. Verify headlight turns on at full brightness
3. Cover APDS9301 — verify auto-brightens (night mode)
4. Flash light at APDS9301 — verify auto-dims (oncoming traffic)
5. Tilt light forward — verify brake detection triggers red flash
6. Press turn signal buttons — verify amber chase pattern

## Node 4: Bike Sensor

### PCB
- 4-layer FR4 PCB (35×25mm) — fits in sensor pod

### Assembly
1. **U1 (RP2040)** — QFN-56, reflow
2. **U2 (nRF52840 module)** — Castellated module, reflow
3. **U3, U4 (A1304 Hall sensors)** — Small SOT-23, reflow
4. **U5 (SP37T TPMS)** — External, install in valve stem
5. **W25Q16JV flash** — SOIC-8, reflow
6. **CR2477 holder** — Through-hole
7. **Wheel magnet** — Install on spoke (align with Hall sensor)
8. **Crank magnet** — Install on crank arm (align with Hall sensor)
9. **Enclosure** — IP67 sensor pod, clip to bike frame

### Testing
1. Flash via SWD (RP2040) and nRF52840
2. Spin wheel — verify speed reading
3. Pedal — verify cadence reading
4. Check TPMS pressure reading (compare with manual gauge)

## Node 5: Smart Lock

### PCB
- 4-layer FR4 PCB (80×50mm) — integrated into U-lock body

### Assembly
1. **U1 (ESP32-C6-MINI-1)** — Castellated module, reflow
2. **U2 (SX1262)** — QFN-24, reflow
3. **U3 (SIM7600G)** — LGA, reflow with stencil
4. **U4 (CAM-M8Q GPS)** — LCC, reflow
5. **U5 (ICM-42688-P)** — LGA-14, reflow
6. **U6 (AS5600)** — SOIC-8, position near deadbolt magnet
7. **U7 (HX711)** — SOP-16, reflow
8. **U8 (DRV8871)** — SOIC-8, reflow
9. **Motor** — Wire to DRV8871 outputs, mount to deadbolt mechanism
10. **Load cell** — Mount in shackle mechanism (detect prying)
11. **Siren** — Solder to MOSFET driver, mount in enclosure
12. **Shackle** — 18mm hardened steel U-lock shackle
13. **Battery** — 18650 Li-ion in battery compartment

### Testing
1. Flash via USB
2. Verify motor extends/retracts deadbolt (AS5600 position feedback)
3. Verify GPS gets fix (LED indicator)
4. Verify SIM7600G registers on network
5. Verify SX1262 communicates with Hub
6. Shake lock when armed — verify tamper detection
7. Pry lock — verify load cell detects force
8. Verify siren activates (120 dB — use hearing protection!)
9. Verify GPS tracking activates during alarm state