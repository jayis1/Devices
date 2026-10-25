# MealBridge protocol

Transport envelope uses `firmware/common/protocol.h`: little-endian version (1), kind, 16-bit node, 32-bit sequence, payload length (0–64), payload, CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff). Frame size is exactly 11 + payload length. CRC detects accidental corruption only; production security requires per-node authenticated encryption and nonce/replay handling, not implemented. JSON backend event shape and units are documented in README; timestamps are ISO8601 UTC with explicit offset.

ESP-NOW is selected for local prototype transport; Wi-Fi/MQTT is hub uplink. Range and interference have not been measured. Each node should buffer up to 128 events in a production implementation; current firmware does not yet implement buffering, ACK/retry, clock sync, pairing or OTA.
