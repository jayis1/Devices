# TrailSync — Wrist Unit Schematic

KiCad schematic for the TrailSync Wrist Unit (nRF52832 + GPS + LoRa + sensors).

## Key Components
- nRF52832 (BLE + Sub-GHz co-processor)
- SX1262 (Sub-GHz 868/915 MHz radio)
- RFM95W (LoRa radio)
- u-blox SAM-M10Q (GPS)
- BMP390 (barometric altimeter)
- MAX30101 (PPG + SpO2)
- TMP117 (skin temperature)
- LSM6DSL (IMU for fall detection)
- SH1106 1.3" OLED (128×64)
- DRV2605L + LRA (haptic driver)
- SPH0645 (microphone for SOS audio)
- W25Q128 (16MB Flash)
- PCF8563 (RTC)

## Power
- 18650 LiFePO4 2600mAh
- USB-C charging (MCP73871)
- 3.3V LDO (TPS62825)

## Antenna
- PCB trace antenna for 868/915 MHz
- Ceramic patch antenna for GPS

*Full KiCad project files to be added.*