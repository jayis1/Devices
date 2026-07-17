# Weather Sentinel Schematic

## Overview

The Weather Sentinel monitors rain accumulation, wind speed/direction, barometric pressure trend, temperature, and humidity. Solar-powered with LiFePO4 battery.

## SoC: ESP32-S3-WROOM-1-N8R2

- Dual-core Xtensa LX7 @ 240 MHz
- 8 MB flash, 2 MB PSRAM
- Rich peripheral set for counters + interrupts

## Sensors

### BME280 (Temp/Humidity/Pressure)
- I²C 0x76
- Critical: barometric pressure trend is a key storm predictor
- 3-hour rolling average vs baseline → rising/steady/falling

### Davis 6410 Anemometer + Vane
- Wind speed: reed switch, pulse counting (GPIO8, ISR)
- Wind direction: potentiometer, 0-360° (GPIO9, ADC)
- 1 pulse = 0.27 m/s

### Optolis TB-204 Tipping Bucket Rain Gauge
- 0.2 mm per tip
- Pulse output (GPIO10, ISR)
- Cumulative tips per 5-min report

## Power

- Solar: 5W 6V monocrystalline
- Battery: LiFePO4 3.2V 3000 mAh
- MCP73871 solar/battery management
- Average consumption: ~2 mA
- Autonomy: 30 days without sun

## PCB Notes

- IP65 Stevenson screen enclosure (UV-resistant ASA)
- Cable glands for anemometer and rain gauge
- Solar panel mounted on enclosure top at 45° angle