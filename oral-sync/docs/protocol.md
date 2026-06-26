# OralSync Sync Protocol (OSMP)

OSMP is a framed binary protocol carried over BLE 5.0 GATT (Nordic-UART-style service) between the Hub (central) and the Toothbrush / Plaque Scanner / Saliva Sensor (peripherals). It is also used over a USB-CDC console for pairing & provisioning.

## 1. GATT Layout

| Item | UUID |
|------|------|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX (hub→node, Write) | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX (node→hub, Notify) | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

MTU negotiated to 247 bytes at connection; frame payload max 180 bytes to fit one notification + framing.

## 2. Frame Format

```
 Offset  Field      Size  Description
 0       SOP        1     Start-of-packet = 0xAA
 1       LEN        1     Payload length (0..180)
 2       TYPE       1     Message type (see §3)
 3       SEQ        1     Sequence number (wraps 0..255)
 4..N    PAYLOAD    LEN   Type-specific payload
 N+1..N+2 CRC16      2     CRC-16/CCITT over [LEN..PAYLOAD], little-endian
```

Total frame size = LEN + 5 bytes. If `LEN` would exceed 180 the sender MUST split into multiple TYPE=CHUNK frames.

## 3. Message Types

| Type | Name            | Direction    | Payload |
|------|-----------------|--------------|---------|
| 0x01 | HELLO           | node→hub     | `[node_type(1)][hw_rev(1)][fw_ver(2)][capabilities(2)]` |
| 0x02 | PAIR_REQ        | node→hub     | `[nonce(16)]` (during pairing only) |
| 0x03 | PAIR_ACK        | hub→node     | `[hub_nonce(16)][session_key(16)]` (ECDH-derived, encrypted with factory key) |
| 0x10 | SESSION_START   | node→hub     | `[session_id(4)][user_id(1)][start_ts(4)]` |
| 0x11 | SESSION_END     | node→hub     | `[session_id(4)][duration_s(2)][coverage_bitmap(8)]` |
| 0x12 | IMU_SAMPLE      | node→hub     | `[ax(2)][ay(2)][az(2)][gx(2)][gy(2)][gz(2)][ts_ms(2)]` (int16, LSB = 0.001 g / 0.1 dps) |
| 0x13 | PRESSURE_SAMPLE | node→hub     | `[pressure_cN(2)][ts_ms(2)]` (centinewton) |
| 0x14 | SCAN_FRAME      | scanner→hub  | `[scan_id(4)][band(1)][thumb_w(1)][thumb_h(1)][jpeg_thumb(≤160)]` |
| 0x15 | SCAN_EMBED      | scanner→hub  | `[scan_id(4)][emb(32 floats=128 B)]` |
| 0x16 | SALIVA_READING  | saliva→hub   | `[ph_x100(2)][nitrite_um(2)][buffer(1)][temp_c10(2)]` |
| 0x20 | COACH_CUE       | hub→node     | `[cue_id(1)][arg(1)]` (e.g. MOVE_TO_UPPER_LEFT, OVERPRESSURE) |
| 0x21 | QUAD_PACE       | hub→node     | `[quadrant(1)]` |
| 0x30 | ACK             | both         | `[acked_seq(1)][status(1)]` |
| 0x31 | NACK            | both         | `[acked_seq(1)][reason(1)]` |
| 0x40 | OTA_CHUNK       | hub→node     | `[offset(4)][len(1)][data(≤128)]` |
| 0x41 | OTA_DONE        | hub→node     | `[crc32(4)]` |
| 0xF0 | PING            | both         | `[ts_ms(4)]` |
| 0xF1 | PONG            | both         | `[ts_ms(4)]` |

## 4. Coverage Bitmap

SESSION_END payload contains an 8-byte bitmap tracking coverage across the FDI tooth chart surfaces. Each bit = one sextant-surface combo:

```
byte 0: upper-right buccal/occlusal/lingual × sextants (bits 0–5)
byte 1: upper-left  ...
byte 2: lower-right ...
byte 3: lower-left  ...
byte 4: anterior buccal/lingual (bits 16–21)
byte 5: posterior mesial/distal (bits 22–27)
bytes 6–7: reserved
```

A "fully covered" 2-min Bass session sets all 48 bits.

## 5. Encryption

After PAIR_ACK, all subsequent frames are encrypted with **AES-128-CCM** (8-byte tag prepended to payload). The session key is derived via ECDH P-256 from the node factory key and the hub's ephemeral key, then HKDF-SHA256 with info `"OSMP-v1"`. Nonce = `[seq(1)][session_ctr(7)]` (session_ctr increments per session).

## 6. Duty Cycling

- Toothbrush: advertises at 20 ms while in use (peripheral), 1 s when idle & paired; Hub scans during `SESSION_START` window only
- Scanner: advertises on demand (button press); Hub auto-connects
- Saliva Sensor: connects once per reading (event-driven), <3 s total
- Hub→cloud: MQTT QoS1, session rollup at SESSION_END + hourly health heartbeat

## 7. Reliability

- Every telemetry frame is ACKed within 500 ms or retransmitted (max 3×)
- IMU_SAMPLE / PRESSURE_SAMPLE use sequence numbers; gaps flagged in cloud
- OTA_CHUNK uses sliding-window of 8 chunks, ACK per window
- CRC16 catches BLE radio corruption (BLE has its own CRC but OSMP adds integrity for cloud-replay protection)

## 8. Example Handshake

```
node  → hub  HELLO  type=TOOTHBRUSH hw_rev=2 fw=1.4
hub   → node  ACK    acked_seq=0 status=OK
node  → hub  SESSION_START  user_id=1
hub   → node  ACK
node  → hub  IMU_SAMPLE × 6000 (50 Hz × 120 s)
node  → hub  PRESSURE_SAMPLE × 6000
hub   → node  COACH_CUE MOVE_TO_UPPER_LEFT  (real-time)
hub   → node  QUAD_PACE 2  (every 30 s)
node  → hub  SESSION_END  duration=120 coverage=0x3F3F3F...
hub   → node  ACK
hub   → cloud MQTT  oralsync/<home>/<tb>/event  session rollup
```