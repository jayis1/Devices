Reference wiring only; not a native KiCad schematic or fabrication release.

| Node | Signal | Connection | Notes |
|---|---|---|---|
| Rack Sentinel | NAU7802 SDA/SCL | ESP32-C3 GPIO4/GPIO5 | 3.3 V logic; verify carrier pullups |
| Rack Sentinel | NAU7802 DRDY | GPIO6 | Input |
| Return Gate | break-beam output | GPIO4 | Open collector; pull up to 3.3 V |
| Return Gate | acknowledge button | GPIO5 | Switch to GND, internal pull-up |
| Both | status LED | GPIO7 via 330 ohm | Series resistor |

Check module pin strapping, breakout levels, decoupling and connector polarity before PCB work.
