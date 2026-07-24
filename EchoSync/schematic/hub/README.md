# Echo Hub — Schematic

## Block Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                     ECHO HUB (ESP32-S3)                         │
│                                                                  │
│  ┌─────────────┐   ┌──────────┐   ┌──────────┐   ┌────────────┐ │
│  │ ESP32-S3    │   │ SX1262   │   │ BME280   │   │ DS3231    │ │
│  │ WROOM-1     │   │ 868MHz   │   │ T/H/P    │   │ RTC       │ │
│  │ N16R8       │   │ LoRa     │   │ I²C      │   │ I²C       │ │
│  │ 16MB Flash  │   │ +22dBm   │   │ 0x76     │   │ 0x68      │ │
│  │ 8MB PSRAM   │   │          │   │          │   │           │ │
│  └──────┬──────┘   └────┬─────┘   └────┬─────┘   └─────┬─────┘ │
│         │ SPI2          │ SPI2          │ I²C            │ I²C  │
│         │               │               │                 │      │
│  ┌──────┴──────┐  ┌─────┴────┐  ┌───────┴──────┐  ┌─────┴────┐ │
│  │ microSD     │  │ 868MHz   │  │ E-ink 2.9"  │  │ Status   │ │
│  │ SPI         │  │ SMA Ant  │  │ UC8151D     │  │ LEDs×3   │ │
│  │             │  │          │  │ SPI         │  │ SK6812   │ │
│  └─────────────┘  └──────────┘  └─────────────┘  └──────────┘ │
│                                                                  │
│  ┌─────────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐ │
│  │ WS2812B    │  │ Bed      │  │ Buzzer   │  │ USB-C 5V    │ │
│  │ 8×8 Matrix │  │ Shaker   │  │ PWM      │  │ TPS25940    │ │
│  │             │  │ Relay    │  │          │  │ AMS1117-3.3│ │
│  └─────────────┘  └──────────┘  └──────────┘  └─────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

## Power Architecture

- **Input:** USB-C 5V via TPS25940 eFuse (overcurrent protection, 5V/2A)
- **Regulation:** AMS1117-3.3 LDO → 3.3V for all ICs
- **Bed Shaker Relay:** 5V directly from USB-C (relay is 5V)
- **Backup:** No battery (always powered); microSD for data buffering

## Key ICs

| IC | Function | Package | Interface |
|----|----------|---------|-----------|
| ESP32-S3-WROOM-1-N16R8 | Main MCU, Wi-Fi, BLE | Module | — |
| SX1262IMLTRT | Sub-GHz LoRa radio | QFN-16 | SPI |
| BME280 | Temp/humidity/pressure | LGA-8 | I²C 0x76 |
| DS3231SN | RTC (battery-backed) | SOIC-16 | I²C 0x68 |
| TPS25940 | eFuse overcurrent protection | VSSOP-10 | GPIO |
| AMS1117-3.3 | 3.3V LDO | SOT-223 | — |
| UC8151D | E-ink display controller | QFN | SPI |
| WS2812B | RGB LED matrix (8×8) | 5050 | GPIO (NRZ) |
| SRD-05VDC-SL-C | Bed-shaker relay | PCB | GPIO |

## Pin Assignments

See `firmware/common/config.h` for complete pin assignments.

### SPI buses:
- **SPI2_HOST:** SX1262 (MOSI=GPIO10, MISO=GPIO9, SCK=GPIO8, CS=GPIO6)
- **SPI3_HOST:** microSD (MOSI=GPIO15, MISO=GPIO16, SCK=GPIO17, CS=GPIO18)
- **SPI (shared):** E-ink (SCK=GPIO19, DIN=GPIO20, CS=GPIO21, DC=GPIO35, RST=GPIO36, BUSY=GPIO37)

### I²C buses:
- **I2C_NUM_0:** BME280 (SDA=GPIO11, SCL=GPIO12) + DS3231 (shared)

### GPIO:
- LED Matrix: GPIO38 (WS2812B NRZ)
- Bed Shaker: GPIO39 (relay, active high)
- Buzzer: GPIO40 (LEDC PWM)
- Status LEDs: GPIO41 (SK6812)