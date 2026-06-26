# TrailSync — Hub Schematic

KiCad schematic for the TrailSync Hub (ESP32-S3 + Sub-GHz + LoRa + WiFi + display).

## Key Components
- ESP32-S3-N16R8 (WiFi6 + BLE + edge compute)
- SX1262 (Sub-GHz 868/915 MHz radio)
- RFM95W (LoRa radio)
- u-blox SAM-M10Q (GPS — hub's known position)
- ILI9488 4.0" IPS TFT (480×320)
- W25Q256 (32MB Flash)
- MicroSD slot (trail maps)
- PCF8563 (RTC)
- MAX98357A + 28mm speaker (SOS relay alerts)
- WS2812 RGB LED + 4× SMD LEDs

## Power
- 5V USB-C mains or vehicle power
- 18650 LiPo 3500mAh backup battery
- MCP73871 charge controller
- 3.3V 1A step-down (TPS62825)

## Display
- Group dashboard: runner positions, vital signs, pace
- Trail conditions: beacon data, weather, hazards
- SOS status: incoming distress, rescue coordination

*Full KiCad project files to be added.*