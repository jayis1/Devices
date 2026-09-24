# HygieneGuard protocol

Author: jayis1

## Radio contract

SX1262 nodes use 868 MHz TDMA only where legal; region-specific builds must select legal frequency, duty cycle, and power. The hub grants slots. BLE is limited to Door Beacon commissioning and local status.

`HGFrame` is little-endian, version 1. `payload_len` is limited to 64 bytes. CRC-32C covers every preceding byte. Receivers drop invalid version, length, CRC, replayed sequence, and expired commands.

| Message | Payload | Meaning |
|---|---|---|
| `HG_TELEMETRY` | flow_ml, soap_g, temperature_c_x100, flags | station observation |
| `HG_EVENT` | event_code, timestamp | voluntary button/pump event |
| `HG_HEALTH` | battery_mv, rssi, fault_bits | node health |
| `HG_COMMAND` | command, expiry_epoch, nonce | display refresh only |

Commands expire after 30 seconds. The only command changes a Door Beacon display; it cannot control plumbing, dispensers, doors, or people.