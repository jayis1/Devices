# Devices

Complex hardware + software device systems that improve daily life for earthlings. Each invention is a full system — multiple hardware nodes, firmware, cloud/edge software, mobile apps, and ML pipelines. A new system drops every 24 hours.

## Philosophy

These aren't single chips on a board. Each device system here is:

- **Multi-node** — 2+ hardware units working together (sensors, actuators, hubs, gateways)
- **Full-stack** — firmware, edge compute, cloud, mobile app, ML
- **Life-improving** — solves a real problem that affects people daily
- **Buildable** — real components, real protocols, manufacturable BOMs
- **Open everything** — KiCad schematics, C/Python firmware, React Native apps, ML training code

## Device Systems

| # | System | Nodes | Domain | Description |
|---|--------|-------|--------|-------------|
| 1 | Aqua Guard | 4 (Hub, Feeder, Sensor×N, Cloud) | Home Pet Care | AI-powered aquarium ecosystem monitor and autoregulator |
|| 2 | HearthKeep | 4 (Hub, Room Monitor×N, Bed Mat, Wearable Tag) | Elder Safety | Ambient elder safety monitor — mmWave radar fall detection, under-mattress vitals, privacy-first |
|| 3 | BreathHome | 4 (Hub, Room Sensor×N, HVAC Controller, Wearable Tag) | Indoor Air Quality | Smart indoor air quality & respiratory health system — multi-sensor monitoring, HVAC actuation, mold prediction, personal exposure tracking |
|| 4 | UrbanHarvest | 4 (Hub, Grow Pod, Plant Sensor×N, Weather Station) | Urban Farming | Intelligent micro-farming system — automated irrigation, disease detection, harvest prediction, climate control for balconies and rooftops |
|| 5 | SleepSync | 4 (Hub, Sleep Strip, Climate Node, Shade Controller×N) | Sleep Health | AI-powered sleep environment optimizer — BCG sleep staging, smart alarm, adaptive soundscapes, climate + light control, apnea detection |
|| 6 | ErgoFlow | 4 (Hub, Chair Pad, Desk Controller, Wearable Tag) | Workspace Wellness | AI-powered adaptive workspace wellness system — mmWave pose detection, pressure mapping, motorized sit-stand desk, circadian lighting, RSI risk prediction |
|| 7 | FlowGuard | 4 (Hub, Valve Controller, Pipe Sensor×N, Appliance Monitor×N) | Home Water Protection | AI-powered water leak detection, pipe health monitoring, and flood prevention — acoustic leak detection, motorized shutoff valve, freeze prediction, flow disaggregation |
|| 8 | MedSync | 4 (Hub, Pill Station, Room Beacon×N, Wearable Tag) | Medication Adherence | AI-powered medication adherence and health monitoring — motorized pill dispenser, weight verification, pulse oximetry, fall detection, proximity reminders, caregiver alerts |
|| 9 | FreshKeep | 4 (Hub, Fridge Node, Pantry Node, Stove Guard) | Kitchen Intelligence | AI-powered kitchen intelligence — food waste elimination, spoilage prediction, kitchen fire prevention, auto grocery lists, barcode inventory, recipe suggestions |
|| 10 | PowerPulse | 4 (Hub, Circuit Monitor, Appliance Tag×N, Solar Node) | Home Energy & Safety | AI-powered home energy intelligence — per-circuit monitoring, arc fault detection, appliance tagging, solar MPPT optimization, time-of-use bill reduction |
|| 11 | CradleKeep | 4 (Hub, Crib Pad, Nursery Monitor, Feeding Station) | Infant Care | AI-powered infant monitoring and care system — ballistocardiography breathing detection, cry classification, bottle warming+tracking, nursery environment optimization ||
|| 12 | SoundNest | 4 (Hub, Room Sensor×N, Masking Speaker×N, Wearable Tag×N) | Home Acoustics | AI-powered home acoustic intelligence — 4-mic array sound classification, adaptive noise masking, personal sound dose tracking, tinnitus relief, privacy masking |
| 13 | WashWise | 4 (Hub, Washer Node, Dryer Node, Stain Scanner) | Laundry Care & Fire Safety | AI-powered laundry care, fire-safety & sustainability system — multispectral stain/fabric ID, auto-detergent dosing, lint fire prediction, dryness detection, energy & water optimization |
| 14 | PorchGuard | 4 (Hub, Porch Camera, Mailbox, Lock) | Home Delivery Security | AI-powered porch security & delivery intelligence — package detection, person re-ID, pirate behavior detection, mail tracking, motorized deadbolt + one-time courier codes, garage parcel drop |
| 15 | ThermoGrid | 4 (Hub, Room Sensor×N, Zone Actuator×M, Comfort Tag) | Home Thermal Comfort & Energy | AI-powered home thermal comfort & energy optimization — radiant temperature sensing (MLX90640), per-zone HVAC control, personal comfort learning (wearable skin temp + HR), thermal mass forecasting, solar self-consumption, 20-40% energy savings |
| 16 | SoleGuard | 4 (Hub, Smart Insole×2, Ankle Tag, Foot Scanner) | Diabetic Foot Health | AI-powered diabetic foot ulcer prevention — 24-point plantar pressure mapping, bilateral skin-temp asymmetry (>2.2°C clinical threshold), gait decline + fall detection, ankle edema (bioimpedance), multispectral wound scanning, 21-day ulcer-risk forecast, offloading alerts |
| 17 | PawSync | 4 (Hub, Collar Tag, Behavior Camera, Smart Feeder) | Pet Health & Behavior | AI-powered pet health, behavior & anxiety management — PPG heart rate/HRV, on-device activity CNN, gait lameness detection, behavior camera with 6-mic vocalization classification, separation anxiety detection + adaptive enrichment, RFID-verified smart feeding, 7-day illness-risk forecast, vet-ready reports |
| 18 | CalmGrid | 4 (Hub, Wrist Band, Room Sentinel, Light Node) | Mental Health & Stress | AI-powered personal stress & mental wellness system — PPG HRV + EDA skin conductance stress detection, on-device activity CNN, privacy-first voice prosody stress (no transcription), tunable-white circadian + de-stress lighting, guided breathing + soundscapes, 14-day burnout-risk forecast (MBI-validated), therapist-ready reports |
| 19 | GreenPulse | 4 (Hub, Plant Tag×N, Leaf Scanner, Water Valve) | Houseplant Care | AI-powered houseplant health monitoring & care system — per-plant capacitive soil moisture + light + temp/humidity tags (18mo coin-cell, Sub-GHz mesh), multispectral (white/UV/NIR) leaf disease & pest scanner with on-device species-ID CNN, latching-solenoid auto-watering with flow/leak detection, per-plant drying-curve LSTM, 40-class disease classifier, 4,000-species identification |
|| 20 | SkinSync | 4 (Mirror Hub, UV Patch×1-2, Skin Scanner, Smart Dispenser) | Skin Health & Sun Safety | AI-powered personal skin health & sun-safety system — wearable UVA/UVB dose tracking with real-time MED burn prevention haptics, multispectral (white/UV/NIR/polarized) skin condition scanner (25+ conditions, ABCDE melanoma detection), Fitzpatrick-personalized sunburn prediction, per-product skincare dispensing with load-cell verification, lesion change tracking, 90-day skin cancer risk forecast, dermatologist-ready clinical reports |
|| 21 | HiveSync | 4 (Gateway, Sensor Node×N, Entrance Monitor, Smart Feeder) | Beekeeping & Pollinator Health | AI-powered beehive health monitoring & management — multi-sensor colony monitoring (temp/humidity/weight/acoustics), swarm prediction LSTM (3-7 day forecast), Varroa mite detection on foragers (entrance camera + IR), queen health assessment (acoustic piping + brood temp), automated feeding (syrup + pollen patty), forager traffic counting (YOLOv8-nano), theft detection (accel + GPS), Sub-GHz 868 MHz mesh, 6-model ML pipeline |
|| 22 | BrewSync | 4 (Hub, Fermenter Node×N, Cellar Monitor, Brew Scanner) | Home Fermentation & Craft Brewing | AI-powered fermentation monitoring & craft brewing intelligence — tilt SG + CO2 evolution dual-confirmation tracking, 72-hour stuck fermentation prediction, spectral infection detection (15-class), automated temperature PID control, pH monitoring, multispectral refractometer scanner, BeerXML recipe import, Sub-GHz 868 MHz mesh + BLE 5.0, 6-model ML pipeline |
|| 23 | TrailSync | 4 (Wrist Unit, Shoe Pod×2, Trail Beacon×N, Hub) | Trail Running & Outdoor Safety | AI-powered trail running safety system — 200 Hz gait analysis with shoe-embedded pressure insole, 12-class injury risk prediction (IT band, plantar fasciitis, stress fracture, etc.), altitude sickness detection (SpO2 + HRV), barometric storm prediction, LoRa mesh SOS relay through trail beacons, automatic fall detection, group tracking, Sub-GHz 868 MHz mesh + LoRa 5-15 km + BLE 5.3, 5-model ML pipeline |
|| 24 | PoolSync | 4 (Hub, Chemistry Probe×1-3, Pool Camera, Equipment Controller) | Pool & Spa Health | AI-powered pool & spa health intelligence — ISFET pH + amperometric free chlorine + ORP monitoring, 3-day algae outbreak forecast (LSTM), automatic chemical dosing with flow verification, water clarity camera with green-channel algae detection, 8× relay equipment control, GFCI + entrapment safety interlocks, freeze protection, energy-optimized pump scheduling (DQN), Sub-GHz 868 MHz TDMA mesh, 6-model ML pipeline |

## Structure

Each device system lives in its own subfolder:

```
<device-name>/
├── README.md              # System overview, architecture, all nodes
├── schematic/              # KiCad projects (one per hardware node)
├── firmware/               # C source per node + shared common/
├── hardware/               # BOMs, gerbers, enclosure designs
├── software/               # Cloud dashboard, ML pipeline, mobile app
├── scripts/                # Setup, deployment, training scripts
└── docs/                   # Assembly, API, protocols, architecture
```

## License

MIT — build it, sell it, improve it.

---

*Invented and maintained by [jayis1](https://github.com/jayis1). New system every 24h.*