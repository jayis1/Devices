# Soil Sensor Node Schematic

## Overview

Solar-powered, multi-parameter soil sensor node. Measures moisture, temperature, pH, NPK, and ambient light. Reports via Sub-GHz mesh every 15 minutes. Designed for 90+ days autonomy without sun.

## MCU: nRF52840 QFAA

- ARM Cortex-M4F @ 64 MHz
- 1 MB flash, 256 KB RAM
- BLE 5.0 (not used for mesh — Sub-GHz preferred for range)
- Ultra-low power: 1.7 µA in System OFF
- 48 GPIOs

## Sub-GHz Radio: SX1262IMLTRT

- 868 MHz, +22 dBm
- Ultra-low power: 0.2 µA in sleep
- SPI interface

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                    nRF52840 QFAA                            │
│                                                             │
│  P0.02 ─── FDC2214 SCL (I²C)                               │
│  P0.03 ─── FDC2214 SDA (I²C)                               │
│  P0.04 ─── DS18B20 data (1-Wire, 4.7kΩ pullup)             │
│  P0.05 ─── VEML7700 SCL (I²C shared)                       │
│  P0.06 ─── VEML7700 SDA (I²C shared)                       │
│  P0.07 ─── pH analog (LMP7721 output)                      │
│  P0.08 ─── Nitrogen ISE analog                             │
│  P0.09 ─── Phosphorus ISE analog                           │
│  P0.10 ─── Potassium ISE analog                            │
│  P0.11 ─── SX1262 NSS (SPI CS)                             │
│  P0.12 ─── SX1262 SCK (SPI CLK)                            │
│  P0.13 ─── SX1262 MISO                                     │
│  P0.14 ─── SX1262 MOSI                                    │
│  P0.15 ─── SX1262 DIO1 (IRQ)                               │
│  P0.16 ─── SX1262 RST                                     │
│  P0.17 ─── SX1262 BUSY                                    │
│  P0.18 ─── Battery voltage divider (×0.5)                 │
│  P0.19 ─── Solar voltage divider (×0.5)                   │
│  P0.20 ─── Status LED (green, via MOSFET)                  │
│  P0.21 ─── ISE power switch (MOSFET gate)                  │
│  P0.22 ─── VDDH enable (high-side switch)                  │
│                                                             │
│  VDD   ─── Decoupling: 4.7µF + 0.1µF                       │
│  VDDH  ─── High-side supply (switched for sensors)        │
│  SWDIO/SWDCLK ─── Programming header (TC2030 tag)          │
└─────────────────────────────────────────────────────────────┘
```

## Sensors

### Capacitive Soil Moisture — FDC2214Q1
- 4-channel resonant capacitance converter
- I²C interface (address 0x2A)
- Two channels for two soil depths (5 cm, 15 cm)
- PCB trace antenna embedded in probe tip
- Corrosion-free (no exposed metal in soil)

### Soil Temperature — DS18B20U+
- 1-Wire digital temperature
- ±0.5°C accuracy, 12-bit resolution
- Waterproof TO-92 or UFQFPN package
- 4.7 kΩ pullup to VDDH

### pH Probe + LMP7721
- Glass pH electrode (BNC connector)
- LMP7721 precision amplifier (femtoamp bias current)
- Output: 0–3.3V mapped to pH 0–14
- Calibration: two-point with pH 4.0 and 7.0 buffers

### NPK Ion-Selective Electrodes
- Three ISE probes (N, P, K) — replaceable
- ADC124S101 4-channel 12-bit ADC (SPI) — actually analog to nRF52 SAADC
- Power-gated via MOSFET (P0.21) to save power between readings
- Calibration: reference solution before each deployment season

### Ambient Light — VEML7700
- I²C (address 0x10)
- 0–120 klux dynamic range
- Used for photosynthesis estimation

## Power Management

### Solar Charging — MCP73871
- Input: 6V 5W solar panel
- Battery: LiFePO4 3.2V 1500 mAh (AA-size or custom)
- Charge current: 500 mA
- Overcharge: 3.65V, overdischarge: 2.5V
- Load sharing: solar charges battery and powers circuit simultaneously

### Voltage Regulation
- nRF52840 runs directly on 3.2V LiFePO4 (1.7–3.6V range)
- VDDH switched for sensor power (minimize idle consumption)
- ISE probes powered only during measurement (P0.21 MOSFET)

### Power Budget
| State | Current | Duration | Daily Energy |
|-------|---------|----------|-------------|
| Deep sleep | 3 µA | 14h 50m | 0.14 Wh |
| Measure | 8 mA | 10 min | 0.004 Wh |
| TX (LoRa) | 18 mA | 10 s | 0.0002 Wh |
| Mesh relay listen | 1 mA | 2 min | 0.001 Wh |
| **Total/day** | | | **~0.15 Wh** |

Solar: 5W × 4h = 20 Wh/day → **130× headroom**

## Mechanical

- IP67 stake enclosure (polycarbonate, UV-stabilized)
- Probe extends 30 cm into soil
- Solar panel on top surface, angled 15°
- BNC connector for pH probe (replaceable)
- ISE probes screwed into bottom (replaceable)