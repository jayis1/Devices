# Hub Node Schematic — CalmGrid

## MCU Architecture

Dual-MCU: **RP2040** (main compute + ML + display) + **ESP32-C6** (WiFi6/BLE5.3 + cloud bridge) + **nRF52840** (BLE mesh radio to wrist band + light node).

## Block Diagram

```
                    ┌────────────────────────────────────────────┐
                    │              HUB NODE                        │
                    │                                              │
  USB-C 5V ──────►  │  TP4056 ── LiPo 2500mAh ── 3.3V LDO         │
                    │                                              │
  RP2040 ─────────  │  GPIO/SPI/I2C/UART  ─── all peripherals     │
                    │  ├── SPI0  ── ILI9488 3.5" TFT               │
                    │  ├── I2C0 ── PCF8563 RTC                     │
                    │  ├── UART0 ── nRF52840 (mesh radio) @1Mbaud  │
                    │  ├── UART1 ── ESP32-C6 (WiFi) @921600        │
                    │  ├── I2S   ── MAX98357A + 28mm speaker       │
                    │  └── GPIO  ── WS2812 RGB + 4× status LEDs    │
                    │                                              │
  nRF52840 ───────  │  BLE 5.3 mesh to wrist band + light node     │
                    │  UART to RP2040                              │
                    │                                              │
  ESP32-C6 ───────  │  WiFi6 to cloud (MQTT/TLS)                   │
                    │  BLE5.3 to mobile app                        │
                    │  UART to RP2040                              │
                    │                                              │
  Qi TX coil ─────  │  5W wireless charging pad for wrist band      │
                    └────────────────────────────────────────────┘
```

## Pin Assignments — RP2040

| Pin | Function | Notes |
|-----|----------|-------|
| GP0  | UART0 TX | → nRF52840 RX (mesh) |
| GP1  | UART0 RX | ← nRF52840 TX |
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
| GP14 | I2C0 SDA | PCF8563 RTC |
| GP15 | I2C0 SCL | PCF8563 RTC |
| GP16 | WS2812   | RGB status LED |
| GP17-20 | GPIO | 4× status LEDs (green/amber/red/blue) |
| GP21 | Qi TX EN | enable Qi charging transmitter |
| GP22 | nRF BOOT | nRF52840 bootloader pin |
| GP26 | ADC0     | battery voltage divider |
| GP27 | ADC1     | hub temperature sensor |

## Power

- USB-C 5V → TP4056 → LiPo 2500mAh → RT9013 3.3V LDO
- Qi transmitter (BQ500212A + coil) for wrist band charging
- Battery backup: ~6 hours of operation without mains

## Physical Design

- PCB: 85×55mm 4-layer FR4
- Enclosure: 90×60×25mm ABS desktop/bedside enclosure
- TFT on front, speaker on bottom, Qi pad on top surface
- USB-C on back, status LEDs on front edge