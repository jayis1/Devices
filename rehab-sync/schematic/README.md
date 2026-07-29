# RehabSync Schematics

KiCad schematic projects for each hardware node in the RehabSync system.

## Nodes

| Folder | Node | SoC | Description |
|--------|------|-----|-------------|
| `hub/` | Rehab Hub | ESP32-S3 | Central gateway + BLE BAN coordinator + display + audio + haptic |
| `body-sensor/` | Body Sensor | nRF52840 | LSM6DSO + LIS3MDL 9-DoF IMU + BLE 5.0 + CR2032 |
| `smart-band/` | Smart Band | nRF52840 | HX711 + 50kg load cell + LSM6DSO + BLE 5.0 + LiPo |
| `pressure-mat/` | Pressure Mat | ESP32-S3 | 16×16 FSR array + ADS1115 + SX1262 Sub-GHz |

Each schematic folder contains:
- `README.md` — Detailed schematic description with pin assignments and bus topology
- KiCad project files (in production: `.kicad_sch`, `.kicad_pcb`, `.kicad_pro`)

## Power Architecture

```
Hub:
  USB-C 5V ──→ MCP73871 (LiPo charger) ──→ 2000mAh LiPo ──→ TPS61023 boost (5V) ──→ AMS1117-3.3 ──→ 3.3V

Body Sensor:
  CR2032 3V ──→ (direct 3V rail) ──→ nRF52840 (1.7-3.6V) + LSM6DSO + LIS3MDL

Smart Band:
  USB-C 5V ──→ MCP73831 (LiPo charger) ──→ 300mAh LiPo ──→ TPS61023 boost (3.3V) ──→ nRF52840 + HX711 + LSM6DSO

Pressure Mat:
  USB-C 5V ──→ AMS1117-3.3 LDO ──→ 3.3V (ESP32-S3 + SX1262 + ADS1115)
```

## Bus Topology

```
ESP32-S3 (Hub):
  ├── SPI2: SX1262 (CS=GPIO6, SCK=GPIO8, MISO=GPIO9, MOSI=GPIO10)
  ├── SPI3: Hub IMU LSM6DSO (CS=GPIO14, SCK=GPIO15, MISO=GPIO16, MOSI=GPIO17)
  ├── SPI3: microSD (CS=GPIO18, shared bus)
  ├── SPI4: TFT Display ILI9488 (CS=GPIO37, SCK=GPIO35, MOSI=GPIO36)
  ├── I²C0: BME280 + DS3231 + DRV2605L + MAX17048 (SDA=GPIO11, SCL=GPIO12)
  ├── I²S: MAX98357A audio (BCLK=GPIO41, LRCK=GPIO42, DIN=GPIO45)
  ├── GPIO: TFT DC/RST/BL, LED status
  └── UART0: USB-C debug (TX=GPIO43, RX=GPIO44)

nRF52840 (Body Sensor):
  ├── SPI0: LSM6DSO (CS=P0.02, SCK=P0.04, MISO=P0.05, MOSI=P0.06)
  ├── SPI0: LIS3MDL (CS=P0.03, shared SPI bus)
  ├── GPIO: LED (P0.09), Button (P0.11), IMU INT (P0.07), Mag INT (P0.08)
  └── BLE 5.0: GATT server to Hub

nRF52840 (Smart Band):
  ├── GPIO: HX711 SCK (P0.02), DOUT (P0.03) — bit-banged
  ├── SPI0: LSM6DSO (CS=P0.04, SCK=P0.05, MISO=P0.06, MOSI=P0.07)
  ├── I²C: MAX17048 (SDA=P0.09, SCL=P0.10)
  └── BLE 5.0: GATT server to Hub

ESP32-S3 (Pressure Mat):
  ├── SPI2: SX1262 (CS=GPIO6, SCK=GPIO8, MISO=GPIO9, MOSI=GPIO10)
  ├── I²C0: ADS1115 (SDA=GPIO11, SCL=GPIO12)
  ├── GPIO: MUX row/col select (GPIO13-20)
  └── GPIO: LED (GPIO42)
```