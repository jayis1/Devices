# Schematic capture status

Native KiCad capture is pending; do not treat these reference pin/net lists as fabrication-ready schematics. Candidate net assignments (verify board module pin mux and RF layout before capture):

- Gateway ESP32-S3-WROOM-1: 5V USB input -> board 3V3 regulator; GPIO4 -> LED (series resistor to GND); onboard BLE antenna keepout per module datasheet. No field wiring.
- Surface ESP32-C6-MINI-1: regulated 3V3; GPIO6 SDA and GPIO7 SCL to SHT40 (0x44) and ADS1115 (0x48), pull-ups 4.7k to 3V3; ADS1115 A0 to capacitive wetness analog output; common ground; decoupling at each device.
- Beacon nRF52840 module: regulated 3V, button between P0.11 and GND with internal pull-up; NFC antenna/module as specified by selected module vendor; BLE antenna keepout.

Power protection, ESD, antenna matching and enclosure design require engineering review. Exact boards/modules and pin mappings must be frozen before PCB design.
