# MosquitoSync — Schematic Overview

Each node has its own KiCad schematic project. The schematics are described
in README files within each subfolder, detailing:

- Block diagram
- Component list with part numbers
- Pin assignments
- Power architecture
- Bus connections (SPI, I²C, I²S, UART, ADC)

## Schematic Projects

| Folder | Node | MCU | Key Components |
|--------|------|-----|----------------|
| `hub/` | Hub/Gateway | ESP32-S3 | SX1262, SIM7000, BME280, DS3231, SD |
| `acoustic-sentinel/` | Acoustic Sentinel | ESP32-S3 | SX1262, 4× ICS-43434 I²S mics, SHT40 |
| `co2-trap/` | CO2 Trap Node | ESP32-S3 | SX1262, OV2640, BME280, propane valve, fan, PTC heater |
| `window-barrier/` | Window Barrier | ESP32 | SX1262, DRV8833, N20 motor, reed switches |
| `weather-sentinel/` | Weather Sentinel | nRF52840 | SX1262, BME280, Davis 6410, rain gauge |

## Common Subcircuits

### SX1262 Sub-GHz Radio (all nodes)
- SPI: MOSI, MISO, SCK, NSS
- Control: DIO1 (IRQ), RST, BUSY
- Antenna: 868 MHz whip (SMA)
- Decoupling: 100nF + 10µF on VDD

### Power Management
- ESP32 nodes: AMS1117-3.3 LDO from 5V USB/solar
- nRF52 node: MCP73871 solar charger + LiFePO4
- Solar nodes: MCP73871 buck-boost charger

### I²C Bus
- Hub: BME280 (0x76) + DS3231 (0x68)
- Acoustic: SHT40 (0x44)
- Trap: BME280 (0x76)
- Weather: BME280 (0x76)