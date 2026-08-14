# PostureSync Assembly Guide

## Prerequisites
- Soldering iron (fine tip, temperature controlled)
- Hot air rework station (for SMD components)
- Digital multimeter
- KiCad 7+ (for schematic review)
- PlatformIO (for firmware flashing)
- 3D printer (for enclosures)

## Node Assembly Order

### 1. PostureSync Hub
1. **PCB fabrication:** Order 4-layer FR4 PCB (80×60mm) from PCBWay
2. **Solder SMD components:** ESP32-S3, SX1262, TPS63020, TP4056 (hot air)
3. **Solder passives:** Resistors, capacitors, inductors
4. **Solder connectors:** USB-C, JST-PH battery connector, headers
5. **Solder through-hole:** E-ink display (via FPC connector), 18650 holder
6. **Flash firmware:** `pio run -e hub -t upload`
7. **Test:** Power on, verify Wi-Fi, BLE, Sub-GHz, e-ink display

### 2. Spine Band
1. **PCB fabrication:** Order 4-layer flex PCB (50×30mm)
2. **Solder SMD:** nRF52840, ICM-42688-P, MAX30102, BMP390, DRV2605L
3. **Solder passives and connectors**
4. **Attach LRA haptic actuator** (adhesive)
5. **Connect LiPo battery** (JST-SH connector)
6. **Flash firmware:** `pio run -e spine_band -t upload`
7. **Test:** Wear between shoulder blades, verify BLE, haptic, IMU

### 3. Posture Garment
1. **PCB fabrication:** Order 4-layer flex PCB (80×40mm)
2. **Solder SMD:** nRF52840, ADS1298, 3× ICM-42688-P
3. **Sew textile electrodes:** 8× Ag/AgCl MedTex 180 patches on compression shirt
4. **Attach snap connectors:** 8× conductive snaps to electrode patches
5. **Connect PCB to garment:** Snap PCB onto garment snaps
6. **Flash firmware:** `pio run -e posture_garment -t upload`
7. **Test:** Put on garment, verify EMG channels, IMU segments

### 4. Smart Chair Pad
1. **PCB fabrication:** Order 2-layer FR4 PCB (40×40cm chair pad)
2. **Solder SMD:** ESP32-S3-MINI, SX1262, MCP23017, HX711
3. **Assemble FSR matrix:** 16×16 pressure sensor array
4. **Wire FSR matrix to PCB**
5. **Flash firmware:** `pio run -e chair_pad -t upload`
6. **Test:** Sit on pad, verify pressure readings, Sub-GHz TX

### 5. Desk Sentinel
1. **PCB fabrication:** Order 2-layer FR4 PCB (30×20mm)
2. **Solder SMD:** RP2040, ESP32-C3, VL53L1X, VL53L0X, VEML7700
3. **Solder through-hole:** USB-C, buzzer, button
4. **3D print enclosure:** Clip-on monitor mount
5. **Flash firmware:** `pio run -e desk_sentinel -t upload`
6. **Test:** Clip to monitor, verify ToF, light, Wi-Fi

## System Setup

1. **Start cloud backend:**
   ```bash
   cd software/dashboard
   docker-compose up -d
   ```

2. **Pair devices:**
   - Power on Hub (connects to Wi-Fi)
   - Power on each node (auto-joins mesh / BLE)
   - Open mobile app → Settings → Add Device

3. **Calibration:**
   - Spine Band: Wear between shoulder blades, press calibrate, follow prompts
   - Posture Garment: Put on, press calibrate, contract each muscle group
   - Chair Pad: Sit centered, press calibrate, lean left/right/slouch

4. **Start monitoring:**
   - Dashboard shows real-time posture score
   - Alerts trigger when poor posture detected
   - Weekly reports generated automatically