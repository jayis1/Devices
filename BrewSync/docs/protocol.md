# BrewSync Mesh Protocol (BSMP) Specification

Version: 1.0

## Overview

BSMP is the binary wireless protocol used between BrewSync nodes and the Hub over Sub-GHz 868/915 MHz radio (SX1262). The Hub acts as coordinator in a star topology. All payloads are AES-128-CCM encrypted after pairing.

## Physical Layer

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915.0 MHz (US) |
| Modulation | LoRa |
| Spreading Factor | SF7 (normal) – SF12 (long range) |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Preamble | 8 symbols |
| Max Payload | 200 bytes |
| TX Power | +14 dBm (default), up to +22 dBm |
| Adaptive SF | Hub commands SF change based on RSSI |

## Frame Format

```
Offset  Size  Field         Description
0       4     PREAMBLE      0xAA 0x55 0xAA 0x55
4       2     ADDR          Destination address (0x0000 = Hub, 0xFFFF = broadcast)
6       1     SEQ           Sequence number (incremented per frame)
7       1     TYPE          Frame type (see below)
8       1     LEN           Payload length (0-200)
9       LEN  PAYLOAD        Encrypted payload
9+LEN   2     CRC           CRC-16/CCITT over ADDR through PAYLOAD (pre-encryption)
```

Total frame size: 11 + LEN bytes (max 211 bytes).

## Address Scheme

| Address | Description |
|---------|-------------|
| 0x0000 | Hub (always) |
| 0x0001 – 0x00FF | Fermenter nodes (assigned during pairing) |
| 0x0100 – 0x01FF | Cellar monitors |
| 0x0200 – 0x02FF | Brew Scanners |
| 0xFFFF | Broadcast (all nodes) |

## Frame Types

| Type | Code | Direction | Description |
|------|------|-----------|-------------|
| BEACON | 0x01 | Hub → All | Hub beacon with network info |
| PAIR_REQ | 0x02 | Node → Hub | Pairing request (unencrypted) |
| PAIR_RESP | 0x03 | Hub → Node | Pairing response with session key |
| TELEMTRY | 0x10 | Node → Hub | Sensor telemetry data |
| COMMAND | 0x11 | Hub → Node | Control command |
| ACK | 0x20 | Either | Acknowledgement |
| ALERT | 0x30 | Hub → Node | Alert notification |
| OTA_INIT | 0x40 | Hub → Node | OTA firmware update init |
| OTA_DATA | 0x41 | Hub → Node | OTA firmware chunk |
| OTA_DONE | 0x42 | Hub → Node | OTA complete, apply |
| HEARTBEAT | 0xF0 | Either | Keep-alive |

## Payload Formats

### BEACON (0x01)
```
Offset  Size  Field
0       4     Network ID
4       1     Channel count
5       1     Current SF (7-12)
6       2     Hub uptime (seconds)
8       1     Max nodes supported
```

### PAIR_REQ (0x02)
```
Offset  Size  Field
0       1     Node type (1=fermenter, 2=cellar, 3=scanner)
1       6     Node EUI-64 (last 6 bytes)
7       1     Capabilities bitmask
8       32    Node ECDH public key (X25519)
```

### PAIR_RESP (0x03)
```
Offset  Size  Field
0       2     Assigned address
2       32    Hub ECDH public key (X25519)
34      16    Encrypted session key (AES-128, encrypted with derived shared secret)
```

### TELEMETRY (0x10) — Fermenter Node
```
Offset  Size  Field
0       4     Timestamp (Unix seconds, UTC)
4       4     Specific Gravity (float32, e.g. 1.065)
8       4     Temperature °C (float32)
12      4     CO2 ppm (float32)
16      4     Pressure bar (float32)
20      4     pH (float32)
24      2     Battery voltage (uint16, mV)
26      1     Flags (bit0: temp_alarm, bit1: sg_alarm, bit2: co2_alarm, bit3: ph_alarm)
27      1     Sensor status bitmask (bit0=sg_ok, bit1=temp_ok, bit2=co2_ok, bit3=press_ok, bit4=ph_ok)
```
Total: 28 bytes

### TELEMETRY (0x10) — Cellar Monitor
```
Offset  Size  Field
0       4     Timestamp (Unix seconds, UTC)
4       4     Temperature °C (float32)
8       4     Humidity %RH (float32)
12      4     Pressure hPa (float32)
16      4     Vibration RMS mg (float32)
20      2     Light lux (uint16)
22      2     Battery voltage (uint16, mV)
24      1     Flags
```
Total: 25 bytes

### COMMAND (0x11)
```
Offset  Size  Field
0       1     Command code
1       1     Parameter length
2       N     Parameters
```

Command codes:
| Code | Name | Parameters |
|------|------|-----------|
| 0x01 | SET_REPORT_INTERVAL | uint16 seconds |
| 0x02 | SET_SF | uint8 spreading factor (7-12) |
| 0x03 | START_BATCH | batch_id (16 bytes) |
| 0x04 | END_BATCH | none |
| 0x05 | CALIBRATE_SENSOR | sensor_type (1 byte) |
| 0x06 | RESET | none |
| 0x07 | SET_TEMP_TARGET | float32 target °C |
| 0x08 | ENABLE_RELAY | relay_id (1 byte), enable (1 byte) |

### ALERT (0x30)
```
Offset  Size  Field
0       1     Alert type
1       1     Severity (0=info, 1=warning, 2=critical)
2       1     Message length
3       N     Message (UTF-8)
```

Alert types:
| Code | Name |
|------|------|
| 0x01 | TEMPERATURE_EXCURSION |
| 0x02 | STUCK_FERMENTATION |
| 0x03 | INFECTION_WARNING |
| 0x04 | BATTERY_LOW |
| 0x05 | SENSOR_MALFUNCTION |
| 0x06 | TARGET_FG_REACHED |
| 0x07 | FERMENTATION_COMPLETE |

## Encryption

After pairing, all frames use AES-128-CCM encryption on the PAYLOAD field. The 16-byte key is derived via X25519 ECDH during pairing. The nonce for each frame is: `[ADDR(2)] [SEQ(1)] [padding(9)]`.

## Retransmission

- Frames requiring ACK (TELEMETRY, COMMAND): retry up to 3 times with exponential backoff (1s, 2s, 4s)
- Beacon: no ACK required
- After 3 failures: mark node as offline, alert user

## Duty Cycle Compliance

EU 868 MHz: Max 1% duty cycle per node. At SF7/125kHz, airtime per frame ≈ 100ms. At 5-minute reporting interval: 0.033% duty cycle — well within limits.

## BLE Scanner Protocol (GATT)

The Brew Scanner communicates with the Hub over BLE 5.0 using a custom GATT service:

- **Service UUID**: `0x19B10000-E8F2-537E-4F6C-D104768A1214`
- **Characteristic: Scan Data** (UUID: `...1215`, Read/Notify): Latest scan results
- **Characteristic: Scan Command** (UUID: `...1216`, Write): Trigger scan type
- **Characteristic: Device Info** (UUID: `...1217`, Read): Battery, firmware version
- **Characteristic: Batch Link** (UUID: `...1218`, Write): Link scanner to batch

Scan command values:
| Value | Scan Type |
|-------|-----------|
| 0x01 | Refractometer (OG/FG from spectral) |
| 0x02 | Infection check |
| 0x03 | Color measurement |
| 0x04 | Full scan (all above) |