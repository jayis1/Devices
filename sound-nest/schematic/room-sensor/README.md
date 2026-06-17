# SoundNest Room Acoustic Sensor Schematic

## nRF52840 + 4-Mic Array

### Microphone Array
4× SPH0645LM4H-6 MEMS microphones in square pattern, 40mm spacing.
All share BCLK (P0.02) and LRCLK (P0.03), individual data outputs.

### Power Supply
- CR123A ×2 (6V) → AP2112K-3.3 LDO → 3.3V @ 600mA
- OR USB-C 5V → AP2112K-3.3 → 3.3V
- Battery voltage divider on P0.19 for ADC monitoring
- Power gating on P0.22 to disable mics in sleep mode

### nRF52840 Pin Connections
| Peripheral | Pins | Interface |
|-----------|------|-----------|
| MIC1 (I2S) | P0.02 (BCLK), P0.03 (LRCLK), P0.04 (DOUT) | I2S |
| MIC2 (I2S) | P0.05 (DOUT) | I2S shared |
| MIC3 (I2S) | P0.06 (DOUT) | I2S shared |
| MIC4 (I2S) | P0.07 (DOUT) | I2S shared |
| SX1262 Radio | P0.11-17 (SPI + control) | SPI |
| SHT40 Temp/Hum | P0.08-09 (I2C) | I2C |
| VEML7700 Light | P0.08-09 (I2C) | I2C shared |
| AM312 PIR | P0.10 | GPIO interrupt |
| WS2812B LED | P0.18 | GPIO bitbang |
| Battery ADC | P0.19 | ADC |
| USB Detect | P0.20 | GPIO |
| Mic Power EN | P0.22 | GPIO (power gating) |
| Setup Button | P0.23 | GPIO |
| Debug UART | P0.25 (TX) | UART |

### I2C Address Map
| Device | Address | Notes |
|--------|---------|-------|
| SHT40-AD1B | 0x44 | Temp/humidity |
| VEML7700 | 0x10 | Ambient light |

### Power Budget (Battery)
| Mode | Current | Battery Life |
|------|---------|-------------|
| Active (16kHz sampling) | 15mA | ~160h |
| Listening (1kHz sampling) | 5mA | ~480h |
| Sleep (10s wake) | 50µA | ~6 months |
| Deep sleep (Sub-GHz off) | 5µA | ~2 years |

### KiCad Project Files
- `room-sensor.kicad_pro`
- `room-sensor.kicad_sch`
- `room-sensor.kicad_pcb`