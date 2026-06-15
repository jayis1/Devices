# MedSync - Hub Node Schematic

## Overview

The hub node is the central coordinator for the MedSync system. It bridges BLE mesh to WiFi/cloud, runs local ML inference, and provides visual/audio reminders.

## Block Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    HUB NODE PCB                          │
│                                                          │
│  ┌──────────────┐    UART     ┌──────────────┐          │
│  │  nRF52840    │◄──────────►│  ESP32-S3     │          │
│  │  (BLE Mesh   │            │  (WiFi6 +     │          │
│  │  Coordinator)│            │   BLE Bridge) │          │
│  │              │            │               │          │
│  │  SPI ──────►│ ILI9488 TFT │               │          │
│  │  I2C ──────►│ DS3231 RTC  │    USB-C     │          │
│  │  I2C ──────►│ PN532 NFC   │◄─────────────│          │
│  │  I2S ──────►│ PCM5102A DAC│               │          │
│  │  I2S ◄──────│ SPH0645 Mic │               │          │
│  │  GPIO ─────►│ WS2812B LEDs│               │          │
│  │  GPIO ─────►│ Piezo Buzzer│               │          │
│  └──────┬───────┘            └───────┬───────┘          │
│         │                            │                   │
│         │  SPI (microSD)             │  USB-C            │
│         │  I2C (expansion)          │  WiFi/BLE         │
│         │                            │                   │
│  ┌──────┴───────┐            ┌───────┴───────┐          │
│  │  16MB Flash   │            │  WiFi/BLE     │          │
│  │  microSD      │            │  Antenna      │          │
│  └───────────────┘            └───────────────┘          │
│                                                          │
│  Power: USB-C 5V → MCP73831 → 18650 Lipo → AP2112-3.3V │
│         → AP6212-1.8V (ESP32-S3)                        │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

## Key Design Notes

1. **Dual MCU Architecture**: nRF52840 handles BLE mesh coordination, local ML, and all sensor/display I/O. ESP32-S3 handles WiFi uplink, MQTT, and camera interface. Communication via UART at 921600 baud.

2. **RTC Independence**: DS3231 has its own CR1220 coin cell backup, ensuring medication schedule timing survives all power failures.

3. **Audio**: PCM5102A I2S DAC drives a 3W speaker for voice reminders. TTS phrases are pre-loaded on microSD card.

4. **NFC**: PN532 NFC reader on top surface allows phone tap for pairing and dose confirmation.

5. **Power**: USB-C 5V primary power with 18650 Lipo backup (2600mAh). MCP73831 handles charging. System survives ~17 hours on battery alone.

## KiCad Project Files

See `hub-node.kicad_pro`, `hub-node.kicad_sch`, `hub-node.kicad_pcb` in this directory.

Note: KiCad project files are not yet generated. Use the pin assignments and BOM from the main README to create the schematic.