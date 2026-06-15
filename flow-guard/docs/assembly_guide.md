# FlowGuard Assembly Guide

## Safety Warnings

⚠️ **WARNING: FlowGuard involves WATER and ELECTRICITY. Follow these safety rules:**
1. **Turn off main water supply** before installing valve controller or flow meter
2. **Turn off electrical power** before working on any mains-connected components
3. **Hire a licensed plumber** for main water line modifications (valve controller, flow meter)
4. **Use GFCI-protected outlets** for all indoor power connections
5. **Do not** install pipe sensors on hot water lines above 60°C (140°F)
6. **Do not** submerge any node except pipe sensors (IP54 rating only)

---

## Hub Node Assembly

### Parts Required
- Hub PCB (4-layer, 110×70mm)
- nRF52840 + ESP32-C6 module
- 2.4" IPS TFT display (ILI9341)
- microSD card (16GB+)
- SPH0645LM4H MEMS microphone
- 18650 Lipo battery (2600mAh)
- USB-C cable
- 3D-printed enclosure (110×70×25mm)
- 4× M2.5 standoffs (10mm)
- 2.4GHz antenna (if external)

### Assembly Steps

1. **Inspect PCB** — Check for solder bridges, missing components, correct component values
2. **Solder SMD components** — Use hot air reflow for QFN packages (nRF52840, ESP32-C6)
3. **Solder through-hole components** — USB-C receptacle, SD card slot, piezo buzzer
4. **Attach TFT display** — Use FPC connector on PCB; do not solder directly
5. **Insert 18650 battery** — Slide into battery holder on PCB bottom
6. **Attach antenna** — Connect SMA pigtail to PCB antenna connector
7. **Test power rails** — Verify 3.3V and 1.8V rails with multimeter before connecting ICs
8. **Flash initial firmware** — Connect USB-C, flash nRF52840 via SWD, then ESP32-C6 via USB
9. **Assemble enclosure** — Mount PCB on standoffs, connect display, close case
10. **Verify operation** — TFT should show FlowGuard logo, Zigbee LED should blink

---

## Valve Controller Assembly

### Parts Required
- Valve controller PCB (4-layer, 130×80mm)
- nRF52832 module
- DRV8871 motor driver
- Motorized ball valve (1" NPT, 12V DC, spring-return)
- MPX5700DP pressure sensor
- YF-S201 flow meter
- DS18B20 waterproof temperature probes (×2)
- Self-regulating heat trace (5W/ft, 3ft)
- 18650 Lipo battery (2600mAh)
- 12V 2A DC adapter
- 3D-printed enclosure (130×80×50mm, IP54)
- 3× PG9 cable glands

### Assembly Steps

1. **Turn off main water supply** — Verify water is completely off
2. **Cut main water line** — Install 1" NPT tee fitting
3. **Install motorized ball valve** — Thread valve into tee, use Teflon tape on all NPT connections
4. **Install flow meter** — Connect YF-S201 inline before or after valve
5. **Mount PCB in enclosure** — Use M2.5 standoffs
6. **Wire DRV8871** — Connect motor leads from ball valve to DRV8871 output
7. **Wire pressure sensor** — Connect MPX5700DP to ADC input with 0.1µF decoupling cap
8. **Wire flow meter** — Connect YF-S201 signal wire to GPIO interrupt pin
9. **Install temperature probes** — Strap DS18B20 to valve body and heat trace
10. **Install heat trace** — Wrap self-regulating heat trace around exposed pipe section, connect to MOSFET driver
11. **Connect battery backup** — Insert 18650 in holder
12. **Thread cables through glands** — Use PG9 cable glands for motor, sensor, and power cables
13. **Test valve operation** — Connect 12V power, verify valve opens/closes with manual buttons
14. **Restore water supply** — Check for leaks at all connections

---

## Pipe Sensor Assembly

### Parts Required
- Pipe sensor PCB (4-layer, 45×30mm)
- nRF52832
- ADXL362 accelerometer
- SPH0645LM4H MEMS microphone
- DS18B20 waterproof probe
- SHT40 humidity sensor
- Conductive leak trace (copper adhesive tape)
- CR2477 coin cell battery
- 3D-printed enclosure (45×30×15mm)
- Hose clamp or zip tie

### Assembly Steps

1. **Solder all SMD components** — Use hot air reflow; nRF52832 and MEMS mic require careful alignment
2. **Attach DS18B20 probe** — Route waterproof probe cable from PCB edge
3. **Solder battery holder** — CR2477 SMD holder on top side
4. **Apply thermal paste** — Small amount on DS18B20 probe tip
5. **Mount on pipe** — Use hose clamp or zip tie around pipe
6. **Attach DS18B20 probe** — Strap probe to pipe surface with thermal paste
7. **Apply conductive leak trace** — Run copper adhesive tape along pipe below sensor
8. **Insert CR2477 battery** — Slide battery into holder
9. **Close enclosure** — Snap-fit cover with battery access slide
10. **Verify operation** — Green LED should blink once per 5 minutes (normal reporting interval)

### Pipe Sensor Placement Guide

| Location | Priority | Reason |
|----------|----------|--------|
| Main water line (after meter) | HIGH | Detect whole-home leaks |
| Hot water heater inlet | HIGH | Water heater failure detection |
| Washing machine supply | MEDIUM | Appliance leak detection |
| Under kitchen sink | MEDIUM | Common leak location |
| Bathroom supply lines | MEDIUM | Most frequent leak source |
| Exterior spigot | LOW | Freeze risk location |
| Basement/crawlspace main | LOW | Hidden leak detection |

---

## Appliance Monitor Assembly

### Parts Required
- Appliance monitor PCB (4-layer, 80×50mm)
- nRF52832
- YF-S201 flow meter
- BME280 temp/humidity/pressure sensor
- XGZP6847A differential pressure sensor
- DS18B20 waterproof probe
- 2× stainless steel probe pads
- 2× AA batteries
- 3D-printed enclosure (80×50×20mm)
- 3/8" compression fitting (for flow meter)
- 3M VHB adhesive pad

### Assembly Steps

1. **Solder all SMD components** — nRF52832, BME280, XGZP6847A
2. **Install flow meter** — Connect YF-S201 to 3/8" compression fitting, wire to PCB
3. **Attach conductivity probes** — Route stainless steel probe pads from PCB bottom edge
4. **Install DS18B20 probe** — Route waterproof probe cable for inlet temperature
5. **Insert AA batteries** — Snap into battery holder
6. **Mount under sink/appliance** — Use 3M VHB adhesive pad
7. **Connect flow meter inline** — Install on supply line using compression fittings
8. **Position probe pads** — Extend to floor level for standing water detection
9. **Verify operation** — Green LED should blink every 5 minutes

---

## Network Setup

### Commissioning New Nodes

1. Power on the hub first and wait for the TFT to show "Ready"
2. In the FlowGuard mobile app, tap "Add Device"
3. The hub enters Zigbee permit-joining mode (LED blinks blue)
4. Insert battery / power on the new sensor node
5. The node's LED will blink rapidly while joining, then turn solid green
6. The app will display the new node and prompt for location/label
7. Repeat for each node
8. Verify mesh connectivity in the app's "Sensor Map" screen

### Testing the System

1. **Simulate a leak**: Wet the conductive trace on a pipe sensor with water
   - Expected: Immediate alert on hub, push notification to phone, valve auto-closes
2. **Test valve operation**: Press the physical open/close buttons on the valve controller
   - Expected: Valve opens/closes within 5 seconds, LED changes color
3. **Test acoustic detection**: Tap on a pipe near a sensor
   - Expected: Sensor detects vibration anomaly, captures acoustic sample
4. **Test pressure monitoring**: Turn on a faucet and observe pressure readings in the app
   - Expected: Flow rate and pressure readings update in real-time
5. **Test freeze protection**: Place ice on a pipe sensor's DS18B20 probe
   - Expected: Temperature drops, freeze alert triggers when below 3°C

---

## Maintenance

| Task | Frequency | Details |
|------|-----------|---------|
| Check pipe sensor batteries | Every 6 months | Replace CR2477 if below 2.4V |
| Check appliance monitor batteries | Every 12 months | Replace AA batteries |
| Clean conductivity probes | Every 3 months | Wipe stainless steel pads with damp cloth |
| Inspect valve operation | Every 6 months | Press open/close buttons, verify smooth operation |
| Descale flow meters | Every 12 months | Remove and soak YF-S201 in vinegar |
| Check DS18B20 thermal paste | Every 12 months | Reapply if dry or loose |
| Update firmware | As available | OTA updates via mobile app |
| Verify mesh connectivity | Every 6 months | Check all nodes online in app |

---

## Troubleshooting

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| Node not joining network | Out of Zigbee range | Add a pipe sensor between hub and node (router) |
| Sensor offline | Dead battery | Replace battery |
| False leak alerts | Humidity on conductive trace | Clean trace, check for condensation |
| Valve won't close | Motor jammed | Check manual override, call plumber |
| Slow mesh response | Too many hops | Add pipe sensor as router between hub and distant nodes |
| Flow readings wrong | Air in flow meter | Bleed air from pipe, restart flow meter |
| Temperature reading wrong | Bad thermal contact | Reapply thermal paste on DS18B20 |
| Acoustic false positives | Vibrating pipes | Add pipe insulation, adjust ADXL362 threshold |