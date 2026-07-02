# JointSync — Protocol Specification

## Packet Format

All JointSync communications use a binary packet format:

```
┌──────────────────────────────────────────────────────────┐
│  Byte  │  Field         │  Description                     │
├──────────────────────────────────────────────────────────┤
│  0     │  sync[0]       │  0x4A ('J')                     │
│  1     │  sync[1]       │  0x53 ('S')                     │
│  2     │  version       │  0x01                           │
│  3     │  msg_type      │  See message type enum          │
│  4-5   │  sender_id     │  Node ID (little-endian)        │
│  6-7   │  seq_num       │  Sequence number (LE)           │
│  8     │  flags         │  Bit 0: encrypted               │
│        │                │  Bit 1: compressed               │
│        │                │  Bit 2: ACK requested           │
│  9     │  payload_len   │  0-245                           │
│  10    │  checksum      │  XOR of bytes 0-9               │
│  11+   │  payload       │  0-245 bytes                     │
└──────────────────────────────────────────────────────────┘
```

**Total:** 11 + payload_len bytes (max 256)

## Node IDs

| ID Range | Node |
|----------|------|
| 0x0000 | Hub |
| 0x0001 - 0x000F | Joint Tags (1-15) |
| 0x0100 - 0x010F | Compression Sleeves (1-15) |
| 0x0200 - 0x020F | Joint Scanners (1-15) |

## Message Types

| Type | Code | Direction | Description |
|------|------|-----------|-------------|
| DATA_IMU | 0x01 | Tag→Hub | BMI270 IMU data (15 bytes) |
| DATA_TEMP | 0x02 | Tag→Hub | TMP117 temperature (6 bytes) |
| DATA_PPG | 0x03 | Tag→Hub | MAX30101 PPG data (38 bytes) |
| DATA_THERMAL | 0x04 | Scanner→Hub | MLX90640 thermal chunk (130 bytes) |
| DATA_IMAGE | 0x05 | Scanner→Hub | OV5640 image metadata |
| DATA_PRESSURE | 0x06 | Sleeve→Hub | Pressure + load cell (8 bytes) |
| CMD_THERAPY | 0x10 | Hub→Sleeve | Compression therapy command (5 bytes) |
| CMD_SCAN | 0x11 | Hub→Scanner | Scan trigger |
| CMD_MODE | 0x12 | Hub→Tag | Mode change (active/sleep) |
| ALERT_FLARE | 0x20 | Hub→Cloud | Flare warning |
| ALERT_INFLAME | 0x21 | Hub→Cloud | Inflammation detected |
| ALERT_THERAPY | 0x22 | Hub→Cloud | Therapy reminder |
| ACK | 0x30 | Any→Sender | Acknowledge |
| NACK | 0x31 | Any→Sender | Reject |
| PAIR_REQ | 0x40 | Node→Hub | Pairing request |
| PAIR_ACK | 0x41 | Hub→Node | Pairing accept |
| HEARTBEAT | 0x50 | Any→Hub | Keepalive |
| STATUS | 0x51 | Any→Hub | Battery + state report |

## BLE GATT

| UUID | Name | Properties |
|------|------|------------|
| 0x4A53 | JointSync Service | — |
| 0x4A01 | Data Characteristic | Write, Notify |
| 0x4A02 | Command Characteristic | Write |
| 0x4A03 | Status Characteristic | Read, Notify |
| 0x4A04 | Config Characteristic | Read, Write |

## Sub-GHz 868 MHz

- Transceiver: CC1120
- Frequency: 868.0 MHz
- Modulation: 2-FSK
- Data rate: 50 kbps
- Deviation: ±20 kHz
- TDMA: 8 slots × 50 ms = 400 ms cycle
  - Slot 0: Hub beacon
  - Slots 1-6: Sleeve data
  - Slot 7: Contention (pairing, status)

## Encryption

Optional AES-CCM-128:
- 16-byte key (per-pairing session key)
- 8-byte nonce
- 8-byte authentication tag
- Activated when flag bit 0 is set