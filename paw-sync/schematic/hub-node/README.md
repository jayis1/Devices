# Hub Node Schematic — PawSync

## MCU Architecture

Dual-MCU: **RP2040** (main compute + ML + display) + **ESP32-C6** (WiFi6/BLE5.3 + cloud bridge) + **nRF52840** (BLE mesh radio to collar + feeder).

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
  nRF52840 ───────  │  BLE 5.3 mesh to collar tag + feeder         │
                    │  UART to RP2040                              │
                    │                                              │
  ESP32-C6 ───────  │  WiFi6 to cloud (MQTT/TLS)                   │
                    │  BLE5.3 to mobile app                        │
                    │  UART to RP2040                              │
                    │                                              │
  Qi TX coil ─────  │  5W wireless charging pad for collar tag      │
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

## Pin Assignments — nRF52840

| Pin | Function |
|-----|----------|
| P0.06 | UART RX ← RP2040 TX |
| P0.08 | UART TX → RP2040 RX |
| P0.09 | BLE antenna (internal) |
| P0.15 | nRF BOOT (from RP2040 GP22) |
| P0.20 | Qi TX status (read) |

## Pin Assignments — ESP32-C6

| Pin | Function |
|-----|----------|
| GPIO5  | UART RX ← RP2040 TX |
| GPIO6  | UART TX → RP2040 RX |
| GPIO2  | WiFi antenna (internal) |
| GPIO8  | BLE antenna (internal) |
| GPIO3  | Boot/EN |

## Power

- USB-C 5V → TP4056 charger → LiPo 2500mAh → 3.3V LDO (RT9013) → all logic
- Qi transmitter (BQ500212A + coil) powered from 5V rail, controlled by RP2040 GP21
- Battery voltage divider on GP26 (ADC) for fuel gauge

## KiCad Project

`schematic/hub-node/hub-node.kicad_sch` — main schematic
`schematic/hub-node/hub-node.kicad_pcb` — 4-layer PCB (RP2040 + ESP32-C6 + nRF52840)