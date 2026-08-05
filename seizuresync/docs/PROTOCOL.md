# SeizureSync — Communication Protocol Specification

## Radio layers

### 1. Sub-GHz 868 MHz TDMA Mesh (SX1262)

All 4 nodes communicate via Sub-GHz 868 MHz for long-range (500 m LoS),
low-power, wall-penetrating mesh networking.

**Modem parameters:**
- Frequency: 868.0 MHz (EU 868 ISM band)
- Bandwidth: 125 kHz
- Spreading factor: 7 (LoRa)
- Coding rate: 4/5
- TX power: +14 dBm
- Encryption: AES-128 CTR
- CRC: CRC-16-CCITT

**TDMA superframe:**
```
Slot 0 (125ms): Hub BEACON (broadcast sync)
Slot 1 (125ms): Band TX
Slot 2 (125ms): Patch TX
Slot 3 (125ms): Beacon TX
Slots 4-7:      Dynamic (additional nodes)
```
- 1 s superframe, 125 ms slot, 5 ms guard
- Hub broadcasts BEACON at start of each superframe
- Nodes sync to beacon and transmit only in their assigned slot
- AES-128 CTR encryption with per-network key
- CRC-16-CCITT on payload

### 2. BLE 5.0 (band↔hub, band↔mobile, patch↔band/hub)

**GATT services:**
| UUID | Name | Description |
|---|---|---|
| 0x2A01 | SeizureService | Seizure event notifications |
| 0x2A02 | SignalService | Raw signal streaming |
| 0x2A03 | ConfigService | Configuration + OTA |

**Pairing**: LE Secure Connections (Numeric Comparison)

**Streaming**: Band streams 200 Hz accel + 100 Hz PPG + 4 Hz EDA to hub
via SignalService notifications (200-byte chunks).

### 3. Wi-Fi/MQTT (hub↔cloud)

- WPA2-Enterprise
- MQTT 3.1.1 to Mosquitto broker
- TLS 1.2 encryption
- Topics: `seizuresync/{patient_id}/{type}`

### 4. 4G LTE (hub backup)

- SIM7600G Cat-4 (150 Mbps down, 50 Mbps up)
- Activated only when Wi-Fi unavailable
- MQTT over TLS via cellular data

## Packet format

All Sub-GHz packets share a common header:

```
Offset  Size  Field
0       6     net_id[6]       Network identifier
6       1     src_node        Source node ID (slot number)
7       1     dst_node        Destination (0xFF = broadcast)
8       1     type            Packet type (sz_pkt_type_t)
9       1     seq             Sequence number
10      2     crc             CRC-16 over payload
```

Payload follows header; CRC-16 (CRC-16-CCITT) appended after payload.

## Packet types

| Type | Code | Payload | Direction |
|---|---|---|---|
| BEACON | 0x01 | none | Hub → all |
| JOIN | 0x02 | node_info | Node → hub |
| JOIN_ACK | 0x03 | assigned_slot | Hub → node |
| HEARTBEAT | 0x04 | heartbeat_payload | Node → hub |
| SEIZURE_ALERT | 0x10 | seizure_payload | Band → hub → beacon |
| AURA_ALERT | 0x11 | aura_payload | Patch → hub → beacon |
| SUDEP_ALERT | 0x12 | sudep_payload | Hub → beacon |
| ACK | 0x20 | ack_payload | Beacon → hub |
| DISPATCH | 0x21 | ack_payload (action=1) | Beacon → hub |
| SIGNAL_CHUNK | 0x30 | raw signal data | Band → hub |
| CONFIG | 0x40 | config data | Hub → node |
| OTA_MODEL | 0x50 | model chunk | Hub → band/patch |
| TEST | 0x60 | test command | Any → any |

## Payload structures

### seizure_payload (SZ_PKT_SEIZURE_ALERT)
```
Offset  Size  Field
0       4     onset_unix      Seizure onset (UTC timestamp)
4       1     semiology       ILAE classification (0-7)
5       1     severity        0-4 (info/aura/seizure/SUDEP/recovery)
6       2     duration_s      Duration in seconds (0=ongoing)
8       1     confidence      0-100%
9       1     recovery_state  0=active 1=postictal 2=recovered
```

### aura_payload (SZ_PKT_AURA_ALERT)
```
Offset  Size  Field
0       4     predicted_unix  Predicted seizure time
4       2     lead_time_s     Estimated lead time (seconds)
6       1     probability     0-100%
```

### sudep_payload (SZ_PKT_SUDEP_ALERT)
```
Offset  Size  Field
0       1     apnea_state     0=normal 1=mild 2=mod 3=sev 4=crit
1       2     apnea_duration_s
3       1     prone_flag       1=prone (face-down)
4       1     spo2_pct         SpO2 in %
5       1     hr_bpm           Heart rate
```

### heartbeat_payload (SZ_PKT_HEARTBEAT)
```
Offset  Size  Field
0       1     battery_pct
1       1     rssi_dbm (signed)
2       1     status_flags    bit0:worn bit1:charging bit2:error
3       2     free_heap_kb
```

### ack_payload (SZ_PKT_ACK / SZ_PKT_DISPATCH)
```
Offset  Size  Field
0       4     event_unix
4       1     action           0=ack 1=dispatch911
```