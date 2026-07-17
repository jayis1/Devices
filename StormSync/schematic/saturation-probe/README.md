# Soil Saturation Probe Schematic

## Overview

The Soil Saturation Probe monitors moisture at 3 depths (15cm, 45cm, 90cm), pore water pressure, and soil temperature. Solar-powered with LiFePO4 battery.

## SoC: nRF52840 QFAA

- Cortex-M4F @ 64 MHz
- 1 MB flash, 256 KB RAM
- BLE 5.0 (not used for Sub-GHz)
- Ultra-low power: ~3 µA deep sleep

## Sensors

### Moisture: FDC2214Q1 (3-channel capacitive)
- 4-channel resonant capacitance sensor
- I²C interface (0x2A)
- Channels 0-2: 15cm, 45cm, 90cm depths
- Corrosion-free (capacitive, not resistive)
- Air value: 800, Water value: 350 (calibration)

### Pore Pressure: MPS20NR (MEMS)
- 0–100 kPa range
- ADC input (AIN7)
- Indicates groundwater table pressure

### Temperature: 3× DS18B20U+
- 1-Wire on separate GPIOs (P0.04, P0.05, P0.06)
- Waterproof, one per depth

## Power

- Solar: 5W 6V monocrystalline panel
- Battery: LiFePO4 3.2V 1500 mAh (2000+ cycle life)
- MCP73871 solar/battery management
- Sensor power-gating: MOSFET switch (P0.21)
- Average consumption: ~0.3 mA (duty-cycled, 15-min intervals)
- Autonomy: 90+ days without sun

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                    nRF52840 QFAA                            │
│                                                             │
│  P0.02 ─── FDC2214 SCL (I²C)                               │
│  P0.03 ─── FDC2214 SDA (I²C)                               │
│  P0.04 ─── DS18B20 #1 (1-Wire, 15cm)                       │
│  P0.05 ─── DS18B20 #2 (1-Wire, 45cm)                       │
│  P0.06 ─── DS18B20 #3 (1-Wire, 90cm)                       │
│  P0.07 ─── MPS20NR analog (AIN7)                           │
│  P0.08 ─── FDC2214 INT                                     │
│  P0.11 ─── SX1262 NSS (SPI CS)                             │
│  P0.12 ─── SX1262 SCK                                      │
│  P0.13 ─── SX1262 MISO                                     │
│  P0.14 ─── SX1262 MOSI                                     │
│  P0.15 ─── SX1262 DIO1                                     │
│  P0.16 ─── SX1262 RST                                      │
│  P0.17 ─── SX1262 BUSY                                     │
│  P0.18 ─── Battery voltage (AIN18)                         │
│  P0.19 ─── Solar voltage (AIN19)                           │
│  P0.20 ─── Status LED                                      │
│  P0.21 ─── Sensor power switch (MOSFET gate)               │
│  P0.22 ─── VDDH enable                                     │
└─────────────────────────────────────────────────────────────┘
```

## Probe Assembly

- 1-meter stainless steel rod with 3 capacitive sensors at 15/45/90cm
- Each sensor: PCB trace capacitor exposed to soil through conformal coating
- Cable from probe head to electronics enclosure (IP67)
- Solar panel mounted on top of stake