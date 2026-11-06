# Conceptual electrical net intent — not KiCad files

No production schematic/PCB is provided. Netlists below are planning references only; review module datasheets and perform ERC, layout, thermal, USB-C, EMC, RF, enclosure, and fire-safety review before fabrication.

Gateway: ESP32-S3 module 3V3/GND with local decoupling; SPI2 to SX1262 (SCK/MOSI/MISO/NSS/BUSY/DIO1/RESET per README); I2C to display and digital temperature sensor; 5V SELV input to rated regulator. Keep antenna feed per module reference layout; no arbitrary long trace or unapproved antenna.

Dock: RP2040 3V3; SPI0 to SX1262; I2C to display/temperature/current monitor; one isolated logic input per slot from presence switch with external pull; current monitor on each charging channel (multiplexing is a design choice not resolved here); hardware pull-down on output enable. Certified enclosed USB-C power paths accept regulated 5V input. No bare-cell charging nets.

Pack tag: nRF52840 BLE module, I2C to fuel gauge/temperature IC, protected pack logic rail. Tag must not connect to charger control or bypass pack protection.

These statements do not establish component compatibility. Exact parts, decoupling, connector pinout, protection ratings, battery behavior, and footprints must be selected and verified before schematic capture.
