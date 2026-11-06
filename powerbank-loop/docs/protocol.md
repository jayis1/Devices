# Node protocol v1

All logical events share: `v` (integer 1), `site` (1..32 ASCII chars), `node` (1..32 ASCII chars), `seq` (uint32), `ts` (UTC epoch seconds or null), `kind` (allow-listed string), `payload` (kind-specific object). Binary radio envelope in `protocol.h` is `[magic 2][version 1][node 1][seq LE32][kind 1][length 1][payload N][CRC16 LE]`, maximum payload 48 bytes. The host codec does not include site or timestamp; radio provisioning binds node to site and gateway adds receive timestamp. This mapping must be reconciled in production implementation.

Kinds: `DOCK_STATUS` (occupied, total, charge-ready counts, fault count), `PACK_STATUS` (opaque pack ID, SOC bucket 0..4, temp deci-C), `HEARTBEAT` (uptime and dropped-event count). Numeric units are explicit. No remote commands are defined.

Gateway ACK carries node and sequence. Transmit at most three times with randomized backoff; duplicate sequence is idempotent. Sequence wrap/reboot persistence must be designed before field deployment. Reject unknown versions, oversized/truncated frames, unknown kinds, out-of-range measurements, and stale timestamps. Unknown measurements use missing/null, never zero.

Radio CRC is not authentication. Production must add a reviewed AEAD (e.g. AES-CCM with unique site/node key, nonce including persistent counter, replay window) and authenticated provisioning. Do not reuse nonces or share fleet-wide keys. BLE tag access requires bonded dock identity with rotating private addresses; implementation absent.

MQTT topic proposal: `powerbankloop/v1/{site}/{node}/telemetry`; TLS, client certificate, broker ACL, and no anonymous publish are deployment requirements. No actuator command topic exists. Payload limit 1024 bytes; dedupe on `(node,seq)`.
