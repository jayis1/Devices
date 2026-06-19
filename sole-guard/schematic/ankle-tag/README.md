# Ankle Wearable Tag Schematic — SoleGuard

## MCU

**nRF52840** — BLE 5.3 mesh relay + sensor sampling + fall detection.

## Block Diagram

```
              ┌──────────────────────────────────────┐
              │         ANKLE WEARABLE TAG             │
              │                                      │
  Qi RX ────  │  5W ── MCP73831 ── LiPo 180mAh       │
              │  ── 3.0V LDO ── nRF52840              │
              │                                      │
  nRF52840 ── │  I2C0 ── LSM6DSO32 (IMU)             │
              │  I2C0 ── TMP117 (skin temp, ±0.1°C)  │
              │  SPI0 ── AD5940 (bioimpedance BIA)   │
              │  GPIO ── 4× electrodes (2 force,     │
              │           2 sense) around ankle      │
              │  PCB trace antenna ── BLE mesh relay │
              └──────────────────────────────────────┘
```

## Pin Assignments — nRF52840

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | I2C0 SDA | LSM6DSO32 + TMP117 (shared bus) |
| P0.03 | I2C0 SCL | LSM6DSO32 + TMP117 |
| P0.04 | SPI0 SCK | AD5940 |
| P0.05 | SPI0 MOSI| AD5940 |
| P0.06 | SPI0 MISO| AD5940 |
| P0.07 | SPI0 CS  | AD5940 CS |
| P0.08 | GPIO out | AD5940 CE (counter electrode enable) |
| P0.09 | GPIO out | AD5940 RE (reference electrode) |
| P0.10 | GPIO in  | AD5940 DRDY (data-ready interrupt) |
| P0.11 | GPIO in  | IMU INT1 (accel data-ready) |
| P0.12 | GPIO in  | IMU INT2 (free-fall interrupt) |
| P0.13 | GPIO in  | Qi RX charging status |
| P0.14 | GPIO out | status LED |
| P0.15 | ADC0     | battery voltage (divider) |
| P0.16-19 | GPIO | 4× electrode switch (HSPA256 analog switch) |
| P0.21 | GPIO out | AD5940 AFE sleep |
| P0.23 | SWDIO | programming |
| P0.24 | SWCLK | programming |

## Bioimpedance Electrode Placement (4-electrode)

```
        Front of ankle
     ┌─────────────────┐
     │   F(+)    F(-)   │   ← Force electrodes (current injection)
     │     │      │     │
     │   S(+)    S(-)   │   ← Sense electrodes (voltage measurement)
     └─────────────────┘
          Back of ankle
```
- 2× force (current) electrodes: medial + lateral ankle, 30mm apart
- 2× sense (voltage) electrodes: between force electrodes, 20mm spacing
- Ag/AgCl gel electrodes, disposable, replaced weekly

## Power

- LiPo 180mAh, 3.7V
- MCP73831 from Qi 5W receiver
- TLV743-3.0 LDO → 3.0V
- Average current: ~0.5mA (10Hz IMU + 4h bioimpedance + relay) → 5–7 days
- Qi charging: 60 min full

## Enclosure

- 32mm diameter disc, 6mm thick
- IP67, silicone band with clip
- Electrodes on skin-facing side (snap connectors for disposable gel electrodes)

## KiCad Project

`schematic/ankle-tag/ankle-tag.kicad_sch` — schematic
`schematic/ankle-tag/ankle-tag.kicad_pcb` — 4-layer PCB