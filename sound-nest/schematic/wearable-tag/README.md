# SoundNest Wearable Sound Tag Schematic

## nRF52832 Badge — Personal Sound Guardian

### Form Factor
45×30×12mm injection-molded PC badge, worn as clip or lanyard.

### Power Supply
- 100mAh Li-Po (3.7V) → AP2112K-3.3 → 3.3V/600mA
- USB-C charging via MCP73831 (4.2V CC/CV)
- Battery voltage divider on P0.12 for ADC monitoring
- 7-day typical life per charge

### nRF52832 Pin Connections
| Peripheral | Pins | Interface |
|-----------|------|-----------|
| SPH0645 Mic | P0.02-04 (I2S BCLK/LRCLK/DOUT) | I2S |
| LIS2DH12 Accel | P0.05-06 (I2C SDA/SCL) | I2C |
| LIS2DH12 INT | P0.07 (INT1) | GPIO interrupt |
| ERM Haptic | P0.09 (PWM), P0.16 (EN) | PWM + GPIO |
| APA106 LED | P0.10 | GPIO bitbang |
| MCP73831 CHG | P0.11 (STAT) | GPIO |
| Battery ADC | P0.12 | ADC |
| Mute Button | P0.13 | GPIO |
| Pair Button | P0.14 | GPIO |
| Mic Enable | P0.15 | GPIO |
| USB Detect | P0.19 | GPIO |
| Debug | P0.20-21 (SWD) | SWD |

### I2C Address Map
| Device | Address | Notes |
|--------|---------|-------|
| LIS2DH12 | 0x19 (SDO=1) | Accelerometer |

### Power Modes
| Mode | Current | Battery Life |
|------|---------|-------------|
| Active (16kHz mic) | 8mA | ~12h |
| Listening (1kHz SPL) | 2mA | ~50h |
| BLE connected (monitoring) | 0.5mA | ~8 days |
| Sleep (wake on event) | 50µA | ~80 days |

### Haptic Alert Patterns
| Priority | Pattern | Duration |
|----------|---------|----------|
| Info | 1× short | 50ms |
| Low | 1× medium | 100ms |
| Medium | 2× medium | 200ms total |
| High | 3× long | 500ms total |
| Critical | 1× very long | 1000ms |

### KiCad Project Files
- `wearable-tag.kicad_pro`
- `wearable-tag.kicad_sch`
- `wearable-tag.kicad_pcb`