# SoundNest Hub Node Schematic

## ESP32-S3 + nRF52840 Dual-MCU Architecture

### Power Supply
- USB-C 5V input → TP4056 Li-Po charger → 2000mAh Li-Po backup
- MP28167 buck converter: 5V → 3.3V @ 2A (main rail)
- AP2112K-3.3 LDO: 3.3V → 3.3V @ 600mA (sensor rail, low-noise)
- Battery voltage divider on GPIO18 for ADC monitoring

### ESP32-S3 Connections
| Peripheral | Pins | Interface |
|-----------|------|-----------|
| ES8388 Codec | GPIO1-4 (I2S), GPIO12-13 (I2C) | I2S + I2C |
| ILI9488 TFT | GPIO5-9 (SPI), GPIO10 (DC), GPIO11 (BL) | SPI |
| SPH0645 Mic | GPIO35 (I2S), GPIO14 (EN) | I2S |
| MicroSD | GPIO5-8 (SPI), GPIO8 (CS) | SPI |
| DS3231 RTC | GPIO12-13 (I2C) | I2C |
| nRF52840 | GPIO44-45 (UART), GPIO46 (RST) | UART |
| WS2812B LEDs | GPIO16 | GPIO bitbang |
| Piezo Buzzer | GPIO15 (PWM) | PWM |
| Speaker Enable | GPIO14 | GPIO |
| USB Sense | GPIO17 | ADC |

### nRF52840 Connections
| Peripheral | Pins | Interface |
|-----------|------|-----------|
| SX1262 Radio | P0.02-08 (SPI + IRQ) | SPI |
| ESP32-S3 | P0.09-10 (UART), P0.20 (RST) | UART |
| Qwiic Expansion | P0.18-19 (I2C) | I2C |
| Status LEDs | P0.13-14 | GPIO |
| DFU Button | P0.15 | GPIO |

### I2C Address Map
| Device | Address | Notes |
|--------|---------|-------|
| ES8388 Codec | 0x10 | Audio ADC/DAC |
| DS3231 RTC | 0x68 | Real-time clock |
| Qwiic Port 1 | 0x00-0x77 | Expansion |
| Qwiic Port 2 | 0x00-0x77 | Expansion |

### Power Budget
| Rail | Voltage | Current | Source |
|------|---------|---------|--------|
| VMAIN | 3.3V | 500mA | MP28167 |
| VSENSOR | 3.3V | 200mA | AP2112K |
| VRTC | 3.0V | 1µA | DS3231 battery |
| VBAT | 3.7V | Varies | Li-Po |

### KiCad Project Files
- `hub-node.kicad_pro` — Project file
- `hub-node.kicad_sch` — Schematic
- `hub-node.kicad_pcb` — PCB layout