# AsthmaSync — Pin Map

## Hub (ESP32-S3-WROOM-1-N16R8)

| GPIO | Function | Peripheral | Notes |
|------|----------|------------|-------|
| 1 | Button: ACK | Tactile switch | Active LOW, pull-up |
| 2 | Button: Silence | Tactile switch | Active LOW, pull-up |
| 3 | SX1262 SCK | SX1262 (HSPI) | Sub-GHz radio SPI clock |
| 4 | SPI CLK | ILI9341 + SD card | Display + storage SPI bus |
| 5 | SPI MOSI | ILI9341 + SD card | Display + storage SPI MOSI |
| 6 | SPI MISO | ILI9341 + SD card | Display + storage SPI MISO |
| 7 | SPI CS (TFT) | ILI9341 | Display chip select |
| 8 | SX1262 MOSI | SX1262 (HSPI) | Sub-GHz radio SPI MOSI |
| 10 | SPI CS (SD) | microSD | SD card chip select |
| 11 | D/C (TFT) | ILI9341 | Display data/command |
| 14 | RESET (TFT) | ILI9341 | Display reset |
| 15 | SX1262 CS | SX1262 | Sub-GHz radio chip select |
| 16 | SX1262 DIO1 | SX1262 | Radio interrupt (IRQ) |
| 17 | SX1262 BUSY | SX1262 | Radio busy signal |
| 18 | SX1262 RESET | SX1262 | Radio reset (active LOW) |
| 38 | RGB LED | WS2812 | Status LED (RMT-driven) |
| 41 | Button: Pair | Tactile switch | Active LOW, pull-up |
| 42 | Buzzer | Piezo | PWM-driven audible alarm |
| 46 | BOOT | Strapping | Boot mode + flash button |

**SPI Buses:**
- **VSPI (GPIO 4/5/6)**: ILI9341 display + microSD card (shared, different CS)
- **HSPI (GPIO 3/8/48)**: SX1262 Sub-GHz radio

## Air Sentinel (ESP32-S3-WROOM-1-N8R2)

| GPIO | Function | Peripheral | Notes |
|------|----------|------------|-------|
| 8 | I²C SCL | PMSA003I, BME688, SGP40, SCD41 | Shared bus, 100 kHz |
| 9 | I²C SDA | Shared sensors | |
| 4 | PMSA003I SET | PMSA003I | Standby control (HIGH=normal) |
| 5 | PMSA003I RST | PMSA003I | Reset (active LOW) |
| 10 | SX1262 CS | SX1262 | Radio chip select |
| 11 | SX1262 DIO1 | SX1262 | Radio IRQ |
| 12 | SX1262 BUSY | SX1262 | Radio busy |
| 13 | SX1262 RESET | SX1262 | Radio reset |
| 14 | SX1262 SCK | SX1262 (VSPI) | Radio SPI clock |
| 15 | SX1262 MISO | SX1262 | Radio SPI MISO |
| 16 | SX1262 MOSI | SX1262 | Radio SPI MOSI |
| 17 | Battery ADC | Voltage divider | Battery voltage monitor |
| 18 | Status LED | WS2812 | Node status indicator |

**I²C Devices (shared bus):**
| Address | Device | Notes |
|---------|--------|-------|
| 0x12 | PMSA003I | PM sensor (custom I²C protocol) |
| 0x62 | SCD41 | CO₂ sensor (needs ≤100 kHz) |
| 0x59 | SGP40 | VOC sensor |
| 0x77 | BME688 | Environmental sensor |

## Inhaler Tag (nRF52840 QFAA)

| nRF Pin | GPIO | Function | Peripheral | Notes |
|---------|------|----------|------------|-------|
| P0.04 | — | I²C SDA | LSM6DSO | TWI bus |
| P0.05 | — | I²C SCL | LSM6DSO | TWI bus |
| P0.06 | — | LSM6DSO INT1 | Accelerometer | Wake-up interrupt |
| P0.07 | — | LSM6DSO INT2 | Accelerometer | Free-fall / activity |
| P0.08 | — | Button | Tactile switch | Long-press = dose confirm |
| P0.11 | — | LED (blue) | Indicator | Status LED |
| P0.13 | — | Buzzer | Piezo | PWM-driven |
| P0.15 | — | Battery ADC | Divider | LiPo voltage (optional) |
| P0.18 | — | NFC-A | NFC antenna | Optional tap-to-log |
| P0.19 | — | NFC-B | NFC antenna | |

**PCB Form Factor:** 18mm diameter circular disc

## Wheeze Band (nRF52840 QFAA)

| nRF Pin | GPIO | Function | Peripheral | Notes |
|---------|------|----------|------------|-------|
| P0.04 | — | Battery ADC | Divider | LiPo voltage monitor |
| P0.06 | — | MAX30101 INT1 | PPG sensor | Data ready interrupt |
| P0.07 | — | LSM6DSO INT1 | IMU | Activity interrupt |
| P0.08 | — | I²S SCK | SPH0645 mic | Mic serial clock |
| P0.09 | — | I²S WS | SPH0645 mic | Mic word select (LRCLK) |
| P0.10 | — | I²S SD | SPH0645 mic | Mic serial data (input) |
| P0.11 | — | I²S MCK | SPH0645 mic | Master clock |
| P0.13 | — | LED (green) | Indicator | Status LED |
| P0.15 | — | Button | Tactile switch | Mark event / SOS |
| P0.16 | — | Vibrator | LRA motor | Haptic alert (PWM) |
| P0.26 | — | I²C SDA | MAX30101, TMP117, LSM6DSO | TWI bus |
| P0.27 | — | I²C SCL | Shared sensors | TWI bus |

**I²C Devices (shared bus):**
| Address | Device | Notes |
|---------|--------|-------|
| 0x57 | MAX30101 | PPG sensor (HR/HRV/SpO₂) |
| 0x48 | TMP117 | Skin temperature (±0.1°C) |
| 0x6A | LSM6DSO | 6-axis IMU |

**I²S Device:**
| Device | Interface | Notes |
|--------|-----------|-------|
| SPH0645LM4H-B | I²S (24-bit) | MEMS microphone, 65 dB SNR |