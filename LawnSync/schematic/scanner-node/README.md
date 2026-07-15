# Lawn Scanner Schematic

## Overview

Solar-assisted multispectral lawn imaging node with on-device ML inference. Captures RGB and NIR images for NDVI computation, disease detection, and weed mapping. Reports results via Sub-GHz mesh and uploads images via Hub/Wi-Fi.

## MCU: ESP32-S3-WROOM-1-N16R8

- Dual-core Xtensa LX7 @ 240 MHz
- 16 MB flash (stores TFLite-Micro models), 8 MB PSRAM (image buffer)
- Wi-Fi 4 + BLE 5.0

## Sub-GHz Radio: SX1262IMLTRT

- 868 MHz, +22 dBm
- SPI interface

## Schematic Blocks

```
┌─────────────────────────────────────────────────────────────┐
│                   ESP32-S3-WROOM-1                          │
│                                                             │
│  GPIO4-11  ─── OV5640 DVP data (D0-D7)                    │
│  GPIO12    ─── OV5640 PCLK (pixel clock)                   │
│  GPIO13    ─── OV5640 HSYNC (horizontal sync)              │
│  GPIO14    ─── OV5640 VSYNC (vertical sync)                 │
│  GPIO15    ─── OV5640 XCLK (camera clock, 20 MHz)           │
│  GPIO16    ─── OV5640 SDA (SCCB I²C, address 0x3C)          │
│  GPIO17    ─── OV5640 SCL (SCCB I²C)                       │
│  GPIO18    ─── OV5640 PWDN (power down)                    │
│  GPIO19    ─── OV5640 RESET                                 │
│  GPIO20    ─── NIR LED array enable (MOSFET gate)           │
│  GPIO21    ─── White LED ring enable (MOSFET gate)          │
│  GPIO22    ─── TSL2591 SDA (I²C bus 1, address 0x29)       │
│  GPIO23    ─── TSL2591 SCL (I²C bus 1)                     │
│  GPIO24    ─── LSM6DSO SDA (I²C bus 1, address 0x6A)        │
│  GPIO25    ─── LSM6DSO SCL (I²C bus 1)                    │
│  GPIO26    ─── NEO-M9N TX (UART from GPS)                  │
│  GPIO27    ─── NEO-M9N RX (UART to GPS)                    │
│  GPIO28    ─── SX1262 NSS (SPI CS)                         │
│  GPIO29    ─── SX1262 SCK (SPI CLK)                        │
│  GPIO30    ─── SX1262 MISO                                │
│  GPIO31    ─── SX1262 MOSI                                │
│  GPIO32    ─── SX1262 DIO1 (IRQ)                          │
│  GPIO33    ─── SX1262 RST                                │
│  GPIO34    ─── SX1262 BUSY                               │
│  GPIO35    ─── Battery voltage (ADC)                       │
│  GPIO36    ─── Status LED (blue)                          │
│  GPIO37    ─── Shutter button (manual trigger)              │
│                                                             │
│  3V3   ─── Decoupling: 10µF + 0.1µF                       │
│  EN    ─── 10kΩ pullup                                     │
│  IO0   ─── 10kΩ pullup + button                             │
└─────────────────────────────────────────────────────────────┘
```

## Camera: OV5640 (5MP, Autofocus)

- DVP parallel interface (8-bit data)
- SCCB (I²C-compatible) control
- Integrated autofocus motor driver
- IR-cut filter (removable for NIR sensitivity)
- Resolution: 2592×1944 native, downsample to 224×224 for ML
- Frame rate: 15 fps at full resolution, 60 fps at VGA

### Capture Sequence
1. White LED ring ON → RGB image (1920×1080) for disease/weed detection
2. White LED ring OFF → NIR LED array ON → NIR image (1920×1080)
3. NIR LED OFF → compute NDVI per pixel
4. Downsample images for on-device inference (224×224 RGB → DiseaseNet)

## Illumination

### White LED Ring
- 6500K high-CRI LEDs (Cree XLamp 5050 or equivalent)
- 16 LEDs in ring around camera lens
- Uniform illumination for close-range (30–50 cm) imaging
- MOSFET switched (AO3400A), 500 mA peak

### NIR LED Array
- 850 nm IR LEDs (Vishay VSLY5850)
- 8 LEDs for NIR illumination
- Used for NDVI: (NIR - Red) / (NIR + Red)
- MOSFET switched (AO3400A), 300 mA peak

## Light Sensor: TSL2591

- I²C address 0x29
- 0–88 klux dynamic range
- Full-spectrum + IR channels
- Used for auto-exposure control

## IMU: LSM6DSO

- I²C address 0x6A
- 3-axis accel + 3-axis gyro
- Used for image orientation tagging
- Helps identify camera tilt for repeat imaging

## GPS: u-blox NEO-M9N

- UART interface (9600 baud default)
- Sub-meter positioning (SBAS augmented)
- Cold start: <30s, warm start: <5s
- Used for geo-tagging images to create NDVI maps of lawn zones

## Edge ML: TFLite-Micro / ESP-DL

### DiseaseNet (int8 quantized)
- MobileNetV3-Small backbone
- 15 classes, ~670 KB
- Inference time: ~200 ms per image on ESP32-S3

### WeedSeg (int8 quantized)
- U-Net-tiny encoder (MobileNetV2)
- 9 classes, ~1.2 MB
- Inference time: ~800 ms per 512×512 image

### Model Storage
- Stored in external 16 MB flash (alongside firmware)
- OTA update via Sub-GHz mesh or Wi-Fi

## Power Management

### Solar — MCP73871
- Input: 5V 3W solar panel
- Battery: LiFePO4 3.2V 2500 mAh
- Charge current: 400 mA

### Power Budget
| State | Current | Duration | Daily Energy |
|-------|---------|----------|-------------|
| Deep sleep | 15 µA | ~23h | 0.001 Wh |
| Camera + LEDs | 250 mA | 60s (2 captures) | 0.014 Wh |
| ML inference | 100 mA | 3s | 0.0008 Wh |
| GPS acquisition | 40 mA | 30s | 0.001 Wh |
| TX (LoRa) | 20 mA | 2s | 0.00004 Wh |
| **Total/day** | | | **~0.02 Wh** |

Solar: 3W × 3h = 9 Wh/day → **450× headroom** (14+ days no sun)

## Mechanical

- IP65 pole-mount enclosure
- Adjustable mounting bracket (0–45° tilt)
- Camera window: AR-coated optical glass
- LED diffusers for uniform illumination
- Pole height: 1.5–2 m above lawn
- Solar panel on top, adjustable tilt