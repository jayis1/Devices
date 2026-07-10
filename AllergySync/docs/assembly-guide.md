# AllergySync — Assembly Guide

## Prerequisites

- Soldering iron (fine tip, temperature-controlled)
- Solder (0.6 mm, 63/37 or lead-free)
- Flux pen
- Tweezers (ESD-safe)
- Magnification (microscope or 5x loupe)
- Multimeter
- USB-C cable
- Computer with ESP-IDF v5.2 and nRF Connect SDK v2.6

## Hub Assembly

### Components
- ESP32-S3-WROOM-1-N16R8 module
- LR1121 transceiver IC
- BMI270 IMU (QFN-14)
- SK6812 RGB LED (2020 package)
- CP2102N USB-UART bridge
- TPS63020 buck-boost converter
- 868 MHz chip antenna
- USB-C connector
- Various passives (see BOM)

### Steps
1. **Apply solder paste** to PCB pads using stencil
2. **Place components**:
   - Place ESP32-S3 module first (largest component)
   - Place LR1121 (QFN-24, center of board)
   - Place BMI270 (QFN-14)
   - Place passives (0402/0603 resistors and capacitors)
   - Place USB-C connector (through-hole + SMT tabs)
3. **Reflow solder** (lead-free profile: peak 245°C for 30s)
4. **Hand-solder** through-hole connectors (JST, pin headers)
5. **Visual inspection** under microscope for solder bridges
6. **Power test**: Connect USB-C, verify 3.3V rail with multimeter
7. **Program**: Flash initial bootloader via USB (CP2102N)

## Room Sentinel Assembly

### Steps
1. Apply solder paste, place components (same process as Hub)
2. **Mount SPS30 sensor** (vertical mount, fan port facing outward)
3. **Mount 50mm fan** to enclosure with 4× M2 screws
4. Ensure fan-to-sensor air path is unobstructed
5. Reflow + inspect
6. **Test SPS30**: Power on, UART should output data frames at 1 Hz

## Window Node Assembly

### Steps
1. Solder nRF52840 module and passives
2. **Mount TMC2209** stepper driver (with thermal pad to PCB ground plane)
3. **Wire NEMA17 stepper motor** to screw terminals (A+, A-, B+, B-)
4. **Mount reed switch** on window frame (magnet on moving sash)
5. **Install relay** for air purifier control (switches AC line — follow local electrical codes)
6. **Connect battery holder** (4× AA NiMH) or USB-C for permanent power
7. **Calibrate**: After flashing, run calibration routine (motor closes until reed triggers)

### Window Actuator Installation
1. Choose appropriate actuator type for your window:
   - **Sliding window**: Rack-and-pinion with belt drive
   - **Casement window**: Chain actuator (worm gear)
   - **Awning window**: Linear actuator
2. Mount stepper motor bracket to window frame (3D-printed or aluminum bracket)
3. Attach drive mechanism to moving sash
4. Connect motor to Window Node's screw terminals
5. Run calibration from the mobile app (Settings → Nodes → Calibrate)

## Wearable Tag Assembly

### Steps
1. Solder nRF52840 module (smallest PCB, 30×30 mm)
2. **Mount PMSA003I** sensor (facing outward, with air gap)
3. Solder BMI270, LR1121, passives
4. **Mount CR2032 holder** (vertical Keystone 3001 or similar)
5. **Print enclosure** (FDM PLA or PETG, clip-on design)
6. Install PCB in enclosure, ensuring PMS sensor air intake is unobstructed
7. **Test**: Insert CR2032, LED should blink to indicate power-on

## Flashing Firmware

### Hub and Room Sentinel (ESP-IDF)
```bash
# Set up ESP-IDF
export IDF_PATH=~/esp/esp-idf
source $IDF_PATH/export.sh

# Flash Hub
cd firmware/hub
idf.py set-target esp32s3
idf.py build flash monitor

# Flash Room Sentinel
cd firmware/room-sentinel
idf.py set-target esp32s3
idf.py build flash monitor
```

### Window Node and Wearable Tag (Zephyr)
```bash
# Set up nRF Connect SDK
export ZEPHYR_BASE=~/ncs/v2.6.0/zephyr
source $ZEPHYR_BASE/.venv/bin/activate

# Flash Window Node
cd firmware/window-node
west build -b nrf52840dk_nrf52840
west flash

# Flash Wearable Tag
cd firmware/wearable-tag
west build -b nrf52840dk_nrf52840
west flash
```

## Pairing

1. Power on Hub, connect to Wi-Fi via the mobile app
2. Power on Room Sentinel — it will auto-join the mesh
3. Power on Window Node(s) — auto-join after calibration
4. Clip on Wearable Tag — it will join the mesh when in range
5. Verify all nodes appear in the mobile app (Settings → Nodes)

## Deployment

### Cloud Backend
```bash
cd software/dashboard
pip install -e .
# Set up PostgreSQL and Redis
# Initialize database
psql -U allergysync -d allergysync -c "$(python -c 'from main import DB_INIT_SQL; print(DB_INIT_SQL)')"
# Start server
uvicorn main:app --host 0.0.0.0 --port 8000
```

### ML Pipeline
```bash
cd software/ml-pipeline
pip install -e .
# Train all models
python train_pollennet.py
python train_pollen_forecast.py
python train_symptom_sensitivity.py
python train_activity_cnn.py
# Export PollenNet model header for firmware
# Copy pollennet_model.h to firmware/room-sentinel/
```