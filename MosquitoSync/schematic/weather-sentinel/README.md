# Weather Sentinel — Schematic

## Block Diagram

```
┌───────────────────────────────────────────────┐
│           nRF52840 QFAA                        │
│           (Cortex-M4F 64MHz)                   │
│                                               │
│  P0.02 ── BME280 SCL (I²C)                  │
│  P0.03 ── BME280 SDA (I²C)                  │
│  P0.04 ── Wind speed (counter interrupt)    │
│  P0.05 ── Wind direction (ADC AIN5)         │
│  P0.06 ── Rain gauge (counter interrupt)    │
│  P0.11 ── SX1262 NSS                        │
│  P0.12 ── SX1262 SCK                        │
│  P0.13 ── SX1262 MISO                       │
│  P0.14 ── SX1262 MOSI                       │
│  P0.15 ── SX1262 DIO1                       │
│  P0.16 ── SX1262 RST                        │
│  P0.17 ── SX1262 BUSY                       │
│  P0.18 ── Battery voltage (ADC AIN18)      │
│  P0.19 ── Solar voltage (ADC AIN19)        │
│  P0.20 ── Status LED                        │
└───────────────────────────────────────────────┘
```

## Subcircuits

### BME280 (Temp/Humidity/Pressure)
- I²C address 0x76
- SCL=P0.02, SDA=P0.03
- 10k pull-ups
- Outdoor-rated, in Stevenson screen

### Davis 6410 Anemometer + Vane
- Wind speed: reed switch, 1 pulse per revolution
  - 2.25 km/h per Hz → 0.625 m/s per pulse/s
  - GPIO P0.04, GPIOTE interrupt (falling edge)
- Wind direction: potentiometer, 0–360°
  - 0–3.3V → ADC AIN5 (P0.05)
  - 10k pull-up, 1µF smoothing cap

### Tipping Bucket Rain Gauge
- 0.2 mm per tip
- GPIO P0.06, GPIOTE interrupt (falling edge)
- Debounce: 150 ms
- Used for breeding site prediction (7–14 day lag)

### SX1262 Sub-GHz Radio
- SPI0 bus: SCK=P0.12, MOSI=P0.14, MISO=P0.13, NSS=P0.11
- DIO1=P0.15, RST=P0.16, BUSY=P0.17
- 868 MHz whip antenna via SMA

### Power
- Solar: 5W 6V monocrystalline panel
- Battery: LiFePO4 3.2V 3000 mAh
- MCP73871: solar charger
- Battery ADC: voltage divider 2:1 → P0.18
- Solar ADC: voltage divider 3:1 → P0.19
- Ultra-low-power: ~20µA in deep sleep between samples

## PCB Layout Notes
- 4-layer board (50×50 mm)
- nRF52 DC/DC converter: proper inductor + caps
- Anemometer/vane: terminal blocks for external wiring
- Stevenson screen: UV-resistant ASA enclosure
- Solar panel: external, weatherproof cable to PCB