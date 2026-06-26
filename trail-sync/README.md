# TrailSync

**AI-powered multi-node trail running & outdoor adventure safety system.** Continuously monitors your biomechanics, navigation, and environment through a wrist-worn unit, shoe-embedded pod, and trail-mounted beacons — detecting injury risk (IT band syndrome, stress fracture, Achilles tendinopathy) before it sidelines you, alerting you to hazardous terrain and weather changes, guiding you back on-trail if you stray, and coordinating emergency SOS with your exact 3D position and physiological state to rescuers — so every trail adventure ends with a cold beer, not a helicopter ride. Solves a problem that affects 50M+ trail runners and hikers: 40% sustain injuries annually, 14,000+ require mountain rescue per year, and most carry only a phone that dies in the cold and has no signal in the backcountry.

---

## What It Does

TrailSync is a 4-node wearable + trail infrastructure system that turns outdoor adventure from risky guesswork into a continuously monitored, data-driven safety practice:

1. **Monitors running biomechanics in real-time** — a Shoe Pod (nRF52833 + IMU + pressure insole, Sub-GHz, coin-cell powered, 30-day life) embedded in your shoe measures 24-point plantar pressure, 3D acceleration/gyro, and ground reaction force at 200 Hz. On-device gait analysis detects asymmetry, overpronation, cadence drift, and impact spikes — the earliest indicators of IT band syndrome, plantar fasciitis, Achilles tendinopathy, and stress fracture. The pod is so light (18g) you forget it's there.

2. **Tracks your physiology and environment** — a Wrist Unit (nRF52832 + PPG + barometric altimeter + GPS + Sub-GHz + LoRa, 18650, 72-hr life) measures heart rate, HRV, skin temperature, blood oxygen (SpO2), and barometric pressure. It knows your altitude, vertical gain/loss, pace, and distance. A sudden HRV drop (>20% from baseline) or SpO2 drop below 94% triggers an altitude sickness alert. Barometric pressure drop > 4 hPa in 3 hours predicts incoming storms.

3. **Guides you with trail intelligence** — Trail Beacons (nRF52833 + Sub-GHz + LoRa + solar, trail-mounted, 2-year life) placed at trail junctions, water sources, and hazard zones broadcast their GPS position, trail difficulty, water availability, and hazard conditions (ice, washout, wildlife). Your Wrist Unit cross-references your GPS track against the trail network and alerts you if you're off-trail — with compass bearing and distance back to the nearest trail. Beacons mesh-relay emergency SOS messages over LoRa to the nearest cell coverage point, giving you backcountry communication even with zero cell signal.

4. **Predicts injury before it happens** — the cloud ML pipeline ingests gait data, HRV trends, vertical gain, pack weight, training load, and sleep data to produce a 7-day injury risk forecast for 12 common trail injuries. "Your left foot impact asymmetry has increased 15% over 3 runs — 68% probability of developing IT band syndrome within 10 days if you don't reduce volume by 30%. Suggested: replace Thursday's 15-mile trail run with a 10-mile flat run." It's like having a sports medicine doctor who never sleeps.

5. **Detects falls and distress automatically** — the Wrist Unit's IMU detects a hard fall (acceleration spike > 8G + orientation change + 10-second stillness). The Shoe Pod confirms impact. If no movement is detected for 30 seconds after a fall, the system auto-triggers an SOS: broadcasts on LoRa to all beacons in range (relayed to cell coverage), sends GPS coordinates + physiological state + injury type prediction to the Hub (if in WiFi range) and cloud. Your emergency contacts get a push notification with your exact location, heart rate, and fall description.

6. **Adapts to your fitness level** — the ML pipeline learns your personal fitness signature (HRV baseline, recovery rate, ventilatory threshold, preferred cadence, gait symmetry) and adjusts pace recommendations, distance limits, and terrain difficulty ratings based on your current state, not generic charts. A V4 climber gets different altitude recommendations than a V8; a 20 mpw runner gets different volume limits than a 60 mpw runner.

7. **Monitors trail conditions in real-time** — Trail Beacons report current conditions: trailhead temperature, humidity, recent rainfall (for mud/flash flood risk), wildfire smoke (optional PM2.5 sensor), and wildlife alerts (trail camera + PIR motion). Your app shows real-time trail conditions at every junction, so you can bail before you're committed to a hazardous route.

8. **Coordinates group safety** — multiple TrailSync users on the same trail mesh together over Sub-GHz. The group leader's Hub sees all group members' positions, physiological states, and pace. If someone falls behind, gets injured, or triggers an SOS, the whole group is notified. "Sarah hasn't moved for 45 seconds at mile 8.2 — she may need help." No more leaving someone behind.

9. **Builds a training journal, automatically** — every run is logged: distance, elevation, pace splits, heart rate zones, ground contact time, vertical oscillation, stride length, injury risk score, terrain type (road/trail/mud/snow), weather conditions. No manual entry. The app generates weekly training reports with injury risk trends, recovery recommendations, and personal records.

10. **Emergency SOS with 3D position** — hold the Wrist Unit button for 5 seconds, or the system auto-triggers on fall + stillness. SOS includes: GPS coordinates, altitude, heart rate, SpO2, injury type (from gait data), direction and distance to nearest trail beacon, and a 30-second audio clip from the wrist microphone. LoRa mesh relays through beacons to reach cell coverage, then to cloud and emergency contacts. Even without cell signal, your SOS can reach help — because trail beacons are placed at cell-coverage points.

All wearables communicate over a Sub-GHz mesh (868/915 MHz, long range, low power). Trail Beacons additionally use LoRa (868/915 MHz, 5-15 km range, mesh relay) for backcountry communication. The Hub bridges to WiFi/cellular for cloud analytics and the mobile app. The Wrist Unit has GPS + barometric altimeter for self-contained navigation even without the Hub or phone.

### The Problem It Solves

- **Trail injuries are endemic:** 40% of trail runners sustain injuries each year — IT band syndrome, plantar fasciitis, Achilles tendinopathy, stress fractures, ankle sprains. Most come from overuse and poor biomechanics that develop over days, not in a single moment. TrailSync detects the early warning signs and warns you before you're injured.
- **People get lost and die on trails:** 3,000+ search-and-rescue operations per year in the US alone. 14,000+ mountain rescue calls annually in the Alps. Hikers stray off-trail, weather changes, darkness falls. GPS on phones drains battery in 4-6 hours; TrailSync's wrist unit lasts 72 hours.
- **Backcountry communication is nonexistent:** No cell signal on 80%+ of mountain trails. Satellite communicators cost $300-500 + $15/month subscription. TrailSync uses LoRa mesh through trail beacons — free, always available, and no subscription required (beacons are community-maintained).
- **Falls are deadly, especially solo:** A twisted ankle 10 miles from the trailhead can become a survival situation. TrailSync detects falls automatically, confirms distress, and broadcasts SOS over LoRa mesh to beacon relay points — with your exact position, physiological state, and injury assessment.
- **Altitude sickness kills silently:** Acute Mountain Sickness affects 25% of people above 8,000 ft. HACE and HAPE are life-threatening. TrailSync monitors HRV, SpO2, and ascent rate — warning you to descend before symptoms become dangerous.
- **Weather changes fast in the mountains:** A sunny morning can become a lightning-dangerous afternoon. TrailSync's barometric altimeter detects pressure drops that predict storms 2-3 hours ahead, and trail beacons report real-time conditions.
- **Trail conditions are unknown:** You start a 15-mile loop and discover at mile 10 that the bridge is washed out. Trail beacons report real-time conditions — so you can make informed decisions at junctions, not after you're committed.
- **Generic training advice fails:** "10% volume increase per week" doesn't account for terrain, vertical gain, pack weight, or your personal injury history. TrailSync personalizes everything to your biomechanics, fitness, and trail conditions.

TrailSync monitors your body, your trail, and your safety — so every adventure ends at the trailhead, not the rescue helicopter.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         TRAILSYNC SYSTEM                                          │
│                                                                                    │
│  ┌─────────────────────┐ Sub-GHz mesh ┌──────────────────────┐                    │
│  │ SHOE POD ×1-2        │◄───────────►│                        │                    │
│  │ (in-shoe, 18g)       │  868/915MHz │    WRIST UNIT          │──── LoRa ──►Trail  │
│  │ nRF52833 + IMU6      │             │    (nRF52832 +         │     mesh     Beacons│
│  │ + pressure insole    │             │     GPS + LoRa +       │              (5-15km│
│  │ 200Hz gait analysis  │             │     barometer + PPG)   │               range)│
│  │ 30-day coin cell     │             │                        │                    │
│  └─────────────────────┘             │  Edge: gait asymmetry   │─── BLE ───► Mobile │
│                                       │   altitude sickness     │             (React │
│  ┌─────────────────────┐ Sub-GHz    │   fall detection         │              Native│
│  │ TRAIL BEACON ×N      │◄──────────►│   storm prediction       │              App)  │
│  │ (trail junctions)     │            └───────────┬────────────┘                    │
│  │ nRF52833 + Sub-GHz  │                         │ WiFi/Cell                      │
│  │ + LoRa + solar      │             ┌──────────▼─────────────┐                    │
│  │ PIR + temp/humidity │             │  HUB (home/vehicle)    │                    │
│  │ GPS + conditions DB │             │  ESP32-S3 + Sub-GHz    │──── WiFi ──►Cloud  │
│  │ LoRa mesh relay     │             │  + LoRa + WiFi6        │             Dashboard│
│  │ 2-year solar life   │             │  Route planner          │             + ML    │
│  └─────────────────────┘             │  Group tracker          │             Pipeline│
│                                       │  SOS relay              │             + Trail │
│                                       └────────────────────────┘              DB      │
│                                                                                    │
│  ┌──────────────────────────────────────────────────────────────────────────────┐ │
│  │                    CLOUD / EDGE SOFTWARE                                       │ │
│  │  ┌──────────┐  ┌───────────────┐  ┌───────────────────────┐                 │ │
│  │  │Dashboard │  │ ML Pipeline   │  │ Mobile App            │                 │ │
│  │  │ (FastAPI)│  │ Gait LSTM    │  │ Live trail map          │                 │ │
│  │  │ + Trail  │  │ Injury risk  │  │ Biomechanics dashboard  │                 │ │
│  │  │   DB     │  │ Altitude sick│  │ SOS button + alerts     │                 │ │
│  │  │ + SOS   │  │ Storm detect  │  │ Training journal        │                 │ │
│  │  │   relay  │  │ Terrain CNN │  │ Injury forecast         │                 │ │
│  │  └──────────┘  └───────────────┘  └───────────────────────┘                 │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Wrist Unit (1 per runner)

The command center. Worn on the wrist (watch form factor). Measures heart rate, HRV, SpO2, skin temp, altitude, and GPS position. Runs on-device fall detection, altitude sickness screening, and storm prediction. Displays navigation, pace, and alerts on a sunlight-readable OLED.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52832 | BLE + Sub-GHz + sensor sampling + on-device ML |
| Radio | SX1262 (Sub-GHz) + RFM95W (LoRa) | Sub-GHz mesh to shoe pods + beacons; LoRa long-range for backcountry relay |
| GPS | u-blox SAM-M10Q | Multi-constellation GNSS, <1m accuracy, 25mA acquisition |
| Altimeter | BMP390 | Barometric pressure for altitude (±0.5m) and storm prediction (Δ4 hPa in 3hr = storm) |
| PPG + SpO2 | MAX30101 | Heart rate, HRV, blood oxygen (SpO2) via photoplethysmography |
| Skin Temp | TMP117 | ±0.1°C skin temperature (core body temp proxy, fever/altitude sickness) |
| IMU | LSM6DSL | 3D accel + gyro for fall detection, arm swing analysis |
| Display | 1.3" OLED (SH1106, 128×64) | Pace, altitude, navigation arrow, alerts — sunlight-readable |
| Haptic | DRV2605L + LRA | Navigation prompts (turn left/right), SOS vibrate, pace alerts |
| Mic | SPH0645 (I2S) | 30-sec audio clip for SOS (rescuers hear your voice) |
| Flash | W25Q128 16MB | Trail map tiles, 72-hr activity ring buffer, gait cache |
| RTC | PCF8563 | Timekeeping for activity logs when GPS is unavailable |
| Battery | 18650 LiFePO4 2600mAh | 72-hr continuous GPS + Sub-GHz + 5-min LoRa TX; rechargeable USB-C |
| LEDs | 3× SMD (RGB) | GPS lock (green), SOS (red), low battery (amber) |
| Buttons | 3× tactile | Power/select, SOS (5-sec hold), mode (map/pace/nav) |
| Antenna | PCB trace (868/915 MHz) + ceramic GPS | Compact, wrist-mountable |
| Watchband | Silicone sport band, 22mm | Quick-release for charging; pod sits on dorsal wrist |
| Enclosure | IP67 polycarbonate, 52×42×15mm | Trail-proof: rain, mud, creek crossings |

**Wrist Unit firmware responsibilities:**
- Acquire GPS fix (cold start ~30s, hot start ~1s); compute pace, distance, elevation
- Sample PPG at 100 Hz for HR/HRV; compute HRV (RMSSD, SDNN) every 60s
- Sample SpO2 via MAX30101 (red+IR) every 30s during altitude mode
- Barometric altitude at 1 Hz; detect pressure drop > 4 hPa/3hr → storm alert
- Fall detection: LSM6DSL acceleration spike > 8G + orientation change + 10s stillness → SOS trigger
- Altitude sickness screening: if SpO2 < 94% + HRV drop > 20% + ascent > 500m/hr → "Descend now" alert
- Sub-GHz mesh: receive gait data from Shoe Pod every 5s; receive beacon data on approach
- LoRa TX: send position + vitals every 5 min; relay SOS on demand; receive beacon broadcasts
- OLED display: 3 screens — pace/distance/HR, map with navigation arrow, altitude/vertical/alerts
- Button interface: short press cycles screens; 5-sec hold triggers manual SOS
- BLE to mobile app for full sync when phone is available
- 72-hr activity ring buffer in flash (GPS track, HR, HRV, SpO2, altitude, gait asymmetry)

### 2. Shoe Pod (1-2 per runner — left + right shoe)

The biomechanics lab. Embedded in the midsole of each trail shoe (or clipped to the laces). Measures 24-point plantar pressure, 3D acceleration/gyro, and ground reaction force at 200 Hz. On-device gait analysis detects asymmetry, overpronation, cadence drift, and impact spikes — the earliest indicators of injury.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52833 | Sub-GHz TX + 200 Hz sensor sampling + on-device gait CNN |
| Radio | SX1262 (Sub-GHz) | 868/915 MHz, 50m range to Wrist Unit, ultra-low-power |
| IMU | LSM6DSL | 3D accel ±16G + 3D gyro ±2000dps, ground impact and pronation |
| Pressure Insole | 24× FSR (force-sensitive resistor) array | 24-point plantar pressure mapping (heel, midfoot, forefoot, toes) |
| Strain Gauge | 2× strain gauge + HX711 | Ground reaction force (vertical + anterior-posterior) |
| Battery | CR2477 coin cell (1 Ah) | 30+ days at 200 Hz sampling + 5s Sub-GHz TX |
| Antenna | PCB trace (868/915 MHz) | Compact, embedded in shoe |
| Enclosure | IP67 polyurethane pod, 45×28×8mm | Fits in midsole pocket or laces; trail-proof |

**Shoe Pod firmware responsibilities:**
- Sample IMU at 200 Hz (3D accel + 3D gyro) continuously during run detection
- Sample 24× FSR pressure array at 200 Hz; compute center-of-pressure trajectory
- Sample HX711 strain gauge at 200 Hz; compute vertical ground reaction force
- On-device gait CNN (TFLite Micro, <150KB): classify gait asymmetry, overpronation, cadence drift from 2-second windows
- Compute gait metrics every stride: contact time, flight time, vertical oscillation, ground contact balance L/R
- Detect impact spikes: vertical GRF > 3.5× body weight → "reduce downhill stride" alert
- Sub-GHz TX: send gait summary to Wrist Unit every 5s (asymmetry index, cadence, impact load, pronation angle)
- Deep sleep between runs (~3 µA); wake on IMU motion detection threshold
- Left/right pairing: both pods sync via Wrist Unit; gait symmetry computed from L/R comparison

### 3. Trail Beacon (N per trail network — placed at junctions, water sources, hazard zones)

The trail intelligence. Solar-powered, weatherproof modules placed at key trail points. Broadcast GPS position, trail difficulty, water availability, hazard conditions, and relay LoRa messages for backcountry SOS.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52833 | Sub-GHz + LoRa control + beacon logic |
| Radio | SX1262 (Sub-GHz) + RFM95W (LoRa) | Sub-GHz mesh to wrist units (50m); LoRa mesh to other beacons (5-15km) |
| GPS | u-blox SAM-M10Q | Beacon position (set once at install, then used for trail junction coordinates) |
| PIR Sensor | AM312 | Motion detection (wildlife or hiker proximity — wake from sleep) |
| Temp/Humidity | BME280 | Trail conditions: temperature, humidity, recent rain estimation |
| PM2.5 (optional) | PMS5003 | Wildfire smoke detection — AQI reporting |
| Solar | 5W monocrystalline panel + MCP73871 | 2-year life with solar charging + LiFePO4 18650 |
| Battery | 18650 LiFePO4 1500mAh | 30+ days without sun (beacon-only mode); solar tops up daily |
| LEDs | 3× SMD (RGB) | Trail junction indicator (color = difficulty), SOS relay indicator |
| Flash | W25Q128 16MB | Trail database, beacon firmware, relay queue |
| Antenna | 868/915 MHz whip antenna + LoRa | 5-15 km LoRa range between beacons |
| Enclosure | IP67 polycarbonate, 120×80×40mm, camo green | Trail-mounted (tree strap or pole mount), weatherproof, UV-resistant |

**Trail Beacon firmware responsibilities:**
- Broadcast beacon ID + GPS position + trail data every 60s on Sub-GHz (for approaching wrist units)
- Broadcast beacon ID + GPS position + conditions every 5 min on LoRa (for other beacons to relay)
- Receive LoRa messages from wrist units (position, SOS) and relay to next beacon toward cell coverage
- Store trail database: junction coordinates, trail difficulty (1-5), water source status, hazard flags, distance to trailhead
- Report conditions: temperature, humidity, PIR motion (wildlife proximity), optional PM2.5
- Solar power management: sleep when battery < 15%; wake when solar charging; full beacon when battery > 30%
- Trail condition updates from cloud (via WiFi hub or LoRa relay from hub): trail closures, hazard alerts, weather warnings

### 4. Hub (1 per household/vehicle — home base or trailhead vehicle)

The coordinator. Stays at home or in the vehicle at the trailhead. Bridges the Sub-GHz mesh and LoRa to WiFi/cellular for cloud sync. Runs route planning, group tracking, and SOS relay coordination. Displays group status and trail conditions on a TFT dashboard.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 (N16R8) | WiFi6 + BLE + edge compute + route planner |
| Radio | SX1262 (Sub-GHz) + RFM95W (LoRa) | Sub-GHz mesh + LoRa relay to trail beacons |
| Display | 4.0" IPS TFT (ILI9488, 480×320) | Group dashboard: positions, vital signs, pace, trail conditions |
| Storage | W25Q256 32MB Flash + MicroSD | Trail maps, route plans, 30-day activity ring buffer, OTA |
| RTC | PCF8563 + CR1220 | Timekeeping for activity logs |
| Audio | MAX98357A + 28mm speaker | SOS relay alert, incoming distress call, storm warning siren |
| Power | 5V USB-C + 18650 LiPo 3500mAh | Mains or vehicle-powered; battery backup for 8-hr outage |
| LEDs | WS2812 RGB + 4× SMD | Group status (all green = everyone OK), SOS (pulsing red), weather (amber) |
| GPS | u-blox SAM-M10Q | Hub's own position (vehicle at trailhead — known reference) |

**Hub firmware responsibilities:**
- Maintain Sub-GHz mesh network with all wrist units and beacons in range
- Receive LoRa messages from distant beacons; relay to cloud via WiFi
- Coordinate SOS: receive distress signal, relay to emergency services via cloud API, track rescue status
- Route planner: compute optimal trail route from trail database (distance, elevation, difficulty, water sources)
- Group tracker: display all group members' positions, vital signs, pace on TFT dashboard
- Storm prediction: receive barometric data from all wrist units, compute pressure trends, predict storms 2-3 hours ahead
- Cloud sync: upload activity data, download trail condition updates, OTA firmware for all nodes
- MQTT over WiFi to cloud dashboard; BLE to mobile app for setup and configuration

---

## Communication Protocol

**Shoe Pods ↔ Wrist Unit:** Sub-GHz (868/915 MHz) via SX1262. Short-range (50m), ultra-low-power. Binary protocol with CRC16. Gait summary every 5s during run.

**Wrist Unit ↔ Trail Beacons:** Sub-GHz (868/915 MHz) on approach + LoRa (868/915 MHz, 5-15km) for backcountry. Wrist unit receives beacon broadcasts; sends position + SOS via LoRa.

**Trail Beacons ↔ Each Other:** LoRa mesh relay (868/915 MHz, 5-15 km). Beacons relay messages toward the nearest hub with cell/WiFi connectivity.

**Hub ↔ Cloud:** WiFi6 (ESP32-S3), MQTT over TLS. Activity data, SOS relay, trail condition updates, OTA firmware.

**Wrist Unit ↔ Mobile App:** BLE 5.3 (instant alerts) + WiFi (full sync when available).

See [docs/protocol.md](docs/protocol.md) for the full frame specification.

---

## ML Pipeline

1. **Gait Analysis LSTM** — Processes 200 Hz IMU + pressure insole data to classify gait patterns. Detects asymmetry (L/R ground contact time difference), overpronation (medial pressure shift), cadence drift (declining cadence over run), and vertical load rate (impact severity). On-device TFLite Micro binary classifies "normal/asymmetric/overpronating/high-impact" every 2 seconds. Cloud LSTM tracks gait trends over 7+ days and predicts injury risk.

2. **Injury Risk Predictor** — 12-class injury risk model (IT band syndrome, plantar fasciitis, Achilles tendinopathy, stress fracture, shin splints, runner's knee, ankle sprain, hamstring strain, hip flexor strain, calf strain, iliotibial band friction, patellar tendinopathy). Fuses gait metrics, HRV trends, training load (acute:chronic workload ratio), vertical gain, terrain type, and 7-day training history to produce a per-injury risk score. 7-day forecast: "IT band risk 68% — reduce volume 30% and add lateral hip strengthening."

3. **Altitude Sickness Detector** — Monitors SpO2, HRV, and ascent rate. Detects Acute Mountain Sickness (AMS) using Lake Louise Scoring (headache + nausea + fatigue) correlated with physiological data. Warns to descend when SpO2 < 94% + HRV drop > 20% + ascent > 500m/hr. Detects HACE/HAPE risk (SpO2 < 88% = descend immediately).

4. **Storm Predictor** — Barometric pressure trend model. Detects rapid pressure drops (> 4 hPa/3hr) that predict thunderstorms, cold fronts, and high winds. Combines with temperature and humidity trends for higher confidence. 2-3 hour advance warning: "Storm arriving in 2 hours — seek shelter or turn back."

5. **Terrain Classifier** — On-device CNN classifies terrain type from shoe pod IMU patterns: road, gravel, dirt trail, mud, snow, ice, rock, sand. Adjusts pace recommendations and injury risk ("high impact on rocky terrain — shorten stride, reduce pace 15%").

See [software/ml-pipeline/](software/ml-pipeline/) for training scripts.

---

## Mobile App

React Native app with:
- **Live Trail Map** — GPS position on offline trail map, breadcrumbs, distance to trailhead, nearest water source, beacon positions
- **Biomechanics Dashboard** — real-time gait symmetry, cadence, vertical oscillation, ground contact time L/R, impact load, pronation angle
- **Navigation** — turn-by-turn trail directions, distance to next junction, bearing to nearest trail if off-trail
- **Injury Forecast** — 7-day injury risk per body part, training load chart, recovery recommendations
- **Altitude Monitor** — current elevation, vertical gain, SpO2, HRV, AMS risk score, "safe to continue" / "descend now"
- **Weather Alerts** — barometric trend, storm prediction, temperature, trail conditions from beacons
- **Group Tracker** — positions and vital signs of all group members on the same trail
- **SOS** — one-tap emergency button (or auto-triggered); sends GPS + vitals + audio clip over LoRa mesh
- **Training Journal** — auto-logged: distance, elevation, pace splits, HR zones, gait metrics, terrain types, weather, injury risk trend

See [software/mobile-app/](software/mobile-app/) for source.

---

## BOMs

- [Wrist Unit BOM](hardware/bom/wrist_unit_bom.csv) — ~$62 (Qty1), ~$34 (10k)
- [Shoe Pod BOM](hardware/bom/shoe_pod_bom.csv) — ~$28 (Qty1), ~$12 (10k)
- [Trail Beacon BOM](hardware/bom/trail_beacon_bom.csv) — ~$38 (Qty1), ~$19 (10k)
- [Hub BOM](hardware/bom/hub_node_bom.csv) — ~$52 (Qty1), ~$28 (10k)

A starter system (1 Wrist Unit + 2 Shoe Pods + 1 Hub) ≈ **$170 retail**. Trail beacons are community-maintained and placed at trail junctions — a trail network of 20 beacons costs ~$380.

---

## Power Architecture

- **Wrist Unit:** 18650 LiFePO4 2600mAh → 72-hr continuous GPS + Sub-GHz + 5-min LoRa TX. USB-C recharge.
- **Shoe Pod:** CR2477 coin cell (1 Ah) → 30+ days at 200 Hz sampling + 5s Sub-GHz TX (sleep between runs ~3 µA).
- **Trail Beacon:** 5W solar + 18650 LiFePO4 1500mAh → 2-year life with solar charging. 30+ days without sun (beacon-only mode).
- **Hub:** 5V USB-C + 18650 LiPo 3500mAh backup → 8-hr outage operation.

---

## Privacy

- GPS tracks are encrypted in transit (TLS) and at rest (AES-256)
- Biomechanics data (gait, pressure, HRV) is health data — stored locally, cloud-sync only with opt-in
- Trail beacon conditions are anonymous (no personal data, just trail conditions)
- SOS broadcasts contain only position + vitals — no identity until relayed to your emergency contacts
- Group tracking is opt-in — you choose who sees your position
- All data is yours; delete anytime; no third-party sharing
- GDPR / CCPA compliant architecture

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) collection — a new complex device system every 24 hours.*