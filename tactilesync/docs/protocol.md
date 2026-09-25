# Protocol specification

## Transport
Nodes use BLE 5.3 LE Secure Connections. Room ranging uses IEEE 802.15.4z UWB between an anchor and a paired Haptic Band. Hub uplink, if enabled, uses MQTT over TLS; topic prefix is `tactilesync/<hub-id>/events`.

## Frame
`ts_frame_t` has version, event type, source ID, key ID, sequence, payload length, payload, and 32-bit message-integrity-code field. Production board layers must use AES-CCM with an installation-specific key; the C reference checksum is deliberately non-cryptographic and must never ship.

## Semantics
- `ZONE`: payload is a quantized range or confidence bin.
- `DOOR`: payload byte 0 is reed state (0 closed, 1 open).
- `APPLIANCE`: payload byte 0 is a locally configured marker event.
- `ACK`: confirms a user acknowledgement of a sequence.

Receivers reject any sequence not strictly greater than the last accepted sequence per source. Pairing rotates keys and resets the source counter only after physical confirmation.
