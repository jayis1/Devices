# AsthmaSync — Air Sentinel Schematic (KiCad)
# ESP32-S3-WROOM-1-N8R2 + PMSA003I + BME688 + SGP40 + SCD41 + SX1262

This folder contains the KiCad schematic project for the AsthmaSync Air Sentinel.

## Components
- **U1**: ESP32-S3-WROOM-1-N8R2 (8MB flash, 2MB PSRAM)
- **U2**: PMSA003I (Plantower PM1.0/2.5/10 particle sensor)
- **U3**: BME688 (Bosch VOC/IAQ + temp + humidity + pressure)
- **U4**: SGP40 (Sensirion VOC index)
- **U5**: SCD41 (Sensirion NDIR CO₂)
- **U6**: SX1262IMLTRT (Sub-GHz transceiver)
- **U7**: WS2812B status LED
- **U8**: TP4056 USB-C charger (battery backup)
- **BT1**: 18650 Li-ion battery holder

## I²C Bus (shared)
All sensors share a single I²C bus (GPIO 8=SCL, GPIO 9=SDA) at 100 kHz.
The SCD41 requires ≤100 kHz, so the bus is kept at that speed.

| Address | Device |
|---------|--------|
| 0x12 | PMSA003I |
| 0x62 | SCD41 |
| 0x59 | SGP40 |
| 0x77 | BME688 |

## Power Architecture
- **Main**: USB-C 5V
- **Backup**: 18650 Li-ion via TP4056
- **3.3V**: LDO (AMS1117-3.3)
- **Sensor power**: 3.3V (all sensors are 3.3V compatible)

## PCB
- 4-layer, 90×70mm, FR4
- Ventilated enclosure for PM sensor airflow
- SCD41 must be positioned away from heat sources