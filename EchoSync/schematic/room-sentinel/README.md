# Room Sentinel — Schematic

## Block Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                   ROOM SENTINEL (ESP32-S3)                   │
│                                                              │
│  ┌─────────────┐   ┌──────────────┐   ┌──────────────────┐  │
│  │ ESP32-S3    │   │ SX1262       │   │ 4× ICS-43434     │  │
│  │ WROOM-1     │   │ 868MHz LoRa  │   │ I²S MEMS Mics    │  │
│  │ N8R2        │   │              │   │ (Square 50mm)    │  │
│  │ 8MB Flash   │   │              │   │                  │  │
│  │ 2MB PSRAM   │   │              │   │  Mic0  Mic1      │  │
│  └──────┬──────┘   └──────┬───────┘   │   ┌──┐  ┌──┐     │  │
│         │                 │           │   │M0│  │M1│     │  │
│         │ SPI2            │ SPI2      │   └──┘  └──┘     │  │
│         │                 │           │   ┌──┐  ┌──┐     │  │
│  ┌──────┴──────┐          │           │   │M2│  │M3│     │  │
│  │ SHT40       │          │           │   └──┘  └──┘     │  │
│  │ T/H Sensor  │   ┌──────┴────┐      │  50mm spacing    │  │
│  │ I²C 0x44    │   │ 868MHz    │      └────────┬─────────┘  │
│  └─────────────┘   │ SMA Ant   │               │ I²S TDM    │
│                    └───────────┘               │            │
│  ┌─────────────┐                               │            │
│  │ SK6812 RGB │           ┌──────────────┐    │            │
│  │ Status LED │           │ Mic Enable   │    │            │
│  └─────────────┘           │ MOSFET Gate  │────┘            │
│                            └──────────────┘                 │
│                                                              │
│  ┌──────────────┐   ┌──────────────────┐                    │
│  │ USB-C 5V     │   │ TPS25940 eFuse   │                    │
│  │ (power only) │   │ AMS1117-3.3 LDO  │                    │
│  └──────────────┘   └──────────────────┘                    │
└──────────────────────────────────────────────────────────────┘
```

## Microphone Array

The 4-mic array uses **ICS-43434** I²S MEMS microphones arranged in a 50mm square:

```
    Mic0 ──── 50mm ──── Mic1
      │                    │
     50mm                 50mm
      │                    │
    Mic2 ──── 50mm ──── Mic3
```

- **TDM mode:** All 4 mics share a single I²S data line (TDM-4)
- **BCLK:** GPIO11 (shared bit clock)
- **LRCLK:** GPIO12 (shared word select)
- **DATA:** GPIO13 (combined data from all 4 mics)
- **Sample rate:** 16 kHz, 16-bit

The 50mm spacing enables **TDOA beamforming** for direction-of-arrival estimation with ±15° accuracy in the 100 Hz – 8 kHz range.

## Key ICs

| IC | Function | Package | Interface |
|----|----------|---------|-----------|
| ESP32-S3-WROOM-1-N8R2 | Main MCU, TFLite-Micro | Module | — |
| SX1262IMLTRT | Sub-GHz LoRa radio | QFN-16 | SPI |
| ICS-43434 ×4 | I²S MEMS microphones | LGA | I²S TDM |
| SHT40 | Temp/humidity sensor | DFN-4 | I²C 0x44 |
| TPS25940 | eFuse overcurrent protection | VSSOP-10 | GPIO |
| AMS1117-3.3 | 3.3V LDO | SOT-223 | — |
| DMG1012UVW | Mic power MOSFET gate | SOT-523 | GPIO |

## Power Architecture

- **Input:** USB-C 5V (power only, no data)
- **Regulation:** AMS1117-3.3 LDO → 3.3V
- **Mic power:** MOSFET-gated 3.3V (GPIO19 enables mics for power savings)
- **Consumption:** ~120 mA active (mics + inference), ~20 mA idle

## Enclosure

- **Material:** 3D-printed ASA (UV-resistant, durable)
- **Mount:** Ceiling or wall mount, acoustically transparent mesh
- **Mic openings:** 4× 3mm holes in square pattern, acoustic foam behind
- **Cable:** USB-C passthrough, strain relief