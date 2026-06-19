# Behavior Camera Schematic — PawSync

## MCU Architecture

Single SoC: **ESP32-S3** (N16R8) — dual-core 240MHz with vector instructions for on-device CV inference + TFLite Micro. 16MB Flash + 8MB PSRAM.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │         BEHAVIOR CAMERA NODE                │
                    │         (wall-mount, plugged)               │
                    │                                            │
  USB-C 5V ───────► │  RT9013 3.3V LDO ──── all logic            │
                    │                                            │
  ESP32-S3 ───────  │  Parallel ── OV5640 camera (5MP, AF)       │
                    │  I2S     ── 6× SPH0645 mic array           │
                    │  GPIO    ── 2× IR LED 940nm (night vision) │
                    │  GPIO    ── Privacy shutter switch          │
                    │  GPIO    ── Privacy active LED             │
                    │  SPI     ── MicroSD (event clips)         │
                    │  WiFi6   ── Hub + Cloud (MQTT events)     │
                    │                                            │
  TFLite Micro ──►  │  Behavior classifier (5fps)                │
  TFLite Micro ──►  │  Vocalization classifier (10Hz)            │
                    │  All inference on-device (no video stream) │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — ESP32-S3

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO4  | I2S WS   | 6-mic array (SPH0645) |
| GPIO5  | I2S BCK  | 6-mic array |
| GPIO6  | I2S DATA | 6-mic array data in |
| GPIO7  | CAM D0   | OV5640 parallel bus |
| GPIO8-15 | CAM D1-D7 | OV5640 parallel bus |
| GPIO16 | CAM PCLK | OV5640 pixel clock |
| GPIO17 | CAM VSYNC| OV5640 vsync |
| GPIO18 | CAM HREF | OV5640 href |
| GPIO19 | CAM SIOC | OV5640 SCCB (I2C) |
| GPIO20 | CAM SIOD | OV5640 SCCB |
| GPIO21 | CAM XCLK | OV5640 master clock (20MHz) |
| GPIO38 | IR LED EN | 940nm IR illumination |
| GPIO39 | Shutter SW | physical privacy shutter switch |
| GPIO47 | Privacy LED | camera active indicator |
| GPIO0  | Boot     | + SD card via SPI |
| GPIO1-3 | SD SPI   | MicroSD (MOSI/MISO/SCK/CS) |

## Power

- USB-C 5V, ~300mA active, ~20mA idle (IR-only night mode)
- No battery — always plugged

## Privacy

- Physical lens shutter (mechanical slider) + GPIO interrupt
- When closed: camera capture disabled, audio continues
- Privacy LED: red when camera is active (open shutter)
- All CV inference on-device — no video stream to cloud by default