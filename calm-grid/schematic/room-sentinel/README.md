# Room Sentinel Schematic — CalmGrid

## MCU Architecture

Single SoC: **ESP32-S3** (N16R8) — handles 6-mic I2S array capture, on-device prosody analysis (TFLite Micro), ambient sensor I2C, and WiFi6 communication.

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │          ROOM SENTINEL NODE                 │
                    │          (wall/desk mounted)                │
                    │                                            │
  USB-C 5V ───────► │  3.3V LDO ──── all logic                   │
                    │                                            │
  ESP32-S3 ───────  │  I2S  ── 6× SPH0645LM4H-B mic array        │
                    │  I2C  ── VEML7700 (ambient light lux+CCT)   │
                    │  I2C  ── SHT40 (temp + humidity)            │
                    │  GPIO ── IR LEDs (night indicator)          │
                    │  GPIO ── Active LED (processing indicator)  │
                    │  GPIO ── Mic mute switch (privacy)          │
                    │  SPI  ── MicroSD (feature cache)            │
                    │  WiFi ── hub + cloud (features only!)       │
                    │                                            │
  6-mic array ───►  │  I2S → VAD → prosody features → CNN         │
                    │  (NO audio stored or transmitted)            │
                    │                                            │
  Privacy ────────  │  Physical mic mute switch + active LED      │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — ESP32-S3

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO4  | I2S WS   | 6-mic array (SPH0645) |
| GPIO5  | I2S BCK  | 6-mic array |
| GPIO6  | I2S DATA | 6-mic array data in |
| GPIO8  | I2C SDA  | VEML7700 + SHT40 |
| GPIO9  | I2C SCL  | VEML7700 + SHT40 |
| GPIO38 | IR LED EN | 940nm IR illumination |
| GPIO39 | Mic mute SW | physical mic mute switch |
| GPIO40 | Active LED | indicator when mic processing |
| GPIO0  | Boot     | + SD card via SPI |
| GPIO1-3 | SD SPI   | MicroSD (MOSI/MISO/SCK/CS) |

## 6-Microphone Array

```
  ┌───────────────────────────┐
  │    Room Sentinel (front)   │
  │                            │
  │    MIC1    MIC2    MIC3   │
  │                            │
  │         [ESP32-S3]         │
  │                            │
  │    MIC4    MIC5    MIC6   │
  │                            │
  │  ● Active LED  ● Mute SW  │
  └───────────────────────────┘
```

6-mic array enables:
- Voice activity detection (VAD) with spatial filtering
- Sound localization (which direction speech comes from)
- Noise reduction via beamforming
- Better prosody feature extraction

## Privacy Architecture

```
  Audio → [VAD] → Speech? → [Prosody Features] → [CNN] → Stress Class
                             (9 floats)           (0-3)
                                 │
                                 ▼
                          [WiFi/MQTT] → Hub/Cloud
                          (features + class ONLY)

  ✗ NO audio recording
  ✗ NO speech-to-text
  ✗ NO audio streaming
  ✓ Physical mic mute switch
  ✓ Active LED when processing
  ✓ Firmware-enforced: no audio buffer in TX path
```

## Power

- USB-C 5V, ~280mA active (mic + DSP), ~20mA idle
- Always plugged (no battery — continuous monitoring)

## Physical Design

- PCB: 60×60mm 4-layer FR4 (circular mic array layout)
- Enclosure: 70×70×25mm ABS, wall/desk mount bracket
- Mic array on front face, sensors on top edge
- USB-C on bottom, mute switch + active LED on front