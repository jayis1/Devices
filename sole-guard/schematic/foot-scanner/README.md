# Foot Scanner Node Schematic — SoleGuard

## MCU

**ESP32-S3** (N16R8: 16MB flash, 8MB PSRAM) — WiFi6 + image capture + on-device TFLite wound detection + HX711 weight scale.

## Block Diagram

```
              ┌──────────────────────────────────────────────┐
              │           FOOT SCANNER NODE                    │
              │                                              │
  USB-C 5V ── │  ── 3.3V LDO ── ESP32-S3                     │
              │  ── 5V  ── LED ring (8× white + IR + UV)     │
              │                                              │
  ESP32-S3 ── │  DVP 8-bit ── OV5640 (5MP, AF)               │
              │  GPIO38 ── LED white MOSFET gate             │
              │  GPIO39 ── LED IR MOSFET gate                │
              │  GPIO40 ── LED UV MOSFET gate                │
              │  GPIO41 ── capacitive touch button           │
              │  GPIO44 ── HX711 SCK                         │
              │  GPIO43 ── HX711 DOUT                        │
              │  SPI2  ── ILI9341 2.4" TFT (scan UI)         │
              │  WiFi6 ── cloud upload + hub coordination    │
              │                                              │
  Scan pad ── │  transparent acrylic surface, camera below, │
              │  4× 50kg load cells (wheatstone bridge) ──   │
              │  HX711 24-bit ADC                            │
              └──────────────────────────────────────────────┘
```

## LED Ring (8× tri-mode)

- 4× white LEDs (Cree 5050, 4000K, 120lm) — standard color/texture imaging
- 2× 850nm IR LEDs — sub-surface inflammation (correlates with heat)
- 2× 405nm UV-A LEDs — fungal fluorescence (tinea pedis fluoresces)
- Each group driven by a logic-level MOSFET (AO3400A) from GPIO

## Pin Assignments — ESP32-S3

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO4  | SCCB SDA | OV5640 I2C (config + autofocus) |
| GPIO5  | SCCB SCL | OV5640 I2C |
| GPIO6  | VSYNC    | OV5640 |
| GPIO7  | HREF     | OV5640 |
| GPIO8  | Y4       | OV5640 data |
| GPIO9  | Y3       | OV5640 data |
| GPIO10 | Y5       | OV5640 data |
| GPIO11 | Y2       | OV5640 data |
| GPIO12 | Y6       | OV5640 data |
| GPIO13 | PCLK     | OV5640 |
| GPIO15 | XCLK     | OV5640 (20MHz) |
| GPIO16 | Y9       | OV5640 data |
| GPIO17 | Y8       | OV5640 data |
| GPIO18 | Y7       | OV5640 data |
| GPIO38 | LED white| MOSFET gate |
| GPIO39 | LED IR   | MOSFET gate |
| GPIO40 | LED UV   | MOSFET gate |
| GPIO41 | Touch    | capacitive button (start scan) |
| GPIO42 | TFT CS   | ILI9341 |
| GPIO43 | HX711 DOUT| load cell data |
| GPIO44 | HX711 SCK| load cell clock |
| GPIO45 | TFT DC   | ILI9341 |
| GPIO46 | TFT BL   | backlight |
| GPIO47 | SPI2 SCK | ILI9341 |
| GPIO48 | SPI2 MOSI| ILI9341 |

## Weight Scale (HX711 + 4× 50kg load cells)

```
   4× load cells (wheatstone bridges) in a square under the scan platform
         ┌──────┐
         │ LC1  │──┐
         ├──────┤  │  E+ (red)   ── HX711 E+
         │ LC2  │  ├── E- (black)── HX711 E-
         ├──────┤  │  A+ (green) ── HX711 A+
         │ LC3  │  │  A- (white) ── HX711 A-
         ├──────┤  │
         │ LC4  │──┘
         └──────┘
```
Calibrated to tare on power-up; resolution ~50g; max 200kg.

## Power

- USB-C 5V (always plugged, bathroom counter)
- No battery (mains appliance)
- 3.3V LDO for ESP32-S3 + OV5640; 5V direct for LED ring + HX711

## Enclosure

- 300×220×40mm scan pad, IP54
- Transparent acrylic top plate (camera sees through)
- LED ring around camera, below the acrylic
- HX711 + load cells in the base
- ESP32-S3 + TFT on the front edge (touch button + display visible to patient)

## KiCad Project

`schematic/foot-scanner/foot-scanner.kicad_sch` — schematic
`schematic/foot-scanner/foot-scanner.kicad_pcb` — 4-layer PCB