# FreshKeep

**AI-powered kitchen intelligence system that eliminates food waste, prevents kitchen fires, and manages your pantry — automatically.** Sees what you can't: spoiled food, forgotten inventory, fire risks, and wasteful habits — then acts on it.

---

## What It Does

FreshKeep is a 4-node system that turns your kitchen into an intelligent, waste-free, fire-safe zone:

1. **Tracks** every item in your fridge and pantry with weight sensors, gas sensors, and camera vision — knows what you have, what's expiring, and what you've forgotten
2. **Prevents** kitchen fires before they start — IR + gas monitoring at the stove, automatic shutoff if unattended cooking is detected
3. **Manages** your grocery list automatically — tracks what you've used, what's running low, generates shopping lists
4. **Saves** the average household $1,500/year in wasted food (Americans throw away 30-40% of their food)
5. **Alerts** you before food spoils — ethylene gas detection predicts ripening, VOC sensors detect spoilage early
6. **Learns** your cooking and shopping patterns over time — optimizes reminders, suggests recipes from expiring ingredients

All nodes communicate over a Sub-GHz mesh network (no WiFi dependency for critical fire-safety functions). A hub node bridges to WiFi/cloud for the dashboard and mobile app.

### The Problem It Solves

- **Food waste**: The average American household wastes $1,500/year in food — 30-40% of all food produced is never eaten
- **Kitchen fires**: Unattended cooking is the #1 cause of home fires (49% of all home fires start in the kitchen)
- **Inventory blindness**: Nobody knows what's in the back of their fridge until it's a science experiment
- **Meal planning friction**: Deciding what to cook requires knowing what you have — most people don't
- **Expiration confusion**: "Use by", "best by", "sell by" — 91% of consumers misunderstand date labels and throw away safe food

FreshKeep automates all of this. You install the hardware, pair it with your app, and it runs your kitchen better than you can.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                          FRESHKEEP SYSTEM                                     │
│                                                                              │
│  ┌───────────────┐  Sub-GHz   ┌──────────────┐                              │
│  │  FRIDGE NODE  │◄──────────►│              │                              │
│  │  (in-fridge)  │  868MHz    │              │                              │
│  │  Camera×2     │  mesh     │              │                              │
│  │  Gas/VOC/temp │           │              │                              │
│  │  Weight×4     │           │   HUB NODE   │                              │
│  └───────────────┘           │  (RP2040 +   │──── WiFi6 ────► Cloud        │
│                               │   ESP32-C6)  │                  Dashboard   │
│  ┌───────────────┐  Sub-GHz  │              │                  + ML         │
│  │  PANTRY NODE  │◄─────────►│              │                  Pipeline     │
│  │  (shelf/cab.) │  mesh    │              │                  + Alerts     │
│  │  Camera×1     │          │              │                              │
│  │  Weight×6     │          │              │──── BLE ──────► Mobile App   │
│  │  Barcode scan │          │              │                  (React Native)│
│  └───────────────┘          └──────┬───────┘                              │
│  ┌───────────────┐                  │ Sub-GHz mesh                         │
│  │ STOVE GUARD   │◄─────────────────┘                                      │
│  │ (above stove) │  (critical fire-safety: mesh priority,                  │
│  │  IR + Gas     │   independent of WiFi)                                   │
│  │  Smoke + Temp │                                                          │
│  │  Gas shutoff  │                                                          │
│  │  Auto-exting. │                                                          │
│  └───────────────┘                                                          │
│                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                    CLOUD / EDGE SOFTWARE                               │  │
│  │  ┌──────────┐  ┌──────────────┐  ┌───────────────────────┐           │  │
│  │  │Dashboard │  │ ML Pipeline │  │ Mobile App             │           │  │
│  │  │ (React)  │  │ (PyTorch)   │  │ (React Native)         │           │  │
│  │  │ Realtime │  │ Food detect │  │ Expiry alerts          │           │  │
│  │  │ History  │  │ Spoilage    │  │ Shopping list           │           │  │
│  │  │ Inventory│  │ Fire detect │  │ Recipe suggestions      │           │  │
│  │  │ Shopping │  │ Pattern     │  │ Fridge/pantry browse   │           │  │
│  │  └──────────┘  └──────────────┘  └───────────────────────┘           │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the Sub-GHz mesh to WiFi/BLE/cloud. Runs local fire-safety rules even when WiFi is down.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + local logic, ESP32-C6 handles WiFi/BLE |
| Radio | SX1262 (868MHz) | Sub-GHz LoRa mesh to all nodes |
| Display | 2.8" IPS TFT (ILI9341) | Kitchen status display — inventory, alerts, recipe suggestions |
| Storage | 32MB W25Q256 Flash + SD card | Local data cache, OTA updates, recipe DB |
| Audio | Piezo buzzer + MAX9814 mic | Fire/expire alarms + voice status updates |
| Power | 5V USB-C + Lipo 2000mAh backup | Stays running during power outage (critical for fire safety) |
| Connectors | 4× I2C, 2× UART, 8× GPIO | Expansion |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler for all nodes)
- Fire-safety rule engine (real-time, always-on, independent of WiFi)
- Data aggregation and time-series buffering
- WiFi uplink to MQTT broker (QoS 1, TLS)
- BLE GATT server for mobile app
- TFT dashboard rendering (inventory, expiry timeline, fire status)
- Local alarm triggers (buzzer + display + voice)
- OTA update distribution to all nodes
- Grocery list generation (local rule-based + cloud ML refinement)

### 2. Fridge Node (1 per system, mounts inside refrigerator)

The eyes inside your fridge. Camera + gas sensors + weight shelves know exactly what's in there and when it'll spoil.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32L476RG | Ultra-low-power ARM Cortex-M4, handles camera + sensors |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Camera | 2× OV5640 (5MP, wide-angle) | Top + shelf view of fridge contents |
| Gas | SGP40 (VOC) + SCD30 (CO2) | Spoilage gas detection, ripening prediction |
| Temp/Humidity | SHT40 | Internal fridge temperature monitoring |
| Ethylene | MQ-3 (alcohol/ethylene) | Fruit ripening detection |
| Weight | 4× HX711 + load cells (5kg) | Shelf weight — tracks item removal/addition |
| Light | TSL2591 ambient light | Door open detection + interior light control |
| Power | 3.7V Lipo 2200mAh + magnetic reed (door switch charging) | Cold-rated battery, charges when door is open via USB-C |
| Enclosure | IP54, cold-rated (-20°C to +5°C) | Designed for fridge interior conditions |

**Fridge node firmware:**
- Captures images every 5 minutes and on door-open events (light sensor trigger)
- Runs lightweight food detection model (TFLite Micro, MobileNet V2)
- Reads gas sensors every 30 seconds — detects spoilage VOC patterns
- Reads weight sensors every 60 seconds — tracks additions/removals
- On-board spoilage detection: SGP40 VOC signature → ripeness/spoilage classification
- Mesh TDMA time-slot transmission to hub
- Ultra-low-power sleep between readings (~80µA avg, cold-optimized)
- Door-open detection triggers immediate photo burst

### 3. Pantry Node (1 per system, mounts in pantry/cabinet)

Tracks dry goods, canned items, spices, and shelf-stable foods. Barcode scanning identifies products automatically.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 | Handles barcode scanner + camera + WiFi provisioning |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Camera | OV2640 (2MP) | Shelf view for visual inventory |
| Barcode | HW-490 (2D barcode scanner module) | Scan items in/out as you add/remove them |
| Weight | 6× HX711 + load cells (10kg) | Per-shelf weight tracking |
| Temp/Humidity | SHT40 + SHT30 (×3) | Monitor humidity on each shelf |
| Light | TSL2591 × 2 | Door-open detection + auto-shelf lighting |
| Display | 1.3" OLED (SH1106) | Quick inventory count, expiry alerts |
| Power | 5V USB-C (mains) + 3.7V Lipo 1200mAh backup | Wall-powered with battery backup |
| Servo | MG90S | Auto-rotate lazy susan for camera scan |

**Pantry node firmware:**
- Barcode scanner reads UPC/EAN/GS1 DataBar on item placement
- Camera captures shelf state on door-open events
- Weight sensors track mass changes per shelf → detect item removal
- Generates "low stock" alerts based on weight thresholds per product
- Tracks expiration dates from barcode database lookup
- Suggests recipes based on available ingredients
- Auto-rotates shelf for full camera coverage (lazy susan)
- Mesh TDMA transmission to hub

### 4. Stove Guard Node (1 per system, mounted above stove)

The lifesaver. Continuously monitors for unattended cooking, gas leaks, smoke, and fire — and can autonomously shut off gas and activate fire suppression.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32F411CE | Fast, deterministic — sub-100ms fire response |
| Radio | SX1261 (868MHz) | Sub-GHz mesh (priority slot — always first in TDMA) |
| IR Thermal | MLX90640 (32×24 thermal array) | Detects overheating pans, oil flash points |
| Gas | MQ-2 (LPG/propane) + MQ-135 (CO/CO2) + MQ-137 (ammonia) | Multi-gas leak detection |
| Smoke | photoelectric smoke detector IC (RE46C190) | Smoke detection |
| Flame IR | VS1838B (modified, 4.3µm IR filter) | Flame detection via IR emission |
| Temp | DS18B20 (×2) | Ambient + stovetop temperature |
| Actuator | Solenoid gas valve (24V NC, 1/2" NPT) | Auto gas shutoff |
| Actuator | Micro-pump + potassium bicarbonate cartridge | Fire suppression (small, targeted) |
| Buzzer | Piezo 105dB siren | Local fire alarm |
| Power | 24V DC (hardwired) + supercap backup | Supercap ensures shutoff works during power failure |

**Stove Guard firmware:**
- Continuous thermal monitoring (10 fps) — detects pan overheating, oil approaching flash point
- Multi-gas leak detection with thresholds (LPG, CO, CO2, NH3)
- Unattended cooking detection: no motion near stove for >10 min with active burner → warning, >20 min → auto-shutoff
- Fire detection: flame IR + smoke + thermal anomaly → immediate gas shutoff + suppression + alarm
- All safety rules run locally on MCU (no cloud dependency for life safety)
- Priority mesh slot — always gets first TDMA slot for instant alert propagation
- Sub-100ms response time for fire shutoff
- Supercap backup: gas valve remains closed for 30 seconds after power loss (failsafe NC valve)

---

## Communication Protocol

### Sub-GHz Mesh (SX1262/61, 868MHz LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa SF7 (normal) / SF9 (alerts) |
| Bandwidth | 125 kHz |
| TX Power | +14 dBm (EU) / +20 dBm (US) |
| Range | 30m indoor (normal) / 200m (long range) |
| Protocol | Custom TDMA (hub is coordinator) |
| Slot Duration | 100ms per node |
| Cycle Time | 500ms (4 slots + 1 priority slot) |

### TDMA Frame Structure

```
| SLOT 0 (STOVE GUARD) | SLOT 1 (FRIDGE) | SLOT 2 (PANTRY) | SLOT 3 (HUB CMD) | SLOT 4 (CTRL/ACK) |
|    100ms priority    |     100ms       |     100ms      |     100ms        |     100ms         |

Total frame: 500ms
Slot 0: STOVE GUARD always gets first slot (fire safety priority, guaranteed latency)
Slot 1: Fridge node uplink data
Slot 2: Pantry node uplink data
Slot 3: Hub broadcasts sync + commands
Slot 4: ACK/retransmit/OTA

In fire alarm mode: STOVE GUARD takes slots 0-4, hub immediately relays to all nodes + cloud
```

### Mesh Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2) ]

TYPE values:
  0x01 = FRIDGE_DATA (gas readings, temp, weights, door events, image-ready flag)
  0x02 = PANTRY_DATA (weights, barcode events, temp/humidity, image-ready flag)
  0x03 = STOVE_GUARD_DATA (thermal array summary, gas levels, flame detection status)
  0x04 = FIRE_ALARM (critical — all nodes stop and relay)
  0x05 = COMMAND (inventory query, gas shutoff, suppression activate)
  0x06 = ACK
  0x07 = OTA_BLOCK (firmware update chunk)
  0x08 = INVENTORY_UPDATE (product added/removed, expiry date)
  0x09 = HEARTBEAT
  0x0A = SHOPPING_LIST (generated by hub, pushed to app)
```

---

## AI / ML Pipeline

### 1. Food Recognition (cloud + on-hub TFLite)

- **On-hub (TFLite Micro, MobileNet V2 SSD)**: Quick identification of common food categories (fruit, vegetable, dairy, meat, condiment, leftovers) from fridge camera — runs every 5 min
- **Cloud (PyTorch, EfficientDet-D0)**: Fine-grained identification — specific apple variety, cheese type, brand recognition from barcode + visual
- Input: 320×240 camera frames (fridge) or 640×480 (pantry)
- Output: Food category + bounding box + confidence
- Training data: Custom kitchen food dataset (10K+ labeled fridge/pantry images) + Food-101 + Grocery Store Dataset

### 2. Spoilage Prediction (on-hub, TFLite Micro)

- Input: Time-series of gas sensor readings (VOC, ethylene, CO2) + temperature + food type + days since purchase
- Model: 1D-CNN + GRU, INT8 quantized, 85KB
- Output: Freshness score (0-100%) per item, days-until-spoilage estimate
- Accuracy: ±1.5 days for produce, ±3 days for dairy
- Triggers: Score <30% = use-today alert, <10% = throw-away alert

### 3. Fire Detection (on Stove Guard MCU, deterministic + ML)

- **Rule engine (deterministic, sub-10ms)**:
  - Thermal: Any pixel >220°C → FIRE_ALARM
  - Gas: LPG >1000 ppm → GAS_LEAK_ALARM
  - Smoke: Photoelectric threshold exceeded → SMOKE_ALARM
  - Unattended: No motion + active burner >20 min → AUTO_SHUTOFF
- **ML refinement (TFLite Micro, 30KB)**:
  - Input: 32×24 thermal frame + gas readings + 5-frame history
  - Model: Tiny MobileNet V1, trained on cooking vs. fire thermal signatures
  - Reduces false positives: distinguishes searing (intentional high heat) from fire
  - Output: Fire confidence 0-1, triggers at >0.85

### 4. Grocery Pattern Learning (cloud, PyTorch)

- Input: Historical purchase data + consumption rates + expiry patterns + seasonal trends
- Model: Transformer-based time series (similar to Temporal Fusion Transformer)
- Output: Predicted shopping list, reorder recommendations, quantity suggestions
- Learns: "You buy milk every 6 days, you're on day 5, suggest adding milk"
- Learns: "You always buy tortillas when you buy chicken, suggest adding tortillas"

### 5. Recipe Suggestion (cloud, LLM-assisted)

- Input: Current inventory (expiring items prioritized) + dietary preferences + cooking skill level
- Model: LLM prompt engineering with structured inventory context
- Output: Ranked recipe suggestions that use items about to expire
- Goal: Maximize food usage, minimize waste

---

## Pin Assignments

### Hub Node (RP2040 + ESP32-C6)

**RP2040 (mesh coordinator + local I/O):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | UART0 TX/RX | ESP32-C6 UART2 (inter-MCU link) |
| GPIO4/GPIO5 | I2C0 SDA/SCL | SX1262 (Sub-GHz radio) |
| GPIO6/GPIO7 | SPI0 SCK/MOSI | SD card + TFT |
| GPIO8 | SPI0 MISO | SD card + TFT |
| GPIO9 | SPI0 CS0 | SD card CS |
| GPIO10 | SPI0 CS1 | TFT CS |
| GPIO11 | TFT DC | Display data/command |
| GPIO12 | TFT RESET | Display reset |
| GPIO13 | TFT BACKLIGHT | Display backlight PWM |
| GPIO14 | SX1262 BUSY | Radio busy signal |
| GPIO15 | SX1262 IRQ | Radio interrupt |
| GPIO16 | SX1262 NRST | Radio reset |
| GPIO17 | SX1262 NSS | Radio SPI chip select |
| GPIO18-21 | SPI1 | SX1262 SPI bus |
| GPIO22 | PIEZO | Buzzer PWM output |
| GPIO23 | USER_BTN | Front panel button |
| GPIO24-26 | LED RGB | Status LED |
| GPIO27 | MAX9814_OUT | Microphone ADC input |

**ESP32-C6 (WiFi/BLE bridge):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | I2C SDA/SCL | (expansion port) |
| GPIO2/GPIO3 | UART0 TX/RX | Debug console |
| GPIO4/GPIO5 | UART1 TX/RX | RP2040 UART0 |
| GPIO12/GPIO13 | USB D+/D- | USB-C port |

### Fridge Node (STM32L476RG)

| Pin | Function | Connected To |
|-----|----------|-------------|
| PA4/PA5 | SPI1 SCK/MISO | OV5640 camera 1 |
| PA6/PA7 | SPI1 MOSI/CS | OV5640 camera 1 |
| PA9/PA10 | UART1 TX/RX | OV5640 camera 2 (parallel iface) |
| PB10/PB11 | I2C1 SDA/SCL | SGP40 + SHT40 |
| PB6/PB7 | I2C2 SDA/SCL | SX1261 radio |
| PB13-PB15 | SPI2 | SCD30 CO2 sensor |
| PA2/PA3 | UART2 TX/RX | SCD30 (alt) |
| PC0 | ADC1_CH10 | HX711 #1 (load cell 1) |
| PC1 | ADC1_CH11 | HX711 #2 (load cell 2) |
| PC2 | ADC1_CH12 | HX711 #3 (load cell 3) |
| PC3 | ADC1_CH13 | HX711 #4 (load cell 4) |
| PA0 | ADC1_CH0 | MQ-3 ethylene (analog) |
| PA1 | GPIO_OUT | Camera 1 enable |
| PB0 | GPIO_OUT | Camera 2 enable |
| PB1 | GPIO_OUT | Fridge interior LED |
| PA8 | ONE_WIRE | DS18B20 temp probe |
| PC6 | QI_CHG | Charging status |
| PB0 | VBAT_SENSE | Battery voltage ADC |

### Pantry Node (ESP32-S3)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | I2C SDA/SCL | SX1261 radio |
| GPIO2-7 | SPI2 | OV2640 camera |
| GPIO8 | CAM_PWDN | Camera power down |
| GPIO9 | CAM_RESET | Camera reset |
| GPIO10 | CAM_XCLK | Camera clock |
| GPIO11-18 | CAM_DATA | Camera parallel data |
| GPIO19/GPIO20 | UART1 TX/RX | HW-490 barcode scanner |
| GPIO21 | SERVO_PWM | Lazy susan servo (MG90S) |
| GPIO35-40 | HX711_CLK×6 | Load cell clock lines (6 shelves) |
| GPIO41-46 | HX711_DAT×6 | Load cell data lines (6 shelves) |
| GPIO47/GPIO48 | I2C SDA/SCL | SH1106 OLED |
| GPIO14/GPIO15 | I2C0 SDA/SCL | SHT40 + 3× SHT30 (shelf humidity) |
| GPIO16 | TSL2591_INT | Light sensor interrupt |
| GPIO17 | TSL2591_INT_2 | Light sensor 2 interrupt |
| GPIO38 | LED_STRIP | WS2812B shelf lighting PWM |
| GPIO39 | BUZZER | Piezo buzzer for barcode confirmation |

### Stove Guard Node (STM32F411CE)

| Pin | Function | Connected To |
|-----|----------|-------------|
| PA4-PA7 | SPI1 | MLX90640 thermal camera |
| PA9/PA10 | UART1 TX/RX | Debug console |
| PB10/PB11 | I2C1 SDA/SCL | SX1261 radio |
| PA0 | ADC1_CH0 | MQ-2 gas sensor (LPG) |
| PA1 | ADC1_CH1 | MQ-135 gas sensor (CO) |
| PA2 | ADC1_CH2 | MQ-137 gas sensor (ammonia) |
| PA8 | ONE_WIRE | DS18B20 ambient temp |
| PB3 | ONE_WIRE_2 | DS18B20 stovetop temp |
| PB6 | GPIO_OUT | Gas solenoid valve control (24V via MOSFET) |
| PB7 | GPIO_OUT | Fire suppression pump (24V via MOSFET) |
| PB8 | GPIO_OUT | 105dB siren PWM |
| PB9 | GPIO_IN | VS1838B flame IR (modified receiver) |
| PB12 | EXTI | RE46C190 smoke detector interrupt |
| PB13 | GPIO_OUT | Smoke detector LED enable |
| PA8 | TIM1_CH1 | MLX90640 sync |
| PC0 | ADC1_CH10 | Supercap voltage monitor |
| PC4 | GPIO_OUT | Watchdog kick |
| PB5 | GPIO_OUT | Status LED red |
| PB6 | GPIO_OUT | Status LED green (redefined for status) |
| PB7 | GPIO_OUT | Status LED blue |

---

## Power Architecture

### Hub Node
```
USB-C 5V ──► MCP73831 ──► Lipo 2000mAh ──► AP2112-3.3V ──► RP2040 + ESP32-C6
                                      ──► AP6212-1.8V ──► SX1262
                           TFT backlight: 5V direct via MOSFET
```
- Average draw: 160mA (WiFi on) → ~12 hours on battery
- Battery backup: auto-fails to battery on USB loss, mesh keeps running

### Fridge Node
```
3.7V Lipo 2200mAh ──► TPS62740-3.3V ──► STM32L476 + sensors
                  ──► AP6212-1.8V ──► SX1261
                  ──► 3.3V ──► OV5640 cameras (enable-gated)
                  ──► 3.3V ──► HX711 load cells (enable-gated)
Recharge: USB-C via magnetic connector (charges when fridge door is open)
```
- Average draw: 3mA (1 reading/min + 1 TX/500ms) → ~25 days on battery
- Cold-rated: Battery and electronics rated to -20°C
- Door-open event triggers camera burst (50mA for 2s) + gas reading

### Pantry Node
```
5V USB-C ──► AP2112-3.3V ──► ESP32-S3 + radio + sensors
         ──► 5V direct ──► HX711 load cells
         ──► 5V direct ──► Barcode scanner
Recharge: 3.7V Lipo 1200mAh backup (MCP73831 charger)
```
- Average draw: 80mA → mains powered, battery is backup only
- Battery runtime: ~10 hours

### Stove Guard Node
```
24V DC (hardwired, building electrical) ──► LM2596-5V ──► AP2112-3.3V ──► STM32F411 + radio + sensors
                                       ──► 5V ──► MLX90640 thermal camera
                                       ──► 24V direct ──► Gas solenoid valve (NC)
                                       ──► 24V direct ──► Suppression pump
Supercap backup: 5F 5.4V ──► holds gas valve closed for 30s after power loss
```
- Average draw: 120mA @ 24V → mains powered
- Supercap ensures gas shutoff completes even during power failure
- NC (normally closed) solenoid = failsafe: power loss → valve closes

---

## Mechanical Design

### Hub Node
- Enclosure: 120×80×30mm ABS plastic (3D printed or injection molded)
- Wall-mountable (keyhole slots) or countertop stand
- 2.8" TFT visible through front window
- Piezo speaker port on top (105dB fire alarm capable)
- USB-C port on bottom
- External SMA antenna connector for Sub-GHz

### Fridge Node
- Form factor: 150×40×25mm (fits on fridge interior top shelf)
- IP54 enclosure, cold-rated PETG plastic
- Two camera modules on flexible arms (adjustable viewing angle)
- 4 load cell pads (integrated into fridge shelves, 3mm thin)
- Magnetic mount (sticks to fridge ceiling or wall)
- USB-C charging port behind magnetic flap
- LED illumination bar (illuminates fridge for camera when door opens)

### Pantry Node
- Form factor: 180×60×30mm (mounts on pantry door interior)
- Barcode scanner window on front face (scan items as you put them in)
- 1.3" OLED shows item count, expiry alerts
- Camera module on articulated arm (adjustable shelf view)
- 6× load cell pads (integrate under pantry shelves)
- Magnetic door sensor for auto-scan on open/close
- Lazy susan servo motor (rotates round shelf for full camera coverage)

### Stove Guard Node
- Form factor: 80×80×25mm (mounts under range hood or on wall behind stove)
- MLX90640 thermal camera with 60° FOV (views all 4 burners)
- Gas solenoid valve installed in-line with gas supply (behind stove, 1/2" NPT)
- Fire suppression nozzle (targeted, aimed at stovetop from above)
- Potassium bicarbonate cartridge (100g, replaceable, food-safe)
- 105dB piezo siren
- Smoke detector chamber integrated
- Status LEDs: green (safe), yellow (warning), red (FIRE)
- Hardwired 24V power supply (building electrical circuit)

---

## Full BOM

### Hub Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | RP2040 | QFN-56 7×7 | 1 | $1.20 | $1.20 |
| 2 | ESP32-C6-MINI-1 | Module | 1 | $3.20 | $3.20 |
| 3 | SX1262 | QFN-24 | 1 | $4.50 | $4.50 |
| 4 | 2.8" IPS TFT (ILI9341) | Module | 1 | $6.80 | $6.80 |
| 5 | 32MB W25Q256 | SOIC-8 | 1 | $1.80 | $1.80 |
| 6 | SD card slot | Micro push-push | 1 | $0.50 | $0.50 |
| 7 | MCP73831 | SOT-23-5 | 1 | $0.40 | $0.40 |
| 8 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 9 | AP6212-1.8 | SOT-23-5 | 1 | $0.35 | $0.35 |
| 10 | Lipo 2000mAh | Custom | 1 | $4.50 | $4.50 |
| 11 | USB-C receptacle | 16-pin SMD | 1 | $0.35 | $0.35 |
| 12 | SMA connector | Edge-mount | 1 | $0.80 | $0.80 |
| 13 | Antenna 868MHz | Wire/PCB | 1 | $1.50 | $1.50 |
| 14 | Piezo buzzer | 12mm SMD | 1 | $0.40 | $0.40 |
| 15 | MAX9814 microphone module | Module | 1 | $1.20 | $1.20 |
| 16 | Passives (R/C/L/inductors) | 0402 | ~60 | $1.50 | $1.50 |
| 17 | PCB 4-layer | 120×80mm | 1 | $3.00 | $3.00 |
| | | | | **Subtotal** | **$32.10** |

### Fridge Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | STM32L476RG | LQFP-64 | 1 | $5.80 | $5.80 |
| 2 | SX1261 | QFN-24 | 1 | $3.80 | $3.80 |
| 3 | OV5640 camera module | Module | 2 | $3.50 | $7.00 |
| 4 | SGP40 VOC sensor | DFN-6 | 1 | $2.80 | $2.80 |
| 5 | SCD30 CO2 sensor | Module | 1 | $12.00 | $12.00 |
| 6 | SHT40 temp/humidity | DFN-4 | 1 | $1.80 | $1.80 |
| 7 | MQ-3 alcohol/ethylene | Module | 1 | $2.50 | $2.50 |
| 8 | HX711 24-bit ADC | SOIC-16 | 4 | $0.80 | $3.20 |
| 9 | Load cell 5kg | Bar type | 4 | $2.00 | $8.00 |
| 10 | TSL2591 light sensor | Module | 1 | $3.50 | $3.50 |
| 11 | WS2812B LED strip | 60 LEDs/m × 0.3m | 1 | $1.50 | $1.50 |
| 12 | TPS62740-3.3 buck converter | SON-12 | 1 | $1.80 | $1.80 |
| 13 | AP6212-1.8 | SOT-23-5 | 1 | $0.35 | $0.35 |
| 14 | Lipo 2200mAh (cold-rated) | Custom pouch | 1 | $6.00 | $6.00 |
| 15 | MCP73831 | SOT-23-5 | 1 | $0.40 | $0.40 |
| 16 | Antenna 868MHz | Wire/PCB | 1 | $1.50 | $1.50 |
| 17 | Cold-rated PETG enclosure | 3D printed | 1 | $4.00 | $4.00 |
| 18 | Magnetic mount + arms | Assembly | 1 | $3.00 | $3.00 |
| 19 | USB-C magnetic connector | Module | 1 | $2.00 | $2.00 |
| 20 | Passives | 0402 | ~50 | $1.20 | $1.20 |
| 21 | PCB 4-layer | 150×40mm | 1 | $3.50 | $3.50 |
| | | | | **Subtotal** | **$76.15** |

### Pantry Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | ESP32-S3-WROOM-1 | Module | 1 | $3.50 | $3.50 |
| 2 | SX1261 | QFN-24 | 1 | $3.80 | $3.80 |
| 3 | OV2640 camera module | Module | 1 | $2.50 | $2.50 |
| 4 | HW-490 barcode scanner | Module | 1 | $8.00 | $8.00 |
| 5 | HX711 24-bit ADC | SOIC-16 | 6 | $0.80 | $4.80 |
| 6 | Load cell 10kg | Bar type | 6 | $3.00 | $18.00 |
| 7 | SHT40 temp/humidity | DFN-4 | 1 | $1.80 | $1.80 |
| 8 | SHT30 temp/humidity | DFN-4 | 3 | $1.20 | $3.60 |
| 9 | TSL2591 light sensor | Module | 2 | $3.50 | $7.00 |
| 10 | SH1106 1.3" OLED | Module | 1 | $2.50 | $2.50 |
| 11 | MG90S servo motor | Standard | 1 | $2.00 | $2.00 |
| 12 | WS2812B LED strip | 60 LEDs/m × 0.5m | 1 | $2.50 | $2.50 |
| 13 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 14 | LM2596-5V buck | TO-263 | 1 | $1.50 | $1.50 |
| 15 | Lipo 1200mAh | Custom pouch | 1 | $3.00 | $3.00 |
| 16 | MCP73831 | SOT-23-5 | 1 | $0.40 | $0.40 |
| 17 | USB-C receptacle | 16-pin SMD | 1 | $0.35 | $0.35 |
| 18 | Antenna 868MHz | Wire/PCB | 1 | $1.50 | $1.50 |
| 19 | Piezo buzzer | 12mm SMD | 1 | $0.40 | $0.40 |
| 20 | Magnetic door sensor | Module | 1 | $1.00 | $1.00 |
| 21 | Passives | 0402 | ~60 | $1.50 | $1.50 |
| 22 | PCB 4-layer | 180×60mm | 1 | $4.00 | $4.00 |
| | | | | **Subtotal** | **$78.45** |

### Stove Guard Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | STM32F411CE | UFQFPN-48 | 1 | $3.50 | $3.50 |
| 2 | SX1261 | QFN-24 | 1 | $3.80 | $3.80 |
| 3 | MLX90640 thermal camera | Module 32×24 | 1 | $28.00 | $28.00 |
| 4 | MQ-2 gas sensor (LPG) | Module | 1 | $1.50 | $1.50 |
| 5 | MQ-135 gas sensor (CO) | Module | 1 | $1.80 | $1.80 |
| 6 | MQ-137 gas sensor (NH3) | Module | 1 | $2.00 | $2.00 |
| 7 | RE46C190 smoke detector IC | DIP-8 | 1 | $1.20 | $1.20 |
| 8 | VS1838B IR receiver (modified) | Module | 1 | $0.80 | $0.80 |
| 9 | DS18B20 temp probe | TO-92 | 2 | $1.50 | $3.00 |
| 10 | Solenoid gas valve (24V NC, 1/2" NPT) | Industrial | 1 | $18.00 | $18.00 |
| 11 | Micro-pump (6V DC) | Module | 1 | $4.00 | $4.00 |
| 12 | Potassium bicarbonate cartridge | 100g | 1 | $3.00 | $3.00 |
| 13 | 105dB piezo siren | Module | 1 | $2.50 | $2.50 |
| 14 | IRLZ44N MOSFET | TO-220 | 2 | $0.60 | $1.20 |
| 15 | LM2596-5V buck | TO-263 | 1 | $1.50 | $1.50 |
| 16 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 17 | 5F 5.4V supercapacitor | Through-hole | 1 | $3.50 | $3.50 |
| 18 | 24V 1A power supply | DIN-rail | 1 | $6.00 | $6.00 |
| 19 | Antenna 868MHz | Wire/PCB | 1 | $1.50 | $1.50 |
| 20 | Status LED RGB | 0806 | 1 | $0.20 | $0.20 |
| 21 | Passives | 0402 | ~40 | $1.00 | $1.00 |
| 22 | PCB 4-layer | 80×80mm | 1 | $3.00 | $3.00 |
| 23 | Fire-rated ABS enclosure | Injection molded | 1 | $5.00 | $5.00 |
| 24 | Suppression nozzle + tubing | Custom | 1 | $3.00 | $3.00 |
| | | | | **Subtotal** | **$104.60** |

### System Total (1 hub + 1 fridge + 1 pantry + 1 stove guard)

**Hardware BOM: ~$291.30**

---

## Software Stack

### Cloud Dashboard (React + FastAPI)

```
software/dashboard/
├── frontend/              # React + Vite + TailwindCSS
│   ├── src/
│   │   ├── components/    # Inventory cards, expiry timeline, fire status
│   │   ├── hooks/         # Real-time MQTT subscription
│   │   ├── pages/         # Dashboard, Inventory, Shopping, Recipes, Fire Log
│   │   └── App.tsx
│   └── package.json
├── backend/               # FastAPI (Python)
│   ├── main.py            # REST + WebSocket server
│   ├── models.py          # SQLAlchemy inventory/sensor/fire models
│   ├── mqtt_bridge.py     # MQTT → DB + WebSocket relay
│   ├── inventory_engine.py # Inventory tracking, expiry management
│   ├── shopping_engine.py  # Grocery list generation
│   ├── recipe_engine.py    # Recipe suggestion from expiring items
│   ├── fire_logger.py      # Fire event logging + reporting
│   └── requirements.txt
└── docker-compose.yml     # Postgres + Mosquitto + API + Frontend
```

### ML Pipeline (Python)

```
software/ml-pipeline/
├── train_food_detection.py    # Train EfficientDet-D0 for food recognition
├── train_spoilage.py          # Train 1D-CNN+GRU spoilage predictor
├── train_fire_thermal.py      # Train tiny MobileNet V1 for fire vs. cooking
├── train_grocery_pattern.py   # Train Transformer for purchase prediction
├── export_tflite.py           # Convert → TFLite INT8 for hub/stove MCU
├── inference_server.py        # Cloud inference for camera frames
├── barcode_db.py              # UPC database lookup + product info
├── datasets/                  # Training data format specs
└── requirements.txt
```

### Mobile App (React Native)

```
software/mobile-app/
├── App.tsx                # Navigation: Home, Inventory, Shopping, Recipes, Fire Safety
├── screens/
│   ├── InventoryView.tsx  # Fridge + pantry inventory with photos
│   ├── ExpiryTimeline.tsx # What's expiring, when, and what to cook
│   ├── ShoppingList.tsx   # Auto-generated grocery list
│   ├── RecipeSuggest.tsx  # AI recipe suggestions from available items
│   ├── FireStatus.tsx     # Stove guard status, fire history
│   └── SetupWizard.tsx    # First-time kitchen configuration
├── services/
│   ├── ble.ts             # Direct BLE connection to hub
│   ├── mqtt.ts            # Cloud MQTT subscription
│   └── push.ts            # FCM/APNs push notifications
└── package.json
```

---

## Kitchen Intelligence Database

Built into the cloud backend, accessible from dashboard and mobile app:

### Food Freshness Thresholds

| Food | Fridge Life | Pantry Life | Spoilage Signs (gas) | Storage Temp |
|------|-------------|-------------|---------------------|--------------|
| Milk | 5-7 days | N/A | Sour VOC, CO2 ↑ | 2-4°C |
| Chicken | 1-2 days | N/A | H2S, amine VOC | 0-4°C |
| Berries | 3-7 days | N/A | Ethylene ↑, ethanol | 2-4°C |
| Bananas | 5-7 days | 3-5 days | Ethylene ↑↑ | 12-15°C |
| Cheese | 2-4 weeks | N/A | Butyric acid, NH3 | 2-4°C |
| Eggs | 3-5 weeks | N/A | H2S | 2-4°C |
| Bread | N/A | 3-7 days | Ethanol, mold VOC | Room temp |
| Rice (cooked) | 4-6 days | N/A | Ethanol, lactic acid | 2-4°C |
| Onions | N/A | 2-3 months | Ethylene, sulfur VOC | Cool, dark |
| Tomatoes | 5-7 days | 3-5 days | Ethylene ↑, ethanol | 12-15°C |

### Fire Safety Thresholds

| Parameter | Warning | Critical | Action |
|-----------|---------|----------|--------|
| Pan surface temp | >200°C | >260°C | >260°C auto-shutoff |
| Oil temp (inferred) | >180°C | >220°C | >220°C = flash point, auto-shutoff |
| LPG concentration | >300 ppm | >1000 ppm | >1000 ppm = gas shutoff + alarm |
| CO concentration | >35 ppm | >100 ppm | >100 ppm = alarm + ventilation |
| Smoke density | Medium | High | High = gas shutoff + suppression |
| Unattended cooking | 10 min | 20 min | 20 min = auto gas shutoff |

---

## Alert System

| Level | Condition | Action |
|-------|-----------|--------|
| INFO | Item approaching expiry (3 days) | Dashboard notification |
| WARNING | Item expires today / gas slight anomaly | Push notification + dashboard highlight |
| URGENT | Item expired / moderate gas reading | Push notification + buzzer beep + suggest recipe |
| CRITICAL | Food spoilage detected / gas leak threshold | Push + SMS + continuous alarm + suggest disposal |
| EMERGENCY | Fire detected / gas explosion risk | Push + SMS + call + siren + auto gas shutoff + suppression |

---

## Getting Started

### Hardware Assembly
See `docs/assembly_guide.md` for detailed step-by-step instructions for each node.

### Flash Firmware
```bash
# Hub node (RP2040)
cd firmware/hub-node
mkdir build && cd build
cmake .. -DPICO_BOARD=pico
make -j4
# Flash via USB: hold BOOTSEL, copy .uf2 to RPI-RP2 drive

# Hub node (ESP32-C6)
cd firmware/hub-node/esp32
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyUSB0 flash

# Fridge node (STM32)
cd firmware/fridge-node
mkdir build && cd build
cmake .. -DTARGET=stm32l476rg
make -j4
# Flash via ST-Link: st-flash write fresh_keep_fridge.bin 0x08000000

# Pantry node (ESP32-S3)
cd firmware/pantry-node
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB1 flash

# Stove guard node (STM32)
cd firmware/stove-guard
mkdir build && cd build
cmake .. -DTARGET=stm32f411
make -j4
# Flash via ST-Link: st-flash write fresh_keep_stove.bin 0x08000000
```

### Cloud Dashboard
```bash
cd software/dashboard
docker-compose up -d
# Access at http://localhost:3000
```

### Mobile App
```bash
cd software/mobile-app
npm install
npx react-native run-android  # or run-ios
```

### Barcode Database
```bash
# Download Open Food Facts database for barcode lookups
cd scripts
python3 download_barcode_db.py
# This creates a local SQLite database of 2M+ products
```

---

## Directory Structure

```
fresh-keep/
├── README.md
├── schematic/
│   ├── hub-node/           # KiCad project for hub
│   ├── fridge-node/        # KiCad project for fridge sensor
│   ├── pantry-node/        # KiCad project for pantry scanner
│   └── stove-guard/        # KiCad project for stove guard
├── firmware/
│   ├── hub-node/           # RP2040 + ESP32-C6 firmware
│   ├── fridge-node/        # STM32L476 firmware
│   ├── pantry-node/        # ESP32-S3 firmware
│   ├── stove-guard/        # STM32F411 firmware
│   └── common/             # Shared mesh protocol, CRC, packet defs
├── hardware/
│   ├── bom/                # BOM.csv per node
│   ├── enclosure/          # 3D-printable STEP/STL files
│   └── gerbers/            # Production gerber files
├── software/
│   ├── dashboard/          # React + FastAPI web app
│   ├── ml-pipeline/        # Training scripts for food, spoilage, fire models
│   └── mobile-app/         # React Native mobile app
├── scripts/                # Calibration, deployment, OTA, barcode DB
└── docs/                   # Assembly, API, protocol, architecture docs
```

---

## Key Differentiators

| Feature | FreshKeep | Smart Fridge | Kitchen Timer | Food Tracking App |
|---------|-----------|-------------|---------------|------------------|
| Food inventory | ✅ Auto (cameras + weight) | ❌ Manual entry | ❌ | ❌ Manual |
| Spoilage prediction | ✅ Gas sensors + ML | ❌ | ❌ | ⚠️ By date only |
| Fire prevention | ✅ Thermal + gas + auto-shutoff | ❌ | ⚠️ Timer only | ❌ |
| Grocery list | ✅ Auto-generated | ❌ | ❌ | ⚠️ Manual |
| Recipe suggestion | ✅ From expiring items | ❌ | ❌ | ⚠️ Basic |
| Barcode scanning | ✅ Auto | ❌ | ❌ | ⚠️ Manual scan |
| Works without WiFi | ✅ Mesh network | ❌ | ❌ | ❌ |
| Fire suppression | ✅ Targeted | ❌ | ❌ | ❌ |
| Real cost | ~$291 | $2000+ | $20 | $0 (manual) |

---

## Safety Certifications & Considerations

- **Stove Guard**: Designed to supplement, not replace, existing smoke/fire detectors
- **Gas shutoff valve**: Normally-closed (NC) — failsafe closes on power loss
- **Fire suppression**: Potassium bicarbonate (baking soda) — food-safe, non-toxic
- **Supercap backup**: Ensures gas shutoff completes even during power failure
- **Thermal array**: MLX90640 is non-contact, passive infrared — no privacy concerns
- **Privacy**: No visual camera on stove guard — only thermal array (32×24 pixels)
- **Compliance**: Designed to meet UL 858 (electric ranges), IEC 60335-2-6 (cooking appliances)

---

*FreshKeep: Stop wasting food. Stop worrying about kitchen fires. Let your kitchen take care of itself.*