# FreshKeep — Assembly Guide

## Safety Warning

⚠️ **The Stove Guard node involves mains voltage (24V) and a gas shutoff valve. Improper installation can cause fire, gas leak, or electric shock. Have a licensed electrician and plumber install the Stove Guard node.**

⚠️ **The gas shutoff valve MUST be installed by a qualified professional. Incorrect installation can create a gas leak hazard.**

⚠️ **The fire suppression system contains potassium bicarbonate. Do not modify or refill the cartridge with other substances.**

---

## Hub Node Assembly

### Parts Required
- Hub PCB (4-layer, 120×80mm)
- RP2040 + ESP32-C6 modules
- SX1262 radio module
- 2.8" ILI9341 TFT display
- All BOM components

### Steps

1. **PCB Assembly**
   - Solder all SMD components (0402 passives, QFN ICs) using hot air reflow
   - Place RP2040 first (center), then ESP32-C6 module
   - Place SX1262 radio with antenna matching network
   - Place voltage regulators (AP2112-3.3V, AP6212-1.8V)
   - Place MCP73831 battery charger
   - Solder all decoupling capacitors (check BOM for values)

2. **Display Connection**
   - Solder TFT FPC connector to PCB
   - Connect 2.8" ILI9341 TFT display via FPC cable
   - Verify SPI communication with test firmware

3. **Radio Assembly**
   - Solder SMA connector for external antenna
   - Connect 868MHz wire/PCB antenna
   - Verify SX1262 initialization with test firmware

4. **Enclosure**
   - 3D print or injection mold ABS enclosure (120×80×30mm)
   - Cut window for TFT display
   - Drill holes for USB-C port, piezo speaker, antenna
   - Insert PCB with standoffs
   - Connect battery

5. **Testing**
   - Flash RP2040 firmware via USB (BOOTSEL mode)
   - Flash ESP32-C6 firmware via UART
   - Verify TFT display shows boot screen
   - Verify Sub-GHz radio communication with test node
   - Verify WiFi connectivity
   - Verify BLE connectivity

---

## Fridge Node Assembly

### Important Notes
- This node operates inside a refrigerator (-20°C to +5°C)
- All components must be cold-rated
- Enclosure must be IP54 (condensation resistant)
- Battery must be cold-rated (Lithium Thionyl Chloride or special Lipo)

### Steps

1. **PCB Assembly**
   - Solder STM32L476RG (LQFP-64)
   - Place SX1261 radio with antenna matching
   - Place all sensor ICs: SGP40, SCD30, SHT40, MQ-3
   - Place 4× HX711 ADCs for load cells
   - Place TPS62740 buck converter (high efficiency for battery)
   - Place MCP73831 battery charger with Qi receiver

2. **Camera Assembly**
   - Mount OV5640 cameras on flexible arms
   - Route camera FPC cables to PCB connectors
   - Test camera focus at 15-30cm range (typical fridge distances)

3. **Load Cell Installation**
   - Place 4× HX711 load cell pads on fridge shelves
   - Route wires to main PCB
   - Calibrate each shelf (see `scripts/calibrate_weights.py`)

4. **Enclosure Sealing**
   - Apply marine-grade epoxy (3M DP270) to all PCB edges
   - Seal camera cable exits with silicone
   - Ensure USB-C magnetic connector has rubber gasket

5. **Installation in Fridge**
   - Mount main unit on fridge ceiling (magnetic mount)
   - Route camera arms to top shelf and middle shelf
   - Place load cell pads under items on each shelf
   - Connect Qi charging pad (if used)

6. **Calibration**
   - Run weight calibration with known weights
   - Test gas sensor readings in clean air
   - Verify camera images are clear and well-lit
   - Test door-open detection with light sensor

---

## Pantry Node Assembly

### Steps

1. **PCB Assembly**
   - Solder ESP32-S3-WROOM-1 module
   - Place SX1261 radio
   - Place OV2640 camera connector
   - Place HW-490 barcode scanner module
   - Place 6× HX711 ADCs
   - Place SH1106 OLED connector
   - Place MG90S servo connector

2. **Barcode Scanner Installation**
   - Mount HW-490 scanner behind a window in the front face
   - Route UART wires to ESP32-S3
   - Test scanning various barcodes (UPC-A, EAN-13, QR codes)

3. **Load Cell Installation**
   - Place 6× load cells under pantry shelves
   - Route wires to main PCB
   - Calibrate each shelf

4. **Lazy Susan Assembly**
   - Mount MG90S servo to pantry shelf
   - Connect lazy susan turntable to servo horn
   - Verify full 360° rotation

5. **Installation**
   - Mount on pantry door interior (magnetic mount)
   - Connect USB-C power
   - Test barcode scanning workflow

---

## Stove Guard Assembly ⚠️ PROFESSIONAL INSTALLATION REQUIRED

### Critical Safety Notes
- **GAS VALVE INSTALLATION MUST BE DONE BY A LICENSED PLUMBER**
- **24V POWER SUPPLY MUST BE INSTALLED BY A LICENSED ELECTRICIAN**
- **DO NOT MODIFY THE FIRE SUPPRESSION SYSTEM**

### Steps

1. **PCB Assembly**
   - Solder STM32F411CE (UFQFPN-48)
   - Place SX1261 radio
   - Place MLX90640 thermal camera connector
   - Place 3× MQ gas sensor modules (MQ-2, MQ-135, MQ-137)
   - Place RE46C190 smoke detector IC
   - Place IRLZ44N MOSFETs for valve and pump control
   - Place 5F supercapacitor
   - Place 105dB siren

2. **Gas Valve Installation** ⚠️ LICENSED PLUMBER ONLY
   - Install 1/2" NPT solenoid gas valve in gas supply line
   - Position: Between gas meter and stove, accessible for maintenance
   - Valve MUST be normally-closed (NC): power loss = valve closes
   - Use Teflon tape on all NPT connections
   - Pressure test all connections with soap bubble test
   - **CRITICAL**: Verify valve closes when power is removed

3. **Fire Suppression Installation**
   - Mount micro-pump and potassium bicarbonate cartridge above stove
   - Route nozzle to aim at stovetop burners from above
   - Test pump activation (without cartridge) to verify nozzle spray pattern
   - **DO NOT** activate suppression with cartridge installed until system is fully tested

4. **Thermal Camera Mounting**
   - Mount MLX90640 under range hood or on wall behind stove
   - Camera must view all 4 burners
   - FOV: 60° (standard lens) should cover typical 30" range
   - Secure with fire-rated bracket

5. **Power Supply Installation** ⚠️ LICENSED ELECTRICIAN ONLY
   - Install 24V DIN-rail power supply in electrical panel
   - Route 24V DC to stove guard via fire-rated cable
   - Connect supercapacitor backup on PCB
   - Verify power supply output with multimeter

6. **Commissioning**
   - Flash STM32F411 firmware via ST-Link
   - Verify thermal camera image (all 4 burners visible)
   - Verify gas sensors read zero in clean air
   - Verify smoke detector triggers on test aerosol
   - Verify gas valve closes on command
   - **Test fire suppression with WATER ONLY** (remove cartridge, connect water supply)
   - After all tests pass, install potassium bicarbonate cartridge

---

## System Pairing

After all nodes are assembled and tested individually:

1. **Power on hub first** — it becomes the mesh coordinator
2. **Power on fridge node** — it joins the mesh automatically
3. **Power on pantry node** — it joins the mesh automatically
4. **Power on stove guard LAST** — it joins the mesh and gets Slot 0
5. **Verify all nodes appear on hub TFT display**
6. **Connect hub to WiFi** via mobile app BLE pairing
7. **Run calibration for all weight sensors**
8. **Test fire alarm response** (use test aerosol, not real fire)
9. **Test gas shutoff response** (verify valve closes on command)

## Post-Installation Checklist

- [ ] Hub TFT shows all 4 nodes online
- [ ] Fridge node reads correct temperature
- [ ] Fridge camera takes clear photos
- [ ] Pantry barcode scanner reads items
- [ ] Pantry weight sensors show correct values
- [ ] Stove guard thermal camera shows all burners
- [ ] Stove guard gas sensors read near-zero in clean air
- [ ] Gas valve opens on stove start (if configured)
- [ ] Gas valve closes on command
- [ ] Fire suppression pump activates on command
- [ ] Mobile app connects via BLE
- [ ] Mobile app shows live data
- [ ] Cloud dashboard shows all nodes
- [ ] Push notifications work