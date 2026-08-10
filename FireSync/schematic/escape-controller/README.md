# Escape Controller — Schematic

## MCU: ESP32-S3-WROOM-1-N8R2

- 8 MB flash, 2 MB PSRAM

## Sub-GHz Radio: SX1262

- 868 MHz LoRa, +22 dBm, TDMA mesh node
- SPI (MOSI=GPIO15, MISO=GPIO16, SCK=GPIO17, NSS=GPIO18)
- DIO1 (GPIO19), RST (GPIO20), BUSY (GPIO21)

## WS2812B LED Strips (4 zones)

| Strip | Zone | Pin | Length |
|-------|------|-----|--------|
| 1 | Front exit path | GPIO4 | 60 LEDs |
| 2 | Back exit path | GPIO5 | 60 LEDs |
| 3 | Hallway | GPIO6 | 60 LEDs |
| 4 | Stairwell | GPIO7 | 60 LEDs |

- SN74AHCT125N level shifter (3.3V → 5V data signal)
- Green = safe exit path, Red = fire zone, Amber = warning, Off = inactive
- RMT peripheral for precise WS2812B timing (800 kHz)

## Audio (Voice Guidance)

| Component | Interface | Pins |
|-----------|-----------|------|
| W25Q128 SPI flash (16 MB) | SPI | GPIO11 (CS), GPIO12 (SCK), GPIO13 (MOSI), GPIO14 (MISO) |
| MAX98357A I²S amp | I²S | GPIO8 (BCLK), GPIO9 (LRCLK), GPIO10 (DATA) |
| Speaker 4Ω 3W | — | Connected to MAX98357A output |

- 96 voice clips: 8 languages × 12 messages (~8 MB compressed)
- 16 kHz sample rate, 16-bit mono, ADPCM compressed

## Door Release Relays (4 doors)

| Door | Pin | Notes |
|------|-----|-------|
| Front door | GPIO22 | Relay 1 (fail-open door strike) |
| Back door | GPIO23 | Relay 2 |
| Garage door | GPIO24 | Relay 3 |
| Bedroom door | GPIO25 | Relay 4 |

- Relays energize to release door strikes (unlock doors)
- Fail-open design: doors unlock on power loss

## Power

- USB-C 5V wall → TPS25940 eFuse → AP2112K-3.3 LDO
- **LiFePO4 3.2V 5000 mAh** (fire-safe chemistry — no thermal runaway)
- TP5000 LiFePO4 charge controller
- 48+ hours backup (idle), 12+ hours (active LEDs + voice)
- Battery voltage on GPIO26 (ADC)

## Why LiFePO4?

LiFePO4 (Lithium Iron Phosphate) is chosen over standard LiPo because:
1. **Chemically stable** — will not thermal-runaway even if punctured/heated
2. **Fire-safe** — critical for a device that must operate DURING a fire
3. **Long cycle life** — 2000+ cycles vs 500 for LiPo
4. **Higher safety margin** — 3.2V nominal, stable discharge curve

## KiCad Project

1. `escape MCU.sch` — ESP32-S3 + decoupling
2. `escape SubGHz.sch` — SX1262 + PCB antenna
3. `escape LEDs.sch` — WS2812B ×4 + SN74AHCT125N level shifter
4. `escape Audio.sch` — W25Q128 + MAX98357A + speaker
5. `escape Doors.sch` — 4× relay + door strike connectors
6. `escape Power.sch` — USB-C, TP5000, LiFePO4, LDO