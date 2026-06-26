# TrailSync — Trail Beacon Schematic

KiCad schematic for the TrailSync Trail Beacon (nRF52833 + Sub-GHz + LoRa + solar).

## Key Components
- nRF52833 (Sub-GHz + LoRa control + beacon logic)
- SX1262 (Sub-GHz 868/915 MHz radio)
- RFM95W (LoRa radio 5-15 km)
- u-blox SAM-M10Q (GPS — position set at install)
- BME280 (temperature, humidity, pressure)
- AM312 (PIR motion sensor)
- PMS5003 (optional PM2.5 for wildfire smoke)
- MCP73871 (solar charge controller)
- 5W monocrystalline solar panel
- 18650 LiFePO4 1500mAh

## Power
- 5W solar panel + MCP73871 → 18650 LiFePO4
- Full beacon mode: > 30% battery (Sub-GHz + LoRa + sensors)
- Reduced mode: 15-30% battery (LoRa only, 10-min intervals)
- Emergency mode: < 15% battery (LoRa receive only, 30-min intervals)
- 30+ days without sun

## Antenna
- 868/915 MHz whip antenna for Sub-GHz + LoRa
- External SMA connector for directional antenna option

## Enclosure
- IP67 polycarbonate, 120×80×40mm, camo green
- Tree strap or pole mount hardware included
- UV-resistant coating

*Full KiCad project files to be added.*