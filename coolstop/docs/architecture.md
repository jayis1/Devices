# Architecture and prototype boundaries

Three nodes: ESP32-S3-WROOM-1-N8 gateway with SSD1306 status display; ESP32-C3-MINI-1 climate node with SHT31-DIS-B; ESP32-C3-MINI-1 supply node with HX711 and a 1 kg load cell. Wi-Fi MQTT on the site LAN connects nodes and gateway to Mosquitto on a Linux host. The host runs FastAPI and SQLite; mobile application is a future client stub. Gateway coordinates/display status but is not the cloud/API host. Internet is not required for LAN status; remote access is out of scope.

| Component | GPIO/bus | Firmware | Reading |
|---|---|---|---|
| Climate SHT31 | SDA 4, SCL 5, I²C address 0x44 | `firmware/climate-node/main.c` | °C and %RH |
| Supply HX711 | DOUT 4, SCK 5 | `firmware/supply-node/main.c` | calibrated grams |
| Gateway OLED | SDA 8, SCL 9, I²C address 0x3C | `firmware/gateway/main.c` | freshness/status display |

Each input uses a single 5 V USB source and the selected module/carrier's 3.3 V regulation. Prototype current and thermal budgets are not characterized; size supplies for Wi-Fi transmit peaks per module datasheet. No battery-life claim. Shared USB power is not isolation; use separate certified supplies in a finished installation. Verify carrier pin availability, pullups, logic voltage, I²C pullup interaction, module RF variant, and HX711/load-cell wiring before assembly.

Failure behavior: reports include sequence and timestamp; API retains latest report per node, with age shown. Missing nodes are unknown. Internet outage does not prevent local MQTT/API if host remains powered. Host/gateway outage disables display/API; staff uses manual checklist. No actuator or automated alarm is present. Keep raw measurements only as long as required by site policy; default local SQLite has no automatic deletion, so operators must set retention before deployment. No personal/visitor data is collected.

The schematic directories contain text pin/net specifications only. They are not native KiCad schematics, ERC-checked, layout files, or evidence of electrical safety. No physical integration or RF range tests were performed. Local radio/module certification rules apply.
