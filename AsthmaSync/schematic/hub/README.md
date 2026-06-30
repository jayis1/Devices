# AsthmaSync — Hub Schematic (KiCad)
# ESP32-S3-WROOM-1-N16R8 + SX1262 + ILI9341 + USB-C

This folder contains the KiCad schematic project for the AsthmaSync Hub.

## Components
- **U1**: ESP32-S3-WROOM-1-N16R8 (16MB flash, 8MB PSRAM)
- **U2**: SX1262IMLTRT (Sub-GHz transceiver, 868 MHz)
- **U3**: ILI9341 2.4" TFT display (SPI)
- **U4**: microSD card slot (SPI)
- **U5**: WS2812B RGB LED
- **U6**: TP4056 USB-C Li-ion charger (backup battery)
- **U7**: HLK-5M05 AC-DC 5V power supply
- **SW1-SW3**: Tactile buttons (ACK, Silence, Pair)
- **LS1**: Piezo buzzer
- **BT1**: CR2032 RTC backup battery

## Power Architecture
- **Main**: 5V from HLK-5M05 (AC mains → 5V/5W)
- **Backup**: 18650 Li-ion via TP4056 (USB-C charging)
- **RTC**: CR2032 for timekeeping during power outage
- **3.3V**: LDO from 5V rail (AMS1117-3.3, 800mA)

## Block Diagram
```
AC Mains → [HLK-5M05 5V] → [AMS1117-3.3V] → ESP32-S3
                                    ↓
USB-C → [TP4056] → 18650 Li-ion → [boost 5V] → (backup rail)
                                                    ↓
CR2032 → RTC backup ──────────────────────────── ESP32-S3 VBAT
```

## SPI Bus Assignment
- **VSPI** (GPIO 4/5/6): ILI9341 display (CS=GPIO7) + microSD (CS=GPIO10)
- **HSPI** (GPIO 3/8/48): SX1262 Sub-GHz radio (CS=GPIO15)

## Notes
- The KiCad `.kicad_pro`, `.kicad_sch`, and `.kicad_pcb` files need to be
  created in KiCad 7+ using the pin assignments from `docs/PIN_MAP.md`.
- ESP32-S3 footprint: standard WROOM-1 SMD module
- SX1262 footprint: QFN-16 (4×4mm)
- PCB: 4-layer, 100×80mm, FR4