# FoodAllergySync

**AI-powered food allergy safety, contamination prevention, and anaphylaxis preparedness system** — a multi-node household and caregiver platform that verifies packaged foods, checks prep surfaces for hidden allergens, keeps school/work lunchboxes safe, and makes sure epinephrine auto-injectors are present, in-range, and not expired.

## What It Solves

Food allergies affect millions of children and adults every day, and the hardest part is not only emergency response — it is **preventing small mistakes from becoming dangerous exposures**.

- **Ingredient labels are dense and inconsistent** — allergen statements, alternate ingredient names, and facility warnings are easy to miss when busy.
- **Cross-contact happens invisibly** — a "safe" meal can become unsafe on a contaminated counter, cutting board, lunch container, or shared appliance.
- **Lunches leave the safe kitchen** — school, daycare, sports, offices, and travel introduce swaps, temperature abuse, and uncertainty about what was packed.
- **Epinephrine compliance is fragile** — auto-injectors are forgotten, overheated in cars, buried in bags, or quietly expire.
- **Families need one shared source of truth** — parents, caregivers, schools, and adults managing their own allergies need synchronized data, not sticky notes and guesswork.

**FoodAllergySync** closes the loop from **ingredient selection -> prep verification -> meal handoff -> emergency readiness**. It combines a kitchen hub, a meal scanner, a rapid allergen strip reader, a smart lunchbox beacon, and an EpiPen guardian tag. Together they identify likely allergens, quantify contamination test strips, verify safe-meal handoffs, track thermal safety, and escalate exposure risk in real time.

---

## System Architecture

```text
┌───────────────────────────────────────────────────────────────────────────────────┐
│                        FoodAllergySync Cloud / Edge Stack                         │
│ FastAPI + MQTT + PostgreSQL + object storage + ML pipeline                       │
│                                                                                   │
│ Models                                                                            │
│ • LabelNet OCR/Parser      - package OCR + ingredient/allergen extraction         │
│ • CrossContact XGBoost     - surface/utensil contamination risk                   │
│ • StripQuant CNN-Regressor - lateral-flow strip line intensity quantification     │
│ • LunchSafe LSTM           - lunchbox temp excursion and swap-risk forecasting     │
│ • ReadyGuard RF            - injector carry compliance / missed-readiness risk     │
│ • ExposureTimeline TFT     - 7-day exposure-risk and trigger pattern forecast     │
└──────────────────────────────────┬────────────────────────────────────────────────┘
                                   │ MQTT over TLS / HTTPS
                        ┌──────────┴──────────┐
                        │ FoodAllergySync Hub │
                        │ CM4 + RP2040 +      │
                        │ SX1262 coordinator  │
                        └──────┬───────┬──────┘
                               │       │
                  Sub-GHz 868  │       │ BLE / Wi-Fi commissioning
                               │       │
      ┌────────────────────────┼───────┼───────────────────────────┬────────────────┐
      │                        │       │                           │                │
┌─────┴────────────┐  ┌────────┴────┐  ┌───────────────┐  ┌────────┴──────┐  ┌──────┴─────────┐
│ Meal Scanner     │  │ Strip Reader │  │ SafeLunch     │  │ EpiPen Guard  │  │ Caregiver /    │
│ OCR+barcode+NIR  │  │ Dock         │  │ Beacon        │  │ Tag + cradle  │  │ mobile app     │
│ scale + UX       │  │ strip optics │  │ temp+NFC+IMU  │  │ temp+presence │  │ RN + push      │
└──────────────────┘  └──────────────┘  └───────────────┘  └───────────────┘  └────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **FoodAllergySync Hub** | Raspberry Pi CM4 + RP2040 + SX1262 | Local policy engine, MQTT bridge, caregiver dashboard, OTA, encrypted family profiles | 12V/3A wall input + 2-cell UPS | Ethernet, Wi-Fi, BLE 5.0, Sub-GHz 868 MHz |
| **Meal Scanner** | ESP32-S3-WROOM-1-N16R8 | Ingredient label capture, barcode lookup, OCR pre-processing, safe meal verification, guided scanning UX | 12V adapter | Wi-Fi setup, Sub-GHz 868 MHz |
| **Strip Reader Dock** | STM32WL55JC | Peanut/milk/egg/gluten strip reading, optical intensity quantification, test logging, calibration | USB-C 5V or 1x18650 | Sub-GHz 868 MHz |
| **SafeLunch Beacon** | nRF52840 + SX1262 | Lunchbox temp logging, safe meal NFC verification, lid-open audit trail, handoff reminders | 1200 mAh LiPo | BLE 5.0, Sub-GHz 868 MHz |
| **EpiPen Guard** | nRF52833 + SX1262 | Auto-injector presence, temperature excursion logging, expiry timer, panic button, carry compliance | CR123A or LiPo cradle | BLE 5.0, Sub-GHz 868 MHz |

---

## Daily User Experience

1. A parent scans a granola bar on the **Meal Scanner**.
2. Barcode + OCR ingredient parsing detect an undeclared risk phrase: **"processed on shared equipment with peanuts"**.
3. The scanner ring turns amber, the app shows the allergy profile affected, and the item is flagged as **not safe for Maya**.
4. A countertop swab is inserted into the **Strip Reader Dock** after lunch prep; the optical reader quantifies a low-but-real peanut residue line.
5. The app recommends re-sanitizing the board and re-testing before packing the lunch.
6. The approved lunch container's NFC sticker is verified by the **SafeLunch Beacon** before the lunchbox is zipped.
7. During the school day, the lunchbox remains below 5 °C and no unexpected lid-open event occurs.
8. At pickup time, the **EpiPen Guard** detects the injector was left in yesterday's bag and pushes an urgent carry reminder.
9. Overnight, the hub updates the family's exposure-risk timeline and caregiver summary.

---

## Node 1 - FoodAllergySync Hub

### Core hardware

- **Compute module:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, slot timing, power sequencing, and radio failover
- **Sub-GHz radio:** Semtech SX1262 at 868 MHz with matched whip antenna
- **Local storage:** 32 GB eMMC + industrial microSD log mirror
- **UI:** 5 inch DSI touchscreen, tri-color status tower LED, piezo buzzer, privacy mute switch
- **Backup:** dual 18650 UPS HAT for >2 h outage operation

### Responsibilities

- Maintains per-person allergy policies: peanut, tree nut, milk, egg, sesame, soy, wheat, shellfish, fish, gluten, custom allergens
- Bridges all node telemetry into MQTT topics and the FastAPI API
- Stores approved product catalog, meal templates, school rules, and caregiver contacts
- Runs edge inference fallback when internet access is unavailable
- Generates daily readiness scores: **meal safety**, **carry compliance**, **surface confidence**, **temperature integrity**

### Key interfaces

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 I2C0 | INA219, DS3231 RTC, buzzer expander |
| RP2040 UART0 | CM4 heartbeat channel |
| CM4 DSI | 5 inch touch display |
| CM4 Ethernet | primary uplink |
| CM4 USB2 | backup export / service |

---

## Node 2 - Meal Scanner

### Hardware architecture

- **SoC:** ESP32-S3-WROOM-1-N16R8
- **Camera:** OV5640 autofocus camera for ingredient panel and front-of-pack capture
- **Barcode engine:** GM65 1D/2D UART scanner
- **Spectral sensor:** AMS AS7341 for simple package/surface spectral fingerprinting and tamper context
- **Weight verification:** 5 kg load cell + HX711 to verify packed portion and lunch assembly
- **Distance sensor:** VL53L1X for scan positioning guidance
- **UX:** 2.4 inch SPI TFT, WS2812B ring, capacitive confirm/reject pads, speaker
- **Optional NFC:** PN532 to bind approved containers and recurring safe meals

### Why these parts

- **ESP32-S3** provides enough RAM and vector instructions for edge pre-processing and simple OCR assistance.
- **OV5640** gives better text readability than cheaper 2 MP modules.
- **AS7341** helps distinguish glossy package glare and can support future strip/surface accessory modes.
- **HX711 + load cell** closes the loop on portion verification, useful for schools and food challenge protocols.

### Pin assignment

| Signal | ESP32-S3 Pin | Peripheral |
|--------|--------------|------------|
| I2C SDA | GPIO8 | AS7341, VL53L1X |
| I2C SCL | GPIO9 | AS7341, VL53L1X |
| UART1 TX/RX | GPIO17 / GPIO18 | GM65 barcode engine |
| SPI TFT MOSI/SCLK | GPIO11 / GPIO12 | ST7789 display |
| TFT CS/DC/RST | GPIO10 / GPIO13 / GPIO14 | ST7789 display |
| HX711 DT/SCK | GPIO4 / GPIO5 | load cell front end |
| LED ring | GPIO21 | WS2812B |
| I2S speaker BCLK/LRCLK/DOUT | GPIO35 / GPIO36 / GPIO37 | audio prompt amp |
| PN532 SPI | GPIO38-41 | NFC module |

---

## Node 3 - Strip Reader Dock

A compact kitchen/clinic dock that reads rapid allergen lateral-flow tests for peanut, milk, egg, gluten, and custom assays.

### Hardware

- **MCU/radio:** STM32WL55JC (Cortex-M4 + integrated Sub-GHz)
- **Optics:** AS7341 spectral sensor + TSL2591 lux sensor for line intensity normalization
- **Illumination:** high-CRI white LED + 470 nm blue LED + 850 nm IR LED on time-multiplexed drivers
- **Strip transport:** spring-loaded strip sled with endstop switch
- **Temperature:** TMP117 for assay temperature compensation
- **UX:** 1.54 inch monochrome e-paper + start button + haptic buzzer
- **Calibration:** removable white reference tile for weekly optical baseline checks

### Measurement flow

1. User inserts the strip cassette.
2. Endstop triggers timed incubation countdown.
3. LEDs strobe in sequence; sensor captures control/test line profiles.
4. Firmware computes normalized line ratios.
5. Dock sends raw line vectors + metadata to hub.
6. Edge model returns **negative / trace / positive / invalid** with confidence.

---

## Node 4 - SafeLunch Beacon

### Role

A lunchbox-mounted node for packed-meal custody, temperature safety, and approved-meal verification.

### Hardware

- **MCU:** nRF52840
- **Long-range radio:** SX1262
- **Temperature / humidity:** SHT41
- **Shock / lid events:** LIS2DW12 accelerometer + reed switch
- **NFC:** ST25DV dynamic tag reader for approved-container stickers
- **UX:** RGB LED, haptic coin motor, small piezo chirp
- **Battery:** 1200 mAh LiPo + MCP73831 charging

### Event logic

- Verifies the approved container sticker before departure
- Logs unsafe temperature excursions for milk/egg-containing meals
- Detects unexpected mid-day lid openings or bag drops
- Sends reminder if lunch is left on the counter past departure time

---

## Node 5 - EpiPen Guard

### Role

A bag-clip or wall-cradle node that makes sure rescue medication is **present, viable, and actually carried**.

### Hardware

- **MCU:** nRF52833
- **Long-range radio:** SX1262
- **Temperature:** TMP117 for storage excursion detection
- **Presence sensing:** hall sensor + cradle reed switch + load switch current sense
- **Motion:** BMA400 low-power accelerometer
- **User controls:** SOS button, acknowledge button, RGB status LED, buzzer
- **Battery:** CR123A for clip tag mode, or rechargeable LiPo cradle mode

### Readiness checks

- Injector present/absent
- Time remaining to expiry
- Overheat / freeze excursions
- Daily carry compliance score
- Panic button relay through hub and caregiver contacts

---

## Network and Protocol

FoodAllergySync uses a **deterministic 868 MHz TDMA mesh** with BLE used only for commissioning and direct mobile diagnostics.

- **Frame length:** up to 48 bytes application payload
- **CRC:** CRC-16/CCITT
- **Crypto:** AES-128 CTR payload encryption with rotating session nonce
- **Addressing:** 16-bit node IDs, 8-bit node type, household network key
- **Classes:** telemetry, events, alerts, commands, config, OTA chunks
- **High priority frames:** exposure alerts, SOS, injector missing, unsafe lunch temp, positive strip result

See [`docs/protocol_spec.md`](docs/protocol_spec.md) and [`firmware/common/protocol.h`](firmware/common/protocol.h).

---

## ML Stack

### 1. LabelNet OCR/Parser
Inputs:
- ingredient-panel image
- front-of-pack image
- barcode metadata
- household allergy profile

Outputs:
- detected allergens
- risk phrases ("may contain", "shared equipment", etc.)
- safe / caution / unsafe recommendation
- structured ingredient tokens

### 2. CrossContact XGBoost
Features:
- strip result history
- prep zone
- utensil / appliance usage chain
- meal type
- sanitation interval
- humidity and residue context

Outputs:
- contamination probability
- recommended re-clean / re-test urgency

### 3. StripQuant CNN-Regressor
Features:
- multi-channel optical line profile
- assay type
- ambient temperature

Outputs:
- normalized test/control ratio
- class: negative / trace / positive / invalid

### 4. LunchSafe LSTM
Features:
- lunchbox temperature curve
- handoff time
- lid-open events
- meal category
- school day schedule

Outputs:
- time-to-threshold forecast
- spoilage / swap suspicion score

### 5. ReadyGuard Random Forest
Features:
- injector carry pattern
- weekday context
- bag handoff history
- temperature excursions
- expiration proximity

Outputs:
- missed-readiness risk
- reminder timing recommendation

### 6. ExposureTimeline Transformer Forecast
Combines all node signals into a 7-day household risk outlook and personalized coaching sequence.

---

## Power Architecture Summary

| Node | Input | Main rails |
|------|-------|------------|
| Hub | 12V DC jack + UPS cells | 5V buck, 3V3 buck |
| Meal Scanner | 12V DC jack | 5V servo/peripheral rail, 3V3 digital rail |
| Strip Reader Dock | USB-C 5V or 18650 | 3V3 analog/digital rail, LED boost |
| SafeLunch Beacon | 1S LiPo | 3V3 buck-boost |
| EpiPen Guard | CR123A or 1S LiPo | 3V0/3V3 low-power rail |

---

## Safety & Regulatory Considerations

- Not a substitute for physician guidance or emergency medical services
- All emergency workflows are **assistive only** and must fail safely
- Optical strip reading should be validated per assay lot and storage conditions
- Temperature logging supports food safety coaching but does not certify sterility
- Radio design must comply with local ISM band regulations
- Data handling should align with HIPAA/GDPR-equivalent expectations where applicable

---

## Repository Layout

```text
FoodAllergySync/
├── README.md
├── schematic/
├── firmware/
├── hardware/bom/
├── software/dashboard/
├── software/ml-pipeline/
├── software/mobile-app/
├── docs/
└── scripts/
```

---

## Build Sequence

1. Assemble hub and radio HAT
2. Commission meal scanner over BLE
3. Calibrate strip reader with reference tile
4. Register allergy profiles and safe foods
5. Attach SafeLunch Beacon to lunchbox and pair NFC meal containers
6. Pair EpiPen Guard to clip tag or home cradle
7. Run `scripts/calibrate.py` and `scripts/deploy.sh`

---

## Included Deliverables

- KiCad-style schematic starter sheets for all five nodes
- C firmware for each node plus shared protocol implementation
- FastAPI + MQTT backend scaffold
- ML training pipeline scripts
- React Native mobile app scaffold
- Node-level BOM CSVs
- Architecture, API, and protocol documentation
- Deployment and calibration scripts
