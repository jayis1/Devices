# Skin Scanner Schematic — SkinSync

## MCU Architecture

Single-MCU: **ESP32-S3** (N16R8) — camera driver + WiFi6 + on-device condition CNN (TFLite Micro).

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │              SKIN SCANNER NODE               │
                    │              (handheld multispectral)         │
                    │                                              │
  18650 ──────────  │  TP4056 ── LiPo 2600mAh ── 3.3V LDO         │
  + USB-C charging  │                                              │
                    │  ESP32-S3 (N16R8) ─── all peripherals        │
                    │  ├── DVP  ── OV5640 5MP camera (autofocus)  │
                    │  ├── I2C  ── SH1106 1.3" OLED                │
                    │  ├── I2C  ── LSM6DSL IMU (angle guidance)   │
                    │  ├── GPIO ── 4× LED ring drivers             │
                    │  │    ├── White 5500K (surface view)         │
                    │  │    ├── UV 365nm (bacterial fluorescence)  │
                    │  │    ├── NIR 850nm (sub-surface + moisture) │
                    │  │    └── Polarized white (glare-free tone)  │
                    │  ├── SPI  ── MicroSD (image archive)         │
                    │  ├── GPIO ── 4× tactile buttons              │
                    │  └── WiFi6 ── Hub/Cloud upload               │
                    │                                              │
  OV5640 ─────────  │  5MP autofocus camera with 4-mode LED ring   │
  + LED ring        │  Capture: white + UV + NIR + polarized (3s)  │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — ESP32-S3

| Pin | Function | Notes |
|-----|----------|-------|
| IO0  | Button 1 | Capture |
| IO1  | Button 2 | Mode (white/UV/NIR/polarized) |
| IO2  | Button 3 | Lesion mark (tag current scan to a lesion) |
| IO3  | Button 4 | Identify (trigger on-device CNN) |
| IO4  | LED EN   | White 5500K LED ring driver (MOSFET gate) |
| IO5  | LED EN   | UV 365nm LED ring driver |
| IO6  | LED EN   | NIR 850nm LED ring driver |
| IO7  | LED EN   | Polarized white LED ring driver |
| IO8  | OLED SDA | I2C to SH1106 |
| IO9  | OLED SCL | I2C to SH1106 |
| IO10 | IMU SDA  | I2C to LSM6DSL |
| IO11 | IMU SCL  | I2C to LSM6DSL |
| IO12 | SD CS    | MicroSD card select |
| IO13 | SD SCK   | SPI to MicroSD |
| IO14 | SD MOSI  | SPI to MicroSD |
| IO15 | SD MISO  | SPI to MicroSD |
| IO21 | CAM D0   | OV5640 parallel data bus (8-bit) |
| IO22 | CAM D1   | |
| IO23 | CAM D2   | |
| IO24 | CAM D3   | |
| IO25 | CAM D4   | |
| IO26 | CAM D5   | |
| IO27 | CAM D6   | |
| IO28 | CAM D7   | |
| IO29 | CAM PCLK | OV5640 pixel clock |
| IO30 | CAM VSYNC| OV5640 vertical sync |
| IO31 | CAM HREF | OV5645 horizontal reference |
| IO32 | CAM SIOC | OV5640 I2C SCCB clock |
| IO33 | CAM SIOD | OV5640 I2C SCCB data |
| IO34 | CAM XCLK | OV5640 external clock (20MHz) |
| IO35 | CAM PWDN | OV5640 power down |
| IO36 | CAM RST  | OV5640 reset |

## LED Ring Design

4 concentric LED rings, each with independent MOSFET driver:
- **White 5500K:** 12× SMD LEDs, 5500K color temp, CRI>90 — surface texture, pore visibility, general imaging
- **UV 365nm:** 6× UV-A LEDs, 365nm — *C. acnes* fluoresces orange-red, fungal fluoresce green-yellow, photodamage patterns
- **NIR 850nm:** 8× IR LEDs, 850nm — sub-surface inflammation, moisture content (water absorption at NIR), wrinkle depth topography
- **Polarized white:** 12× white LEDs + linear polarizer film — eliminates surface glare/oil reflection, reveals true skin tone and lesion borders

## Power

- 18650 LiPo 2600mAh + USB-C charging (TP4056)
- 3.3V LDO for MCU + sensors
- LED rings: 3.0-3.3V direct from battery (MOSFET switched)
- Camera: 3.3V + 1.8V (internal LDO on OV5640)
- Estimated: ~150 scans per charge (LED ring is primary power draw)

## Physical Design

- PCB: 90×40mm 4-layer FR4
- Enclosure: 95×45×30mm pen-like handheld, matte black
- Camera + LED ring on top face (contact or close-focus)
- OLED + buttons on front face
- USB-C on bottom
- MicroSD slot on side
- Lanyard hole