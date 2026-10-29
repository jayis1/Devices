# Pin-level reference assignments (schematic capture TODO)

These are connection plans, not KiCad schematics. Do not fabricate from this page alone. All signals are 3.3 V logic.

| Board | Reference | Pin | Connection | Notes |
|---|---|---|---|---|
| Printer observer | nRF52840 module | P0.02/AIN0 | Phototransistor + 10k pull-up | ADC observes status LED optically; never connect to printer electrical nets |
| Printer observer | nRF52840 module | P0.11 | Reed switch to GND | Internal pull-up, optional enclosure context |
| Printer observer | nRF52840 module | SWDIO/SWDCLK | Tag-Connect pads | Programming/debug |
| Filament sentinel | nRF52840 module | P0.26/P0.27 | SHT31 SDA/SCL | 4.7k pull-ups to 3V3; address 0x44 |
| Filament sentinel | nRF52840 module | P0.11 | Door reed to GND | Pull-up |
| Gateway | ESP32-S3-DevKitC-1 | USB/UART | USB-C power/programming | BLE and Wi-Fi; no printer control |

Conceptual block diagrams only; no electrical-rule-checked schematic files are provided. BOM rows identify reference components, not validated footprints or complete production assemblies.
