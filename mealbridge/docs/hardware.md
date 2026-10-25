# Hardware notes

Prototype estimate (USD, indicative, 2026-10; no supplier quote). Same 3.3 V logic domain; use module breakout carrier boards for prototypes, not bare-module layouts.

| Node | SoC/module | Interfaces and pins | Power |
|---|---|---|---|
| Hub | ESP32-S3-WROOM-1-N8 | I2C SDA GPIO8, SCL GPIO9 for SSD1306 optional display; UART0 GPIO43/44 debug; Wi-Fi MQTT uplink | 5 V USB → 3.3 V carrier regulator; budget 500 mA peak |
| Shelf scale | ESP32-C3-MINI-1 | HX711 DOUT GPIO4, SCK GPIO5; load-cell bridge to HX711 differential input; GPIOs are not boot straps on module carrier assumed—verify carrier schematic | 5 V USB → carrier 3.3 V; budget 250 mA peak |
| Fridge sentinel | ESP32-C3-MINI-1 | TMP117 I2C SDA GPIO4/SCL GPIO5, address 0x48; pull-ups to 3.3 V; external antenna not required | USB supply outside cold chamber; budget 250 mA peak |
| Pantry station | ESP32-C3-MINI-1 | SSD1306 I2C SDA GPIO4/SCL GPIO5 (0x3C), momentary button GPIO3 to GND with pull-up | 5 V USB → carrier 3.3 V; budget 250 mA peak |

BOM estimates: ESP32-S3-WROOM-1-N8 module $5; ESP32-C3-MINI-1 ×3 $3 each; HX711 module $3, 5 kg load cell $5, TMP117 breakout $5, SSD1306 display $4, buttons/passives/wiring $8, USB supplies/cables $30, carrier boards $10, rough total $79–83. Exact purchased breakout variants and footprints must be selected before PCB design. Each node must have local decoupling, reset/programming access and strain relief. Do not use GPIO6–11 on C3 modules (flash interface). Verify pin assignments against the selected carrier before assembly.

## Netlist intent

The `schematic/` subdirectories contain textual netlists. They are design notes, not KiCad schematics; no PCB, ERC or physical test has been completed. Load cell excitation and HX711 analog wiring follow the module manufacturer's reference circuit. Use certified enclosed USB supplies only; no mains circuitry is designed here.
