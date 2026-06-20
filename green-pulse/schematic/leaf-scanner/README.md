# Leaf Scanner Schematic — GreenPulse

## MCU Architecture

Single-MCU: **ESP32-S3** (N16R8 — 16MB Flash, 8MB PSRAM). Handles camera, WiFi, on-device species-ID CNN (TFLite Micro), and OLED display.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │           LEAF SCANNER (handheld)            │
                    │                                              │
  18650 3.7V ────►  │  TP4056 ── LiPo 2600mAh ── 3.3V LDO         │
  (USB-C charge)    │                                              │
                    │  ESP32-S3 (N16R8)                             │
                    │  ├── DVP  ── OV5640 5MP camera (autofocus)   │
                    │  ├── SPI  ── SH1106 1.3" OLED                 │
                    │  ├── GPIO ── LED ring (white / UV / NIR)     │
                    │  ├── GPIO ── 3× tactile buttons              │
                    │  ├── SDIO ── MicroSD (image archive)         │
                    │  └── WiFi ── Hub / Cloud upload              │
                    │                                              │
  OV5640 ─────────  │  5MP autofocus, YUV/RGB output               │
                    │  DVP parallel interface to ESP32-S3          │
                    │                                              │
  LED Ring ───────  │  3 illumination modes:                       │
                    │  ├── White 5500K — visible (human-eye view)  │
                    │  ├── UV 365nm — pest/fungal fluorescence     │
                    │  └── NIR 850nm — pre-symptomatic stress      │
                    │  Sequenced: 3 shots in 2 seconds            │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — ESP32-S3

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO2  | OV5640 PWDN | Camera power-down |
| GPIO3  | OV5640 RESET | Camera reset |
| GPIO4  | LED White | 5500K white LED ring (MOSFET driven) |
| GPIO5  | LED UV | 365nm UV LED (MOSFET driven, safety interlock) |
| GPIO6  | LED NIR | 850nm NIR LED (MOSFET driven) |
| GPIO7  | Button: Capture | Active low, pull-up |
| GPIO8  | Button: Mode | Cycle white/UV/NIR/multi |
| GPIO9  | Button: Identify | Cycle target tag ID |
| GPIO10 | SD CS | MicroSD SPI select |
| GPIO11 | OLED DC | SH1106 data/command |
| GPIO12 | SPI SCK | Shared SPI (OLED + SD) |
| GPIO13 | SPI MOSI | Shared SPI |
| GPIO14 | SPI MISO | Shared SPI (SD) |
| GPIO15 | OLED CS | SH1106 chip select |
| GPIO16-23 | DVP D0-D7 | OV5640 parallel data (8-bit) |
| GPIO24 | DVP PCLK | OV5640 pixel clock |
| GPIO25 | DVP HSYNC | OV5640 horizontal sync |
| GPIO26 | DVP VSYNC | OV5640 vertical sync |
| GPIO27 | DVP XCLK | OV5640 master clock (20 MHz) |
| GPIO28 | ADC | Battery voltage (1/3 divider) |
| GPIO29 | GPIO | UV safety interlock (active high = enabled) |

## Multispectral Imaging

- **White (5500K):** Human-eye-equivalent visible light. Used for species-ID CNN and user preview on OLED. Reveals visible symptoms (yellowing, spots, webbing).
- **UV (365nm):** Causes fungal structures and pest residues to fluoresce. Powdery mildew fluoresces blue-white; some pest frass fluoresces yellow-green. Reveals infection not visible under white light.
- **NIR (850nm):** Healthy leaf tissue reflects NIR strongly (~50-70%); stressed tissue reflects less. Pre-symptomatic water stress, nutrient deficiency, and early disease reduce NIR reflectance days before visible symptoms. This is the same principle used in satellite/precision-ag crop stress monitoring (NDVI).

The 3-shot sequence (white → UV → NIR, each 100ms exposure + 100ms settle) completes in 2 seconds. Images are stored individually and as a fused multispectral tensor for the cloud disease/pest CNN.

## Power

- 18650 2600mAh LiPo + USB-C (TP4056 charging)
- Active: ~150 mA (camera + LEDs + WiFi) for ~5 sec per scan
- Sleep: ~1 mA (OLED off, WiFi off)
- Battery life: ~200 scans per charge, ~2 weeks standby

## UV Safety

- 365nm UV LED has safety interlock: only energized when capture button is pressed and held
- Auto-off after 200ms exposure per shot
- UV LED has physical shutter cover (slide open only during scan)
- Maximum UV exposure per scan: <1 mW/cm² at 50mm for 200ms = safe for brief use

## Physical Design

- PCB: 70×35mm 4-layer FR4 (wand form factor)
- Enclosure: 80×40×30mm handheld wand (like a digital thermometer)
- Camera on tip, OLED + buttons on body, USB-C on base
- LED ring around camera lens (12 LEDs: 6 white, 3 UV, 3 NIR)
- Weight: ~120g with battery