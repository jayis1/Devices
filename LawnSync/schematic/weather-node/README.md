# Weather Station Schematic

## Overview

Solar-powered outdoor weather station. Measures temperature, humidity, pressure, wind speed/direction, rainfall, solar irradiance, and UV index. Reports every 5 minutes via Sub-GHz mesh.

## MCU: ESP32-S3-WROOM-1-N8R2

- Dual-core Xtensa LX7 @ 240 MHz
- 8 MB flash, 2 MB PSRAM
- Wi-Fi 4 + BLE 5.0 (BLE for setup, Sub-GHz for data)

## Sub-GHz Radio: SX1262IMLTRT

- 868 MHz, +22 dBm
- SPI interface, DIO1 interrupt

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                   ESP32-S3-WROOM-1                          │
│                                                             │
│  GPIO4  ─── BME280 SDA (I²C bus 0)                         │
│  GPIO5  ─── BME280 SCL (I²C bus 0)                         │
│  GPIO6  ─── VEML6075 SDA (I²C bus 1)                       │
│  GPIO7  ─── VEML6075 SCL (I²C bus 1)                       │
│  GPIO8  ─── Wind speed pulse (counter interrupt)           │
│  GPIO9  ─── Wind direction (ADC, 0–3.3V)                   │
│  GPIO10 ─── Rain gauge pulse (counter interrupt)            │
│  GPIO11 ─── Solar irradiance (ADC)                         │
│  GPIO12 ─── SX1262 NSS (SPI CS)                             │
│  GPIO13 ─── SX1262 SCK (SPI CLK)                           │
│  GPIO14 ─── SX1262 MISO                                    │
│  GPIO15 ─── SX1262 MOSI                                    │
│  GPIO16 ─── SX1262 DIO1 (IRQ)                              │
│  GPIO17 ─── SX1262 RST                                    │
│  GPIO18 ─── SX1262 BUSY                                   │
│  GPIO19 ─── Battery voltage (ADC)                          │
│  GPIO20 ─── Status LED (green)                             │
│  GPIO21 ─── Rain sensor tip (backup/secondary)             │
│                                                             │
│  3V3   ─── Decoupling: 10µF + 0.1µF                       │
│  EN    ─── 10kΩ pullup                                     │
│  IO0   ─── 10kΩ pullup + button                             │
└─────────────────────────────────────────────────────────────┘
```

## Sensors

### BME280 — Temperature / Humidity / Pressure
- I²C address 0x76
- ±1°C temp, ±3% humidity, ±1 hPa pressure
- Used for ET₀ (evapotranspiration) calculation

### VEML6075 — UV Index
- I²C address 0x10
- UVA + UVB sensing
- UV index 0–15+

### Davis 6410 Anemometer + Vane
- Wind speed: Reed switch, 1 pulse per 2.25 km/h (or ~0.62 m/s per pulse)
- Wind direction: Potentiometer 0–360° (0–20 kΩ)
- Voltage divider: 10 kΩ reference resistor → 3.3V supply
- Direction voltage: 0–3.3V mapped to 0–360°

### Tipping Bucket Rain Gauge — Optolis TB-204
- 0.2 mm per tip
- Reed switch, GPIO interrupt
- Heated version available for snow detection (optional upgrade)

### Solar Irradiance — Photodiode / Si Cell
- Monocrystalline silicon photodiode (Hamamatsu S11371 or equivalent)
- Transimpedance amplifier (OPA333)
- Output: 0–3.3V mapped to 0–2000 W/m²
- Used for ET₀ (Penman-Monteith equation)

## Power Management

### Solar — MCP73871
- Input: 6V 5W solar panel
- Battery: LiFePO4 3.2V 3000 mAh
- Charge current: 500 mA
- Load sharing (solar powers circuit + charges battery)

### Regulation
- AP2112K-3.3 LDO: 3.2V LiFePO4 → 3.3V (low dropout)
- Or run ESP32-S3 directly on 3.2V (acceptable range 3.0–3.6V)

### Power Budget
| State | Current | Duration | Daily Energy |
|-------|---------|----------|-------------|
| Deep sleep | 10 µA | ~4m50s | negligible |
| Awake + measure | 50 mA | 10s × 288/day | 0.4 Wh |
| TX (LoRa SF7) | 20 mA | 0.5s × 288 | 0.008 Wh |
| **Total/day** | | | **~0.4 Wh** |

Solar: 5W × 4h = 20 Wh/day → **50× headroom** (30 days autonomy in dark)

## Enclosure

- Stevenson screen (UV-stabilized ASA plastic)
- IP65 rated
- Separate radiation shield for BME280
- Solar panel mounted on top at 15° tilt
- Anemometer/vane mounted on 1.5–2 m pole (included)
- Rain gauge mounted separately on level surface (included)