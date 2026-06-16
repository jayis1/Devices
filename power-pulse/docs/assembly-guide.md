# PowerPulse Assembly Guide

## ⚠️ SAFETY WARNING

**PowerPulse involves working with mains electricity (120/240VAC) and high-current DC circuits (up to 20A, 58V). Only qualified electricians should install the circuit monitor and solar node. The appliance tag also connects to mains power. If you are not qualified, hire a professional.**

## Tools Required

- Soldering iron (temperature-controlled, fine tip)
- Solder (63/37 leaded recommended for first-time builders)
- Flux pen
- Multimeter
- Oscilloscope (helpful for debugging MPPT and CT signals)
- Hot air rework station (for QFN packages)
- Wire strippers
- Phillips and flat-head screwdrivers
- Breaker panel access tools (for circuit monitor installation)
- Multimeter with AC current clamp (for calibration)
- Known load for calibration (e.g., 100W incandescent bulb)

## Assembly Order

### 1. Hub Node (easiest, start here)

1. **Inspect PCB** for manufacturing defects
2. **Solder passive components** (resistors, capacitors) — start with smallest
3. **Solder CC1101 module** — align carefully, use minimal solder
4. **Solder ESP32-S3 module** — hot air rework recommended
5. **Solder remaining ICs** (CH340C, MP28167, TP4056)
6. **Solder connectors** (USB-C, microSD, pin headers)
7. **Solder LEDs and buzzer**
8. **Install 18650 battery holder** — solder and secure with adhesive
9. **Connect whip antenna** (86mm wire soldered to CC1101 ANT pad)
10. **Flash firmware** via USB-C: `idf.py flash monitor`
11. **Verify**: Connect USB-C power, check 3.3V rail, WiFi AP appears

### 2. Circuit Monitor (⚠️ requires electrician)

1. **Assemble PCB** — same process as hub, but note the isolation boundary
2. **Do NOT bridge the isolation gap** between the CT/ADC side and MCU side
3. **Solder ADS131E08 chips** (QFN-48) — use hot air, verify with microscope
4. **Solder ISO7741 isolators** — these straddle the isolation boundary
5. **Install HLK-PM03** — ensure proper creepage/clearance distances
6. **Screw terminal blocks** for CT wires
7. **Flash firmware** via SWD header (ST-Link v2)
8. **Calibrate** with known load (see calibration guide)
9. **Install in breaker panel** — ONLY after de-energizing and locking out the panel
10. **Clamp CT sensors** around each circuit breaker wire
11. **Connect voltage sense transformer** to mains (use proper spade connectors)
12. **Power up and verify** — check Sub-GHz link to hub

### 3. Appliance Tags (plug and play)

1. **Assemble PCB** (smallest board, ~45×55mm)
2. **Solder nRF52840 module** first (largest component)
3. **Solder BL0937** — careful alignment of QFN package
4. **Solder G3MB-202P SSR** — observe polarity markings
5. **Solder OLED display** — use right-angle header
6. **Solder remaining components** (LDO, caps, button, LED)
7. **Flash firmware** via USB-C or SWD
8. **Calibrate BL0937** with known load
9. **Pair with hub** via long-press button (3 seconds)
10. **Plug into wall outlet** and verify BLE mesh connection

### 4. Solar Node (⚠️ high current DC)

1. **Assemble PCB** — special attention to power section
2. **Solder RP2040 and W25Q16** first
3. **Solder power MOSFETs** (IRFP4468) — use heatsink compound
4. **Mount heatsink** to MOSFETs with thermal paste
5. **Solder INA260 sensors** (small QFN)
6. **Solder inductor** — reinforce pads with extra solder for current handling
7. **Solder capacitors** — observe polarity on electrolytics
8. **Wire MC4 connectors** to solar input pads
9. **Flash firmware** via USB (BOOTSEL mode)
10. **Test MPPT** with bench power supply (set to 30V, current limit 5A)
11. **Verify safety shutdown** by pulling GPIO27 high
12. **Install near battery** in weatherproof enclosure
13. **Connect to solar panel** and battery bank

## Initial Setup

1. Power up hub node first — wait for WiFi AP
2. Connect to `PowerPulse-Setup` WiFi and configure home WiFi
3. Hub will connect to WiFi and start accepting node registrations
4. Power up circuit monitor — it should auto-register with hub
5. Plug in appliance tags one by one — pair each with long-press button
6. Install and power up solar node
7. Open mobile app and verify all nodes appear
8. Run calibration script: `python calibrate_ct.py --hub http://powerpulse.local:8000`
9. Assign circuits and appliances in the app
10. Verify data is flowing on dashboard

## Troubleshooting

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| Hub not connecting to WiFi | Wrong credentials | Reset and reconfigure via AP mode |
| Circuit monitor not registering | Sub-GHz range issue | Move hub closer to panel |
| CT reading always zero | CT not clamped / wrong orientation | Check clamp orientation and connection |
| Appliance tag not pairing | Already paired to another hub | Factory reset (hold button 10s) |
| Solar node MPPT stuck at 0% | PV not connected or under voltage | Check MC4 connections, verify >10V |
| Arc fault false positives | CT calibration offset | Recalibrate with no load |
| Dashboard not updating | MQTT broker not running | Check `docker compose logs mosquitto` |