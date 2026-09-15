# WorkshopSync protocol v1

Author: jayis1

## Frame

`version:u8 | type:u8 | node_id:u32 LE | seq:u32 LE | epoch:u32 LE | length:u8 | payload:0..96 | crc32c:u32 LE`

The CRC covers bytes through the payload. Receivers require exact wire length, version 1, payload ≤96 bytes, a valid CRC, and a sequence newer than the last accepted sequence for a node. Nodes begin a new session with a persisted boot epoch; the hub stores `(epoch, seq)` and does not infer that a duplicate is fresh.

## Types

| Type | Value | Direction | Meaning |
|---|---:|---|---|
| HEARTBEAT | 1 | node→hub | health, battery, RSSI |
| TELEMETRY | 2 | node→hub | sensor features and quality |
| READINESS | 3 | hub→app | explainable advisory card |
| COMMAND | 16 | hub→node | non-safety configuration only |
| ACK | 17 | node→hub | command result |
| FAULT | 18 | node→hub | local fault/stale state |

## Command rules

Commands include a random nonce, policy version, and expiry epoch. Nodes discard expired, duplicate, unsupported, malformed, or oversized commands; they acknowledge rejections. No v1 command changes mains power, motor state, guarding, or emergency-stop behavior. Retry uses exponential backoff (1, 2, 4 seconds; three attempts). MQTT duplicates are expected and are deduplicated with node ID + epoch + sequence.

## Provisioning

A physical button starts a 60-second enrollment window. The hub assigns a node ID and records a per-device key through a local authenticated channel. Secrets are never represented in source, test fixtures, logs, screenshots, or export files.