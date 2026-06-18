# ThermoGrid — Assembly Guide

## Overview

ThermoGrid consists of 4 node types. Install the **hub first**, then
**room sensors** (one per room), then **zone actuators** (one per
heating/cooling zone), then pair your **comfort tag**.

## Parts List

| Node | Key Components | Est. Cost |
|------|---------------|-----------|
| Hub | RP2040 + ESP32-C6, SX1262, ILI9488 TFT, PCF8563 RTC, LiPo 2500mAh | ~$26 |
| Room Sensor | STM32WL55JC, MLX90640, SHT45, SDP810, BMP390, HLK-LD2410B, AM612, AA + solar | ~$22 |
| Zone Actuator | ESP32-C3, SX1261, Danfoss RA2 or MG996R servo, relays, DS18B20 | ~$18 |
| Comfort Tag | nRF52840, MAX30208, TMP117, MAX30101, LSM6DSO, SHT40, CR2032 | ~$16 |

## Step 1: Hub Node

### Assembly
1. Solder all components per schematic (`schematic/hub-node/`)
2. Flash firmware: `firmware/hub-node/hub_main.c` (RP2040) + ESP32-C6 WiFi bridge
3. Connect 915MHz antenna to SMA connector
4. Insert CR1220 into RTC holder (timekeeping during power outage)
5. Insert LiPo battery (backup power — critical for freeze protection during outage)
6. Connect USB-C power

### Placement
- Indoors, central location (within 30m of all room sensors)
- Wall-mount or shelf
- Connect to WiFi via mobile app (BLE pairing)
- Connect boiler/heat-pump relay to GPIO22 output (via optocoupler + MOSFET)

### Verification
- TFT displays "ThermoGrid Hub v1.0"
- Status LED: solid green (running)
- Display shows 0 zones, 0 nodes (until sensors enrolled)

---

## Step 2: Room Sensors (one per room)

### Assembly
1. Solder all components per schematic (`schematic/room-sensor/`)
2. Flash firmware: `firmware/room-sensor/room_sensor_main.c` (STM32WL55JC)
3. Mount MLX90640 with clear FOV to walls/floor (not blocked by furniture)
4. Mount SDP810 with gasketed port to room air (for air velocity)
5. Mount PIR (AM612) covering the room area
6. Mount mmWave (HLK-LD2410B) facing room center
7. Insert 2× AA batteries
8. Mount solar panel on top (facing window/light source)

### Placement
- Wall-mount at 1.5m height (human comfort zone)
- Away from direct heat sources (radiator, oven, sunny window)
- Away from drafts (not directly above HVAC vent or near door)
- Vented enclosure — air must reach humidity + air velocity sensors
- One per room: living room, bedroom, kitchen, bathroom, office, etc.

### Verification
- Hub display shows new sensor enrolled
- Wave hand in front → occupancy updates
- Open a window → WINDOW_OPEN event (hub pauses that zone)
- Check temperature reading matches a reference thermometer (±0.1°C)

---

## Step 3: Zone Actuators (one per heating/cooling zone)

### Assembly
1. Solder all components per schematic (`schematic/zone-actuator/`)
2. Flash firmware: `firmware/zone-actuator/zone_actuator_main.c` (ESP32-C3)
3. Select actuator type via jumper:
   - **Radiator valve:** Connect Danfoss RA2 motor to H-bridge output
   - **HVAC damper:** Connect MG996R servo to PWM output
   - **Relay:** Connect zone valve / boiler relay to relay outputs
4. Mount DS18B20 on the heating pipe/floor (for feedback + energy accounting)
5. (Optional) Mount YF-S201 flow sensor in the heating water line
6. Connect power: 24VAC from boiler/transformer, or 4× AA

### Placement
- **Radiator valve:** Replace existing TRV head on radiator (M30×1.5 thread)
- **HVAC damper:** Near air handler, connect damper linkage to servo
- **Relay:** In junction box near boiler/manifold/air handler

### Verification
- Hub sends setpoint → valve moves to correct position
- PID loop: room temp approaches setpoint (check via room sensor)
- If hub offline >10min: actuator holds last setpoint (failsafe)
- Pipe temp reading from DS18B20 (should be warm when heating)
- Flow sensor (if installed): reports flow when valve open

---

## Step 4: Comfort Tag

### Assembly
1. Solder all components per schematic (`schematic/comfort-tag/`)
2. Flash firmware: `firmware/comfort-tag/comfort_tag_main.c` (nRF52840)
3. Insert CR2032 battery
4. Pair via BLE in mobile app (Settings → Pair Comfort Tag)

### Placement
- Wear on wrist (watch band), chest (clip), or clothing (pin)
- MAX30208 skin temp sensor should contact skin
- MAX30101 PPG sensor should face skin (green/IR LEDs)

### Verification
- App shows comfort tag connected with battery %
- Skin temp reads ~31-33°C (normal wrist temp)
- HR reads 60-90 bpm at rest
- Press "I'm cold" button → zone boosts +1.5°C for 30 min
- Press "I'm hot" button → zone reduces -1.5°C for 30 min

---

## Step 5: Thermal Calibration

Run `scripts/calibrate_thermal.py --all`:

### Sensor Calibration
- Room sensor: SHT45 temp offset (compare to reference thermometer)
- MLX90640: MRT offset (compare to known surface temp)
- SDP810: air velocity scale (use anemometer reference)
- ALS-PT19: light scale (compare to lux meter)

### Thermal Model Calibration (3-day learning period)
The system learns your home's thermal characteristics:
- **Thermal mass** (how fast rooms heat up / cool down)
- **Heat loss** (insulation quality, per room)
- **Solar gain** (how much sun warms each room through windows)
- **Inter-room airflow** (how opening a door affects adjacent rooms)

During calibration:
1. System measures normal operation for 2 days
2. On day 3, it runs controlled experiments:
   - Opens windows briefly (measures air exchange rate)
   - Pulses heating on/off (measures thermal time constant)
   - Monitors sun through windows (measures solar gain coefficient)
3. After 3 days, the thermal forecast model is personalized to your home

---

## Step 6: Comfort Training

1. Wear your comfort tag for 1-2 days
2. Press "I'm cold" or "I'm hot" whenever you feel uncomfortable
   (aim for 20+ votes over the first week)
3. The system learns your personal thermal preferences
4. After ~50 votes, your personal comfort model is well-trained
5. The system auto-adjusts zones based on your comfort score

---

## Step 7: Set Up Schedules

In the app → Settings → Schedules:
- **Wake:** 6:30 AM → bedroom 22°C
- **Away:** 8:00 AM → all zones 16°C (frost protect)
- **Return:** 5:30 PM → living room 21°C, kitchen 20°C
- **Sleep:** 10:30 PM → bedroom 18°C, other zones 16°C

The system will also learn your routines automatically (HMM model) and
pre-condition rooms before you enter them.

---

## Step 8: Connect Solar (optional)

If you have solar panels:
1. Connect your inverter API in app (Settings → Solar)
2. The system queries real-time production
3. When surplus >500W, it boosts heating zones (solar self-consumption)
4. View solar self-consumption gauge in Energy tab

You're saving energy. 🌡️⚡