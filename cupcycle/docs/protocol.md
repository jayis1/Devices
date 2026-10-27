# CupCycle protocol

Invented and maintained by jayis1. Reference prototype, not a security protocol.

## Local frame

13-byte frame, little-endian integers: byte 0 version (1); byte 1 node (1 hub, 2 chute, 3 rack); byte 2 kind (1 count, 2 rack state, 3 health); bytes 3–4 sequence; bytes 5–8 Unix seconds; bytes 9–10 value; bytes 11–12 CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff, no reflection/xorout) over bytes 0–10. Count is cups in the current batch, saturating at 65535. Rack state: 0 unknown, 1 inspection pending, 2 staff-confirmed ready, 3 hold. Health value is bit flags: bit0 low battery, bit1 sensor fault, bit2 queue overflow.

Reject wrong length, unknown version/kind/node, bad CRC. Sequence equality means duplicate; sequence comparison uses modulo 65536. Timestamp is informational unless hub clock is valid; then reject >24h old. CRC detects transmission errors only. Use BLE LE Secure Connections, authenticated commissioning, per-node identity and revocation in deployed firmware; do not treat advertisements or CRC as authenticated commands. No production BLE stack is included here.

## MQTT mapping

Optional service publish topic `cupcycle/v1/{site_id}/{node_id}/events`; site_id and node_id are provisioned opaque identifiers (not names/addresses). JSON fields mirror frame: `version,node,kind,sequence,unix_s,value`. Broker must require TLS, unique device credentials, topic ACLs, payload size limits, and site isolation. The sample backend accepts HTTP locally and does not implement authentication; do not expose it to an untrusted network. MQTT publish failures retain bounded local history at the hub, not in this backend.

## Failure rules

Missing node means stale/unknown, never zero. Duplicate sequence is ignored. Reconnect with bounded exponential backoff. Commands, if added, must include idempotency key and expiry; no actuation is supported. Persist only aggregate counts and readiness acknowledgements.
