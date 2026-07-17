# Sump Pit Sentinel Schematic

## Overview

The Sump Pit Sentinel monitors water level, pump operation, vibration, flow, and temperature in the sump pit. It is the most critical sensor node — it must operate during power outages (which coincide with storms).

## SoC: ESP32-WROOM-32E

- Dual-core Xtensa LX6 @ 240 MHz
- 4 MB flash
- Wi-Fi 4 (2.4 GHz, not used — Sub-GHz only)
- 34 GPIOs

## Key Sensors

### Water Level: JSN-SR04T (Ultrasonic)
- Range: 25–450 cm, ±1 cm
- IP67 waterproof transducer
- Mounts at top of sump pit, pointing down
- Trigger (GPIO25) + Echo (GPIO26)
- water_level = pit_depth - measured_distance

### Pump Current: SCT-013-030 (CT Clamp)
- 30A non-invasive current transformer
- 1V output at 30A → 10Ω burden resistor
- ADC1_CH0 (GPIO27)
- Sample at 1 kHz for 20ms → compute RMS
- Detects: pump on/off, overload, startup surge shape

### Vibration: ADXL355 (3-axis Accelerometer)
- ±2g/±4g/±8g ranges, 20-bit resolution
- Ultra-low noise: 25 µg/√Hz
- SPI interface (GPIO14-16)
- 1024 samples at 1 kHz → RMS + peak for cloud FFT
- Detects: bearing wear, impeller damage, motor degradation

### Flow: YF-S201 (Hall Effect)
- 1–30 L/min, pulse output
- GPIO17 (input)
- F = pulse_freq / 7.5 (L/min)

### Water Temp: DS18B20U+
- 1-Wire on GPIO32
- ±0.5°C, waterproof
- Monitors sump water temperature (cold water = groundwater, warm = other sources)

## Power Architecture

- **Primary:** 24VAC transformer (mains)
  - LM2596S-5.0 buck → 5V → AP2112K-3.3 → 3.3V
- **Backup:** 12V 5Ah SLA battery (critical!)
  - MCP73871 float charger (charges when mains present)
  - Automatic switchover when mains lost
  - 48 hours monitoring at reduced 60s sample rate
- **Mains detection:** GPIO34 (input-only, optocoupler isolated)

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-WROOM-32E                          │
│                                                             │
│  GPIO4  ─── SX1262 NSS (SPI CS)                            │
│  GPIO5  ─── SX1262 SCK                                     │
│  GPIO18 ─── SX1262 MISO                                    │
│  GPIO23 ─── SX1262 MOSI                                    │
│  GPIO19 ─── SX1262 DIO1                                    │
│  GPIO22 ─── SX1262 RST                                     │
│  GPIO21 ─── SX1262 BUSY                                    │
│  GPIO25 ─── JSN-SR04T Trigger                              │
│  GPIO26 ─── JSN-SR04T Echo                                 │
│  GPIO27 ─── CT Clamp (ADC1_CH0)                            │
│  GPIO14 ─── ADXL355 CS                                     │
│  GPIO12 ─── ADXL355 SCK                                    │
│  GPIO13 ─── ADXL355 MISO                                   │
│  GPIO15 ─── ADXL355 MOSI                                   │
│  GPIO16 ─── ADXL355 INT1                                   │
│  GPIO17 ─── Flow meter pulse                               │
│  GPIO32 ─── DS18B20 (1-Wire)                               │
│  GPIO33 ─── Battery voltage (ADC1_CH5)                     │
│  GPIO34 ─── Mains detect (input only)                      │
│  GPIO35 ─── Pump running LED                               │
└─────────────────────────────────────────────────────────────┘
```

## PCB Notes

- 4-layer PCB: signal / GND / power / signal
- CT clamp analog trace: keep short, away from digital
- ADXL355: mount close to pump mounting bracket for best vibration coupling
- Ultrasonic transducer: external cable to pit-top mount
- IP67 enclosure above sump pit (electronics must not get wet!)