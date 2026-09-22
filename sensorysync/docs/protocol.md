# SensorySync protocol v1

Author: jayis1

## Transport and topics

Field nodes use region-appropriate 868/915 MHz SX1262 TDMA only after local RF approval. BLE 5.3 is limited to Comfort Band provisioning and hub status. Hub MQTT topics are exactly:

- `sensorysync/v1/<node_id>/telemetry`
- `sensorysync/v1/<node_id>/command`
- `sensorysync/v1/<node_id>/health`

Clients may publish only their assigned node ID. Commands are hub-originated and include an expiry.

## Binary frame

Little-endian frame: `version:u8 | node_id:u8 | type:u8 | flags:u8 | epoch:u32 | seq:u32 | payload_len:u16 | payload[0..64] | crc32c:u32`. CRC-32C covers all preceding bytes. Receivers reject versions other than 1, payloads above 64 bytes, invalid CRC, replayed sequence numbers in an epoch, and expired commands. `type=1` telemetry, `2` health, `16` command, `17` command result.

Telemetry JSON includes `node_id`, `epoch`, `seq`, `captured_at`, `quality` (`ok|stale|fault|calibrating|time_untrusted`) and numeric metrics. Units: lux, dBA estimate, ppm CO2, degC, percentRH, microsiemens EDA, and `0..1` output levels.

## Command policy

Permitted commands are `set_light_level`, `set_fan_level`, `set_noise_level`, and `stop_outputs`; all accept `value` in `[0,1]` and `expires_at`. Nodes accept matching target ID, future expiry no more than 30 seconds ahead, and monotonic command sequence. `stop_outputs` is always local and does not require radio. Rejections emit a command result with `reason`.
