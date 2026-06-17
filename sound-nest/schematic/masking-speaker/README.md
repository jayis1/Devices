# SoundNest Smart Masking Speaker Schematic

## ESP32-S3-MINI-1 + PCM5102A Hi-Fi Audio

### Audio Chain
```
ESP32-S3 I2S → PCM5102A DAC → MAX98306 Amp → 2× TDM-0303K0P Speakers
                                                                  ↓
SPH0645 Mic ← ← ← ← ← ← ← ← ← ← Room feedback ← ← ← ← ← ←
```

### Power Supply
- USB-C 5V → AP2112K-3.3 (3.3V/600mA for MCU+radio)
- USB-C 5V → SY8089 (1.8V/1A for DAC)
- USB-C 5V directly to MAX98306 amp (5V/2A)
- Power sequencing: MCU → DAC → AMP (delayed enable)

### ESP32-S3-MINI-1 Pin Connections
| Peripheral | Pins | Interface |
|-----------|------|-----------|
| PCM5102A DAC | GPIO1-3 (I2S BCLK/LRCLK/DOUT), GPIO4 (FMT), GPIO5 (XSMT) | I2S |
| SPH0645 Mic | GPIO6-8 (I2S BCLK/LRCLK/DIN) | I2S |
| MAX98306 Enable | GPIO9 (SD_MODE) | GPIO |
| SX1262 Radio | GPIO10-16 (SPI + control) | SPI |
| WS2812B LED | GPIO17 | GPIO bitbang |
| IR LED 1 | GPIO18 | GPIO |
| IR LED 2 | GPIO19 | GPIO |
| Mode Button | GPIO21 | GPIO |

### Audio Specifications
| Parameter | Value |
|-----------|-------|
| DAC Resolution | 32-bit |
| DAC Sample Rate | Up to 384kHz |
| Amp Output Power | 2× 3W @ 4Ω |
| Speaker Freq Range | 200Hz - 15kHz |
| Mic SNR | 65dB |
| Latency | < 50ms (I2S → DAC → Speaker) |

### Parabolic Enclosure
3D-printed ABS parabolic reflector behind speakers creates ±30° directional sound cone for privacy masking mode.

### KiCad Project Files
- `masking-speaker.kicad_pro`
- `masking-speaker.kicad_sch`
- `masking-speaker.kicad_pcb`