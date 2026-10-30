# Design and pin contract

Reference prototype only; verify against exact board revisions before assembly. Each node uses a USB-powered ESP32-C3-DevKitM-1. I2C INA219 breakout senses voltage/current on an appropriately rated USB VBUS path. Hub uses ESP32-S3-DevKitC-1. All logic is 3.3 V; breakout must be 3.3 V logic compatible.

| Node | Signal | MCU pin | Net / note |
|---|---|---:|---|
| Hub | I2C SDA | GPIO8 | display expansion reserved |
| Hub | I2C SCL | GPIO9 | display expansion reserved |
| Hub | Status LED | GPIO48 | board revision dependent; confirm |
| Bay monitor | I2C SDA | GPIO4 | INA219 SDA |
| Bay monitor | I2C SCL | GPIO5 | INA219 SCL |
| Bay monitor | Alert input | GPIO6 | optional INA219 ALERT, pull-up to 3.3 V |

Schematic/netlist notes: regulated 3.3 V is supplied by dev board; USB 5 V remains on external approved adapter and the rated breakout. INA219 VIN+/VIN− are series connected to USB positive only; USB ground common. Never connect a bare board to mains. Keep USB connector shield and enclosure strain relief per connector vendor guidance. Hub display is optional and omitted from the first prototype.

BOMs are in `hardware/bom/`. Confirm the precise INA219 breakout layout, shunt value, connector current rating, and calibration before use. No actual KiCad schematic, PCB, ERC, or thermal validation is supplied.

Power: hub and each bay monitor use independent approved USB 5 V adapters. No battery claims. Measure idle and full-load dissipation before enclosing; keep the sensing board accessible and ventilated. In operation, zero/invalid sensor data maps to unknown, never available.

Bring-up: install ESP-IDF 5.x and target `esp32c3` / `esp32s3`; create projects from the per-node examples, configure BLE and Wi-Fi credentials out-of-band, and provision one unique node ID. The supplied C frame codec is portable and can be tested on host using the firmware README command. It is not a complete ESP-IDF application.
