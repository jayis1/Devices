# MigraineSync — Pin Map Reference

## Hub (ESP32-S3-WROOM-1-N16R8)

| GPIO | Function | Peripheral | Notes |
|------|----------|------------|-------|
| 1 | Button: ACK | Tactile switch | Active LOW, 10k pull-up |
| 2 | Button: Silence | Tactile switch | Active LOW, 10k pull-up |
| 3 | SX1262 SCK | SX1262 (HSPI) | |
| 4 | SPI CLK | ILI9341 + microSD | Shared SPI bus |
| 5 | SPI MOSI | ILI9341 + microSD | |
| 6 | SPI MISO | ILI9341 + microSD | |
| 7 | SPI CS (TFT) | ILI9341 | Active LOW |
| 8 | SX1262 MOSI | SX1262 (HSPI) | |
| 10 | SPI CS (SD) | microSD | Active LOW |
| 11 | D/C (TFT) | ILI9341 | |
| 14 | RESET (TFT) | ILI9341 | |
| 15 | SX1262 CS | SX1262 | Active LOW |
| 16 | SX1262 DIO1 | SX1262 | Interrupt |
| 17 | SX1262 BUSY | SX1262 | |
| 18 | SX1262 RESET | SX1262 | Active LOW |
| 38 | RGB LED | WS2812 | |
| 41 | Button: Pair | Tactile switch | Active LOW |
| 42 | Buzzer | PWM | |
| 46 | BOOT | Strapping pin | Flash mode |

## Env Sentinel (ESP32-S3-WROOM-1-N8R2)

| GPIO | Function | Peripheral | Notes |
|------|----------|------------|-------|
| 8 | I²C SCL | TCA9548A mux | Shared bus via mux |
| 9 | I²C SDA | TCA9548A mux | |
| 10 | SX1262 CS | SX1262 (VSPI) | |
| 11 | SX1262 DIO1 | SX1262 | Interrupt |
| 12 | SX1262 BUSY | SX1262 | |
| 13 | SX1262 RESET | SX1262 | |
| 14 | SX1262 SCK | SX1262 | |
| 15 | SX1262 MISO | SX1262 | |
| 16 | SX1262 MOSI | SX1262 | |
| 17 | Battery ADC | Voltage divider | Optional 18650 |
| 18 | Status LED | WS2812 | |

### I²C Mux Channels (TCA9548A at 0x70)

| Channel | Device(s) | Address(es) |
|---------|-----------|-------------|
| 0 | BMP390 barometer | 0x76 |
| 1 | SPL06-007 sound | 0x76 |
| 2 | VEML7700 light, SHT45 temp/rh | 0x10, 0x44 |
| 3 | BME688 VOC, SCD41 CO₂ | 0x77, 0x62 |

## Aura Band (nRF52840 QFAA)

| nRF Pin | Function | Peripheral | Notes |
|---------|----------|------------|-------|
| P0.06 | MAX30101 INT1 | PPG interrupt | |
| P0.07 | LSM6DSO INT1 | Accel interrupt | Activity + wake |
| P0.08 | I²C SDA | MAX30101, TMP117, BMP390, VEML7700, LSM6DSO | |
| P0.09 | I²C SCL | Shared | |
| P0.13 | LED (green) | Status indicator | |
| P0.15 | Button | Tactile switch | Mark prodrome/migraine |
| P0.16 | Vibrator | PWM (LR motor) | Haptic alert |
| P0.04 | Battery ADC | LiPo voltage divider | |
| VDD-nRF | TP4056 BAT | Charge IC | USB-C charging |

### I²C Bus Devices

| Device | Address | Notes |
|--------|---------|-------|
| MAX30101 | 0x57 | PPG: green/red/IR |
| TMP117 | 0x48 | Skin temperature |
| BMP390 | 0x76 | Barometric pressure |
| VEML7700 | 0x10 | Ambient light |
| LSM6DSO | 0x6A | 6-axis IMU |

## Hydrate Tag (nRF52840 QFAA)

| nRF Pin | Function | Peripheral | Notes |
|---------|----------|------------|-------|
| P0.04 | HX711 SCK | Load cell clock | Bit-bang GPIO |
| P0.05 | HX711 DOUT | Load cell data | Bit-bang GPIO |
| P0.06 | LSM6DSO INT1 | Tilt interrupt | Wake source |
| P0.08 | I²C SDA | LSM6DSO | |
| P0.09 | I²C SCL | LSM6DSO | |
| P0.11 | LED (blue) | Indicator | |
| P0.13 | Buzzer | PWM | |
| P0.15 | Button | Tactile switch | Manual intake mark |
| P0.16 | HX711 RATE | Sample rate | GND = 10 Hz |