# Acoustic Sentinel — Schematic

## Block Diagram

```
┌─────────────────────────────────────────────────┐
│           ESP32-S3-WROOM-1-N8R2                  │
│                                                 │
│  ┌─────────┐  ┌────────┐  ┌─────────────────┐  │
│  │ 240MHz  │  │Vector  │  │  TFLite-Micro   │  │
│  │ dualcore │  │instr.  │  │  WingNet CNN   │  │
│  │         │  │(CNN)   │  │  int8 140KB    │  │
│  └─────────┘  └────────┘  └─────────────────┘  │
│                                                 │
│  GPIO4  ── SX1262 DIO1                         │
│  GPIO5  ── SX1262 BUSY                         │
│  GPIO6  ── SX1262 NSS                          │
│  GPIO7  ── SX1262 RST                          │
│  GPIO8  ── SX1262 SCK                          │
│  GPIO9  ── SX1262 MISO                         │
│  GPIO10 ── SX1262 MOSI                         │
│                                                 │
│  GPIO11 ── I²S BCLK ──┬── Mic 1 BCLK           │
│                        ├── Mic 2 BCLK           │
│                        ├── Mic 3 BCLK           │
│                        └── Mic 4 BCLK           │
│  GPIO12 ── I²S LRCLK ─┬── Mic 1 LRCLK          │
│                        ├── Mic 2 LRCLK          │
│                        ├── Mic 3 LRCLK          │
│                        └── Mic 4 LRCLK          │
│  GPIO13 ── I²S DATA ── Mic 1 DATA (ch0)        │
│                                                 │
│  GPIO14 ── SHT40 SDA (I²C)                     │
│  GPIO15 ── SHT40 SCL (I²C)                     │
│  GPIO16 ── Battery voltage (ADC)               │
│  GPIO17 ── SK6812 LED                          │
│  GPIO18 ── USB power detect                    │
│  GPIO19 ── Mic enable (MOSFET gate)           │
└─────────────────────────────────────────────────┘
```

## Microphone Array (4× ICS-43434)

### I²S MEMS Microphone
- Part: ICS-43434 (TDK InvenSense)
- Interface: I²S, 16-bit, up to 50 kHz
- SNR: 65 dB SNR, sensitivity -26 dBFS
- Frequency response: 50 Hz–20 kHz (flat)
- Captures mosquito wingbeat: 300–700 Hz

### 4-Mic Array Configuration
- 4 mics arranged in square (40mm spacing) for beamforming
- TDM mode: all mics share BCLK + LRCLK, separate DATA pins
- ESP32-S3 I²S peripheral: 16 kHz, 16-bit, left channel
- Production: delay-and-sum beamforming for directional detection

### Power Gating
- GPIO19 → MOSFET gate → mic array VDD
- Cuts mic power during sleep to save battery
- 4.7k pull-down on MOSFET gate (default off)

## SHT40 Temperature/Humidity
- I²C address 0x44
- ±0.2°C temperature accuracy
- ±1.8% RH humidity accuracy
- Power: 3.3V (always on, 0.2µA standby)

## Power Management
- USB-C 5V input (primary) OR solar + LiPo
- MCP73871: LiPo charger (3.7V 1200 mAh)
- AMS1117-3.3: 5V → 3.3V LDO
- Solar: 3W 5V panel (indoor light harvesting)
- Battery ADC: voltage divider 2:1 → GPIO16

## WingNet CNN (On-Device)
- TFLite-Micro int8 quantized model (~140 KB)
- Stored in flash partition
- Input: 64×32 mel-spectrogram (1s audio @ 16 kHz)
- Inference: <200 ms on ESP32-S3 (vector instructions)
- Output: 8-class softmax (7 mosquito species + non-mosquito)

## PCB Layout Notes
- 4-layer board (40×40 mm)
- Mic array: acoustic ports on PCB, gasket to enclosure
- Keep I²S traces short and equal-length
- Audio ground: separate from digital ground, star topology
- Enclosure: acoustically transparent mesh (3D-printed ASA)