# Smoke Node Schematic

## Overview

The Smoke Node monitors BBQ smoker smoke quality using a PMS5003
laser particulate sensor, BME680 VOC sensor, MQ-135 gas sensor, and
UV flame detector. Runs SmokeNet 1D-CNN on-device for 5-class smoke
quality classification.

## SoC: ESP32-S3-WROOM-1-N8R2

- 8 MB flash, 2 MB PSRAM
- Dual-core Xtensa LX7 @ 240 MHz

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
| GPIO11 | BME280/BME680 SDA | I²C0 | Shared I²C bus |
| GPIO12 | BME280/BME680 SCL | I²C0 | Shared I²C bus |
| GPIO13 | MQ-135 ADC | ADC1_CH4 | Gas concentration |
| GPIO14 | PMS5003 TX | UART1 | PM sensor TX |
| GPIO15 | PMS5003 RX | UART1 | PM sensor RX |
| GPIO16 | UV flame ADC | ADC1_CH5 | Flame intensity |
| GPIO17 | UV flame IRQ | GPIO | Flame interrupt |
| GPIO18 | LED | SK6812 | Smoke quality indicator |
| GPIO19 | PMS5003 EN | GPIO | PM sensor power gate |
| GPIO43 | USB TX | UART0 | Debug |
| GPIO44 | USB RX | UART0 | Debug |

## Power

- **Input:** USB-C 5V (continuous power)
- **LDO:** AMS1117-3.3
- **Current draw:** ~100 mA (PMS5003 fan + BME680 heater)

## Block Diagram

```
┌─────────────────────────────────────────────┐
│          ESP32-S3-WROOM-1-N8R2              │
│  ┌──────┐ ┌──────┐                          │
│  │ CPU0 │ │ CPU1 │                          │
│  └──┬───┘ └──┬───┘                          │
│     │        │                              │
│  ┌──┴────────┴──────┐                       │
│  │GPIO/SPI/I²C/UART │                       │
│  └──┬─────┬────┬───┘                        │
└─────┼─────┼────┼───────────────────────────┘
      │     │    │
   SPI2   I²C0  UART1+ADC
      │     │    │
  ┌───┴──┐ ┌┴──┐ ┌┴──────┐
  │SX1262│ │BME│ │PMS5003│
  │Radio │ │680│ │(PM2.5)│
  └──────┘ │   │ └──────┘
           │   │
           │BME│ ┌──────┐
           │280│ │MQ-135│
           └───┘ └──────┘
                  ┌──────┐
                  │UV    │
                  │Flame │
                  └──────┘
```

## Key Design Notes

1. **PMS5003:** Plantower laser PM sensor. UART at 9600 baud. 32-byte
   frame protocol with PM1.0, PM2.5, PM10 (factory + ambient calibrations).
   Internal fan runs continuously, ~10 s warmup.

2. **BME680:** Bosch 4-in-1 sensor: temp, humidity, pressure, VOC/gas
   resistance. I²C interface. BSEC library for VOC index calculation.
   Heater runs in 2s duty cycle.

3. **MQ-135:** Detects CO₂, NOx, ammonia, benzene. Used for smoke
   chemistry analysis. ADC read at 1 Hz. Baseline established on startup.

4. **UV flame sensor:** UV-TRON tube for UV-only flame detection
   (185–260 nm). Immune to visible/IR light. Detects smoker chamber
   flame through smoke (IR sensors can't see through smoke).

5. **Smoke classification:** SmokeNet 1D-CNN classifies smoke into
   5 classes:
   - Clean Blue (ideal, PM2.5 < 80, VOC < 150)
   - Thin Blue (good, PM2.5 80-120, VOC 100-200)
   - Dirty White (fair, PM2.5 120-150, VOC 150-250)
   - Creosote (bad, PM2.5 > 150, VOC > 250)
   - No Smoke (PM2.5 < 30)

6. **Enclosure:** Aluminum housing with PTFE filter for sensor intakes.
   Rated to 150°C ambient for smoker chamber mounting.

7. **I²C bus:** BME280 and BME680 share I²C bus at 100 kHz.
   BME680 at address 0x77, BME280 at 0x76.