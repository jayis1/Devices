# ThermoGrid comfort-tag Schematic

## KiCad Project

Open `comfort-tag.kicad_pro` in KiCad 8+ to view/edit the schematic.

## Schematic Notes

- 4-layer PCB (Signal, GND, Power, Signal), 25×25mm (wearable form factor)
- Minimum trace width: 0.1mm (signal), 0.3mm (power)
- Via size: 0.25mm drill, 0.5mm pad
- Ultra-compact: designed for wrist-band clip, lanyard, or pin form factor
- Ground plane on layer 2 (unbroken for RF performance / BLE antenna)

## Design Rules

- All decoupling capacitors within 1mm of IC power pins (space constrained)
- nRF52840 integrated BLE antenna — PCB trace or chip antenna (no external antenna)
- MAX30208 (skin temp) placed on PCB edge that contacts skin
- TMP117 (air temp) placed away from body, on opposite side of PCB if possible
- MAX30101 (PPG) needs optical isolation — green/IR LEDs face skin
- LSM6DSO (IMU) centered on PCB for best motion detection
- CR2032 holder on back of PCB (against wrist/skin side)
- Two I2C buses: TWI0 (MAX30208, TMP117, SHT40) and TWI1 (MAX30101, LSM6DSO)
  Separation prevents timing conflicts (MAX30101 needs specific I2C timing)

## Key Design Considerations

- nRF52840: lowest-power BLE MCU, runs 8-12 months on CR2032
- MAX30208: clinical-grade skin temperature (±0.1°C) — key comfort input
  Placed on the edge of PCB that touches the wrist/chest
- TMP117: air temperature near body (±0.1°C)
  Must be away from body heat — ideally on opposite side or with thermal isolation
- MAX30101: PPG heart rate + HRV
  Needs optical path to skin (green + IR LEDs)
  Power-hungry: only sampled every 2 minutes (not continuous)
- LSM6DSO: 6-axis IMU for activity classification
  Detects: sedentary, light, moderate, vigorous, sleeping
  Low power: 12.5Hz sampling
- SHT40: local humidity near skin (sweat evaporation comfort)
- Vote buttons: two tactile buttons (I'm cold / I'm hot), large, easy to press
- Status LED: single LED, blinks on vote + alive heartbeat
- Deep sleep: <15µA (SystemOFF mode), ~5mA for 50ms during measurement burst

## Pin Assignments (nRF52840)

| Pin | Function | Notes |
|-----|----------|-------|
| P0.11 | GPIO | MAX30101 interrupt (PPG data ready) |
| P0.12 | GPIO EXTI | LSM6DSO interrupt 1 (activity change) |
| P0.13 | GPIO EXTI | "I'm cold" vote button (active low, pullup) |
| P0.14 | GPIO EXTI | "I'm hot" vote button (active low, pullup) |
| P0.15 | GPIO | Status LED |
| P0.24-25 | TWI1 SDA/SCL | MAX30101, LSM6DSO |
| P0.26-27 | TWI0 SDA/SCL | MAX30208, TMP117, SHT40 |
| P0.30 | SAADC | Battery voltage sense (CR2032) |

## PCB Layout (Top View)

```
┌────────────────────┐
│  [Vote Cold] [Vote]│ ← tactile buttons
│               [Hot]│
│  ┌──────────────┐  │
│  │  MAX30101    │  │ ← PPG (faces skin)
│  │  (green/IR)  │  │
│  ├──────────────┤  │
│  │  MAX30208    │  │ ← skin temp (edge, contacts skin)
│  │  TMP117      │  │
│  │  SHT40       │  │
│  ├──────────────┤  │
│  │  nRF52840   │  │
│  │  LSM6DSO    │  │
│  ├──────────────┤  │
│  │  CR2032      │  │ ← battery (back side)
│  │  Holder      │  │
│  └──────────────┘  │
│    [LED]  [BLE ant]│ ← PCB trace antenna
└────────────────────┘
```

## Power Budget (CR2032, 220mAh)

| State | Current | Duration | Consumption |
|-------|---------|----------|-------------|
| Deep sleep | 15µA | 29.5s/30s | 0.44µA avg |
| Sensor burst | 5mA | 0.5s/30s | 83µA avg |
| MAX30101 HR | 8mA | 0.2s/120s | 13µA avg |
| BLE TX | 4.5mA | 0.1s/30s | 15µA avg |
| **Total avg** | | | **~111µA** |
| **Battery life** | 220mAh / 0.111mA | | **~2000 hours ≈ 83 days** |

Note: with aggressive duty cycling and sleep optimization, target 8-12 months.
Production firmware uses: 2min sampling (not 30s) during inactivity,
5s sampling only when active (detected by IMU).

## Schematic Files

- `comfort-tag.kicad_sch` — Main schematic
- `comfort-tag.kicad_pcb` — PCB layout (work in progress)
- `comfort-tag.kicad_pro` — KiCad project file

See BOM in `hardware/bom/comfort_tag_bom.csv` for component details.