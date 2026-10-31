# Architecture and bring-up

Status: design only. No fabricated board or validated hardware. ESP32-C3-MINI-1 and ESP32-C6-DevKitC-1 are proposed targets; verify exact revisions and pin availability against datasheets before PCB capture. Vendor SDK sensor/radio bindings are not implemented.

## Proposed pin/net schedule

| Node | Signal | Pin | Notes |
|---|---|---|---|
| Observer | INA219 SDA/SCL | GPIO6/GPIO7 | 3.3 V I2C, 4.7 kΩ pullups; confirm module variant |
| Observer | reed input | GPIO4 | Switch to GND, pull-up, 30 ms debounce |
| Observer | LED | GPIO5 | 1 kΩ series resistor |
| Gateway | LED | GPIO8 | 1 kΩ; optional add-on |
| Gateway | USB serial | onboard | DevKitC USB-UART |

Observer measures only an approved isolated extra-low-voltage DC supply output. Never connect to mains, appliance internals, or unisolated supply. INA219 common-mode limit is 26 V; select shunt/range and verify dissipation before use. USB 5 V, module regulator, 100 nF local decoupling, 10 µF bulk, input resettable fuse and reverse-polarity protection on custom board. No battery design. Pin schedule is proposed and must be checked before capture.

## Behavior / failure policy

Sample once per second. Uncalibrated, sensor fault, boot, or three missed samples means `unknown`. Calibrated thresholds use hysteresis. Gateway rejects duplicate/out-of-order sequence values and forwards latest state. Backend expires observations at 90 seconds. Never actuates appliances, certifies safety/cleanliness/availability, or tracks people. Manual indicators remain authoritative.

## Power, enclosure, assembly

Planning only: USB powered observer 35–100 mA average (higher transmit peaks); gateway 80–250 mA average. No battery-life estimate. Flame-retardant enclosure, strain-relieved low-voltage leads, no ingress rating; keep away from heat/liquid. Before use, bench-test only with isolated current-limited supply; calibrate idle/active thresholds without opening appliance; test disconnected sensor, radio loss, reboot, and stale expiry. Hardware is unbuilt and untested.

## Security limitations

CRC is not authentication. Encrypted ESP-NOW peer provisioning, key rotation/revocation, signed OTA, MQTT TLS/client certificates, authenticated API, rate limits and audit are unimplemented. Keep services on isolated trusted LAN; production deployment is blocked pending those controls. No safety or medical purpose.