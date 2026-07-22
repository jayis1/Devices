# Room Sentinel — Schematic

## Block Diagram

```
┌─────────────────────────────────────────────┐
│              ESP32-S3-WROOM-1-N8R2           │
│                                             │
│  GPIO4  ── SX1262 DIO1 (IRQ)               │
│  GPIO5  ── SX1262 BUSY                     │
│  GPIO6  ── SX1262 NSS (SPI CS)             │
│  GPIO7  ── SX1262 RST                      │
│  GPIO8  ── SX1262 SCK (SPI)                │
│  GPIO9  ── SX1262 MISO                     │
│  GPIO10 ── SX1262 MOSI                     │
│  GPIO11 ── I²S mic BCLK (4-mic array)      │
│  GPIO12 ── I²S mic LRCLK                   │
│  GPIO13 ── I²S mic DATA                    │
│  GPIO14 ── SHT40 SDA (I²C)                 │
│  GPIO15 ── SHT40 SCL                       │
│  GPIO16 ── SGP40 SDA (I²C shared)          │
│  GPIO17 ── SGP40 SCL                       │
│  GPIO18 ── SK6812 LED                     │
│  GPIO19 ── Mic enable (MOSFET gate)        │
│  GPIO43 ── USB TX                          │
│  GPIO44 ── USB RX                          │
└─────────────────────────────────────────────┘
```

## Subcircuits

### I²S 4-Microphone Array
- 4× **ICS-43434** I²S MEMS microphones
  - 26 dB SNR, flat response 50 Hz – 20 kHz
  - TDM mode: 4 channels on single I²S data line
  - BCLK=GPIO11, LRCLK=GPIO12, DATA=GPIO13
  - Captures ambient voice at 16 kHz, 16-bit
  - Used for VoiceNet CNN voice quality classification
- Power gated via GPIO19 (mic enable MOSFET)

### SX1262 Sub-GHz Radio
- SPI2 host bus at 8 MHz
- 868 MHz whip antenna via SMA connector
- Same as hub configuration

### SHT40 (Temp/Humidity)
- I²C address: 0x44 (GPIO14/GPIO15)
- ±0.2°C, ±1.8% RH
- Monitors room dryness (RH <40% damages vocal cords)

### SGP40 (VOC Sensor)
- I²C address: 0x59 (GPIO16/GPIO17, shared bus)
- VOC index 0–500
- Air quality affects voice (irritants → vocal fold inflammation)
- 1.8V reference, requires 3.3V→1.8V level shifter

### Power
- USB-C 5V continuous power
- AMS1117-3.3 LDO
- No battery needed (always-plugged)

### Status LED
- SK6812 RGB on GPIO18
- Green: normal, Yellow: voice quality degraded, Red: critical

## PCB Layout Notes
- 4-layer board (40×40 mm)
- Microphone array: 4 mics in square configuration, 30 mm spacing
- Acoustically transparent enclosure
- RF section: away from audio section
- SGP40: air flow access, not enclosed