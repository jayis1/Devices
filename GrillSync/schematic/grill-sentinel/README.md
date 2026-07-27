# Grill Sentinel Schematic

## Overview

The Grill Sentinel monitors the grill surface using a MLX90640 32×24
thermal array, detects propane gas leaks (MQ-2), flame (IR detector),
and flare-up acoustic patterns (piezo). Runs FlareUpNet LSTM on-device
for 8–15s flare-up prediction.

## SoC: ESP32-S3-WROOM-1-N8R2

- 8 MB flash, 2 MB PSRAM
- Dual-core Xtensa LX7 @ 240 MHz
- Vector instructions for LSTM acceleration

## Pin Assignments

| GPIO | Function | Bus | Notes |
|------|----------|-----|-------|
| GPIO4 | SX1262 DIO1 | Radio IRQ | Interrupt |
| GPIO5 | SX1262 BUSY | Radio | Busy signal |
| GPIO6 | SX1262 NSS | SPI2 CS | Radio CS |
| GPIO7 | SX1262 RST | GPIO | Radio reset |
| GPIO8 | SX1262 SCK | SPI2 | Radio clock |
| GPIO9 | SX1262 MISO | SPI2 | Radio MISO |
| GPIO10 | SX1262 MOSI | SPI2 | Radio MOSI |
| GPIO11 | MLX90640 SDA | I²C0 | Thermal array (400 kHz) |
| GPIO12 | MLX90640 SCL | I²C0 | Thermal array |
| GPIO13 | BME280 SDA | I²C0 | Ambient (shared bus) |
| GPIO14 | BME280 SCL | I²C0 | Ambient |
| GPIO15 | MQ-2 ADC | ADC1_CH4 | Gas concentration |
| GPIO16 | Flame ADC | ADC1_CH5 | IR flame intensity |
| GPIO17 | Flame IRQ | GPIO | Flame threshold interrupt |
| GPIO18 | Piezo ADC | ADC1_CH7 | Acoustic energy |
| GPIO19 | LED | SK6812 | Status indicator |
| GPIO20 | Thermal EN | GPIO | MLX90640 power gate |
| GPIO43 | USB TX | UART0 | Debug |
| GPIO44 | USB RX | UART0 | Debug |

## Power

- **Input:** USB-C 5V (continuous power)
- **LDO:** AMS1117-3.3
- **Current draw:** ~120 mA (MLX90640 is power-hungry)

## Block Diagram

```
┌─────────────────────────────────────────────┐
│          ESP32-S3-WROOM-1-N8R2              │
│  ┌──────┐ ┌──────┐ ┌──────┐                │
│  │ CPU0 │ │ CPU1 │ │ BLE  │                │
│  └──┬───┘ └──┬───┘ └──────┘                │
│     │        │                              │
│  ┌──┴────────┴──────┐                       │
│  │  GPIO/SPI/I²C/ADC│                       │
│  └──┬─────┬────┬───┘                        │
└─────┼─────┼────┼───────────────────────────┘
      │     │    │
   SPI2   I²C0  ADC
      │     │    │
  ┌───┴──┐ ┌┴──┐ ┌┴──────┐
  │SX1262│ │MLX│ │MQ-2  │
  │Radio │ │906│ │ (Gas)│
  └──────┘ │40 │ └──────┘
           └───┘  ┌──────┐
           ┌───┐  │Flame │
           │BME│  │ IR   │
           │280│  └──────┘
           └───┘  ┌──────┐
                  │Piezo│
                  └──────┘
```

## Key Design Notes

1. **MLX90640 thermal array:** 32×24 pixel IR sensor. I²C at 400 kHz
   (required for 16 Hz refresh). Provides 768-pixel temperature map of
   grill surface. Range: -40°C to +350°C, ±1°C accuracy.

2. **MQ-2 gas sensor:** Detects propane, natural gas, LPG. Sensitivity:
   300–10000 ppm. 10% LEL alarm at 2100 ppm. Requires 30s warmup.

3. **IR flame detector:** GY-302 module with photodiode sensitive to
   flame IR emission (760–1100 nm). ADC + threshold GPIO interrupt.

4. **Piezo acoustic sensor:** 35mm piezo disc mounted on grill body to
   detect fat-drip sounds and flare-up acoustic patterns. ADC sampled
   at 4 kHz, RMS computed over 50-sample windows.

5. **Enclosure:** Glass-filled ASA (rated to 120°C ambient), with
   AR-coated Fresnel lens for the MLX90640. Grill-side shelf mount.

6. **Thermal frame compression:** 768 pixels compressed to 40–120 bytes
   using delta encoding + RLE, transmitted at 2 Hz during active cook.