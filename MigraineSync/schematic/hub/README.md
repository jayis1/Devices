# MigraineSync — Hub Schematic

## SoC: ESP32-S3-WROOM-1-N16R8

### Power
- USB-C 5V → 3.3V LDO (AMS1117-3.3) → ESP32-S3 + peripherals
- CR2032 backup → DS3231 RTC (I²C)
- Optional 18650 via TP4056 → 3.3V LDO

### SPI Bus 1 (VSPI): TFT + microSD
| Signal | GPIO |
|--------|------|
| CLK    | 4    |
| MOSI   | 5    |
| MISO   | 6    |
| TFT_CS | 7    |
| SD_CS  | 10   |
| TFT_DC | 11   |
| TFT_RST| 14   |

### SPI Bus 2 (HSPI): SX1262
| Signal   | GPIO |
|----------|------|
| SCK      | 3    |
| MOSI     | 8    |
| MISO     | 48   |
| CS       | 15   |
| DIO1 IRQ | 16   |
| BUSY     | 17   |
| RESET    | 18   |

### GPIO
| Function  | GPIO |
|-----------|------|
| BTN_ACK   | 1    |
| BTN_SILENCE| 2   |
| BTN_PAIR  | 41   |
| RGB_LED   | 38   |
| BUZZER    | 42   |

### I²C
| Signal | GPIO |
|--------|------|
| SCL    | (N/A — no I²C peripherals on hub) |
| SDA    | |

### KiCad Project
- `hub.kicad_pro` — KiCad 7+ project
- `hub.kicad_sch` — main schematic
- `hub.kicad_pcb` — 4-layer PCB (80×50 mm)
- `hub.kicad_prl` — project settings

### PCB Stackup
- Layer 1: Signal + components
- Layer 2: GND plane
- Layer 3: 3.3V power plane
- Layer 4: Signal + routing

### RF Section
- SX1262 RF trace: 50Ω microstrip on 1.6mm FR4
- TCXO 32 MHz → SX1262
- 868 MHz chip antenna with pi-network matching