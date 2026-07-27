# GrillSync Schematics

KiCad schematic projects for each hardware node in the GrillSync system.

## Nodes

| Folder | Node | SoC | Description |
|--------|------|-----|-------------|
| `hub/` | Grill Hub | ESP32-S3 | Central gateway + display + gas shutoff relay |
| `grill-sentinel/` | Grill Sentinel | ESP32-S3 | MLX90640 thermal array + MQ-2 gas + flame + piezo |
| `meat-probe/` | Meat Probe | nRF52840 | 4× MAX31855 Type-K thermocouples + BLE 5.0 |
| `smoke-node/` | Smoke Node | ESP32-S3 | PMS5003 + BME680 + MQ-135 + UV flame |

Each schematic folder contains:
- `README.md` — Detailed schematic description with pin assignments and bus topology
- KiCad project files (in production: `.kicad_sch`, `.kicad_pcb`, `.kicad_pro`)

## Power Architecture

```
USB-C 5V ──→ TPS25940 eFuse ──→ AMS1117-3.3 LDO ──→ 3.3V (MCU, sensors)
                                               ──→ 3.3V (SX1262, BME280)

12V Barrel ──→ MP1584 Buck ──→ 5V ──→ AMS1117-3.3 ──→ 3.3V
           ──→ 12V (direct) ──→ Gas Shutoff Relay + Motorized Ball Valve
```

## Bus Topology

```
ESP32-S3:
  ├── SPI2: SX1262 (CS=GPIO6, SCK=GPIO8, MISO=GPIO9, MOSI=GPIO10)
  ├── SPI3: TFT Display (CS=GPIO21, SCK=GPIO19, MOSI=GPIO20)
  ├── I²C0: BME280 + DS3231 (SDA=GPIO11/13, SCL=GPIO12/14)
  ├── GPIO: LED Ring (WS2812B), Gas Relay, Buzzer
  └── UART0: USB-C debug

nRF52840:
  ├── SPI0: 4× MAX31855 (CS1=P0.02, CS2=P0.03, CS3=P0.04, CS4=P0.05)
  ├── GPIO: LED, Buttons, Charger status
  └── BLE 5.0: GATT service to Hub
```