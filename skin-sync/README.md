# SkinSync

**AI-powered multi-node skin health & sun-safety system.** Continuously tracks personal UV exposure with a wearable patch, scans skin with multispectral imaging to detect conditions (acne, hyperpigmentation, rosacea, eczema, early melanoma signs) weeks before they're visible to the eye, learns your Fitzpatrick skin type and personal sun-response curve, optimizes your skincare routine with per-product dosing, and predicts skin cancer risk from cumulative UV dose — so anyone can maintain healthy, youthful skin and catch problems early without guesswork, sunburns, or wasted products. Solves a problem that affects every earthling: skin is the body's largest organ, UV damage is cumulative and the #1 cause of premature aging and cancer, and 1 in 5 people will develop skin cancer in their lifetime — yet nobody tracks their UV dose or monitors their skin proactively.

---

## What It Does

SkinSync is a 4-node ambient + wearable system that turns personal skincare and sun safety from guesswork into a data-driven, continuously monitored health practice:

1. **Tracks personal UV exposure** — a wearable UV Patch (nRF52832 + VEML6075 UVA/UVB sensor + TMP117 skin temperature, Sub-GHz, coin-cell powered, 14-day battery) measures cumulative UV dose in real-time as you go about your day. It knows the difference between "walking to the car" (2 min, minimal) and "lunch outside" (45 min, significant) and builds a complete picture of your daily UV exposure — not just "did you get sun" but exactly how much UVA (aging) and UVB (burning) dose you accumulated, compared to your personal skin-type tolerance.

2. **Learns your skin type & sun-response curve** — the cloud ML pipeline learns your Fitzpatrick skin type (I-VI) from your first week of UV + skin-temp + burn/flush data, then builds a personal erythema (sunburn) threshold model. It predicts *your* minimal erythema dose (MED) — the UV amount that causes first visible redness — and warns you before you reach it. A Fitzpatrick II (fair) person burns at 200 J/m²; a Fitzpatrick V (brown) person tolerates 1000+ J/m². Generic "reapply every 2 hours" advice is replaced with personalized, real-time guidance.

3. **Scans skin for conditions & early signs** — a handheld Skin Scanner node (ESP32-S3 + OV5640 multispectral camera + white/UV/NIR/polarized LED ring) captures multispectral skin images. On-device + cloud ML classifies 25+ skin conditions: acne (comedonal, inflammatory, cystic), hyperpigmentation (melasma, PIH, sun spots), rosacea, eczema, seborrheic dermatitis, actinic keratosis (pre-cancer), and early melanoma signs (ABCDE asymmetry/border/color/diameter/evolving). UV fluorescence reveals bacterial activity (acne-causing *C. acnes* fluoresces orange-red under UV); NIR shows sub-surface inflammation and moisture levels; cross-polarized light eliminates surface glare to reveal true skin tone and lesion borders.

4. **Tracks skin aging over time** — longitudinal multispectral scans build a skin quality timeline: wrinkle depth (NIR surface topography), elasticity proxy (NIR backscatter), hydration (UV fluorescence quenching), and pigmentation index. The app shows your "skin age" vs. chronological age and tracks improvement from skincare products. See whether that $80 retinol is actually working over 12 weeks — with data, not hope.

5. **Optimizes your skincare routine** — the cloud ML learns which products and ingredients actually improve *your* skin (per-scan before/after comparison, ingredient interaction modeling). It recommends routine adjustments: "add a vitamin C serum in the morning (your PIH isn't fading with retinol alone)," "reduce niacinamide to 5% — your barrier is compromised at 10%," "skip exfoliation this week — your skin barrier score is low."

6. **Dispenses the right amount** — a Smart Dispenser node (ESP32-C6 + peristaltic pumps + precision load cells) dosed the exact recommended amount of each skincare product. No more wasting $60 serum by using 3 pumps when 1 pump covers your face. The dispenser tracks product usage, knows when you're running low, and auto-reorders. It also ensures you actually apply sunscreen every morning — and the UV patch verifies you re-applied when warned.

7. **Predicts skin cancer risk** — a temporal CNN fuses 90 days of UV exposure, skin-type, scan history, and lesion evolution to compute a personal skin cancer risk score. It flags changing moles (evolving border/color/diameter — the most specific melanoma indicator) and alerts you to see a dermatologist with a comparison image: "the mole on your left shoulder has grown 1.2mm and darkened in 30 days — see a dermatologist." Early melanoma detection has a 99% 5-year survival rate; late detection drops to 30%.

8. **Sun burn prevention, in real-time** — the UV Patch buzzes when you've reached 70% of your personal daily MED. "You have 15 minutes before you start burning — seek shade or reapply SPF." No more falling asleep at the beach and waking up red. The patch also warns about UVA (which penetrates clouds and windows — you're aging your skin driving without sunscreen).

9. **Sunscreen verification** — when the UV patch detects UV exposure but the app shows no sunscreen was dispensed that morning, it alerts: "You're getting UV exposure — did you apply sunscreen?" If sunscreen was dispensed, it tracks reapplication timing based on your activity (sweat/swim reduces SPF effectiveness faster).

10. **Dermatologist-ready reports** — the app generates a clinical report: skin type, UV exposure history (daily/weekly/monthly J/m²), skin condition timeline, lesion tracking (with change comparison images), product history, and risk scores. Bring it to your annual skin check and your dermatologist has data, not memory.

All UV Patches communicate over a Sub-GHz mesh (868/915 MHz, long range, low power, coin-cell for 14+ days). The Skin Scanner and Smart Dispenser are WiFi. The Mirror Hub bridges Sub-GHz mesh to WiFi/cellular for cloud analytics, ML inference, and the mobile app, and displays your morning skincare dashboard right on your bathroom mirror.

### The Problem It Solves

- **UV damage is cumulative and invisible:** Every sunburn doubles your risk of melanoma. UVA penetrates clouds and windows, causing 80% of visible skin aging (photoaging). Yet nobody knows their actual daily UV dose — "I was only outside for a few minutes" adds up. SkinSync measures it continuously, personally, and warns before damage occurs.
- **Skin cancer is the most common cancer:** 1 in 5 Americans will develop skin cancer. Melanoma rates have doubled since 1982. Early detection is the difference between 99% and 30% survival. Nobody tracks their moles systematically — by the time you notice a change, it may be late. SkinSync photographs and tracks every lesion over time.
- **Skincare is guesswork:** The average person spends $300+/year on skincare and has no idea if it's working. Products are chosen based on marketing, not data. SkinSync's multispectral scans objectively measure whether products improve *your* skin — because everyone's skin biology is different.
- **Sunscreen advice is generic:** "Reapply every 2 hours" ignores skin type, UV intensity, activity, and sweat. A Fitzpatrick II person in July sun burns in 10 minutes unprotected; a Fitzpatrick V person has 50+ minutes. SkinSync personalizes everything to your skin type, location, and real-time UV index.
- **Nobody monitors their skin proactively:** People notice skin problems when they're already visible — a breakout, a dark spot, a changing mole. Multispectral imaging reveals sub-surface inflammation, bacterial activity, and pigmentation changes weeks before they surface. Early intervention prevents the problem entirely.
- **Product waste is enormous:** Most people use 2-3× too much product (serums, creams, sunscreen). The Smart Dispenser delivers the exact dermatologist-recommended amount per product type — saving money and preventing over-application irritation.
- **Dermatologist visits lack data:** Your annual skin check relies on memory and visual inspection. SkinSync provides a structured history: UV dose, lesion changes (with comparison photos), condition progression, and product response — making each visit data-driven.

SkinSync measures your UV exposure continuously, scans your skin for problems before they're visible, tracks every mole for change, optimizes your skincare with data, and tells you exactly when to seek professional care — so your skin stays healthy for life.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         SKINSYNC SYSTEM                                           │
│                                                                                    │
│  ┌─────────────────────┐ Sub-GHz mesh ┌──────────────────────┐                     │
│  │ UV PATCH ×1-2        │◄───────────►│                        │                     │
│  │ (wrist/shoulder)      │  868/915MHz │    MIRROR HUB          │──── WiFi6 ──►Cloud │
│  │ nRF52832 + VEML6075  │             │    (RP2040 +           │             Dashboard│
│  │ UVA/UVB + TMP117     │             │     ESP32-C6)          │             + ML     │
│  │ skin temp + coin cell│             │                        │             Pipeline│
│  │ 14-day battery        │             │  Edge: UV dose calc     │             + Skin DB │
│  └─────────────────────┘             │   condition preview     │─── BLE ───► Mobile   │
│                                       │  TFT: morning routine   │             (React   │
│  ┌─────────────────────┐             │   + UV dose bar         │              Native) │
│  │ SKIN SCANNER          │── WiFi6 ──►│                        │                     │
│  │ (handheld)            │            │  Speaker: sun alerts    │                     │
│  │ ESP32-S3 + OV5640     │            └───────────┬────────────┘                     │
│  │ multispectral        │                        │ WiFi                             │
│  │ + white/UV/NIR/pol    │            ┌──────────▼─────────────┐                     │
│  │ Edge: condition CNN   │            │  SMART DISPENSER        │                     │
│  │ + melanoma ABCDE      │            │  (countertop)           │                     │
│  └─────────────────────┘            │  ESP32-C6 + peristaltic  │                     │
│                                       │  pumps + load cells     │                     │
│                                       │  per-product dosing     │                     │
│                                       └────────────────────────┘                     │
│                                                                                    │
│  ┌──────────────────────────────────────────────────────────────────────────────┐ │
│  │                    CLOUD / EDGE SOFTWARE                                       │ │
│  │  ┌──────────┐  ┌───────────────┐  ┌───────────────────────┐                 │ │
│  │  │Dashboard │  │ ML Pipeline   │  │ Mobile App            │                 │ │
│  │  │ (FastAPI)│  │ Condition CNN │  │ UV dose tracker        │                 │ │
│  │  │ + Skin  │  │ Melanoma ABCDE │  │ Skin scan results      │                 │ │
│  │  │   DB    │  │ UV risk model │  │ Skincare routine       │                 │ │
│  │  │ Product │  │ Routine optim  │  │ Lesion tracker         │                 │ │
│  │  │ library │  │ Skin age model │  │ Derm report export     │                 │ │
│  │  └──────────┘  └───────────────┘  └───────────────────────┘                 │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Mirror Hub (1 per system)

The brain. Mounted on or integrated into a bathroom mirror. Bridges the Sub-GHz mesh to WiFi/cloud, displays your morning skincare dashboard (today's UV forecast, skin condition status, recommended products + amounts), runs edge UV-dose calculation + condition preview, and sends dispensing commands to the Smart Dispenser.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + edge ML + display; ESP32-C6 handles WiFi6 + Sub-GHz co-proc |
| Radio | nRF52832 (Sub-GHz via SX1262) + ESP32-C6 BLE | Sub-GHz mesh to UV patches; BLE to mobile app |
| Display | 5.0" IPS TFT (ILI9488, 480×320) | Morning dashboard: UV forecast, skin condition, product queue, sun burn countdown |
| Storage | W25Q256 32MB Flash + MicroSD | Skin profile cache, 90-day ring buffer of UV data, scan thumbnails, OTA |
| RTC | PCF8563 + CR1220 | Timekeeping for UV dose accumulation without WiFi |
| Audio | MAX98357A + 28mm speaker | Sun alerts ("UV high — apply sunscreen"), morning routine chime |
| Power | 5V USB-C + LiPo 2500mAh backup | Bathroom-mounted, mains-powered normally |
| LEDs | WS2812 RGB + 4× SMD | System state: green=all good, amber=UV warning, red=lesion change alert |
| Sensors | BME280 (bathroom temp/humidity) | Bathroom humidity affects skin barrier; humidity-linked skincare adjustments |
| Connectors | 2× I2C, 2× UART, 6× GPIO | Expansion (additional dispenser units, bathroom scale) |

**Mirror Hub firmware responsibilities:**
- Maintain Sub-GHz mesh network with all UV patches
- Aggregate per-patch UVA/UVB dose, skin temp every 1 min (active) / 5 min (sleeping)
- Run edge UV-dose calculation (erythema effectiveness spectrum × dose → MED fraction)
- Display morning dashboard on TFT: today's UV index forecast, skin condition summary, product queue with amounts, "your MED today: 35% used"
- Trigger dispensing: send product + amount commands to Smart Dispenser
- Buffer 90 days of UV + scan data locally (SD card) for cloud sync
- MQTT over WiFi to cloud; BLE to mobile app for instant alerts
- Sun burn alert: when patch reports MED fraction > 70%, buzz patch + push notification

### 2. UV Patch Node (1-2 per system — wrist/shoulder wearable)

The sentinel. Worn on the wrist (like a watch) or shoulder (clip-on). Measures UVA + UVB + skin temperature continuously. Buzzes when you approach your personal sunburn threshold.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52832 | Sub-GHz mesh + sensor sampling + 14-day coin-cell life |
| Radio | SX1262 (Sub-GHz) | 868/915 MHz, 50m indoor range, ultra-low-power |
| UV Sensor | VEML6075 | UVA (320-400nm) + UVB (280-320nm) separately, ±10% accuracy |
| Skin Temp | TMP117 | ±0.1°C medical-grade skin temperature (for flush/burn detection) |
| Ambient Light | LTR390 | UV index + ambient lux (indoor/outdoor detection) |
| Haptic | DRV2605L + LRA | Sun-burn-warning vibration (different patterns: 1 pulse = 50% MED, 2 pulses = 70%, 3 pulses = 90%) |
| Power | CR2477 coin cell (1Ah) | 14+ days at 1-min UV sampling + Sub-GHz TX |
| LEDs | 1× SMD | Pairing + low-battery indicator |
| Antenna | PCB trace antenna (868/915 MHz) | Compact, no external antenna needed |
| Enclosure | 35×25×8mm silicone wristband or clip | Skin-contact: UV sensor faces up/out, temp sensor faces skin |

**UV Patch firmware responsibilities:**
- Sample UVA + UVB (VEML6075, 3 reads averaged) every 1 min when active (outdoor/light detected), every 5 min when sleeping (dark/indoor)
- Compute UVA dose (J/m²) and UVB dose (J/m²) accumulators
- Skin temp monitoring: detect flush (>2°C rise from baseline = possible sunburn onset)
- Sub-GHz TX every 5 min (dose deltas + current UV index + skin temp)
- Haptic alert: when cumulative MED fraction crosses 50/70/90% thresholds (computed from personal MED received from hub)
- Deep sleep (~14 µA) between samples for 14-day coin-cell life
- Pairing mode (button press → BLE advertisement for app pairing)

### 3. Skin Scanner Node (1 per system, handheld)

The diagnostician. Handheld multispectral skin imager. Captures white/UV/NIR/polarized images for condition classification, lesion tracking, and skin quality measurement.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 (N16R8) | Camera driver + WiFi + on-device condition CNN (TFLite Micro) |
| Camera | OV5640 (5MP, autofocus) | High-res skin capture with 4 illumination modes |
| LED Ring | White 5500K + 365nm UV + 850nm NIR + cross-polarized white | Four modes: visible (surface), UV (bacterial fluorescence), NIR (sub-surface inflammation + moisture), polarized (true skin tone, eliminates glare) |
| Display | 1.3" OLED (SH1106) | Preview + scan result + condition classification |
| Storage | MicroSD | Image archive (scan progression over time) |
| Power | 18650 LiPo 2600mAh + USB-C | ~150 scans per charge; handheld |
| Buttons | 4× tactile | Capture / mode (white/UV/NIR/polarized) / lesion mark / identify |
| IMU | LSM6DSL | Orientation for consistent scan angles (for reproducible lesion tracking) |

**Skin Scanner firmware responsibilities:**
- Capture multispectral image set (white + UV + NIR + polarized, 4 shots in 3 seconds)
- Run on-device condition CNN (25 conditions, ~3s inference) → display result + confidence
- Run on-device melanoma ABCDE pre-screen (TFLite Micro binary "benign/suspect")
- Lesion tracking mode: re-photograph a marked lesion at same angle/distance (IMU-guided) for change detection
- Upload full multispectral set to cloud for high-res condition CNN + dermatologist report
- OLED preview + result display; SD card archive with timestamp + body-location tag
- WiFi to hub/cloud (no Sub-GHz — image data is too large for mesh)

### 4. Smart Dispenser Node (1 per system, countertop)

The actuator. Dispenses the exact recommended amount of each skincare product. Tracks usage and inventory.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-C6 | WiFi + Sub-GHz receiver + pump control |
| Radio | SX1262 (Sub-GHz) | Receives dispensing commands from hub (no WiFi needed if near hub) |
| Pumps | 4× peristaltic micro-pump (3V DC) | One per product slot (cleanser, serum, moisturizer, sunscreen) |
| Load Cells | 4× HX711 + 5kg load cell | Per-product weight tracking (usage + remaining inventory) |
| Valves | 4× normally-closed solenoid | Prevent drip after dispensing |
| Display | 0.96" OLED (SSD1306) | Per-product remaining % + last dispensed amount |
| Power | 5V USB-C + 18650 backup | Pumps need 3V; MCU on 3.3V LDO; battery for outage |
| Buttons | 4× tactile | Manual dispense per slot (override app) |
| Slots | 4× removable cartridges | User fills with their products; each cartridge is RFID-tagged with product ID |

**Smart Dispenser firmware responsibilities:**
- Receive dispensing commands from hub (product slot + amount in ml/mg)
- Run peristaltic pump for computed duration (calibrated ml/sec per slot)
- Monitor load cell: confirm amount dispensed (±0.1g), detect empty cartridge
- Report dispensing result (product, amount, timestamp) to hub
- Safety: max dispensing time per slot (prevent over-dispensing)
- RFID reader: identify which product is in each slot (auto-loads product profile)
- Low-product alert: notify hub when cartridge < 15% remaining
- Auto-reorder: hub sends reorder notification to cloud when product is projected to run out in 7 days

---

## Communication Protocol

**UV Patches ↔ Hub:** Sub-GHz mesh (868 MHz EU / 915 MHz US) via SX1262. Long-range (50m indoor), ultra-low-power (coin-cell), binary protocol with CRC16.

**Skin Scanner → Hub/Cloud:** WiFi6 (ESP32-S3). Image data is too large for Sub-GHz. Scanner uploads multispectral sets directly to cloud; hub gets a notification.

**Hub ↔ Smart Dispenser:** Sub-GHz (SX1262) or WiFi6 (ESP32-C6). Low-latency dispensing commands.

**Hub → Cloud:** WiFi6 (ESP32-C6), MQTT over TLS. UV dose telemetry, scan results, dispensing logs, risk scores.

**Hub → Mobile App:** BLE 5.3 (instant alerts) + WiFi (full sync via cloud).

See [docs/protocol.md](docs/protocol.md) for the full frame specification.

---

## ML Pipeline

1. **Skin Condition Classifier** — Multispectral (white+UV+NIR+polarized) skin image CNN. 25+ condition classes: acne (comedonal, inflammatory, cystic), hyperpigmentation (melasma, post-inflammatory hyperpigmentation, solar lentigines), rosacea (erythematotelangiectatic, papulopustular), eczema, seborrheic dermatitis, actinic keratosis, basal cell carcinoma signs, squamous cell carcinoma signs, melasma, vitiligo, fungal acne, dermatitis, psoriasis (facial), perioral dermatitis, folliculitis, milia, xerosis, keratosis pilaris, skin barrier damage, seborrheic keratosis, angioma, telangiectasia, hyperpigmentation clusters. Architecture: EfficientNet-Lite + 4-channel multispectral input → 25-class output. Cloud inference (~20MB); edge gets a binary "normal/suspect" pre-screen (TFLite Micro, <200KB).

2. **Melanoma ABCDE Detector** — Lesion image analysis for melanoma warning signs: Asymmetry (shape analysis), Border irregularity (edge Fourier descriptors), Color variegation (multi-color pixel clustering), Diameter (>6mm threshold), Evolution (comparison with prior scan — the most specific indicator). YOLOv8-nano for lesion detection + custom CNN for ABCDE scoring. Outputs a 0-100 risk score + "see dermatologist" recommendation at >50. Edge: binary "benign/suspect" pre-screen.

3. **UV Risk Model** — Personal UV dose → erythema (burn) risk model. Uses the ISO 17166 erythema effectiveness spectrum to convert measured UVA+UVB irradiance to effective dose (SED). Learns personal MED from skin-type + first-burn data. Predicts hours-to-burn at current UV index + remaining outdoor time. Also computes cumulative annual UV dose for skin cancer risk correlation.

4. **Skincare Routine Optimizer** — Per-user ingredient efficacy model. Tracks skin condition scores over time correlated with product usage (from dispenser logs). Recommends routine changes: add/remove/adjust products, ingredient concentrations, application timing. Models ingredient interactions (retinol + AHA irritation, vitamin C + niacinamide compatibility, SPF + retinol timing). Personalized — what works for *your* skin, not generic advice.

5. **Skin Age Model** — Longitudinal skin quality scoring from multispectral scans. Computes a "skin age" from wrinkle depth (NIR topography), elasticity proxy (NIR backscatter pattern), hydration (UV fluorescence), pigmentation index (polarized tone analysis), and pore size. Tracks change over time; shows whether products are improving skin age vs. chronological age.

See [software/ml-pipeline/](software/ml-pipeline/) for training scripts.

---

## Mobile App

React Native app with:
- **UV Dashboard** — today's UV dose (J/m²), MED fraction bar (0-100%), UV index forecast, hours-to-burn, real-time exposure graph
- **Skin Scan** — trigger Skin Scanner, view condition results with annotated images, lesion tracker with change comparison
- **Skincare Routine** — morning/evening routine with product + amount, dispensing history, product inventory
- **Skin Timeline** — chronological skin condition + quality trends, "skin age" vs chronological age
- **Lesion Tracker** — registered moles/lesions with side-by-side change detection images + ABCDE scores
- **Alerts** — "UV at 80% MED — seek shade," "Lesion on left shoulder changed — see dermatologist," "Product #2 running low — reorder?"
- **Derm Report** — export a clinical PDF: skin type, UV history, scan results, lesion changes, product history, risk scores

See [software/mobile-app/](software/mobile-app/) for source.

---

## BOMs

- [Mirror Hub BOM](hardware/bom/hub_node_bom.csv) — ~$48 (Qty1), ~$26 (10k)
- [UV Patch BOM](hardware/bom/uv_patch_bom.csv) — ~$22 (Qty1), ~$9 (10k)
- [Skin Scanner BOM](hardware/bom/skin_scanner_bom.csv) — ~$42 (Qty1), ~$24 (10k)
- [Smart Dispenser BOM](hardware/bom/dispenser_node_bom.csv) — ~$32 (Qty1), ~$18 (10k)

A starter system (1 Mirror Hub + 1 UV Patch + 1 Skin Scanner + 1 Smart Dispenser) ≈ **$144 retail**, expandable to 2 patches + 4 dispenser slots.

---

## Power Architecture

- **Mirror Hub:** USB-C 5V + LiPo 2500mAh backup (~6 hr outage)
- **UV Patch:** CR2477 coin cell (1 Ah) → 14+ days at 1-min UV sampling + Sub-GHz TX (14 µA sleep, ~25 mA active for 200ms)
- **Skin Scanner:** 18650 2600mAh + USB-C → ~150 scans/charge
- **Smart Dispenser:** 5V USB-C + 18650 backup → pumps on 3V, MCU on 3.3V LDO

---

## Privacy

- Skin images are highly sensitive biometric data — all images encrypted in transit (TLS) and at rest (AES-256)
- Images stored locally on scanner SD card + optional cloud backup (user-controlled, off by default)
- UV dose data is aggregate (J/m²), not location-tracked
- No cameras in living spaces (Skin Scanner is handheld, user-initiated, no always-on imaging)
- Dermatologist reports are user-generated and user-shared — no automatic data sharing
- All data is yours; delete anytime; no third-party sharing
- HIPAA-aware architecture (for US clinical use cases)

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) collection — a new complex device system every 24 hours.*