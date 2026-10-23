# Pin assignments and draft netlists

Board-level assignments are illustrative ESP32 module GPIO selections and must be checked against the exact carrier schematic before fabrication. Logic is 3.3 V only; I2C pull-ups to 3.3 V; no mains or actuator outputs.

| Node | SoC/module | Pin | Signal | Net/part |
|---|---|---:|---|---|
| Gateway | ESP32-S3-WROOM-1-N8 | GPIO8 | I2C SDA | display/status bus |
| Gateway | ESP32-S3-WROOM-1-N8 | GPIO9 | I2C SCL | display/status bus |
| Gateway | ESP32-S3-WROOM-1-N8 | GPIO43/44 | UART TX/RX | debug console |
| Climate | ESP32-C6-WROOM-1 | GPIO6 | I2C SDA | SHT31 + SCD41 |
| Climate | ESP32-C6-WROOM-1 | GPIO7 | I2C SCL | SHT31 + SCD41 |
| Supply | ESP32-C6-WROOM-1 | GPIO4 | HX711 DOUT | load-cell ADC |
| Supply | ESP32-C6-WROOM-1 | GPIO5 | HX711 SCK | load-cell ADC |
| Supply | ESP32-C6-WROOM-1 | GPIO10 | GPIO input | break-beam receiver |

Verify exact module variant pin exposure, boot strapping, antenna keep-out, decoupling, and radio coexistence against manufacturer datasheets and carrier design before assembly. No complete KiCad schematics are supplied.
