# BatteryLoop architecture and pin plan

## Signal flow

Tray platform → four load cells as a single bridge → NAU7802 differential ADC → ESP32-C3 I2C; one VL53L0X monitors tray occupancy. Dock observer's single VL53L0X senses one fixed bay. Return beacon provides an explicit human event. ESP32-C3 nodes send BLE advertisements to Pico W; gateway service on a USB-connected host receives serial/bridge data and forwards MQTT. Pico W Wi-Fi is a possible future gateway implementation, not the current backend runtime.

## Pin/net schedule (module GPIO)

| Board | Signal | GPIO / pin | Net |
|---|---|---|---|
| Tray ESP32-C3 | I2C SDA / SCL | GPIO4 / GPIO5 | SDA / SCL shared by NAU7802 + VL53L0X (distinct addresses required) |
| Tray ESP32-C3 | VL53L0X XSHUT | GPIO6 | XSHUT |
| Dock ESP32-C3 | I2C SDA / SCL | GPIO4 / GPIO5 | SDA / SCL |
| Dock ESP32-C3 | return button | GPIO7 | BTN to ground, internal pull-up |
| nRF52810 beacon | button / LED | P0.11 / P0.13 | BTN / LED (series resistor required) |
| Pico W | USB serial | USB D+/D− | USB data to gateway host |

These are design proposals. Check exact carrier/module pin availability, boot straps, pull-up voltage, bus address and sensor breakout circuitry before committing to a PCB. Add 100 nF + 10 µF local decoupling; one I2C pull-up pair per bus. No mains or charging connections.

## Power and mechanical

Tray/gateway use certified external 5 V USB supplies. Dock uses two AA cells and a 3.3 V regulator; verify regulator quiescent current and transient headroom for BLE before selecting parts. Beacon uses a CR2032 candidate only after pulse-current and lifetime testing. Load-cell mechanical platform must constrain off-axis loads and protect cells from overload; do not infer individual cell state from total tray weight without a human scan/assignment workflow. This prototype only reports aggregate mass/occupancy.

## Trust and failure boundaries

BLE advertisements can be spoofed and replayed; no security is implemented. Do not use this prototype outside a trusted lab. Production requires authenticated pairing, per-device keys, replay-resistant counters persisted across resets, signed firmware, TLS MQTT, site authorization, retention policy, and deletion support. Sensor failure, power loss, stale data, offline broker, and queue overflow must show unknown/stale—not “ready.” A household member confirms actual cell chemistry and charger compatibility.
