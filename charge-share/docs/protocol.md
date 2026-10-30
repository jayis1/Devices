# Protocol v1

Transport: BLE GATT for node-to-hub; hub-to-service MQTT over TLS in production. Topic `chargeshare/v1/{site_id}/{node_id}/telemetry`. Prototype service accepts equivalent JSON over HTTP. BLE transport security and cloud auth require deployment-specific provisioning; do not use on an untrusted network.

Frame is exactly 20 little-endian bytes: version u8 (1), message_type u8 (1 telemetry), node_id u32, sequence u32, uptime_ms u32, millivolts u16, milliamps u16, CRC-16/CCITT-FALSE u16 over preceding 18 bytes. CRC detects accidental corruption, not malicious alteration. Reject unknown versions/types, invalid CRC, duplicate/old sequence, and lengths other than 20. MQTT/JSON additionally carries `site_id` and UTC `observed_at`; no identifier for the person or attached client is permitted.

JSON example: `{"version":1,"node_id":17,"sequence":8,"uptime_ms":12345,"millivolts":5000,"milliamps":750,"observed_at":"2026-01-01T00:00:00Z"}`. A missing heartbeat marks status stale after the configured timeout; stale or invalid values must display unknown. No remote commands or actuation exist.
