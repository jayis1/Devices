# Gateway connection reference (not a KiCad schematic)

Documented by jayis1.

Conceptual pin/net list only; no KiCad library mapping, ERC or PCB exists.

| Ref | Part | Connections |
|---|---|---|
| U1 | ESP32-S3-WROOM-1 | 3V3/GND; GPIO8 SDA, GPIO9 SCL, GPIO4 status, GPIO0 service input (boot strap caution) |
| J1 | USB-C input | VBUS to protected 5V/3V3 supply; GND; correct CC resistors required |
| U2 | SSD1306 display module | SDA GPIO8; SCL GPIO9; 3V3; GND |
| D1 | status LED | GPIO4 through current-limit resistor |
| F1 | resettable fuse | USB VBUS protection |

Use the manufacturer reference design and antenna keepout; verify USB-C power implementation before fabrication.
