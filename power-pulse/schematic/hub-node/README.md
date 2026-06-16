# PowerPulse Hub Node Schematic

## Overview
The hub node is the central coordinator of the PowerPulse system. It aggregates data from circuit monitors and appliance tags via Sub-GHz and BLE, runs local inference for time-critical alerts, and bridges to the cloud via WiFi/MQTT.

## Key Design Decisions
- **ESP32-S3** chosen for dual-core (WiFi + BLE + Sub-GHz SPI), 8MB PSRAM for ML inference, and mature ESP-IDF ecosystem
- **CC1101 Sub-GHz radio** for reliable communication through metal breaker panels where 2.4GHz fails
- **18650 backup battery** for offline operation during power outages (critical for arc fault alerts)
- **microSD card** for local data buffering (up to 7 days offline storage)

## Schematic Notes
- All decoupling capacitors: 100nF ceramic + 10µF tantalum per power pin
- CC1101 antenna: 868 MHz whip antenna, λ/4 = 86mm wire
- Pull-up resistors on I2C bus (SDA/SCL): 4.7kΩ
- ESP32-S3 strapping pins: GPIO0, GPIO3, GPIO45, GPIO46 must be left floating or pulled correctly at boot
- USB-C port follows USB 2.0 specification (5V only, no PD)

## Power Supply
- Input: USB-C 5V / 18650 3.7V
- Main rail: 5V USB-C direct → MP28167 buck → 3.3V @ 2A
- Battery: TP4056 charger → DW01A protection → 3.3V via auto-switch diode OR-ing
- Total budget: ~800mA @ 3.3V (WiFi TX burst), typical 200mA

## KiCad Project
The KiCad project is in this directory. Open `hub-node.kicad_pro` in KiCad 7+.