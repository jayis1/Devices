# Mirror Hub Schematic — SkinSync

## MCU Architecture

Dual-MCU: **RP2040** (main compute + edge ML + display) + **ESP32-C6** (WiFi6/BLE5.3 + cloud bridge) + **nRF52832 + SX1262** (Sub-GHz mesh radio to UV patches + smart dispenser).

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │              MIRROR HUB                      │
                    │                                              │
  USB-C 5V ──────►  │  TP4056 ── LiPo 2500mAh ── 3.3V LDO         │
                    │                                              │
  RP2040 ─────────  │  GPIO/SPI/I2C/UART  ─── all peripherals     │
                    │  ├── SPI0  ── ILI9488 5.0" TFT (mirror)      │
                    │  ├── I2C0 ── PCF8563 RTC + BME280             │
                    │  ├── UART0 ── nRF52832+SX1262 (mesh) @1Mbaud │
                    │  ├── UART1 ── ESP32-C6 (WiFi) @921600        │
                    │  ├── I2S   ── MAX98357A + 28mm speaker       │
                    │  └── GPIO  ── WS2812 RGB + 4× status LEDs    │
                    │                                              │
  nRF52832 ───────  │  Sub-GHz mesh to UV patches + dispenser      │
  + SX1262 ──────  │  868/915 MHz, +10 dBm, 50m indoor range       │
                    │  UART to RP2040                              │
                    │                                              │
  ESP32-C6 ───────  │  WiFi6 to cloud (MQTT/TLS)                   │
                    │  BLE5.3 to mobile app                        │
                    │  UART to RP2040                              │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — RP2040

| Pin | Function | Notes |
|-----|----------|-------|
| GP0  | UART0 TX | → nRF52832 RX (mesh) |
| GP1  | UART0 RX | ← nRF52832 TX |
| GP2  | I2S BCK  | → MAX98357A |
| GP3  | I2S WS   | → MAX98357A |
| GP4  | UART1 TX | → ESP32-C6 RX (WiFi) |
| GP5  | UART1 RX | ← ESP32-C6 TX |
| GP6  | I2S DATA | → MAX98357A DIN |
| GP7  | TFT DC   | ILI9488 data/command |
| GP8  | TFT CS   | SPI0 CS for TFT |
| GP9  | SPI0 SCK | shared SPI0 |
| GP10 | SPI0 MOSI| shared SPI0 |
| GP11 | SPI0 MISO| shared SPI0 (SD card) |
| GP12 | SD CS    | MicroSD card select |
| GP13 | Flash CS | W25Q256 external flash |
| GP14 | I2C0 SDA | PCF8563 RTC + BME280 |
| GP15 | I2C0 SCL | PCF8563 RTC + BME280 |
| GP16 | WS2812   | RGB status LED |
| GP17-20 | GPIO | 4× status LEDs (green/amber/red/blue) |
| GP21 | nRF BOOT | nRF52832 bootloader pin |
| GP26 | ADC0     | battery voltage divider |
| GP27 | ADC1     | hub temperature sensor |

## Power

- USB-C 5V → TP4056 → LiPo 2500mAh → RT9013 3.3V LDO
- Battery backup: ~6 hours of operation without mains
- nRF52832 + SX1262 draw ~20 mA TX, 5 mA RX (duty-cycled)

## Sub-GHz Mesh Details

- SX1262 configured at 868 MHz (EU) or 915 MHz (US)
- LoRa-like modulation: SF7, BW=125 kHz, +10 dBm
- Sync word: 0xSS (SkinSync proprietary)
- Mesh: patches relay neighbor packets (store-and-forward)
- Range: 50m indoor (concrete walls), 200m line-of-sight

## Physical Design

- PCB: 85×55mm 4-layer FR4
- Enclosure: 90×60×25mm ABS, mounts behind/alongside bathroom mirror
- TFT visible through two-way mirror film (semi-transparent when off)
- USB-C on bottom edge, status LEDs on front edge
- BME280 sensor exposed to bathroom air for humidity tracking