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