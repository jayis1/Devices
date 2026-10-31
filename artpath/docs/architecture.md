# Architecture and interface schedule

## Roles

Gateway (Pi Zero 2 W): local content cache, operator API, BLE central, local UTC timestamps. Artwork station (ESP32-C3): tactile trigger, NFC item selection, local audio playback, BLE readiness report. Optional context node: SHT40 ambient conditions for enclosure maintenance only. These roles are independent of visitor identity.

## Artwork station pin schedule (candidate)

| Signal | ESP32-C3 GPIO | Bus/connector | Notes |
|---|---:|---|---|
| I2C SDA | 4 | J2 | PN532 address/module straps to be confirmed |
| I2C SCL | 5 | J2 | 3.3 V pull-ups, size per bus capacitance |
| Button | 2 | J3 | normally open to GND; debounce in firmware |
| I2S BCLK | 6 | J4 | MAX98357A |
| I2S LRCLK | 7 | J4 | MAX98357A |
| I2S DOUT | 8 | J4 | MAX98357A |
| UART0 TX/RX | 21/20 | J1 | debug; verify module/carrier routing |
| 3V3/GND | — | J1/J2/J4 | common return, star audio supply return |

No native KiCad schematic or PCB layout is supplied. This table is an interface proposal only. Check exact module package pinout, boot strapping, antenna keepout, I²C pull-ups and supply transients against vendor documents before designing a board.

## Context node

SHT40 on 3V3 I²C at 0x44; candidate pins SDA 4/SCL 5; UART0 debug. Keep humidity sensor away from regulator, speaker exhaust and direct rain/sun. Its absence never affects artwork content availability.

## Gateway interfaces

Local REST: `GET /health`, `GET /content/{content_id}`, `POST /readiness` (validated versioned event). Persistent state is SQLite. The API must remain loopback-bound by default. BLE device enrollment is physical-installation-only in the future implementation. No remote control endpoint.

## Failure and privacy

Stale telemetry means unknown readiness. Offline cache is authoritative for installed content; no cloud is required for playback. There is no visitor event logging, location history, camera, or microphone. Retain only equipment readiness data and content revision. Operator must define site retention, access policy, and deletion before connecting any external service.
