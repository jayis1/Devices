# CompostSync — Assembly Guide

## Tools Required

- Soldering iron (fine tip, 25-40W)
- Solder (0.6mm rosin-core)
- Flux pen
- Multimeter
- Heat gun (for SMD reflow, optional)
- 3D printer (for enclosures, PETG recommended)
- #0 and #1 Phillips screwdrivers
- Wire strippers
- Crimper (for JST connectors)

## Node Assembly Order

### 1. Hub Assembly

1. **Order PCB** from JLCPCB using `schematic/hub/hub.kicad_pcb` (Gerbers)
2. **Solder components** in order:
   - ESP32-WROOM-32E module (use stencil + reflow, or hand-solder with flux)
   - AP2112 LDO (SOT-23-5)
   - TP4056 + DW01A + FS8205A (SOIC-8 / SOT-23-6)
   - SX1262 module (pre-assembled breakout — solder headers)
   - SSD1306 OLED (pre-assembled — solder headers or use I2C cable)
   - BME280 breakout (solder headers)
   - microSD socket (SMD)
   - WS2812B LED (SMD 5050)
   - USB-C receptacle
   - 18650 holder
   - Pin headers, resistors, capacitors
3. **Print enclosure** — `hub_enclosure.stl` (PETG, 0.4mm nozzle, 30% infill)
4. **Install antenna** — 868MHz whip or PCB antenna
5. **Flash firmware** — `scripts/flash_all.sh hub`

### 2. Bin Node Assembly

1. **Order PCB** from `schematic/bin-node/bin-node.kicad_pcb`
2. **Solder** (same order as Hub, plus):
   - SCD41 breakout (solder I2C cable — keep outside enclosure for air flow)
   - MQ-4 sensor (mount with ventilation holes)
   - HX711 + load cell connector
   - Servo connector (3-pin JST)
   - DS18B20 waterproof probes (use screw terminals for cable connection)
   - Capacitive moisture sensors (solder via cable)
3. **Calibrate**:
   - Run `scripts/calibrate_moisture.py` — follow prompts (dry air, then water)
   - Run `scripts/calibrate_loadcell.py` — place known weight, calibrate scale
4. **Print enclosure** — `bin_enclosure.stl` (IP65, silicone gasket)
5. **Mount in compost bin**:
   - Load cells under bin (or weigh platform)
   - Temperature probes at 3 depths (10/30/50 cm)
   - Moisture sensors at 3 depths
   - SCD41 CO₂ sensor near top of bin (air space)
   - MQ-4 near top (methane rises)
   - Servo on vent flap
   - Solar panel on top/side of bin

### 3. Soil Probe Assembly

1. **Assemble Pico** + breakout board:
   - Solder Pico to custom PCB or use breadboard
   - Connect HM-19 BLE module (UART)
   - Connect SSD1306 OLED (I2C #0)
   - Connect SCD41 via TCA9548A mux (I2C #1)
   - Connect MCP3201 SPI ADC for pH
   - Solder 4× DS18B20 probes along probe shaft (at 5/15/25/35 cm marks)
   - Solder 3× capacitive moisture sensors (at 5/15/25 cm)
   - Connect pH probe (BNC connector on probe head)
2. **Print wand enclosure** — `soil_probe_enclosure.stl` (IP65, tube shape)
3. **Insert probe** deep into compost pile (push until head is submerged)

### 4. Weather Station Assembly

1. **Use Adafruit Feather nRF52840** as base (pre-assembled board)
2. **Solder SX1262** module to breakout (connect via SPI)
3. **Connect BME280** via I2C
4. **Mount anemometer** on mast (2m height, clear of obstructions)
5. **Mount rain gauge** level, away from trees
6. **Wire reed switches** (anemometer + rain gauge) to GPIOTE pins
7. **Connect solar panel** to MCP73871 charger → LiPo battery
8. **Print Stevenson screen** — `stevenson_screen.stl` (louvered radiation shield)
9. **Mount** in garden, 2m above ground, away from buildings

## Firmware Flashing

```bash
# Hub (ESP32 — needs ESP-IDF v5.1+)
cd firmware/hub
idf.py build
idf.py -p /dev/ttyUSB0 flash

# Bin Node (ESP32 — same toolchain)
cd firmware/bin-node
idf.py build
idf.py -p /dev/ttyUSB1 flash

# Soil Probe (RP2040 — needs Pico SDK)
cd firmware/soil-probe
mkdir build && cd build
cmake .. -DPICO_SDK_PATH=/path/to/pico-sdk
make -j4
# Hold BOOTSEL on Pico, plug in USB, copy soil_probe.uf2 to RPI-RP2 drive

# Weather Station (nRF52840 — needs nRF5 SDK)
cd firmware/weather-station
make
# Flash with: nrfjprog --program _build/weather_station.hex --sectorerase
```

## Cloud Deployment

```bash
# Install dependencies
cd software/dashboard
pip install -r requirements.txt

# Start PostgreSQL + TimescaleDB
docker run -d --name compostsync-db \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_DB=compostsync \
  -p 5432:5432 \
  timescale/timescaledb:latest

# Start Mosquitto MQTT broker
docker run -d --name mqtt -p 1883:1883 eclipse-mosquitto

# Start backend
uvicorn main:app --host 0.0.0.0 --port 8000

# Start ML inference service
cd ../ml-pipeline
python inference.py
```

## Mobile App

```bash
cd software/mobile-app
npm install
npx react-native run-android  # or run-ios
```